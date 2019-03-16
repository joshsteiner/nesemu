#include <iostream>

#include "nesemu.h"

Cpu cpu;
Ppu ppu;
Memory memory;
Screen screen;

auto run_loop(Console& console) -> void
{
	uint32_t tick, prev_tick = SDL_GetTicks();

	for (;;) {
		tick = SDL_GetTicks();

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
		std::clog << str(cpu.take_snapshot()) << '\n';
		console.step();
		screen.update();
		//	prev_tick = tick;
		//}
	}
}

auto main(int argc, char **argv) -> int
{
	std::ios::sync_with_stdio(0);

	Console console;

	try {
		if (argc < 2) {
			std::cerr << "not enough arguments...\n";
			return EXIT_FAILURE;
		}

		console.load(argv[1]);
		run_loop(console);
	} catch (std::exception e) {
		std::cerr << e.what() << '\n';
		return EXIT_FAILURE;
	} catch (std::string e) {
		std::cerr << e << '\n';
		return EXIT_FAILURE;
	} catch (const char* e) {
		std::cerr << e << '\n';
		return EXIT_FAILURE;
	} 

	return EXIT_SUCCESS;
}
