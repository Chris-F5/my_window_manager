#ifndef MWM_DECORATION
#define MWM_DECORATION

#include <xcb/xcb.h>

typedef struct {
    xcb_font_t xFont;
    int ascent, decent;
} Font;

typedef struct {
    int x, y, w, h;
    xcb_drawable_t drawable;
    xcb_gcontext_t graphicsCtx;
} Decoration;

void Font_init(Font* font, xcb_connection_t* con, char* fontName);

void Font_destroy(Font* font, xcb_connection_t* con);

void Decoration_init(
    Decoration* d,
    xcb_connection_t* con,
    xcb_screen_t* screen,
    int x, int y, int w, int h);

void Decoration_drawRect(
    Decoration* d,
    xcb_connection_t* con,
    int x, int y, int w, int h,
    uint32_t col);

void Decoration_drawText(
    Decoration* d,
    xcb_connection_t* con,
    int x, int y,
    char* text,
    Font* font,
    uint32_t foreground,
    uint32_t background);

void Decoration_expose(
    Decoration* d,
    xcb_connection_t* con,
    xcb_screen_t* screen);

void Decoration_destroy(Decoration* d, xcb_connection_t* con);

#endif
