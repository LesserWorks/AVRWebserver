DEVICE = atmega328p
F_CPU = 8000000UL
CC = avr-gcc
CFLAGS = -O2 -std=gnu2x -Wall -Wno-main -mmcu=$(DEVICE) -DF_CPU=$(F_CPU) -I ~/Desktop/AVRprojects

.PHONY: all
all: RTCtest.hex
	
RTCtest.hex: RTCtest.elf
	avr-objcopy -j .text -j .data -O ihex $< $@
RTCtest.elf: RTCtest.o uart.o RTC.o rtc3231.o i2c_master.o
	$(CC) -mmcu=$(DEVICE) $^ -o $@
	
RTCtest.o: RTCtest.c  ../uartlibrary/uart.h RTC.h
	$(CC) $(CFLAGS) -c $<

uart.o: ../uartlibrary/uart.c ../uartlibrary/uart.h
	$(CC) $(CFLAGS) -c $<

RTC.o: RTC.c RTC.h rtc3231.h
	$(CC) $(CFLAGS) -c $<

rtc3231.o: rtc3231.c rtc3231.h i2c_master.h
	$(CC) $(CFLAGS) -c $<
	
i2c_master.o: i2c_master.c i2c_master.h 
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f *.elf *.hex *.o

.PHONY: flash
flash: all
	avrdude -c avrisp2 -p $(DEVICE) -P /dev/tty.usbmodem001992931 -U flash:w:RTCtest.hex