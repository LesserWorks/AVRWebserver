#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "HeaderStructs/HeaderStructs.h"
//#include "ENC28J60_macros/ENC28J60_macros.h"
#include "ENC28J60_functions/ENC28J60_functions.h"
#include "ARP/ARP.h"
#include "Checksum/Checksum.h"
#include "DHCP/DHCP.h"
#include "Socket/Socket.h"
#include "WebserverDriver.h"

#ifndef F_CPU
#error "F_CPU"
#endif

#if MAX_SOCKETS > 127 || MAX_SOCKETS < 1
#error "Invalid number of sockets"
#endif

#if MAX_STREAMS > 127 || MAX_STREAMS < 1
#error "Invalid number of streams"
#endif

#if (RX_MASK & STREAM_RX_SIZE)
#error "RX stream buffer not power of two"
#endif
#if STREAM_RX_SIZE > 32768
#error "RX stream too big"
#endif
#if (TX_MASK & STREAM_TX_SIZE)
#error "TX stream buffer not power of two"
#endif
#if STREAM_TX_SIZE > 32768
#error "TX stream too big"
#endif


#define STACK_HIGH *(const volatile uint8_t *)0x5E
#define STACK_LOW *(const volatile uint8_t *)0x5D


static void IPv4processor(const void *const restrict ip);
static void ICMPv4processor(const void *const restrict ip, const void *const restrict icmp); 
static void Layer3processor(const void *const restrict ip, const void *const restrict layer3);
static void incomingMessage(const struct IPv4header *const restrict ip, const void *const restrict layer3);
static void writeRX(struct Stream *const restrict stream, const struct IPv4header *const restrict ip, const void *const restrict layer3);

const struct IPv4 broadcastIP = {{255, 255, 255, 255}};

uint8_t NICsetup(void)
{
	SerialInit();
  	sei();
  	_delay_ms(500);
  	SS_low();
  	SS_high();
  	WriteReg(ECOCON, 0); // Disable clock out pin
  	WriteWord(ERXST, RX_BUF_ST); // RX starts at 0x0000
  	WriteWord(ERXWRPT, RX_BUF_ST);
  	WriteWord(ERXND, RX_BUF_END); // RX buffer can hold 3 maximum length packets
  	WriteWord(ERXRDPT, RX_BUF_END); // Write protection pointer
  	WriteWord(ETXST, TX_BUF_ST); // TX buffer start pointer will always be here

  	WriteReg(EIE, (1 << PKTIE) | (1 << LINKIE)); // Enable interrupt for link change and packet reception
    WriteReg(EIR, 0);
    WriteReg(ERXFCON, (1 << UCEN) | (1 << CRCEN) | (1 << BCEN) | (1 << MCEN));
    WriteReg(MACON2, 0);
  	WriteReg(MACON1, (1 << MARXEN) | (1 << TXPAUS) | (1 << RXPAUS));
  	WriteReg(MACON3, (1 << PADCFG0) | (1 << TXCRCEN) | (1 << FRMLNEN) | (1 << FULDPX) | (1 << HFRMEN));
  	WriteReg(MACON4, 1 << DEFER);
  	WriteReg(MABBIPG, 0x15); // Back-to-back inter packet gap
  	WriteWord(MAIPG, 0x0C12);
  	WriteWord(MAMXFL, 1530);
    WriteReg(MAADR1, unicastMAC.addr[0]);
    WriteReg(MAADR2, unicastMAC.addr[1]);
    WriteReg(MAADR3, unicastMAC.addr[2]);
    WriteReg(MAADR4, unicastMAC.addr[3]);
    WriteReg(MAADR5, unicastMAC.addr[4]);
    WriteReg(MAADR6, unicastMAC.addr[5]);
  	WriteWord(EPAUS, 40000); // Pause timer set for 4 milliseconds
  	WritePHY(PHCON1, 1 << PDPXMD, 0); // Full duplex
  	WritePHY(PHCON2, 1 << HDLDIS, 0); // Don't loop back packets
  	//WritePHY(PHLCON, 0x3A, 0xA2);
  	WritePHY(PHLCON, (1 << LACFG0) | 0b00110000, (1 << LBCFG1) | (1 << STRCH));
  	WritePHY(PHIE, 0, (1 << PLNKIE) | (1 << PGEIE));
  	//StartPHYscan(PHSTAT2);
  	WriteReg(ECON1, 1 << RXEN); // Enable reception
  	WriteReg(ECON2, 1 << AUTOINC);

  	printf("\nWaiting for link...\n");
  	while(!(ReadPHY(PHSTAT2) & (1 << (LSTAT + 8))));
  	printf("Link is up!\n");
  	return ReadReg(EREVID);
}

void packetHandler(void) {
	while(packetPending()) {
		cli();
		//printf("SP: %u\n", (uint16_t)STACK_HIGH << 8 | STACK_LOW); // If I read STACK_LOW it makes it crash
		sei();
		const uint16_t frameSize = getFrameSize();
		if(frameSize == 0) {
			printf("Bad frame size\n");
			return;
		}
		else
			printf("Frame size: %u ", frameSize);
		uint8_t rx[frameSize]; // Make buffer to hold incoming packet
		readFrame(rx, frameSize); // Read it from ENC28J60
		const struct EthernetFrame *const eth = (struct EthernetFrame *)rx; // Overlay ethernet header struct
		printf("Packet ethertype: 0x%04X\n", eth->ethertype);
		switch(eth->ethertype) {
			case ETHER_ARP: // 0x0806
				ARPprocessor((struct ARP *)((uint8_t *)eth + sizeof(struct EthernetFrame)));
				break;
			case ETHER_IPv4: // 0x0800
				IPv4processor((uint8_t *)eth + sizeof(struct EthernetFrame));
				break;
			case ETHER_IPv6: // 0x86DD
				break;
		}
	}
}

// For sending, it is socket, connect, send/recv
// connect takes IP and port of dest and src port doesn't matter
// Buffer should be allocated upon call to connect

// For receiving, it is socket, bind, listen, accept, send/recv
// Bind sets the port here that others must connect. Buffers must be allocated in low-level drivers
// upon incoming connects whose descriptors are returned in accept
int8_t socket(const uint8_t protocol) {
	for(int8_t i = 0; i < MAX_SOCKETS; i++) // Find next available socket and return it
	{
		if(!sockets[i].inUse)
		{
			// We don't allocate a stream here since that will be done in connect or accept
			sockets[i].port = 0; // Not assigned yet
			sockets[i].protocol = protocol;
			sockets[i].listening = 0; // Not listening yet
			sockets[i].inUse = 1;
			return i;
		}
	}
	return -1; // No available sockets
}

int8_t bindlisten(const int8_t socket, const uint16_t port) {
	if(socket < MAX_SOCKETS && socket >= 0 && sockets[socket].inUse) {
		sockets[socket].listening = 1; // This is a listening socket
		sockets[socket].port = port; // Our local port
		return 0;
	}
	return -1;
}

int8_t connect(const int8_t socket, const uint16_t port, const struct IPv4 *const destIP) {
	if(socket < MAX_SOCKETS && socket >= 0 && sockets[socket].inUse && !sockets[socket].listening) {
		sockets[socket].port = 32876; // Some random local port number
		// Now we need to allocate a stream for this socket
		for(int8_t i = 0; i < MAX_STREAMS; i++) {
			if(!streams[i].inUse) { // Look for an unused stream
				streams[i].parent = socket; // Set this stream's parent so we can find the source port
				streams[i].remotePort = port;
				streams[i].remoteIP = *destIP;
				streams[i].rx.head = 0;
				streams[i].rx.tail = 0;
				streams[i].tx.head = 0;
				streams[i].tx.tail = 0; // Clear out ring buffers
				streams[i].accepted = 1; // Set flag so that accept() will never return it
				streams[i].inUse = 1;
				if(sockets[socket].protocol == PROTO_TCP) {
					//streams[i].state = TCP;
					// Send TCP SYN
				}
				else
					streams[i].state = UDP_MODE;
				return i;
			}
		}
	}
	return -1;
}

int8_t accept(const int8_t socket, const uint8_t flags) {
	if(socket < MAX_SOCKETS && socket >= 0 && sockets[socket].inUse && sockets[socket].listening) {
		while(1) {
			for(uint8_t i = 0; i < MAX_STREAMS; i++) {
				if(streams[i].inUse && !streams[i].accepted && streams[i].parent == socket) {
					streams[i].accepted = 1; // So we don't return it from accept again
					return i; // Return stream descriptor
				}
			}
			if(flags == MSG_DONTWAIT) 
				break; // return EWOULDBLOCK
			else
				packetHandler(); // Process more packets before checking streams again
		}
	}
	return -1;
}

void closeSocket(const int8_t socket) {
	if(socket < MAX_SOCKETS && socket >= 0)
		sockets[socket].inUse = 0;
}

void closeStream(const int8_t stream) {
	if(stream < MAX_STREAMS && stream >= 0) {
		if(streams[stream].state != UDP_MODE)
			TCPclose(stream);
		streams[stream].inUse = 0;
	}
}

static void incomingMessage(const struct IPv4header *const restrict ip, const void *const restrict layer3) {
	// First see if there is already a stream for this client. If so, add this message there.
	for(uint8_t i = 0; i < MAX_STREAMS; i++) {
		// See if stream is in use and has matching port, address, and protocol. The layer3 trick works for both UDP and TCP packets
		if(streams[i].inUse && sockets[streams[i].parent].protocol == ip->protocol 
			&& streams[i].remotePort == PORTS(layer3)->srcPort
			&& memcmp(&streams[i].remoteIP, &ip->srcIP, sizeof(struct IPv4)) == 0) {
			printf("Enqueued packet from %u\n", streams[i].remotePort);
			writeRX(&streams[i], ip, layer3);
			return;
		}
	}
	// If not, see if any sockets are listening for this port and protocol and if so, allocate a new stream and put it there
	for(uint8_t i = 0; i < MAX_SOCKETS; i++) {
		if(sockets[i].inUse && sockets[i].listening && sockets[i].protocol == ip->protocol
			&& sockets[i].port == PORTS(layer3)->destPort) {
			for(uint8_t j = 0; j < MAX_STREAMS; j++) {
				if(!streams[j].inUse) { // Look for an unused stream
					streams[j].parent = i; // Set this stream's parent so we can find the source port
					streams[j].remotePort = PORTS(layer3)->srcPort;
					streams[j].remoteIP = ip->srcIP;
					streams[j].rx.head = 0;
					streams[j].rx.tail = 0;
					streams[j].tx.head = 0;
					streams[j].tx.tail = 0; // Clear out ring buffers which is needed for both TCP and UDP
					if(sockets[i].protocol == PROTO_UDP)
						streams[j].state = UDP_MODE;
					else // TCP mode
						streams[j].state = LISTEN; 
					streams[j].accepted = 0; // No one has called accept and received this stream yet
					streams[j].inUse = 1;
					printf("New packet from %u.%u.%u.%u:%u to %u\n", ip->srcIP.addr[0], ip->srcIP.addr[1], ip->srcIP.addr[2], ip->srcIP.addr[3],
						PORTS(layer3)->srcPort, sockets[i].port);
					writeRX(&streams[j], ip, layer3);
					return;
				}
			}
			return; // If we found a listening socket but not open stream, drop the packet
		}
	}
}

static void writeRX(struct Stream *const restrict stream, const struct IPv4header *const restrict ip, const void *const restrict layer3) {
	if(stream->state == UDP_MODE) { // This is a UDP socket
		const struct UDPheader *const restrict udp = (struct UDPheader *)layer3;
		const uint8_t *const restrict payload = (uint8_t *)layer3 + sizeof(struct UDPheader);
		// Uses zero-waste ring buffer https://www.snellman.net/blog/archive/2016-12-13-ring-buffers/
		const uint16_t payloadLen = udp->length - sizeof(struct UDPheader);
		if(STREAM_RX_SIZE - (stream->rx.head - stream->rx.tail) >= payloadLen + sizeof(uint16_t)) { // Ensure we have space for whole datagram
			stream->rx.buf[stream->rx.head++ & RX_MASK] = payloadLen & 0xFF;
			stream->rx.buf[stream->rx.head++ & RX_MASK] = payloadLen >> 8; // Write in datagram length first
			printf("Going to write %u bytes\n", payloadLen);
			for(uint16_t i = 0; i < payloadLen; i++)
				stream->rx.buf[stream->rx.head++ & RX_MASK] = payload[i]; // Write in datagram
		}
	}
	else { // This is a TCP socket
		TCPprocessor(stream, ip, (struct TCPheader *)layer3);
	}
}

int16_t recv(const int8_t stream, void *const dest, const int16_t buflen, const uint8_t flags) {
	if(stream < MAX_STREAMS && stream >= 0 && streams[stream].inUse && streams[stream].accepted && buflen >= 0) {
		if(streams[stream].state == UDP_MODE) { // UDP stream
			while(1) {
				if(streams[stream].rx.head != streams[stream].rx.tail) { // Is there a message waiting
					int16_t length = streams[stream].rx.buf[streams[stream].rx.tail++ & RX_MASK]; // Get low byte
					length |= (int16_t)streams[stream].rx.buf[streams[stream].rx.tail++ & RX_MASK] << 8; // Get high byte
					for(int16_t i = 0; i < length; i++) {
						if(i < buflen)
							((uint8_t *)dest)[i] = streams[stream].rx.buf[streams[stream].rx.tail++ & RX_MASK];
						else
							streams[stream].rx.tail++; // Free buffer space anyway
					}
					return buflen > length ? length : buflen; // We wrote to user the minimum of these
				}
				else if(flags == MSG_DONTWAIT)
					break; // return EWOULDBLOCK
				else
					packetHandler(); // Process more packets before checking streams again
			}
		}
		else { // This is a TCP stream
			return TCPrecv(stream, dest, buflen, flags);
		}
	}
	return -1;
}

int16_t send(const int8_t stream, const void *const src, const uint16_t buflen, const uint8_t flags) {
	if(stream < MAX_STREAMS && stream >= 0 && streams[stream].inUse && streams[stream].accepted) {
		if(streams[stream].state == UDP_MODE) { // In UDP just send it immediately
			const struct UDPheader udp = {.srcPort = sockets[streams[stream].parent].port, 
										  .destPort = streams[stream].remotePort, 
									      .length = sizeof(udp) + buflen, 
									      .checksum = 0};
			sendIPv4packet(&streams[stream].remoteIP, &localIP, PROTO_UDP, udp.length, 2, 
						   LAYERS({&udp, sizeof(udp)},
								  {src, buflen}));
			return buflen;

		}
		else { // This is a TCP socket
			return TCPsend(stream, src, buflen, flags);
		}
	}
	return -1;
}

static void IPv4processor(const void *const restrict ip)
{
	switch(((struct IPv4header *)ip)->protocol)
	{
		case PROTO_ICMPv4:
			ICMPv4processor(ip, (uint8_t *)ip + ((struct IPv4header *)ip)->iht * 4);
			break;
		case PROTO_TCP: // TCP
		case PROTO_UDP: // UDP
			Layer3processor(ip, (uint8_t *)ip + ((struct IPv4header *)ip)->iht * 4);
			break;
		case PROTO_ICMPv6: // ICMPv6
		default:
			printf("IPv4 packet payload: %u\n", ((struct IPv4header *)ip)->protocol);
	}
}

/*
The variadic arguments must be a pointer to the data followed by the length of the data, repeating
*/
void sendIPv4packet(const struct IPv4 *const dest, const struct IPv4 *const src, 
					const uint8_t protocol, const uint16_t payloadLen, const uint8_t payloadNum, const struct Layer payload[])
{
	struct IPv4header packet = {.version = 4, .iht = 5, .dscp = 0, .ecn = 0, .length = payloadLen + sizeof(struct IPv4header), 
								.id = rand(), .zero = 0, .df = 1, .mf = 0, .offset = 0, .ttl = 60, 
								.protocol = protocol, .checksum = 0, .srcIP = *src, .destIP = *dest};
	packet.checksum = checksumUnrolled(&packet, (uint8_t *)&packet + sizeof(struct IPv4header));

	const struct MAC *const destMAC = memcmp(dest, &broadcastIP, sizeof(struct IPv4)) == 0 ? 
																			 &broadcastMAC : arp(&routerIP);
	printf("IP src: %u.%u.%u.%u dest: %u.%u.%u.%u\n", packet.srcIP.addr[0], packet.srcIP.addr[1], packet.srcIP.addr[2], packet.srcIP.addr[3],
						packet.destIP.addr[0], packet.destIP.addr[1], packet.destIP.addr[2], packet.destIP.addr[3]);
	if(destMAC == NULL) {
		puts("Couldn't find MAC");
		return;
	}
	sendEthernetFrame(destMAC, &unicastMAC, ETHER_IPv4, &packet, sizeof(struct IPv4header), payloadNum, payload);
}

static void ICMPv4processor(const void *const restrict ip, const void *const restrict icmp)
{
	switch(((struct ICMPv4header *)icmp)->type)
	{
		case 0: // Echo reply
			break;
		case 8: // Echo request
		{
			uint8_t reply[((struct IPv4header *)ip)->length - ((struct IPv4header *)ip)->iht * 4]; // Large enough to hold ICMP payload
			struct ICMPv4header *const icmpReply = (struct ICMPv4header *)reply;
			icmpReply->type = 0;
			icmpReply->code = 0;
			icmpReply->checksum = 0;
			icmpReply->id = ((struct ICMPv4header *)icmp)->id;
			icmpReply->seq = ((struct ICMPv4header *)icmp)->seq;
			memcpy((uint8_t *)icmpReply + sizeof(struct ICMPv4header), 
					icmp + sizeof(struct ICMPv4header), 
					sizeof(reply) - sizeof(struct ICMPv4header)); // Copy extra data
			// I originally had it as sizeof(ICMPv4header) and the Mac rejected all its pings
			icmpReply->checksum = checksumUnrolled(icmpReply, (uint8_t *)icmpReply + sizeof(reply));

			sendIPv4packet(&((struct IPv4header *)ip)->srcIP, &localIP, PROTO_ICMPv4, sizeof(reply), 1, LAYERS({reply, sizeof(reply)}));
			// Dest IP, Src IP, ICMPv4 code, total payload length, number of payloads, first payload content, size of first content
			printf("Sent ping.\n");
			break;
		}
	}
}

static void Layer3processor(const void *const restrict ip, const void *const restrict layer3)
{
	// The only layer 3 protocol the "kernel" manages is DHCP, others must have a user-opened port
	switch(PORTS(layer3)->destPort)
	{
		case PORT_DHCP_CLIENT: // DHCP
			if(((struct IPv4header *)ip)->protocol == PROTO_UDP) // Only support DHCP over UDP so far
				DHCPprocessor(ip, (struct DHCPheader *)((uint8_t *)layer3 + sizeof(struct UDPheader)));
			break;
		case PORT_HTTPS: // QUIC
		default: // See if we opened a port on whatever is coming in
			incomingMessage((struct IPv4header *)ip, layer3);
	}
}

void printPacket(const uint8_t *const p, const uint16_t len) {
	for(uint16_t i = 0; i < len; i++) {
		printf("%02X ", p[i]);
	}
	printf("\n");
}
