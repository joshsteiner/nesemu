#include "screen.h"
#include "common.h"
#include "config.h"

static void sdl_assert(bool cond)
{
	if (!cond) {
		std::ostringstream err{ "SDL Error: " };
		err << SDL_GetError();
		throw err.str();
	}
}

Screen::Screen()
{
	sdl_assert(SDL_Init(SDL_INIT_EVERYTHING) == 0);

	window = SDL_CreateWindow(
		window_title.c_str(),
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		display_width, display_height,
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

Color Screen::get_fg(unsigned r, unsigned c)
{
	r %= display_height;
	c %= display_width;
	return front[r][c];
}

void Screen::set_fg(unsigned r, unsigned c, Color value)
{
	r %= display_height;
	c %= display_width;
	front[r][c] = value;
}

Color Screen::get_bg(unsigned r, unsigned c)
{
	r %= display_height;
	c %= display_width;
	return back[r][c];
}

void Screen::set_bg(unsigned r, unsigned c, Color value)
{
	LOG_FMT("set=%d,%d", r, c);
	r %= display_height;
	c %= display_width;
	back[r][c] = value;
}

void Screen::swap()
{
	std::swap(front, back);
}

void Screen::render()
{
	auto pixels = static_cast<Color*>(surface->pixels);

	for (int r = 0; r < display_height; ++r) {
		for (int c = 0; c < display_width; ++c) {
			pixels[r * surface->pitch / 4 + c] = front[r][c];
		}
	}

	SDL_UpdateWindowSurface(window);
}

uint8_t Screen::get_joypad_state(int controller_number)
{
	return joypad_state[controller_number];
}
