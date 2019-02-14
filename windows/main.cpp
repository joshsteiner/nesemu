#include "../src/nesemu.h"
#include "gfx_sdl.h"

auto run_loop() -> void
{
	SDL_Event event;

	for (;;) {
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_QUIT:
				return;
			}
		}
	}
}

auto main(int argc, char **argv) -> int
{
	GraphicsSdl gfx;
	Console console{ gfx.pixel_buffer() };

	run_loop();

	return 0;
}
