/*
 * Z80 8-bit Load Group
 */

proc @_z80_8bit_load_group
begin
	ld B, C;
	ld B, 42;
	ld B, (HL);
	ld B, (IX+1);
	ld B, (IY-1);
	ld (HL), C;
	ld (IX+1), C;
	ld (IY-1), C;
	ld (HL), 42;
	ld (IX+1), 42;
	ld (IY-1), 42;
	ld A, (BC);
	ld A, (DE);
	ld A, (4660); /* 0x1234 */
	ld (BC), A;
	ld (DE), A;
	ld (4660), A; /* 0x1234 */
	ld A, I;
	ld A, R;
	ld I, A;
	ld R, A;
end;
