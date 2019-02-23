#include <iostream>

#include "../src/nesemu.h"
#include "gfx_sdl.h"

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
		std::cerr << str(console.cpu.take_snapshot()) << '\n';
		console.step();
		console.pixels->update();
		//	prev_tick = tick;
		//}
	}
}

auto main(int argc, char **argv) -> int
{
	std::ios::sync_with_stdio(0);

	try {
		GraphicsSdl gfx;
		Console console{ gfx.pixel_buffer() };

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
