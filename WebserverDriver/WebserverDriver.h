#ifndef WEBSERVER_DRIVER_H
#define WEBSERVER_DRIVER_H
#ifdef __cplusplus
extern "C" {
#endif

#define MSG_DONTWAIT 1
#define MSG_WAITALL 2

extern uint8_t NICsetup(void);
extern void packetHandler(void);
/* Use these for type in socketbind
#define PROTO_TCP 6
#define PROTO_UDP 17
*/

// Returns socket descriptor on success, negative on failure
extern int8_t socket(const uint8_t protocol);

// Returns 0 on success, negative on failure
extern int8_t bindlisten(const int8_t socket, const uint16_t port);

// Returns a stream descriptor on success, negative on failure
extern int8_t connect(const int8_t socket, const uint16_t port, const struct IPv4 *const destIP);

// Returns a stream descriptor on success, negative on failure
extern int8_t accept(const int8_t socket, const uint8_t flags);

extern int16_t recv(const int8_t stream, void *const dest, const int16_t buflen, const uint8_t flags);
extern int16_t send(const int8_t stream, const void *const src, const uint16_t buflen, const uint8_t flags);

extern void printPacket(const uint8_t *const p, const uint16_t len);


extern void closeSocket(const int8_t socket);
extern void closeStream(const int8_t stream);


extern void sendIPv4packet(const struct IPv4 *const dest, const struct IPv4 *const src, 
					const uint8_t protocol, const uint16_t payloadLen, const uint8_t payloadNum, const struct Layer payload[]);

extern const struct IPv4 broadcastIP;

#ifdef __cplusplus
}
#endif
#endif // WEBSERVER_DRIVER_H