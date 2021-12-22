#ifndef DHCP_H
#define DHCP_H
#ifdef __cplusplus
extern "C" {
#endif

extern void DHCPsetup(void);
extern uint8_t DHCPready(void);
extern void DHCPprocessor(const void *const restrict ip, const struct DHCPheader *const restrict dhcp);

#ifdef __cplusplus
}
#endif
#endif // DHCP_H