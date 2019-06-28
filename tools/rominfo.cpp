#include "../src/cart.h"
#include <cstdio>
#include <iostream>
#include <iomanip>

#define H(n) cart.rom.end()[n]
#define V(n) H(-2*(n)+1), H(-2*n)

int main(int argc, char** argv)
{
	if (argc != 2) {
		std::cerr << "USAGE: romfinfo ROM" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	std::ifstream file{ argv[1], std::ios::binary };
	auto cart = Cartridge::from_ines(file);


	printf("Cartridge %s info:\n"
		"NMI VECTOR: %02X%02X\n"
		"RESET VECTOR: %02X%02X\n"
		"IRQ VECTOR: %02X%02X\n",
		argv[1], V(3), V(2), V(1));

	return EXIT_SUCCESS;
}
