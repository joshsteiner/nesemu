#include <cassert>
#include <sstream>

#include "memory.h"
#include "common.h"

auto Memory::at(ExtendedAddr addr) -> uint8_t&
{
	if (addr < 0) {
		switch (addr) {
		case ARegisterExtAddr: return cpu.a;
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
		return ppu.readRegister(addr);
	} else if (BETWEEN(addr, 0x4000, 0x4018)) {
		if (addr == 0x4014) {
			return ppu.readRegister(addr);
		}
		return 0;
	} else {
		return at(addr);
	}
}

auto Memory::write(ExtendedAddr addr, uint8_t value) -> void
{
	std::clog << "in  Memory::write(" << std::hex << addr << "," << (int)value << ")\n";
	if (BETWEEN(addr, 0x2000, 0x4000)) {
		std::clog << "+ 1\n";
		addr = (addr % 0x8) + 0x2000;
		std::clog << "+ 2\n";
		ppu.writeRegister(addr, value);
		std::clog << "+ 3\n";
	} else if (BETWEEN(addr, 0x4000, 0x4018)) {
		std::clog << "+ 4\n";
		if (addr == 0x4014) {
			std::clog << "+ 5\n";
			ppu.writeRegister(addr, value);
			std::clog << "+ 6\n";
		}
	} else {
		std::clog << "+ 7\n";
		at(addr) = value;
		std::clog << "+ 8\n";
	}
	std::clog << "out Memory::write\n";
}
