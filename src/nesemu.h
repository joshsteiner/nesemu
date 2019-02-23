#pragma once

#include <memory>

#include "graphics.h"
#include "console/cpu.h"
#include "console/ppu.h"
#include "console/memory.h"

/*
  Memory( Cpu*, Ppu* )
  Cpu( Memory* )
  Ppu( Memory*, Cpu*, Gui* )
  Gui( Memory* )
*/

class Console
{
public:
	Cpu cpu;
	Ppu ppu;
	Memory memory;
	std::unique_ptr<PixelBuffer> pixels;

public:
	Console(std::unique_ptr<PixelBuffer> pixels = nullptr)
	{
		this->pixels = std::move(pixels);

		memory.connect(&cpu);
		memory.connect(&ppu);

		cpu.connect(&memory);

		ppu.connect(&cpu);
		ppu.connect(&memory);
		ppu.connect(this->pixels.get());
	}

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