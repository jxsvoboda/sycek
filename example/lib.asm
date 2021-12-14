;
; Library module written in assembly that will be linked with
; module compiled from test.c
;

GLOBAL _putpixel

; putpixel(x, y)
;   BC = x
;   DE = y
._putpixel
	; A := x/8
	ld A, C
	srl A
	srl A
	srl A

	; HL := 0x4000 + x/8
	ld H, 0x40
	ld L, A

	; DE := y * 32 == y << 5
	sla E
	rl D
	sla E
	rl D
	sla E
	rl D
	sla E
	rl D
	sla E
	rl D

	; live registers: BC, DE, HL
	; swap bits 5-7 and 8-10 of DE
	push BC

	ld A, E
	rlc A
	rlc A
	rlc A
	and 0x07
	ld B, A

	ld A, D
	and 0xf8
	or B
	ld B, A

	ld A, D
	rrc A
	rrc A
	rrc A
	and 0xe0
	ld C, A

	ld A, E
	and 0x1f
	or C
	ld C, A

	ld D, B
	ld E, C
	pop BC

	; BC = x
	; DE = swapped(y * 32)

	; final address of pixel = 0x4000 + x/8 + swapped(y * 32)
	add HL, DE

	;
	; Now computing bit pattern 0x80 >> (x & 0x7)
	;

	ld A, C
	and 0x7
	ld C, A ; C := x & 0x7

	ld A, 0x80

	; if x & 0x7 == 0, no shifting
	jr Z, l_noshift

	; take number 0x80 and shift it right (x & 0x7) times
	ld B, C
l_shiftloop:
	SRL A
	djnz l_shiftloop
l_noshift:
	; write prepared bit pattern
	ld (HL), A
	ret
