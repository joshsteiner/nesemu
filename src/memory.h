#ifndef NESEMU_MEMORY_H
#define NESEMU_MEMORY_H

#include <array>
#include <cstdint>

#include "cpu.h"
#include "ppu.h"
#include "cart.h"

class Cpu;
class Ppu;
class Cartridge;

const size_t memory_size = 0x10000;
const size_t internal_ram = 0x800;
const size_t page_size = 0x100;

// negative numbers point to registers
// non-negative to normal memory addresses
using Extended_addr = int;
const Extended_addr a_register_ext_addr = -1;

class Memory
{
public:
	Cartridge cart;

	uint8_t read(Extended_addr addr);
	void write(Extended_addr addr, uint8_t value);

	inline void connect(Cartridge cart) { this->cart = std::move(cart); }

private:
	std::array<uint8_t, internal_ram> ram;
	uint8_t& at(Extended_addr addr);
};

extern Ppu ppu;
extern Cpu cpu;

#endif
