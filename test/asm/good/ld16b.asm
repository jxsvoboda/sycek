/*
 * Z80 16-bit Load Group
 */

proc @_z80_16bit_load_group
begin
	ld SP, 4660; /* ld dd, nn */
	ld IX, 4660;
	ld IY, 4660;
	ld HL, (4660);
	ld SP, (4660); /* ld dd, (nn) */
	ld IX, (4660);
	ld IY, (4660);

	ld (4660), HL;
	ld (4660), SP; /* ld (nn), dd */
	ld (4660), IX;
	ld (4660), IY;
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
