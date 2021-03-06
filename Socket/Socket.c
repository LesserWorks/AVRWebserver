#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "HeaderStructs/HeaderStructs.h"
#include "Checksum/Checksum.h"
#include "RTC/RTC.h"
#include "WebserverDriver/WebserverDriver.h"
#include "Socket.h"

#define RETRANSMIT_PERIOD 5

struct Socket sockets[MAX_SOCKETS] = {0}; // Where we store our socket descriptors
struct Stream streams[MAX_STREAMS] = {0}; // Our pool of streams that sockets can acquire

static const void *getTCPoption(const uint8_t *const options, const uint8_t num);
static uint16_t TCPchecksum(const struct IPv4 *const restrict destIP, const struct TCPheader *const restrict tcp, 
							const uint8_t options[], const uint8_t optionsLen, const uint8_t data[], const uint16_t dataLen);
static void sendWhatWeCan(const int8_t stream);
static void sendTCPpacket(const struct Stream *const restrict stream, const uint32_t seq, const uint32_t ack, 
	const uint16_t flags, const uint8_t options[], const uint8_t optionsLen, const uint8_t data[], const uint16_t dataLen);

// Main state machine: http://www.tcpipguide.com/free/t_TCPOperationalOverviewandtheTCPFiniteStateMachineF-2.htm
// http://www.tcpipguide.com/free/t_TCPConnectionManagementandProblemHandlingtheConnec-2.htm
// On wireshark, when I click a link on the website, sends syn etc, then content, then server closes the connection
// It does send RST when receives SYN for port with no listener
// If one end sends FIN, it can still receive residual data from other end, can send data in FIN segment
// From FIN_WAIT_1, if it receives FIN ACK it goes to TIME_WAIT

// Standard MSS without options is 536 bytes
// TCP todo:
// Add separate retransmit timers for each TCP segment
// Allow receiving TCP frames out of order

void TCPprocessor(struct Stream *const restrict stream, const struct IPv4header *const restrict ip, const struct TCPheader *const restrict tcp) {
	printf("TCPprocessor state = %u, flags = 0x%02X\n", stream->state, tcp->flags);
	switch(stream->state) {
		case CLOSED:
			break;
		case LISTEN: // Expecting SYN, send SYN_ACK
			//printf("Case LISTEN, flags = %x\n", tcp->flags);
			if(tcp->flags & SYN) {
				puts("Got SYN packet");
				stream->tx.window = tcp->window; // For now before window scaling
				const uint8_t *const scale = getTCPoption((uint8_t *)tcp + sizeof(struct TCPheader), 3); // Process window scaling option
				if(scale == NULL)
					stream->tx.scale = 1;
				else
					stream->tx.scale = 1 << scale[1];
				puts("Processed options");
				stream->tx.next = 0; // This is not initialized by incomingPacket()
				const uint8_t options[] = {2, 4, 536 >> 8, 536 & 0xFF, // MSS option
											1, 1, 1, 0}; // End of options, must make size a multiple of 4
				stream->tx.rawseq = rand();
				stream->rx.rawseq = tcp->seq; // Temporarily set our RX zero point

				sendTCPpacket(stream, 0, 1, SYN | ACK, options, sizeof(options), NULL, 0);

				stream->tx.rawseq += 1; // Account for phantom byte when setting our TX zero point
				stream->state = SYN_RECEIVED;
				// We don't need to worry about the transmit window since we are only sending a phantom byte
				//printf("SYN-ACK from %u.%u.%u.%u:%u to %u.%u.%u.%u:%u\n", localIP.addr[0], localIP.addr[1], localIP.addr[2], localIP.addr[3],
				//		resp.srcPort, stream->remoteIP.addr[0], stream->remoteIP.addr[1], stream->remoteIP.addr[2], stream->remoteIP.addr[3], resp.destPort);
			}
			break; // else drop the packet
		case SYN_RECEIVED: // Expecting an ACK, then no further action
			if(tcp->flags == ACK) {
				stream->rx.rawseq += 1; // Set RX zero point to right past phantom byte
				stream->state = ESTABLISHED;
				puts("Stream established");
			}
			else if(tcp->flags & RST)
				stream->state = LISTEN;
			break;
		case ESTABLISHED: // These are the three states in which we can receive data
		case FIN_WAIT_1:
		case FIN_WAIT_2: {
			if(tcp->flags & RST) {
				stream->state = CLOSED;
				break;
			}
			const uint16_t payloadLen = ip->length - ip->iht * 4 - tcp->offset * 4; 
			printf("Est payload = %u\n", payloadLen);
			if(STREAM_RX_SIZE - (stream->rx.head - stream->rx.tail) >= payloadLen && payloadLen > 0) { // Do we have room for this payload?
				if(tcp->seq - stream->rx.rawseq == stream->rx.head) { // Is this payload contiguous with any previous payloads?
					const uint8_t *const restrict payload = (uint8_t *)tcp + tcp->offset * 4;
					for(uint16_t i = 0; i < payloadLen; i++)
						stream->rx.buf[stream->rx.head++ & RX_MASK] = payload[i]; // Write in this data
					// Send ACK packet 
					const uint8_t options[] = {1, 1, 1, 0};
					sendTCPpacket(stream, stream->tx.next, stream->rx.head, ACK, options, sizeof(options), NULL, 0); 
					puts("Est sent ACK");
				} // A proper implementation would allow receiving discontiguous received payloads and selective acknowledgement
				else {
					puts("Not contiguous");
				}
			}
			stream->tx.window = stream->tx.scale * tcp->window; // Update our send window
			stream->tx.tail = tcp->ack - stream->tx.rawseq; // Move tail to after last ACKed byte

			// Have we received a FIN frame and we have ACKed all their data up to FIN?
			if(tcp->flags & FIN && tcp->seq == stream->rx.head + stream->rx.rawseq) {
				// Send ACK to their FIN here
				sendTCPpacket(stream, stream->tx.next, stream->rx.head + 1, ACK, NULL, 0, NULL, 0);
				switch(stream->state) { // Sorry for a nested switch here
					case ESTABLISHED:
						stream->state = CLOSE_WAIT; // Now we wait for user to call closeStream()
						break;
					case FIN_WAIT_1:
						if(stream->tx.tail > stream->tx.next) { // If our FIN was also ACKed with this packet (also see if() below)
							stream->state = TIME_WAIT; // All done, just wait for all packets to get through now
							stream->timer = RTCsetTimer(TIME_WAIT_SECONDS);
						}
						else
							stream->state = CLOSING; // Wait for them to ACK our FIN
						break;
					case FIN_WAIT_2:
						stream->state = TIME_WAIT; // All done, just wait for all packets to get through now
						stream->timer = RTCsetTimer(TIME_WAIT_SECONDS);
						break;
					default: // Never reaches here
						break;
				}
			}
			// This if-statement assumes that when user calls closeStream() it does not increment tx.next with phantom byte
			else if(stream->state == FIN_WAIT_1 && stream->tx.tail > stream->tx.next) // If we received an ACK for our FIN
				stream->state = FIN_WAIT_2; // Now wait for their FIN
			break;
		}
		case LAST_ACK: // In these two states we are just waiting for them to ACK our FIN
		case CLOSING:
			if(tcp->flags & RST) {
				stream->state = CLOSED;
				stream->inUse = 0; // Free this stream
				break;
			}
			stream->tx.tail = tcp->ack - stream->tx.rawseq; // Move tail to after last ACKed byte
			if((tcp->flags & ACK) && stream->tx.tail > stream->tx.next) { // They ACKed our FIN
				if(stream->state == LAST_ACK) {
					stream->state = CLOSED;
					stream->inUse = 0; // Free this stream
				}
				else { // CLOSING
					stream->state = TIME_WAIT;
					stream->timer = RTCsetTimer(TIME_WAIT_SECONDS);
				}
			}
			break;
		case SYN_SENT: // TCP active open (i.e. sending SYN) not currently supported
		case CLOSE_WAIT: // We do not expect to receive packets in these states
		case TIME_WAIT:
		default:
			if(tcp->flags & RST)
				stream->state = CLOSED;
	}
}

// This function will not be called by the user directly
int16_t TCPrecv(const int8_t stream, void *const dest, const int16_t buflen, const uint8_t flags) {
	struct Stream *const s = &streams[stream];
	while(1) {
		if(s->rx.head != s->rx.tail) { // Is there data waiting
			int16_t length = s->rx.head - s->rx.tail;
			for(int16_t i = 0; i < length; i++) {
				if(i < buflen)
					((uint8_t *)dest)[i] = s->rx.buf[s->rx.tail++ & RX_MASK];
				else
					break; // From for-loop
			}

			return buflen > length ? length : buflen; // We wrote to user the minimum of these
		}
		// The following three states are the only ones in which we can still receive data
		else if(!(s->state == ESTABLISHED || s->state == FIN_WAIT_1 || s->state == FIN_WAIT_2)) 
			return 0; // Return 0 if other end sent FIN and there is no more data waiting
		else if(flags & MSG_DONTWAIT)
			return -1; // return EWOULDBLOCK
		else
			packetHandler(); // Process more packets before checking streams again
	}
}

// This function will not be called by the user directly
int16_t TCPsend(const int8_t stream, const void *const src, const int16_t buflen, const uint8_t flags) {
	struct Stream *const s = &streams[stream];
	if(s->state == ESTABLISHED || s->state == CLOSE_WAIT) { // These are the only states we can send data from
		int16_t room = STREAM_TX_SIZE - (s->tx.head - s->tx.tail); // Available space in TX buffer
		for(int16_t i = 0; i < room; i++) {
			if(i < buflen)
				s->tx.buf[s->tx.head++ & TX_MASK] = ((uint8_t *)src)[i];
			else
				break; // From for-loop
		}
		// At this point we have written all the data we can into the TX buffer, now we need to send some of it
		sendWhatWeCan(stream);
		return buflen > room ? room : buflen; // Return how much we wrote into TX buffer, not how much we actually sent
	}
	else
		return -1; // This stream is not in a sendable state
}

void TCPclose(const int8_t stream) {
	struct Stream *const s = &streams[stream];
	switch(s->state) {
		// It only makes sense to call close in the following states
		case ESTABLISHED:
		case CLOSE_WAIT:
			sendTCPpacket(s, s->tx.next, s->rx.head, FIN | ACK, NULL, 0, NULL, 0);
			// We arbitrarily decide to not increment tx.next here despite sending a phantom byte
			if(s->state == ESTABLISHED)
				s->state = FIN_WAIT_1; // Wait for them to ACK our FIN before they send their own FIN
			else // CLOSE_WAIT
				s->state = LAST_ACK; // Wait for them to ACK our FIN (they already sent their FIN)
			break;
		default:
			s->state = CLOSED; // Strictly speaking unnecessary to set this
			s->inUse = 0; // Free this stream
			break;
	}
}

static void sendWhatWeCan(const int8_t stream) {
	struct Stream *const s = &streams[stream];
	const uint16_t ableToSend = s->tx.tail + s->tx.window < s->tx.head ? 
						  		s->tx.tail + s->tx.window - s->tx.next :
						  		s->tx.head - s->tx.next; // Calculate how much we can send based on send window
	if(ableToSend > 0) { // This means we will even send 1-byte payloads, which is very inefficient
		uint8_t temp[ableToSend]; // Make temp buffer to straighten out circular buffer
		const uint32_t prevNext = s->tx.next;
		for(uint16_t i = 0; i < ableToSend; i++)
			temp[i] = s->tx.buf[s->tx.next++ & TX_MASK]; // Copy what we'll send in this packet to temp
		sendTCPpacket(s, prevNext, s->rx.head, ACK, NULL, 0, temp, ableToSend);
		RTCresetTimer(s->timer, RETRANSMIT_PERIOD); // After every sent data frame, reset retransmission timer
	}
}

static void sendTCPpacket(const struct Stream *const restrict stream, const uint32_t seq, const uint32_t ack, 
	const uint16_t flags, const uint8_t options[], const uint8_t optionsLen, const uint8_t data[], const uint16_t dataLen) {
	struct TCPheader pkt = {.srcPort = sockets[stream->parent].port, 
							.destPort = stream->remotePort,
							.seq = seq + stream->tx.rawseq, .ack = ack + stream->rx.rawseq,  
							.offset = (sizeof(struct TCPheader) + optionsLen) / 4, 
							.zero = 0, .flags = flags, 
							.window = STREAM_RX_SIZE - (stream->rx.head - stream->rx.tail),
							.checksum = 0, .urgent = 0};
	pkt.checksum = TCPchecksum(&stream->remoteIP, &pkt, options, optionsLen, data, dataLen);
	sendIPv4packet(&stream->remoteIP, &localIP, PROTO_TCP, sizeof(pkt) + optionsLen + dataLen, 3, 
						LAYERS({&pkt, sizeof(pkt)},
							   {options, optionsLen},
							   {data, dataLen}));
}

void handleTCPtimers(void) {
	// This function handles timer-dependent TCP operations
	for(uint16_t i = 0; i < MAX_STREAMS; i++) {
		if(streams[i].inUse && streams[i].state != UDP_MODE) {
			if(streams[i].state == TIME_WAIT && RTCtimerDone(streams[i].timer)) { // If TIME_WAIT timer finished
				streams[i].state = CLOSED;
				streams[i].inUse = 0;
			}
			// If retransmit timer expired while in a state where they haven't ACKed our FIN
			else if((streams[i].state == ESTABLISHED || streams[i].state == FIN_WAIT_1 || streams[i].state == CLOSING 
				   || streams[i].state == CLOSE_WAIT || streams[i].state == LAST_ACK)  && RTCtimerDone(streams[i].timer)) { 
				streams[i].timer = RTCsetTimer(RETRANSMIT_PERIOD);
				sendWhatWeCan(i);
			}
		}
	}
}

static const void *getTCPoption(const uint8_t *const options, const uint8_t num) { // Returns address of length byte of that option
	for(uint16_t i = 0; options[i] != 0x00; i++)
		if(options[i] != 0x01) { // if not padding
			if(options[i] == num) // the option we're looking for
				return &options[i+1]; // Return address of next val which is length of option
			else 
				i += options[i+1] - 1; // Advance i to end of this current option
		}
	return NULL;
}

static uint16_t checksumC(const uint8_t data[], const uint16_t lenBytes) {
	uint32_t running = 0;
	for(uint16_t i = 0; i < lenBytes; i++) {
		uint16_t high = data[i++];
		uint8_t low = data[i];
		running += (high << 8) + low;
	}
	while(running > 0xFFFF)
		running = (running & 0xFFFF) + (running >> 16);
	return ~running;
}

static uint16_t TCPchecksum(const struct IPv4 *const restrict destIP, const struct TCPheader *const restrict tcp, 
							const uint8_t options[], const uint8_t optionsLen, const uint8_t data[], const uint16_t dataLen) {
	struct TCPpseudoHeader pseudo = {.srcIP = localIP, .destIP = *destIP, .zero = 0, .protocol = PROTO_TCP, 
									 .length = sizeof(struct TCPheader) + optionsLen + dataLen};
	uint16_t checksum = 0;
	checksum = checksumUpdate(checksum, &pseudo, sizeof(pseudo));
	checksum = checksumUpdate(checksum, tcp, sizeof(struct TCPheader));
	checksum = checksumUpdate(checksum, options, optionsLen);
	checksum = checksumUpdate(checksum, data, dataLen);
	return ~checksum;
	/*
	uint8_t checksumData[sizeof(struct TCPpseudoHeader) + sizeof(struct TCPheader) + optionsLen + dataLen];
	struct TCPpseudoHeader *pseudo = (struct TCPpseudoHeader *)checksumData;
	pseudo->srcIP = localIP;
	pseudo->destIP = *destIP;
	pseudo->zero = 0;
	pseudo->protocol = PROTO_TCP;
	pseudo->length = sizeof(struct TCPheader) + optionsLen + dataLen;
	uint8_t *ptr = checksumData + sizeof(struct TCPpseudoHeader);
	
	memcpy(ptr, tcp, sizeof(struct TCPheader));
	ptr += sizeof(struct TCPheader);
	memcpy(ptr, options, optionsLen);
	ptr += optionsLen;
	memcpy(ptr, data, dataLen);
	ptr += dataLen;
	return checksumUnrolled(checksumData, ptr);
	*/
}
