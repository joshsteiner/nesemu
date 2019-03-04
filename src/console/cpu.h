#pragma once

#include <array>
#include <memory>
#include <cstdint>
#include <sstream>

#include "common.h"
#include "cart.h"
#include "memory.h"
#include "ppu.h"

class Memory;
using ExtendedAddr = int;

// pc, instr, a, x, y, p, sp, cyc
using CpuSnapshot = std::tuple<
	uint16_t, std::vector<uint8_t>, 
	uint8_t, uint8_t, uint8_t, uint8_t, uint8_t,
	unsigned
>;

auto str(const CpuSnapshot& snap) -> std::string;

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

	using Cycles = std::pair<unsigned, Penalty>;
	using Op = std::tuple<Instruction, Mode, Cycles>;

public:
	enum class Interrupt
	{
		None, Irq, Nmi
	};

private: // constants
	static const uint16_t StackPage = 0x0100;
	static const unsigned CpuCycleWraparound = 341;

	static const ExtendedAddr NmiVecAddr = 0xFFFA;
	static const ExtendedAddr ResetVecAddr = 0xFFFC;
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

public: // ctors,dtors
	Cpu();

private: // methods
	auto push(uint8_t value) -> void;
	auto push_addr(uint16_t value) -> void;
	auto pull() -> uint8_t;
	auto pull_addr() -> uint16_t;

	auto read_addr_from_mem(uint16_t addr) -> uint16_t;

	auto peek_arg() -> uint8_t;
	auto peek_addr_arg() -> uint16_t;

	auto zn(uint8_t val) -> void;

	auto get_addr(Mode mode) -> ExtendedAddr;
	auto get_arg_size(Mode mode) -> unsigned;
	auto exec_instr(Instruction instr, ExtendedAddr addr) -> void;
	auto get_op(uint8_t opcode) -> Op;

	template <Interrupt Int>
	auto do_int() -> void
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
	// inline auto connect(Memory* memory) -> void { this->memory = memory; }

	auto step() -> unsigned;
	auto reset() -> void;

	template <Interrupt Int> 
	auto trigger() -> void 
	{ 
		if (Int == Interrupt::Nmi || !status.interrupt_disable) {
			interrupt = Int;
		}
	}

	auto stall(unsigned cycles) -> void { cycle_stall = cycles; }

	auto take_snapshot() -> CpuSnapshot;
};

extern Cpu cpu;
extern Memory memory;
