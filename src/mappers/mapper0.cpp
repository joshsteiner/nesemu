#include "mapper0.h"

#include <cassert>

static size_t banks_count;
static int banks[2];

uint8_t mapper0_read(Extended_addr addr);
void mapper0_write(Extended_addr addr, uint8_t value);

void init_mapper0(Mapper& mapper)
{
	mapper.read = mapper0_read;
	mapper.write = mapper0_write;

	banks_count = 0;
	banks[0] = 0;
	banks[1] = 0;
}

uint8_t mapper0_read(Extended_addr addr)
{
	switch (addr) {
	case 0x0000 ... 0x1FFF:
		return cart->vrom.at(addr);
	case 0x6000 ... 0x7FFF:
		throw std::invalid_argument{ "save ram read" };
	case 0x8000 ... 0xBFFF:
	{
		auto index = banks[0] * 0x4000 + addr - 0x8000;
		return cart->rom.at(index);
	}
	case 0xC000 ... 0xFFFF:
	{
		auto index = banks[1] * 0x4000 + addr - 0xC000;
		return cart->rom.at(index);
	}
	default:
		throw std::invalid_argument{ std::to_string(addr) };
	}
}

void mapper0_write(Extended_addr addr, uint8_t value)
{
	switch (addr) {
	case 0x0000 ... 0x1FFF:
		cart->vrom.at(addr) = value;
		break;
	case 0x6000 ... 0x7FFF:
		throw std::invalid_argument{ "save ram write" };
	case 0x8000 ... 0xFFFF:
		assert(0);
		banks[0] = value % banks_count; // ???
		break;
	default:
		throw std::invalid_argument{ std::to_string(addr) };
	}
}