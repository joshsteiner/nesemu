#ifndef NESEMU_SCREEN_H
#define NESEMU_SCREEN_H

#include <SDL.h>
#undef main

#include <cstdint>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>

using Color = uint32_t;

const size_t display_width  = 256;
const size_t display_height = 240;

const std::string window_title = "nesemu";

class Screen {
public:
	Screen();
	~Screen();

	Color get(unsigned r, unsigned c);
	void set(unsigned r, unsigned c, Color value);
	void update();

private:
	SDL_Window* window;
	SDL_Surface* surface;

};

extern Screen screen;

#endif
