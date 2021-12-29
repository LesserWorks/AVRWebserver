#ifndef DHCP_H
#define DHCP_H
#ifdef __cplusplus
extern "C" {
#endif

extern void DHCPsetup(void);
extern uint8_t DHCPready(void);
extern void DHCPprocessor(const struct DHCPheader *const restrict dhcp);
extern void handleDHCPtimers(void);

#ifdef __cplusplus
}
#endif
#endif // DHCP_H