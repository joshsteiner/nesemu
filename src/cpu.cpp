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

Op::Op()
	: valid(false)
	, instr(Instruction::nop)
	, mode(Mode::implied)
	, base_cycle(0)
	, penalty(Penalty::none)
{}

Op::Op(Instruction instr, Mode mode, unsigned base_cycle, Penalty penalty)
	: valid(true)
	, instr(instr)
	, mode(mode)
	, base_cycle(base_cycle)
	, penalty(penalty)
{}


bool Op::operator==(const Op& other) const
{
	return valid == other.valid 
		&& instr == other.instr
		&& mode == other.mode
		&& base_cycle == other.base_cycle
		&& penalty == other.penalty;
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

void Cpu::do_int()
{
	if (interrupt == Interrupt::none) {
		return;
	}

	push_addr(program_counter);
	push(status.raw | BIT(4) | BIT(5));

	switch (interrupt) {
	case Interrupt::irq:
		program_counter = read_addr_from_mem(irq_vec_addr);
		LOG("IRQ addr accessed");
		break;
	case Interrupt::nmi:
		program_counter = read_addr_from_mem(nmi_vec_addr);
		LOG("NMI addr accessed");
		break;
	}

	status.break_ = 1;
	status.interrupt_disable = 1;
	cycle = (cycle + 7) % cpu_cycle_wraparound; 
	this->interrupt = Interrupt::none;
}

void Cpu::trigger(Interrupt interrupt)
{
	LOG("interrupt triggered");
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

static const std::array<Op, 0x100> ops = {
	Op{ Instruction::brk, Mode::implied, 7, Penalty::none },
	Op{ Instruction::ora, Mode::indirect_x, 6, Penalty::none },
	invalid_op,
	invalid_op,
	Op{ Instruction::nop, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::ora, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::asl, Mode::zero_page, 5, Penalty::none },
	invalid_op,
	Op{ Instruction::php, Mode::implied, 3, Penalty::none },
	Op{ Instruction::ora, Mode::immediate, 2, Penalty::none },
	Op{ Instruction::asl, Mode::accumulator, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::nop, Mode::absolute, 4, Penalty::none },
	Op{ Instruction::ora, Mode::absolute, 4, Penalty::none },
	Op{ Instruction::asl, Mode::absolute, 6, Penalty::none },
	invalid_op,

	Op{ Instruction::bpl, Mode::relative, 2, Penalty::branch },
	Op{ Instruction::ora, Mode::indirect_y, 5, Penalty::page_cross },
	invalid_op,
	invalid_op,
	Op{ Instruction::nop, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::ora, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::asl, Mode::zero_page_x, 6, Penalty::none },
	invalid_op,
	Op{ Instruction::clc, Mode::implied, 2, Penalty::none },
	Op{ Instruction::ora, Mode::absolute_y, 4, Penalty::page_cross },
	Op{ Instruction::nop, Mode::implied, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::nop, Mode::absolute_x, 4, Penalty::page_cross },
	Op{ Instruction::ora, Mode::absolute_x, 4, Penalty::page_cross },
	Op{ Instruction::asl, Mode::absolute_x, 7, Penalty::none },
	invalid_op,

	Op{ Instruction::jsr, Mode::absolute, 6, Penalty::none },
	Op{ Instruction::and_, Mode::indirect_x, 6, Penalty::none },
	invalid_op,
	invalid_op,
	Op{ Instruction::bit, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::and_, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::rol, Mode::zero_page, 5, Penalty::none },
	invalid_op,
	Op{ Instruction::plp, Mode::implied, 4, Penalty::none },
	Op{ Instruction::and_, Mode::immediate, 2, Penalty::none },
	Op{ Instruction::rol, Mode::accumulator, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::bit, Mode::absolute, 4, Penalty::none },
	Op{ Instruction::and_, Mode::absolute, 4, Penalty::none },
	Op{ Instruction::rol, Mode::absolute, 6, Penalty::none },
	invalid_op,

	Op{ Instruction::bmi, Mode::relative, 2, Penalty::branch },
	Op{ Instruction::and_, Mode::indirect_y, 5, Penalty::page_cross },
	invalid_op,
	invalid_op,
	Op{ Instruction::nop, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::and_, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::rol, Mode::zero_page_x, 6, Penalty::none },
	invalid_op,
	Op{ Instruction::sec, Mode::implied, 2, Penalty::none },
	Op{ Instruction::and_, Mode::absolute_y, 4, Penalty::page_cross },
	Op{ Instruction::nop, Mode::implied, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::nop, Mode::absolute_x, 4, Penalty::page_cross },
	Op{ Instruction::and_, Mode::absolute_x, 4, Penalty::page_cross },
	Op{ Instruction::rol, Mode::absolute_x, 7, Penalty::none },
	invalid_op,

	Op{ Instruction::rti, Mode::implied, 6, Penalty::none },
	Op{ Instruction::eor, Mode::indirect_x, 6, Penalty::none },
	invalid_op,
	invalid_op,
	Op{ Instruction::nop, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::eor, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::lsr, Mode::zero_page, 5, Penalty::none },
	invalid_op,
	Op{ Instruction::pha, Mode::implied, 3, Penalty::none },
	Op{ Instruction::eor, Mode::immediate, 2, Penalty::none },
	Op{ Instruction::lsr, Mode::accumulator, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::jmp, Mode::absolute, 3, Penalty::none },
	Op{ Instruction::eor, Mode::absolute, 4, Penalty::none },
	Op{ Instruction::lsr, Mode::absolute, 6, Penalty::none },
	invalid_op,

	Op{ Instruction::bvc, Mode::relative, 2, Penalty::branch },
	Op{ Instruction::eor, Mode::indirect_y, 5, Penalty::page_cross },
	invalid_op,
	invalid_op,
	Op{ Instruction::nop, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::eor, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::lsr, Mode::zero_page_x, 6, Penalty::none },
	invalid_op,
	Op{ Instruction::cli, Mode::implied, 2, Penalty::none },
	Op{ Instruction::eor, Mode::absolute_y, 4, Penalty::page_cross },
	Op{ Instruction::nop, Mode::implied, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::nop, Mode::absolute_x, 4, Penalty::page_cross },
	Op{ Instruction::eor, Mode::absolute_x, 4, Penalty::page_cross },
	Op{ Instruction::lsr, Mode::absolute_x, 7, Penalty::none },
	invalid_op,

	Op{ Instruction::rts, Mode::implied, 6, Penalty::none },
	Op{ Instruction::adc, Mode::indirect_x, 6, Penalty::none },
	invalid_op,
	invalid_op,
	Op{ Instruction::nop, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::adc, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::ror, Mode::zero_page, 5, Penalty::none },
	invalid_op,
	Op{ Instruction::pla, Mode::implied, 4, Penalty::none },
	Op{ Instruction::adc, Mode::immediate, 2, Penalty::none },
	Op{ Instruction::ror, Mode::accumulator, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::jmp, Mode::indirect, 5, Penalty::none },
	Op{ Instruction::adc, Mode::absolute, 4, Penalty::none },
	Op{ Instruction::ror, Mode::absolute, 6, Penalty::none },
	invalid_op,

	Op{ Instruction::bvs, Mode::relative, 2, Penalty::branch },
	Op{ Instruction::adc, Mode::indirect_y, 5, Penalty::page_cross },
	invalid_op,
	invalid_op,
	Op{ Instruction::nop, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::adc, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::ror, Mode::zero_page_x, 6, Penalty::none },
	invalid_op,
	Op{ Instruction::sei, Mode::implied, 2, Penalty::none },
	Op{ Instruction::adc, Mode::absolute_y, 4, Penalty::page_cross },
	Op{ Instruction::nop, Mode::implied, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::nop, Mode::absolute_x, 4, Penalty::page_cross },
	Op{ Instruction::adc, Mode::absolute_x, 4, Penalty::page_cross },
	Op{ Instruction::ror, Mode::absolute_x, 7, Penalty::none },
	invalid_op,

	Op{ Instruction::nop, Mode::immediate, 2, Penalty::none },
	Op{ Instruction::sta, Mode::indirect_x, 6, Penalty::none },
	Op{ Instruction::nop, Mode::immediate, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::sty, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::sta, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::stx, Mode::zero_page, 3, Penalty::none },
	invalid_op,
	Op{ Instruction::dey, Mode::implied, 2, Penalty::none },
	Op{ Instruction::nop, Mode::immediate, 2, Penalty::none },
	Op{ Instruction::txa, Mode::implied, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::sty, Mode::absolute, 4, Penalty::none },
	Op{ Instruction::sta, Mode::absolute, 4, Penalty::none },
	Op{ Instruction::stx, Mode::absolute, 4, Penalty::none },
	invalid_op,

	Op{ Instruction::bcc, Mode::relative, 2, Penalty::branch },
	Op{ Instruction::sta, Mode::indirect_y, 6, Penalty::none },
	invalid_op,
	invalid_op,
	Op{ Instruction::sty, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::sta, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::stx, Mode::zero_page_y, 4, Penalty::none },
	invalid_op,
	Op{ Instruction::tya, Mode::implied, 2, Penalty::none },
	Op{ Instruction::sta, Mode::absolute_y, 5, Penalty::none },
	Op{ Instruction::txs, Mode::implied, 2, Penalty::none },
	invalid_op,
	invalid_op,
	Op{ Instruction::sta, Mode::absolute_x, 5, Penalty::none },
	invalid_op,
	invalid_op,

	Op{ Instruction::ldy, Mode::immediate, 2, Penalty::none },
	Op{ Instruction::lda, Mode::indirect_x, 6, Penalty::none },
	Op{ Instruction::ldx, Mode::immediate, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::ldy, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::lda, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::ldx, Mode::zero_page, 3, Penalty::none },
	invalid_op,
	Op{ Instruction::tay, Mode::implied, 2, Penalty::none },
	Op{ Instruction::lda, Mode::immediate, 2, Penalty::none },
	Op{ Instruction::tax, Mode::implied, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::ldy, Mode::absolute, 4, Penalty::none },
	Op{ Instruction::lda, Mode::absolute, 4, Penalty::none },
	Op{ Instruction::ldx, Mode::absolute, 4, Penalty::none },
	invalid_op,

	Op{ Instruction::bcs, Mode::relative, 2, Penalty::branch },
	Op{ Instruction::lda, Mode::indirect_y, 5, Penalty::page_cross },
	invalid_op,
	invalid_op,
	Op{ Instruction::ldy, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::lda, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::ldx, Mode::zero_page_y, 4, Penalty::none },
	invalid_op,
	Op{ Instruction::clv, Mode::implied, 2, Penalty::none },
	Op{ Instruction::lda, Mode::absolute_y, 4, Penalty::page_cross },
	Op{ Instruction::tsx, Mode::implied, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::ldy, Mode::absolute_x, 4, Penalty::page_cross },
	Op{ Instruction::lda, Mode::absolute_x, 4, Penalty::page_cross },
	Op{ Instruction::ldx, Mode::absolute_y, 4, Penalty::page_cross },
	invalid_op,

	Op{ Instruction::cpy, Mode::immediate, 2, Penalty::none },
	Op{ Instruction::cmp, Mode::indirect_x, 6, Penalty::none },
	Op{ Instruction::nop, Mode::immediate, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::cpy, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::cmp, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::dec, Mode::zero_page, 5, Penalty::none },
	invalid_op,
	Op{ Instruction::iny, Mode::implied, 2, Penalty::none },
	Op{ Instruction::cmp, Mode::immediate, 2, Penalty::none },
	Op{ Instruction::dex, Mode::implied, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::cpy, Mode::absolute, 4, Penalty::none },
	Op{ Instruction::cmp, Mode::absolute, 4, Penalty::none },
	Op{ Instruction::dec, Mode::absolute, 6, Penalty::none },
	invalid_op,

	Op{ Instruction::bne, Mode::relative, 2, Penalty::branch },
	Op{ Instruction::cmp, Mode::indirect_y, 5, Penalty::page_cross },
	invalid_op,
	invalid_op,
	Op{ Instruction::nop, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::cmp, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::dec, Mode::zero_page_x, 6, Penalty::none },
	invalid_op,
	Op{ Instruction::cld, Mode::implied, 2, Penalty::none },
	Op{ Instruction::cmp, Mode::absolute_y, 4, Penalty::page_cross },
	Op{ Instruction::nop, Mode::implied, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::nop, Mode::absolute_x, 4, Penalty::page_cross },
	Op{ Instruction::cmp, Mode::absolute_x, 4, Penalty::page_cross },
	Op{ Instruction::dec, Mode::absolute_x, 7, Penalty::none },
	invalid_op,

	Op{ Instruction::cpx, Mode::immediate, 2, Penalty::none },
	Op{ Instruction::sbc, Mode::indirect_x, 6, Penalty::none },
	Op{ Instruction::nop, Mode::immediate, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::cpx, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::sbc, Mode::zero_page, 3, Penalty::none },
	Op{ Instruction::inc, Mode::zero_page, 5, Penalty::none },
	invalid_op,
	Op{ Instruction::inx, Mode::implied, 2, Penalty::none },
	Op{ Instruction::sbc, Mode::immediate, 2, Penalty::none },
	Op{ Instruction::nop, Mode::implied, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::cpx, Mode::absolute, 4, Penalty::none },
	Op{ Instruction::sbc, Mode::absolute, 4, Penalty::none },
	Op{ Instruction::inc, Mode::absolute, 6, Penalty::none },
	invalid_op,

	Op{ Instruction::beq, Mode::relative, 2, Penalty::branch },
	Op{ Instruction::sbc, Mode::indirect_y, 5, Penalty::page_cross },
	invalid_op,
	invalid_op,
	Op{ Instruction::nop, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::sbc, Mode::zero_page_x, 4, Penalty::none },
	Op{ Instruction::inc, Mode::zero_page_x, 6, Penalty::none },
	invalid_op,
	Op{ Instruction::sed, Mode::implied, 2, Penalty::none },
	Op{ Instruction::sbc, Mode::absolute_y, 4, Penalty::page_cross },
	Op{ Instruction::nop, Mode::implied, 2, Penalty::none },
	invalid_op,
	Op{ Instruction::nop, Mode::absolute_x, 4, Penalty::page_cross },
	Op{ Instruction::sbc, Mode::absolute_x, 4, Penalty::page_cross },
	Op{ Instruction::inc, Mode::absolute_x, 7, Penalty::none },
	invalid_op
};

const Op& Cpu::get_op(uint8_t opcode)
{
	const auto& op = ops[opcode];
	if (op == invalid_op) {
		throw std::invalid_argument{ std::to_string(opcode) };
	}
	return op;
}

unsigned Cpu::step()
{
#if LOGGING_ENABLED
	if (program_counter == memory.read_addr(Cpu::nmi_vec_addr)) {
		LOG("NMI addr accessed");
	} else if (program_counter == memory.read_addr(Cpu::reset_vec_addr)) {
		LOG("RESET addr accessed");
	} else if (program_counter == memory.read_addr(Cpu::irq_vec_addr)) {
		LOG("IRQ addr accessed");
	}
#endif

	if (cycle_stall) {
		cycle_stall--;
		cycle = (cycle + 1) % cpu_cycle_wraparound; // TODO: 1 or 3?
		return 1;
	}

	auto start_cycle = cycle;

	do_int();

	unsigned penalty_sum = 0;

	const auto& op = get_op(memory.read(program_counter));

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

	LOG_FMT("cycle_end=%u, cycle_begin=%u", cycle, start_cycle);
	auto cycle_diff = (int)cycle - (int)start_cycle;
	if (cycle_diff < 0) {
		cycle_diff += cpu_cycle_wraparound;
	}

	return cycle_diff;
}

void Cpu::reset()
{
	program_counter = read_addr_from_mem(reset_vec_addr);
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
