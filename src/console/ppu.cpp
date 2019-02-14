#include "ppu.h"

auto Ppu::reset() -> void
{
	// TODO: use constants
	cycle = 340;
	scanline = 240;
	frame = 0;
	write_register(PpuCtrlRegAddr, 0);
	write_register(PpuMaskRegAddr, 0);
	write_register(OamAddrRegAddr, 0);
}

auto Ppu::step() -> void
{

}

auto Ppu::read_register(uint16_t addr) -> uint8_t
{
	// TODO: mirroring
	if (addr >= 0x2000 && addr < 0x4000) {
		addr = (addr % 8) + 0x2000;
	}

	switch (addr) {
	case PpuStatusRegddr:
	{
		// TODO: NMI change?
		auto result = status.raw;
		status.vblank_started = 0;
		w = 0;
		return result;
	}
	case OamDataRegAddr:
		return obj_attr_map[obj_attr_map_addr];
	case PpuDataRegAddr:
	{
		// TODO: ppudata
		uint8_t value = 0; // TODO ppu::read(v)
		if (v % 0x4000 < 0x3F00) {
			std::swap(value, buffered);
		}
		else {
			buffered = 0; // ppu::read(v - 0x1000)
		}

		v += ctrl.vram_incr ? 32 : 1;

		return value;
	}
	}
}

auto Ppu::write_register(uint16_t addr, uint8_t value) -> void 
{
	// TODO: mirroring
	if (addr >= 0x2000 && addr < 0x4000) {
		addr = (addr % 8) + 0x2000;
	}

	switch (addr) {
	case PpuCtrlRegAddr:
		// TODO: change NMI
		ctrl.raw = value;
		t = (t & 0xF3FF) | ((value & 0x03) << 10);
		break;
	case PpuMaskRegAddr:
		mask.raw = value;
		break;
	case OamAddrRegAddr:
		obj_attr_map_addr = value;
		break;
	case OamDataRegAddr:
		obj_attr_map[obj_attr_map_addr++] = value;
		break;
	case PpuScrollRegAddr:
		if (w) {
			t = (t & 0x8FFF) | ((value & 0x07) << 12);
			t = (t & 0xFC1F) | ((value & 0xF8) << 2);
		} else {
			t = (t & 0xFFE0) | (value >> 3);
			x = value & 0x07;
		}
		w = !w;
		break;
	case PpuAddrRegAddr:
		if (w) {
			t = (t & 0xFF00) | value;
			v = t;
		} else {
			t = (t & 0x80FF) | ((value & 0x3F) << 8);
		}
		w = !w;
		break;
	case PpuDataRegAddr:
		// TODO: ppudata
		// ppu::write(v, value)
		v += ctrl.vram_incr ? 32 : 1;
		break;
	case OamDmaRegAddr:
	{
		uint16_t addr = value << 8;
		for (int i = 0; i < ObjAttrMapSize; i++) {
			obj_attr_map[obj_attr_map_addr++] = cpu->memory[addr++];
		}
		cpu->stall(cpu->cycle % 2 ? 514 : 513);
		break;
	}
	}
}