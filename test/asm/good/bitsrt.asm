/*
 * Z80 Bit Set, Reset and Test Group
 */

proc @_z80_bit_set_reset_test_group
begin
	bit 0, B;
	bit 0, (HL);
	bit 0, (IX+1);
	bit 0, (IY-1);
	set 0, B;
	set 0, (HL);
	set 0, (IX+1);
	set 0, (IY-1);
	res 0, B;
	res 0, (HL);
	res 0, (IX+1);
	res 0, (IY-1);
end;
