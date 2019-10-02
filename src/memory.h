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

class Memory {
public:
	uint8_t read(Extended_addr addr);
	uint16_t read_addr(Extended_addr addr);
	void write(Extended_addr addr, uint8_t value);

private:
	std::array<uint8_t, internal_ram> ram;
	uint8_t& at(Extended_addr addr);
};


extern Ppu ppu;
extern Cpu cpu;

#endif
