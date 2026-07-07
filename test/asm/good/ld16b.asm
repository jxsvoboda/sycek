/*
 * Z80 16-bit Load Group
 */

proc @_z80_16bit_load_group
begin
	ld SP, 0x1234; /* ld dd, nn */
	ld IX, 0x1234;
	ld IY, 0x1234;
	ld HL, (0x1234);
	ld SP, (0x1234); /* ld dd, (nn) */
	ld IX, (0x1234);
	ld IY, (0x1234);

	ld (0x1234), HL;
	ld (0x1234), SP; /* ld (nn), dd */
	ld (0x1234), IX;
	ld (0x1234), IY;
	ld SP, HL;
	ld SP, IX;
	ld SP, IY;
	push AF; /* push qq */
	push IX;
	push IY;
	pop AF; /* pop qq */
	pop IX;
	pop IY;
end;
