/*#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

//#include <SDL.h>
//#include <SDL_main.h>

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 256

struct Graphics
{
	SDL_Window   *window;
	SDL_Renderer *renderer;
	SDL_Surface  *screen;
	SDL_Surface  *charset;
	SDL_Texture  *scrtex;
	struct {
		Uint32 black, white, red, green, blue;
	} pallete;
};

// draw a surface sprite on a surface screen in point (x, y)
// (x, y) is the center of sprite on screen
void DrawSurface(SDL_Surface *screen, SDL_Surface *sprite, int x, int y) {
	SDL_Rect dest;
	dest.x = x - sprite->w / 2;
	dest.y = y - sprite->h / 2;
	dest.w = sprite->w;
	dest.h = sprite->h;
	SDL_BlitSurface(sprite, NULL, screen, &dest);
}

// draw a single pixel
void DrawPixel(SDL_Surface *surface, int x, int y, Uint32 color) {
	int bpp = surface->format->BytesPerPixel;
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
	*(Uint32 *)p = color;
}

void graphics_init(Graphics *graphics)
{
	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, 0,
	                            &graphics->window, &graphics->renderer);

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	SDL_RenderSetLogicalSize(graphics->renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
	SDL_SetRenderDrawColor(graphics->renderer, 0, 0, 0, 255);

	SDL_SetWindowTitle(graphics->window, "2048");

	graphics->screen = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32,
	                                        0x00FF0000, 0x0000FF00, 0x000000FF,
											0xFF000000);

	graphics->scrtex = SDL_CreateTexture(graphics->renderer,
	                                     SDL_PIXELFORMAT_ARGB8888,
										 SDL_TEXTUREACCESS_STREAMING,
										 SCREEN_WIDTH, SCREEN_HEIGHT);

	SDL_ShowCursor(SDL_DISABLE);

	graphics->charset = SDL_LoadBMP("./cs8x8.bmp");

	SDL_SetColorKey(graphics->charset, true, 0x000000);

	graphics->pallete.black = SDL_MapRGB(graphics->screen->format, 0x00, 0x00, 0x00);
	graphics->pallete.white = SDL_MapRGB(graphics->screen->format, 0xFF, 0xFF, 0xFF);
	graphics->pallete.red   = SDL_MapRGB(graphics->screen->format, 0xFF, 0x00, 0x00);
	graphics->pallete.green = SDL_MapRGB(graphics->screen->format, 0x00, 0xFF, 0x00);
	graphics->pallete.blue  = SDL_MapRGB(graphics->screen->format, 0x00, 0x00, 0xFF);
}

void graphics_clean(Graphics *graphics)
{
	SDL_FreeSurface(graphics->charset);
	SDL_FreeSurface(graphics->screen);
	SDL_DestroyTexture(graphics->scrtex);
	SDL_DestroyRenderer(graphics->renderer);
	SDL_DestroyWindow(graphics->window);
	SDL_Quit();
}

void clear(SDL_Surface *screen, Uint32 color)
{
	SDL_FillRect(screen, NULL, color);
}

void update(Graphics *graphics)
{
	SDL_UpdateTexture(graphics->scrtex, NULL, graphics->screen->pixels,
	                  graphics->screen->pitch);
	//SDL_RenderClear(graphics->renderer);
	SDL_RenderCopy(graphics->renderer, graphics->scrtex, NULL, NULL);
	SDL_RenderPresent(graphics->renderer);
}
*/