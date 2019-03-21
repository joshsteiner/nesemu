#include "ppu.h"


Ppu::Ppu() { Reset(); }

void Ppu::Reset()
{
	cycle = 340;
	scan_line = 240;
	frame = 0;
	writeControl(0);
	writeMask(0);
	writeOAMAddress(0);
}

uint8_t Ppu::read(uint16_t addr)
{
	std::clog << "Ppu::read(" << std::hex << addr << ")\n";

	if (BETWEEN(addr, 0, 0x2000)) {
		return memory.cart.vrom.at(addr);
	} else if (BETWEEN(addr, 0x2000, 0x3F00)) {
		// no nametable mirroring in Ppu::read, only memory mirroring instead 
		if (addr > 0x3000) {
			addr -= 0x1000;
		}
		addr -= 0x2000;
		std::clog << "nametable_data: reading: " << addr << "\n";
		return nametable_data.at(addr);
	} else if (BETWEEN(addr, 0x3F00, 0x4000)) {
		addr = (addr - 0x3F00) % 0x20;
		return paletteData.at(addr);
	}

	// TEMPORARY SOLUTION: ignore this for now
	return read(addr % 0x4000);
	throw std::string("Invalid addr in Ppu::read()");
}

void Ppu::write(uint16_t addr, uint8_t value)
{
	std::clog << "Ppu::write(" << addr << ", " << (int)value << ")";
	if (BETWEEN(addr, 0, 0x2000)) {
		// is this memory read only?
		std::clog << "potential ROM write\n";
		memory.cart.vrom.at(addr) = value;
		return;
	} else if (BETWEEN(addr, 0x2000, 0x3F00)) {
		if (addr > 0x3000) {
			addr -= 0x1000;
		}
		addr -= 0x2000;
		unsigned neutral_bit;
		switch (memory.cart.mirroring) {
		case VERTICAL:
			// Vertical: $2000 = $2800 ; $2400 = $2C00
			neutral_bit = (1 << 11);
			break;
		case HORIZONTAL:
			// Horizontal: $2000 = $2400 ; $2800 = $2C00 
			neutral_bit = (1 << 10);
			break;
		}
		std::clog << "nametable_data: writing to: " << addr << " & " << (addr ^ neutral_bit) << "\n";
		nametable_data.at(addr) = nametable_data.at(addr ^ neutral_bit) = value;
		return;
	} else if (BETWEEN(addr, 0x3F00, 0x4000)) {
		addr = (addr - 0x3F00) % 0x20;
		paletteData.at(addr) = value;
		return;
	}

	throw std::string("Invalid addr in Ppu::write()");
}

uint8_t& Ppu::palette_at(uint16_t addr)
{
	if (addr >= 16 && addr % 4 == 0) {
		addr -= 16;
	}
	return paletteData.at(addr);
}

uint8_t Ppu::readRegister(uint16_t address)
{
	switch (address) {
	case 0x2002: return readStatus();
	case 0x2004: return readOAMData();
	case 0x2007: return readData();
	}
	return 0;
}

void Ppu::writeRegister(uint16_t address, uint8_t value)
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
void Ppu::writeControl(uint8_t value)
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
void Ppu::writeMask(uint8_t value) 
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
uint8_t Ppu::readStatus() 
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
void Ppu::writeOAMAddress(uint8_t value) 
{
	oamAddress = value;
}

// $2004: OAMDATA (read)
uint8_t Ppu::readOAMData()
{
	return oamData.at(oamAddress);
}

// $2004: OAMDATA (write)
void Ppu::writeOAMData(uint8_t value)
{
	oamData.at(oamAddress) = value;
	oamAddress++;
}

// $2005: PPUSCROLL
void Ppu::writeScroll(uint8_t value)
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
void Ppu::writeAddress(uint8_t value)
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
uint8_t Ppu::readData() 
{
	// TODO: read from ppu, not memory
	auto value = read(v);
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
void Ppu::writeData(uint8_t value)
{
	// TODO: read from ppu, not memory
	std::clog << "Ppu::writeData : v=" << std::hex << v << "\n";
	write(v, value);
	v += (flagIncrement == 0) ? 1 : 32;
}

// $4014: OAMDMA
void Ppu::writeDMA(uint8_t value) 
{
	auto address = value << 8;
	for (auto i = 0; i < 256; i++) {
		// TODO: wrap
		oamData.at(oamAddress) = memory.read(address);
		oamAddress++;
		address++;
	}
	cpu.stall(cpu.cycle % 2 == 1 ? 514 : 513);
}

// NTSC Timing Helper Functions

void Ppu::incrementX()
{
	if ((v & 0x001F) == 31) {
		v &= 0xFFE0;
		v ^= 0x0400;
	} else {
		v++;
	}
}

void Ppu::incrementY() 
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

void Ppu::copyX() { v = (v & 0xFBE0) | (t & 0x041F); }
void Ppu::copyY() { v = (v & 0x841F) | (t & 0x7BE0); }

void Ppu::nmiChange()
{
	bool nmi = nmiOutput && nmiOccurred;
	if (nmi && !nmiPrevious) {
		// TODO: this fixes some games but the delay shouldn't have to be so
		// long, so the timings are off somewhere
		nmiDelay = 15;
	}
	nmiPrevious = nmi;
}

void Ppu::setVerticalBlank()
{
	screen.update();
	nmiOccurred = true;
	nmiChange();
}

void Ppu::clearVerticalBlank()
{
	nmiOccurred = false;
	nmiChange();
}

void Ppu::fetchNameTableByte()
{
	auto address = 0x2000 | (v & 0x0FFF);
	nameTableByte = memory.read(address);
}

void Ppu::fetchAttributeTableByte() 
{
	auto address = 0x23C0 | (v & 0x0C00) | ((v >> 4) & 0x38) | ((v >> 2) & 0x07);
	auto shift = ((v >> 4) & 4) | (v & 2);
	attributeTableByte = ((memory.read(address) >> shift) & 3) << 2;
}

void Ppu::fetchLowTileByte()
{
	auto fineY = (v >> 12) & 7;
	auto table = flagBackgroundTable;
	auto tile = nameTableByte;
	auto address = 0x1000 * table + tile * 16 + fineY;
	lowTileByte = memory.read(address);
}

void Ppu::fetchHighTileByte()
{
	auto fineY = (v >> 12) & 7;
	auto table = flagBackgroundTable;
	auto tile = nameTableByte;
	auto address = 0x1000 * table + tile * 16 + fineY;
	highTileByte = memory.read(address + 8);
}

void Ppu::storeTileData()
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

uint32_t Ppu::fetchTileData() 
{
	return tileData >> 32;
}

uint8_t Ppu::backgroundPixel()
{
	if (flagShowBackground == 0) {
		return 0;
	}
	auto data = fetchTileData() >> ((7 - x) * 4);
	return data & 0x0F;
}

std::pair<uint8_t, uint8_t> Ppu::spritePixel() 
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

void Ppu::renderPixel() 
{
	auto x = cycle - 1;
	auto y = scan_line;
	auto background = backgroundPixel();
	uint8_t i, sprite;
	std::tie(i, sprite) = spritePixel();
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

uint32_t Ppu::fetchSpritePattern(int i, int row) 
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

void Ppu::evaluateSprites()
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
void Ppu::tick()
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
void Ppu::step() 
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

