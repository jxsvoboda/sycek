/*
 * Z80 Rotate and Shift Group
 */

proc @_z80_rot_shift_group
begin
	rlca;
	rla;
	rrca;
	rra;
	rlc B;
	rlc (HL);
	rlc (IX+1);
	rlc (IY-1);
	rl B;
	rl (HL);
	rl (IX+1);
	rl (IY-1);
	rrc B;
	rrc (HL);
	rrc (IX+1);
	rrc (IY-1);
	rr B;
	rr (HL);
	rr (IX+1);
	rr (IY-1);
	sla B;
	sla (HL);
	sla (IX+1);
	sla (IY-1);
	sra B;
	sra (HL);
	sra (IX+1);
	sra (IY-1);
	srl B;
	srl (HL);
	srl (IX+1);
	srl (IY-1);
	rld;
	rrd;
end;
