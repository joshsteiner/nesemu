#include <iostream>

#include "nesemu.h"
#include "common.h"
#include "config.h"

Cpu cpu;
Ppu ppu;
Memory memory;
Cartridge* cart = nullptr;
Screen screen;
Controllers controllers;
std::ostream& logger = std::clog;
uint8_t joypad_state[2];

void run_loop(Console& console)
{
	for (;;) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				return;
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym) {
				case JOYPAD_1_A:      joypad_state[0] |= (1 << 0); break;
				case JOYPAD_1_B:      joypad_state[0] |= (1 << 1); break;
				case JOYPAD_1_SELECT: joypad_state[0] |= (1 << 2); break;
				case JOYPAD_1_START:  joypad_state[0] |= (1 << 3); break;
				case JOYPAD_1_UP:     joypad_state[0] |= (1 << 4); break;
				case JOYPAD_1_DOWN:   joypad_state[0] |= (1 << 5); break;
				case JOYPAD_1_LEFT:   joypad_state[0] |= (1 << 6); break;
				case JOYPAD_1_RIGHT:  joypad_state[0] |= (1 << 7); break;
				}
				break;
			case SDL_KEYUP:
				switch (event.key.keysym.sym) {
				case JOYPAD_1_A:      joypad_state[0] &= ~(1 << 0); break;
				case JOYPAD_1_B:      joypad_state[0] &= ~(1 << 1); break;
				case JOYPAD_1_SELECT: joypad_state[0] &= ~(1 << 2); break;
				case JOYPAD_1_START:  joypad_state[0] &= ~(1 << 3); break;
				case JOYPAD_1_UP:     joypad_state[0] &= ~(1 << 4); break;
				case JOYPAD_1_DOWN:   joypad_state[0] &= ~(1 << 5); break;
				case JOYPAD_1_LEFT:   joypad_state[0] &= ~(1 << 6); break;
				case JOYPAD_1_RIGHT:  joypad_state[0] &= ~(1 << 7); break;
				}
				break;
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

	joypad_state[0] = 0;
	joypad_state[1] = 0;

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
