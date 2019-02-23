#include <cassert>
#include <sstream>

#include "memory.h"
#include "common.h"

auto Memory::at(ExtendedAddr addr) -> uint8_t&
{
	if (addr < 0) {
		switch (addr) {
		case ARegisterExtAddr: return cpu->a;
		}
	} else if (BETWEEN(addr, 0x0000, 0x2000)) {
		return ram[addr % 0x800];
	} else if (BETWEEN(addr, 0x2000, 0x4000)) {
		throw "use either read or write";
	} else if (BETWEEN(addr, 0x4000, 0x4018)) {
		// TODO: APU/IO registers
		throw "apu/io register r/w";
	} else if (BETWEEN(addr, 0x4018, 0x4020)) {
		throw "CPU Test mode is disabled";
	} else if (BETWEEN(addr, 0x4020, 0x10000)) {
		try {
			switch (cart.prg_rom.size() / 0x4000) {
			case 1: return cart.prg_rom.at(addr - 0xC000);
			case 2: return cart.prg_rom.at(addr - 0x8000);
			}
		} catch (std::exception& e) {
			printf("in Memory::at(%x)\n", addr);
			puts(e.what());
			exit(EXIT_FAILURE);
		}

		std::ostringstream ss;
		ss << "invalid cart prg rom size: " << cart.prg_rom.size();
		throw ss.str();
	}

	throw "invalid addr in Memory::at";
}

auto Memory::read(ExtendedAddr addr) -> uint8_t
{
	if (BETWEEN(addr, 0x2000, 0x4000)) {
		addr = (addr % 0x8) + 0x2000;
		return ppu->readRegister(addr);
	} else if (BETWEEN(addr, 0x4000, 0x4018)) {
		if (addr == 0x4014) {
			return ppu->readRegister(addr);
		}
		return 0;
	} else {
		return at(addr);
	}
}

auto Memory::write(ExtendedAddr addr, uint8_t value) -> void
{
	if (BETWEEN(addr, 0x2000, 0x4000)) {
		addr = (addr % 0x8) + 0x2000;
		ppu->writeRegister(addr, value);
	} else if (BETWEEN(addr, 0x4000, 0x4018)) {
		if (addr == 0x4014) {
			ppu->writeRegister(addr, value);
		}
	} else {
		at(addr) = value;
	}
}

ExtPtr::ExtPtr(ExtendedAddr addr, Memory& memory)
	: addr(addr)
	, memory(memory)
{}

auto ExtPtr::read() -> uint8_t 
{
	return memory.read(addr);
}

auto ExtPtr::write(uint8_t value) -> void
{
	memory.write(addr, value);
}

auto ExtPtr::address() -> ExtendedAddr
{
	return addr;
}