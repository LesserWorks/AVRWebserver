#ifndef CHECKSUM_H
#define CHECKSUM_H
#ifdef __cplusplus
extern "C" {
#endif

extern uint16_t checksumUpdate(uint16_t context, const void *data, uint16_t len);
extern uint16_t checksumUnrolled(void *data, void *end);

// This version written in C, lenBytes must be even
//extern uint16_t checksumC(const uint8_t data[], const uint16_t lenBytes);


#ifdef __cplusplus
}
#endif
#endif