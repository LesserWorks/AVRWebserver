.section .text
; uint16_t checksumBig(uint8_t *data, uint8_t *end);
.global checksumBig
; This function calculates a 16 bit one's complement checksum
; compatible with IPv4, TCP, etc.
; The data to checksum over must be in big endian as specified by IP.
; r24:25 must hold the address of the first byte to checksum over
; r22:23 must hold one past the address of the last byte to checksum over
; End pointer can be calulated as data + sizeof(data) if data is a compile time array
checksumBig:
movw Z, r24		; pointer in Z
ld r25, Z+
ld r24, Z+ 			; r24:25 used as checksum accumulator
loopBig:
cp ZL, r22 
cpc ZH, r23 		; compare current pointer with end pointer
brsh endSequence 	; Are we at or past end pointer?
ld r27, Z+			; high byte first
ld r26, Z+ 			; low byte (big endian)
add r24, r26 		; add low bytes
adc r25, r27 		; add high bytes
adc r24, r1 		; add carry back around
adc r25, r1
rjmp loopBig
endSequence: 		; reached end of data to checksum over
com r24
com r25 			; flip bits
ret
; Even for maximum length packet of 1500 bytes, checksum will not take more than a millisecond
; Best case scenario of 0 words will take 5 + 4 + 6 = 15 cycles
; For n words with n > 0, will take 5 + 13(n - 1) + 10 = 15 + 13(n - 1) = 13n + 2 cycles

; uint16_t checksumUnrolled(uint8_t *data, uint8_t *end);
.global checksumUnrolled
; This function also computes the same checksum, but unrolls the loop
; to increase performace (see Wikipedia loop unrolling). 
; The first part of the code before "ijmp" just calculates
; the length of the data mod 4 to jump into the middle of the loop.
checksumUnrolled:
movw X, r24 		; pointer in X
ld r25, X+
ld r24, X+			; r24:25 will be accumulator, load first word
movw r20, r22 		; r20:21 also holds end pointer for later use
sub r22, XL			; subtract end pointer from current pointer to find length
sbc r23, XH 		; r22:23 now holds remaining length in units of 8 bit bytes
lsr r23
ror r22				; divide length by 2 to get it in units of 16 bit words
movw r30, r22 		; r30:31 now also holds length in units of 16 bit words
lsr r31
ror r30
lsr r31
ror r30 			; divide length by 4, discarding decimal part (floor function)
lsl r30
rol r31
lsl r30
rol r31				; multiply length by 4, now holds (length - (length % 4))
sub r22, r30		; calculate (length - (length - (length % 4)))
sbc r23, r31		; r22:23 now holds (length % 4)
ldi ZL, pm_lo8(jumpTable)	; critically important to use pm_lo8 instead of lo8 
ldi ZH, pm_hi8(jumpTable)	; so that we get a word address instead of byte address
add ZL, r22			; add (length % 4) to base address of jump table
adc ZH, r1
ijmp				; ijmp jumps to whatever address is in the Z pointer

jumpTable:
rjmp mod0 			; from here we jump into the middle of the loop
rjmp mod1
rjmp mod2
rjmp mod3

loopCondition:
cp XL, r20 
cpc XH, r21 		; compare current pointer with end pointer
brsh retSequence 	; Are we at or past end pointer?

ld r23, X+			; load word (big endian) into r22:23
ld r22, X+
add r24, r22		; add to accumulator in r24:25
adc r25, r23
adc r24, r1
adc r25, r1 		; add carries back around
mod3:
ld r23, X+
ld r22, X+
add r24, r22
adc r25, r23
adc r24, r1
adc r25, r1 		; add carries back around
mod2:
ld r23, X+
ld r22, X+
add r24, r22
adc r25, r23
adc r24, r1
adc r25, r1 		; add carries back around
mod1:
ld r23, X+
ld r22, X+
add r24, r22
adc r25, r23
adc r24, r1
adc r25, r1 		; add carries back around
mod0:
rjmp loopCondition
retSequence:
com r24
com r25 			; com is one's complement, it just flips the bits, like ~ in C
ret
; The unrolled version is tested and works
; Execution time is:
; 29 cycles up to and including rjmp in jump table
; If mod0, then 2 to get to first condition check (loopCondition)
; If mod1, then 8+2 to first condition check
; If mod2, then 8+8+2 to first condition check
; If mod3, then 8+8+8+2 to first condition check
; Thus first time through loop will take ((n-1) mod 4)*8 + 2 cycles
; Subsequent loops take 3+8+8+8+8+2 = 37 cycles
; The last time, when condition is false, will be 4+6=10 cycles
; For n bytes where n > 0, will be 29 + ((n-1) mod 4)*8 + 2 + (n - 1 - ((n-1) mod 4))/4*37 + 10
; = 41 + ((n-1) mod 4)*8 + (n - 1 - ((n-1) mod 4))/4*37
; = 41 + (n - 1 - floor((n-1)/4)*4)*8 + floor((n-1)/4)*4/4*37
; = 41 + (n - 1 - floor((n-1)/4)*4)*8 + floor((n-1)/4)*37
.end
; checksumBig is faster for up to 7 bytes, for 8 or more bytes, checksumUnrolled is faster
; For packet of 1500 bytes, checksumBig take 19502 cycles and checksumUnrolled takes 13903 cycles,
; a difference of 5599 cycles = 280 microseconds.
; A max length packet takes 0.9751 ms with checksumBig and 0.6952 ms with checksumUnrolled