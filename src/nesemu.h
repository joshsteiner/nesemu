#pragma once

#include <memory>

#include "screen.h"
#include "console/cpu.h"
#include "console/ppu.h"
#include "console/memory.h"

class Console
{
public:
	auto step() -> void
	{
		auto cpu_cycles = cpu.step();
		for (unsigned i = 0; i < cpu_cycles; i++) {
			ppu.step();
		}
	}

	auto load(std::string path) -> void
	{
		std::ifstream file{ path, std::ios::binary };
		if (!file.is_open()) { throw "file error"; }
		auto cart = Cartridge::from_ines(file);
		memory.connect(cart);
		cpu.reset();
		ppu.Reset();
	}
};