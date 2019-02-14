#include "common.h"
#include "cart.h"

const size_t HeaderSize = 16;
const size_t TrainerSize = 0x200;
const size_t PrgRomPageSize = 0x4000;
const size_t ChrRomPageSize = 0x2000;

const char *MagicConst = "NES\x1A";

auto Cartridge::from_ines(std::ifstream& file) -> Cartridge
{
	int i = 0;
	
	if (!file.is_open()) {
		throw "file error";
	}

	for (; i < 4; i++) {
		if (file.get() != MagicConst[i]) {
			throw "invalid cart";
		}
	}

	Cartridge cart;

	size_t prg_rom_size = file.get() * PrgRomPageSize;
	size_t chr_rom_size = file.get() * ChrRomPageSize;
	uint8_t flag6 = file.get();
	uint8_t flag7 = file.get();

	cart.mirroring   = flag6 & bit(0); 
	cart.mapper      = (flag7 & 0xF0) | (flag6 >> 4); 
	cart.has_battery = flag6 & bit(1);

	bool hasTrainer = flag6 & bit(2);

	// skip rest of header
	file.seekg(HeaderSize, std::ifstream::beg);

	// skip trainer
	if (hasTrainer) {
		file.seekg(TrainerSize, std::ifstream::cur);
	}

	// read prg_rom
	cart.prg_rom.reserve(prg_rom_size);
	for (int j = 0; j < prg_rom_size; j++) {
		auto c = file.get();
		cart.prg_rom.push_back(c);
	}

	// read chr_rom
	cart.chr_rom.reserve(chr_rom_size);
	for (int j = 0; j < chr_rom_size; j++) {
		auto c = file.get();
		cart.chr_rom.push_back(c);
	}

	return cart;
}
