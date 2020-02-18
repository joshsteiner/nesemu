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

enum class Button {
	a, b,
	select, start,
	up, down, left, right
};

class Screen {
public:
	Screen();
	~Screen();

	void set_bg(unsigned r, unsigned c, Color value);

	void swap();
	void render();

	uint8_t get_joypad_state(int controller_number);
	bool button_pressed(int controller, Button button);
	void set_joypad_state(int controller, Button button);
	void clear_joypad_state(int controller, Button button);

private:
	SDL_Window* window;
	SDL_Surface* surface;
	uint8_t joypad_state[2];
	Color front[display_height][display_width];
	Color back[display_height][display_width];
};

extern Screen screen;

#endif
