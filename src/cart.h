#ifndef NESEMU_CART_H
#define NESEMU_CART_H

#include "common.h"

#include <vector>
#include <fstream>
#include <cstdint>

enum class Mirroring {
	horizontal = 0,
	vertical = 1
};

class Cartridge {
public:
	bool has_battery;
	int mapper;
	Mirroring mirroring;

	std::vector<uint8_t> rom;
	std::vector<uint8_t> vrom;

	static Cartridge from_ines(std::ifstream& file);

private:
	static const size_t header_size = 16;
	static const size_t trainer_size = 0x200;
	static const size_t rom_page_size = 0x4000;
	static const size_t vrom_page_size = 0x2000;

	static constexpr const char* magic_const = "NES\x1A";
};

#endif
