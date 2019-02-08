#include "ppu.h"

auto Ppu::reset() -> void
{
	// TODO: cycle, scanline, frame
	write_register(PpuCtrlRegAddr, 0);
	write_register(PpuMaskRegAddr, 0);
	write_register(OamAddrRegAddr, 0);
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
		// TODO: ppudata
		return 0;
	}
}

/*
func (ppu *PPU) readData() byte {
	value := ppu.Read(ppu.v)
	// emulate buffered reads
	if ppu.v%0x4000 < 0x3F00 {
		buffered := ppu.bufferedData
		ppu.bufferedData = value
		value = buffered
	} else {
		ppu.bufferedData = ppu.Read(ppu.v - 0x1000)
	}
	// increment address
	if ppu.flagIncrement == 0 {
		ppu.v += 1
	} else {
		ppu.v += 32
	}
	return value
}

*/

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
		break;
	case OamDmaRegAddr:
	{
		uint16_t addr = value << 8;
		for (int i = 0; i < OamMapSize; i++) {
			obj_attr_map[obj_attr_map_addr++] = cpu.memory[addr++];
		}
		// TODO: stall cpu
		break;
	}
	}
}