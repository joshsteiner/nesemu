#ifndef NESEMU_CPU_H
#define NESEMU_CPU_H

#include "common.h"
#include "cart.h"
#include "memory.h"
#include "ppu.h"

#include <array>
#include <memory>
#include <cstdint>
#include <sstream>

class Memory;
using ExtendedAddr = int;

class CpuSnapshot
{
public:
	const uint16_t pc;
	const std::vector<uint8_t> instr;
	const uint8_t a, x, y;
	const uint8_t p, sp;
	const unsigned cyc;

public:
	CpuSnapshot(uint16_t pc, std::vector<uint8_t> instr, uint8_t a,
	            uint8_t x, uint8_t y, uint8_t p, uint8_t sp, unsigned cyc);

	std::string str() const;
};

class Cpu 
{
	friend class Memory;
	friend class ExtPtr;

private: // types
	using StatusRegister = FLAG_BYTE({
		unsigned carry             : 1;
		unsigned zero              : 1;
		unsigned interrupt_disable : 1;
		unsigned decimal_mode      : 1;
		unsigned break_            : 1;
		unsigned                   : 1;
		unsigned overflow          : 1;
		unsigned negative          : 1;
	});

	enum Mode
	{
		Implied, Accumulator, Immediate, Relative,
		ZeroPage, ZeroPageX, ZeroPageY,
		Absolute, AbsoluteX, AbsoluteY,
		Indirect, IndirectX, IndirectY
	};
	
	enum Instruction
	{
		Nop,
		Inc, Inx, Iny, Dec, Dex, Dey, 
		Clc, Cld, Cli, Clv, Sec, Sed, Sei,
		Tax, Tay, Txa, Tya, Txs, Tsx,
		Php, Pha, Plp, Pla,
		Bcs, Bcc, Beq, Bne, Bmi, Bpl, Bvs, Bvc,
		Lda, Ldx, Ldy, Sta, Stx, Sty,
		Bit, Cmp, Cpx, Cpy,
		And, Ora, Eor, Asl, Lsr, Rol, Ror, Adc, Sbc,
		Jmp, Jsr, Rts, 
		Brk, Rti
	};

	enum class Penalty 
	{
		None, PageCross, Branch
	};

	struct Op
	{
		Instruction instr;
		Mode mode;
		unsigned base_cycle;
		Penalty penalty;

		Op(Instruction instr, Mode mode, unsigned base_cycle, Penalty penalty)
			: instr(instr)
			, mode(mode)
			, base_cycle(base_cycle)
			, penalty(penalty)
		{}
 
	};

public:
	enum class Interrupt
	{
		None, Irq, Nmi
	};

private: // constants
	static const uint16_t StackPage = 0x0100;
	static const unsigned CpuCycleWraparound = 341;

	static const ExtendedAddr NmiVecAddr    = 0xFFFA;
	static const ExtendedAddr ResetVecAddr  = 0xFFFC;
	static const ExtendedAddr IrqBrkVecAddr = 0xFFFE;
	
public: // members
	// registers
	uint8_t a, x, y;
	uint8_t stack_ptr;
	uint16_t program_counter;
	StatusRegister status;

	unsigned cycle;
	unsigned cycle_stall;

private: // members
	bool jumped;
	bool page_crossed;
	Interrupt interrupt;

	// Memory* memory;

public: // ctors, dtors
	Cpu();

private: // methods
	void push(uint8_t value);
	void push_addr(uint16_t value);
	uint8_t pull();
	uint16_t pull_addr();

	uint16_t read_addr_from_mem(uint16_t addr);

	uint8_t peek_arg();
	uint16_t peek_addr_arg();

	void zn(uint8_t val);

	ExtendedAddr get_addr(Mode mode);
	unsigned get_arg_size(Mode mode);
	void exec_instr(Instruction instr, ExtendedAddr addr);
	Op get_op(uint8_t opcode);

	template <Interrupt Int>
	void do_int()
	{
		push_addr(program_counter);
		push(status.raw | bit(4) | bit(5));
		ExtendedAddr addr;
		switch (Int) {
		case Interrupt::Irq: addr = IrqBrkVecAddr; break;
		case Interrupt::Nmi: addr = NmiVecAddr; break;
		}
		program_counter = read_addr_from_mem(addr);
		status.break_ = 1;
		status.interrupt_disable = 1;
		cycle += 7;
		interrupt = Interrupt::None;
	}

public: // methods
	// inline auto connect(Memory* memory) { this->memory = memory; }

	unsigned step();
	void reset();

	template <Interrupt Int> 
	void trigger() 
	{ 
		if (Int == Interrupt::Nmi || !status.interrupt_disable) {
			interrupt = Int;
		}
	}

	void stall(unsigned cycles) { cycle_stall = cycles; }

	CpuSnapshot take_snapshot();
};

extern Cpu cpu;
extern Memory memory;

#endif
