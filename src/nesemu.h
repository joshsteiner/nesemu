#ifndef NESEMU_CONSOLE_H
#define NESEMU_CONSOLE_H

#include "screen.h"
#include "cpu.h"
#include "ppu.h"
#include "memory.h"

#include <memory>

class Console
{
public:
	void step()
	{
		auto cpu_cycles = cpu.step();
		for (unsigned i = 0; i < cpu_cycles; i++) {
			ppu.step();
		}
	}

	void load(std::string path)
	{
		std::ifstream file{ path, std::ios::binary };
		if (!file.is_open()) { 
			throw "file error"; 
		}
		auto cart = Cartridge::from_ines(file);
		memory.connect(cart);
		cpu.reset();
		ppu.reset();
	}
};

#endif
