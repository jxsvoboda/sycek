/*
 *Z80 8-bit Arithmetic Group
 */

proc @_z80_8bit_arithmetic_group
begin
	add A, B;
	add A, 42;
	add A, (HL);
	add A, (IX+1);
	add A, (IY-1);
	adc A, B;
	adc A, 42;
	adc A, (HL);
	adc A, (IX+1);
	adc A, (IY-1);
	sub B;
	sub 42;
	sub (HL);
	sub (IX+1);
	sub (IY-1);
	sbc A, B;
	sbc A, 42;
	sbc A, (HL);
	sbc A, (IX+1);
	sbc A, (IX-1);
	and B;
	and 42;
	and (HL);
	and (IX+1);
	and (IX-1);
	or B;
	or 42;
	or (HL);
	or (IX+1);
	or (IY-1);
	xor B;
	xor 42;
	xor (HL);
	xor (IX+1);
	xor (IY-1);
	cp B;
	cp 42;
	cp (HL);
	cp (IX+1);
	cp (IY-1);
	inc B;
	inc (HL);
	inc (IX+1);
	inc (IY-1);
	dec B;
	dec (HL);
	dec (IX+1);
	dec (IY-1);
end;
