#include "screen.h"

Screen::Screen()
{
	sdl_assert(SDL_Init(SDL_INIT_EVERYTHING) == 0);

	window = SDL_CreateWindow(
		WindowTitle.c_str(),
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		DisplayWidth, DisplayHeight,
		0
	);

	sdl_assert(window != nullptr);

	surface = SDL_GetWindowSurface(window);
	sdl_assert(surface != nullptr);
}

Screen::~Screen()
{
	SDL_DestroyWindow(window);
	SDL_Quit();
}

auto Screen::get(unsigned r, unsigned c) -> Color
{
	auto pixels = static_cast<Color*>(surface->pixels);
	return pixels[c * surface->pitch + r];
}

auto Screen::set(unsigned r, unsigned c, Color value) -> void
{
	std::clog << "set(r=" << r << "c=" << c << "val=" << std::hex << value << '\n';
	auto pixels = static_cast<Color*>(surface->pixels);
	pixels[r * surface->pitch + c] = value;
}

auto Screen::update() -> void
{
	SDL_UpdateWindowSurface(window);
}
