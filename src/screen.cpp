#include "screen.h"
#include "common.h"
#include "config.h"

Screen screen;

static void sdl_assert(bool cond)
{
	if (!cond) {
		std::ostringstream err{ "SDL Error: " };
		err << SDL_GetError();
		GLOBAL_ERROR(err.str().c_str());
	}
}

Screen::Screen()
{
	joypad_state[0] = 0;
	joypad_state[1] = 0;

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

static int button_mapping(Button b)
{
	switch (b) {
		case Button::a:
			return 0;
		case Button::b:
			return 1;
		case Button::select:
			return 2;
		case Button::start:
			return 3;
		case Button::up:
			return 4;
		case Button::down:
			return 5;
		case Button::left:
			return 6;
		case Button::right:
			return 7;
	}
}

uint8_t Screen::get_joypad_state(int controller_number)
{
	return joypad_state[controller_number];
}

bool Screen::button_pressed(int controller, Button button)
{
	return joypad_state[controller] & BIT(button_mapping(button));
}

void Screen::set_joypad_state(int controller, Button button)
{
	joypad_state[controller] |= BIT(button_mapping(button));
}

void Screen::clear_joypad_state(int controller, Button button)
{
	joypad_state[controller] &= ~BIT(button_mapping(button));
}