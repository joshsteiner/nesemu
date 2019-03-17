#ifndef NESEMU_PPU_H
#define NESEMU_PPU_H

#include "screen.h"
#include "memory.h"
#include "cpu.h"

class Memory;
class Cpu;

class Ppu
{
public:
	unsigned cycle = 0;
	unsigned scan_line = 0; 
	unsigned frame = 0;

	std::array<uint8_t, 32>   paletteData;
	std::array<uint8_t, 2048> nametable_data;
	std::array<uint8_t, 256>  oamData;

	std::array<Color, 64> palette{
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
	uint16_t v = 0, t = 0;
	uint8_t  x = 0, w = 0, f = 0;

	uint8_t reg = 0;

	// NMI flags
	bool nmiOccurred = false, nmiOutput = false, nmiPrevious = false;
	uint8_t nmiDelay = 0;

	// background temporary variables
	uint8_t nameTableByte = 0, attributeTableByte = 0, lowTileByte = 0, highTileByte = 0;
	uint64_t tileData = 0;

	// sprite temporary variables
	unsigned spriteCount = 0;
	std::array<uint32_t, 8> spritePatterns;
	std::array<uint8_t, 8> spritePositions, spritePriorities, spriteIndexes;

	// $2000 PPUCTRL
	uint8_t
		flagNameTable = 0,
		flagIncrement = 0,
		flagSpriteTable = 0,
		flagBackgroundTable = 0,
		flagSpriteSize = 0,
		flagMasterSlave = 0;

	// $2001 PPUMASK
	uint8_t
		flagGrayscale = 0,
		flagShowLeftBackground = 0,
		flagShowLeftSprites = 0,
		flagShowBackground = 0,
		flagShowSprites = 0,
		flagRedTint = 0,
		flagGreenTint = 0,
		flagBlueTint = 0;

	// $2002 PPUSTATUS
	uint8_t
		flagSpriteZeroHit = 0,
		flagSpriteOverflow = 0;

	// $2003 OAMADDR
	uint8_t oamAddress = 0;

	// $2007 PPUDATA
	uint8_t bufferedData = 0; // for buffered reads

	Ppu();

	void Reset(); 
	uint8_t& palette_at(uint16_t addr);
	uint8_t readRegister(uint16_t address);

	void writeRegister(uint16_t address, uint8_t value);
	void writeControl(uint8_t value);
	void writeMask(uint8_t value);
	uint8_t readStatus();
	void writeOAMAddress(uint8_t value);
	uint8_t readOAMData();
	void writeOAMData(uint8_t value);
	void writeScroll(uint8_t value);
	void writeAddress(uint8_t value);
	uint8_t readData();
	void writeData(uint8_t value);
	void writeDMA(uint8_t value);
	void incrementX();
	void incrementY();
	void copyX();
	void copyY();
	void nmiChange();
	void setVerticalBlank(); 
	void clearVerticalBlank(); 
	void fetchNameTableByte(); 
	void fetchAttributeTableByte(); 
	void fetchLowTileByte(); 
	void fetchHighTileByte(); 
	void storeTileData(); 
	uint32_t fetchTileData(); 
	uint8_t backgroundPixel(); 
	std::pair<uint8_t, uint8_t> spritePixel(); 
	void renderPixel(); 
	uint32_t fetchSpritePattern(int i, int row); 
	void evaluateSprites(); 
	void tick(); 
	void step();
};

extern Cpu cpu;
extern Memory memory;

#endif
