#include "gfx_sdl.h"


PixelBufferSdl::PixelBufferSdl(GraphicsSdl& graphics)
	: cursor(0)
{
	SDL_Surface& surface = *graphics.surface;
	pixels = static_cast<Color*>(surface.pixels);
	window = graphics.window;
	h = surface.h;
	w = surface.h;
	pitch = surface.pitch;
}

auto PixelBufferSdl::get(unsigned r, unsigned c) -> Color
{
	return pixels[c * pitch + r];
}

auto PixelBufferSdl::set(unsigned r, unsigned c, Color value) -> void
{
	printf("set(r=%u,c=%u,val=%X)\n", r,c,value);
	pixels[r * h + c] = value;
}

auto PixelBufferSdl::operator<<(Color value) -> PixelBuffer&
{
	pixels[cursor++] = value;
	if (cursor % pitch >= w) {
		cursor += pitch - w;
	}
	return *this;
}

auto PixelBufferSdl::update() -> void
{
	SDL_UpdateWindowSurface(window);
}

auto PixelBufferSdl::reset_cursor() -> void
{
	cursor = 0;
}

GraphicsSdl::GraphicsSdl()
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

GraphicsSdl::~GraphicsSdl()
{
	SDL_DestroyWindow(window);
	SDL_Quit();
}

auto GraphicsSdl::pixel_buffer() -> std::unique_ptr<PixelBuffer>
{
	return std::make_unique<PixelBufferSdl>(*this);
}