#include "./decoration.h"

#include <string.h>
#include <stdlib.h>

#include <xcb/xproto.h>

void Font_init(Font* font, xcb_connection_t* con, char* fontName)
{
    xcb_query_font_cookie_t fontCookie;
    xcb_query_font_reply_t* fontInfo;

    font->xFont = xcb_generate_id(con);
    xcb_open_font(con, font->xFont, strlen(fontName), fontName);

    fontCookie = xcb_query_font(con, font->xFont);
    fontInfo = xcb_query_font_reply(con, fontCookie, NULL);

    font->ascent = fontInfo->font_ascent;
    font->decent = fontInfo->font_descent;

    free(fontInfo);
}

void Font_destroy(Font* font, xcb_connection_t* con)
{
    xcb_close_font(con, font->xFont);
}

void Decoration_init(
    Decoration* d,
    xcb_connection_t* con,
    xcb_screen_t* screen,
    int x, int y, int w, int h)
{
    d->x = x;
    d->y = y;
    d->w = w;
    d->h = h;
    d->drawable = xcb_generate_id(con);
    xcb_create_pixmap(
        con,
        screen->root_depth,
        d->drawable,
        screen->root,
        d->w,
        d->h);
    d->graphicsCtx = xcb_generate_id(con);
    xcb_create_gc(con, d->graphicsCtx, d->drawable, 0, NULL);
}

void Decoration_drawRect(
    Decoration* d,
    xcb_connection_t* con,
    int x, int y, int w, int h,
    uint32_t col)
{
    uint32_t mask = XCB_GC_FOREGROUND;
    uint32_t values[] = {
        col
    };
    xcb_change_gc(con, d->graphicsCtx, mask, values);

    xcb_rectangle_t rect;
    rect.x = x;
    rect.y = y;
    rect.width = w;
    rect.height = h;
    xcb_poly_fill_rectangle(
        con,
        d->drawable,
        d->graphicsCtx,
        1,
        &rect);
}

void Decoration_drawText(
    Decoration* d,
    xcb_connection_t* con,
    int x, int y,
    char* text,
    Font* font,
    uint32_t foreground,
    uint32_t background)
{
    uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
    uint32_t values[] = {
        foreground, background, font->xFont
    };
    xcb_change_gc(con, d->graphicsCtx, mask, values);

    xcb_image_text_8(
        con,
        strlen(text),
        d->drawable,
        d->graphicsCtx,
        x, y + font->ascent,
        text);
}

void Decoration_expose(
    Decoration* d,
    xcb_connection_t* con,
    xcb_screen_t* screen)
{
    xcb_copy_area(
        con,
        d->drawable,
        screen->root,
        d->graphicsCtx,
        0, 0,
        d->x, d->y, d->w, d->h);
}

void Decoration_destroy(Decoration* d, xcb_connection_t* con)
{
    xcb_free_pixmap(con, d->drawable);
    xcb_free_gc(con, d->graphicsCtx);
}
