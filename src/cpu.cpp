#include <iostream>
#include <iomanip>

#include "cpu.h"

static uint8_t hi_byte(uint16_t addr)
{
	return addr >> 8;
}

static uint8_t lo_byte(uint16_t addr)
{
	return (uint8_t)addr;
}

static uint16_t as_addr(uint8_t hi, uint8_t lo)
{
	return (hi << 8) | lo;
}

static bool pages_differ(uint16_t addr1, uint16_t addr2)
{
	return (addr1 & 0xFF00) != (addr2 & 0xFF00);
}

CpuSnapshot::CpuSnapshot(uint16_t pc, std::vector<uint8_t> instr, uint8_t a, 
                         uint8_t x, uint8_t y, uint8_t p, uint8_t sp, unsigned cyc)
	: pc(pc)
	, instr(instr)
	, a(a)
	, x(x)
	, y(y)
	, p(p)
	, sp(sp)
	, cyc(cyc)
{}

std::string CpuSnapshot::str() const
{
	std::ostringstream strm;
	strm
		<< std::uppercase
		<< std::hex
		<< std::setfill('0')
		<< std::setw(4)
		<< pc << "  ";

	for (auto instr : instr) {
		strm << std::setw(2) << (int)instr << ' ';
	}

	strm << ' ';

	for (auto i = 3 - instr.size(); i > 0; --i) {
		strm << "   ";
	}

	// TODO: deassemble instruction

	strm 
		<< "A:" << std::setw(2) << (int)a << ' '
		<< "X:" << std::setw(2) << (int)x << ' '
		<< "Y:" << std::setw(2) << (int)y << ' '
		<< "P:" << std::setw(2) << (int)p << ' '
		<< "SP:" << std::setw(2) << (int)sp << ' '
		<< "CYC:" << std::setfill(' ') << std::setw(3) << std::dec << cyc;

	return strm.str();
}

Cpu::Cpu()
{
	a = x = y = 0;
	status.raw = 0;
	program_counter = 0;
	stack_ptr = 0xFF;
	cycle = 0;
	cycle_stall = 0;
}

void Cpu::push(uint8_t value)
{
	memory.write(StackPage + stack_ptr, value);
	stack_ptr--;
}

void Cpu::push_addr(uint16_t addr)
{
	push(hi_byte(addr));
	push(lo_byte(addr));
}

uint8_t Cpu::pull()
{
	stack_ptr++;
	return memory.read(StackPage + stack_ptr);
}

uint16_t Cpu::pull_addr()
{
	auto lo = pull();
	auto hi = pull();
	return as_addr(hi, lo);
}

uint16_t Cpu::read_addr_from_mem(uint16_t addr)
{
	uint16_t page = 0xFF00 & addr;
	return as_addr(
		memory.read(((addr + 1) & 0x00FF) | page),
		memory.read(addr)
	);
}

uint8_t Cpu::peek_arg()
{
	return memory.read(program_counter + 1);
}

uint16_t Cpu::peek_addr_arg()
{
	return as_addr(
		memory.read(program_counter + 2),
		memory.read(program_counter + 1)
	);
}

void Cpu::zn(uint8_t val)
{
	status.bits.zero = !val;
	status.bits.negative = val & bit(7) ? 1 : 0;
}

ExtendedAddr Cpu::get_addr(Mode mode)
{
	ExtendedAddr addr;
	page_crossed = false;

	switch (mode) {
	case Implied:
		addr = 0;
		break;
	case Accumulator:
		addr = ARegisterExtAddr;
		break;
	case Immediate:
		addr = program_counter + 1;
		break;
	case Relative:
		addr = program_counter + (int8_t)peek_arg() + 2;
		page_crossed = pages_differ(program_counter + 2, addr); 
		break;
	case ZeroPage:
		addr = peek_arg();
		break;
	case ZeroPageX:
		addr = (peek_arg() + x) % PageSize;
		break;
	case ZeroPageY:
		addr = (peek_arg() + y) % PageSize;
		break;
	case Absolute:
		addr = peek_addr_arg();
		break;
	case AbsoluteX:
		addr = (peek_addr_arg() + x) % MemorySize;
		page_crossed = pages_differ(peek_addr_arg(), addr);
		break;
	case AbsoluteY:
		addr = (peek_addr_arg() + y) % MemorySize;
		page_crossed = pages_differ(peek_addr_arg(), addr);
		break;
	case Indirect:
		addr = read_addr_from_mem(peek_addr_arg());
		break;
	case IndirectX:
		addr = as_addr(
			memory.read((peek_arg() + x + 1) % PageSize),
			memory.read((peek_arg() + x) % PageSize)
		);
		page_crossed = pages_differ(addr - x, addr);
		break;
	case IndirectY:
		addr = (as_addr(
			memory.read((peek_arg() + 1) % PageSize),
			memory.read(peek_arg())
		) + y) % MemorySize;
		page_crossed = pages_differ(addr - y, addr);
		break;
	}

	return addr;
}

unsigned Cpu::get_arg_size(Mode mode)
{
	switch (mode) {
	case Implied: 
	case Accumulator:
		return 0;
	case Immediate:
	case Relative:
	case ZeroPage:
	case ZeroPageX:
	case ZeroPageY:
	case IndirectX:
	case IndirectY:
		return 1;
	case Absolute:
	case AbsoluteX:
	case AbsoluteY:
	case Indirect:
		return 2;
	}
}

void Cpu::exec_instr(Instruction instr, ExtendedAddr addr)
{
	jumped = false;

	switch (instr) {
	case Nop:
		break;
	case Inc:
		memory.write(addr, memory.read(addr) + 1);
		zn(memory.read(addr));
		break;
	case Inx:
		++x;
		zn(x);
		break;
	case Iny:
		++y;
		zn(y);
		break;
	case Dec:
		memory.write(addr, memory.read(addr) - 1);
		zn(memory.read(addr));
		break;
	case Dex:
		--x;
		zn(x);
		break;
	case Dey:
		--y;
		zn(y);
		break;
	case Clc:
		status.bits.carry = 0;
		break;
	case Cld:
		status.bits.decimal_mode = 0;
		break;
	case Cli:
		status.bits.interrupt_disable = 0;
		break;
	case Clv:
		status.bits.overflow = 0;
		break;
	case Sec:
		status.bits.carry = 1;
		break;
	case Sed:
		status.bits.decimal_mode = 1;
		break;
	case Sei:
		status.bits.interrupt_disable = 1;
		break;
	case Tax:
		zn(x = a);
		break;
	case Tay:
		zn(y = a);
		break;
	case Txa:
		zn(a = x);
		break;
	case Tya:
		zn(a = y);
		break;
	case Txs:
		stack_ptr = x;
		break;
	case Tsx:
		zn(x = stack_ptr);
		break;
	case Php:
		push(status.raw | bit(4) | bit(5));
		break;
	case Pha:
		push(a);
		break;
	case Plp:
		status.raw = pull() & 0xEF | 0x20;
		break;
	case Pla:
		zn(a = pull());
		break;
	case Bcs:
		if (status.bits.carry) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Bcc:
		if (!status.bits.carry) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Beq:
		if (status.bits.zero) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Bne:
		if (!status.bits.zero) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Bmi:
		if (status.bits.negative) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Bpl:
		if (!status.bits.negative) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Bvs:
		if (status.bits.overflow) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Bvc:
		if (!status.bits.overflow) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Lda:
		zn(a = memory.read(addr));
		break;
	case Ldx:
		zn(x = memory.read(addr));
		break;
	case Ldy:
		zn(y = memory.read(addr));
		break;
	case Sta:
		memory.write(addr, a);
		break;
	case Stx:
		memory.write(addr, x);
		break;
	case Sty:
		memory.write(addr, y);
		break;
	case Bit: 
	{
		auto m = memory.read(addr);
		status.bits.zero = (a & m) == 0;
		status.bits.overflow = (m >> 6) & 1;
		status.bits.negative = (m >> 7) & 1;
		break;
	}
	case Cmp:
	{
		auto m = memory.read(addr);
		status.bits.carry = a >= m;
		status.bits.zero = a == m;
		status.bits.negative = ((a - m) >> 7) & 1;
		break;
	}
	case Cpx:
	{
		auto m = memory.read(addr);
		status.bits.carry = x >= m;
		status.bits.zero = x == m;
		status.bits.negative = ((x - m) >> 7) & 1;
		break;
	}
	case Cpy: 
	{
		auto m = memory.read(addr);
		status.bits.carry = y >= m;
		status.bits.zero = y == m;
		status.bits.negative = ((y - m) >> 7) & 1;
		break;
	}
	case And:
		zn(a &= memory.read(addr));
		break;
	case Ora:
		zn(a |= memory.read(addr));
		break;
	case Eor:
		zn(a ^= memory.read(addr));
		break;
	case Asl:
		status.bits.carry = memory.read(addr) & bit(7) ? 1 : 0;
		memory.write(addr, memory.read(addr) << 1);
		zn(memory.read(addr));
		break;
	case Lsr:
		status.bits.carry = memory.read(addr) & bit(0) ? 1 : 0;
		memory.write(addr, memory.read(addr) >> 1);
		zn(memory.read(addr));
		break;
	case Rol:
	{
		auto old_carry = status.bits.carry;
		status.bits.carry = memory.read(addr) & bit(7) ? 1 : 0;
		memory.write(addr, memory.read(addr) << 1 | old_carry);
		zn(memory.read(addr));
		break;
	}
	case Ror:
	{
		auto old_carry = status.bits.carry;
		status.bits.carry = memory.read(addr) & bit(0) ? 1 : 0;
		memory.write(addr, memory.read(addr) >> 1 | old_carry << 7);
		zn(memory.read(addr));
		break;
	}
	case Adc:
	{
		auto old_a = a;
		zn(a += memory.read(addr) + status.bits.carry);
		status.bits.carry = old_a + memory.read(addr) + status.bits.carry > 0xFF;
		status.bits.overflow = !((old_a ^ memory.read(addr)) & bit(7)) && ((old_a ^ a) & bit(7));
		break;
	}
	case Sbc:
	{
		auto old_a = a;
		zn(a -= memory.read(addr) + !status.bits.carry);
		status.bits.carry = old_a - memory.read(addr) - !status.bits.carry >= 0x00;
		status.bits.overflow = (old_a ^ memory.read(addr)) & bit(7) && ((old_a ^ a) & bit(7));
		break;
	}
	case Jmp:
		program_counter = addr;
		jumped = true;
		break;
	case Jsr:
		push_addr(program_counter + 2);
		program_counter = addr;
		jumped = true;
		break;
	case Rts:
		program_counter = pull_addr() + 1;
		jumped = true;
		break;
	case Brk:
		program_counter++;
		push_addr(program_counter);
		push(status.raw | bit(4) | bit(5));
		status.bits.break_ = 1;
		program_counter = read_addr_from_mem(IrqBrkVecAddr);
		jumped = true;
		break;
	case Rti:
		status.raw = pull() & 0xEF | 0x20;
		program_counter = pull_addr();
		jumped = true;
		break;
	}
}

Cpu::Op Cpu::get_op(uint8_t opcode)
{
	switch (opcode) {
	case 0x00: return Op{ Brk, Implied, 7, Penalty::None };
	case 0x01: return Op{ Ora, IndirectX, 6, Penalty::None };
	case 0x05: return Op{ Ora, ZeroPage, 3, Penalty::None };
	case 0x06: return Op{ Asl, ZeroPage, 5, Penalty::None };
	case 0x08: return Op{ Php, Implied, 3, Penalty::None };
	case 0x09: return Op{ Ora, Immediate, 2, Penalty::None };
	case 0x0A: return Op{ Asl, Accumulator, 2, Penalty::None };
	case 0x0D: return Op{ Ora, Absolute, 4, Penalty::None };
	case 0x0E: return Op{ Asl, Absolute, 6, Penalty::None };

	case 0x10: return Op{ Bpl, Relative, 2, Penalty::Branch };
	case 0x11: return Op{ Ora, IndirectY, 5, Penalty::PageCross };
	case 0x15: return Op{ Ora, ZeroPageX, 4, Penalty::None };
	case 0x16: return Op{ Asl, ZeroPageX, 6, Penalty::None };
	case 0x18: return Op{ Clc, Implied, 2, Penalty::None };
	case 0x19: return Op{ Ora, AbsoluteY, 4, Penalty::PageCross };
	case 0x1D: return Op{ Ora, AbsoluteX, 4, Penalty::PageCross };
	case 0x1E: return Op{ Asl, AbsoluteX, 7, Penalty::None };

	case 0x20: return Op{ Jsr, Absolute, 6, Penalty::None };
	case 0x21: return Op{ And, IndirectX, 6, Penalty::None };
	case 0x24: return Op{ Bit, ZeroPage, 3, Penalty::None };
	case 0x25: return Op{ And, ZeroPage, 3, Penalty::None };
	case 0x26: return Op{ Rol, ZeroPage, 5, Penalty::None };
	case 0x28: return Op{ Plp, Implied, 4, Penalty::None };
	case 0x29: return Op{ And, Immediate, 2, Penalty::None };
	case 0x2A: return Op{ Rol, Accumulator, 2, Penalty::None };
	case 0x2C: return Op{ Bit, Absolute, 4, Penalty::None };
	case 0x2D: return Op{ And, Absolute, 4, Penalty::None };
	case 0x2E: return Op{ Rol, Absolute, 6, Penalty::None };

	case 0x30: return Op{ Bmi, Relative, 2, Penalty::Branch };
	case 0x31: return Op{ And, IndirectY, 5, Penalty::PageCross };
	case 0x35: return Op{ And, ZeroPageX, 4, Penalty::None };
	case 0x36: return Op{ Rol, ZeroPageX, 6, Penalty::None };
	case 0x38: return Op{ Sec, Implied, 2, Penalty::None };
	case 0x39: return Op{ And, AbsoluteY, 4, Penalty::PageCross };
	case 0x3D: return Op{ And, AbsoluteX, 4, Penalty::PageCross };
	case 0x3E: return Op{ Rol, AbsoluteX, 7, Penalty::None };

	case 0x40: return Op{ Rti, Implied, 6, Penalty::None };
	case 0x41: return Op{ Eor, IndirectX, 6, Penalty::None };
	case 0x45: return Op{ Eor, ZeroPage, 3, Penalty::None };
	case 0x46: return Op{ Lsr, ZeroPage, 5, Penalty::None };
	case 0x48: return Op{ Pha, Implied, 3, Penalty::None };
	case 0x49: return Op{ Eor, Immediate, 2, Penalty::None };
	case 0x4A: return Op{ Lsr, Accumulator, 2, Penalty::None };
	case 0x4C: return Op{ Jmp, Absolute, 3, Penalty::None };
	case 0x4D: return Op{ Eor, Absolute, 4, Penalty::None };
	case 0x4E: return Op{ Lsr, Absolute, 6, Penalty::None };

	case 0x50: return Op{ Bvc, Relative, 2, Penalty::Branch };
	case 0x51: return Op{ Eor, IndirectY, 5, Penalty::PageCross };
	case 0x55: return Op{ Eor, ZeroPageX, 4, Penalty::None };
	case 0x56: return Op{ Lsr, ZeroPageX, 6, Penalty::None };
	case 0x58: return Op{ Cli, Implied, 2, Penalty::None };
	case 0x59: return Op{ Eor, AbsoluteY, 4, Penalty::PageCross };
	case 0x5D: return Op{ Eor, AbsoluteX, 4, Penalty::PageCross };
	case 0x5E: return Op{ Lsr, AbsoluteX, 7, Penalty::None };

	case 0x60: return Op{ Rts, Implied, 6, Penalty::None };
	case 0x61: return Op{ Adc, IndirectX, 6, Penalty::None };
	case 0x65: return Op{ Adc, ZeroPage, 3, Penalty::None };
	case 0x66: return Op{ Ror, ZeroPage, 5, Penalty::None };
	case 0x68: return Op{ Pla, Implied, 4, Penalty::None };
	case 0x69: return Op{ Adc, Immediate, 2, Penalty::None };
	case 0x6A: return Op{ Ror, Accumulator, 2, Penalty::None };
	case 0x6C: return Op{ Jmp, Indirect, 5, Penalty::None };
	case 0x6D: return Op{ Adc, Absolute, 4, Penalty::None };
	case 0x6E: return Op{ Ror, Absolute, 6, Penalty::None };

	case 0x70: return Op{ Bvs, Relative, 2, Penalty::Branch };
	case 0x71: return Op{ Adc, IndirectY, 5, Penalty::PageCross };
	case 0x75: return Op{ Adc, ZeroPageX, 4, Penalty::None };
	case 0x76: return Op{ Ror, ZeroPageX, 6, Penalty::None };
	case 0x78: return Op{ Sei, Implied, 2, Penalty::None };
	case 0x79: return Op{ Adc, AbsoluteY, 4, Penalty::PageCross };
	case 0x7D: return Op{ Adc, AbsoluteX, 4, Penalty::PageCross };
	case 0x7E: return Op{ Ror, AbsoluteX, 7, Penalty::None };

	case 0x81: return Op{ Sta, IndirectX, 6, Penalty::None };
	case 0x84: return Op{ Sty, ZeroPage, 3, Penalty::None };
	case 0x85: return Op{ Sta, ZeroPage, 3, Penalty::None };
	case 0x86: return Op{ Stx, ZeroPage, 3, Penalty::None };
	case 0x88: return Op{ Dey, Implied, 2, Penalty::None };
	case 0x8A: return Op{ Txa, Implied, 2, Penalty::None };
	case 0x8C: return Op{ Sty, Absolute, 4, Penalty::None };
	case 0x8D: return Op{ Sta, Absolute, 4, Penalty::None };
	case 0x8E: return Op{ Stx, Absolute, 4, Penalty::None };

	case 0x90: return Op{ Bcc, Relative, 2, Penalty::Branch };
	case 0x91: return Op{ Sta, IndirectY, 6, Penalty::None };
	case 0x94: return Op{ Sty, ZeroPageX, 4, Penalty::None };
	case 0x95: return Op{ Sta, ZeroPageX, 4, Penalty::None };
	case 0x96: return Op{ Stx, ZeroPageY, 4, Penalty::None };
	case 0x98: return Op{ Tya, Implied, 2, Penalty::None };
	case 0x99: return Op{ Sta, AbsoluteY, 5, Penalty::None };
	case 0x9A: return Op{ Txs, Implied, 2, Penalty::None };
	case 0x9D: return Op{ Sta, AbsoluteX, 5, Penalty::None };

	case 0xA0: return Op{ Ldy, Immediate, 2, Penalty::None };
	case 0xA1: return Op{ Lda, IndirectX, 6, Penalty::None };
	case 0xA2: return Op{ Ldx, Immediate, 2, Penalty::None };
	case 0xA4: return Op{ Ldy, ZeroPage, 3, Penalty::None };
	case 0xA5: return Op{ Lda, ZeroPage, 3, Penalty::None };
	case 0xA6: return Op{ Ldx, ZeroPage, 3, Penalty::None };
	case 0xA8: return Op{ Tay, Implied, 2, Penalty::None };
	case 0xA9: return Op{ Lda, Immediate, 2, Penalty::None };
	case 0xAA: return Op{ Tax, Implied, 2, Penalty::None };
	case 0xAC: return Op{ Ldy, Absolute, 4, Penalty::None };
	case 0xAD: return Op{ Lda, Absolute, 4, Penalty::None };
	case 0xAE: return Op{ Ldx, Absolute, 4, Penalty::None };

	case 0xB0: return Op{ Bcs, Relative, 2, Penalty::Branch };
	case 0xB1: return Op{ Lda, IndirectY, 5, Penalty::PageCross };
	case 0xB4: return Op{ Ldy, ZeroPageX, 4, Penalty::None };
	case 0xB5: return Op{ Lda, ZeroPageX, 4, Penalty::None };
	case 0xB6: return Op{ Ldx, ZeroPageY, 4, Penalty::None };
	case 0xB8: return Op{ Clv, Implied, 2, Penalty::None };
	case 0xB9: return Op{ Lda, AbsoluteY, 4, Penalty::PageCross };
	case 0xBA: return Op{ Tsx, Implied, 2, Penalty::None };
	case 0xBC: return Op{ Ldy, AbsoluteX, 4, Penalty::PageCross };
	case 0xBD: return Op{ Lda, AbsoluteX, 4, Penalty::PageCross };
	case 0xBE: return Op{ Ldx, AbsoluteY, 4, Penalty::PageCross };

	case 0xC0: return Op{ Cpy, Immediate, 2, Penalty::None };
	case 0xC1: return Op{ Cmp, IndirectX, 6, Penalty::None };
	case 0xC4: return Op{ Cpy, ZeroPage, 3, Penalty::None };
	case 0xC5: return Op{ Cmp, ZeroPage, 3, Penalty::None };
	case 0xC6: return Op{ Dec, ZeroPage, 5, Penalty::None };
	case 0xC8: return Op{ Iny, Implied, 2, Penalty::None };
	case 0xC9: return Op{ Cmp, Immediate, 2, Penalty::None };
	case 0xCA: return Op{ Dex, Implied, 2, Penalty::None };
	case 0xCC: return Op{ Cpy, Absolute, 4, Penalty::None };
	case 0xCD: return Op{ Cmp, Absolute, 4, Penalty::None };
	case 0xCE: return Op{ Dec, Absolute, 6, Penalty::None };

	case 0xD0: return Op{ Bne, Relative, 2, Penalty::Branch };
	case 0xD1: return Op{ Cmp, IndirectY, 5, Penalty::PageCross };
	case 0xD5: return Op{ Cmp, ZeroPageX, 4, Penalty::None };
	case 0xD6: return Op{ Dec, ZeroPageX, 6, Penalty::None };
	case 0xD8: return Op{ Cld, Implied, 2, Penalty::None };
	case 0xD9: return Op{ Cmp, AbsoluteY, 4, Penalty::PageCross };
	case 0xDD: return Op{ Cmp, AbsoluteX, 4, Penalty::PageCross };
	case 0xDE: return Op{ Dec, AbsoluteX, 7, Penalty::None };

	case 0xE0: return Op{ Cpx, Immediate, 2, Penalty::None };
	case 0xE1: return Op{ Sbc, IndirectX, 6, Penalty::None };
	case 0xE4: return Op{ Cpx, ZeroPage, 3, Penalty::None };
	case 0xE5: return Op{ Sbc, ZeroPage, 3, Penalty::None };
	case 0xE6: return Op{ Inc, ZeroPage, 5, Penalty::None };
	case 0xE8: return Op{ Inx, Implied, 2, Penalty::None };
	case 0xE9: return Op{ Sbc, Immediate, 2, Penalty::None };
	case 0xEA: return Op{ Nop, Implied, 2, Penalty::None };
	case 0xEC: return Op{ Cpx, Absolute, 4, Penalty::None };
	case 0xED: return Op{ Sbc, Absolute, 4, Penalty::None };
	case 0xEE: return Op{ Inc, Absolute, 6, Penalty::None };

	case 0xF0: return Op{ Beq, Relative, 2, Penalty::Branch };
	case 0xF1: return Op{ Sbc, IndirectY, 5, Penalty::PageCross };
	case 0xF5: return Op{ Sbc, ZeroPageX, 4, Penalty::None };
	case 0xF6: return Op{ Inc, ZeroPageX, 6, Penalty::None };
	case 0xF8: return Op{ Sed, Implied, 2, Penalty::None };
	case 0xF9: return Op{ Sbc, AbsoluteY, 4, Penalty::PageCross };
	case 0xFD: return Op{ Sbc, AbsoluteX, 4, Penalty::PageCross };
	case 0xFE: return Op{ Inc, AbsoluteX, 7, Penalty::None };

	default: 
	{
		std::ostringstream ss;
		ss << "invalid opcode " << std::hex << (int)opcode;
		throw ss.str();
	}
	}
}

unsigned Cpu::step()
{
	if (cycle_stall) {
		cycle_stall--;
		cycle = (cycle + 1) % CpuCycleWraparound; // TODO: 1 or 3?
		return 1;
	}

	auto start_cycle = cycle;

	switch (interrupt) {
	case Interrupt::Irq:
		std::cerr << "irq\n";
		do_int<Interrupt::Irq>();
		break;
	case Interrupt::Nmi:
		std::cerr << "nmi\n";
		do_int<Interrupt::Nmi>();
		break;
	}

	unsigned penalty_sum = 0;

	// auto [instr, mode, cycles] = get_op(memory.read(program_counter));
	// auto [base_cycle, penalty] = cycles;
	auto op = get_op(memory.read(program_counter));

	auto old_pc = program_counter;

	exec_instr(op.instr, get_addr(op.mode));

	switch (op.penalty) {
	case Penalty::Branch:
		if (!jumped) { break; }
		penalty_sum++;
	case Penalty::PageCross:
		if (page_crossed) {
			penalty_sum++;
		}
	case Penalty::None:
		break;
	}

	if (!jumped) {
		program_counter += get_arg_size(op.mode) + 1;
	}

	cycle = (cycle + (op.base_cycle + penalty_sum) * 3) % CpuCycleWraparound;

	auto cycle_diff = (int)cycle - (int)start_cycle;
	if (cycle_diff < 0) {
		cycle_diff += CpuCycleWraparound;
	}

	return cycle_diff;
}

void Cpu::reset()
{
	program_counter = read_addr_from_mem(ResetVecAddr);
	std::clog << "pc=" << std::hex << program_counter << "\n";
}

CpuSnapshot Cpu::take_snapshot()
{
	auto opcode = memory.read(program_counter);
	std::vector<uint8_t> instr{ opcode };
	auto arg_size = get_arg_size(get_op(opcode).mode);
	for (unsigned i = 0; i < arg_size; i++) {
		instr.push_back(memory.read(program_counter + 1 + i));
	}
	return CpuSnapshot{ program_counter, instr, a, x, y, status.raw, stack_ptr, cycle };
}
