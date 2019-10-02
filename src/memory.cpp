#include "memory.h"
#include "common.h"
#include "controller.h"

#include <cassert>
#include <sstream>

uint8_t Memory::read(Extended_addr addr)
{
	switch (addr) {
	case a_register_ext_addr: 
		return cpu.a;
	case 0 ... 0x1FFF:        
		return ram.at(addr % 0x800);
	case 0x2000 ... 0x3FFF:   
		return ppu.read_register(0x2000 + addr % 8);
	case 0x4014:              
		return ppu.read_register(addr);
	case 0x4015:   
		LOG_FMT("read from unimplemented memory region %04X", addr); 
		return 0;
	case 0x4016:
		return controllers.read_state(0);
	case 0x4017:
		return controllers.read_state(1);
	case 0x6000 ... 0xFFFF:   
		return cart->read(addr);
	default:
		throw std::invalid_argument{ std::to_string(addr) };
	}
}

void Memory::write(Extended_addr addr, uint8_t value)
{
	switch (addr) {
	case a_register_ext_addr:
		cpu.a = value;
		break;
	case 0 ... 0x1FFF:
		ram.at(addr % 0x800) = value;
		break;
	case 0x2000 ... 0x3FFF:
		addr = 0x2000 + addr % 0x8;
		ppu.write_register(addr, value);
		break;
	case 0x4000 ... 0x4013:
		LOG("APU register write");
		break;
	case 0x4014:
		ppu.write_register(addr, value);
		break;
	case 0x4015:
	case 0x4017:
		LOG("APU register write");
		break;
	case 0x4016:
		controllers.write_strobe(value & 1);
		break;
	case 0x4018 ... 0x5FFF:
		// I/O registers
		break;
	case 0x6000 ... 0xFFFF:
		cart->write(addr, value);
		break;
	default:
		throw std::invalid_argument{ std::to_string(addr) };
	}
}

uint16_t Memory::read_addr(Extended_addr addr)
{
	return read(addr) + 0x100 * read(addr + 1);
}
