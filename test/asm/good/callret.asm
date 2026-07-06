/*
 * Z80 Call and Return Group
 */

proc @_z80_call_return_group
begin
	call @_z80_call_return_group;
	call NZ, @_z80_call_return_group; /* call cc, nn */
	ret;
	ret NZ; /* ret cc */
	reti;
	retn;
	rst 0;
	rst 8;
	rst 16;
	rst 24;
	rst 32;
	rst 40;
	rst 48;
	rst 56;
end;
