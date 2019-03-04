#include "ppu.h"


Ppu::Ppu() { Reset(); }

auto Ppu::Reset() -> void
{
	cycle = 340;
	scan_line = 240;
	frame = 0;
	writeControl(0);
	writeMask(0);
	writeOAMAddress(0);
}

auto Ppu::palette_at(uint16_t addr) -> uint8_t&
{
	if (addr >= 16 && addr % 4 == 0) {
		addr -= 16;
	}
	return paletteData.at(addr);
}

auto Ppu::readRegister(uint16_t address) -> uint8_t
{
	switch (address) {
	case 0x2002: return readStatus();
	case 0x2004: return readOAMData();
	case 0x2007: return readData();
	}
	return 0;
}

auto Ppu::writeRegister(uint16_t address, uint8_t value) -> void
{
	reg = value;
	switch (address) {
	case 0x2000: writeControl(value); break;
	case 0x2001: writeMask(value); break;
	case 0x2003: writeOAMAddress(value); break;
	case 0x2004: writeOAMData(value); break;
	case 0x2005: writeScroll(value); break;
	case 0x2006: writeAddress(value); break;
	case 0x2007: writeData(value); break;
	case 0x4014: writeDMA(value); break;
	}
}

// $2000: PPUCTRL
auto Ppu::writeControl(uint8_t value) -> void
{
	flagNameTable = (value >> 0) & 3;
	flagIncrement = (value >> 2) & 1;
	flagSpriteTable = (value >> 3) & 1;
	flagBackgroundTable = (value >> 4) & 1;
	flagSpriteSize = (value >> 5) & 1;
	flagMasterSlave = (value >> 6) & 1;
	nmiOutput = ((value >> 7) & 1) == 1;
	nmiChange();
	t = (t & 0xF3FF) | ((value & 0x03) << 10);
}

// $2001: PPUMASK
auto Ppu::writeMask(uint8_t value) -> void 
{
	flagGrayscale = (value >> 0) & 1;
	flagShowLeftBackground = (value >> 1) & 1;
	flagShowLeftSprites = (value >> 2) & 1;
	flagShowBackground = (value >> 3) & 1;
	flagShowSprites = (value >> 4) & 1;
	flagRedTint = (value >> 5) & 1;
	flagGreenTint = (value >> 6) & 1;
	flagBlueTint = (value >> 7) & 1;
}

// $2002: PPUSTATUS
auto Ppu::readStatus() -> uint8_t 
{
	auto result = reg & 0x1F;
	result |= flagSpriteOverflow << 5;
	result |= flagSpriteZeroHit << 6;
	if (nmiOccurred) {
		result |= 1 << 7;
	}
	nmiOccurred = false;
	nmiChange();
	w = 0;
	return result;
}

// $2003: OAMADDR
auto Ppu::writeOAMAddress(uint8_t value) -> void 
{
	oamAddress = value;
}

// $2004: OAMDATA (read)
auto Ppu::readOAMData() -> uint8_t
{
	return oamData.at(oamAddress);
}

// $2004: OAMDATA (write)
auto Ppu::writeOAMData(uint8_t value) -> void
{
	oamData.at(oamAddress) = value;
	oamAddress++;
}

// $2005: PPUSCROLL
auto Ppu::writeScroll(uint8_t value) -> void
{
	if (w == 0) {
		t = (t & 0xFFE0) | (value >> 3);
		x = value & 0x07;
		w = 1;
	} else {
		t = (t & 0x8FFF) | ((value & 0x07) << 12);
		t = (t & 0xFC1F) | ((value & 0xF8) << 2);
		w = 0;
	}
}

// $2006: PPUADDR
auto Ppu::writeAddress(uint8_t value) -> void
{
	if (w == 0) {
		t = (t & 0x80FF) | ((value & 0x3F) << 8);
		w = 1;
	} else {
		t = (t & 0xFF00) | value;
		v = t;
		w = 0;
	}
}

// $2007: PPUDATA (read)
auto Ppu::readData() -> uint8_t 
{
	auto value = memory.read(v);
	// emulate buffered reads
	if (v % 0x4000 < 0x3F00) {
		auto buffered = bufferedData;
		bufferedData = value;
		value = buffered;
	} else {
		bufferedData = memory.read(v - 0x1000);
	}
	// increment address
	v += (flagIncrement == 0) ? 1 : 32;
	return value;
}

// $2007: PPUDATA (write)
auto Ppu::writeData(uint8_t value) -> void
{
	memory.write(v, value);
	v += (flagIncrement == 0) ? 1 : 32;
}

// $4014: OAMDMA
auto Ppu::writeDMA(uint8_t value) -> void 
{
	auto address = value << 8;
	for (auto i = 0; i < 256; i++) {
		oamData.at(oamAddress) = memory.read(address);
		oamAddress++;
		address++;
	}
	cpu.stall(cpu.cycle % 2 == 1 ? 514 : 513);
}

// NTSC Timing Helper Functions

auto Ppu::incrementX() -> void
{
	if ((v & 0x001F) == 31) {
		v &= 0xFFE0;
		v ^= 0x0400;
	} else {
		v++;
	}
}

auto Ppu::incrementY() -> void 
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

auto Ppu::copyX() -> void { v = (v & 0xFBE0) | (t & 0x041F); }
auto Ppu::copyY() -> void { v = (v & 0x841F) | (t & 0x7BE0); }

auto Ppu::nmiChange() -> void
{
	bool nmi = nmiOutput && nmiOccurred;
	if (nmi && !nmiPrevious) {
		// TODO: this fixes some games but the delay shouldn't have to be so
		// long, so the timings are off somewhere
		nmiDelay = 15;
	}
	nmiPrevious = nmi;
}

auto Ppu::setVerticalBlank() -> void
{
	screen.update();
	nmiOccurred = true;
	nmiChange();
}

auto Ppu::clearVerticalBlank() -> void
{
	nmiOccurred = false;
	nmiChange();
}

auto Ppu::fetchNameTableByte() -> void
{
	auto address = 0x2000 | (v & 0x0FFF);
	nameTableByte = memory.read(address);
}

auto Ppu::fetchAttributeTableByte() -> void 
{
	auto address = 0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07);
	auto shift = ((v >> 4) & 4) | (v & 2);
	attributeTableByte = ((memory.read(address) >> shift) & 3) << 2;
}

auto Ppu::fetchLowTileByte() -> void
{
	auto fineY = (v >> 12) & 7;
	auto table = flagBackgroundTable;
	auto tile = nameTableByte;
	auto address = 0x1000 * table + tile * 16 + fineY;
	lowTileByte = memory.read(address);
}

auto Ppu::fetchHighTileByte() -> void
{
	auto fineY = (v >> 12) & 7;
	auto table = flagBackgroundTable;
	auto tile = nameTableByte;
	auto address = 0x1000 * table + tile * 16 + fineY;
	highTileByte = memory.read(address + 8);
}

auto Ppu::storeTileData() -> void
{
	uint32_t data = 0;
	for (auto i = 0; i < 8; i++) {
		auto a = attributeTableByte;
		auto p1 = (lowTileByte & 0x80) >> 7;
		auto p2 = (highTileByte & 0x80) >> 6;
		lowTileByte <<= 1;
		highTileByte <<= 1;
		data <<= 4;
		data |= a | p1 | p2;
	}
	tileData |= data;
}

auto Ppu::fetchTileData() -> uint32_t 
{
	return tileData >> 32;
}

auto Ppu::backgroundPixel() ->  uint8_t 
{
	if (flagShowBackground == 0) {
		return 0;
	}
	auto data = fetchTileData() >> ((7 - x) * 4);
	return data & 0x0F;
}

auto Ppu::spritePixel() -> std::pair<uint8_t, uint8_t> 
{
	if (flagShowSprites == 0) {
		return { 0, 0 };
	}
	for (unsigned i = 0; i < spriteCount; i++) {
		auto offset = (cycle - 1) - spritePositions[i];
		if (offset < 0 || offset > 7) {
			continue;
		}
		offset = 7 - offset;
		auto color = (spritePatterns[i] >> offset * 4) & 0x0F;
		if (color % 4 == 0) {
			continue;
		}
		return { i, color };
	}
	return { 0, 0 };
}

auto Ppu::renderPixel() 
{
	auto x = cycle - 1;
	auto y = scan_line;
	auto background = backgroundPixel();
	auto [i, sprite] = spritePixel();
	if (x < 8 && flagShowLeftBackground == 0) {
		background = 0;
	}
	if (x < 8 && flagShowLeftSprites == 0) {
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
		if (spriteIndexes.at(i) == 0 && x < 255) {
			flagSpriteZeroHit = 1;
		}
		if (spritePriorities.at(i) == 0) {
			color = sprite | 0x10;
		} else {
			color = background;
		}
	}
	auto c = palette[palette_at(color) % 64];
	screen.set(y, x, c);
}

auto Ppu::fetchSpritePattern(int i, int row) -> uint32_t 
{
	auto tile = oamData.at(i * 4 + 1);
	auto attributes = oamData.at(i * 4 + 2);
	uint16_t address;
	if (flagSpriteSize == 0) {
		if ((attributes & 0x80) == 0x80) {
			row = 7 - row;
		}
		auto table = flagSpriteTable;
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
	auto lowTileByte = memory.read(address);
	auto highTileByte = memory.read(address + 8);
	uint32_t data = 0;
	for (auto i = 0; i < 8; i++) {
		uint8_t p1, p2;
		if ((attributes & 0x40) == 0x40) {
			p1 = (lowTileByte & 1) << 0;
			p2 = (highTileByte & 1) << 1;
			lowTileByte >>= 1;
			highTileByte >>= 1;
		} else {
			p1 = (lowTileByte & 0x80) >> 7;
			p2 = (highTileByte & 0x80) >> 6;
			lowTileByte <<= 1;
			highTileByte <<= 1;
		}
		data <<= 4;
		data |= a | p1 | p2;
	}
	return data;
}

auto Ppu::evaluateSprites() -> void
{
	unsigned h = flagSpriteSize == 0 ? 8 : 16;
	auto count = 0;
	for (auto i = 0; i < 64; i++) {
		auto y = oamData.at(i * 4 + 0);
		auto a = oamData.at(i * 4 + 2);
		auto x = oamData.at(i * 4 + 3);
		auto row = scan_line - y;
		if (row < 0 || row >= h) {
			continue;
		}
		if (count < 8) {
			spritePatterns.at(count) = fetchSpritePattern(i, row);
			spritePositions.at(count) = x;
			spritePriorities.at(count) = (a >> 5) & 1;
			spriteIndexes.at(count) = i;
		}
		count++;
	}
	if (count > 8) {
		count = 8;
		flagSpriteOverflow = 1;
	}
	spriteCount = count;
}

// tick updates Cycle, ScanLine and Frame counters
auto Ppu::tick() -> void
{
	if (nmiDelay > 0) {
		nmiDelay--;
		if (nmiDelay == 0 && nmiOutput && nmiOccurred) {
			cpu.trigger<Cpu::Interrupt::Nmi>();
		}
	}

	if (flagShowBackground != 0 || flagShowSprites != 0) {
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
auto Ppu::step() -> void 
{
	tick();

	auto renderingEnabled = flagShowBackground != 0 || flagShowSprites != 0;
	auto preLine = scan_line == 261;
	auto visibleLine = scan_line < 240;
	auto renderLine = preLine || visibleLine;
	auto preFetchCycle = cycle >= 321 && cycle <= 336;
	auto visibleCycle = cycle >= 1 && cycle <= 256;
	auto fetchCycle = preFetchCycle || visibleCycle;

	// background logic
	if (renderingEnabled) {
		if (visibleLine && visibleCycle) {
			renderPixel();
		}
		if (renderLine && fetchCycle) {
			tileData <<= 4;
			switch (cycle % 8) {
			case 1: fetchNameTableByte(); break;
			case 3: fetchAttributeTableByte(); break;
			case 5: fetchLowTileByte(); break;
			case 7: fetchHighTileByte(); break;
			case 0: storeTileData(); break;
			}
		}
		if (preLine && cycle >= 280 && cycle <= 304) {
			copyY();
		}
		if (renderLine) {
			if (fetchCycle && cycle % 8 == 0) { incrementX(); }
			if (cycle == 256) { incrementY(); }
			if (cycle == 257) { copyX(); }
		}
	}

	// sprite logic
	if (renderingEnabled) {
		if (cycle == 257) {
			if (visibleLine) {
				evaluateSprites();
			} else {
				spriteCount = 0; 
			}
		}
	}

	// vblank logic
	if (scan_line == 241 && cycle == 1) {
		setVerticalBlank(); 
	} 
	if (preLine && cycle == 1) { 
		clearVerticalBlank(); 
		flagSpriteZeroHit = 0; 
		flagSpriteOverflow = 0; 
	}
}

