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

Cpu_snapshot::Cpu_snapshot(
	uint16_t pc, std::vector<uint8_t> instr, uint8_t a,
	uint8_t x, uint8_t y, uint8_t p, uint8_t sp, unsigned cyc
	)
	: pc(pc) , instr(std::move(instr))
	, a(a) , x(x) , y(y)
	, p(p) , sp(sp) , cyc(cyc)
{}

bool Cpu_snapshot::operator==(const Cpu_snapshot& other) const
{
	return pc == other.pc 
		&& instr == other.instr
		&& a == other.a
		&& x == other.x
		&& y == other.y
		&& p == other.p
		&& sp == other.sp
		&& cyc == other.cyc;
}

bool Cpu_snapshot::operator!=(const Cpu_snapshot& other) const
{
	return !(*this == other);
}

std::string Cpu_snapshot::str() const
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

Cpu::Op::Op(Instruction instr, Mode mode, unsigned base_cycle, Penalty penalty)
	: instr(instr)
	, mode(mode)
	, base_cycle(base_cycle)
	, penalty(penalty)
{}

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
	memory.write(stack_page + stack_ptr, value);
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
	return memory.read(stack_page + stack_ptr);
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

void Cpu::do_int(Interrupt iterrupt)
{
	push_addr(program_counter);
	push(status.raw | BIT(4) | BIT(5));
	Extended_addr addr;
	switch (interrupt) {
	case Interrupt::irq:
		addr = irq_vec_addr;
		break;
	case Interrupt::nmi:
		addr = nmi_vec_addr;
		break;
	}
	program_counter = read_addr_from_mem(addr);
	status.break_ = 1;
	status.interrupt_disable = 1;
	cycle += 7;
	this->interrupt = Interrupt::none;
}

void Cpu::trigger(Interrupt interrupt)
{
	if (interrupt == Interrupt::nmi || !status.interrupt_disable) {
		this->interrupt = interrupt;
	}
}

void Cpu::stall(unsigned cycles)
{
	cycle_stall = cycles;
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
	status.zero = !val;
	status.negative = val & BIT(7) ? 1 : 0;
}

Extended_addr Cpu::get_addr(Mode mode)
{
	Extended_addr addr;
	page_crossed = false;

	switch (mode) {
	case Mode::implied:
		addr = 0;
		break;
	case Mode::accumulator:
		addr = a_register_ext_addr;
		break;
	case Mode::immediate:
		addr = program_counter + 1;
		break;
	case Mode::relative:
		addr = program_counter + (int8_t)peek_arg() + 2;
		page_crossed = pages_differ(program_counter + 2, addr);
		break;
	case Mode::zero_page:
		addr = peek_arg();
		break;
	case Mode::zero_page_x:
		addr = (peek_arg() + x) % page_size;
		break;
	case Mode::zero_page_y:
		addr = (peek_arg() + y) % page_size;
		break;
	case Mode::absolute:
		addr = peek_addr_arg();
		break;
	case Mode::absolute_x:
		addr = (peek_addr_arg() + x) % memory_size;
		page_crossed = pages_differ(peek_addr_arg(), addr);
		break;
	case Mode::absolute_y:
		addr = (peek_addr_arg() + y) % memory_size;
		page_crossed = pages_differ(peek_addr_arg(), addr);
		break;
	case Mode::indirect:
		addr = read_addr_from_mem(peek_addr_arg());
		break;
	case Mode::indirect_x:
		addr = as_addr(
			memory.read((peek_arg() + x + 1) % page_size),
			memory.read((peek_arg() + x) % page_size)
		);
		page_crossed = pages_differ(addr - x, addr);
		break;
	case Mode::indirect_y:
		addr = (as_addr(
			memory.read((peek_arg() + 1) % page_size),
			memory.read(peek_arg())
		) + y) % memory_size;
		page_crossed = pages_differ(addr - y, addr);
		break;
	}

	return addr;
}

unsigned Cpu::get_arg_size(Mode mode)
{
	switch (mode) {
	case Mode::implied:
	case Mode::accumulator:
		return 0;
	case Mode::immediate:
	case Mode::relative:
	case Mode::zero_page:
	case Mode::zero_page_x:
	case Mode::zero_page_y:
	case Mode::indirect_x:
	case Mode::indirect_y:
		return 1;
	case Mode::absolute:
	case Mode::absolute_x:
	case Mode::absolute_y:
	case Mode::indirect:
		return 2;
	}
}

void Cpu::exec_instr(Instruction instr, Extended_addr addr)
{
	jumped = false;

	switch (instr) {
	case Instruction::nop:
		break;
	case Instruction::inc:
		memory.write(addr, memory.read(addr) + 1);
		zn(memory.read(addr));
		break;
	case Instruction::inx:
		++x;
		zn(x);
		break;
	case Instruction::iny:
		++y;
		zn(y);
		break;
	case Instruction::dec:
		memory.write(addr, memory.read(addr) - 1);
		zn(memory.read(addr));
		break;
	case Instruction::dex:
		--x;
		zn(x);
		break;
	case Instruction::dey:
		--y;
		zn(y);
		break;
	case Instruction::clc:
		status.carry = 0;
		break;
	case Instruction::cld:
		status.decimal_mode = 0;
		break;
	case Instruction::cli:
		status.interrupt_disable = 0;
		break;
	case Instruction::clv:
		status.overflow = 0;
		break;
	case Instruction::sec:
		status.carry = 1;
		break;
	case Instruction::sed:
		status.decimal_mode = 1;
		break;
	case Instruction::sei:
		status.interrupt_disable = 1;
		break;
	case Instruction::tax:
		zn(x = a);
		break;
	case Instruction::tay:
		zn(y = a);
		break;
	case Instruction::txa:
		zn(a = x);
		break;
	case Instruction::tya:
		zn(a = y);
		break;
	case Instruction::txs:
		stack_ptr = x;
		break;
	case Instruction::tsx:
		zn(x = stack_ptr);
		break;
	case Instruction::php:
		push(status.raw | BIT(4) | BIT(5));
		break;
	case Instruction::pha:
		push(a);
		break;
	case Instruction::plp:
		status.raw = (pull() & ~BIT(4)) | BIT(5);
		break;
	case Instruction::pla:
		zn(a = pull());
		break;
	case Instruction::bcs:
		if (status.carry) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Instruction::bcc:
		if (!status.carry) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Instruction::beq:
		if (status.zero) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Instruction::bne:
		if (!status.zero) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Instruction::bmi:
		if (status.negative) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Instruction::bpl:
		if (!status.negative) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Instruction::bvs:
		if (status.overflow) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Instruction::bvc:
		if (!status.overflow) {
			program_counter = addr;
			jumped = true;
		}
		break;
	case Instruction::lda:
		zn(a = memory.read(addr));
		break;
	case Instruction::ldx:
		zn(x = memory.read(addr));
		break;
	case Instruction::ldy:
		zn(y = memory.read(addr));
		break;
	case Instruction::sta:
		memory.write(addr, a);
		break;
	case Instruction::stx:
		memory.write(addr, x);
		break;
	case Instruction::sty:
		memory.write(addr, y);
		break;
	case Instruction::bit:
	{
		auto m = memory.read(addr);
		status.zero = (a & m) == 0;
		status.overflow = (m >> 6) & 1;
		status.negative = (m >> 7) & 1;
		break;
	}
	case Instruction::cmp:
	{
		auto m = memory.read(addr);
		status.carry = a >= m;
		status.zero = a == m;
		status.negative = ((a - m) >> 7) & 1;
		break;
	}
	case Instruction::cpx:
	{
		auto m = memory.read(addr);
		status.carry = x >= m;
		status.zero = x == m;
		status.negative = ((x - m) >> 7) & 1;
		break;
	}
	case Instruction::cpy:
	{
		auto m = memory.read(addr);
		status.carry = y >= m;
		status.zero = y == m;
		status.negative = ((y - m) >> 7) & 1;
		break;
	}
	case Instruction::and_:
		zn(a &= memory.read(addr));
		break;
	case Instruction::ora:
		zn(a |= memory.read(addr));
		break;
	case Instruction::eor:
		zn(a ^= memory.read(addr));
		break;
	case Instruction::asl:
		status.carry = memory.read(addr) & BIT(7) ? 1 : 0;
		memory.write(addr, memory.read(addr) << 1);
		zn(memory.read(addr));
		break;
	case Instruction::lsr:
		status.carry = memory.read(addr) & BIT(0) ? 1 : 0;
		memory.write(addr, memory.read(addr) >> 1);
		zn(memory.read(addr));
		break;
	case Instruction::rol:
	{
		auto old_carry = status.carry;
		status.carry = memory.read(addr) & BIT(7) ? 1 : 0;
		memory.write(addr, memory.read(addr) << 1 | old_carry);
		zn(memory.read(addr));
		break;
	}
	case Instruction::ror:
	{
		auto old_carry = status.carry;
		status.carry = memory.read(addr) & BIT(0) ? 1 : 0;
		memory.write(addr, memory.read(addr) >> 1 | old_carry << 7);
		zn(memory.read(addr));
		break;
	}
	case Instruction::adc:
	{
		auto old_a = a;
		zn(a += memory.read(addr) + status.carry);
		status.carry = old_a + memory.read(addr) + status.carry > 0xFF;
		status.overflow = !((old_a ^ memory.read(addr)) & BIT(7)) && ((old_a ^ a) & BIT(7));
		break;
	}
	case Instruction::sbc:
	{
		auto old_a = a;
		zn(a -= memory.read(addr) + !status.carry);
		status.carry = old_a - memory.read(addr) - !status.carry >= 0x00;
		status.overflow = (old_a ^ memory.read(addr)) & BIT(7) && ((old_a ^ a) & BIT(7));
		break;
	}
	case Instruction::jmp:
		program_counter = addr;
		jumped = true;
		break;
	case Instruction::jsr:
		push_addr(program_counter + 2);
		program_counter = addr;
		jumped = true;
		break;
	case Instruction::rts:
		program_counter = pull_addr() + 1;
		jumped = true;
		break;
	case Instruction::brk:
		program_counter++;
		push_addr(program_counter);
		push(status.raw | BIT(4) | BIT(5));
		status.break_ = 1;
		program_counter = read_addr_from_mem(irq_vec_addr);
		jumped = true;
		break;
	case Instruction::rti:
		status.raw = (pull() & ~BIT(4)) | BIT(5);
		program_counter = pull_addr();
		jumped = true;
		break;
	}
}

Cpu::Op Cpu::get_op(uint8_t opcode)
{
	switch (opcode) {
	case 0x00: return Op{ Instruction::brk, Mode::implied, 7, Penalty::none };
	case 0x01: return Op{ Instruction::ora, Mode::indirect_x, 6, Penalty::none };
	case 0x04: return Op{ Instruction::nop, Mode::zero_page, 3, Penalty::none };
	case 0x05: return Op{ Instruction::ora, Mode::zero_page, 3, Penalty::none };
	case 0x06: return Op{ Instruction::asl, Mode::zero_page, 5, Penalty::none };
	case 0x08: return Op{ Instruction::php, Mode::implied, 3, Penalty::none };
	case 0x09: return Op{ Instruction::ora, Mode::immediate, 2, Penalty::none };
	case 0x0A: return Op{ Instruction::asl, Mode::accumulator, 2, Penalty::none };
	case 0x0C: return Op{ Instruction::nop, Mode::absolute, 4, Penalty::none };
	case 0x0D: return Op{ Instruction::ora, Mode::absolute, 4, Penalty::none };
	case 0x0E: return Op{ Instruction::asl, Mode::absolute, 6, Penalty::none };

	case 0x10: return Op{ Instruction::bpl, Mode::relative, 2, Penalty::branch };
	case 0x11: return Op{ Instruction::ora, Mode::indirect_y, 5, Penalty::page_cross };
	case 0x14: return Op{ Instruction::nop, Mode::zero_page_x, 4, Penalty::none };
	case 0x15: return Op{ Instruction::ora, Mode::zero_page_x, 4, Penalty::none };
	case 0x16: return Op{ Instruction::asl, Mode::zero_page_x, 6, Penalty::none };
	case 0x18: return Op{ Instruction::clc, Mode::implied, 2, Penalty::none };
	case 0x19: return Op{ Instruction::ora, Mode::absolute_y, 4, Penalty::page_cross };
	case 0x1A: return Op{ Instruction::nop, Mode::implied, 2, Penalty::none };
	case 0x1C: return Op{ Instruction::nop, Mode::absolute_x, 4, Penalty::page_cross };
	case 0x1D: return Op{ Instruction::ora, Mode::absolute_x, 4, Penalty::page_cross };
	case 0x1E: return Op{ Instruction::asl, Mode::absolute_x, 7, Penalty::none };

	case 0x20: return Op{ Instruction::jsr, Mode::absolute, 6, Penalty::none };
	case 0x21: return Op{ Instruction::and_, Mode::indirect_x, 6, Penalty::none };
	case 0x24: return Op{ Instruction::bit, Mode::zero_page, 3, Penalty::none };
	case 0x25: return Op{ Instruction::and_, Mode::zero_page, 3, Penalty::none };
	case 0x26: return Op{ Instruction::rol, Mode::zero_page, 5, Penalty::none };
	case 0x28: return Op{ Instruction::plp, Mode::implied, 4, Penalty::none };
	case 0x29: return Op{ Instruction::and_, Mode::immediate, 2, Penalty::none };
	case 0x2A: return Op{ Instruction::rol, Mode::accumulator, 2, Penalty::none };
	case 0x2C: return Op{ Instruction::bit, Mode::absolute, 4, Penalty::none };
	case 0x2D: return Op{ Instruction::and_, Mode::absolute, 4, Penalty::none };
	case 0x2E: return Op{ Instruction::rol, Mode::absolute, 6, Penalty::none };

	case 0x30: return Op{ Instruction::bmi, Mode::relative, 2, Penalty::branch };
	case 0x31: return Op{ Instruction::and_, Mode::indirect_y, 5, Penalty::page_cross };
	case 0x34: return Op{ Instruction::nop, Mode::zero_page_x, 4, Penalty::none };
	case 0x35: return Op{ Instruction::and_, Mode::zero_page_x, 4, Penalty::none };
	case 0x36: return Op{ Instruction::rol, Mode::zero_page_x, 6, Penalty::none };
	case 0x38: return Op{ Instruction::sec, Mode::implied, 2, Penalty::none };
	case 0x39: return Op{ Instruction::and_, Mode::absolute_y, 4, Penalty::page_cross };
	case 0x3A: return Op{ Instruction::nop, Mode::implied, 2, Penalty::none };
	case 0x3C: return Op{ Instruction::nop, Mode::absolute_x, 4, Penalty::page_cross };
	case 0x3D: return Op{ Instruction::and_, Mode::absolute_x, 4, Penalty::page_cross };
	case 0x3E: return Op{ Instruction::rol, Mode::absolute_x, 7, Penalty::none };

	case 0x40: return Op{ Instruction::rti, Mode::implied, 6, Penalty::none };
	case 0x41: return Op{ Instruction::eor, Mode::indirect_x, 6, Penalty::none };
	case 0x44: return Op{ Instruction::nop, Mode::zero_page, 3, Penalty::none };
	case 0x45: return Op{ Instruction::eor, Mode::zero_page, 3, Penalty::none };
	case 0x46: return Op{ Instruction::lsr, Mode::zero_page, 5, Penalty::none };
	case 0x48: return Op{ Instruction::pha, Mode::implied, 3, Penalty::none };
	case 0x49: return Op{ Instruction::eor, Mode::immediate, 2, Penalty::none };
	case 0x4A: return Op{ Instruction::lsr, Mode::accumulator, 2, Penalty::none };
	case 0x4C: return Op{ Instruction::jmp, Mode::absolute, 3, Penalty::none };
	case 0x4D: return Op{ Instruction::eor, Mode::absolute, 4, Penalty::none };
	case 0x4E: return Op{ Instruction::lsr, Mode::absolute, 6, Penalty::none };

	case 0x50: return Op{ Instruction::bvc, Mode::relative, 2, Penalty::branch };
	case 0x51: return Op{ Instruction::eor, Mode::indirect_y, 5, Penalty::page_cross };
	case 0x54: return Op{ Instruction::nop, Mode::zero_page_x, 4, Penalty::none };
	case 0x55: return Op{ Instruction::eor, Mode::zero_page_x, 4, Penalty::none };
	case 0x56: return Op{ Instruction::lsr, Mode::zero_page_x, 6, Penalty::none };
	case 0x58: return Op{ Instruction::cli, Mode::implied, 2, Penalty::none };
	case 0x59: return Op{ Instruction::eor, Mode::absolute_y, 4, Penalty::page_cross };
	case 0x5A: return Op{ Instruction::nop, Mode::implied, 2, Penalty::none };
	case 0x5C: return Op{ Instruction::nop, Mode::absolute_x, 4, Penalty::page_cross };
	case 0x5D: return Op{ Instruction::eor, Mode::absolute_x, 4, Penalty::page_cross };
	case 0x5E: return Op{ Instruction::lsr, Mode::absolute_x, 7, Penalty::none };

	case 0x60: return Op{ Instruction::rts, Mode::implied, 6, Penalty::none };
	case 0x61: return Op{ Instruction::adc, Mode::indirect_x, 6, Penalty::none };
	case 0x64: return Op{ Instruction::nop, Mode::zero_page, 3, Penalty::none };
	case 0x65: return Op{ Instruction::adc, Mode::zero_page, 3, Penalty::none };
	case 0x66: return Op{ Instruction::ror, Mode::zero_page, 5, Penalty::none };
	case 0x68: return Op{ Instruction::pla, Mode::implied, 4, Penalty::none };
	case 0x69: return Op{ Instruction::adc, Mode::immediate, 2, Penalty::none };
	case 0x6A: return Op{ Instruction::ror, Mode::accumulator, 2, Penalty::none };
	case 0x6C: return Op{ Instruction::jmp, Mode::indirect, 5, Penalty::none };
	case 0x6D: return Op{ Instruction::adc, Mode::absolute, 4, Penalty::none };
	case 0x6E: return Op{ Instruction::ror, Mode::absolute, 6, Penalty::none };

	case 0x70: return Op{ Instruction::bvs, Mode::relative, 2, Penalty::branch };
	case 0x71: return Op{ Instruction::adc, Mode::indirect_y, 5, Penalty::page_cross };
	case 0x74: return Op{ Instruction::nop, Mode::zero_page_x, 4, Penalty::none };
	case 0x75: return Op{ Instruction::adc, Mode::zero_page_x, 4, Penalty::none };
	case 0x76: return Op{ Instruction::ror, Mode::zero_page_x, 6, Penalty::none };
	case 0x78: return Op{ Instruction::sei, Mode::implied, 2, Penalty::none };
	case 0x79: return Op{ Instruction::adc, Mode::absolute_y, 4, Penalty::page_cross };
	case 0x7A: return Op{ Instruction::nop, Mode::implied, 2, Penalty::none };
	case 0x7C: return Op{ Instruction::nop, Mode::absolute_x, 4, Penalty::page_cross };
	case 0x7D: return Op{ Instruction::adc, Mode::absolute_x, 4, Penalty::page_cross };
	case 0x7E: return Op{ Instruction::ror, Mode::absolute_x, 7, Penalty::none };

	case 0x80: return Op{ Instruction::nop, Mode::immediate, 2, Penalty::none };
	case 0x81: return Op{ Instruction::sta, Mode::indirect_x, 6, Penalty::none };
	case 0x82: return Op{ Instruction::nop, Mode::immediate, 2, Penalty::none };
	case 0x84: return Op{ Instruction::sty, Mode::zero_page, 3, Penalty::none };
	case 0x85: return Op{ Instruction::sta, Mode::zero_page, 3, Penalty::none };
	case 0x86: return Op{ Instruction::stx, Mode::zero_page, 3, Penalty::none };
	case 0x88: return Op{ Instruction::dey, Mode::implied, 2, Penalty::none };
	case 0x89: return Op{ Instruction::nop, Mode::immediate, 2, Penalty::none };
	case 0x8A: return Op{ Instruction::txa, Mode::implied, 2, Penalty::none };
	case 0x8C: return Op{ Instruction::sty, Mode::absolute, 4, Penalty::none };
	case 0x8D: return Op{ Instruction::sta, Mode::absolute, 4, Penalty::none };
	case 0x8E: return Op{ Instruction::stx, Mode::absolute, 4, Penalty::none };

	case 0x90: return Op{ Instruction::bcc, Mode::relative, 2, Penalty::branch };
	case 0x91: return Op{ Instruction::sta, Mode::indirect_y, 6, Penalty::none };
	case 0x94: return Op{ Instruction::sty, Mode::zero_page_x, 4, Penalty::none };
	case 0x95: return Op{ Instruction::sta, Mode::zero_page_x, 4, Penalty::none };
	case 0x96: return Op{ Instruction::stx, Mode::zero_page_y, 4, Penalty::none };
	case 0x98: return Op{ Instruction::tya, Mode::implied, 2, Penalty::none };
	case 0x99: return Op{ Instruction::sta, Mode::absolute_y, 5, Penalty::none };
	case 0x9A: return Op{ Instruction::txs, Mode::implied, 2, Penalty::none };
	case 0x9D: return Op{ Instruction::sta, Mode::absolute_x, 5, Penalty::none };

	case 0xA0: return Op{ Instruction::ldy, Mode::immediate, 2, Penalty::none };
	case 0xA1: return Op{ Instruction::lda, Mode::indirect_x, 6, Penalty::none };
	case 0xA2: return Op{ Instruction::ldx, Mode::immediate, 2, Penalty::none };
	case 0xA4: return Op{ Instruction::ldy, Mode::zero_page, 3, Penalty::none };
	case 0xA5: return Op{ Instruction::lda, Mode::zero_page, 3, Penalty::none };
	case 0xA6: return Op{ Instruction::ldx, Mode::zero_page, 3, Penalty::none };
	case 0xA8: return Op{ Instruction::tay, Mode::implied, 2, Penalty::none };
	case 0xA9: return Op{ Instruction::lda, Mode::immediate, 2, Penalty::none };
	case 0xAA: return Op{ Instruction::tax, Mode::implied, 2, Penalty::none };
	case 0xAC: return Op{ Instruction::ldy, Mode::absolute, 4, Penalty::none };
	case 0xAD: return Op{ Instruction::lda, Mode::absolute, 4, Penalty::none };
	case 0xAE: return Op{ Instruction::ldx, Mode::absolute, 4, Penalty::none };

	case 0xB0: return Op{ Instruction::bcs, Mode::relative, 2, Penalty::branch };
	case 0xB1: return Op{ Instruction::lda, Mode::indirect_y, 5, Penalty::page_cross };
	case 0xB4: return Op{ Instruction::ldy, Mode::zero_page_x, 4, Penalty::none };
	case 0xB5: return Op{ Instruction::lda, Mode::zero_page_x, 4, Penalty::none };
	case 0xB6: return Op{ Instruction::ldx, Mode::zero_page_y, 4, Penalty::none };
	case 0xB8: return Op{ Instruction::clv, Mode::implied, 2, Penalty::none };
	case 0xB9: return Op{ Instruction::lda, Mode::absolute_y, 4, Penalty::page_cross };
	case 0xBA: return Op{ Instruction::tsx, Mode::implied, 2, Penalty::none };
	case 0xBC: return Op{ Instruction::ldy, Mode::absolute_x, 4, Penalty::page_cross };
	case 0xBD: return Op{ Instruction::lda, Mode::absolute_x, 4, Penalty::page_cross };
	case 0xBE: return Op{ Instruction::ldx, Mode::absolute_y, 4, Penalty::page_cross };

	case 0xC0: return Op{ Instruction::cpy, Mode::immediate, 2, Penalty::none };
	case 0xC1: return Op{ Instruction::cmp, Mode::indirect_x, 6, Penalty::none };
	case 0xC2: return Op{ Instruction::nop, Mode::immediate, 2, Penalty::none };
	case 0xC4: return Op{ Instruction::cpy, Mode::zero_page, 3, Penalty::none };
	case 0xC5: return Op{ Instruction::cmp, Mode::zero_page, 3, Penalty::none };
	case 0xC6: return Op{ Instruction::dec, Mode::zero_page, 5, Penalty::none };
	case 0xC8: return Op{ Instruction::iny, Mode::implied, 2, Penalty::none };
	case 0xC9: return Op{ Instruction::cmp, Mode::immediate, 2, Penalty::none };
	case 0xCA: return Op{ Instruction::dex, Mode::implied, 2, Penalty::none };
	case 0xCC: return Op{ Instruction::cpy, Mode::absolute, 4, Penalty::none };
	case 0xCD: return Op{ Instruction::cmp, Mode::absolute, 4, Penalty::none };
	case 0xCE: return Op{ Instruction::dec, Mode::absolute, 6, Penalty::none };

	case 0xD0: return Op{ Instruction::bne, Mode::relative, 2, Penalty::branch };
	case 0xD1: return Op{ Instruction::cmp, Mode::indirect_y, 5, Penalty::page_cross };
	case 0xD4: return Op{ Instruction::nop, Mode::zero_page_x, 4, Penalty::none };
	case 0xD5: return Op{ Instruction::cmp, Mode::zero_page_x, 4, Penalty::none };
	case 0xD6: return Op{ Instruction::dec, Mode::zero_page_x, 6, Penalty::none };
	case 0xD8: return Op{ Instruction::cld, Mode::implied, 2, Penalty::none };
	case 0xD9: return Op{ Instruction::cmp, Mode::absolute_y, 4, Penalty::page_cross };
	case 0xDA: return Op{ Instruction::nop, Mode::implied, 2, Penalty::none };
	case 0xDC: return Op{ Instruction::nop, Mode::absolute_x, 4, Penalty::page_cross };
	case 0xDD: return Op{ Instruction::cmp, Mode::absolute_x, 4, Penalty::page_cross };
	case 0xDE: return Op{ Instruction::dec, Mode::absolute_x, 7, Penalty::none };

	case 0xE0: return Op{ Instruction::cpx, Mode::immediate, 2, Penalty::none };
	case 0xE1: return Op{ Instruction::sbc, Mode::indirect_x, 6, Penalty::none };
	case 0xE2: return Op{ Instruction::nop, Mode::immediate, 2, Penalty::none };
	case 0xE4: return Op{ Instruction::cpx, Mode::zero_page, 3, Penalty::none };
	case 0xE5: return Op{ Instruction::sbc, Mode::zero_page, 3, Penalty::none };
	case 0xE6: return Op{ Instruction::inc, Mode::zero_page, 5, Penalty::none };
	case 0xE8: return Op{ Instruction::inx, Mode::implied, 2, Penalty::none };
	case 0xE9: return Op{ Instruction::sbc, Mode::immediate, 2, Penalty::none };
	case 0xEA: return Op{ Instruction::nop, Mode::implied, 2, Penalty::none };
	case 0xEC: return Op{ Instruction::cpx, Mode::absolute, 4, Penalty::none };
	case 0xED: return Op{ Instruction::sbc, Mode::absolute, 4, Penalty::none };
	case 0xEE: return Op{ Instruction::inc, Mode::absolute, 6, Penalty::none };

	case 0xF0: return Op{ Instruction::beq, Mode::relative, 2, Penalty::branch };
	case 0xF1: return Op{ Instruction::sbc, Mode::indirect_y, 5, Penalty::page_cross };
	case 0xF4: return Op{ Instruction::nop, Mode::zero_page_x, 4, Penalty::none };
	case 0xF5: return Op{ Instruction::sbc, Mode::zero_page_x, 4, Penalty::none };
	case 0xF6: return Op{ Instruction::inc, Mode::zero_page_x, 6, Penalty::none };
	case 0xF8: return Op{ Instruction::sed, Mode::implied, 2, Penalty::none };
	case 0xF9: return Op{ Instruction::sbc, Mode::absolute_y, 4, Penalty::page_cross };
	case 0xFA: return Op{ Instruction::nop, Mode::implied, 2, Penalty::none };
	case 0xFC: return Op{ Instruction::nop, Mode::absolute_x, 4, Penalty::page_cross };
	case 0xFD: return Op{ Instruction::sbc, Mode::absolute_x, 4, Penalty::page_cross };
	case 0xFE: return Op{ Instruction::inc, Mode::absolute_x, 7, Penalty::none };

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
		cycle = (cycle + 1) % cpu_cycle_wraparound; // TODO: 1 or 3?
		return 1;
	}

	auto start_cycle = cycle;

	switch (interrupt) {
	case Interrupt::irq:
		logger << "irq\n";
		do_int(Interrupt::irq);
		break;
	case Interrupt::nmi:
		logger << "nmi\n";
		do_int(Interrupt::nmi);
		break;
	case Interrupt::none:
		break;
	}

	unsigned penalty_sum = 0;

	auto op = get_op(memory.read(program_counter));

	exec_instr(op.instr, get_addr(op.mode));

	switch (op.penalty) {
	case Penalty::branch:
		if (!jumped) { break; }
		penalty_sum++;
	case Penalty::page_cross:
		if (page_crossed) {
			penalty_sum++;
		}
	case Penalty::none:
		break;
	}

	if (!jumped) {
		program_counter += get_arg_size(op.mode) + 1;
	}

	cycle = (cycle + (op.base_cycle + penalty_sum) * 3) % cpu_cycle_wraparound;

	auto cycle_diff = (int)cycle - (int)start_cycle;
	if (cycle_diff < 0) {
		cycle_diff += cpu_cycle_wraparound;
	}

	return cycle_diff;
}

void Cpu::reset()
{
	program_counter = read_addr_from_mem(reset_vec_addr);
	logger << "pc=" << std::hex << program_counter << "\n";
}

Cpu_snapshot Cpu::take_snapshot()
{
	auto opcode = memory.read(program_counter);
	std::vector<uint8_t> instr{ opcode };
	auto arg_size = get_arg_size(get_op(opcode).mode);
	for (unsigned i = 0; i < arg_size; i++) {
		instr.push_back(memory.read(program_counter + 1 + i));
	}
	return Cpu_snapshot{ program_counter, instr, a, x, y, status.raw, stack_ptr, cycle };
}
