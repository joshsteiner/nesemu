#pragma once

#include <memory>

#include "graphics.h"
#include "console/cpu.h"
#include "console/ppu.h"

class Console
{
private:
	Cpu cpu;
	Ppu ppu;
	std::unique_ptr<PixelBuffer> pixels;

public:
	Console(std::unique_ptr<PixelBuffer> pixels)
	{
		this->pixels = std::move(pixels);
		cpu.connect(&ppu);
		ppu.connect(&cpu);
		ppu.connect(this->pixels.get());
	}
};