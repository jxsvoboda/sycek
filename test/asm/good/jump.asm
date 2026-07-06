/*
 * Z80 Jump Group
 */

proc @_z80_jump_group
begin
	jp %label;
	jp NZ, %label; /* jp cc, nn */
	jr %label;
	jr C, %label;
	jr NC, %label;
	jr Z, %label;
	jr NZ, %label;
	jp (HL);
	jp (IX);
	jp (IY);
	djnz %label;
%label:
end;
