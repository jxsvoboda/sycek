/*
 *Z80 16-bit Arithmetic Group
 */

proc @_z80_16bit_arithmetic_group
begin
	add HL, SP; /* add HL, ss */
	adc HL, SP; /* adc HL, ss */
	sbc HL, SP; /* sbc HL, ss */
	add IX, IX; /* add IX, pp */
	add IY, IY; /* add IY, rr */
	inc SP; /* inc ss */
	inc IX;
	inc IY;
	dec SP; /* dec ss */
	dec IX;
	dec IY;
end;
