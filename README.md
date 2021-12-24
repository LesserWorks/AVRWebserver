# AVRWebserver
This code is for a webserver running on a custom board with an ATmega1284, but can be easily adapted to other platforms.

All "user" code should go in Main/main.c

The following functions and modules must be reimplemented to port this to another platform. More details to come.

checksumUnrolled() - written in AVR assembly, but can easily be rewritten in C
getFrameSize() - specific to ENC28J60
readFrame() - specific to ENC28J60
sendEthernetFrame() - specific to ENC28J60
NICsetup() - specific to ENC28J60
RTC module - uses a counter on the AVR