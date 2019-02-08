#pragma once

#include <array>
#include <memory>
#include <cstdint>
#include <sstream>

#include "nesemu.h"
#include "cart.h"

// Size constants 
const size_t MemorySize = 0x10000;
const size_t PageSize = 0x100;

const uint16_t StackPage = 0x0100;

// Vectors (addr of LO byte)
const uint16_t NmiVecAddr = 0xFFFA;
const uint16_t ResetVecAddr = 0xFFFC;
const uint16_t IrqBrkVecAddr = 0xFFFE;

// negative numbers point to registers
// non-negative to normal memory addresses
using ExtendedAddr = int;
const ExtendedAddr ARegisterExtAddr = -1;

using StatusRegister = FlagByte({
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

struct CpuState
{
	uint8_t a = 0;
	uint8_t x = 0; 
	uint8_t y = 0;
	uint8_t stack_ptr = 0;
	uint16_t program_counter = 0;
	StatusRegister status;

	CpuState()
	{
		status.raw = 0;
	}
};

struct CpuSnapshot : public CpuState 
{
	std::vector<uint8_t> curr_instr = {};
	unsigned cycle = 0;

	CpuSnapshot() {}

	CpuSnapshot(uint16_t pc, std::vector<uint8_t> instr, uint8_t a, uint8_t x, 
	            uint8_t y, uint8_t p, uint8_t sp, unsigned cyc)
		: cycle(cyc)
		, curr_instr(instr)
	{
		this->program_counter = pc;
		this->curr_instr = instr;
		this->a = a;
		this->x = x;
		this->y = y;
		this->status.raw = p;
		this->stack_ptr = sp;
		this->cycle = cyc;
	}

	auto operator==(const CpuSnapshot& other) -> bool
	{
		return this->program_counter == other.program_counter
			&& this->curr_instr == other.curr_instr
			&& this->a == other.a
			&& this->x == other.x
			&& this->y == other.y
			&& this->status.raw == other.status.raw
			&& this->stack_ptr == other.stack_ptr
			&& this->cycle == other.cycle;
	}

	inline auto operator!=(const CpuSnapshot& other) -> bool
	{
		return !(*this == other);
	}

	auto to_str() -> std::string
	{
		std::ostringstream strm;
		strm
			<< std::hex
			<< "CpuSnapshot{"
			<< "PC=" << program_counter << ';'
			// TODO: instr
			<< "A=" << (int)a << ';'
			<< "X=" << (int)x << ';'
			<< "Y=" << (int)y << ';'
			<< "status=" << (int)status.raw << ';'
			<< "stack_ptr=" << (int)stack_ptr << ';'
			<< "cycle=" << std::dec << cycle
			<< "}";
		return strm.str();
	}
};

class Cpu : public CpuState
{
private:
	class Memory
	{
	private:
		std::array<uint8_t, MemorySize> data;
		Cpu& cpu;
	public:
		Memory(Cpu& cpu);
		auto operator[](ExtendedAddr addr) -> uint8_t&;
	};

	class ExtPtr
	{
	private:
		Cpu& cpu;
		Mode mode;
	public:
		ExtPtr(Cpu& cpu, Mode mode);
		auto operator*() -> uint8_t&;
		auto addr() -> ExtendedAddr;
	};

public:
	unsigned cycle;
	Memory memory;

public:
	Cpu() : memory(*this)
	{
		program_counter = 0;
		stack_ptr = 0xFF;
	}

private:
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
	auto exec_instr(Instruction instr, ExtPtr ptr) -> bool;
	auto get_op(uint8_t opcode) -> Op;

public:
	auto step() -> unsigned;
	auto reset() -> void;

	auto load_cart(const Cartridge& cart) -> void;

	auto take_snapshot() -> CpuSnapshot;
};

inline auto hi_byte(uint16_t addr) -> uint8_t
{
	return addr >> 8;
}

inline auto lo_byte(uint16_t addr) -> uint8_t
{
	return (uint8_t)addr;
}

inline auto as_addr(uint8_t hi, uint8_t lo) -> uint16_t
{
	return (hi << 8) | lo;
}
