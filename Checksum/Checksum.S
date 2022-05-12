.section .text
; uint16_t checksumUpdate(uint16_t context, const void *data, uint16_t len)
.global checksumUpdate
; context in r25:24, data in r23:22, len in r21:20, assumes len is a multiple of 2
checksumUpdate:
movw X, r22 ; copy data pointer to X
mov r22, r20 ; make copy of length lower byte
add r20, XL
adc r21, XH ; add length to start pointer to get end pointer in r21:20
lsr r22 ; divide lower byte of len by 2 to get number of 16 bit words
andi r22, 0b00000011 ; isolate last two LSBs, giving length % 4
ldi ZL, pm_lo8(tableJump) 
ldi ZH, pm_hi8(tableJump) ; load the flash address of the jump table
add ZL, r22
adc ZH, r1 ; add length % 4 to the address of the jump table
ijmp
tableJump:
rjmp zeroLeftover
rjmp oneLeftover
rjmp twoLeftover
rjmp threeLeftover
addLoop:
ld r23, X+
ld r22, X+ ; load next word big endian
add r24, r22
adc r25, r23 ; add to accumulator
adc r24, r1 ; add carry back around
threeLeftover:
ld r23, X+
ld r22, X+
add r24, r22
adc r25, r23
adc r24, r1
twoLeftover:
ld r23, X+
ld r22, X+
add r24, r22
adc r25, r23
adc r24, r1
oneLeftover:
ld r23, X+
ld r22, X+
add r24, r22
adc r25, r23
adc r24, r1
zeroLeftover:
cp XL, r20 
cpc XH, r21 		; compare current pointer with end pointer
brlo addLoop
ret ; do not complement output since we are not outputting final checksum, just a running context

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
; The following is way over complicated. To calculate mod 4, just AND with 0b00000011
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