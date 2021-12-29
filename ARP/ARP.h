#ifndef ARP_H
#define ARP_H
#ifdef __cplusplus
extern "C" {
#endif

#define ARP_WAIT_TIME 500 // milliseconds it should wait (blocking) to receive an ARP response
#define ARP_TABLE_LEN 20 // Entries in ARP table

extern const struct MAC *arp(const void *const target);
extern void arpRequest(const void *const target);
extern void ARPprocessor(const struct ARP *const arp);
extern void claimIP(const void *const ip);


#ifdef __cplusplus
}
#endif
#endif // ARP_H