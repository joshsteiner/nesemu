#ifndef NESEMU_SCREEN_H
#define NESEMU_SCREEN_H

#include <SDL.h>
#undef main

#include <cstdint>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>

#include "controller.h"

using Color = uint32_t;

const size_t display_width = 256;
const size_t display_height = 240;

const std::string window_title = "nesemu";

class Screen {
public:
	Screen();
	~Screen();

	Color get_fg(unsigned r, unsigned c);
	void set_fg(unsigned r, unsigned c, Color value);
	Color get_bg(unsigned r, unsigned c);
	void set_bg(unsigned r, unsigned c, Color value);

	void swap();
	void render();

	uint8_t get_joypad_state(int controller_number);

private:
	SDL_Window* window;
	SDL_Surface* surface;
	Color front[display_height][display_width];
	Color back[display_height][display_width];
};

extern Screen screen;
extern uint8_t joypad_state[2];

#endif
