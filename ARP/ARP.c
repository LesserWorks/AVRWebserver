#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <avr/io.h>
#include <util/delay.h>
#include "HeaderStructs/HeaderStructs.h"
#include "ENC28J60_macros/ENC28J60_macros.h"
#include "ENC28J60_functions/ENC28J60_functions.h"
#include "WebserverDriver/WebserverDriver.h"
#include "ARP.h"

#define ARP_REQUEST 1
#define ARP_REPLY 2

#if ARP_TABLE_LEN > 255
#error "ARP table too big"
#endif

struct ARPentry {
	struct MAC mac;
	struct IPv4 ip;
	uint8_t priority;
};
static struct ARPentry arpTable[ARP_TABLE_LEN] = {0};
static uint8_t nextSlot = 0;

const struct MAC *arp(const void *const target)
{
	for(uint8_t i = 0; i < ARP_TABLE_LEN; ++i) // Search table
		if(arpTable[i].priority && !memcmp(target, &arpTable[i].ip, sizeof(struct IPv4)))
			return &arpTable[i].mac;
	// If we get here, the given IP wasn't in the ARP table
		/*
	const struct ARP arpRequest = {1, 0x0800, 6, 4, ARP_REQUEST, unicastMAC, localIP, zeroMAC, *(const struct IPv4 *)target};
	sendEthernetFrame(&broadcastMAC, &unicastMAC, ETHER_ARP, &arpRequest, sizeof(struct ARP), 0, NULL); // Send arp request for given IP
	_delay_ms(ARP_WAIT_TIME); // Should replace this with timer eventually
	while(packetPending())
		packetHandler();
	for(uint8_t i = 0; i < ARP_TABLE_LEN; ++i) // Search table again
		if(arpTable[i].priority && !memcmp(target, &arpTable[i].ip, sizeof(struct IPv4)))
			return &arpTable[i].mac;
			*/
	return NULL; // Didn't find it even after sending ARP
}

const void arpRequest(const void *const target) {
	const struct ARP arpRequest = {1, 0x0800, 6, 4, ARP_REQUEST, unicastMAC, localIP, zeroMAC, *(const struct IPv4 *)target};
	sendEthernetFrame(&broadcastMAC, &unicastMAC, ETHER_ARP, &arpRequest, sizeof(struct ARP), 0, NULL); // Send arp request for given IP
}

void ARPprocessor(const struct ARP *const arp) { // Responds to an incoming arp request
	printf("ARP target: %u.%u.%u.%u, opcode: %u\n", arp->targetIP.addr[0], arp->targetIP.addr[1], arp->targetIP.addr[2], arp->targetIP.addr[3], arp->op);
	if(arp->op == ARP_REQUEST && memcmp(&arp->targetIP, &localIP, sizeof(struct IPv4)) == 0) { // Are we the target of this ARP request?
		const struct ARP arpReply = {1, 0x0800, 6, 4, ARP_REPLY, unicastMAC, localIP, arp->srcMAC, arp->srcIP};
		sendEthernetFrame(&arp->srcMAC, &unicastMAC, ETHER_ARP, &arpReply, sizeof(struct ARP), 0, NULL);
	}
	// If incoming arp does not have zeros for src IP and MAC
	if(memcmp(&arp->srcIP, &(struct IPv4){{0}}, sizeof(struct IPv4)) != 0 && memcmp(&arp->srcMAC, &zeroMAC, sizeof(struct MAC)) != 0) {
		for(uint8_t i = 0; i < ARP_TABLE_LEN; i++) {
			if(arpTable[i].priority && !memcmp(&arp->srcIP, &arpTable[i].ip, sizeof(struct IPv4))) {
				arpTable[i].mac = arp->srcMAC;
				arpTable[i].priority = 1;
			}
		}
		// Didn't already have entry in table
		if(memcmp(&arpTable[nextSlot].ip, &routerIP, sizeof(struct IPv4)) == 0) // Don't overwrite router MAC
			nextSlot = (nextSlot + 1) % ARP_TABLE_LEN;
		arpTable[nextSlot].ip = arp->srcIP;
		arpTable[nextSlot].mac = arp->srcMAC;
		arpTable[nextSlot].priority = 1;
		nextSlot = (nextSlot + 1) % ARP_TABLE_LEN;
	}
}

void claimIP(const void *const ip) { // Sends an arp announcement
	const struct ARP arpAnnouncement = {1, 0x0800, 6, 4, ARP_REQUEST, unicastMAC, *(const struct IPv4 *)ip, 
																		zeroMAC, *(const struct IPv4 *)ip};
	sendEthernetFrame(&broadcastMAC, &unicastMAC, ETHER_ARP, &arpAnnouncement, sizeof(struct ARP), 0, NULL);
}
