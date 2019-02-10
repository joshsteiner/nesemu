#include "cpu.h"
#include "nesemu.h"
 
Cpu::Memory::Memory(Cpu& cpu)
	: cpu(cpu) 
{
}

auto Cpu::Memory::operator[](ExtendedAddr addr) -> uint8_t&
{
	if (addr >= 0) {
		return data[addr];
	}

	switch (addr) {
	case ARegisterExtAddr:
		return cpu.a;
	}
}

Cpu::ExtPtr::ExtPtr(Cpu& cpu, Mode mode)
	: cpu(cpu)
	, mode(mode)
{
}

auto Cpu::ExtPtr::operator*() -> uint8_t&
{
	return cpu.memory[cpu.get_addr(mode)];
}

auto Cpu::ExtPtr::addr() -> ExtendedAddr
{
	return cpu.get_addr(mode);
}

auto Cpu::push(uint8_t value) -> void
{
	memory[StackPage + stack_ptr] = value;
	stack_ptr--;
}

auto Cpu::push_addr(uint16_t addr) -> void
{
	push(hi_byte(addr));
	push(lo_byte(addr));
}

auto Cpu::pull() -> uint8_t
{
	stack_ptr++;
	return memory[StackPage + stack_ptr];
}

auto Cpu::pull_addr() -> uint16_t
{
	auto lo = pull();
	auto hi = pull();
	return as_addr(hi, lo);
}

auto Cpu::read_addr_from_mem(uint16_t addr) -> uint16_t
{
	uint16_t page = 0xFF00 & addr;
	return as_addr(
		memory[((addr + 1) & 0x00FF) | page],
		memory[addr & 0x00FF]
	);
}

auto Cpu::peek_arg() -> uint8_t
{
	return memory[program_counter + 1];
}

auto Cpu::peek_addr_arg() -> uint16_t
{
	return as_addr(
		memory[program_counter + 2],
		memory[program_counter + 1]
	);
}

auto Cpu::zn(uint8_t val) -> void
{
	status.zero = !val;
	status.negative = val & bit(7) ? 1 : 0;
}

auto Cpu::get_addr(Mode mode) -> ExtendedAddr
{
	switch (mode) {
	case Implied:
		return 0;
	case Accumulator:
		return ARegisterExtAddr;
	case Immediate:
		return program_counter + 1;
	case Relative:
		return program_counter + (int8_t)peek_arg() + 2;
	case ZeroPage:
		return peek_arg();
	case ZeroPageX:
		return (peek_arg() + x) % PageSize;
	case ZeroPageY:
		return (peek_arg() + y) % PageSize;
	case Absolute:
		return peek_addr_arg();
	case AbsoluteX:
		return (peek_addr_arg() + x) % MemorySize;
	case AbsoluteY:
		return (peek_addr_arg() + y) % MemorySize;
	case Indirect:
		return read_addr_from_mem(peek_addr_arg());
	case IndirectX:
		return as_addr(
			memory[(peek_arg() + x + 1) % PageSize],
			memory[(peek_arg() + x) % PageSize]
		);
	case IndirectY:
		return (as_addr(
			memory[(peek_arg() + 1) % PageSize],
			memory[peek_arg()]
		) + y) % MemorySize;
	}
}

auto Cpu::get_arg_size(Mode mode) -> unsigned
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

auto Cpu::exec_instr(Instruction instr, ExtPtr m) -> std::pair<bool, bool>
{
	bool jumped = false;
	bool page_crossed = false;

	switch (instr) {
	case Nop:
		break;
	case Inc:
		++*m;
		zn(*m);
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
		--*m;
		zn(*m);
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
		status.carry = 0;
		break;
	case Cld:
		status.decimal_mode = 0;
		break;
	case Cli:
		status.interrupt_disable = 0;
		break;
	case Clv:
		status.overflow = 0;
		break;
	case Sec:
		status.carry = 1;
		break;
	case Sed:
		status.decimal_mode = 1;
		break;
	case Sei:
		status.interrupt_disable = 1;
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
		if (status.carry) {
			program_counter = m.addr();
			jumped = true;
		}
		break;
	case Bcc:
		if (!status.carry) {
			program_counter = m.addr();
			jumped = true;
		}
		break;
	case Beq:
		if (status.zero) {
			program_counter = m.addr();
			jumped = true;
		}
		break;
	case Bne:
		if (!status.zero) {
			program_counter = m.addr();
			jumped = true;
		}
		break;
	case Bmi:
		if (status.negative) {
			program_counter = m.addr();
			jumped = true;
		}
		break;
	case Bpl:
		if (!status.negative) {
			program_counter = m.addr();
			jumped = true;
		}
		break;
	case Bvs:
		if (status.overflow) {
			program_counter = m.addr();
			jumped = true;
		}
		break;
	case Bvc:
		if (!status.overflow) {
			program_counter = m.addr();
			jumped = true;
		}
		break;
	case Lda:
		zn(a = *m);
		break;
	case Ldx:
		zn(x = *m);
		break;
	case Ldy:
		zn(y = *m);
		break;
	case Sta:
		*m = a;
		break;
	case Stx:
		*m = x;
		break;
	case Sty:
		*m = y;
		break;
	case Bit:
		status.zero = (a & *m) == 0;
		status.overflow = *m & bit(6) ? 1 : 0;
		status.negative = *m & bit(7) ? 1 : 0;
		break;
	case Cmp:
		status.carry = a >= *m;
		status.zero = a == *m;
		status.negative = (a - *m) & bit(7) ? 1 : 0;
		break;
	case Cpx:
		status.carry = x >= *m;
		status.zero = x == *m;
		status.negative = (x - *m) & bit(7) ? 1 : 0;
		break;
	case Cpy:
		status.carry = y >= *m;
		status.zero = y == *m;
		status.negative = (y - *m) & bit(7) ? 1 : 0;
		break;
	case And:
		zn(a &= *m);
		break;
	case Ora:
		zn(a |= *m);
		break;
	case Eor:
		zn(a ^= *m);
		break;
	case Asl:
		status.carry = *m & bit(7) ? 1 : 0;
		zn(*m <<= 1);
		break;
	case Lsr:
		status.carry = *m & bit(0) ? 1 : 0;
		zn(*m >>= 1);
		break;
	case Rol:
	{
		auto old_carry = status.carry;
		status.carry = *m & bit(7) ? 1 : 0;
		zn(*m = *m << 1 | old_carry);
		break;
	}
	case Ror:
	{
		auto old_carry = status.carry;
		status.carry = *m & bit(0) ? 1 : 0;
		zn(*m = *m >> 1 | old_carry << 7);
		break;
	}
	case Adc:
	{
		auto old_a = a;
		zn(a += *m + status.carry);
		status.carry = old_a + *m + status.carry > 0xFF;
		status.overflow = !((old_a ^ *m) & bit(7)) && ((old_a ^ a) & bit(7));
		break;
	}
	case Sbc:
	{
		auto old_a = a;
		zn(a -= *m + !status.carry);
		status.carry = old_a - *m - !status.carry >= 0x00;
		status.overflow = (old_a ^ *m) & bit(7) && ((old_a ^ a) & bit(7));
		break;
	}
	case Jmp:
		program_counter = m.addr();
		jumped = true;
		break;
	case Jsr:
		push_addr(program_counter + 2);
		program_counter = m.addr();
		jumped = true;
	case Rts:
		program_counter = pull_addr() + 1;
		jumped = true;
		break;
	case Brk:
		program_counter++;
		push_addr(program_counter);
		push(status.raw | bit(4) | bit(5));
		status.break_ = 1;
		program_counter = read_addr_from_mem(IrqBrkVecAddr);
		jumped = true;
		break;
	case Rti:
		status.raw = pull() & 0xEF | 0x20;
		program_counter = pull_addr();
		jumped = true;
		break;
	}

	return std::make_pair(jumped, page_crossed);
}

auto Cpu::get_op(uint8_t opcode) -> Op
{
	switch (opcode) {
	case 0x00: return { Brk, Implied, { 7, Penalty::None } };
	case 0x01: return { Ora, IndirectX, { 6, Penalty::None } };
	case 0x05: return { Ora, ZeroPage, { 3, Penalty::None } };
	case 0x06: return { Asl, ZeroPage, { 5, Penalty::None } };
	case 0x08: return { Php, Implied, { 3, Penalty::None } };
	case 0x09: return { Ora, Immediate, { 2, Penalty::None } };
	case 0x0A: return { Asl, Accumulator, { 2, Penalty::None } };
	case 0x0D: return { Ora, Absolute, { 4, Penalty::None } };
	case 0x0E: return { Asl, Absolute, { 6, Penalty::None } };

	case 0x10: return { Bpl, Relative, { 2, Penalty::Branch } };
	case 0x11: return { Ora, IndirectY, { 5, Penalty::PageCross } };
	case 0x15: return { Ora, ZeroPageX, { 4, Penalty::None } };
	case 0x16: return { Asl, ZeroPageX, { 6, Penalty::None } };
	case 0x18: return { Clc, Implied, { 2, Penalty::None } };
	case 0x19: return { Ora, AbsoluteY, { 4, Penalty::PageCross } };
	case 0x1D: return { Ora, AbsoluteX, { 4, Penalty::PageCross } };
	case 0x1E: return { Asl, AbsoluteX, { 7, Penalty::None } };

	case 0x20: return { Jsr, Absolute, { 6, Penalty::None } };
	case 0x21: return { And, IndirectX, { 6, Penalty::None } };
	case 0x24: return { Bit, ZeroPage, { 3, Penalty::None } };
	case 0x25: return { And, ZeroPage, { 3, Penalty::None } };
	case 0x26: return { Rol, ZeroPage, { 5, Penalty::None } };
	case 0x28: return { Plp, Implied, { 4, Penalty::None } };
	case 0x29: return { And, Immediate, { 2, Penalty::None } };
	case 0x2A: return { Rol, Accumulator, { 2, Penalty::None } };
	case 0x2C: return { Bit, Absolute, { 4, Penalty::None } };
	case 0x2D: return { And, Absolute, { 4, Penalty::None } };
	case 0x2E: return { Rol, Absolute, { 6, Penalty::None } };

	case 0x30: return { Bmi, Relative, { 2, Penalty::Branch } };
	case 0x31: return { And, IndirectY, { 5, Penalty::PageCross } };
	case 0x35: return { And, ZeroPageX, { 4, Penalty::None } };
	case 0x36: return { Rol, ZeroPageX, { 6, Penalty::None } };
	case 0x38: return { Sec, Implied, { 2, Penalty::None } };
	case 0x39: return { And, AbsoluteY, { 4, Penalty::PageCross } };
	case 0x3D: return { And, AbsoluteX, { 4, Penalty::PageCross } };
	case 0x3E: return { Rol, AbsoluteX, { 7, Penalty::None } };

	case 0x40: return { Rti, Implied, { 6, Penalty::None } };
	case 0x41: return { Eor, IndirectX, { 6, Penalty::None } };
	case 0x45: return { Eor, ZeroPage, { 3, Penalty::None } };
	case 0x46: return { Lsr, ZeroPage, { 5, Penalty::None } };
	case 0x48: return { Pha, Implied, { 3, Penalty::None } };
	case 0x49: return { Eor, Immediate, { 2, Penalty::None } };
	case 0x4A: return { Lsr, Accumulator, { 2, Penalty::None } };
	case 0x4C: return { Jmp, Absolute, { 3, Penalty::None } };
	case 0x4D: return { Eor, Absolute, { 4, Penalty::None } };
	case 0x4E: return { Lsr, Absolute, { 6, Penalty::None } };

	case 0x50: return { Bvc, Relative, { 2, Penalty::Branch } };
	case 0x51: return { Eor, IndirectY, { 5, Penalty::PageCross } };
	case 0x55: return { Eor, ZeroPageX, { 4, Penalty::None } };
	case 0x56: return { Lsr, ZeroPageX, { 6, Penalty::None } };
	case 0x58: return { Cli, Implied, { 2, Penalty::None } };
	case 0x59: return { Eor, AbsoluteY, { 4, Penalty::PageCross } };
	case 0x5D: return { Eor, AbsoluteX, { 4, Penalty::PageCross } };
	case 0x5E: return { Lsr, AbsoluteX, { 7, Penalty::None } };

	case 0x60: return { Rts, Implied, { 6, Penalty::None } };
	case 0x61: return { Adc, IndirectX, { 6, Penalty::None } };
	case 0x65: return { Adc, ZeroPage, { 3, Penalty::None } };
	case 0x66: return { Ror, ZeroPage, { 5, Penalty::None } };
	case 0x68: return { Pla, Implied, { 4, Penalty::None } };
	case 0x69: return { Adc, Immediate, { 2, Penalty::None } };
	case 0x6A: return { Ror, Accumulator, { 2, Penalty::None } };
	case 0x6C: return { Jmp, Indirect, { 5, Penalty::None } };
	case 0x6D: return { Adc, Absolute, { 4, Penalty::None } };
	case 0x6E: return { Ror, Absolute, { 6, Penalty::None } };

	case 0x70: return { Bvs, Relative, { 2, Penalty::Branch } };
	case 0x71: return { Adc, IndirectY, { 5, Penalty::PageCross } };
	case 0x75: return { Adc, ZeroPageX, { 4, Penalty::None } };
	case 0x76: return { Ror, ZeroPageX, { 6, Penalty::None } };
	case 0x78: return { Sei, Implied, { 2, Penalty::None } };
	case 0x79: return { Adc, AbsoluteY, { 4, Penalty::PageCross } };
	case 0x7D: return { Adc, AbsoluteX, { 4, Penalty::PageCross } };
	case 0x7E: return { Ror, AbsoluteX, { 7, Penalty::None } };

	case 0x81: return { Sta, IndirectX, { 6, Penalty::None } };
	case 0x84: return { Sty, ZeroPage, { 3, Penalty::None } };
	case 0x85: return { Sta, ZeroPage, { 3, Penalty::None } };
	case 0x86: return { Stx, ZeroPage, { 3, Penalty::None } };
	case 0x88: return { Dey, Implied, { 2, Penalty::None } };
	case 0x8A: return { Txa, Implied, { 2, Penalty::None } };
	case 0x8C: return { Sty, Absolute, { 4, Penalty::None } };
	case 0x8D: return { Sta, Absolute, { 4, Penalty::None } };
	case 0x8E: return { Stx, Absolute, { 4, Penalty::None } };

	case 0x90: return { Bcc, Relative, { 2, Penalty::Branch } };
	case 0x91: return { Sta, IndirectY, { 6, Penalty::None } };
	case 0x94: return { Sty, ZeroPageX, { 4, Penalty::None } };
	case 0x95: return { Sta, ZeroPageX, { 4, Penalty::None } };
	case 0x96: return { Stx, ZeroPageY, { 4, Penalty::None } };
	case 0x98: return { Tya, Implied, { 2, Penalty::None } };
	case 0x99: return { Sta, AbsoluteY, { 5, Penalty::None } };
	case 0x9A: return { Txs, Implied, { 2, Penalty::None } };
	case 0x9D: return { Sta, AbsoluteX, { 5, Penalty::None } };

	case 0xA0: return { Ldy, Immediate, { 2, Penalty::None } };
	case 0xA1: return { Lda, IndirectX, { 6, Penalty::None } };
	case 0xA2: return { Ldx, Immediate, { 2, Penalty::None } };
	case 0xA4: return { Ldy, ZeroPage, { 3, Penalty::None } };
	case 0xA5: return { Lda, ZeroPage, { 3, Penalty::None } };
	case 0xA6: return { Ldx, ZeroPage, { 3, Penalty::None } };
	case 0xA8: return { Tay, Implied, { 2, Penalty::None } };
	case 0xA9: return { Lda, Immediate, { 2, Penalty::None } };
	case 0xAA: return { Tax, Implied, { 2, Penalty::None } };
	case 0xAC: return { Ldy, Absolute, { 4, Penalty::None } };
	case 0xAD: return { Lda, Absolute, { 4, Penalty::None } };
	case 0xAE: return { Ldx, Absolute, { 4, Penalty::None } };

	case 0xB0: return { Bcs, Relative, { 2, Penalty::Branch } };
	case 0xB1: return { Lda, IndirectY, { 5, Penalty::PageCross } };
	case 0xB4: return { Ldy, ZeroPageX, { 4, Penalty::None } };
	case 0xB5: return { Lda, ZeroPageX, { 4, Penalty::None } };
	case 0xB6: return { Ldx, ZeroPageY, { 4, Penalty::None } };
	case 0xB8: return { Clv, Implied, { 2, Penalty::None } };
	case 0xB9: return { Lda, AbsoluteY, { 4, Penalty::PageCross } };
	case 0xBA: return { Tsx, Implied, { 2, Penalty::None } };
	case 0xBC: return { Ldy, AbsoluteX, { 4, Penalty::PageCross } };
	case 0xBD: return { Lda, AbsoluteX, { 4, Penalty::PageCross } };
	case 0xBE: return { Ldx, AbsoluteY, { 4, Penalty::PageCross } };

	case 0xC0: return { Cpy, Immediate, { 2, Penalty::None } };
	case 0xC1: return { Cmp, IndirectX, { 6, Penalty::None } };
	case 0xC4: return { Cpy, ZeroPage, { 3, Penalty::None } };
	case 0xC5: return { Cmp, ZeroPage, { 3, Penalty::None } };
	case 0xC6: return { Dec, ZeroPage, { 5, Penalty::None } };
	case 0xC8: return { Iny, Implied, { 2, Penalty::None } };
	case 0xC9: return { Cmp, Immediate, { 2, Penalty::None } };
	case 0xCA: return { Dex, Implied, { 2, Penalty::None } };
	case 0xCC: return { Cpy, Absolute, { 4, Penalty::None } };
	case 0xCD: return { Cmp, Absolute, { 4, Penalty::None } };
	case 0xCE: return { Dec, Absolute, { 6, Penalty::None } };

	case 0xD0: return { Bne, Relative, { 2, Penalty::Branch } };
	case 0xD1: return { Cmp, IndirectY, { 5, Penalty::PageCross } };
	case 0xD5: return { Cmp, ZeroPageX, { 4, Penalty::None } };
	case 0xD6: return { Dec, ZeroPageX, { 6, Penalty::None } };
	case 0xD8: return { Cld, Implied, { 2, Penalty::None } };
	case 0xD9: return { Cmp, AbsoluteY, { 4, Penalty::PageCross } };
	case 0xDD: return { Cmp, AbsoluteX, { 4, Penalty::PageCross } };
	case 0xDE: return { Dec, AbsoluteX, { 7, Penalty::None } };

	case 0xE0: return { Cpx, Immediate, { 2, Penalty::None } };
	case 0xE1: return { Sbc, IndirectX, { 6, Penalty::None } };
	case 0xE4: return { Cpx, ZeroPage, { 3, Penalty::None } };
	case 0xE5: return { Sbc, ZeroPage, { 3, Penalty::None } };
	case 0xE6: return { Inc, ZeroPage, { 5, Penalty::None } };
	case 0xE8: return { Inx, Implied, { 2, Penalty::None } };
	case 0xE9: return { Sbc, Immediate, { 2, Penalty::None } };
	case 0xEA: return { Nop, Implied, { 2, Penalty::None } };
	case 0xEC: return { Cpx, Absolute, { 4, Penalty::None } };
	case 0xED: return { Sbc, Absolute, { 4, Penalty::None } };
	case 0xEE: return { Inc, Absolute, { 6, Penalty::None } };

	case 0xf0: return { Beq, Relative, { 2, Penalty::Branch } };
	case 0xF1: return { Sbc, IndirectY, { 5, Penalty::PageCross } };
	case 0xF5: return { Sbc, ZeroPageX, { 4, Penalty::None } };
	case 0xF6: return { Inc, ZeroPageX, { 6, Penalty::None } };
	case 0xF8: return { Sed, Implied, { 2, Penalty::None } };
	case 0xF9: return { Sbc, AbsoluteY, { 4, Penalty::PageCross } };
	case 0xFD: return { Sbc, AbsoluteX, { 4, Penalty::PageCross } };
	case 0xFE: return { Inc, AbsoluteX, { 7, Penalty::None } };

	default: throw "invalid opcode";
	}
};

auto Cpu::step() -> void
{
	unsigned penalty_sum = 0;

	auto [instr, mode, cycles] = get_op(memory[program_counter]);
	auto [base_cycle, penalty] = cycles;

	auto old_pc = program_counter;

	if (program_counter == 0xD922) {
		// debug
		int r = 5;
	}

	bool pages_differ = ((old_pc + get_arg_size(mode) + 1) & 0xFF00) != (get_addr(mode) & 0xFF00);
	bool jumped = exec_instr(instr, ExtPtr{ *this, mode });

	switch (penalty) {
	case Penalty::Branch:
		if (!jumped) { break; }
		penalty_sum++;
	case Penalty::PageCross:
		if (pages_differ) {
			penalty_sum++;
		}
	case Penalty::None:
		break;
	}

	if (!jumped) {
		program_counter += get_arg_size(mode) + 1;
	}

	this->cycle = (this->cycle + (base_cycle + penalty_sum) * 3) % CpuCycleWraparound;
}

auto Cpu::reset() -> void 
{
	program_counter = read_addr_from_mem(ResetVecAddr);
}

auto Cpu::load_cart(const Cartridge& cart) -> void
{
	int start;

	switch (cart.prg_rom.size() / 0x4000) {
	case 1:
		start = 0xC000;
		break;
	case 2:
		start = 0x8000;
		break;
	default:
		throw "too many ROM banks!\n";
	}

	for (int i = 0; i < cart.prg_rom.size(); i++) {
		memory[start + i] = cart.prg_rom[i];
	}
}

auto Cpu::take_snapshot() -> CpuSnapshot
{
	auto opcode = memory[program_counter];
	std::vector<uint8_t> instr{ opcode };
	auto arg_size = get_arg_size(std::get<1>(get_op(opcode)));
	for (unsigned i = 0; i < arg_size; i++) {
		instr.push_back(memory[program_counter + 1 + i]);
	}
	return CpuSnapshot{ program_counter, instr, a, x, y, status.raw, stack_ptr, cycle };
}
