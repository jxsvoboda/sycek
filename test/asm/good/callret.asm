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
	rst 0x00;
	rst 0x08;
	rst 0x10;
	rst 0x18;
	rst 0x20;
	rst 0x28;
	rst 0x30;
	rst 0x38;
end;
