#ifndef HEADER_STRUCTS_H // Include guard
#define HEADER_STRUCTS_H
#ifdef __cplusplus
extern "C" {
#endif


#define ETHER_ARP 0x0806
#define ETHER_IPv4 0x0800
#define ETHER_IPv6 0x86DD

#define PROTO_ICMPv4 1
#define PROTO_TCP 6
#define PROTO_UDP 17
#define PROTO_ICMPv6 58

#define PORT_DHCP_SERVER 67
#define PORT_DHCP_CLIENT 68
#define PORT_HTTP 80
#define PORT_HTTPS 443

#define PORTS(x) ((struct CommonPorts *)(x))

struct Layer 
{
	const void *const data;
	const uint16_t len;
};
#define LAYERS(...) ((const struct Layer []){__VA_ARGS__})

struct __attribute__((packed)) IPv4
{
	uint8_t addr[4];
};
struct __attribute__((packed)) MAC
{
	uint8_t addr[6];
};

extern struct MAC unicastMAC;
extern struct MAC broadcastMAC;
extern struct MAC zeroMAC;
extern struct IPv4 localIP;
extern struct IPv4 routerIP;

struct __attribute__((packed, scalar_storage_order("big-endian"))) EthernetFrame
{
	struct MAC destMAC;
	struct MAC srcMAC;
	uint16_t ethertype;
};
struct __attribute__((packed, scalar_storage_order("big-endian"))) ARP
{
	uint16_t HTYPE;
	uint16_t PTYPE;
	uint8_t HLEN;
	uint8_t PLEN;
	uint16_t op;
	struct MAC srcMAC;
	struct IPv4 srcIP;
	struct MAC targetMAC;
	struct IPv4 targetIP;
};

struct __attribute__((packed, scalar_storage_order("big-endian"))) IPv4header
{
	uint8_t version : 4, 
			iht : 4;
	uint8_t dscp : 6,
			ecn : 2;
	uint16_t length;
	uint16_t id;
	uint16_t zero : 1,
			 df : 1,
			 mf : 1,
			 offset : 13;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t checksum;
	struct IPv4 srcIP;
	struct IPv4 destIP;
};

struct __attribute__((packed, scalar_storage_order("big-endian"))) ICMPv4header
{
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint16_t id;
	uint16_t seq;
};

struct __attribute__((packed, scalar_storage_order("big-endian"))) UDPheader
{
	uint16_t srcPort;
	uint16_t destPort;
	uint16_t length;
	uint16_t checksum;
};

struct __attribute__((packed, scalar_storage_order("big-endian"))) TCPheader
{
	uint16_t srcPort;
	uint16_t destPort;
	uint32_t seq;
	uint32_t ack;
	uint16_t offset : 4,
			 zero : 3,
			 flags : 9;
	uint16_t window;
	uint16_t checksum;
	uint16_t urgent;
};
#define NS (1 << 8)
#define CWR (1 << 7)
#define ECE (1 << 6)
#define URG (1 << 5)
#define ACK (1 << 4)
#define PSH (1 << 3)
#define RST (1 << 2)
#define SYN (1 << 1)
#define FIN (1 << 0)

struct __attribute__((packed, scalar_storage_order("big-endian"))) TCPpseudoHeader
{
	struct IPv4 srcIP;
	struct IPv4 destIP;
	uint8_t zero;
	uint8_t protocol; // is always PROTO_TCP
	uint16_t length; // of actual TCP header and data together
};

struct __attribute__((packed, scalar_storage_order("big-endian"))) CommonPorts
{
	uint16_t srcPort;
	uint16_t destPort;
};

struct __attribute__((packed, scalar_storage_order("big-endian"))) DHCPheader
{
	uint8_t op;
	uint8_t HTYPE;
	uint8_t HLEN;
	uint8_t hops;
	uint32_t xid;
	uint16_t secs;
	uint16_t flags;
	struct IPv4 clientIP;
	struct IPv4 yourIP;
	struct IPv4 serverIP;
	struct IPv4 gatewayIP;
	struct MAC clientHW;
	struct {uint8_t padding[10];} padding; // 10 bytes of padding after MAC address
	struct {uint8_t serverName[64];} serverName;
	struct {uint8_t bootfile[128];} bootfile;
	struct IPv4 magicCookie; // In hex: 63, 82, 53, 63
};


/*
struct __attribute__((packed)) IPv6Header
{
	uint32_t etc;
	uint16_t payloadLen;
	uint8_t nextHeader;
	uint8_t hopLimit;
	struct IPaddress srcAddr;
	struct IPaddress destAddr;
	//uint8_t srcAddr[16];
	//uint8_t destAddr[16];
};
// Version is (etc >> 4) & 0x000000F
// Traffic class is (etc & 0x0000000F) << 4 | ((etc >> 12) & 0x0000F)
// Flow label is (etc >> 24) | ((etc >> 8) & 0x00FF00) | (etc & 0x00000F00) << 8
// Probably won't ever need to access these fields actually

struct __attribute__((packed)) ICMPv6Header
{
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
};
struct __attribute__((packed)) ICMPv6PseudoHeader
{
	struct IPaddress srcAddr;
	struct IPaddress destAddr;
	//uint8_t srcAddr[16];
	//uint8_t destAddr[16];
	uint32_t length;
	uint16_t zeros;
	uint8_t moreZeros;
	uint8_t nextHeader;
};
struct __attribute__((packed)) RouterAdvert
{
	uint8_t hopLimit;
	uint8_t flags;
	uint16_t lifetime;
	uint32_t reachableTime;
	uint32_t retransmitTime;
};
struct __attribute__((packed)) RouterSolicit
{
	uint32_t zeros;
};
struct __attribute__((packed)) NeighborSolicit
{
	uint32_t reserved;
	struct IPaddress targetAddr;
	//uint8_t targetAddr[16];
	
};
struct __attribute__((packed)) SrcAddrOpt // The size of these address option headers is 8 bytes
{
	uint8_t type; // Type = 1
	uint8_t length;
	uint8_t srcAddr[6];
};
struct __attribute__((packed)) TargetAddrOpt
{
	uint8_t type; // Type = 2
	uint8_t length;
	uint8_t targetAddr[6];
};
struct __attribute__((packed)) PrefixOpt
{
	uint8_t type; // Type = 3
	uint8_t length;
	uint8_t prefixLen; // In bits, not bytes
	uint8_t flags;
	uint32_t validLife;
	uint32_t prefLife;
	uint32_t reserved;
	uint8_t prefix[16];
};
struct __attribute__((packed)) MTUOpt
{
	uint8_t type; // Type = 5
	uint8_t length;
	uint16_t reserved;
	uint32_t MTU;
};
struct __attribute__((packed)) Echo // Both Echo Request (128) and Echo Reply (129) use the same format
{
	uint8_t identifierH; // These 16 bit fields are split into high and low bytes for efficiency in copying
	uint8_t identifierL;
	uint8_t sequenceNumH;
	uint8_t sequenceNumL;
};
*/
#ifdef __cplusplus
}
#endif
#endif // HEADER_STRUCTS_H