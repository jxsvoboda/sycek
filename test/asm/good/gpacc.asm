/*
 * Z80 General-Purpose Arithmetic and CPU Control Groups
 */

proc @_z80_gparith_cpu_ctl_group
begin
	daa;
	cpl;
	neg;
	ccf;
	scf;
	nop;
	halt;
	di;
	ei;
	im 0;
	im 1;
	im 2;
end;
