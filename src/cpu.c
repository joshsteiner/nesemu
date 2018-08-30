#include "nesemu.h"

/* Size constants */
#define MEMORY_SIZE  0x10000
#define PAGE_SIZE    0x100

#define STACK_PAGE   0x0100 

/* Vectors (LO byte addr) */
#define NMI_VEC      0xFFFA
#define RESET_VEC    0xFFFC
#define IRQ_BRK_VEC  0xFFFE

#define PTR2ADDR(ptr) ((ptr) - CPU.mem)

#define AS_ADDR(hi, lo) (hi << 8 | lo)
#define HI_BYTE(addr) (addr >> 8)
#define LO_BYTE(addr) (addr & 0xFF)


struct {
	uint8_t  A, X, Y, S, P;
	uint16_t PC;
	uint8_t mem[MEMORY_SIZE];
} CPU;

enum Status {
	CARRY             = bit(0),
	ZERO              = bit(1),
	INTERRUPT_DISABLE = bit(2),
	DECIMAL_MODE      = bit(3),
	BREAK             = bit(4),
	OVERFLOW          = bit(6),
	NEGATIVE          = bit(7)
};


uint8_t read_mem(int addr);

void set_status(enum Status mask) 
{
	CPU.P |= mask;
}

void clear_status(enum Status mask) 
{
	CPU.P &= ~mask;
}

int status_is_set(enum Status mask) 
{
	return !!(CPU.P & mask);
}

int status_is_clear(enum Status mask)
{
	return !(CPU.P & mask);
}

void set_status_if(enum Status mask, int cond)
{
	if (cond) {
		set_status(mask);
	} else {
		clear_status(mask);
	}
}

void push(uint8_t val) 
{
	CPU.mem[STACK_PAGE + CPU.S--] = val;
}

void push_addr(uint16_t addr) 
{
	push(HI_BYTE(addr));
	push(LO_BYTE(addr));
}

uint8_t pull(void) 
{
	return read_mem(STACK_PAGE + ++CPU.S);
}

uint16_t pull_addr(void) 
{
	uint8_t lo = pull();
	uint8_t hi = pull();
	return AS_ADDR(hi, lo);
}

enum {
	INVALID_ADDR = -100,
	A_ADDR
};

uint8_t read_mem(int addr)
{
	/* page wrap */
	if (addr >= 0) {
		return CPU.mem[addr];
	}

	switch (addr) {
	case A_ADDR:
		return CPU.A;
	}
}


void write_mem(int addr, uint8_t val)
{
	if (addr >= 0) {
		CPU.mem[addr] = val;
	}

	switch (addr) {
	case A_ADDR:
		CPU.A = val;
	}
}

/* Reads address whose LO byte is pointed to by addr. */
uint16_t read_addr(uint16_t addr) 
{
	uint16_t page = 0xFF00 & addr;
	return AS_ADDR(
		read_mem(((addr + 1) & 0xFF) | page),
		read_mem(addr)
	);
}

uint8_t peek_arg(void) 
{
	return read_mem(CPU.PC + 1);
}

uint16_t peek_addr_arg(void) 
{
	return AS_ADDR(read_mem(CPU.PC + 2), read_mem(CPU.PC + 1));
}

void ZN(uint8_t val) 
{
	set_status_if(ZERO, !val);
	set_status_if(NEGATIVE, val & bit(7));
}


typedef int (*mode_impl)(void);

struct mode {
	mode_impl impl;
	const char *fmt;
	const char *name;
	enum { NONE=0, BYTE=1, ADDR=2 } arg;
};

#define MODE(NAME, ARG, FMT, GET)                    \
static int NAME##_impl(void) { return GET; }         \
static const struct mode NAME = {                    \
	.impl = NAME##_impl,                         \
	.arg  = ARG,                                 \
	.fmt  = FMT,                                 \
	.name = #NAME                                \
};

MODE(IMPLIED,      NONE,  "%s         ",    INVALID_ADDR)
MODE(ACCUMULATOR,  NONE,  "%s A       ",    A_ADDR)
MODE(IMMEDIATE,    BYTE,  "%s #$%02X   ",   CPU.PC + 1)
MODE(RELATIVE,     BYTE,  "%s $%04X   ",    CPU.PC + (int8_t)peek_arg() + 2)
MODE(ZERO_PAGE,    BYTE,  "%s $%02X     ",  peek_arg())
MODE(ZERO_PAGE_X,  BYTE,  "%s $%02X,X   ",  (peek_arg() + CPU.X) % PAGE_SIZE)
MODE(ZERO_PAGE_Y,  BYTE,  "%s $%02X,Y   ",  (peek_arg() + CPU.Y) % PAGE_SIZE)
MODE(ABSOLUTE,     ADDR,  "%s $%04X   ",    peek_addr_arg())
MODE(ABSOLUTE_X,   ADDR,  "%s $%04X,X ",    (peek_addr_arg() + CPU.X) % MEMORY_SIZE)
MODE(ABSOLUTE_Y,   ADDR,  "%s $%04X,Y ",    (peek_addr_arg() + CPU.Y) % MEMORY_SIZE)
MODE(INDIRECT,     ADDR,  "%s ($%04X) ",    read_addr(peek_addr_arg()))

MODE(INDIRECT_X,   BYTE,  "%s ($%02X,X) ",  AS_ADDR(
	read_mem((peek_arg() + CPU.X + 1) % PAGE_SIZE),
	read_mem((peek_arg() + CPU.X) % PAGE_SIZE)
))

MODE(INDIRECT_Y,   BYTE,  "%s ($%02X),Y ",  
	(AS_ADDR(
		read_mem((peek_arg() + 1) % PAGE_SIZE), 
		read_mem(peek_arg())
	) + CPU.Y) % MEMORY_SIZE 
)


struct instr {
	int (*impl)(mode_impl m);
	const char *name;	
};

/* instructions that use return values can return early */
#define INSTR(NAME, EXEC)                                            \
static int NAME##_impl(mode_impl m) { do EXEC while (0); return 0; } \
static const struct instr NAME = {                                   \
	.name = #NAME,                                               \
	.impl = NAME##_impl                                          \
};

#define M_GET    read_mem(m())
#define M_SET(v) write_mem(m(), v)
#define M_ADDR   m()

INSTR(NOP, {})

INSTR(INC, { M_SET(M_GET + 1); ZN(M_GET); })
INSTR(INX, { ZN(++CPU.X); })
INSTR(INY, { ZN(++CPU.Y); })

INSTR(DEC, { M_SET(M_GET - 1); ZN(M_GET); })
INSTR(DEX, { ZN(--CPU.X); })
INSTR(DEY, { ZN(--CPU.Y); })

INSTR(CLC, { clear_status(CARRY); })
INSTR(CLD, { clear_status(DECIMAL_MODE); })
INSTR(CLI, { clear_status(INTERRUPT_DISABLE); })
INSTR(CLV, { clear_status(OVERFLOW); })

INSTR(SEC, { set_status(CARRY); })
INSTR(SED, { set_status(DECIMAL_MODE); })
INSTR(SEI, { set_status(INTERRUPT_DISABLE); })

INSTR(TAX, { ZN(CPU.X = CPU.A); })
INSTR(TAY, { ZN(CPU.Y = CPU.A); })
INSTR(TXA, { ZN(CPU.A = CPU.X); })
INSTR(TYA, { ZN(CPU.A = CPU.Y); })
INSTR(TXS, { CPU.S = CPU.X; })
INSTR(TSX, { ZN(CPU.X = CPU.S); })

INSTR(PHP, { push(CPU.P | bit(5) | BREAK); })
INSTR(PHA, { push(CPU.A); })

INSTR(PLP, { CPU.P = pull() & 0xEF | 0x20; })
INSTR(PLA, { ZN(CPU.A = pull()); })

INSTR(BCS, { if (status_is_set(CARRY))      { CPU.PC = M_ADDR; return 1; } })
INSTR(BCC, { if (status_is_clear(CARRY))    { CPU.PC = M_ADDR; return 1; } })
INSTR(BEQ, { if (status_is_set(ZERO))       { CPU.PC = M_ADDR; return 1; } })
INSTR(BNE, { if (status_is_clear(ZERO))     { CPU.PC = M_ADDR; return 1; } })
INSTR(BMI, { if (status_is_set(NEGATIVE))   { CPU.PC = M_ADDR; return 1; } })
INSTR(BPL, { if (status_is_clear(NEGATIVE)) { CPU.PC = M_ADDR; return 1; } })
INSTR(BVS, { if (status_is_set(OVERFLOW))   { CPU.PC = M_ADDR; return 1; } })
INSTR(BVC, { if (status_is_clear(OVERFLOW)) { CPU.PC = M_ADDR; return 1; } })

INSTR(LDA, { ZN(CPU.A = M_GET); })
INSTR(LDX, { ZN(CPU.X = M_GET); })
INSTR(LDY, { ZN(CPU.Y = M_GET); })
INSTR(STA, { M_SET(CPU.A); })
INSTR(STX, { M_SET(CPU.X); })
INSTR(STY, { M_SET(CPU.Y); })

INSTR(BIT, {
	set_status_if(ZERO, (CPU.A & M_GET) == 0);
	set_status_if(OVERFLOW, M_GET & bit(6));
	set_status_if(NEGATIVE, M_GET & bit(7));
})

static void compare(uint8_t reg, mode_impl m) 
{
	set_status_if(CARRY, reg >= M_GET);
	set_status_if(ZERO, reg == M_GET);
	set_status_if(NEGATIVE, (reg - M_GET) & bit(7));
}

INSTR(CMP, { compare(CPU.A, m); })
INSTR(CPX, { compare(CPU.X, m); })
INSTR(CPY, { compare(CPU.Y, m); })

INSTR(AND, { ZN(CPU.A &= M_GET); })
INSTR(ORA, { ZN(CPU.A |= M_GET); })
INSTR(EOR, { ZN(CPU.A ^= M_GET); })

INSTR(ASL, {
	set_status_if(CARRY, M_GET & bit(7));
	M_SET(M_GET << 1);
	ZN(M_GET);
})

INSTR(LSR, { 
	set_status_if(CARRY, M_GET & bit(0));
	M_SET(M_GET >> 1);
	ZN(M_GET);
})

INSTR(ROL, {
	int old_carry = status_is_set(CARRY) ? 1 : 0;
	set_status_if(CARRY, M_GET & bit(7));
	M_SET(M_GET << 1 | old_carry);
	ZN(M_GET);
})

INSTR(ROR, {
	int old_carry = status_is_set(CARRY) ? 1 : 0;
	set_status_if(CARRY, M_GET & bit(0));
	M_SET(M_GET >> 1 | old_carry << 7);
	ZN(M_GET);
})

INSTR(ADC, {
	uint8_t a = CPU.A;
	CPU.A += M_GET + status_is_set(CARRY);
	ZN(CPU.A);
	set_status_if(CARRY, a + M_GET + status_is_set(CARRY) > 0xFF);
	set_status_if(OVERFLOW, !((a ^ M_GET) & bit(7)) && (a ^ CPU.A) & bit(7));
})

INSTR(SBC, {
	uint8_t a = CPU.A;
	CPU.A = a - M_GET - (1 - status_is_set(CARRY));
	ZN(CPU.A);
	set_status_if(CARRY, a - M_GET - status_is_clear(CARRY) >= 0x00);
	set_status_if(OVERFLOW, (a ^ M_GET) & bit(7) && (a ^ CPU.A) & bit(7));
})

INSTR(JMP, { CPU.PC = M_ADDR; return 1; })

INSTR(JSR, {
	push_addr(CPU.PC + 2);
	CPU.PC = M_ADDR;
	return 1;
})

INSTR(RTS, { 
	CPU.PC = pull_addr() + 1; 
	return 1; 
})

INSTR(BRK, {
	++CPU.PC;
	push_addr(CPU.PC);
	push(CPU.P | BREAK | bit(5));
	set_status(BREAK);
	CPU.PC = read_addr(IRQ_BRK_VEC);
	return 1;
})

INSTR(RTI, {
	CPU.P = pull() & 0xEF | 0x20;
	CPU.PC = pull_addr();
	return 1; 
})

struct cycles {
	int base;
	enum {
		NO_PENALTY, 
		PAGE_CROSS_PENALTY, 
		BRANCH_PENALTY
	} penalty;
};

#define CYCLES( BASE, PENALTY ) \
	(struct cycles){ .base = BASE, .penalty = PENALTY }


struct op {
	struct instr instr;
	struct mode mode;
	struct cycles cycles;
};

#define INVALID { { 0,0 }, { 0,0,0,0 }, { 0,0 } } 

// instruction, mode, length, cycle base, cycle penalty
static struct op OPCODES[16 * 16] = { INVALID };

static void init_ops(void)
{
	#define DEF_OP(n,i,m,c) OPCODES[n] = (struct op){ i, m, c }

	DEF_OP( 0x00, BRK, IMPLIED,     CYCLES(7, NO_PENALTY) );
	DEF_OP( 0x01, ORA, INDIRECT_X,  CYCLES(6, NO_PENALTY) );
	DEF_OP( 0x05, ORA, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0x06, ASL, ZERO_PAGE,   CYCLES(5, NO_PENALTY) );
	DEF_OP( 0x08, PHP, IMPLIED,     CYCLES(3, NO_PENALTY) );
	DEF_OP( 0x09, ORA, IMMEDIATE,   CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x0a, ASL, ACCUMULATOR, CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x0d, ORA, ABSOLUTE,    CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x0e, ASL, ABSOLUTE,    CYCLES(6, NO_PENALTY) );

	DEF_OP( 0x10, BPL, RELATIVE,    CYCLES(2, BRANCH_PENALTY) );
	DEF_OP( 0x11, ORA, INDIRECT_Y,  CYCLES(5, PAGE_CROSS_PENALTY) );
	DEF_OP( 0x15, ORA, ZERO_PAGE_X, CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x16, ASL, ZERO_PAGE_X, CYCLES(6, NO_PENALTY) );
	DEF_OP( 0x18, CLC, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x19, ORA, ABSOLUTE_Y,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0x1d, ORA, ABSOLUTE_X,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0x1e, ASL, ABSOLUTE_X,  CYCLES(7, NO_PENALTY) );

	DEF_OP( 0x20, JSR, ABSOLUTE,    CYCLES(6, NO_PENALTY) );
	DEF_OP( 0x21, AND, INDIRECT_X,  CYCLES(6, NO_PENALTY) );
	DEF_OP( 0x24, BIT, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0x25, AND, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0x26, ROL, ZERO_PAGE,   CYCLES(5, NO_PENALTY) );
	DEF_OP( 0x28, PLP, IMPLIED,     CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x29, AND, IMMEDIATE,   CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x2a, ROL, ACCUMULATOR, CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x2c, BIT, ABSOLUTE,    CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x2d, AND, ABSOLUTE,    CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x2e, ROL, ABSOLUTE,    CYCLES(6, NO_PENALTY) );

	DEF_OP( 0x30, BMI, RELATIVE,    CYCLES(2, BRANCH_PENALTY) );
	DEF_OP( 0x31, AND, INDIRECT_Y,  CYCLES(5, PAGE_CROSS_PENALTY) );
	DEF_OP( 0x35, AND, ZERO_PAGE_X, CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x36, ROL, ZERO_PAGE_X, CYCLES(6, NO_PENALTY) );
	DEF_OP( 0x38, SEC, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x39, AND, ABSOLUTE_Y,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0x3d, AND, ABSOLUTE_X,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0x3e, ROL, ABSOLUTE_X,  CYCLES(7, NO_PENALTY) );

	DEF_OP( 0x40, RTI, IMPLIED,     CYCLES(6, NO_PENALTY) );
	DEF_OP( 0x41, EOR, INDIRECT_X,  CYCLES(6, NO_PENALTY) );
	DEF_OP( 0x45, EOR, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0x46, LSR, ZERO_PAGE,   CYCLES(5, NO_PENALTY) );
	DEF_OP( 0x48, PHA, IMPLIED,     CYCLES(3, NO_PENALTY) );
	DEF_OP( 0x49, EOR, IMMEDIATE,   CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x4a, LSR, ACCUMULATOR, CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x4c, JMP, ABSOLUTE,    CYCLES(3, NO_PENALTY) );
	DEF_OP( 0x4d, EOR, ABSOLUTE,    CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x4e, LSR, ABSOLUTE,    CYCLES(6, NO_PENALTY) );

	DEF_OP( 0x50, BVC, RELATIVE,    CYCLES(2, BRANCH_PENALTY) );
	DEF_OP( 0x51, EOR, INDIRECT_Y , CYCLES(5, PAGE_CROSS_PENALTY) );
	DEF_OP( 0x55, EOR, ZERO_PAGE_X, CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x56, LSR, ZERO_PAGE_X, CYCLES(6, NO_PENALTY) );
	DEF_OP( 0x58, CLI, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x59, EOR, ABSOLUTE_Y,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0x5d, EOR, ABSOLUTE_X,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0x5e, LSR, ABSOLUTE_X,  CYCLES(7, NO_PENALTY) );

	DEF_OP( 0x60, RTS, IMPLIED,     CYCLES(6, NO_PENALTY) );
	DEF_OP( 0x61, ADC, INDIRECT_X,  CYCLES(6, NO_PENALTY) );
	DEF_OP( 0x65, ADC, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0x66, ROR, ZERO_PAGE,   CYCLES(5, NO_PENALTY) );
	DEF_OP( 0x68, PLA, IMPLIED,     CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x69, ADC, IMMEDIATE,   CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x6a, ROR, ACCUMULATOR, CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x6c, JMP, INDIRECT,    CYCLES(5, NO_PENALTY) );
	DEF_OP( 0x6d, ADC, ABSOLUTE,    CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x6e, ROR, ABSOLUTE,    CYCLES(6, NO_PENALTY) );

	DEF_OP( 0x70, BVS, RELATIVE,    CYCLES(2, BRANCH_PENALTY) );
	DEF_OP( 0x71, ADC, INDIRECT_Y , CYCLES(5, PAGE_CROSS_PENALTY) );
	DEF_OP( 0x75, ADC, ZERO_PAGE_X, CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x76, ROR, ZERO_PAGE_X, CYCLES(6, NO_PENALTY) );
	DEF_OP( 0x78, SEI, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x79, ADC, ABSOLUTE_Y,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0x7d, ADC, ABSOLUTE_X,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0x7e, ROR, ABSOLUTE_X,  CYCLES(7, NO_PENALTY) );

	DEF_OP( 0x81, STA, INDIRECT_X,  CYCLES(6, NO_PENALTY) );
	DEF_OP( 0x84, STY, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0x85, STA, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0x86, STX, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0x88, DEY, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x8a, TXA, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x8c, STY, ABSOLUTE,    CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x8d, STA, ABSOLUTE,    CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x8e, STX, ABSOLUTE,    CYCLES(4, NO_PENALTY) );

	DEF_OP( 0x90, BCC, RELATIVE,    CYCLES(2, BRANCH_PENALTY) );
	DEF_OP( 0x91, STA, INDIRECT_Y , CYCLES(6, NO_PENALTY) );
	DEF_OP( 0x94, STY, ZERO_PAGE_X, CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x95, STA, ZERO_PAGE_X, CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x96, STX, ZERO_PAGE_Y, CYCLES(4, NO_PENALTY) );
	DEF_OP( 0x98, TYA, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x99, STA, ABSOLUTE_Y,  CYCLES(5, NO_PENALTY) );
	DEF_OP( 0x9a, TXS, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0x9d, STA, ABSOLUTE_X,  CYCLES(5, NO_PENALTY) );

	DEF_OP( 0xa0, LDY, IMMEDIATE,   CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xa1, LDA, INDIRECT_X,  CYCLES(6, NO_PENALTY) );
	DEF_OP( 0xa2, LDX, IMMEDIATE,   CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xa4, LDY, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0xa5, LDA, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0xa6, LDX, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0xa8, TAY, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xa9, LDA, IMMEDIATE,   CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xaa, TAX, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xac, LDY, ABSOLUTE,    CYCLES(4, NO_PENALTY) );
	DEF_OP( 0xad, LDA, ABSOLUTE,    CYCLES(4, NO_PENALTY) );
	DEF_OP( 0xae, LDX, ABSOLUTE,    CYCLES(4, NO_PENALTY) );

	DEF_OP( 0xb0, BCS, RELATIVE,    CYCLES(2, BRANCH_PENALTY) );
	DEF_OP( 0xb1, LDA, INDIRECT_Y , CYCLES(5, PAGE_CROSS_PENALTY) );
	DEF_OP( 0xb4, LDY, ZERO_PAGE_X, CYCLES(4, NO_PENALTY) );
	DEF_OP( 0xb5, LDA, ZERO_PAGE_X, CYCLES(4, NO_PENALTY) );
	DEF_OP( 0xb6, LDX, ZERO_PAGE_Y, CYCLES(4, NO_PENALTY) );
	DEF_OP( 0xb8, CLV, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xb9, LDA, ABSOLUTE_Y,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0xba, TSX, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xbc, LDY, ABSOLUTE_X,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0xbd, LDA, ABSOLUTE_X,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0xbe, LDX, ABSOLUTE_Y,  CYCLES(4, PAGE_CROSS_PENALTY) );

	DEF_OP( 0xc0, CPY, IMMEDIATE,   CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xc1, CMP, INDIRECT_X,  CYCLES(6, NO_PENALTY) );
	DEF_OP( 0xc4, CPY, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0xc5, CMP, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0xc6, DEC, ZERO_PAGE,   CYCLES(5, NO_PENALTY) );
	DEF_OP( 0xc8, INY, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xc9, CMP, IMMEDIATE,   CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xca, DEX, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xcc, CPY, ABSOLUTE,    CYCLES(4, NO_PENALTY) );
	DEF_OP( 0xcd, CMP, ABSOLUTE,    CYCLES(4, NO_PENALTY) );
	DEF_OP( 0xce, DEC, ABSOLUTE,    CYCLES(6, NO_PENALTY) );

	DEF_OP( 0xd0, BNE, RELATIVE,    CYCLES(2, BRANCH_PENALTY) );
	DEF_OP( 0xd1, CMP, INDIRECT_Y , CYCLES(5, PAGE_CROSS_PENALTY) );
	DEF_OP( 0xd5, CMP, ZERO_PAGE_X, CYCLES(4, NO_PENALTY) );
	DEF_OP( 0xd6, DEC, ZERO_PAGE_X, CYCLES(6, NO_PENALTY) );
	DEF_OP( 0xd8, CLD, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xd9, CMP, ABSOLUTE_Y,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0xdd, CMP, ABSOLUTE_X,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0xde, DEC, ABSOLUTE_X,  CYCLES(7, NO_PENALTY) );

	DEF_OP( 0xe0, CPX, IMMEDIATE,   CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xe1, SBC, INDIRECT_X,  CYCLES(6, NO_PENALTY) );
	DEF_OP( 0xe4, CPX, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0xe5, SBC, ZERO_PAGE,   CYCLES(3, NO_PENALTY) );
	DEF_OP( 0xe6, INC, ZERO_PAGE,   CYCLES(5, NO_PENALTY) );
	DEF_OP( 0xe8, INX, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xe9, SBC, IMMEDIATE,   CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xea, NOP, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xec, CPX, ABSOLUTE,    CYCLES(4, NO_PENALTY) );
	DEF_OP( 0xed, SBC, ABSOLUTE,    CYCLES(4, NO_PENALTY) );
	DEF_OP( 0xee, INC, ABSOLUTE,    CYCLES(6, NO_PENALTY) );

	DEF_OP( 0xf0, BEQ, RELATIVE,    CYCLES(2, BRANCH_PENALTY) );
	DEF_OP( 0xf1, SBC, INDIRECT_Y , CYCLES(5, PAGE_CROSS_PENALTY) );
	DEF_OP( 0xf5, SBC, ZERO_PAGE_X, CYCLES(4, NO_PENALTY) );
	DEF_OP( 0xf6, INC, ZERO_PAGE_X, CYCLES(6, NO_PENALTY) );
	DEF_OP( 0xf8, SED, IMPLIED,     CYCLES(2, NO_PENALTY) );
	DEF_OP( 0xf9, SBC, ABSOLUTE_Y,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0xfd, SBC, ABSOLUTE_X,  CYCLES(4, PAGE_CROSS_PENALTY) );
	DEF_OP( 0xfe, INC, ABSOLUTE_X,  CYCLES(7, NO_PENALTY) );

	#undef DEF_OP
};

void log_CPU_step(const struct op *op)
{
	LOG("%04X ", CPU.PC);

	if (op->cycles.base == 0) {
		fprintf(stderr, "invalid opcode $%02X\n", CPU.mem[CPU.PC]);
		exit(EXIT_FAILURE);
	}

	switch (op->mode.arg) {
	case NONE:
		LOG("%02X        ", read_mem(CPU.PC));
		LOG(op->mode.fmt, op->instr.name);
		break;
	case BYTE:
		LOG("%02X %02X     ", read_mem(CPU.PC), read_mem(CPU.PC + 1));
		LOG(op->mode.fmt, op->instr.name, read_mem(op->mode.impl()));
		break;
	case ADDR:
		LOG("%02X %02X %02X  ", read_mem(CPU.PC), read_mem(CPU.PC + 1), read_mem(CPU.PC + 2));
		LOG(op->mode.fmt, op->instr.name, read_mem(op->mode.impl()));
		break;
	}

	LOG("A:%02X X:%02X Y:%02X S:%02X P:%02X\n", CPU.A, CPU.X, CPU.Y, CPU.S, CPU.P);
}

int step_CPU(void)
{
	static int once = 1;
	if (once) {
		init_ops();
		once = 0;
	}

	struct op op = OPCODES[read_mem(CPU.PC)];
	int penalty = 0;

	log_CPU_step(&op);

	uint16_t old_pc = CPU.PC;

	#define PAGES_DIFFER ((old_pc & 0xFF00) != ((op.mode.impl()) & 0xFF00))

	int jumped = op.instr.impl(op.mode.impl);

	// do instruction, add cycle penalty
	switch (op.cycles.penalty) {
	case BRANCH_PENALTY:
		if (jumped) {
			penalty++;
			if (PAGES_DIFFER) {
				penalty++;
			}
		}
		break;

	case PAGE_CROSS_PENALTY:
		if (PAGES_DIFFER) {
			penalty++;
		}
		break;

	case NO_PENALTY:
		break;
	}

	if (!jumped) {
		CPU.PC += op.mode.arg + 1;
	}

	return op.cycles.base + penalty;
}

void reset_CPU(void) 
{
	CPU.PC = read_addr(RESET_VEC);
}

void run_CPU(void)
{
	while (CPU.PC != MEMORY_SIZE - 1) {
		step_CPU();
	}
}

void run_nestest(void)
{
	CPU.S = 0xFF;
	CPU.P = 0x24;
	CPU.PC = 0;
	CPU.mem[1] = 0x00;
	CPU.mem[2] = 0xC0;

	uint16_t return_addr = CPU.PC + 3;
	uint8_t zero = CPU.mem[0x00];

	JSR_impl(ABSOLUTE_impl);
	
	do {
		step_CPU();
		if (CPU.mem[0x00] != zero) {
			zero = CPU.mem[0x00];
			fprintf(stderr, "$00: $%02X @ PC=$%04X\n", zero, CPU.PC);
		}
	} while (CPU.PC != return_addr);
}

void dump_mem(const char *filename)
{
	FILE *f = fopen(filename, "w");
	for (int i = 0; i < MEMORY_SIZE; i++) {
		fputc(CPU.mem[i], f);
	}
	fclose(f);
}

void load_cart(struct Cartridge *cart)
{
	switch (cart->prg_rom_size / 0x4000) {
	case 1:
		memcpy(CPU.mem + 0xC000, cart->prg_rom, 0x4000 * sizeof(uint8_t));
		break;
	case 2:
		memcpy(CPU.mem + 0x8000, cart->prg_rom, 0x8000 * sizeof(uint8_t));
		break;
	default:
		fprintf(stderr, "too many ROM banks!\n");
		exit(EXIT_FAILURE);
	}
}

