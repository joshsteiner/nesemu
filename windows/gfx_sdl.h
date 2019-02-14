#pragma once

#include <SDL.h>
#undef main

#include <sstream>
#include <memory>

#include "../src/graphics.h"

class GraphicsSdl;

inline auto sdl_assert(bool cond) -> void
{
	if (!cond) {
		std::ostringstream err{ "SDL Error: " };
		err << SDL_GetError();
		throw err.str();
	}
}

class PixelBufferSdl : public PixelBuffer
{
private:
	unsigned cursor;
	Color* pixels;
	unsigned h, w, pitch;

public:
	PixelBufferSdl(GraphicsSdl& graphics);

public:
	auto get(unsigned r, unsigned c) -> Color override;
	auto set(unsigned r, unsigned c, Color value) -> void override;
	auto operator<<(Color value) -> PixelBuffer& override;
	auto reset_cursor() -> void;
};

class GraphicsSdl 
{
	friend PixelBufferSdl;

private:
	SDL_Window* window;
	SDL_Surface* surface;

public:
	GraphicsSdl();
	~GraphicsSdl();

public:
	auto pixel_buffer() -> std::unique_ptr<PixelBuffer>;
};