#pragma once

#include <cstdint>
#include <array>

#include "common.h"
#include "cpu.h"
#include "memory.h"
#include "../graphics.h"

class Cpu;
class Memory;

class Ppu
{
private:
	static const size_t ObjAttrMapSize = 0x100;
	static const size_t NametableDataSize = 0x800;
	static const size_t PaletteDataSize = 0x20;

private:
	using Status = FLAG_BYTE({
		unsigned sprite_overflow : 1;
		unsigned sprite0_hit     : 1;
		unsigned vblank_started  : 1;
	});

	using Control = FLAG_BYTE({
		unsigned base_nt         : 2;
		unsigned vram_incr       : 1;
		unsigned sprite_pt       : 1;
		unsigned background_pt   : 1;
		unsigned big_sprite      : 1;
		unsigned master_slave    : 1;
		unsigned generate_vblank : 1;
	});

	using Mask = FLAG_BYTE({
		unsigned greyscale            : 1;
		unsigned show_left_background : 1;
		unsigned show_left_sprites    : 1;
		unsigned show_background      : 1;
		unsigned show_sprites         : 1;
		unsigned emph_red             : 1;
		unsigned emph_green           : 1;
		unsigned emph_blue            : 1;
	});

	union VramRegister
	{
		unsigned raw : 15;
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

	struct SpriteAttrObj
	{
		uint8_t y;
		uint8_t idx;
		FLAG_BYTE({
			unsigned palette    : 2;
			unsigned            : 3;
			unsigned priority   : 1;
			unsigned flip_horiz : 1;
			unsigned flip_vert  : 1;
		}) attr;
		uint8_t x;
	};

	union ObjAttrMap
	{
		uint8_t raw[ObjAttrMapSize];
		SpriteAttrObj obj[ObjAttrMapSize / 4];
	};

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

	enum PpuState
	{
		PreRender, Render, PostRender, VBlank
	};

private:
	Cpu* cpu;
	Memory* memory;
	PixelBuffer* pixel_buffer;

	std::array<uint8_t, NametableDataSize> nametable_data;
	std::array<uint8_t, PaletteDataSize> palette_data;

	uint8_t obj_attr_map_addr;
	ObjAttrMap obj_attr_map;

	std::vector<SpriteAttrObj> scanline_sprites;

	Status status;
	Control ctrl;
	Mask mask;

	unsigned cycle;    
	unsigned scanline;
	unsigned frame; 
	PpuState ppu_state;

	uint8_t buffered;

	VramRegister vram_addr;
	VramRegister tmp_vram_addr;
	uint8_t fine_x;
	bool write;

public:
	inline auto connect(Cpu* cpu) -> void { this->cpu = cpu; }
	inline auto connect(Memory* memory) -> void { this->memory = memory; }
	inline auto connect(PixelBuffer* pixel_buffer) -> void { this->pixel_buffer = pixel_buffer; }

	auto reset() -> void;
	auto step() -> void;

	auto read_register(uint16_t addr) -> uint8_t;
	auto write_register(uint16_t addr, uint8_t value) -> void;

private:
	auto incr_x() -> void;
	auto incr_y() -> void;
	auto copy_x() -> void;
	auto copy_y() -> void;

	auto tile_addr() -> uint16_t;
	auto attr_addr() -> uint16_t;

	auto palette_at(uint16_t addr) -> uint8_t&;

	auto step_pre_render() -> void;
	auto step_post_render() -> void;
	auto step_render() -> void;
	auto step_vblank() -> void;

	auto eval_sprites() -> void;
};