#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>

#include "./decoration.h"

#define MAX_EVENT_CODE 34

typedef struct {
    int monX, monY, monW, monH;
    int winX, winY, winW, winH;
} Monitor;

typedef struct {
    unsigned int mod;
    xcb_keysym_t keySym;
    void (*func)(void);
} KeyboardShortcut;

void spawn(char** args);
void menu(void);
void configureRequest(xcb_generic_event_t* genericEvent);
void mapRequest(xcb_generic_event_t* genericEvent);
void keyPress(xcb_generic_event_t* genericEvent);
void expose(xcb_generic_event_t* genericEvent);

static xcb_connection_t* con;
static xcb_screen_t* screen;
static const xcb_setup_t* setup;
static Font font;
static Decoration testDecoration;
static int quit = 0;
static KeyboardShortcut keyShortcuts[] = {
    {XCB_MOD_MASK_1, 0xff0d, menu}
};
static void (*eventHandlers[MAX_EVENT_CODE + 1])(xcb_generic_event_t*) = {
    [XCB_KEY_PRESS] = keyPress,
    [XCB_EXPOSE] = expose,
    [XCB_MAP_REQUEST] = mapRequest,
    [XCB_CONFIGURE_REQUEST] = configureRequest,
};

void spawn(char** args)
{
    if(fork() == 0)
    {
        setsid();
        execvp(args[0], args);
        exit(0);
    }
}

void menu(void)
{
    char* args[] = {"dmenu_run", NULL};
    spawn(args);
}

void configureRequest(xcb_generic_event_t* genericEvent)
{
    xcb_configure_request_event_t* event 
        = (xcb_configure_request_event_t*)genericEvent;
    uint16_t mask
        = XCB_CONFIG_WINDOW_X
        | XCB_CONFIG_WINDOW_Y
        | XCB_CONFIG_WINDOW_WIDTH
        | XCB_CONFIG_WINDOW_HEIGHT
        | XCB_CONFIG_WINDOW_BORDER_WIDTH
        | XCB_CONFIG_WINDOW_SIBLING
        | XCB_CONFIG_WINDOW_STACK_MODE;
    uint32_t values[] = {
        event->x,
        event->y,
        event->width,
        event->height,
        event->border_width,
        event->sibling,
        event->stack_mode,
    };
    xcb_configure_window(con, screen->root, mask, values);
    printf("configured window %d %d %d %d\n", event->x, event->y, event->width, event->height);
    xcb_flush(con);
}

void mapRequest(xcb_generic_event_t* genericEvent)
{
    xcb_map_request_event_t* event = (xcb_map_request_event_t*)genericEvent;
    xcb_map_window(con, event->window);
    xcb_flush(con);
}

void keyPress(xcb_generic_event_t* genericEvent)
{
    printf("key\n");
    xcb_key_press_event_t* event = (xcb_key_press_event_t*)genericEvent;

    xcb_key_symbols_t* symbols = xcb_key_symbols_alloc(con);
    xcb_keysym_t keySym;
    keySym = symbols 
        ? xcb_key_symbols_get_keysym(symbols, event->detail, 0) 
        : 0;
    xcb_key_symbols_free(symbols);

    for(int i = 0; i < sizeof(keyShortcuts) / sizeof(keyShortcuts[0]); i++) {
        if(keyShortcuts[i].mod == event->state 
        && keyShortcuts[i].keySym == keySym) {
            printf("KEY ACCEPT\n");
            keyShortcuts[i].func();
        }
    }
}

void expose(xcb_generic_event_t* genericEvent)
{
    Decoration_expose(&testDecoration, con, screen);
    xcb_flush(con);
}

int main()
{
    int conErr, i, screenNum;
    xcb_screen_iterator_t screenIter;
    xcb_void_cookie_t cookie;

    /* CONNECT */
    con = xcb_connect(NULL, &screenNum);
    if((conErr = xcb_connection_has_error(con))) {
        fprintf(
            stderr,
            "failed to connect to x server. error code %d\n",
            conErr);
        exit(1);
    }

    /* SETUP */
    setup = xcb_get_setup(con);
    screenIter = xcb_setup_roots_iterator(setup);
    for(i = 0; i < screenNum; i++)
        xcb_screen_next(&screenIter);
    screen = screenIter.data;
    xcb_ungrab_key(con, XCB_GRAB_ANY, screen->root, XCB_MOD_MASK_ANY);
    for(int i = 0; i < sizeof(keyShortcuts) / sizeof(keyShortcuts[i]); i++) {
        xcb_key_symbols_t* keySyms;
        xcb_keycode_t* keycode;
        keySyms = xcb_key_symbols_alloc(con);
        keycode = keySyms 
            ? xcb_key_symbols_get_keycode(keySyms, keyShortcuts[i].keySym)
            : NULL;
        xcb_key_symbols_free(keySyms);
        if(keycode == NULL) {
            fprintf(stderr, "failed to register key shortcut\n");
        } else {
            xcb_grab_key(
                con,
                1,
                screen->root,
                keyShortcuts[i].mod,
                *keycode,
                XCB_GRAB_MODE_ASYNC,
                XCB_GRAB_MODE_ASYNC);
        }
    }

    /* CURSOR */
    xcb_font_t cursorFont = xcb_generate_id(con);
    xcb_open_font(con, cursorFont, strlen("cursor"), "cursor");

    xcb_cursor_t cursor = xcb_generate_id(con);
    xcb_create_glyph_cursor(
        con,
        cursor,
        cursorFont,
        cursorFont, 
        58,
        58 + 1,
        0, 0, 0,
        0, 0, 0);

    /* DECORATION */
    Font_init(&font, con, "fixed");
    Decoration_init(&testDecoration, con, screen, 400, 600, 800, 200);
    Decoration_drawRect(&testDecoration, con, 0, 0, 800, 200, screen->white_pixel);
    Decoration_drawText(
        &testDecoration,
        con,
        0, 0,
        "Hiy AAAAAAA",
        &font,
        screen->white_pixel,
        screen->black_pixel);

    /* WINDOW ATTRIBS */
    uint32_t values[] = {
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
        | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
        | XCB_EVENT_MASK_STRUCTURE_NOTIFY
        | XCB_EVENT_MASK_KEY_PRESS
        | XCB_EVENT_MASK_EXPOSURE
    };
    cookie = xcb_change_window_attributes_checked(
        con,
        screen->root,
        XCB_CW_EVENT_MASK,
        values);
    xcb_generic_error_t* err;
    if((err = xcb_request_check(con, cookie))) {
        if(err->error_code == XCB_ACCESS) {
            fprintf(stderr, "failed to register for events. denied access. \
maybe another window manager is running?\n");
        } else {
            fprintf(
                stderr,
                "failed to register for events. error code %d\n",
                err->error_code);
        }
        free(err);
        xcb_disconnect(con);
        exit(1);
    }
    xcb_flush(con);

    /* EVENT LOOP */
    while(quit == 0) {
        if((conErr = xcb_connection_has_error(con))) {
            fprintf(
                stderr,
                "x server connection unexpectedly closed. error code %d\n",
                conErr);
            exit(1);
        }
        xcb_generic_event_t* event = xcb_wait_for_event(con);

        unsigned char eventCode = event->response_type & ~0x80;
        if(eventCode > MAX_EVENT_CODE)
        {
            fprintf(stderr, "received event code greater than MAX_EVENT_CODE");
            eventCode = 0;
        }
        void (*handlerFunc)(xcb_generic_event_t*) = eventHandlers[eventCode];
        if(handlerFunc)
            handlerFunc(event);
    }

    /* CLEANUP */
    xcb_free_cursor(con, cursor);
    xcb_disconnect(con);

    return 0;
}
