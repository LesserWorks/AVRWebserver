; http://sourceware.org/binutils/docs/as/AVR_002dModifiers.html#AVR_002dModifiers
; http://mil.ufl.edu/3744/docs/XMEGA/C/Mixed%20C%20and%20Assembly%20Atmel%20XMega.pdf
.section .data

RAMjumpTable: .ds.w 2

.section .text

FlashjumpTable:

.global InitTable
InitTable:
ldi ZL, lo8(RAMjumpTable)
ldi ZH, hi8(RAMjumpTable)
ldi r24, pm_lo8(Fun1)
ldi r25, pm_hi8(Fun1)
ldi r22, pm_lo8(Fun2)
ldi r23, pm_hi8(Fun2)
st Z+, r24
st Z+, r25
st Z+, r22
st Z+, r23
ret

; void jumpTo(uint8_t offset)
.global jumpTo
jumpTo:
ldi XL, lo8(RAMjumpTable)
ldi XH, hi8(RAMjumpTable)
lsl r24			; multiply by 2 since each jumpTable entry is 2 bytes
add XL, r24
adc XH, r1
ld ZL, X+
ld ZH, X		; Z now hold function address
icall
ret				; or could do ijmp instead of icall ret


; Below are sample functions referenced in jump table
Fun1:
ret

Fun2:
ret
.end