#pragma once

#include <vector>
#include <fstream>
#include <cstdint>

struct Cartridge 
{
	bool has_battery;
	int mapper;
	int mirroring;
	
	std::vector<uint8_t> prg_rom;
	std::vector<uint8_t> chr_rom;

	static auto from_ines(std::ifstream& file) -> Cartridge;
};
