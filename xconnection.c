#include "./xconnection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xproto.h>

void XConnection_init(XConnection* x)
{
    /* connect */
    {
        int conErr;

        /* use DISPLAY enviroment varable for screen number */
        x->con = xcb_connect(NULL, &x->screenNum);
        if((conErr = xcb_connection_has_error(x->con))) {
            fprintf(
                stderr,
                "failed to connect to x server. error code %d\n",
                conErr);
            exit(1);
        }
    }

    /* find screen */
    {
        const xcb_setup_t* setup;
        xcb_screen_iterator_t screenIter;
        int i;

        setup = xcb_get_setup(x->con);

        screenIter = xcb_setup_roots_iterator(setup);
        for(i = 0; i < x->screenNum; i++)
            xcb_screen_next(&screenIter);
        x->screen = screenIter.data;

        x->minKeycode = setup->min_keycode;
        x->maxKeycode = setup->max_keycode;
    }

    /* setup keymap */
    {
        xcb_get_keyboard_mapping_cookie_t cookie;
        xcb_generic_error_t* error;
        xcb_get_keyboard_mapping_reply_t* reply;
        int keycodeCount, i;

        keycodeCount = x->maxKeycode - x->minKeycode + 1;
        cookie = xcb_get_keyboard_mapping(
            x->con,
            x->minKeycode,
            keycodeCount);
        reply = xcb_get_keyboard_mapping_reply(
            x->con,
            cookie,
            &error);

        if(reply == NULL) {
            fprintf(
                stderr,
                "failed to get keyboard mapping (%d)\n",
                error->error_code);
            free(error);
            exit(1);
        }

        xcb_keysym_t* replyKeymap = xcb_get_keyboard_mapping_keysyms(reply);
        x->keymap = malloc(keycodeCount * sizeof(xcb_keysym_t));
        for(i = 0; i < keycodeCount; i++) {
            x->keymap[i] = replyKeymap[i * reply->keysyms_per_keycode];
        }

        free(reply);
    }
}

xcb_keysym_t XConnection_keycodeToKeysym(XConnection* x, xcb_keycode_t keycode)
{
    if(keycode < x->minKeycode || keycode > x->maxKeycode)
        return 0;
    
    return x->keymap[keycode - x->minKeycode];
}

xcb_keycode_t XConnection_keysymToKeycode(XConnection* x, xcb_keysym_t keysym)
{
    for(int keycode = x->minKeycode; keycode <= x->maxKeycode; keycode++) {
        if(XConnection_keycodeToKeysym(x, keycode) == keysym) {
            return keycode;
        }
    }
    return 0;
}

void XConnection_destroy(XConnection* x)
{
    xcb_disconnect(x->con);
    free(x->keymap);
}

void XCursor_init(XCursor* c, XConnection* x)
{
    c->font = xcb_generate_id(x->con);
    xcb_open_font(x->con, c->font, strlen("cursor"), "cursor");

    c->cursor = xcb_generate_id(x->con);
    xcb_create_glyph_cursor(
        x->con,
        c->cursor,
        c->font,
        c->font, 
        58,
        58 + 1,
        0, 0, 0,
        0, 0, 0);
}

void XCursor_destroy(XCursor* c, XConnection* x)
{
    xcb_free_cursor(x->con, c->cursor);
    xcb_close_font(x->con, c->font);
}
