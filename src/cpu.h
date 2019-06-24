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
using Extended_addr = int;

class Cpu_snapshot {
public:
	const uint16_t pc;
	const std::vector<uint8_t> instr;
	const uint8_t a, x, y;
	const uint8_t p, sp;
	const unsigned cyc;

	Cpu_snapshot(uint16_t pc, std::vector<uint8_t> instr, uint8_t a,
	             uint8_t x, uint8_t y, uint8_t p, uint8_t sp, unsigned cyc);

	std::string str() const;
};

class Cpu {
	friend class Memory;
	friend class ExtPtr;

private:
	using Status_register = FLAG_BYTE({
		unsigned carry             : 1;
		unsigned zero              : 1;
		unsigned interrupt_disable : 1;
		unsigned decimal_mode      : 1;
		unsigned break_            : 1;
		unsigned                   : 1;
		unsigned overflow          : 1;
		unsigned negative          : 1;
	});

	enum class Mode {
		implied, accumulator, immediate, relative,
		zero_page, zero_page_x, zero_page_y,
		absolute, absolute_x, absolute_y,
		indirect, indirect_x, indirect_y
	};

	enum Instruction {
		nop,
		inc, inx, iny, dec, dex, dey,
		clc, cld, cli, clv, sec, sed, sei,
		tax, tay, txa, tya, txs, tsx,
		php, pha, plp, pla,
		bcs, bcc, beq, bne, bmi, bpl, bvs, bvc,
		lda, ldx, ldy, sta, stx, sty,
		bit, cmp, cpx, cpy,
		and_, ora, eor, asl, lsr, rol, ror, adc, sbc,
		jmp, jsr, rts,
		brk, rti
	};

	enum class Penalty {
		none, page_cross, branch
	};

	struct Op {
		const Instruction instr;
		const Mode mode;
		const unsigned base_cycle;
		const Penalty penalty;

		Op(Instruction instr, Mode mode, unsigned base_cycle, Penalty penalty);
	};

public:
	enum class Interrupt {
		none, irq, nmi
	};

	uint8_t a, x, y;
	uint8_t stack_ptr;
	uint16_t program_counter;
	Status_register status;

	unsigned cycle;
	unsigned cycle_stall;

	Cpu();

	unsigned step();
	void reset();

	void trigger(Interrupt interrupt);
	void stall(unsigned cycles);

	Cpu_snapshot take_snapshot();

private:
	static const uint16_t stack_page = 0x0100;
	static const unsigned cpu_cycle_wraparound = 341;

	static const Extended_addr nmi_vec_addr = 0xFFFA;
	static const Extended_addr reset_vec_addr = 0xFFFC;
	static const Extended_addr irq_vec_addr = 0xFFFE;

	bool jumped;
	bool page_crossed;
	Interrupt interrupt;

	void push(uint8_t value);
	void push_addr(uint16_t value);
	uint8_t pull();
	uint16_t pull_addr();

	uint16_t read_addr_from_mem(uint16_t addr);

	uint8_t peek_arg();
	uint16_t peek_addr_arg();

	void zn(uint8_t val);

	Extended_addr get_addr(Mode mode);
	unsigned get_arg_size(Mode mode);
	void exec_instr(Instruction instr, Extended_addr addr);
	Op get_op(uint8_t opcode);

	void do_int(Interrupt interrupt);
};

extern Cpu cpu;
extern Memory memory;

#endif
