#ifndef NESEMU_PPU_H
#define NESEMU_PPU_H

#include "screen.h"
#include "memory.h"
#include "cpu.h"

class Memory;
class Cpu;

const std::array<Color, 64> palette = {
	0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00,
	0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
	0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
	0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
	0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22,
	0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
	0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
	0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000
};

enum class Scanline_type {
	visible, post, nmi, pre 
};

union Vram_register {
	unsigned raw : 15;
	unsigned addr : 14;
	struct {
		unsigned low : 8;
		unsigned hi  : 7;
	};
	struct {
		unsigned coarse_x  : 5;
		unsigned coarse_y  : 5;
		unsigned nt_select : 2;
		unsigned fine_y    : 3;
	};
};

using Bg_attr = FLAG_BYTE({
	unsigned top_left     : 2;
	unsigned top_right    : 2;
	unsigned bottom_left  : 2;
	unsigned bottom_right : 2;
});

class Name_table {
	// TODO: mirroring
public:
	uint8_t& at(unsigned idx) 
	{ 
		return data.at(idx); 
	}

	uint8_t get_byte(unsigned addr) const
	{
		if (!BETWEEN(addr, 0, 960)) {
			throw std::invalid_argument{ "" };
		}
		return data.at(addr);
	}
private:
	std::array<uint8_t, 0x400> data;
};

class Palette_table {
public:
	uint8_t& at(unsigned idx) { return data.at(idx); }
private:
	std::array<uint8_t, 0x20> data;
};

using Control = FLAG_BYTE({
	unsigned nametable        : 2;
	unsigned increment        : 1;
	unsigned sprite_table     : 1;
	unsigned background_table : 1;
	unsigned sprite_size      : 1;
	unsigned master_slave     : 1;
	unsigned nmi_output       : 1;
});

using Mask = FLAG_BYTE({
	unsigned grayscale            : 1;
	unsigned show_left_background : 1;
	unsigned show_left_sprites    : 1;
	unsigned show_background      : 1;
	unsigned show_sprites         : 1;
	unsigned red_tint             : 1;
	unsigned green_tint           : 1;
	unsigned blue_tint            : 1;
});

using Status = FLAG_BYTE({
	unsigned filler          : 5;
	unsigned sprite_zero_hit : 1;
	unsigned sprite_overflow : 1;
	unsigned nmi_occurred    : 1;
});

struct Sprite_obj {
	uint8_t y;
	uint8_t index;
	FLAG_BYTE({
		unsigned palette           : 2;
		unsigned                   : 3;
		unsigned behind_background : 1;
		unsigned flip_horizontal   : 1;
		unsigned flip_vertical     : 1;
	}) attr;
	uint8_t x;
	uint8_t dataL, dataH;
	uint8_t id;
};

template <size_t Sz>
struct Sprite_data {
	static const size_t size = Sz;
	Sprite_obj sprites[Sz];

	uint8_t& at(size_t idx)
	{
		idx %= (Sz * 4);
		switch (idx % 4) {
		case 0: return sprites[idx / 4].y;
		case 1: return sprites[idx / 4].index;
		case 2: return sprites[idx / 4].attr.raw;
		case 3: return sprites[idx / 4].x;
		}
	}

	Sprite_obj& sprite_at(size_t idx)
	{
		idx %= Sz;
		return sprites[idx];
	}

	void clear();
};

class Ppu {
public:
	unsigned cycle = 0;

	Ppu();

	void reset();
	void step();
	uint8_t read_register(uint16_t address);
	void write_register(uint16_t address, uint8_t value);
	uint8_t read(uint16_t addr);
	void write(uint16_t addr, uint8_t value);

private:
	unsigned scan_line = 0;
	unsigned dot = 0;
	unsigned frame = 0;

	Palette_table palette_data;
	std::array<uint8_t, 0x800> nametable_data;

	std::array<uint8_t, 0x100> oam_data;
	Sprite_data<8> primary_oam, secondary_oam;

	// PPU registers
	Vram_register v;
	Vram_register t;

	uint8_t x = 0;
	uint8_t w = 0;
	uint8_t f = 0;

	uint8_t reg = 0;

	// NMI flags
	bool nmi_previous = false;
	uint8_t nmi_delay = 0;

	// background temporary variables
	uint8_t nametable_byte = 0;
	uint8_t attributetable_byte = 0;
	uint8_t low_tile_byte = 0;
	uint8_t high_tile_byte = 0;

	// Background shift registers:
	// TODO: rename or remove these
	uint8_t attribute_shift_low;
	uint8_t attribute_shift_high; 
	uint16_t background_shift_low;
	uint16_t background_shift_high;
	bool attribute_latch_low; 
	bool attribute_latch_high;

	Control control;
	Mask mask;
	Status status;

	uint8_t oam_address = 0;
	uint8_t buffered_data = 0;

	void incr_x();
	void incr_y();
	void copy_x();
	void copy_y();

	bool rendering();
	int spr_height();

	uint16_t nt_addr();
	uint16_t at_addr();
	uint16_t bg_addr();

	void reload_shift();
	void eval_sprites();
	void load_sprites();
	void pixel();
	void scanline_cycle(Scanline_type scanline_type);
};

extern Cpu cpu;
extern Memory memory;

#endif
