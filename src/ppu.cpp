#include "ppu.h"

Ppu ppu;

uint16_t nt_mirror(uint16_t addr);


Ppu::Ppu()
{
	reset();
}

void Ppu::reset()
{
	cycle = 340;
	scan_line = 240;
	frame = 0;
	control.raw = 0;
	mask.raw = 0;
	oam_address = 0;
	w = 0;
	dot = 0;
	f = 0;
}

uint8_t Ppu::read(uint16_t addr)
{
	addr %= 0x4000;

	switch(addr) {
	case 0 ... 0x1FFF:
		return cart->vrom.at(addr);
	case 0x2000 ... 0x3EFF:
		return nametable_data.at(nt_mirror(addr));
	case 0x3F00 ... 0x3FFF:
		if ((addr & 0x13) == 0x10) {
			addr &= ~0x10;
		}
		return palette_data.at(addr & 0x1F) & (mask.grayscale ? 0x30 : 0xFF);
	default:
		return 0;
	}
}

void Ppu::write(uint16_t addr, uint8_t value)
{
	addr %= 0x4000;

	switch (addr) {
	case 0 ... 0x1FFF:
		cart->vrom.at(addr) = value;
		break;
	case 0x2000 ... 0x3EFF:
		nametable_data.at(nt_mirror(addr)) = value;
		break;
	case 0x3F00 ... 0x3FFF:
		if ((addr & 0x13) == 0x10) {
			addr &= ~0x10;
		}
		palette_data.at(addr & 0x1F) = value;
		break;
	}
}

uint8_t Ppu::read_register(uint16_t address)
{
	switch (address) {
	case 0x2002:
	{
		status.filler = reg;
		auto result = status.raw;
		status.nmi_occurred = 0;
		w = 0;
		return result;
	}
	case 0x2004:
		return oam_data.at(oam_address);
	case 0x2007:
	{
		auto value = read(v.raw);
		if (v.raw % 0x4000 < 0x3F00) {
			std::swap(buffered_data, value);
		} else {
			buffered_data = memory.read(v.raw - 0x1000);
		}
		v.raw += (control.increment == 0) ? 1 : 32;
		return value;
	}
	}
	return 0;
}

void Ppu::write_register(uint16_t address, uint8_t value)
{
	reg = value;
	switch (address) {
	case 0x2000:
		control.raw = value;
		t.nt_select = value;
		break;
	case 0x2001:
		mask.raw = value;
		break;
	case 0x2003:
		oam_address = value;
		break;
	case 0x2004:

		oam_data.at(oam_address) = value;
		++oam_address;
		break;
	case 0x2005:
		if (w == 0) {
			t.coarse_x = value >> 3;
			x = value & 0x7;
			w = 1;
		} else {
			t.coarse_y = value >> 3;
			t.fine_y = value & 0x7;
			w = 0;
		}
		break;
	case 0x2006:
		if (w == 0) {
			t.hi = value;
			w = 1;
		} else {
			t.low = value;
			v.raw = t.raw;
			w = 0;
		}
		break;
	case 0x2007:
		LOG_FMT("Ppu::writeData : v=%X", v.raw);
		write(v.raw, value);
		v.raw += (control.increment == 0) ? 1 : 32;
		break;
	case 0x4014:
	{
		auto address = value << 8;
		for (auto i = 0; i < 256; ++i) {
			oam_data.at(oam_address) = memory.read(address);
			++oam_address;
			++address;
		}
		cpu.stall(cpu.cycle % 2 == 1 ? 514 : 513);
		break;
	}
	}
}

void Ppu::incr_x()
{
	if (!rendering()) {
		return;
	}
	++v.coarse_x;
	if (v.coarse_x == 0) {
		v.nt_select ^= BIT(0);
	}
}

void Ppu::incr_y()
{
	if (!rendering()) {
		return;
	}
	++v.fine_y;
	if (v.fine_y == 0) {
		++v.coarse_y;
		if (v.coarse_y == 30) {
			v.coarse_y = 0;
			v.nt_select = BIT(1);
		}
	}
}

void Ppu::copy_x()
{
	if (!rendering()) {
		return;
	}
	v.coarse_x = t.coarse_x;
	v.nt_select &= BIT(0);
	v.nt_select |= t.nt_select & BIT(0);
}

void Ppu::copy_y()
{
	if (!rendering()) {
		return;
	}
	v.coarse_y = t.coarse_y;
	v.fine_y = t.fine_y;
	v.nt_select &= BIT(1);
	v.nt_select |= t.nt_select & BIT(1);
}

bool Ppu::rendering()
{
	return mask.show_background || mask.show_sprites;
}

int Ppu::spr_height()
{
	return control.sprite_size ? 16 : 8;
}

/* Get CIRAM address according to mirroring */
uint16_t nt_mirror(uint16_t addr)
{
	switch (cart->mirroring) {
	case Mirroring::vertical:
		return addr % 0x800;
	case Mirroring::horizontal:
		return ((addr / 2) & 0x400) + (addr % 0x400);
	default:
		return addr - 0x2000;
	}
}

/* Calculate graphics addresses */
// TODO: rename these
uint16_t Ppu::nt_addr()
{
	return 0x2000 | (v.raw & 0xFFF);
}

uint16_t Ppu::at_addr()
{
	return 0x23C0 | (v.nt_select << 10) | ((v.coarse_y / 4) << 3) | (v.coarse_x / 4);
}

uint16_t Ppu::bg_addr()
{
	return (control.background_table * 0x1000) + (nametable_byte * 16) + v.fine_y;
}

/* Put new data into the shift registers */
void Ppu::reload_shift()
{
	background_shift_low = (background_shift_low & 0xFF00) | low_tile_byte;
	background_shift_high = (background_shift_high & 0xFF00) | high_tile_byte;

	attribute_latch_low = (attributetable_byte & 1);
	attribute_latch_high = (attributetable_byte & 2);
}

/* Clear secondary OAM */
template <size_t Sz>
void Sprite_data<Sz>::clear()
{
	for (auto& sprite : sprites) {
		sprite.y = 0xFF;
		sprite.id = 64;
		sprite.y = 0xFF;
		sprite.index = 0xFF;
		sprite.attr.raw = 0xFF;
		sprite.x = 0xFF;
		sprite.dataL = 0;
		sprite.dataH = 0;
	}
}


/* Fill secondary OAM with the sprite infos for the next scanline */
void Ppu::eval_sprites()
{
	int n = 0;
	for (int i = 0; i < 64; ++i) {
		int line = (scan_line == 261 ? -1 : scan_line) - oam_data[i * 4 + 0];
		// If the sprite is in the scanline, copy its properties into secondary OAM:
		if (line >= 0 && line < spr_height()) {
			auto& sprite = secondary_oam.sprite_at(n);

			sprite.id       = i;
			sprite.y        = oam_data[i * 4 + 0];
			sprite.index    = oam_data[i * 4 + 1];
			sprite.attr.raw = oam_data[i * 4 + 2];
			sprite.x        = oam_data[i * 4 + 3];

			++n;
			if (n > 8) {
				status.sprite_overflow = true;
				break;
			}
		}
	}
}

/* Load the sprite info into primary OAM and fetch their tile data. */
void Ppu::load_sprites()
{
	uint16_t addr;
	for (int i = 0; i < 8; ++i) {
		auto& sprite = primary_oam.sprite_at(i) = secondary_oam.sprite_at(i);  // Copy secondary OAM into primary.

		// Different address modes depending on the sprite height:
		if (spr_height() == 16) {
			addr = ((sprite.index & 1) * 0x1000) + ((sprite.index & ~1) * 16);
		} else {
			addr = (control.sprite_table * 0x1000) + (sprite.index  * 16);
		}

		unsigned sprY = (scan_line - sprite.y) % spr_height();  // Line inside the sprite.
		if (sprite.attr.flip_vertical) {
			sprY ^= spr_height() - 1;	  // Vertical flip.
		}
		addr += sprY + (sprY & 8);  // Select the second tile if on 8x16.

		sprite.dataL = read(addr + 0);
		sprite.dataH = read(addr + 8);
	}
}

// tmp
static inline int NTH_BIT(int x, int n)
{
	return (x >> n) & 1;
}


/* Process a pixel, draw it if it's on screen */
void Ppu::pixel()
{
	uint8_t palette_nr = 0;
	uint8_t obj_palette_nr = 0;
	bool objPriority = 0;
	int x_ = dot - 2;

	if (scan_line < 240 && x_ >= 0 && x_ < 256) {
		// Background:
		if (mask.show_background && !(!mask.show_left_background && x_ < 8)) {
			palette_nr = (NTH_BIT(background_shift_high, 15 - x) << 1)
				| NTH_BIT(background_shift_low, 15 - x);
			if (palette_nr) {
				palette_nr |= ((NTH_BIT(attribute_shift_high,  7 - x) << 1)
					| NTH_BIT(attribute_shift_low,  7 - x)) << 2;
			}
		}
		// Sprites:
		if (mask.show_sprites && !(!mask.show_left_sprites && x_ < 8))
			for (int i = 7; i >= 0; --i) {
				auto& sprite = primary_oam.sprite_at(i);

				if (sprite.id == 64) {
					continue;
				}
				unsigned sprX = x_ - sprite.x;
				if (sprX >= 8) {  // Not in range.
					continue;
				}
				if (sprite.attr.flip_horizontal) {
					sprX ^= 7;
				}
				uint8_t sprPalette = (NTH_BIT(sprite.dataH, 7 - sprX) << 1)
					| NTH_BIT(sprite.dataL, 7 - sprX);
				if (sprPalette == 0) {  // Transparent pixel.
					continue;
				}

				if (sprite.id == 0 && palette_nr && x_ != 255) {
					status.sprite_zero_hit = 1;
				}
				sprPalette |= (sprite.attr.raw & 3) << 2;
				obj_palette_nr  = sprPalette + 16;
				objPriority = sprite.attr.raw & 0x20;
			}
		// Evaluate priority:
		if (obj_palette_nr && (palette_nr == 0 || objPriority == 0)) {
			palette_nr = obj_palette_nr;
		}

		screen.set_bg(scan_line, x_, palette.at(read(0x3F00 + (rendering() ? palette_nr : 0))));
	}
	// Perform background shifts:
	background_shift_low <<= 1;
	background_shift_high <<= 1;
	attribute_shift_low = (attribute_shift_low << 1) | attribute_latch_low;
	attribute_shift_high = (attribute_shift_high << 1) | attribute_latch_high;
}


/* Execute a cycle of a scanline */
void Ppu::scanline_cycle(Scanline_type scanline_type)
{
	static uint16_t addr;

	switch (scanline_type) {
	case Scanline_type::nmi:
		if (dot == 1) {
			status.nmi_occurred = 1;
			if (control.nmi_output) {
				cpu.trigger(Cpu::Interrupt::nmi);
			}
		}
		break;
	case Scanline_type::post:
		if (dot == 0) {
			screen.swap();
			screen.render();
		}
		break;
	case Scanline_type::visible:
	case Scanline_type::pre:
		// Sprites:
		switch (dot) {
		case 1:
			secondary_oam.clear();
			if (scanline_type == Scanline_type::pre) {
				status.sprite_overflow = 0;
				status.sprite_zero_hit = 0;
			}
			break;
		case 257:
			eval_sprites();
			break;
		case 321:
			load_sprites();
			break;
		}

		// Background:
		switch (dot) {
		case 2 ... 255:
		case 322 ... 337:
			pixel();
			switch (dot % 8) {
			// Nametable:
			case 1:
				addr = nt_addr();
				reload_shift();
				break;
			case 2:
				nametable_byte = read(addr);
				break;
			// Attribute:
			case 3:
				addr = at_addr();
				break;
			case 4:
				attributetable_byte = read(addr);
				if (v.coarse_y & 2) {
					attributetable_byte >>= 4;
				}
				if (v.coarse_y & 2) {
					attributetable_byte >>= 2;
				}
				break;
			// Background (low bits):
			case 5:
				addr = bg_addr();
				break;
			case 6:
				low_tile_byte = read(addr);
				break;
			// Background (high bits):
			case 7:
				addr += 8;
				break;
			case 0:
				high_tile_byte = read(addr);
				incr_x();  // h_scroll
				break;
			}
			break;
		case 256:  // Vertical bump.
			pixel();
			high_tile_byte = read(addr);
			incr_y();
			break;
		case 257:  // Update horizontal position.
			pixel();
			reload_shift();
			copy_x(); //h_update();
			break;
		case 280 ... 304:
			if (scanline_type == Scanline_type::pre) {
				// Update vertical position.
				copy_y();
			}
			break;

		// No shift reloading:
		case 1:
			addr = nt_addr();
			if (scanline_type == Scanline_type::pre) {
				status.nmi_occurred = 0;
			}
			break;
		case 321:
		case 339:
			addr = nt_addr();
			break;
		// Nametable fetch instead of attribute:
		case 338:
			nametable_byte = read(addr);
			break;
		case 340:
			nametable_byte = read(addr);
			if (scanline_type == Scanline_type::pre && rendering() && f) {
				++dot;
			}
			break;
		}

		// Signal scanline to mapper:
		if (dot == 260 && rendering()) {
			// TODO
			//cart->signal_scanline();
		}
		break;
	}
}

/* Execute a PPU cycle. */
void Ppu::step()
{
	switch (scan_line) {
	case 0 ... 239:
		scanline_cycle(Scanline_type::visible);
		break;
	case 240:
		scanline_cycle(Scanline_type::post);
		break;
	case 241:
		scanline_cycle(Scanline_type::nmi);
		break;
	case 261:
		scanline_cycle(Scanline_type::pre);
		break;
	}
	// Update dot and scanline counters:
	++dot;
	if (dot > 340) {
		dot %= 341;
		if (++scan_line > 261) {
			scan_line = 0;
			f ^= 1;
		}
	}
}

