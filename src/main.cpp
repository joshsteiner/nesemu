#include <iostream>

#include "nesemu.h"
#include "common.h"
#include "config.h"

std::ostream& logger = std::clog;

Button sdl_to_button(int x)
{
	switch (x) {
		case JOYPAD_1_A:
			return Button::a;
		case JOYPAD_1_B:
			return Button::b;
		case JOYPAD_1_SELECT:
			return Button::select;
		case JOYPAD_1_START:
			return Button::start;
		case JOYPAD_1_UP:
			return Button::up;
		case JOYPAD_1_DOWN:
			return Button::down;
		case JOYPAD_1_LEFT:
			return Button::left;
		case JOYPAD_1_RIGHT:
			return Button::right;
	}
}

void run_loop(Console& console)
{
	SDL_Event event;
	unsigned cpu_cycles = 0;

	for (;;) {
		if (cpu_cycles == 0) {
			while (SDL_PollEvent(&event)) {
				switch (event.type) {
				case SDL_QUIT:
					return;
				case SDL_KEYDOWN:
					switch (event.key.keysym.sym) {
					case JOYPAD_1_A:
					case JOYPAD_1_B:
					case JOYPAD_1_SELECT:
					case JOYPAD_1_START:
					case JOYPAD_1_UP:
					case JOYPAD_1_DOWN:
					case JOYPAD_1_LEFT:
					case JOYPAD_1_RIGHT:
						screen.set_joypad_state(0, sdl_to_button(event.key.keysym.sym));
						break;
					}
					break;
				case SDL_KEYUP:
					switch (event.key.keysym.sym) {
					case JOYPAD_1_A:
					case JOYPAD_1_B:
					case JOYPAD_1_SELECT:
					case JOYPAD_1_START:
					case JOYPAD_1_UP:
					case JOYPAD_1_DOWN:
					case JOYPAD_1_LEFT:
					case JOYPAD_1_RIGHT:
						screen.clear_joypad_state(0, sdl_to_button(event.key.keysym.sym));
						break;
					}
					break;
				}
			}

			cpu_cycles = cpu.step();
		}

		ppu.step();
		--cpu_cycles;
	}
}

int main(int argc, char** argv)
{
	Console console;

	if (argc < 2) {
		logger << "not enough arguments...\n";
		return EXIT_FAILURE;
	}

	console.load(argv[1]);
	run_loop(console);

	return EXIT_SUCCESS;
}
