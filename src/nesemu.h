#ifndef NESEMU_H
#define NESEMU_H

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define bit(n) ((uint8_t)(1 << n))

extern FILE *log_file;
#define LOG(...) fprintf(log_file, __VA_ARGS__)

struct Cartridge {
	int has_battery;
	int mapper;
	int mirroring;
	
	size_t prg_rom_size;
	uint8_t *prg_rom;

	size_t chr_rom_size;
	uint8_t *chr_rom;
};

int step_CPU(void);
void reset_CPU(void);
void run_CPU(void);
void run_nestest(void);
void dump_mem(const char *filename);
void load_cart(struct Cartridge *cart);
int from_iNES(FILE *file, struct Cartridge *cart);

#endif /* NESEMU_H */
