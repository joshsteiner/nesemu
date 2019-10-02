#ifndef NESEMU_CART_H
#define NESEMU_CART_H

#include "common.h"

#include <vector>
#include <fstream>
#include <cstdint>
#include <memory>

const size_t header_size = 16;
const size_t trainer_size = 0x200;
const size_t rom_page_size = 0x4000;
const size_t vrom_page_size = 0x2000;

constexpr const char* magic_const = "NES\x1A";

enum class Mirroring {
	horizontal = 0,
	vertical = 1
};

class Mapper {
public:
	virtual uint8_t read(Extended_addr addr) = 0;
	virtual void write(Extended_addr addr, uint8_t value) = 0;

	static Mapper* choose(uint8_t number);
};

class Cartridge {
public:
	bool has_battery;
	Mirroring mirroring;

	std::vector<uint8_t> rom;
	std::vector<uint8_t> vrom;

	static Cartridge* from_ines(std::ifstream& file);

	uint8_t read(Extended_addr addr);
	void write(Extended_addr addr, uint8_t value);
private:
	std::unique_ptr<Mapper> mapper;
};

extern Cartridge* cart;

#endif
