#include "ppu.h"

#include <vector>
#include <cstdint>
#include <cassert>

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
	switch (ppu_state) {
	case PreRender:  step_pre_render();  break;
	case Render:     step_render();      break;
	case PostRender: step_post_render(); break;
	case VBlank:     step_vblank();      break;
	default: assert(0);
	}

	++cycle;
}

auto Ppu::step() -> void
{
	// TODO: update cycle, scanline, frame counter
	
	bool rendering_enabled = mask.show_background || mask.show_sprites;
	auto pre_line = scanline == 261;
	auto visible_line = scanline < 240;
	auto render_line = pre_line || visible_line;
	auto pre_fetch_cycle = BETWEEN(cycle, 312, 327);
	auto visible_cycle = BETWEEN(cycle, 1, 257);
	auto fetch_cycle = pre_fetch_cycle || visible_cycle;

	if (rendering_enabled) {
		if (visible_line && visible_cycle) {
			// render pixel
		}
		if (render_line && fetch_cycle) {
			// ???
		}
		if (pre_line && BETWEEN(cycle, 280, 305)) {
			copy_y();
		}
		if (render_line) {
			if (fetch_cycle && (cycle & 0x7) == 0) {
				incr_x();
			}
			if (cycle == 256) {
				incr_y();
			}
			if (cycle == 257) {
				copy_x();
			}
		}
	}

	if (rendering_enabled) {
		if (cycle == 257) {
			if (visible_line) {
				eval_sprites(); 
			}
		}
	}

	if (scanline == 241 && cycle == 1) {
		// set vblank
	}
	if (pre_line && cycle == 1) {
		// clear vblank
		status.sprite0_hit = 0;
		status.sprite_overflow = 0;
	}
}

auto Ppu::read_register(uint16_t addr) -> uint8_t
{
	switch (addr) {
	case PpuStatusRegddr:
	{
		// TODO: NMI change?
		auto result = status.raw;
		status.vblank_started = 0;
		write = false;
		return result;
	}
	case OamDataRegAddr:
		return obj_attr_map.raw[obj_attr_map_addr];
	case PpuDataRegAddr:
	{
		auto value = memory->read(addr);
		if (vram_addr.raw % 0x4000 < 0x3F00) {
			std::swap(value, buffered);
		} else {
			buffered = memory->read(vram_addr.raw - 0x1000);
		}

		vram_addr.raw += ctrl.vram_incr ? 32 : 1;

		return value;
	}
	}
}

auto Ppu::write_register(uint16_t addr, uint8_t value) -> void 
{
	switch (addr) {
	case PpuCtrlRegAddr:
		// TODO: change NMI
		ctrl.raw = value;
		tmp_vram_addr.nt_select = value;
		break;
	case PpuMaskRegAddr:
		mask.raw = value;
		break;
	case OamAddrRegAddr:
		obj_attr_map_addr = value;
		break;
	case OamDataRegAddr:
		obj_attr_map.raw[obj_attr_map_addr++] = value;
		break;
	case PpuScrollRegAddr:
		if (write) {
			tmp_vram_addr.coarse_y = value >> 3;
			tmp_vram_addr.fine_y = value;
		} else {
			tmp_vram_addr.coarse_x = value >> 3;
			vram_addr.raw = value;
		}
		write = !write;
		break;
	case PpuAddrRegAddr:
		if (write) {
			tmp_vram_addr.low = value;
			vram_addr.raw = tmp_vram_addr.raw;
		} else {
			tmp_vram_addr.hi = value;
		}
		write = !write;
		break;
	case PpuDataRegAddr:
		memory->write(vram_addr.raw, value);
		vram_addr.raw += ctrl.vram_incr ? 32 : 1;
		break;
	case OamDmaRegAddr:
	{
		uint16_t addr = value << 8;
		for (int i = 0; i < ObjAttrMapSize; i++) {
			obj_attr_map.raw[obj_attr_map_addr++] = memory->read(addr++);
		}
		cpu->stall(cpu->cycle % 2 ? 514 : 513);
		break;
	}
	}
}

auto Ppu::incr_x() -> void
{
	vram_addr.coarse_x++;
	if (vram_addr.coarse_x == 0) {
		vram_addr.nt_select ^= bit(0);
	}
}

auto Ppu::incr_y() -> void
{
	vram_addr.fine_y++;
	if (vram_addr.fine_y == 0) {
		vram_addr.coarse_y++;
		if (vram_addr.coarse_y == 30) {
			vram_addr.coarse_y = 0;
			vram_addr.nt_select ^= bit(1);
		} 
	}
}

auto Ppu::copy_x() -> void
{
	vram_addr.coarse_x = tmp_vram_addr.coarse_x;
	vram_addr.nt_select &= bit(0);
	vram_addr.nt_select |= tmp_vram_addr.nt_select & bit(0);
}

auto Ppu::copy_y() -> void
{
	vram_addr.coarse_y = tmp_vram_addr.coarse_y;
	vram_addr.fine_y = tmp_vram_addr.fine_y;
	vram_addr.nt_select &= bit(1);
	vram_addr.nt_select |= tmp_vram_addr.nt_select & bit(1);
}

auto Ppu::tile_addr() -> uint16_t
{
	return 0x2000 | (vram_addr.raw & 0x0FFF);
}

auto Ppu::attr_addr() -> uint16_t
{
	return 0x23C0 | (vram_addr.raw & 0x0C00) | ((vram_addr.raw >> 4) & 0x38) | ((vram_addr.raw >> 2) & 0x7);
}

auto Ppu::palette_at(uint16_t addr) -> uint8_t&
{
	if (addr >= 16 && addr % 4 == 0) {
		addr -= 16;
	}
	return palette_data[addr];
}

auto Ppu::step_pre_render() -> void
{
	if (cycle == 1) {
		status.vblank_started = 0;
		status.sprite0_hit = 0;
	} else if (cycle == 258 && mask.show_background && mask.show_sprites) {
		copy_x();
	} else if (cycle > 280 && cycle <= 304 && mask.show_background && mask.show_sprites) {
		copy_y();
	}

	//if rendering is on, every other frame is one cycle shorter
	if (cycle >= 340 - (odd(frame) && mask.show_background && mask.show_sprites)) {
		ppu_state = Render;
		cycle = 0;
		scanline = 0;
	}
}

auto Ppu::step_post_render() -> void
{
	if (cycle >= 340) {
		scanline++;
		cycle = 0;
		ppu_state = VBlank;

		for (int x = 0; x < m_pictureBuffer.size(); x++) {
			for (int y = 0; y < m_pictureBuffer[0].size(); ++y) {
				m_screen.setPixel(x, y, m_pictureBuffer[x][y]);
			}
		}
	}
}

auto Ppu::step_render() -> void
{
	if (cycle > 0 && cycle <= 256) {
		uint8_t bgColor = 0;
		uint8_t sprColor = 0;
		bool bgOpaque = false; 
		bool sprOpaque = true;
		bool spriteForeground = false;

		// int x = cycle - 1;
		// int y = scanline;

		if (mask.show_background) {
			auto x_fine = (fine_x + cycle - 1) % 8;
			if (mask.show_left_background || x >= 8) {
				//fetch tile
				auto tile = ppu->read(tile_addr());

				//fetch pattern
				//Each pattern occupies 16 bytes, so multiply by 16
				addr = (tile * 16) + vram_addr.fine_y;
				addr |= m_bgPage << 12; //set whether the pattern is in the high or low page
				//Get the corresponding bit determined by (8 - x_fine) from the right
				bgColor = (memory->read(addr) >> (7 ^ x_fine)) & 1; //bit 0 of palette entry
				bgColor |= ((memory->read(addr + 8) >> (7 ^ x_fine)) & 1) << 1; //bit 1

				bgOpaque = bgColor; //flag used to calculate final pixel with the sprite pixel

				//fetch attribute and calculate higher two bits of palette
				addr = 0x23C0 | (vram_addr.raw & 0x0C00) | ((vram_addr.raw >> 4) & 0x38) | ((vram_addr.raw >> 2) & 0x07);
				auto attribute = memory->read(addr);
				int shift = ((vram_addr.raw >> 4) & 4) | (vram_addr.raw & 2);
				//Extract and set the upper two bits for the color
				bgColor |= ((attribute >> shift) & 0x3) << 2;
			}
			//Increment/wrap coarse X
			if (x_fine == 7) {
				incr_x();
			}
		}

		if (mask.show_sprites && (mask.show_left_sprites || cycle > 8)) {

			for (const auto& spr : scanline_sprites) {
				
				// uint8_t spr_x = m_spriteMemory[i * 4 + 3];

				if (0 > cycle - 1 - spr.x || cycle - 1 - spr.x >= 8) {
					continue;
				}

				// uint8_t spr_y = m_spriteMemory[i * 4 + 0] + 1;
				// uint8_t tile = m_spriteMemory[i * 4 + 1];
				// uint8_t attribute = m_spriteMemory[i * 4 + 2];

				auto length = ctrl.big_sprite ? 16 : 8;

				auto x_shift = (cycle - 1 - spr.x) % 8;
				auto y_offset = (scanline - spr.y) % length; // (spr.y + 1)?

				if (!spr.attr.flip_horiz) { 
					x_shift ^= 7;
				}
				if (spr.attr.flip_vert) { 
					y_offset ^= (length - 1);
				}

				auto addr = 0;

				if (!ctrl.big_sprite) {
					addr = spr.idx * 16 + y_offset;
					if (ctrl.sprite_pt) {
						addr += 0x1000;
					}
				} else /*8x16 sprites*/ {
					//bit-3 is one if it is the bottom tile of the sprite, multiply by two to get the next pattern
					y_offset = (y_offset & 7) | ((y_offset & 8) << 1);
					addr = (spr.idx >> 1) * 32 + y_offset;
					addr |= (spr.idx & 1) << 12; //Bank 0x1000 if bit-0 is high
				}

				sprColor |= (memory->read(addr) >> (x_shift)) & 1; //bit 0 of palette entry
				sprColor |= ((memory->read(addr + 8) >> (x_shift)) & 1) << 1; //bit 1

				if (!(sprOpaque = sprColor)) {
					sprColor = 0;
					continue;
				}

				//Select sprite palette
				sprColor |= 0x10 | (spr.attr.palette << 2); //bits 2-3

				spriteForeground = !spr.attr.priority;

				//Sprite-0 hit detection
				if (!status.sprite0_hit && mask.show_background && i == 0 && sprOpaque && bgOpaque) {
					status.sprite0_hit = 1;
				}

				break; //Exit the loop now since we've found the highest priority sprite
			}
		}

		uint8_t paletteAddr = bgColor;

		if ((!bgOpaque && sprOpaque) || (bgOpaque && sprOpaque && spriteForeground)) {
			paletteAddr = sprColor;
		} else if (!bgOpaque && !sprOpaque) {
			paletteAddr = 0;
		}

		pixel_buffer->set(scanline, cycle - 1, )

		m_pictureBuffer[x][y] = sf::Color(colors[m_bus.readPalette(paletteAddr)]);

	} else if (cycle == 257 && mask.show_background) {
		incr_y();
	} else if (cycle == 258 && mask.show_background && mask.show_sprites) {
		copy_x();
	}

	if (cycle >= 340) {
		eval_sprites();
		++scanline;
		cycle = 0;
	}

	if (scanline >= 240) {
		ppu_state = PostRender;
	}
}

auto Ppu::step_vblank() -> void
{
	if (cycle == 1 && scanline == 241) {
		status.vblank_started = 1;
		if (ctrl.generate_vblank) {
			m_vblankCallback();
		}
	}

	if (cycle >= 340) {
		++scanline;
		cycle = 0;
	}

	if (scanline >= 261) {
		ppu_state = PreRender;
		scanline = 0;
	}
}

// NOTE: Sprite Overflow not implemented!
auto Ppu::eval_sprites() -> void 
{
	scanline_sprites.clear();
	unsigned height = ctrl.big_sprite ? 16 : 8;
	for (const auto& sprite : obj_attr_map.obj) {
		if (BETWEEN(sprite.y, scanline, scanline + height)) {
			scanline_sprites.push_back(sprite);
		}
	}
	
	while (scanline_sprites.size() > 8) {
		scanline_sprites.pop_back();
		status.sprite_overflow = true;
	}
}

