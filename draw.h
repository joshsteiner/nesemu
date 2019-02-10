#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include "../SDL-2.0.7/include/SDL.h"
#include "../SDL-2.0.7/include/SDL_main.h"

#define SCREEN_WIDTH   640
#define SCREEN_HEIGHT  480

#define BWIDTH  4
#define BHEIGHT 4

typedef int Tile;

typedef enum
{
	LEFT, RIGHT, UP, DOWN
} Dir;

typedef struct
{
	int r, c;
} Pos;

typedef struct
{
	int h, w;
	Tile **cells;
} Board;

typedef struct
{
	int pts;
	int curr;
	Board *boards[2];
} BoardSave;

typedef struct
{
	int time;
	int pts;
	int boardh, boardw;
} Score;

typedef struct
{
	int capacity, length;
	Score *scores;
} ScoreBoard;

typedef struct
{
	int length;
	Pos *elemidx;
	Board *board_ref;
} BoardSlice;

typedef struct
{
	enum { SHIFT, SHIFT_MERGE } type;
	Pos src, dest;
	int val;
} Animation;

typedef struct
{
	Animation *begin, *end;
	Pos *clear_begin, *clear_end;
} Animations;

typedef struct
{
	SDL_Window   *window;
	SDL_Renderer *renderer;
	SDL_Surface  *screen;
	SDL_Surface  *charset;
	SDL_Texture  *scrtex;
	struct {
		Uint32 black, white, red, green, blue;
	} pallete;
} Graphics;

// draw a text txt on surface screen, starting from the point (x, y)
// charset is a 128x128 bitmap containing character images
void DrawString(Graphics *graphics, int x, int y, const char *text, double scale)
{
	int px, py, c;
	SDL_Rect s, d;
	s.w = 8;
	s.h = 8;
	d.w = 8 * scale;
	d.h = 8 * scale;
	while (*text) {
		c = *text & 255;
		px = (c % 16) * 8;
		py = (c / 16) * 8;
		s.x = px;
		s.y = py;
		d.x = x;
		d.y = y;
		SDL_BlitScaled(graphics->charset, &s, graphics->screen, &d);
		//SDL_BlitSurface(graphics->charset, &s, graphics->screen, &d);
		x += 8 * scale;
		text++;
	}
}

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

// draw a vertical (when dx = 0, dy = 1) or horizontal (when dx = 1, dy = 0) line
void DrawLine(SDL_Surface *screen, int x, int y, int l, int dx, int dy,
              Uint32 color) {
	for(int i = 0; i < l; i++) {
		DrawPixel(screen, x, y, color);
		x += dx;
		y += dy;
	}
}

// draw a rectangle of size l by k
void DrawRectangle(SDL_Surface *screen, int x, int y, int l, int k,
                   Uint32 outlineColor, Uint32 fillColor)
{
	int i;
	DrawLine(screen, x, y, k, 0, 1, outlineColor);
	DrawLine(screen, x + l - 1, y, k, 0, 1, outlineColor);
	DrawLine(screen, x, y, l, 1, 0, outlineColor);
	DrawLine(screen, x, y + k - 1, l, 1, 0, outlineColor);
	for(i = y + 1; i < y + k - 1; i++)
		DrawLine(screen, x + 1, i, l - 2, 1, 0, fillColor);
}


void draw_info(Graphics *graphics, Uint32 outline, Uint32 fill, int pts, int t)
{
	char text[128];

	DrawRectangle(graphics->screen, 4, 4, SCREEN_WIDTH - 8, 36, outline, fill);
	sprintf(text, "2048 Joshua Steiner 175644 pkty: %d czas: %d", pts, t);
	DrawString(graphics, graphics->screen->w / 2 - strlen(text) * 8 / 2, 26,
	           text, 1.0);
}

int num_of_digits(int n)
{
	int r = ceil(log10((float)n));
	return r;
}

void draw_tile(Graphics *graphics, int x, int y, int value, Uint32 color)
{
	DrawRectangle(graphics->screen, x, y, 100, 100, graphics->pallete.black, color);
	if (value) {
		char str[100];
		sprintf(str, "%d ", value);
		DrawString(graphics, x + 10, y + 10, str, 5.0 - num_of_digits(value) * 0.6);
	}
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
