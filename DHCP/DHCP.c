#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include "HeaderStructs/HeaderStructs.h"
#include "ENC28J60_functions/ENC28J60_functions.h"
#include "WebserverDriver/WebserverDriver.h"
#include "RTC/RTC.h"
#include "ARP/ARP.h"
#include "DHCP.h"

// defines for option message type
#define DISCOVER 1
#define OFFER 2
#define REQUEST 3
#define DECLINE 4
#define DHCP_ACK 5
#define NAK 6
#define RELEASE 7
#define INFORM 8

// See here for the DCHP state machine 
// http://www.tcpipguide.com/free/t_DHCPGeneralOperationandClientFiniteStateMachine.htm
// Options
// http://www.tcpipguide.com/free/t_SummaryOfDHCPOptionsBOOTPVendorInformationFields-6.htm
enum __attribute__((packed)) DHCPstate {INIT, SELECTING, REQUESTING, INIT_REBOOT, \
	REBOOTING, BOUND, RENEWING, REBINDING};

struct __attribute__((packed)) DHCPdata
{
	struct IPv4 assignedIP;
	struct IPv4 serverIP;
};

// This struct allows us to easily extract a big-endian uint32_t from a DHCP option array
struct __attribute__((packed, scalar_storage_order("big-endian"))) TimerVal {
	uint32_t val;
};

#define DHCP_EEPROM (struct DCHPdata *)0
static enum DHCPstate state = INIT;
static uint32_t expectedID;
static int8_t T1;
static int8_t T2; // Our DHCP lease timers
static const void *getDHCPoption(const void *const dhcp, const uint8_t num);
static void DHCPsendInit(void);

uint8_t DHCPready(void) {
	return state == BOUND;
}

void DHCPsetup(void) { // called by user on startup
	struct DHCPdata stored;
	eeprom_read_block(&stored, DHCP_EEPROM, sizeof(stored));
	if(memcmp(&stored.assignedIP, &broadcastIP, sizeof(struct IPv4)) == 0 || memcmp(&stored.serverIP, &broadcastIP, sizeof(struct IPv4)) == 0) // See if EEPROM there is uninitialized
		state = INIT;
	else
		state = INIT_REBOOT;
	DHCPsendInit();
}

static void DHCPsendInit(void) {
	switch(state) {
		case INIT: {
			// Make DCHP Discover message
			const struct DHCPheader packet = {.op = 1, .HTYPE = 1, .HLEN = 6, .hops = 0, .xid = rand(),
										.secs = 0, .flags = 0, .clientIP = {{0}}, .yourIP = {{0}}, 
										.serverIP = {{0}}, .gatewayIP = {{0}}, .clientHW = unicastMAC, 
										.padding = {{0}}, .serverName = {{0}}, .bootfile = {{0}}, 
										.magicCookie = {{0x63, 0x82, 0x53, 0x63}}};

			const uint8_t options[] = {53, 1, DISCOVER, // message type is discover
									   12, 6, 'A', 'V', 'R', 'w', 'e', 'b', // host name (Mac sends it without null)
									   55, 2, 1, 3, // parameter request
									   61, 7, 1, unicastMAC.addr[0], unicastMAC.addr[1], unicastMAC.addr[2], unicastMAC.addr[3], unicastMAC.addr[4], unicastMAC.addr[5],
									   0xFF};
			const struct UDPheader udp = {.srcPort = PORT_DHCP_CLIENT, .destPort = PORT_DHCP_SERVER, 
									  .length = sizeof(udp) + sizeof(packet) + sizeof(options), 
									  .checksum = 0};

			state = SELECTING;
			expectedID = packet.xid;

			sendIPv4packet(&broadcastIP, &(const struct IPv4){{0, 0, 0, 0}}, PROTO_UDP, udp.length, 3, 
						   LAYERS({&udp, sizeof(udp)},
								  {&packet, sizeof(packet)},
								  {options, sizeof(options)}));
			break;
		}
		case INIT_REBOOT: {
			struct DHCPdata stored;
			eeprom_read_block(&stored, DHCP_EEPROM, sizeof(stored));
			const struct DHCPheader packet = {.op = 1, .HTYPE = 1, .HLEN = 6, .hops = 0, .xid = rand(),
								.secs = 0, .flags = 0, .clientIP = {{0}}, .yourIP = {{0}}, 
								.serverIP = {{0}}, .gatewayIP = {{0}}, .clientHW = unicastMAC, 
								.padding = {{0}}, .serverName = {{0}}, .bootfile = {{0}}, 
								.magicCookie = {{0x63, 0x82, 0x53, 0x63}}};

			const uint8_t options[] = {53, 1, REQUEST, // message type is request
									   12, 6, 'A', 'V', 'R', 'w', 'e', 'b', // host name (Mac sends it without null)
									   50, 4, stored.assignedIP.addr[0], stored.assignedIP.addr[1], stored.assignedIP.addr[2], stored.assignedIP.addr[3], // requested addr
									   61, 7, 1, unicastMAC.addr[0], unicastMAC.addr[1], unicastMAC.addr[2], unicastMAC.addr[3], unicastMAC.addr[4], unicastMAC.addr[5], // client identifier
									   54, 4, stored.serverIP.addr[0], stored.serverIP.addr[1], stored.serverIP.addr[2], stored.serverIP.addr[3], // server identifier
									   55, 1, 3, // request routers
									   0xFF};
			const struct UDPheader udp = {.srcPort = PORT_DHCP_CLIENT, .destPort = PORT_DHCP_SERVER, 
									  .length = sizeof(udp) + sizeof(packet) + sizeof(options), 
									  .checksum = 0};

			state = REBOOTING;
			expectedID = packet.xid;

			sendIPv4packet(&broadcastIP, &(const struct IPv4){{0, 0, 0, 0}}, PROTO_UDP, udp.length, 3, 
							LAYERS({&udp, sizeof(udp)},
								   {&packet, sizeof(packet)},
								   {options, sizeof(options)}));
			break;
		}
		default:
			break;
	}
}

void handleDHCPtimers(void) {
	if(state == BOUND && RTCtimerDone(T1)) { // Important that it short-circuits this condition
		// send request to initial DHCP server
		const struct DHCPheader packet = {.op = 1, .HTYPE = 1, .HLEN = 6, .hops = 0, .xid = rand(),
								.secs = 0, .flags = 0, .clientIP = {{0}}, .yourIP = {{0}}, 
								.serverIP = {{0}}, .gatewayIP = {{0}}, .clientHW = unicastMAC, 
								.padding = {{0}}, .serverName = {{0}}, .bootfile = {{0}}, 
								.magicCookie = {{0x63, 0x82, 0x53, 0x63}}};
		const uint8_t options[] = {53, 1, REQUEST, // message type is request
								   12, 6, 'A', 'V', 'R', 'w', 'e', 'b', // host name (Mac sends it without null)
								   50, 4, localIP.addr[0], localIP.addr[1], localIP.addr[2], localIP.addr[3], // requested addr
								   61, 7, 1, unicastMAC.addr[0], unicastMAC.addr[1], unicastMAC.addr[2], unicastMAC.addr[3], unicastMAC.addr[4], unicastMAC.addr[5], // client identifier
								   54, 4, routerIP.addr[0], routerIP.addr[1], routerIP.addr[2], routerIP.addr[3], // server identifier
								   55, 3, 3, 58, 59, // request routers and lease times
								   0xFF};
		const struct UDPheader udp = {.srcPort = PORT_DHCP_CLIENT, .destPort = PORT_DHCP_SERVER, 
									  .length = sizeof(udp) + sizeof(packet) + sizeof(options), 
									  .checksum = 0};
		state = RENEWING;
		expectedID = packet.xid;
		sendIPv4packet(&routerIP, &localIP, PROTO_UDP, udp.length, 3, 
									LAYERS({&udp, sizeof(udp)},
										   {&packet, sizeof(packet)},
										   {options, sizeof(options)}));
	}
	else if(state == RENEWING && RTCtimerDone(T2)) {
		// send broadcast request
		const struct DHCPheader packet = {.op = 1, .HTYPE = 1, .HLEN = 6, .hops = 0, .xid = rand(),
								.secs = 0, .flags = 0, .clientIP = {{0}}, .yourIP = {{0}}, 
								.serverIP = {{0}}, .gatewayIP = {{0}}, .clientHW = unicastMAC, 
								.padding = {{0}}, .serverName = {{0}}, .bootfile = {{0}}, 
								.magicCookie = {{0x63, 0x82, 0x53, 0x63}}};
		const uint8_t options[] = {53, 1, REQUEST, // message type is request
								   12, 6, 'A', 'V', 'R', 'w', 'e', 'b', // host name (Mac sends it without null)
								   50, 4, localIP.addr[0], localIP.addr[1], localIP.addr[2], localIP.addr[3], // requested addr
								   61, 7, 1, unicastMAC.addr[0], unicastMAC.addr[1], unicastMAC.addr[2], unicastMAC.addr[3], unicastMAC.addr[4], unicastMAC.addr[5], // client identifier
								   55, 3, 3, 58, 59, // request routers and lease times
								   0xFF};
		const struct UDPheader udp = {.srcPort = PORT_DHCP_CLIENT, .destPort = PORT_DHCP_SERVER, 
									  .length = sizeof(udp) + sizeof(packet) + sizeof(options), 
									  .checksum = 0};
		state = RENEWING;
		expectedID = packet.xid;
		sendIPv4packet(&broadcastIP, &localIP, PROTO_UDP, udp.length, 3, 
									LAYERS({&udp, sizeof(udp)},
										   {&packet, sizeof(packet)},
										   {options, sizeof(options)}));
		state = REBINDING;
	}
}

void DHCPprocessor(const struct DHCPheader *const restrict dhcp) {
	if(dhcp->xid == expectedID) {
		const uint8_t *const messageType = getDHCPoption(dhcp, 53); // Get option 53, which is message type
		printf("Got DHCP message type %u\n", messageType[1]);
		switch(state) { // based on our current state, we expect different received messages
			case SELECTING: { // Expecting offer
				if(messageType[1] == OFFER) { // Accept the offer, send request message
					printf("Received DHCP offer\n");
					const uint8_t *const server = getDHCPoption(dhcp, 54); // Get server identifier
					const struct DHCPheader packet = {.op = 1, .HTYPE = 1, .HLEN = 6, .hops = 0, .xid = dhcp->xid,
								.secs = 0, .flags = 0, .clientIP = {{0}}, .yourIP = {{0}}, 
								.serverIP = {{0}}, .gatewayIP = {{0}}, .clientHW = unicastMAC, 
								.padding = {{0}}, .serverName = {{0}}, .bootfile = {{0}}, 
								.magicCookie = {{0x63, 0x82, 0x53, 0x63}}};

					const uint8_t options[] = {53, 1, REQUEST, // message type is request
											   12, 6, 'A', 'V', 'R', 'w', 'e', 'b', // host name (Mac sends it without null)
											   50, 4, dhcp->yourIP.addr[0], dhcp->yourIP.addr[1], dhcp->yourIP.addr[2], dhcp->yourIP.addr[3], // requested addr
											   61, 7, 1, unicastMAC.addr[0], unicastMAC.addr[1], unicastMAC.addr[2], unicastMAC.addr[3], unicastMAC.addr[4], unicastMAC.addr[5], // client identifier
											   54, 4, server[1], server[2], server[3], server[4], // server identifier
											   55, 3, 3, 58, 59, // request routers and lease times
											   0xFF};
					const struct UDPheader udp = {.srcPort = PORT_DHCP_CLIENT, .destPort = PORT_DHCP_SERVER, 
											  .length = sizeof(udp) + sizeof(packet) + sizeof(options), 
											  .checksum = 0};

					state = REQUESTING;
					expectedID = packet.xid;

					sendIPv4packet(&broadcastIP, &(const struct IPv4){{0, 0, 0, 0}}, PROTO_UDP, udp.length, 3, 
									LAYERS({&udp, sizeof(udp)},
										   {&packet, sizeof(packet)},
										   {options, sizeof(options)}));

				}
				break;
			}
			case REBINDING:
			case RENEWING:
			case REBOOTING:
			case REQUESTING: { // Expecting DHCP_ACK or NAK
				if(messageType[1] == DHCP_ACK) {
					localIP = dhcp->yourIP; // Copy over the assigned IP
					const uint8_t *const t1val = getDHCPoption(dhcp, 58);
					if(t1val != NULL) {
						T1 = RTCsetTimer(((struct TimerVal *)&t1val[1])->val);
					}
					const uint8_t *const t2val = getDHCPoption(dhcp, 59);
					if(t2val != NULL) {
						if(state == RENEWING) // T2 timer has not expired yet
							RTCresetTimer(T2, ((struct TimerVal *)&t2val[1])->val);
						else // T2 timer has not been set initially yet
							T2 = RTCsetTimer(((struct TimerVal *)&t2val[1])->val); // Extract T2 timer values
					}
					const uint8_t *const routerList = getDHCPoption(dhcp, 3);
					if(routerList != NULL) {
						const struct IPv4 *const serverip = (struct IPv4 *)&routerList[1];
						routerIP = *serverip;
					}
					else {
						const uint8_t *const server = getDHCPoption(dhcp, 54); // Get server identifier
						const struct IPv4 *const serverip = (struct IPv4 *)&server[1];
						routerIP = *serverip;
					}
					const struct DHCPdata updated = {localIP, routerIP};
					eeprom_update_block(&updated, DHCP_EEPROM, sizeof(updated));
					printf("Bound on address %u.%u.%u.%u\n", localIP.addr[0], localIP.addr[1], localIP.addr[2], localIP.addr[3]);
					state = BOUND;
					arpRequest(&routerIP); // Get router MAC in the table
				}
				else if(messageType[1] == NAK) {// server retracted its offer
					state = INIT;
					DHCPsendInit(); // go back to send discover message
				}
				break;
			}
			// We don't expect to receive DHCP packets when in the following states
			case BOUND: 
			case INIT:
			case INIT_REBOOT:
				break;
		}
	}
}

static const void *getDHCPoption(const void *const dhcp, const uint8_t num) { // Returns address of length byte of that option
	const uint8_t *const options = (uint8_t *)dhcp + sizeof(struct DHCPheader);
	for(uint16_t i = 0; options[i] != 0xFF; i++)
		if(options[i] != 0) { // if not padding
			if(options[i] == num) // the option we're looking for
				return &options[i+1]; // Return address of next val which is length of option
			else 
				i += options[i+1] + 1; // Advance i to end of this current option
		}
	return NULL;
}
