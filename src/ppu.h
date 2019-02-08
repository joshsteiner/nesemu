#pragma once

#include <cstdint>
#include <array>

#include "nesemu.h"
#include "cpu.h"

const size_t ObjAttrMapSize = 0x100;
const size_t NametableDataSize = 0x800;
const size_t PaletteDataSize = 0x20;

using Status = FlagByte({
	unsigned                 : 5;
	unsigned sprite_overflow : 1;
	unsigned sprite0_hit     : 1;
	unsigned vblank_started  : 1;
});

using Control = FlagByte({
	unsigned base_nt         : 2;
	unsigned vram_incr       : 1;
	unsigned sprite_pt       : 1;
	unsigned background_pt   : 1;
	unsigned big_sprite      : 1;
	unsigned master_slave    : 1;
	unsigned generate_vblank : 1;
});

using Mask = FlagByte({
	unsigned greyscale            : 1;
	unsigned show_left_background : 1;
	unsigned show_left_sprites    : 1;
	unsigned show_background      : 1;
	unsigned show_sprites         : 1;
	unsigned emph_red             : 1;
	unsigned emph_green           : 1;
	unsigned emph_blue            : 1;
});

static_assert(sizeof(Status) == 1);
static_assert(sizeof(Control) == 1);
static_assert(sizeof(Mask) == 1);

struct SpriteAttrObj
{
	uint8_t y;
	uint8_t idx;
	FlagByte({
		unsigned palette    : 2;
		unsigned            : 3;
		unsigned priority   : 1;
		unsigned flip_horiz : 1;
		unsigned flip_vert  : 1;
	}) attr;
	uint8_t x;
};

static_assert(sizeof(SpriteAttrObj) == 4);

enum PpuRegAddr
{
	PpuCtrlRegAddr   = 0x2000,
	PpuMaskRegAddr   = 0x2001,
	PpuStatusRegddr  = 0x2002,
	OamAddrRegAddr   = 0x2003,
	OamDataRegAddr   = 0x2004,
	PpuScrollRegAddr = 0x2005,
	PpuAddrRegAddr   = 0x2006,
	PpuDataRegAddr   = 0x2007,
	OamDmaRegAddr    = 0x4014
};

class Ppu
{
private:
	Cpu& cpu;

	std::array<uint8_t, NametableDataSize> nametable_data;
	std::array<uint8_t, PaletteDataSize> palette_data;

	uint8_t obj_attr_map_addr;
	std::array<uint8_t, ObjAttrMapSize> obj_attr_map;

	Status status;
	Control ctrl;
	Mask mask;

	unsigned cycle;    // 0-340
	unsigned scanline; // 0-261, 0-239=visible, 240=post, 241-260=vblank, 261=pre
	unsigned frame; // frame counter

	struct 
	{
		unsigned v : 15;
		unsigned t : 15;
		unsigned x : 3;
		unsigned w : 1;
		unsigned f : 1;
	};

public:
	auto reset() -> void;

	auto read_register(uint16_t addr) -> uint8_t;
	auto write_register(uint16_t addr, uint8_t value) -> void;
};