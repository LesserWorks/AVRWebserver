#ifndef SOCKET_H 
#define SOCKET_H
#ifdef __cplusplus
#define restrict __restrict__
extern "C" {
#endif

#define MAX_SOCKETS 2 // Maximum number of sockets we will allow open at once
#define MAX_STREAMS 5 // Maximum number of streams total on the device
// The following must be powers of two
#define STREAM_RX_SIZE 1024 // Length in bytes of each statically allocated RX stream buffer
#define STREAM_TX_SIZE 512 // Length in bytes of each statically allocated TX stream buffer

#define TIME_WAIT_SECONDS 10 // How many seconds TCP streams remain in TIME_WAIT before closing

#define RX_MASK (STREAM_RX_SIZE - 1)
#define TX_MASK (STREAM_TX_SIZE - 1)

enum __attribute__((packed)) TCPstate {UDP_MODE, CLOSED, LISTEN, SYN_SENT, SYN_RECEIVED, ESTABLISHED, \
		CLOSE_WAIT, LAST_ACK, FIN_WAIT_1, FIN_WAIT_2, CLOSING, TIME_WAIT};

struct RX
{
	uint32_t head; // Even though 32 bits is much larger than we need for an array index,
	uint32_t tail; // they should be this big since we do arithmetic with them and raw 32 bit sequence numbers 
	uint32_t rawseq; // In TCP mode, the raw sequence number received in the last ACK of the handshake
	uint8_t buf[STREAM_RX_SIZE];
};

struct TX
{
	uint32_t head;
	uint32_t tail; // In TCP mode, this points to first byte of sent but unacknowledged data (everything behind it is ACKed)
	uint32_t next; // In TCP mode, this points to the first byte of unsent data that has been written by user
	uint32_t window; // In TCP mode, the current send window
	uint32_t rawseq; // In TCP mode, the raw sequence number of the last sent packet
	uint16_t scale; // Window scaling
	uint8_t buf[STREAM_TX_SIZE];
};
// Initially head = tail = next. User calls send, moves head pointer. 
// Driver sends it, moves next pointer. Receives ACK, moves tail pointer

struct Stream
{
	uint8_t inUse : 1,
			accepted : 1;
	enum TCPstate state; // Holds TCP state or UDP
	int8_t parent; // Index of the socket using this stream
	int8_t timer; // Used for TIME_WAIT timer
	uint16_t remotePort;
	struct IPv4 remoteIP; // Address and port of who this stream is communicating with
	struct RX rx;
	struct TX tx; // RX and TX ring buffers
};

struct Socket 
{
	uint16_t port; // Local port of this socket
	uint8_t protocol; // TCP or UDP
	uint8_t inUse : 1,
			listening : 1; // If this is a listening socket or not
};

extern struct Socket sockets[MAX_SOCKETS]; // Where we store our socket descriptors
extern struct Stream streams[MAX_STREAMS]; // Our pool of streams that sockets can acquire

extern void TCPprocessor(struct Stream *const restrict stream, const struct IPv4header *const restrict ip, const struct TCPheader *const restrict tcp);
extern int16_t TCPrecv(const int8_t stream, void *const dest, const int16_t buflen, const uint8_t flags);
extern int16_t TCPsend(const int8_t stream, const void *const src, const int16_t buflen, const uint8_t flags);
extern void TCPclose(const int8_t stream);
extern void handleTCPtimers(void);

#ifdef __cplusplus
}
#endif
#endif // SOCKET_H
