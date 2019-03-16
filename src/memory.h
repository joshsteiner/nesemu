#pragma once

#include <array>
#include <cstdint>

#include "cpu.h"
#include "ppu.h"
#include "cart.h"

class Cpu;
class Ppu;
class Cartridge;

const size_t MemorySize = 0x10000;
const size_t InternalRam = 0x800;
const size_t PageSize = 0x100;

// negative numbers point to registers
// non-negative to normal memory addresses
using ExtendedAddr = int;
const ExtendedAddr ARegisterExtAddr = -1;

class Memory
{
private:
	std::array<uint8_t, InternalRam> ram;
	// Cpu* cpu;
	// Ppu* ppu;
	Cartridge cart;

private:
	auto at(ExtendedAddr addr) -> uint8_t&;

public:
	auto read(ExtendedAddr addr) -> uint8_t;
	auto write(ExtendedAddr addr, uint8_t value) -> void;

	//inline auto connect(Cpu* cpu) -> void { this->cpu = cpu; }
	//inline auto connect(Ppu* ppu) -> void { this->ppu = ppu; }
	inline auto connect(Cartridge cart) -> void { this->cart = cart; }
};

extern Ppu ppu;
extern Cpu cpu;