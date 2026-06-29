/*
 * Z80 Exchange, Block Transfer and Search Group
 */

proc @_z80_ex_bt_search_group
begin
	ex DE, HL;
	ex AF, AF';
	exx;
	ex (SP), HL;
	ex (SP), IX;
	ex (SP), IY;
	ldi;
	ldir;
	ldd;
	lddr;
	cpi;
	cpir;
	cpd;
	cpdr;
end;
