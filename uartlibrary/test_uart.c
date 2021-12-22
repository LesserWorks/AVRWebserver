/*************************************************************************
Title:    Example program for the Interrupt controlled UART library
Author:   Peter Fleury <pfleury@gmx.ch>   http://tinyurl.com/peterfleury
File:     $Id: test_uart.c,v 1.7 2015/01/31 17:46:31 peter Exp $
Software: AVR-GCC 4.x
Hardware: AVR with built-in UART/USART

DESCRIPTION:
          This example shows how to use the UART library uart.c

*************************************************************************/
#include <stdint.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "AsmTest/AsmTest.h"
#include "uart.h"


/* define CPU frequency in Hz in Makefile */
#ifndef F_CPU
#error "F_CPU undefined, please define CPU frequency in Hz in Makefile"
#endif

/* Define UART buad rate here */
#define UART_BAUD_RATE      9600      
static void printHex(uint16_t val)
{
  uart_puts("0x");
  uint8_t hh = val >> 12;
  uint8_t hl = val >> 8 & 0x000F;
  uint8_t lh = val >> 4 & 0x000F; // Separate nibbles
  uint8_t ll = val & 0x000F;
  hh = hh > 9 ? hh + 7 + '0' : hh + '0';
  hl = hl > 9 ? hl + 7 + '0' : hl + '0';
  lh = lh > 9 ? lh + 7 + '0' : lh + '0';
  ll = ll > 9 ? ll + 7 + '0' : ll + '0';
  //USARTsendString("0x");
  uart_putc(hh);
  uart_putc(hl);
  uart_putc(lh);
  uart_putc(ll);
  uart_puts("\r\n");
  return;
}

int main(void)
{
    unsigned int c;
    char buffer[7];
    int  num=134;
	//uint8_t data[] = {0b01011110, 0b10000110, 0b01100000, 0b10101100,
	//				  0b00101010, 0b01110001, 0b10110101, 0b10000001};
	uint8_t dataLittle[] = {0xFB, 0x23, 0xC0, 0x34, 0x90, 0xA0, 0xAF, 0xBC, 0x05, 0xFC};
	uint8_t dataBig[] = {0x23, 0xFB, 0x34, 0xC0, 0xA0, 0x90, 0xBC, 0xAF, 0xFC, 0x05};
	DDRB = 0xFF;
	DDRD = 0b00000010;
	PORTD = 0;

    
    /*
     *  Initialize UART library, pass baudrate and AVR cpu clock
     *  with the macro 
     *  UART_BAUD_SELECT() (normal speed mode )
     *  or 
     *  UART_BAUD_SELECT_DOUBLE_SPEED() ( double speed mode)
     */
    uart_init( UART_BAUD_SELECT(UART_BAUD_RATE,F_CPU) ); 
    
    /*
     * now enable interrupt, since UART library is interrupt controlled
     */
    sei();
    
    /*
     *  Transmit string to UART
     *  The string is buffered by the uart library in a circular buffer
     *  and one character at a time is transmitted to the UART using interrupts.
     *  uart_puts() blocks if it can not write the whole string to the circular 
     *  buffer
     */
    uart_puts("String stored in SRAM\n");
    
    /*
     * Transmit string from program memory to UART
     */
    uart_puts_P("String stored in FLASH\n");
    
        
    /* 
     * Use standard avr-libc functions to convert numbers into string
     * before transmitting via UART
     */     
    itoa( num, buffer, 10);   // convert interger into string (decimal format)         
    uart_puts(buffer);        // and transmit string to UART

    
    /*
     * Transmit single character to UART
     */
    uart_putc('\n');
    uart_puts_P("About to call add function...\r\n");
	const uint16_t sum = addAsm(45, 46);
	if(sum == 45 + 46)
	{
		uart_puts_P("Sum correct.\r\n");
	}
	else
	{
		uart_puts_P("Sum incorrect.\r\n");
	}
	uart_puts_P("Calling checksum...\r\n");
	const uint16_t checkLittle = checksumLittle(dataLittle, sizeof(dataLittle));
	const uint16_t checkBig = checksumBig(dataBig, dataBig + sizeof(dataBig));
    itoa(0x4DFE, buffer, 10);   // convert interger into string (decimal format)  
	uart_puts_P("Checksum should be: \r\n");       
    //uart_puts(buffer); 
	printHex(0x4DFE);
	uart_puts_P("\r\nCalculated checksumLittle is: ");
    itoa(checkLittle, buffer, 10);   // convert interger into string (decimal format)        
    //uart_puts(buffer); 
	printHex(checkLittle);
	uart_puts_P("\r\nCalculated checksumBig is: ");
	printHex(checkBig);
	uart_putc('\n');
    for(;;)
    {
        /*
         * Get received character from ringbuffer
         * uart_getc() returns in the lower byte the received character and 
         * in the higher byte (bitmask) the last receive error
         * UART_NO_DATA is returned when no data is available.
         *
         */
        c = uart_getc();
        if ( c & UART_NO_DATA )
        {
            /* 
             * no data available from UART 
             */
        }
        else
        {
            /*
             * new data available from UART
             * check for Frame or Overrun error
             */
            if ( c & UART_FRAME_ERROR )
            {
                /* Framing Error detected, i.e no stop bit detected */
                uart_puts_P("UART Frame Error: ");
            }
            if ( c & UART_OVERRUN_ERROR )
            {
                /* 
                 * Overrun, a character already present in the UART UDR register was 
                 * not read by the interrupt handler before the next character arrived,
                 * one or more received characters have been dropped
                 */
                uart_puts_P("UART Overrun Error: ");
            }
            if ( c & UART_BUFFER_OVERFLOW )
            {
                /* 
                 * We are not reading the receive buffer fast enough,
                 * one or more received character have been dropped 
                 */
                uart_puts_P("Buffer overflow error: ");
            }
            /* 
             * send received character back
             */
            uart_putc( (unsigned char)c );
        }
    }
    
}
