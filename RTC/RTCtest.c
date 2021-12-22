#ifndef F_CPU
#error "F_CPU"
#endif

#include <stdint.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "RTC.h"
#include "uartlibrary/uart.h"
#define UART_BAUD_RATE      9600 

static int write_char_helper(char var, FILE *stream);
static int read_char_helper(FILE *stream);
static FILE mystream = FDEV_SETUP_STREAM(write_char_helper, read_char_helper, _FDEV_SETUP_RW);


void main(void)
{
	struct Time time;
	stdout = &mystream;
  	stdin = &mystream;
  	uart_init(UART_BAUD_SELECT(UART_BAUD_RATE, F_CPU)); 
  	RTC.init();
  	printf_P(PSTR("Enter date in this format: DD/MM/YYYY HH:MM:SS\n"));
  	char input[25];
  	uint8_t day, mon, hour, min, sec;
  	uint16_t year;
  	fgets(input, sizeof(input), stdin);
	sscanf(input, "%hhu/%hhu/%u %hhu:%hhu:%hhu", &day, &mon, &year, &hour, &min, &sec);
	printf_P(PSTR("Input date: %u/%u/%u %u:%u:%u\n"), day, mon, year, hour, min, sec);
	RTC.setTime(sec, min, hour, day, mon, year);

	while(1)
	{
		RTC.read(&time);
		printf("\r%02u/%02u/%04u %02u:%02u:%02u Unix: %lu", time.day, time.mon, time.year, time.hour, time.min, time.sec, time.unix);
		_delay_ms(1000);

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