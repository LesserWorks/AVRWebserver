#ifndef ENC28J60_MACROS_H
#define ENC28J60_MACROS_H
#ifdef __cplusplus
extern "C" {
#endif


// Macro functions for USART in SPI mode
#define SerialInit() (UBRR1L = 0, UBRR1H = 0, UCSR1C = (1 << UMSEL11) | (1 << UMSEL10), \
  				   	 UCSR1B = (1 << RXEN1) | (1 << TXEN1))
#define SerialTX(data) UDR1 = (data); while(!(UCSR1A & (1 << UDRE1)));
#define SerialTXend() UCSR1A = 1 << TXC1; while(!(UCSR1A & (1 << TXC1)));

inline uint8_t SerialRX(void) 
{
	UDR1 = 0;
	while(!(UCSR1A & (1 << RXC1)));
	return UDR1;
}
inline uint8_t SerialRXend(void)
{
	while(!(UCSR1A & (1 << RXC1)));
	return UDR1;
}
#define SerialRXflush() while(UCSR1A & (1 << RXC1)) { UDR1; }
	
struct __attribute__((packed)) RXstatusVector // Data written by ENC28J60 before the received packet
{
	uint16_t length; // Length of packet in bytes starting with dest MAC and including CRC
	uint8_t // Bits 16-23
		longDropEvent : 1, // LSB
		res1 : 1,
		carrierEvent : 1,
		res2 : 1,
		CRCerror : 1,
		lengthError : 1,
		lengthOutOfRange : 1,
		receivedOK : 1;
	uint8_t // bits 24-31
		multicast : 1,
		broadcast : 1,
		dribble : 1,
		controlFrame : 1,
		pauseFrame : 1,
		unknownOpcode : 1,
		VLANframe : 1,
		zero : 1;
}; 
// 0, 7, 15, 23, 31
#define RX_BUF_ST 0U // Must be 0 because of errata
#define RX_BUF_END 4573U // Must be odd number
#define TX_BUF_ST 4574U // Must be even number
#define PACKET_ST (TX_BUF_ST + 1)
#define BUF_END 8191U // Last address
#define BUF_LEN 8192U // 8 kilobytes
#define ETHERNET_LEN 14U // Two MAC addresses and ethertype field
#define IP_LEN 40U
#define ICMP_RS_LEN 16U // Includes source address option
#define ICMP_RA_LEN 16U // Does not include options
#define ICMP_NS_LEN 32U // Includes source address option
#define ICMP_NA_LEN 32U // Includes target address option
#define ICMP_PSEUDO_LEN 40U
#define TCP_LEN 20U
#define TCP_PSEUDO_LEN 40U // Conveniently, the TCP and ICMP pseudo lens are the same as the IP len
// Locations in buffer of prebuilt packet headers
#define IP_LOC (BUF_LEN - IP_LEN) // 8192 - 40 = 8152
#define TCP_LOC (IP_LOC - TCP_LEN) // 8132
#define TCP_PSEUDO_LOC (TCP_LOC - TCP_PSEUDO_LEN) // 8092
#define ICMP_PSEUDO_LOC (TCP_PSEUDO_LOC - ICMP_PSEUDO_LEN) // 8052
#define ICMP_RS_LOC (ICMP_PSEUDO_LOC - ICMP_RS_LEN) // 8036
#define ICMP_NS_LOC (ICMP_RS_LOC - ICMP_NS_LEN) // 8004
#define ICMP_NA_LOC (ICMP_NS_LOC - ICMP_NA_LEN) // 7972

#define SS_PORT PORTD
#define SS_PIN 5
#define RST_PORT PORTD		
#define RST_PIN 7
#define SS_low() SS_PORT &= ~(1 << SS_PIN);
#define SS_high() SS_PORT |= (1 << SS_PIN);
#define RST_low() RST_PORT &= ~(1 << RST_PIN);
#define RST_high() RST_PORT |= (1 << RST_PIN);

#define SYSTEM_RESET 1
#define TX_RESET 2
#define RX_RESET 3
#define PHY_RESET 4
#define HARD_RESET 5

/* SPI opcodes */
#define RCR(address) ((address) & 0b00011111U)
#define RBM 0b00111010U
#define WCR(address) (((address) & 0b00011111U) | 0b01000000U)
#define WBM 0b01111010U
#define BFS(address) (((address) & 0b00011111U) | 0b10000000U)
#define BFC(address) (((address) & 0b00011111U) | 0b10100000U)
#define SRC 0b11111111U
#define PHY(address) ((address) & 0b00011111U)

// Highest address in each bank is 0x1F = 0b00011111
#define BANK0 0b00000000U
#define BANK1 0b00100000U
#define BANK2 0b01000000U
#define BANK3 0b01100000U
#define BANK_MASK 0b01100000U
#define ALL_BANKS BANK0
#define MAI_REG 0b10000000U

/* PHY registers */
#define PHCON1  0x00U
#define PHSTAT1 0x01U
#define PHID1   0x02U
#define PHID2   0x03U
// Addresses 0x04-0x09 not used
#define PHCON2  0x10U
#define PHSTAT2 0x11U
#define PHIE    0x12U
#define PHIR    0x13U
#define PHLCON  0x14U // Bits 13-12 are reserved and must be written as 1

/* All banks */
// Address 0x1A is the equivalent of EDATA because the RBM and WBM opcodes require 0x1A as their address
#define EIE   (0x1BU | ALL_BANKS)
#define EIR   (0x1CU | ALL_BANKS)
#define ESTAT (0x1DU | ALL_BANKS)
#define ECON2 (0x1EU | ALL_BANKS)
#define ECON1 (0x1FU | ALL_BANKS)

/* Bank 0 */
#define ERDPTL   (0x00U | BANK0)
#define ERDPTH   (0x01U | BANK0)
#define EWRPTL   (0x02U | BANK0)
#define EWRPTH   (0x03U | BANK0)
#define ETXSTL   (0x04U | BANK0)
#define ETXSTH   (0x05U | BANK0)
#define ETXNDL   (0x06U | BANK0)
#define ETXNDH   (0x07U | BANK0)
#define ERXSTL   (0x08U | BANK0)
#define ERXSTH   (0x09U | BANK0)
#define ERXNDL   (0x0AU | BANK0)
#define ERXNDH   (0x0BU | BANK0)
#define ERXRDPTL (0x0CU | BANK0)
#define ERXRDPTH (0x0DU | BANK0)
#define ERXWRPTL (0x0EU | BANK0)
#define ERXWRPTH (0x0FU | BANK0)
#define EDMASTL  (0x10U | BANK0)
#define EDMASTH  (0x11U | BANK0)
#define EDMANDL  (0x12U | BANK0)
#define EDMANDH  (0x13U | BANK0)
#define EDMADSTL (0x14U | BANK0)
#define EDMADSTH (0x15U | BANK0)
#define EDMACSL  (0x16U | BANK0)
#define EDMACSH  (0x17U | BANK0)
// Addresses 0x18-0x19 not used

/* Bank 1 */
#define EHT0    (0x00U | BANK1)
#define EHT1    (0x01U | BANK1)
#define EHT2    (0x02U | BANK1)
#define EHT3    (0x03U | BANK1)
#define EHT4    (0x04U | BANK1)
#define EHT5    (0x05U | BANK1)
#define EHT6    (0x06U | BANK1)
#define EHT7    (0x07U | BANK1)
#define EPMM0   (0x08U | BANK1)
#define EPMM1   (0x09U | BANK1)
#define EPMM2   (0x0AU | BANK1)
#define EPMM3   (0x0BU | BANK1)
#define EPMM4   (0x0CU | BANK1)
#define EPMM5   (0x0DU | BANK1)
#define EPMM6   (0x0EU | BANK1)
#define EPMM7   (0x0FU | BANK1)
#define EPMCSL  (0x10U | BANK1)
#define EPMCSH  (0x11U | BANK1)
// Addresses 0x12-0x13 not used
#define EPMOL   (0x14U | BANK1)
#define EPMOH   (0x15U | BANK1)
#define EWOLIE  (0x16U | BANK1) // Deprecated, do not use
#define EWOLIR  (0x17U | BANK1) // Deprecated, do not use
#define ERXFCON (0x18U | BANK1)
#define EPKTCNT (0x19U | BANK1)

/* Bank 2 */
#define MACON1   (0x00U | BANK2 | MAI_REG)
#define MACON2   (0x01U | BANK2 | MAI_REG) // Deprecated, do not use
#define MACON3   (0x02U | BANK2 | MAI_REG)
#define MACON4   (0x03U | BANK2 | MAI_REG)
#define MABBIPG  (0x04U | BANK2 | MAI_REG)
// Address 0x05 not used
#define MAIPGL   (0x06U | BANK2 | MAI_REG)
#define MAIPGH   (0x07U | BANK2 | MAI_REG)
#define MACLCON1 (0x08U | BANK2 | MAI_REG)
#define MACLCON2 (0x09U | BANK2 | MAI_REG)
#define MAMXFLL  (0x0AU | BANK2 | MAI_REG)
#define MAMXFLH  (0x0BU | BANK2 | MAI_REG)
// Address 0x0C reserved
#define MAPHSUP  (0x0DU | BANK2 | MAI_REG) // Deprecated, do not use
// Address 0x0E reserved
// Address 0x0F not used
// Address 0x10 reserved
#define MICON    (0x11U | BANK2 | MAI_REG) // Deprecated, do not use
#define MICMD    (0x12U | BANK2 | MAI_REG)
// Address 0x13 not used
#define MIREGADR (0x14U | BANK2 | MAI_REG)
// Address 0x15 reserved
#define MIWRL    (0x16U | BANK2 | MAI_REG)
#define MIWRH    (0x17U | BANK2 | MAI_REG)
#define MIRDL    (0x18U | BANK2 | MAI_REG)
#define MIRDH    (0x19U | BANK2 | MAI_REG)

/* Bank 3 */
#define MAADR5  (0x00U | BANK3 | MAI_REG)
#define MAADR6  (0x01U | BANK3 | MAI_REG)
#define MAADR3  (0x02U | BANK3 | MAI_REG)
#define MAADR4  (0x03U | BANK3 | MAI_REG)
#define MAADR1  (0x04U | BANK3 | MAI_REG)
#define MAADR2  (0x05U | BANK3 | MAI_REG)
#define EBSTSD  (0x06U | BANK3 | MAI_REG)
#define EBSTCON (0x07U | BANK3 | MAI_REG)
#define EBSTCSL (0x08U | BANK3 | MAI_REG)
#define EBSTCSH (0x09U | BANK3 | MAI_REG)
#define MISTAT  (0x0AU | BANK3 | MAI_REG)
// Addresses 0x0B-0x11 not used
#define EREVID  (0x12U | BANK3)
// Addresses 0x13-0x14 not used
#define ECOCON  (0x15U | BANK3)
// Address 0x16 reserved
#define EFLOCON (0x17U | BANK3)
#define EPAUSL  (0x18U | BANK3)
#define EPAUSH  (0x19U | BANK3)

/* Bit defines */
// EIE
#define INTIE  7U
#define PKTIE  6U
#define DMAIE  5U
#define LINKIE 4U
#define TXIE   3U
#define WOLIE  2U // Deprecated, do not use // Wake on LAN interupt enable
#define TXERIE 1U
#define RXERIE 0U
// EIR
#define PKTIF  6U
#define DMAIF  5U
#define LINKIF 4U
#define TXIF   3U
#define WOLIF  2U // Deprecated, do not use // Wake on LAN interrupt flag
#define TXERIF 1U
#define RXERIF 0U
// ESTAT
#define INT     7U
#define BUFER   6U
#define LATECOL 4U
#define RXBUSY  2U
#define TXABRT  1U
#define CLKRDY  0U // Called PHYRDY in on-chip J60 module
// ECON2
#define AUTOINC 7U
#define PKTDEC  6U
#define PWRSV   5U // Called ETHEN in on-chip J60 module
#define VRPS    3U
// ECON1
#define TXRST  7U
#define RXRST  6U
#define DMAST  5U
#define CSUMEN 4U
#define TXRTS  3U
#define RXEN   2U
#define BSEL1  1U
#define BSEL0  0U
// EWOLIE // Deprecated, do not use
#define UCWOLIE 7U // Unicast wake on LAN interrupt enable
#define AWOLIE  6U // Any packet wake on LAN interrupt enable
#define PMWOLIE 4U // Pattern match wake on LAN interrupt enable
#define MPWOLIE 3U // etc.
#define HTWOLIE 2U
#define MCWOLIE 1U
#define BCWOLIE 0U
// EWOLIR // Deprecated, do not use
#define UCWOLIF 7U // Unicast wake on LAN interrupt flag
#define AWOLIF  6U // Any packet wake on LAN interrupt flag
#define PMWOLIF 4U // Pattern match wake on LAN interrupt flag
#define MPWOLIF 3U // etc
#define HTWOLIF 2U
#define MCWOLIF 1U
#define BCWOLIF 0U
// ERXFCON
#define UCEN  7U
#define ANDOR 6U
#define CRCEN 5U
#define PMEN  4U
#define MPEN  3U
#define HTEN  2U
#define MCEN  1U
#define BCEN  0U
// MACON1
#define TXPAUS  3U
#define RXPAUS  2U
#define PASSALL 1U
#define MARXEN  0U
// MACON2 // Deprecated, do not use
#define MARST   7U // MAC reset
#define RNDRST  6U // Random number generator reset
#define MARXRST 3U // MAC control sublayer and receive logic reset
#define RFUNRST 2U // Receive function logic reset
#define MATXRST 1U // MAC control sublayer and transmit logic reset
#define TFUNRST 0U // Transmit function logic reset
// MACON3
#define PADCFG2 7U
#define PADCFG1 6U
#define PADCFG0 5U
#define TXCRCEN 4U
#define PHDREN  3U
#define HFRMEN  2U
#define FRMLNEN 1U
#define FULDPX  0U
// MACON4
#define DEFER   6U
#define BPEN    5U
#define NOBKOFF 4U
#define LONGPRE 1U // Deprecated, do not use // Long preamble enforcement enable (12+ preamble bytes)
#define PUREPRE 0U // Deprecated, do not use // Pure preamble enforcement enable (checked against 0x55)
// MAPHSUP // Deprecated, do not use
#define RSTINTFC 7U // Interface module reset
#define RSTRMII  3U // RMII module reset
// MICON // Deprecated, do not use
#define RSTMII 7U // MII module reset
// MICMD
#define MIISCAN 1U
#define MIIRD   0U
// EBSTCON
#define PSV2   7U
#define PSV1   6U
#define PSV0   5U
#define PSEL   4U
#define TMSEL1 3U
#define TMSEL0 2U
#define TME    1U
#define BISTST 0U
// MISTAT
#define NVALID 2U
#define SCAN   1U
#define BUSY   0U
// ECOCON
#define COCON2 2U
#define COCON1 1U
#define COCON0 0U
// EFLOCON
#define FULDPXS 2U
#define FCEN1   1U
#define FCEN0   0U
// PHCON1
#define PRST    7U
#define PLOOPBK 6U
#define PPWRSV  3U
#define PDPXMD  0U
// PHSTAT1
#define PFDPX  4U
#define PHDPX  3U
#define LLSTAT 2U
#define JBSTAT 1U
// PHCON2
#define FRCLNK 6U
#define TXDIS  5U
#define JABBER 2U
#define HDLDIS 0U
// PHSTAT2
#define TXSTAT  5U
#define RXSTAT  4U
#define COLSTAT 3U
#define LSTAT   2U
#define DPXSTAT 1U
#define PLRITY  5U
// PHIE
#define PLNKIE 4U
#define PGEIE  1U
// PHIR
#define PLNKIF 4U
#define PGIF   2U
// PHLCON // Bits 13-12 are reserved and must be written as 1
#define LACFG3 3U
#define LACFG2 2U
#define LACFG1 1U
#define LACFG0 0U
#define LBCFG3 7U
#define LBCFG2 6U
#define LBCFG1 5U
#define LBCFG0 4U
#define LFRQ1  3U
#define LFRQ0  2U
#define STRCH  1U
// Control byte bit defines 
#define PHUGEEN   3U
#define PPADEN    2U
#define PCRCEN    1U
#define POVERRIDE 0U
	
#ifdef __cplusplus
}
#endif
#endif // ENC28J60_MACROS_H