struct instr {
	int (*impl)(mode_impl m);
	const char *name;	
};

/* instructions that use return values can return early */
#define INSTR( NAME, EXEC )                                          \
static int NAME##_impl(mode_impl m) { do EXEC while (0); return 0; } \
static const struct instr NAME = {                                   \
	.name = #NAME,                                               \
	.impl = NAME##_impl                                          \
};


INSTR( NOP, {} )

INSTR( INC, { ZN(++*m()); } )
INSTR( INX, { ZN(++CPU.X); } )
INSTR( INY, { ZN(++CPU.Y); } )

INSTR( DEC, { ZN(--*m()); } )
INSTR( DEX, { ZN(--CPU.X); } )
INSTR( DEY, { ZN(--CPU.Y); } )

INSTR( CLC, { clear_status(CARRY); } )
INSTR( CLD, { clear_status(DECIMAL_MODE); } )
INSTR( CLI, { clear_status(INTERRUPT_DISABLE); } )
INSTR( CLV, { clear_status(OVERFLOW); } )

INSTR( SEC, { set_status(CARRY); } )
INSTR( SED, { set_status(DECIMAL_MODE); } )
INSTR( SEI, { set_status(INTERRUPT_DISABLE); } )

INSTR( TAX, { ZN(CPU.X = CPU.A); } )
INSTR( TAY, { ZN(CPU.Y = CPU.A); } )
INSTR( TXA, { ZN(CPU.A = CPU.X); } )
INSTR( TYA, { ZN(CPU.A = CPU.Y); } )
INSTR( TXS, { ZN(CPU.S = CPU.X); } )
INSTR( TSX, { ZN(CPU.X = CPU.S); } )

// TODO: set 5th bit? set BREAK?
INSTR( PHP, { push(CPU.P | bit(5) | BREAK); } )
INSTR( PHA, { push(CPU.A); } )

INSTR( PLP, { CPU.P = pull(); } )
INSTR( PLA, { ZN(CPU.A = pull()); } )

INSTR( BCS, { if (status_is_set(CARRY))      { CPU.PC = PTR2ADDR(m()); return 1; } } )
INSTR( BCC, { if (status_is_clear(CARRY))    { CPU.PC = PTR2ADDR(m()); return 1; } } )
INSTR( BEQ, { if (status_is_set(ZERO))       { CPU.PC = PTR2ADDR(m()); return 1; } } )
INSTR( BNE, { if (status_is_clear(ZERO))     { CPU.PC = PTR2ADDR(m()); return 1; } } )
INSTR( BMI, { if (status_is_set(NEGATIVE))   { CPU.PC = PTR2ADDR(m()); return 1; } } )
INSTR( BPL, { if (status_is_clear(NEGATIVE)) { CPU.PC = PTR2ADDR(m()); return 1; } } )
INSTR( BVS, { if (status_is_set(OVERFLOW))   { CPU.PC = PTR2ADDR(m()); return 1; } } )
INSTR( BVC, { if (status_is_clear(OVERFLOW)) { CPU.PC = PTR2ADDR(m()); return 1; } } )
    
INSTR( LDA, { ZN(CPU.A = *m()); } )
INSTR( LDX, { ZN(CPU.X = *m()); } )
INSTR( LDY, { ZN(CPU.Y = *m()); } )
INSTR( STA, { ZN(*m() = CPU.A); } )
INSTR( STX, { ZN(*m() = CPU.X); } )
INSTR( STY, { ZN(*m() = CPU.Y); } )

INSTR( BIT, {
	set_status_if(ZERO, *m());
	set_status_if(OVERFLOW, *m() & bit(6));
	set_status_if(NEGATIVE, *m() & bit(7));
} )

static void compare(uint8_t reg, mode_impl m) 
{
	set_status_if(CARRY, reg >= *m());
	set_status_if(ZERO, reg == *m());
	set_status_if(NEGATIVE, (reg - *m()) & bit(7));
}

INSTR( CMP, { compare(CPU.A, m); } )
INSTR( CPX, { compare(CPU.X, m); } )
INSTR( CPY, { compare(CPU.Y, m); } )

INSTR( AND, { ZN(CPU.A &= *m()); } )
INSTR( ORA, { ZN(CPU.A |= *m()); } )
INSTR( EOR, { ZN(CPU.A ^= *m()); } )

INSTR( ASL, {
	set_status_if(CARRY, *m() & bit(7));
	ZN(*m() <<= 1);
} )

INSTR( LSR, { 
	set_status_if(CARRY, *m() & bit(1));
	ZN(*m() >>= 1);
} )

INSTR( ROL, {
	int old_carry = status_is_set(CARRY);
	set_status_if(CARRY, *m() & bit(7));
	*m() <<= 1;
	ZN(*m() |= old_carry ? bit(1) : 0);
} )

INSTR( ROR, {
	int old_carry = status_is_set(CARRY);
	set_status_if(CARRY, *m() & bit(0));
	*m() >>= 1;
	ZN(*m() |= old_carry ? bit(7) : 0);
} )

INSTR( ADC, {
	uint8_t a = CPU.A;
	CPU.A += *m() + status_is_set(CARRY);
	ZN(CPU.A);
	set_status_if(CARRY, a + *m() + status_is_set(CARRY) > 0xFF);
	set_status_if(OVERFLOW, (a ^ *m()) & bit(7) == 0 && (a ^ CPU.A) & bit(7) != 0);
} )

INSTR( SBC, {
	uint8_t a = CPU.A;
	CPU.A -= *m() + status_is_clear(CARRY);
	ZN(CPU.A);
	set_status_if(CARRY, a - *m() - status_is_clear(CARRY) >= 0x00);
	set_status_if(OVERFLOW, (a ^ *m()) & bit(7) != 0 && (a ^ CPU.A) & bit(7) != 0);
} )

INSTR( JMP, { CPU.PC = PTR2ADDR(m()); return 1; } )

INSTR( JSR, {
	push_addr(CPU.PC + 2);
	CPU.PC = PTR2ADDR(m());
	return 1;
} )

INSTR( RTS, { CPU.PC = pull_addr() + 1; return 1; } )

INSTR( BRK, {
	++CPU.PC;
	push_addr(CPU.PC);
	push(CPU.P | BREAK | bit(5));
	set_status(BREAK);
	CPU.PC = read_addr(IRQ_BRK_VEC);
	return 1;
} )

INSTR( RTI, {
	CPU.P = pull();
	CPU.PC = pull_addr();
	return 1;
} )