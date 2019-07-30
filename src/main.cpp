#include <iostream>

#include "nesemu.h"
#include "common.h"

Cpu cpu;
Ppu ppu;
Memory memory;
Cartridge* cart = nullptr;
Screen screen;
std::ostream& logger = std::clog;

void run_loop(Console& console)
{
	for (;;) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				return;
			}
		}

		//if (tick - prev_tick > 100) {
		//printf("cpu cyc: %u, ppu cyc: %u, ppu scanline: %u\n", 
		//	console.cpu.cycle, console.ppu.cycle, console.ppu.scan_line);
		LOG_FMT("%s", cpu.take_snapshot().str().c_str());
		console.step();
		//screen.render();
		//	prev_tick = tick;
		//}
	}
}

int main(int argc, char **argv)
{
	Console console;

	try {
		if (argc < 2) {
			logger << "not enough arguments...\n";
			return EXIT_FAILURE;
		}

		console.load(argv[1]);
		run_loop(console);
	} catch (std::exception& e) {
		logger << e.what() << '\n';
		return EXIT_FAILURE;
	} catch (std::string e) {
		logger << e << '\n';
		return EXIT_FAILURE;
	} catch (const char* e) {
		logger << e << '\n';
		return EXIT_FAILURE;
	} 

	return EXIT_SUCCESS;
}
