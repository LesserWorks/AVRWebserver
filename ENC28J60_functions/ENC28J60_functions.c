#include <stdint.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "HeaderStructs/HeaderStructs.h"
#include "ENC28J60_macros/ENC28J60_macros.h"
#include "ENC28J60_functions.h"
#ifndef F_CPU
#error "F_CPU"
#endif

static inline uint32_t CRC32(const uint8_t data[], const uint8_t len);
static void checkBank(const uint8_t registerName);
static void writeBuffer(const uint8_t *const data, const uint16_t len);
static void readBuffer(uint8_t dest[], const uint16_t len);
static uint16_t npp = RX_BUF_ST;

void WriteReg(const uint8_t registerName, const uint8_t data)
{
	checkBank(registerName);
  	SS_low();
  	SerialTX(WCR(registerName));
  	SerialTX(data);
  	SerialTXend();
  	if(registerName & MAI_REG)
  	{
  		// See page 82 of datasheet for following values
  	__asm__ volatile("rjmp .+0\n\t" // Waste 4 clock cycles before raising SS.
               	"rjmp .+0\n\t" // We need 210 ns from last SCK cycle to SS high when accessing MAC or MII registers
                ::);
  	}
  	SS_high();
  	return;
}
// Use when writing to MAC or MII registers // Deprecated
void WriteRegDelayed(const uint8_t registerName, const uint8_t data) 
{
	checkBank(registerName);
  	SS_low();
  	SerialTX(WCR(registerName));
  	SerialTX(data);
  	SerialTXend();
  	// See page 82 of datasheet for following values
  	__asm__ volatile("rjmp .+0\n\t" // Waste 4 clock cycles before raising SS.
               	"rjmp .+0\n\t" // We need 210 ns from last SCK cycle to SS high when accessing MAC or MII registers
                ::);
  	SS_high(); // This take 2 cycles (sbi)
  	return;
}
// Also a minimum of 50 ns is required between raising SS and lowering SS for next transmission
// Since the fastest way we have to toggle SS is sbi, cbi, and each one takes at least 1 cycle, that's 50 ns right there.
uint8_t ReadReg(const uint8_t registerName)
{
		checkBank(registerName);
  	SerialRXflush();
  	SS_low();
  	SerialTX(RCR(registerName));
  	SerialRX();
  	if(registerName & MAI_REG)
  	{
  		SerialRX();
  	}
  	const uint8_t readData = SerialRXend();
  	SS_high();
  	return readData;
}

void SetRegBit(const uint8_t registerName, const uint8_t data)
{
	checkBank(registerName);
  	SS_low();
  	SerialTX(BFS(registerName));
  	SerialTX(data);
  	SerialTXend();
  	SS_high();
  	return;
}
void ClearRegBit(const uint8_t registerName, const uint8_t data)
{
	checkBank(registerName);
  	SS_low();
  	SerialTX(BFC(registerName));
  	SerialTX(data);
  	SerialTXend();
  	SS_high();
  	return;
}
void WritePHY(const uint8_t registerName, const uint8_t dataH, const uint8_t dataL)
{
  	while(ReadReg(MISTAT) & (1 << BUSY)); // Wait till BUSY bit clears
  	WriteReg(MIREGADR, registerName);
  	WriteReg(MIWRL, dataL);
  	WriteReg(MIWRH, dataH);
  	return;
}
uint16_t ReadPHY(const uint8_t registerName)
{
  while(ReadReg(MISTAT) & (1 << BUSY)); // Wait till BUSY bit clears
  WriteReg(MIREGADR, registerName);
  WriteReg(MICMD, 1 << MIIRD);
  while(ReadReg(MISTAT) & (1 << BUSY)); // Wait till BUSY bit clears
  WriteReg(MICMD, 0);
  return (((uint16_t)ReadReg(MIRDH)) << 8) | ReadReg(MIRDL);
}
void StartPHYscan(const uint8_t registerName)
{
  while(ReadReg(MISTAT) & (1 << BUSY)); // Wait till BUSY bit clears
  WriteReg(MIREGADR, registerName);
  WriteReg(MICMD, 1 << MIISCAN);
  while(ReadReg(MISTAT) & (1 << NVALID)); // Wait till NVALID bit clears
  return;
}
uint8_t ReadPHYscan(const uint8_t whichByte)
{
  if(whichByte) // 0, you get low byte, nonzero you get high byte
  {
    return ReadReg(MIRDH);
  }
  else
  {
    return ReadReg(MIRDL);
  }
}
void StopPHYscan(void)
{
  WriteReg(MICMD, 0);
  return;
}
void IPv6reset(const uint8_t resetType)
{
  switch(resetType)
  {
    case SYSTEM_RESET:
		SS_low();
      	SerialTX(SRC);
      	SerialTXend();
      	SS_high();
      	_delay_us(50);
      	return;
    case TX_RESET:
      	SetRegBit(ECON1, 1 << TXRST);
      	ClearRegBit(ECON1, 1 << TXRST);
      	return;
    case RX_RESET:
      	SetRegBit(ECON1, 1 << RXRST);
      	ClearRegBit(ECON1, 1 << RXRST);
      	return;
    case HARD_RESET:
		RST_low();
		_delay_us(1);
		RST_high();
      	return;
    case PHY_RESET:
	{
		uint16_t phyVal = ReadPHY(PHCON1);
		phyVal |= 1 << (PRST + 8); // Add 8 to PRST because it is designed for use in an 8 bit register
		WritePHY(PHCON1, phyVal >> 8, 0);
		while(ReadPHY(PHCON1) & (1 << (PRST + 8))); // Wait for PRST to clear
		return;
	}
  }  
}

void DMAcopy(const uint16_t srcStart, const uint16_t srcStop, const uint16_t dest)
{
	while(ReadReg(ECON1) & (1 << DMAST)); // Wait till DMAST clears
	ClearRegBit(ECON1, (1 << CSUMEN) | (1 << BSEL1) | (1 << BSEL0)); // DMA in copy mode and bank 0
	WriteWord(EDMAST, srcStart);
	WriteWord(EDMAND, srcStop);
	WriteWord(EDMADST, dest);
	SetRegBit(ECON1, 1 << DMAST);
	return;
}

void DMAchecksum(const uint16_t start, const uint16_t stop)
{
	while(ReadReg(ECON1) & (1 << DMAST)); // Wait till DMAST clears
	setBank0();
	SetRegBit(ECON1, 1 << CSUMEN);
	WriteWord(EDMAST, start);
	WriteWord(EDMAND, stop);
	while(ReadReg(ESTAT) & 1 << RXBUSY); // Wait till RX engine stops so we don't terminate any packets with next step
	ClearRegBit(ECON1, 1 << RXEN); // Must do this because of errata
	SetRegBit(ECON1, 1 << DMAST); 
	return;
}
uint16_t getChecksum(void)
{
	while(ReadReg(ECON1) & (1 << DMAST)); // Wait till DMAST clears
	SetRegBit(ECON1, 1 << RXEN); // Enable reception after DMA finishes
	setBank0();
	return ReadWord(EDMACS);
}

void sendEthernetFrame(const struct MAC *const dest, const struct MAC *const src, const uint16_t ethertype, 
					   const void *const firstData, const uint16_t firstLen, const uint8_t layers, const struct Layer payload[])
{
	printf("In sendEthernetFrame ");
	const struct EthernetFrame ether = {*dest, *src, ethertype};
	while(ReadReg(ECON1) & (1 << TXRTS)); // Wait till TX engine idle
	WriteWord(EWRPT, TX_BUF_ST); // Place write pointer at beginning of packet
	writeBuffer((uint8_t [1]){0}, 1); // Write zero for per-packet control byte
	writeBuffer((void *)&ether, sizeof(struct EthernetFrame)); // Write in ethernet header
	writeBuffer(firstData, firstLen); // Write the first block of data (usually ARP or IP)
	for(uint8_t i = 0; i < layers; i++) // Write all the additional blocks from the layer list
	{
		writeBuffer(payload[i].data, payload[i].len);
	}
	const uint16_t packetEnd = ReadWord(EWRPT);
	WriteWord(ETXND, packetEnd - 1); // Write pointer ends up right after packet
	while(ReadReg(ECON1) & (1 << DMAST)); // Wait till DMAST clears
	ClearRegBit(EIR, 1 << TXIF);
	SetRegBit(ECON1, 1 << TXRST);
	ClearRegBit(ECON1, 1 << TXRST); // Reset transmit logic
	SetRegBit(ECON1, 1 << TXRTS); // Send message
	printf("Sent\n");
}

uint8_t packetPending(void)
{
	return ReadReg(EPKTCNT);
}

uint16_t getFrameSize(void)
{
	//static uint16_t npp = RX_BUF_ST; // I think this function should always access the global npp since readFrame needs it
	if(!ReadReg(EPKTCNT)) return 0;
	WriteWord(ERDPT, npp); // Set read pointer to npp
	SerialRXflush();
  SS_low();
  SerialTX(RBM); // Send read buffer opcode
  SerialRX();
  npp = SerialRX();
  npp |= (uint16_t)SerialRX() << 8; // Read in new npp
  uint16_t rxBytes = SerialRX();
  rxBytes |= (uint16_t)SerialRX() << 8; // Read in number of received bytes from RX vector
  SerialRX();
  SerialRXend(); // Read in last two bytes of RX vector
  SS_high();
	return rxBytes - 4; // Subtract 4 due to CRC at end we don't care about
}
void readFrame(uint8_t buffer[], const uint16_t len)
{
  	readBuffer(buffer, len); // Read pointer better be right after rx vector at this point
  	if(npp != RX_BUF_ST) // Likely
		{
			WriteWord(ERXRDPT, npp - 1); // Move write protection pointer
		}
		else // Unlikely
		{
			WriteWord(ERXRDPT, RX_BUF_END);
		}
		SetRegBit(ECON2, 1 << PKTDEC); // Decrement EPKTCNT
		printf("LRF\n");
}
void startPauseFrames(void)
{
	SetRegBit(EFLOCON, 1 << FCEN1);
	ClearRegBit(EFLOCON, 1 << FCEN0);
	return;
}
void stopPauseFrames(void)
{
	SetRegBit(EFLOCON, (1 << FCEN1) | (1 << FCEN0));
	return;
}
uint16_t freeBufferSpace(void)
{
	const uint16_t readpt = ReadWord(ERXRDPT);
	uint16_t writept = 0;
	// Now perform an atomic read on ERXWRPT
	uint8_t prev = 0;
	do
	{
		prev = ReadReg(EPKTCNT);
		writept = ReadWord(ERXWRPT); // Reads both high and low bytes	
	}
	while(prev != ReadReg(EPKTCNT)); // Read again if a packet was received under our noses
	if(writept > readpt) // This if statement is taken straight from the datasheet
	{
		return (RX_BUF_END - RX_BUF_ST) - (writept - readpt);
	}
	else if(writept == readpt)
	{
		return RX_BUF_END - RX_BUF_ST;
	}
	else
	{
		return readpt - writept - 1;
	}
}
void addMACtoTable_R(const uint8_t mac[6]) // Operates on MAC address in RAM
{
	const uint32_t hash = CRC32(mac, 6);
	const uint8_t hashPtr = (uint8_t)((hash >> 23) & 0b000111111U); // Isolate hash table pointer
	SetRegBit(EHT0 + (hashPtr / 8), 1 << (hashPtr % 8)); // Set bit in hash table
	return;
}
void addMACtoTable_P(const uint8_t mac[6]) // Operates on MAC address in flash
{
	uint8_t ramMAC[6] = {0};
	for(uint8_t i = 0; i < 6; ++i)
	{
		ramMAC[i] = pgm_read_byte(&mac[i]); // Copy progmem MAC into ram to send to next function
	}
	const uint32_t hash = CRC32(ramMAC, 6);
	const uint8_t hashPtr = (uint8_t)((hash >> 23) & 0b000111111U); // Isolate hash table pointer
	SetRegBit(EHT0 + (hashPtr / 8), 1 << (hashPtr % 8)); // Set bit in hash table
	// We can add to EHT0 because it is just a #define for memory address and EHT1, EHT2, etc.
	// are sequential after it
	return;
}

extern inline uint8_t SerialRX(void);
// I think this function is copied
static inline uint32_t CRC32(const uint8_t data[], const uint8_t len) // Used to calculate hash table hashes
{
	uint32_t runningCRC = 0xFFFFFFFF; // Holds the CRC as it is computed
	for(uint8_t i = 0; i < len; ++i) // Goes through every data member
	{
		for(uint8_t j = 0; j < 8; ++j) // Goes through 8 bits (0-7)
		{
			const uint8_t MSB = runningCRC >> 31;
			const uint8_t xorVal = MSB ^ (data[i] >> j);
			const uint8_t andVal = xorVal & 1;
			
			if(andVal) 
			{ // 0x04C11DB7 == 0000 0100 1100 0001 0001 1101 1011 0111
				runningCRC = (runningCRC << 1) ^ 0x04C11DB7U; // Shift to left by one, clearing LSB
			}
			else // andVal LSB was a 0.
			{
				runningCRC <<= 1; // Shift to the left by one, clearing LSB
			}
		}
	}
	return runningCRC;
}
static void checkBank(const uint8_t registerName) 
{
	static uint8_t lastBank = BANK0;
	const uint8_t givenBank = registerName & BANK_MASK;
	if(givenBank - lastBank)
	{
		lastBank = givenBank;
		SS_low();
		SerialTX(BFS(ECON1));
		SerialTX(givenBank >> 5); // Shift bank bits which are in 6-5 to 1-0
		SerialTXend();
		SS_high();
		SS_low();
		SerialTX(BFC(ECON1));
		SerialTX(((~givenBank) & BANK_MASK) >> 5);
		SerialTXend();
		SS_high();
	}
	return;
}

static void writeBuffer(const uint8_t *const data, const uint16_t len)
{
	// This condition is critical since higher-level functions may pass zero-length data in that should not cause
	// any activity on the SPI lines
	if(len > 0) { 
		SS_low();
  	SerialTX(WBM); // Begin buffer write at wherever EWRPT is
  	for (uint16_t d = 0; d < len; ++d)
  	{
   		SerialTX(data[d]); // Do buffer write
  	}
  	SerialTXend();
  	SS_high();
  }
}

static void readBuffer(uint8_t dest[], const uint16_t len)
{
		printf("ERB-");
  	SerialRXflush();
  	SS_low();
  	SerialTX(RBM); // Send read buffer opcode
  	SerialRX();
  	uint16_t i = 0;
  	for (i = 0; i < len - 1; ++i)
  	{
  		dest[i] = SerialRX();
  	}
  	dest[i] = SerialRXend();
  	SS_high();
  	return;
}