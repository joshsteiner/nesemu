#include "nesemu.h"

#define HEADER_SIZE       16
#define PRG_ROM_PAGE_SIZE 0x4000
#define CHR_ROM_PAGE_SIZE 0x2000

#define CART_INVALID 1

#define MIRROR_V 0
#define MIRROR_H 1


int from_iNES(FILE *file, struct Cartridge *cart) 
{
	static const char *MAGIC_CONST = "NES\x1A";

	int i = 0;

	for (; i < 4; i++) {
		if (fgetc(file) != MAGIC_CONST[i]) {
			return CART_INVALID;
		}
	}

	size_t prg_rom_page_count = fgetc(file);
	size_t chr_rom_page_count = fgetc(file);
	uint8_t f6                = fgetc(file);
	uint8_t f7                = fgetc(file);

	cart->mirroring   = f6 & bit(0); 
	cart->mapper      = (f7 & 0xF0) | (f6 >> 4); 
	cart->has_battery = !!(f6 & bit(1));

	int hasTrainer = f6 & bit(2);

	/* skip header */
	fseek(file, HEADER_SIZE, SEEK_SET);

	/* skip trainer */
	if (hasTrainer) {
		fseek(file, 0x200, SEEK_CUR);
	}

	/* TODO: safe malloc */
	/* read prgRom */
	cart->prg_rom_size = prg_rom_page_count * PRG_ROM_PAGE_SIZE;
	cart->prg_rom = malloc(cart->prg_rom_size * sizeof(uint8_t));
	for (size_t j = 0; j < cart->prg_rom_size; j++) {
		cart->prg_rom[j] = fgetc(file);
	}

	/* read chrRom */
	cart->chr_rom_size = (chr_rom_page_count || 1) * CHR_ROM_PAGE_SIZE;
	cart->chr_rom = malloc(cart->chr_rom_size * sizeof(uint8_t));
	for (size_t j = 0; j < cart->chr_rom_size; j++) {
		cart->chr_rom[j] = fgetc(file);
	}

	return 0;
}
