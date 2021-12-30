;
; Library module written in assembly that will be linked with
; module compiled from test.c
;

GLOBAL _putpixel

; putpixel(x, y)
;   BC = x
;   DE = y
._putpixel
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

	; A := x/8
	ld A, C
	srl A
	srl A
	srl A

	; HL := 0x4000 + x/8
	ld H, 0x40
	ld L, A

	; HL := 0x4000 + x / 8 + y * 32
	add HL, DE

	; live registers: BC, HL
	; swap bits 5-7 and 8-10 of HL using DE for temporary storage

	; D(0:2) := L(7:5)
	ld A, L
	rlc A
	rlc A
	rlc A
	and 0x07
	ld D, A

	; D := H(7:3) L(7:5)
	ld A, H
	and 0xf8
	or D
	ld D, A

	; E(7:5) := H(2:0)
	ld A, H
	rrc A
	rrc A
	rrc A
	and 0xe0
	ld E, A

	; E := H(2:0) L(4:0)
	ld A, L
	and 0x1f
	or E
	ld E, A

	ld H, D
	ld L, E

	; BC = x
	; HL = pixel address

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
