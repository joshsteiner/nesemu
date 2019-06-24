#include "ppu.h"


Ppu::Ppu()
{
	reset();
}

void Ppu::reset()
{
	cycle = 340;
	scan_line = 240;
	frame = 0;
	//writeControl(0);
	//writeMask(0);
	//writeOAMAddress(0);
}

uint8_t Ppu::read(uint16_t addr)
{
	logger << "Ppu::read(" << std::hex << addr << ")\n";

	if (BETWEEN(addr, 0, 0x2000)) {
		return memory.cart.vrom.at(addr);
	} else if (BETWEEN(addr, 0x2000, 0x3F00)) {
		// no nametable mirroring in Ppu::read, only memory mirroring instead
		if (addr > 0x3000) {
			addr -= 0x1000;
		}
		addr -= 0x2000;
		logger << "nametable_data: reading: " << addr << "\n";
		return nametable_data.at(addr);
	} else if (BETWEEN(addr, 0x3F00, 0x4000)) {
		addr = (addr - 0x3F00) % 0x20;
		return palette_data.at(addr);
	}

	// TEMPORARY SOLUTION: ignore this for now
	return read(addr % 0x4000);
	throw std::string("Invalid addr in Ppu::read()");
}

void Ppu::write(uint16_t addr, uint8_t value)
{
	logger << "Ppu::write(" << addr << ", " << (int)value << ")";
	if (BETWEEN(addr, 0, 0x2000)) {
		// is this memory read only?
		logger << "potential ROM write\n";
		memory.cart.vrom.at(addr) = value;
		return;
	} else if (BETWEEN(addr, 0x2000, 0x3F00)) {
		if (addr > 0x3000) {
			addr -= 0x1000;
		}
		addr -= 0x2000;
		unsigned neutral_bit;
		switch (memory.cart.mirroring) {
		case Mirroring::vertical:
			// Vertical: $2000 = $2800 ; $2400 = $2C00
			neutral_bit = (1 << 11);
			break;
		case Mirroring::horizontal:
			// Horizontal: $2000 = $2400 ; $2800 = $2C00
			neutral_bit = (1 << 10);
			break;
		}
		logger << "nametable_data: writing to: " << addr << " & " << (addr ^ neutral_bit) << "\n";
		nametable_data.at(addr) = nametable_data.at(addr ^ neutral_bit) = value;
		return;
	} else if (BETWEEN(addr, 0x3F00, 0x4000)) {
		addr = (addr - 0x3F00) % 0x20;
		palette_data.at(addr) = value;
		return;
	}

	throw std::string("Invalid addr in Ppu::write()");
}

uint8_t& Ppu::palette_at(uint16_t addr)
{
	if (addr >= 16 && addr % 4 == 0) {
		addr -= 16;
	}
	return palette_data.at(addr);
}

uint8_t Ppu::read_register(uint16_t address)
{
	switch (address) {
	case 0x2002:
	{
		status.filler = reg;
		auto result = status.raw;
		status.nmi_occurred = 0;
		nmi_change();
		w = 0;
		return result;
	}
	case 0x2004:
		return oam_data.at(oam_address);
	case 0x2007:
	{
		// TODO: read from ppu, not memory
		auto value = read(v);
		// emulate buffered reads
		if (v % 0x4000 < 0x3F00) {
			std::swap(buffered_data, value);
		} else {
			buffered_data = memory.read(v - 0x1000);
		}
		// increment address
		v += (control.increment == 0) ? 1 : 32;
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
		nmi_change();
		t = (t & 0xF3FF) | ((value & 0x03) << 10);
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
			t = (t & 0xFFE0) | (value >> 3);
			x = value & 0x07;
			w = 1;
		} else {
			t = (t & 0x8FFF) | ((value & 0x07) << 12);
			t = (t & 0xFC1F) | ((value & 0xF8) << 2);
			w = 0;
		}
		break;
	case 0x2006:
		if (w == 0) {
			t = (t & 0x80FF) | ((value & 0x3F) << 8);
			w = 1;
		} else {
			t = (t & 0xFF00) | value;
			v = t;
			w = 0;
		}
		break;
	case 0x2007:
		// TODO: write to ppu, not memory
		logger << "Ppu::writeData : v=" << std::hex << v << "\n";
		write(v, value);
		v += (control.increment == 0) ? 1 : 32;
		break;
	case 0x4014:
	{
		auto address = value << 8;
		for (auto i = 0; i < 256; i++) {
			// TODO: wrap
			oam_data.at(oam_address) = memory.read(address);
			oam_address++;
			address++;
		}
		cpu.stall(cpu.cycle % 2 == 1 ? 514 : 513);
		break;
	}
	}
}

void Ppu::incr_x()
{
	if ((v & 0x001F) == 31) {
		v &= 0xFFE0;
		v ^= 0x0400;
	} else {
		v++;
	}
}

void Ppu::incr_y()
{
	if ((v & 0x7000) != 0x7000) {
		v += 0x1000;
	} else {
		v &= 0x8FFF;
		auto y = (v & 0x03E0) >> 5;
		if (y == 29) {
			y = 0;
			v ^= 0x0800;
		} else if (y == 31) {
			y = 0;
		} else {
			y++;
		}
		v = (v & 0xFC1F) | (y << 5);
	}
}

void Ppu::copy_x()
{
	v = (v & 0xFBE0) | (t & 0x041F);
}

void Ppu::copy_y()
{
	v = (v & 0x841F) | (t & 0x7BE0);
}

void Ppu::nmi_change()
{
	bool nmi = control.nmi_output && status.nmi_occurred;
	if (nmi && !nmi_previous) {
		// TODO: this fixes some games but the delay shouldn't have to be so
		// long, so the timings are off somewhere
		nmi_delay = 15;
	}
	nmi_previous = nmi;
}

void Ppu::set_vertical_blank()
{
	screen.update();
	nmi_occurred = true;
	nmi_change();
}

void Ppu::clear_vertical_blank()
{
	status.nmi_occurred = false;
	nmi_change();
}

void Ppu::fetch_nametable_byte()
{
	auto address = 0x2000 | (v & 0x0FFF);
	nametable_byte = memory.read(address);
}

void Ppu::fetch_attribute_table_byte()
{
	auto address = 0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07);
	auto shift = ((v >> 4) & 4) | (v & 2);
	attributetable_byte = ((memory.read(address) >> shift) & 3) << 2;
}

void Ppu::fetch_low_tile_byte()
{
	auto fine_y = (v >> 12) & 7;
	auto table = control.background_table;
	auto tile = nametable_byte;
	auto address = 0x1000 * table + tile * 16 + fine_y;
	low_tile_byte = memory.read(address);
}

void Ppu::fetch_high_tile_byte()
{
	auto fine_y = (v >> 12) & 7;
	auto table = control.background_table;
	auto tile = nametable_byte;
	auto address = 0x1000 * table + tile * 16 + fine_y;
	high_tile_byte = memory.read(address + 8);
}

void Ppu::store_tile_data()
{
	uint32_t data = 0;
	for (auto i = 0; i < 8; i++) {
		auto a =  attributetable_byte;
		auto p1 = (low_tile_byte & 0x80) >> 7;
		auto p2 = (high_tile_byte & 0x80) >> 6;
		low_tile_byte <<= 1;
		high_tile_byte <<= 1;
		data <<= 4;
		data |= a | p1 | p2;
	}
	tile_data |= data;
}

uint32_t Ppu::fetch_tile_data()
{
	return tile_data >> 32;
}

uint8_t Ppu::background_pixel()
{
	if (!mask.show_background) {
		return 0;
	}
	auto data = fetch_tile_data() >> ((7 - x) * 4);
	return data & 0x0F;
}

std::pair<uint8_t, uint8_t> Ppu::sprite_pixel()
{
	if (!mask.show_sprites) {
		return { 0, 0 };
	}
	for (unsigned i = 0; i < sprite_count; ++i) {
		auto offset = (cycle - 1) - sprite_positions[i];
		if (offset < 0 || offset > 7) {
			continue;
		}
		offset = 7 - offset;
		auto color = (sprite_patterns[i] >> offset * 4) & 0x0F;
		if (color % 4 == 0) {
			continue;
		}
		return { i, color };
	}
	return { 0, 0 };
}

void Ppu::render_pixel()
{
	auto x = cycle - 1;
	auto y = scan_line;
	auto background = background_pixel();
	uint8_t i, sprite;
	std::tie(i, sprite) = sprite_pixel();
	if (x < 8 && !mask.show_left_background) {
		background = 0;
	}
	if (x < 8 && !mask.show_left_sprites) {
		sprite = 0;
	}
	auto b = background % 4 != 0;
	auto s = sprite % 4 != 0;
	uint8_t color;
	if (!b && !s) {
		color = 0;
	} else if (!b && s) {
		color = sprite | 0x10;
	} else if (b && !s) {
		color = background;
	} else {
		if (sprite_indexes[i] == 0 && x < 255) {
			status.sprite_zero_hit = 1;
		}
		if (sprite_priorities[i] == 0) {
			color = sprite | 0x10;
		} else {
			color = background;
		}
	}
	auto c = palette[palette_at(color) % 64];
	screen.set(y, x, c);
}

uint32_t Ppu::fetch_sprite_pattern(int i, int row)
{
	auto tile = oam_data.at(i * 4 + 1);
	auto attributes = oam_data.at(i * 4 + 2);
	uint16_t address;
	if (control.sprite_size == 0) {
		if ((attributes & 0x80) == 0x80) {
			row = 7 - row;
		}
		auto table = control.sprite_table;
		address = 0x1000 * table + tile * 16 + row;
	} else {
		if ((attributes & 0x80) == 0x80) {
			row = 15 - row;
		}
		auto table = tile & 1;
		tile &= 0xFE;
		if (row > 7) {
			tile++;
			row -= 8;
		}
		address = 0x1000 * table + tile * 16 + row;
	}
	auto a = (attributes & 3) << 2;
	// !!! IMPORTANT: is this a local variable or class? !!!
	// auto low_tile_byte = memory.read(address);
	// auto high_tile_byte = memory.read(address + 8);
	uint32_t data = 0;
	for (auto i = 0; i < 8; i++) {
		uint8_t p1, p2;
		if ((attributes & 0x40) == 0x40) {
			p1 = (low_tile_byte & 1) << 0;
			p2 = (high_tile_byte & 1) << 1;
			low_tile_byte >>= 1;
			high_tile_byte >>= 1;
		} else {
			p1 = (low_tile_byte & 0x80) >> 7;
			p2 = (high_tile_byte & 0x80) >> 6;
			low_tile_byte <<= 1;
			high_tile_byte <<= 1;
		}
		data <<= 4;
		data |= a | p1 | p2;
	}
	return data;
}

void Ppu::evaluate_sprites()
{
	unsigned h = control.sprite_size == 0 ? 8 : 16;
	auto count = 0;
	for (auto i = 0; i < 64; ++i) {
		auto y = oam_data.at(i * 4 + 0);
		auto a = oam_data.at(i * 4 + 2);
		auto x = oam_data.at(i * 4 + 3);
		auto row = scan_line - y;
		if (row < 0 || row >= h) {
			continue;
		}
		if (count < 8) {
			sprite_patterns.at(count) = fetch_sprite_pattern(i, row);
			sprite_positions.at(count) = x;
			sprite_priorities.at(count) = (a >> 5) & 1;
			sprite_indexes.at(count) = i;
		}
		count++;
	}
	if (count > 8) {
		count = 8;
		status.sprite_overflow = 1;
	}
	sprite_count = count;
}

// tick updates Cycle, ScanLine and Frame counters
void Ppu::tick()
{
	if (nmi_delay > 0) {
		nmi_delay--;
		if (nmi_delay == 0 && control.nmi_output && status.nmi_occurred) {
			cpu.trigger(Cpu::Interrupt::nmi);
		}
	}

	if (mask.show_background || mask.show_sprites) {
		if (f == 1 && scan_line == 261 && cycle == 339) {
			cycle = 0;
			scan_line = 0;
			frame++;
			f ^= 1;
			return;
		}
	}

	cycle++;
	if (cycle > 340) {
		cycle = 0;
		scan_line++;
		if (scan_line > 261) {
			scan_line = 0;
			frame++;
			f ^= 1;
		}
	}
}

// Step executes a single PPU cycle
void Ppu::step()
{
	tick();

	bool rendering_enabled = mask.show_background || mask.show_sprites;

	bool pre_line = scan_line == 261;
	bool visible_line = scan_line < 240;
	bool render_line = pre_line || visible_line;

	bool prefetch_cycle = BETWEEN(cycle, 321, 337);
	bool visible_cycle = BETWEEN(cycle, 1, 257);
	bool fetch_cycle = prefetch_cycle || visible_cycle;

	// background logic
	if (rendering_enabled) {
		if (visible_line && visible_cycle) {
			render_pixel();
		}
		if (render_line && fetch_cycle) {
			tile_data <<= 4;
			switch (cycle % 8) {
			case 1:
				fetch_nametable_byte();
				break;
			case 3:
				fetch_attribute_table_byte();
				break;
			case 5:
				fetch_low_tile_byte();
				break;
			case 7:
				fetch_high_tile_byte();
				break;
			case 0:
				store_tile_data();
				break;
			}
		}
		if (pre_line && BETWEEN(cycle, 280, 305)) {
			copy_y();
		}
		if (render_line) {
			if (fetch_cycle && cycle % 8 == 0) {
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

	// sprite logic
	if (rendering_enabled) {
		if (cycle == 257) {
			if (visible_line) {
				evaluate_sprites();
			} else {
				sprite_count = 0;
			}
		}
	}

	// vblank logic
	if (scan_line == 241 && cycle == 1) {
		set_vertical_blank();
	}
	if (pre_line && cycle == 1) {
		clear_vertical_blank();
		status.sprite_zero_hit = 0;
		status.sprite_overflow = 0;
	}
}

