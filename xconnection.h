#ifndef MWM_XCONNECTION_H
#define MWM_XCONNECTION_H

#include <xcb/xcb.h>

typedef struct {
    xcb_connection_t* con;
    xcb_screen_t* screen;
    int screenNum;
    xcb_keycode_t minKeycode, maxKeycode;
    xcb_keysym_t* keymap;
} XConnection;

typedef struct {
    xcb_font_t font;
    xcb_cursor_t cursor;
} XCursor;

void XConnection_init(XConnection* x);
xcb_keysym_t XConnection_keycodeToKeysym(XConnection* x, xcb_keycode_t keycode);
xcb_keycode_t XConnection_keysymToKeycode(XConnection* x, xcb_keysym_t keysym);
void XConnection_destroy(XConnection* x);

void XCursor_init(XCursor* c, XConnection* x);
void XCursor_destroy(XCursor* c, XConnection* x);

#endif
