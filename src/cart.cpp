#include "cart.h"
#include "mappers/mapper0.h"

Cartridge* cart = nullptr;

static void choose_mapper(Mapper& mapper, uint8_t number)
{
	switch (number) {
	case 0:
		init_mapper0(mapper);
		break;
	default:
		throw "unimplemented";
	}
}

Cartridge* Cartridge::from_ines(std::ifstream& file)
{
	file.exceptions(std::ios::failbit | std::ios::badbit);

	for (int i = 0; i < 4; i++) {
		if (file.get() != magic_const[i]) {
			throw std::invalid_argument{ "invalid rom" };
		}
	}

	auto* cart = new Cartridge;

	size_t rom_size = file.get() * rom_page_size;
	size_t vrom_size = file.get() * vrom_page_size;
	uint8_t flag6 = file.get();
	uint8_t flag7 = file.get();

	cart->mirroring = flag6 & BIT(0) ? Mirroring::vertical : Mirroring::horizontal;
	choose_mapper(cart->mapper, (flag7 & 0xF0) | (flag6 >> 4));
	cart->has_battery = flag6 & BIT(1);

	bool has_trainer = flag6 & BIT(2);

	// skip rest of header
	file.seekg(header_size, std::ifstream::beg);

	// skip trainer
	if (has_trainer) {
		file.seekg(trainer_size, std::ifstream::cur);
	}

	// read prg_rom
	cart->rom.reserve(rom_size);
	for (size_t i = 0; i < rom_size; i++) {
		cart->rom.push_back(file.get());
	}

	// read chr_rom
	cart->vrom.reserve(vrom_size);
	for (size_t i = 0; i < vrom_size; i++) {
		cart->vrom.push_back(file.get());
	}

	return cart;
}

uint8_t Cartridge::read(Extended_addr addr)
{
	return mapper.read(addr);
}

void Cartridge::write(Extended_addr addr, uint8_t value)
{
	mapper.write(addr, value);
}