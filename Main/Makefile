DEVICE = atmega1284
F_CPU = 8000000UL
YEAR = $(shell date +"%Y")
MONTH = $(shell date +"%m" | sed 's/-0/-/;s/^0//')
DAY = $(shell date +"%d" | sed 's/-0/-/;s/^0//')
HOUR = $(shell date +"%H" | sed 's/-0/-/;s/^0//')
MINUTE = $(shell date +"%M" | sed 's/-0/-/;s/^0//')
SECOND = $(shell date +"%S" | sed 's/-0/-/;s/^0//')
TIMEZONE = $(shell date +"%z" | sed 's/-0/-/;s/^0//')
# date +"%S" | sed 's/-0/-/;s/^0//'
CC = avr-gcc
CFLAGS = -O2 -std=gnu2x -Wall -Wextra -Wno-scalar-storage-order -mmcu=$(DEVICE) -DF_CPU=$(F_CPU) -fdata-sections -ffunction-sections -I ../ \
-DYEAR=$(YEAR) -DMONTH=$(MONTH) -DDAY=$(DAY) -DHOUR=$(HOUR) -DMINUTE=$(MINUTE) -DSECOND=$(SECOND) -DTIMEZONE=$(TIMEZONE)

.PHONY: all
all: Main.hex
	
Main.hex: Main.elf
	avr-objcopy -j .text -j .data -O ihex $< $@
	avr-size $<

Main.elf: Main.o ENC28J60_functions.o uart.o ARP.o WebserverDriver.o Checksum.o DHCP.o Socket.o RTC.o
	$(CC) -mmcu=$(DEVICE) -Wl,--gc-sections $^ -o $@
	
Main.o: main.c  Homepage.html ../uartlibrary/uart.h \
	../ENC28J60_macros/ENC28J60_macros.h ../ENC28J60_functions/ENC28J60_functions.h \
	../HeaderStructs/HeaderStructs.h ../ARP/ARP.h ../DHCP/DHCP.h
	$(CC) $(CFLAGS) -c $<

WebserverDriver.o: ../WebserverDriver/WebserverDriver.c ../WebserverDriver/WebserverDriver.h \
	../HeaderStructs/HeaderStructs.h ../ENC28J60_macros/ENC28J60_macros.h \
	../ENC28J60_functions/ENC28J60_functions.h ../Checksum/Checksum.h ../Socket/Socket.h
	$(CC) $(CFLAGS) -c $< 

ENC28J60_functions.o: ../ENC28J60_functions/ENC28J60_functions.c ../HeaderStructs/HeaderStructs.h \
	../ENC28J60_macros/ENC28J60_macros.h ../ENC28J60_functions/ENC28J60_functions.h
	$(CC) $(CFLAGS) -c $<

Checksum.o: ../Checksum/Checksum.S ../Checksum/Checksum.h
	$(CC) $(CFLAGS) -c $<

DHCP.o: ../DHCP/DHCP.c ../DHCP/DHCP.h ../WebserverDriver/WebserverDriver.h \
	../HeaderStructs/HeaderStructs.h ../ENC28J60_functions/ENC28J60_functions.h ../ARP/ARP.h
	$(CC) $(CFLAGS) -c $<

Socket.o: ../Socket/Socket.c ../Socket/Socket.h ../HeaderStructs/HeaderStructs.h ../Checksum/Checksum.h \
	../WebserverDriver/WebserverDriver.h ../ENC28J60_functions/ENC28J60_functions.h
	$(CC) $(CFLAGS) -c $<

RTC.o: ../RTC/RTC.c ../RTC/RTC.h
	$(CC) $(CFLAGS) -c $<
	
uart.o: ../uartlibrary/uart.c ../uartlibrary/uart.h
	$(CC) $(CFLAGS) -c $<

ARP.o: ../ARP/ARP.c ../ARP/ARP.h ../HeaderStructs/HeaderStructs.h ../ENC28J60_macros/ENC28J60_macros.h \
	../ENC28J60_functions/ENC28J60_functions.h ../WebserverDriver/WebserverDriver.h
	$(CC) $(CFLAGS) -c $<
	
.PHONY: clean
clean:
	rm -f *.elf *.hex *.o

.PHONY: flash
flash: all
	avrdude -c avrisp2 -p $(DEVICE) -P /dev/tty.usbmodem001992931 -U flash:w:Main.hex

.PHONY: asm
asm: Main.elf
	avr-objdump $< -d -S
