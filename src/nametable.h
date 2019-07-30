#ifndef NESEMU_NAMETABLE_H
#define NESEMU_NAMETABLE_H

struct Pattern {
	int pixels[8][8]; // 0..3
};

struct Tile {
	Color pixels[8][8];
};

enum class Mirroring {
	vertical, horizontal;
};

class PatternTable {
public:
	Pattern get_pattern(unsigned idx);
private:
	std::array<uint8_t, 512> data;
};

Pattern PatternTable::get_pattern(unsigned idx)
{
	Pattern p;
	for (int i = 0; i < 8; ++i) {
		for (int j = 0; j < 8; ++j) {
			p[i][j] = (((idx + i) >> j) & 1) + (((idx + 8 + i) >> j) & 1) * 2;
		}
	}
	return p;
}

class Nametable {
public:
	Tile get_tile(unsigned row, unsigned column);
private:
	std::array<uint8_t, 1024> data;
	Mirroring mirroring;
};

Tile Nametable::get_tile(unsigned row, unsigned column)
{
	
}

#endif
