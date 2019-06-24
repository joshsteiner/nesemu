#ifndef NESEMU_PPU_H
#define NESEMU_PPU_H

#include "screen.h"
#include "memory.h"
#include "cpu.h"

class Memory;
class Cpu;

class Ppu {
private:
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
		unsigned filler          : 7;
		unsigned sprite_zero_hit : 1;
		unsigned sprite_overflow : 1;
		unsigned nmi_occurred    : 1;
	});

public:
	Ppu();

	void reset();
	void step();
	uint8_t read_register(uint16_t address);
	void write_register(uint16_t address, uint8_t value);
	uint8_t read(uint16_t addr);
	void write(uint16_t addr, uint8_t value);

private:
	unsigned cycle = 0;
	unsigned scan_line = 0;
	unsigned frame = 0;

	std::array<uint8_t, 32>     palette_data;
	std::array<uint8_t, 0x1000> nametable_data;
	std::array<uint8_t, 256>    oam_data;

	std::array<Color, 64> palette = {
		0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00,
		0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
		0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
		0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
		0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22,
		0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
		0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
		0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000,
	};

	// PPU registers
	uint16_t v = 0;
	uint16_t t = 0;
	uint8_t x = 0;
	uint8_t w = 0;
	uint8_t f = 0;

	uint8_t reg = 0;

	// NMI flags
	bool nmi_occurred = false;
	bool nmi_output = false;
	bool nmi_previous = false;
	uint8_t nmi_delay = 0;

	// background temporary variables
	uint8_t nametable_byte = 0;
	uint8_t attributetable_byte = 0;
	uint8_t low_tile_byte = 0;
	uint8_t high_tile_byte = 0;
	uint64_t tile_data = 0;

	// sprite temporary variables
	unsigned sprite_count = 0;
	std::array<uint32_t, 8> sprite_patterns;
	std::array<uint8_t, 8> sprite_positions;
	std::array<uint8_t, 8> sprite_priorities;
	std::array<uint8_t, 8> sprite_indexes;

	Control control;
	Mask mask;
	Status status;

	// $2003 OAMADDR
	uint8_t oam_address = 0;

	// $2007 PPUDATA
	uint8_t buffered_data = 0; // for buffered reads

	uint8_t& palette_at(uint16_t addr);

	void incr_x();
	void incr_y();
	void copy_x();
	void copy_y();

	void nmi_change();
	void set_vertical_blank();
	void clear_vertical_blank();
	void fetch_nametable_byte();
	void fetch_attribute_table_byte();
	void fetch_low_tile_byte();
	void fetch_high_tile_byte();
	void store_tile_data();
	uint32_t fetch_tile_data();
	uint8_t background_pixel();
	std::pair<uint8_t, uint8_t> sprite_pixel();
	void render_pixel();
	uint32_t fetch_sprite_pattern(int i, int row);
	void evaluate_sprites();
	void tick();
};

extern Cpu cpu;
extern Memory memory;

#endif
