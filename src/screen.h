#pragma once

#include <SDL.h>
#undef main

#include <cstdint>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>

using Color = uint32_t;

const size_t DisplayWidth = 256;
const size_t DisplayHeight = 240;

const std::string WindowTitle = "nesemu";

inline auto sdl_assert(bool cond) -> void
{
	if (!cond) {
		std::ostringstream err{ "SDL Error: " };
		err << SDL_GetError();
		throw err.str();
	}
}

class Screen
{
private:
	SDL_Window* window;
	SDL_Surface* surface;

public:
	Screen();
	~Screen();

public:
	auto get(unsigned r, unsigned c) -> Color;
	auto set(unsigned r, unsigned c, Color value) -> void;
	auto update() -> void;
};

extern Screen screen;
