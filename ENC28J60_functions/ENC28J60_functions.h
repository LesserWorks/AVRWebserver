#ifndef ENC28J60_FUNCTIONS_H // Include guard
#define ENC28J60_FUNCTIONS_H
#ifdef __cplusplus
extern "C" {
#endif
	
#define setBank0()
#define setBank1()
#define setBank2()
#define setBank3()
#define WriteWord(reg, data) (WriteReg(reg##L, (data) & 255U), WriteReg(reg##H, (data) >> 8))
#define ReadWord(reg) (ReadReg(reg##L) | (uint16_t)ReadReg(reg##H) << 8)
#define disableInt2() EIMSK &= ~(1 << INT2)
#define enableInt2() EIMSK |= (1 << INT2)


extern void WriteReg(const uint8_t registerName, const uint8_t data);

extern void WriteRegDelayed(const uint8_t registerName, const uint8_t data);

extern uint8_t ReadReg(const uint8_t registerName);

extern void SetRegBit(const uint8_t registerName, const uint8_t data);

extern void ClearRegBit(const uint8_t registerName, const uint8_t data);

extern void WritePHY(const uint8_t registerName, const uint8_t dataH, const uint8_t dataL);

extern uint16_t ReadPHY(const uint8_t registerName);

extern void StartPHYscan(const uint8_t registerName);

extern uint8_t ReadPHYscan(const uint8_t whichByte);

extern void StopPHYscan(void);

extern void IPv6reset(const uint8_t resetType);

extern void DMAcopy(const uint16_t srcStart, const uint16_t srcStop, const uint16_t dest);

extern void DMAchecksum(const uint16_t start, const uint16_t stop);

extern uint16_t getChecksum(void);

extern void sendEthernetFrame(const struct MAC *const dest, const struct MAC *const src, const uint16_t ethertype, 
					   const void *const firstData, const uint16_t firstLen, const uint8_t layers, const struct Layer payload[]);

extern uint16_t getFrameSize(void);

extern void readFrame(uint8_t buffer[], const uint16_t len);

extern uint8_t packetPending(void);

extern void startPauseFrames(void);

extern void stopPauseFrames(void);

extern uint16_t freeBufferSpace(void);

extern void addMACtoTable_R(const uint8_t mac[6]); // Operates on RAM

extern void addMACtoTable_P(const uint8_t mac[6]); // Operates on PROGMEM


#ifdef __cplusplus
}
#endif
#endif // ENC28J60_FUNCTIONS_H
/* // CRC32 function that actually works: https://ideone.com/r6JL7R
uint32_t enc624j600CalcCrc(const void *data, size_t length)
{
	uint8_t i;
    uint8_t j;
     //Point to the data over which to calculate the CRC
    const uint8_t *p = (uint8_t *) data;
      //CRC preset value
    uint32_t crc = 0xFFFFFFFF;
   
      //Loop through data
    for(i = 0; i < length; i++)
    {
         //The message is processed bit by bit
		for(j = 0; j < 8; j++)
        {
            //Update CRC value
			if(((crc >> 31) ^ (p[i] >> j)) & 0x01)
               crc = (crc << 1) ^ 0x04C11DB7;
            else
               crc = crc << 1;
        }
    }
      //Return CRC value
	return crc;
}

uint32_t CRC32(const uint8_t data[], const uint8_t len)
{
	uint32_t runningCRC = 0xFFFFFFFF; // Holds the CRC as it is computed
	for(uint8_t i = 0; i < len; ++i) // Goes through every data member
	{
		for(uint8_t j = 0; j < 8; ++j) // Goes through 8 bits (0-7)
		{
			const uint8_t MSB = runningCRC >> 31; // Get MSB of 32 bit CRC value
			const uint8_t xorVal = MSB ^ (data[i] >> j); // XOR 0000000X with data member shifted from 0 to 7 bits over
			// Upper 7 bits will be unmodified.
			// If CRC MSB was a 1, data LSB will be toggled, else unmodified
			const uint8_t andVal = xorVal & 1; // Clear all bits except LSB
			// Under what circumstances will the LSB be a 1?
			// If the data byte shifted over LSB is a 1, and CRC MSB is a 1, then LSB is 0.
			// If the data byte shifted over LSB is a 1, and CRC MSB is a 0, then LSB is 1.
			// If the data byte shifted over LSB is a 0, and CRC MSB is a 1, then LSB is 1.
			// If the data byte shifted over LSB is a 0, and CRC MSB is a 0, then LSB is 0.
			if(andVal) // Is the andVal LSB a 1?
			{ // 0x04C11DB7 == 0000 0100 1100 0001 0001 1101 1011 0111
				runningCRC = (runningCRC << 1) ^ 0x04C11DB7; // Shift to left by one, clearing LSB
				// Then XOR with polynomial, LSB will always be set
			}
			else // andVal LSB was a 0.
			{
				runningCRC <<= 1; // Shift to the left by one, clearing LSB
			}
		}
	}
	return runningCRC;
}
// All nodes: 33:33:0:0:0:1 hash = 0xF99BAABA Pointer = 110011 = 0x33 = EHT6<3>
// SNM is FF02:0:0:0:0:1:FF then last 3 bytes of IP address
// Multicast MAC is 33:33 then last 4 bytes of IP
// Using stateless autoconfiguration, SNM is FF02:0:0:0:0:1:FF:MAC2:MAC1:MAC0
// MAC for SNM is 33:33:FF:MAC2:MAC1:MAC0
// Using MAC2 = B6, MAC1 = CC, and MAC0 = AE
// Then SNM is FF02:0:0:0:0:1:FFB6:CCAE
// Then MAC for SNM is 33:33:FF:B6:CC:AE hash = 0xEFDEA5C6 Pointer = 011111 = 0x1F = EHT3<7>
*/
/*
Start with MAC address, then make EUI-64 whose last 3 bytes are the same as last 3 bytes of MAC address.
Add FE80:0:0:0 to beginning to get link-local address of FE80:0:0:0:EUI-64
Then get group ID from RA to make globally unique IP address of GroupID:EUI-64
*/
/*
Ping in IPv6: Host A wants to ping Host B. Host A knows its unicast IPv6 address and its unicast MAC address.
It also knows Host B's unicast IPv6 address because it was told to ping that address. However, it does not know
Host B's unicast MAC address. So Host A first sends a neighbor solicitation with its own unicast IPv6 and MAC addresses
as Source. For Destination IPv6 address, it uses Host B's SNM address. For Destination MAC, it uses Host B's SNM MAC.
Host B replies with neighbor advertisement using all unicast addresses. 
Then Host A sends the Echo Request with all unicast addresses.
*/