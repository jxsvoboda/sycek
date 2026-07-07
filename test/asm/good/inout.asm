/*
 * Z80 Input and Output Group
 */

proc @_z80_input_output_group
begin
	in A, (42);
	in B, (C);
	ini;
	inir;
	ind;
	indr;
	out (42), A;
	out (C), B;
	outi;
	otir;
	outd;
	otdr;
end;
