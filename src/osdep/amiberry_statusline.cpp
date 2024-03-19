#include "SDL.h"
#include "SDL_ttf.h"

#include "sysconfig.h"
#include "sysdeps.h"

#include "options.h"
#include "uae.h"
#include "amiberry_gfx.h"
#include "picasso96.h"
#include "statusline.h"
#include "gui.h"
#include "xwin.h"

static SDL_Renderer* statusline_renderer;
static SDL_Surface* statusline_bitmap;
static SDL_Texture* statusline_texture;
static TTF_Font* statusline_font;
static SDL_Color statusline_color;
static int statusline_width;
static int statusline_height = TD_TOTAL_HEIGHT;
static char* td_new_numbers;
static int statusline_fontsize, statusline_fontstyle, statusline_fontweight;
static bool statusline_customfont;
static char statusline_fontname[256];

void deletestatusline(int monid)
{
	if (monid)
		return;

	if (!statusline_renderer)
		return;
	if (statusline_bitmap)
		SDL_FreeSurface(statusline_bitmap);
	if (statusline_renderer)
		SDL_DestroyRenderer(statusline_renderer);
	if (statusline_texture)
		SDL_DestroyTexture(statusline_texture);
	if (statusline_font)
		TTF_CloseFont(statusline_font);

	statusline_bitmap = NULL;
	statusline_texture = NULL;
	statusline_renderer = NULL;
	statusline_font = NULL;
}

static char* ldp_font_bitmap;
static int ldp_font_width, ldp_font_height;

static void create_ldp_font(SDL_Window* parent)
{
	//TODO : Implement
}

static void create_led_font(SDL_Window* parent, int monid)
{
	int width = 128;
	int height = 128;
	void* bm;

	statusline_set_font(NULL, 0, 0);

	xfree(td_new_numbers);

	statusline_fontsize = 8;
	_tcscpy(statusline_fontname, "FreeMono");
	statusline_fontweight = TTF_STYLE_NORMAL;

	int y = getdpiforwindow(parent);
	statusline_fontsize = -statusline_fontsize * y / 72;
	statusline_fontsize = statusline_fontsize * statusline_get_multiplier(monid) / 100;

	TTF_Font* font = TTF_OpenFont(statusline_fontname, statusline_fontsize);
	if (font) {

		int w = 0;
		int h = TTF_FontHeight(font) + 2;
		for (int i = 0; i < td_characters[i]; i++) {
			int sz_w, sz_h;
			if (TTF_SizeText(font, &td_characters[i], &sz_w, &sz_h) == 0) {
				if (sz_w > w)
					w = sz_w;
				if (sz_h > h)
					h = sz_h;
			}
		}
		int offsetx = 10;
		int offsety = 9;
		w += 1;
		if (h < -statusline_fontsize) {
			h = -statusline_fontsize;
		}
		td_new_numbers = (char*)calloc(w * h * NUMBERS_NUM, sizeof(char));
		if (td_new_numbers) {
			SDL_Color textColor = { 255, 255, 255, 255 }; // white color
			for (int i = 0; i < td_characters[i]; i++) {
				SDL_Surface* text_surface = TTF_RenderText_Solid(font, &td_characters[i], textColor);
				if (text_surface) {
					for (int y = 0; y < h; y++) {
						uae_u8* src = (uae_u8*)bm + (y + offsety) * width + offsetx;
						char* dst = td_new_numbers + i * w + h * w * NUMBERS_NUM;
						for (int x = 0; x < w; x++) {
							uae_u8 b = *src++;
							if (b == 2 && dst[0] == '-') {
								*dst = '+';
							}
							dst++;
						}
					}
					SDL_FreeSurface(text_surface);
				}
			}
		}
		statusline_set_font(td_new_numbers, w, h);
		TTF_CloseFont(font);
	}
}

bool createstatusline(SDL_Window* parentWindow, int monid)
{
	struct AmigaMonitor* mon = &AMonitors[monid];

	if (monid)
		return false;

	deletestatusline(mon->monitor_id);
	statusline_renderer = SDL_CreateRenderer(parentWindow, -1, SDL_RENDERER_ACCELERATED);
	if (!statusline_renderer)
		return false;

	create_led_font(parentWindow, monid);
	create_ldp_font(parentWindow);

	statusline_width = (mon->currentmode.current_width + 31) & ~31;
	statusline_fontsize -= (2 * TD_DEFAULT_PADY - 1);
	statusline_height = -statusline_fontsize;

	statusline_font = TTF_OpenFont("FreeMono", statusline_fontsize);
	if (!statusline_font) {
		SDL_Log("Unable to load font: %s", TTF_GetError());
		return false;
	}

	statusline_color = { 255, 255, 255, 255 }; // white color

	return true;
}

void statusline_updated(int monid)
{
	if (monid)
		return;
	struct AmigaMonitor* mon = &AMonitors[monid];
	//if (mon->hStatusWnd)
		SDL_RenderPresent(statusline_renderer);
}

void statusline_render(int monid, uae_u8* buf, int bpp, int pitch, int maxwidth, int maxheight, uae_u32* rc, uae_u32* gc, uae_u32* bc, uae_u32* alpha)
{
	struct AmigaMonitor* mon = &AMonitors[monid];
	uae_u32 white = rc[0xff] | gc[0xff] | bc[0xff] | (alpha ? alpha[0xff] : 0);
	uae_u32 black = rc[0x00] | gc[0x00] | bc[0x00] | (alpha ? alpha[0xa0] : 0);

	if (monid || !statusline_renderer) {
		return;
	}

	const TCHAR* text = statusline_fetch();
	if (!text)
		return;

	SDL_Surface* textSurface = TTF_RenderText_Solid(statusline_font, text, statusline_color);
	statusline_texture = SDL_CreateTextureFromSurface(statusline_renderer, textSurface);
	SDL_FreeSurface(textSurface);

	SDL_Rect textRect;
	textRect.x = 0;
	textRect.y = 0;
	SDL_QueryTexture(statusline_texture, NULL, NULL, &textRect.w, &textRect.h);

	SDL_RenderCopy(statusline_renderer, statusline_texture, NULL, &textRect);
	SDL_RenderPresent(statusline_renderer);
}

void ldp_render(const char* txt, int len, uae_u8* buf, struct vidbuffer* vd, int dx, int dy, int mx, int my)
{
	if (!ldp_font_bitmap) {
		return;
	}
	int bpp = vd->pixbytes;
	uae_u32 white = 0xffffff;
	uae_u8* dbuf2 = buf + dy * vd->rowbytes + dx * bpp;
	for (int i = 0; i < len; i++) {
		uae_u8* dbuf = dbuf2 + i * ldp_font_width * mx * bpp;
		char ch = *txt++;
		if (ch >= 128 || ch < 32) {
			ch = 0;
		}
		else {
			ch -= 32;
		}
		char* font = ldp_font_bitmap + ch * ldp_font_width * ldp_font_height;
		for (int y = 0; y < ldp_font_height; y++) {
			for (int mmy = 0; mmy < my; mmy++) {
				char* font2 = font;
				for (int x = 0; x < ldp_font_width; x++) {
					if (*font2) {
						for (int mmx = 0; mmx < mx; mmx++) {
							int xx = x * mx + mmx;
							if (dy + y * my + mmy >= 0 && dy + y * my + mmy < vd->inheight) {
								if (dx + xx >= 0 && dx + xx < vd->inwidth) {
									dbuf[xx * bpp + 0] = 0xff;
									dbuf[xx * bpp + 1] = 0xff;
									if (bpp == 4) {
										dbuf[xx * bpp + 2] = 0xff;
										dbuf[xx * bpp + 3] = 0xff;
									}
								}
							}
						}
					}
					font2++;
				}
				dbuf += vd->rowbytes;
			}
			font += ldp_font_width;
		}
		dx += ldp_font_width;
	}
}