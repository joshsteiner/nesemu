#include "memory.h"
#include "common.h"

#include <cassert>
#include <sstream>

uint8_t& Memory::at(Extended_addr addr)
{
	if (addr < 0) {
		switch (addr) {
		case a_register_ext_addr: 
			return cpu.a;
		}
	} else if (BETWEEN(addr, 0x0000, 0x2000)) {
		return ram.at(addr % 0x800);
	} else if (BETWEEN(addr, 0x2000, 0x4000)) {
		throw "use either read or write";
	} else if (BETWEEN(addr, 0x4000, 0x4018)) {
		// TODO: APU/IO registers
		throw "apu/io register r/w";
	} else if (BETWEEN(addr, 0x4018, 0x4020)) {
		throw "CPU Test mode is disabled";
	} else if (BETWEEN(addr, 0x4020, 0x10000)) {
		try {
			switch (cart.rom.size() / 0x4000) {
			case 1: 
				return cart.rom.at(addr - 0xC000);
			case 2: 
				return cart.rom.at(addr - 0x8000);
			}
		} catch (std::exception& e) {
			logger << "in Memory::at(" << std::hex << addr << "\n";
			logger << e.what() << "\n";
			exit(EXIT_FAILURE);
		}

		std::ostringstream ss;
		ss << "invalid cart prg rom size: " << cart.rom.size();
		throw ss.str();
	}

	throw "invalid addr in Memory::at";
}

uint8_t Memory::read(Extended_addr addr)
{
	if (BETWEEN(addr, 0x2000, 0x4000)) {
		addr = (addr % 0x8) + 0x2000;
		return ppu.read_register(addr);
	} else if (BETWEEN(addr, 0x4000, 0x4018)) {
		if (addr == 0x4014) {
			return ppu.read_register(addr);
		}
		return 0;
	} else {
		return at(addr);
	}
}

void Memory::write(Extended_addr addr, uint8_t value)
{
	logger << "in  Memory::write(" << std::hex << addr << "," << (int)value << ")\n";
	if (BETWEEN(addr, 0x2000, 0x4000)) {
		addr = (addr % 0x8) + 0x2000;
		ppu.write_register(addr, value);
	} else if (BETWEEN(addr, 0x4000, 0x4018)) {
		if (addr == 0x4014) {
			ppu.write_register(addr, value);
		}
	} else {
		at(addr) = value;
	}
}
