#include "nesemu.h"

/* Size constants */
#define MEMORY_SIZE  0x10000
#define PAGE_SIZE    0x100

/* Vectors (LO byte addr) */
#define NMI_VEC      0xFFFA
#define RESET_VEC    0xFFFC
#define IRQ_BRK_VEC  0xFFFE


#define PTR2ADDR(ptr) ((ptr) - CPU.mem)

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

uint8_t read_mem(uint16_t addr);

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
	CPU.mem[CPU.S--] = val;
}

#define HI_BYTE(addr) (addr >> 8)
#define LO_BYTE(addr) (addr & 0xFF)

void push_addr(uint16_t addr) 
{
	push(HI_BYTE(addr));
	push(LO_BYTE(addr));
}

uint8_t pull(void) 
{
	return read_mem(++CPU.S);
}

#define AS_ADDR(hi, lo) (hi << 8 | lo)

uint16_t pull_addr(void) 
{
	uint8_t lo = pull();
	uint8_t hi = pull();
	return AS_ADDR(hi, lo);
}

uint8_t read_mem(uint16_t addr)
{
	return CPU.mem[addr];
}

/* Reads address whose LO byte is pointed to by addr. */
uint16_t read_addr(uint16_t addr) 
{
	/* TODO: page wrap */
	uint8_t lo = read_mem(addr);
	uint8_t hi = read_mem(addr + 1);
	return AS_ADDR(hi, lo);
}

uint8_t peek_arg(void) 
{
	return read_mem(CPU.PC + 1);
}

uint16_t peek_addr_arg(void) 
{
	return read_addr(CPU.PC + 1);
}

void ZN(uint8_t val) 
{
	set_status_if(ZERO, !val);
	set_status_if(NEGATIVE, val & bit(7));
}


#include "modes.inc"
#include "instr.inc"


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
	#include "opcode_table.inc"
};


int step_CPU(void)
{
	static int once = 1;
	if (once) {
		init_ops();
		once = 0;
	}

	struct op op = OPCODES[read_mem(CPU.PC)];
	int penalty = 0;

	LOG("%04X ", CPU.PC);

	if (op.cycles.base == 0) {
		fprintf(stderr, "invalid opcode $%02X\n", CPU.mem[CPU.PC]);
		exit(EXIT_FAILURE);
	}

	switch (op.mode.arg) {
	case NONE:
		LOG("%02X        ", read_mem(CPU.PC));
		LOG(op.mode.fmt, op.instr.name); 
		break;
	case BYTE:
		LOG("%02X %02X     ", read_mem(CPU.PC), read_mem(CPU.PC + 1));
		if (op.cycles.penalty == BRANCH_PENALTY) {
			LOG(op.mode.fmt, op.instr.name, PTR2ADDR(op.mode.impl()));
		} else {
			LOG(op.mode.fmt, op.instr.name, *op.mode.impl());
		}
		break;
	case ADDR:
		LOG("%02X %02X %02X  ", read_mem(CPU.PC), read_mem(CPU.PC + 1), read_mem(CPU.PC + 2));
		LOG(op.mode.fmt, op.instr.name, PTR2ADDR(op.mode.impl()));
		break;
	}

	LOG("A:%02X X:%02X Y:%02X S:%02X P:%02X\n", CPU.A, CPU.X, CPU.Y, CPU.S, CPU.P);

	uint16_t old_pc = CPU.PC;

	#define PAGES_DIFFER ((old_pc & 0xFF00) != (PTR2ADDR(op.mode.impl()) & 0xFF00))

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

	if (jumped) {
	//	CPU.PC++;
	} else {
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
	CPU.P = 24;
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

