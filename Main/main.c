#ifndef F_CPU
#error "F_CPU"
#endif
#include <stdint.h>
#include <string.h>
#include <avr/io.h>
#include <stdio.h>
#//include <avr/pgmspace.h>
//#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "uartlibrary/uart.h"
#include "HeaderStructs/HeaderStructs.h"
#include "RTC/RTC.h"
#include "WebserverDriver/WebserverDriver.h"
#include "DHCP/DHCP.h"
#define STR(x) PSTR(#x)
#define UART_BAUD_RATE      9600 

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if GCC_VERSION < 100200
#warning "GCC version is less than 10.2.0"
#endif


static const char html[] = {
#include "Homepage.html"
};

static const char httpHeader[] = {
"HTTP/1.1 200 OK\r\n"
"Content-Type: txt/html\r\n"
"Content-Encoding: identity\r\n"
};
// Finish with Content-Length: sizeof(html)\r\n\r\n

static int write_char_helper(char var, FILE *stream);
static int read_char_helper(FILE *stream);
static FILE mystream = FDEV_SETUP_STREAM(write_char_helper, read_char_helper, _FDEV_SETUP_RW);
/*
    fgets(input, sizeof(input), stdin);
    sscanf(input, "%d", &x);
*/
uint8_t mcusr_mirror __attribute__ ((section (".noinit")));

void get_mcusr(void) \
  __attribute__((naked)) \
  __attribute__((section(".init3")));
void get_mcusr(void)
{
  mcusr_mirror = MCUSR;
  MCUSR = 0;
  wdt_disable();
}
struct MAC unicastMAC = {{0x0, 0x1E, 0xC0, 0x8C, 0x3E, 0x00}};
struct MAC broadcastMAC = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
struct MAC zeroMAC = {{0}};
struct IPv4 localIP = {{192, 168, 0, 0}};
struct IPv4 routerIP = {{192, 168, 1, 1}};


void main(void)
{
    stdout = &mystream;
  	stdin = &mystream;
  	DDRA = 0;
  	DDRB = 0;
  	DDRC = 0;
  	DDRD = (1 << PORTD1) | (1 << PORTD3) | (1 << PORTD4) | (1 << PORTD5) | (1 << PORTD7);
  	PORTA = 0;
  	PORTB = 0;
  	PORTC = 0;
  	PORTD = (1 << PORTD5) | (1 << PORTD7);
  	wdt_reset();
  	uart_init(UART_BAUD_SELECT(UART_BAUD_RATE, F_CPU)); 
    RTC.init();
    RTC.setTimeZone(TIMEZONE / 100);
    RTC.setTime(SECOND, MINUTE, HOUR, DAY, MONTH, YEAR); // Sets with local time of compilation

    NICsetup();

    DHCPsetup();
    while(!DHCPready())
      packetHandler();
    puts("Main loop bound");

    int8_t socketTCP = socket(PROTO_TCP);
    if(socketTCP < 0)
      puts("Socket TCP failed");
    
    if(bindlisten(socketTCP, 80) < 0)
      puts("Bind TCP failed");

    int8_t timer = RTC.setTimer(2);
    while(1) {
      if(RTC.timerDone(timer) == 1) {
        puts("Timer done");
        break;
      }
    }

    packetHandler();
    int8_t streamTCP = -1;
    

    uint8_t buf[1000];
  	while(1)
  	{
        packetHandler();

        if(streamTCP < 0) { // Only call accept if we don't already have an open stream
            streamTCP = accept(socketTCP, 0);
            if(streamTCP >= 0) 
                puts("Main.c client connected");
        }
        else {
            int16_t retvalTCP = recv(streamTCP, buf, sizeof(buf), MSG_DONTWAIT); // We'll assume this will read the whole header
            if(retvalTCP > 0 && memcmp(buf, "GET", 3) == 0) {
                puts("Got GET request.");
                char resp[sizeof(httpHeader) + 2] = {0};
                strcpy(resp, httpHeader);
                char contentLen[40];
                sprintf(contentLen, "Content-Length: %d\r\n\r\n", sizeof(html));

                uint16_t totalSize = strlen(resp) + strlen(contentLen) + sizeof(html);
                if(totalSize % 2 != 0)
                  totalSize += 1;
                char sendBuf[totalSize + 2];
                memset(sendBuf, 0, sizeof(sendBuf));
                char *ptr = sendBuf;
                memcpy(ptr, resp, strlen(resp));
                ptr += strlen(resp);
                memcpy(ptr, contentLen, strlen(contentLen));
                ptr += strlen(contentLen);
                memcpy(ptr, html, sizeof(html));

                if(send(streamTCP, sendBuf, totalSize, 0) < 0)
                    puts("Send header failed");
                puts("Just sent HTTP data");
                closeStream(streamTCP); 
                streamTCP = -1;
            }
            else if(retvalTCP == 0) { // Client sent TCP fin
                closeStream(streamTCP); // Close it ourselves
                streamTCP = -1; // Allow calling accept again
            }
        }
	  } 
}

// This function is called by printf as a stream handler
static int write_char_helper(char var, FILE *stream) {
	uart_putc(var);
	return 0;
}

// This function blocks until a character is read, thus it never returns EOF
static int read_char_helper(FILE *stream) {
  uint16_t c;
  do 
  {
    c = uart_getc();
  }
  while(c & UART_NO_DATA);
  
  if(c & (UART_BUFFER_OVERFLOW | UART_OVERRUN_ERROR | UART_FRAME_ERROR))
    return _FDEV_ERR;
  else
    return c;
}
