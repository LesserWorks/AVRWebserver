#include <stdint.h>
#include "Checksum.h"

uint16_t checksumC(const uint8_t data[], const uint16_t lenBytes) {
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