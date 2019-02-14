#pragma once

#include <array>
#include <memory>
#include <cstdint>
#include <sstream>
#include <cstdio>

#include "common.h"
#include "cart.h"

class Ppu;

const size_t MemorySize = 0x10000;
const size_t PageSize = 0x100;

const uint16_t StackPage = 0x0100;

const uint16_t NmiVecAddr = 0xFFFA;
const uint16_t ResetVecAddr = 0xFFFC;
const uint16_t IrqBrkVecAddr = 0xFFFE;

const unsigned CpuCycleWraparound = 341;

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

class CpuState
{
public:
	uint8_t a, x, y;
	uint8_t stack_ptr;
	uint16_t program_counter;
	StatusRegister status;

public:
	CpuState();
};

class CpuSnapshot : public CpuState 
{
public:
	std::vector<uint8_t> curr_instr;
	unsigned cycle;

public:
	CpuSnapshot();
	CpuSnapshot(uint16_t pc, std::vector<uint8_t> instr, uint8_t a, uint8_t x,
	            uint8_t y, uint8_t p, uint8_t sp, unsigned cyc);

public:
	auto operator==(const CpuSnapshot& other) -> bool;
	auto operator!=(const CpuSnapshot& other) -> bool;

	auto to_str() -> std::string;
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
	
private:
	Ppu* ppu;
	bool jumped;
	bool page_crossed;

public:
	unsigned cycle;
	unsigned cycle_stall;
	Memory memory;

public:
	Cpu();

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
	auto exec_instr(Instruction instr, ExtPtr ptr) -> void;
	auto get_op(uint8_t opcode) -> Op;

public:
	auto connect(Ppu* ppu) -> void
	{
		this->ppu = ppu;
	}

	auto step() -> void;
	auto reset() -> void;

	auto stall(unsigned cycles) -> void
	{
		cycle_stall = cycles;
	}

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

inline auto pages_differ(uint16_t addr1, uint16_t addr2) -> bool
{
	return (addr1 & 0xFF00) != (addr2 & 0xFF00);
}