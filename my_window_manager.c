#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>

#include "./decoration.h"
#include "./xconnection.h"

#define MAX_EVENT_CODE 34

typedef struct {
    int monX, monY, monW, monH;
    int winX, winY, winW, winH;
} Monitor;

typedef struct {
    unsigned int mod;
    xcb_keysym_t keysym;
    void (*func)(void);
} KeyboardShortcut;

void spawn(char** args);
void menu(void);
void configureRequest(XConnection* x, xcb_generic_event_t* genericEvent);
void mapRequest(XConnection* x, xcb_generic_event_t* genericEvent);
void keyPress(XConnection* x, xcb_generic_event_t* genericEvent);
void expose(XConnection* x, xcb_generic_event_t* genericEvent);

static KeyboardShortcut keyShortcuts[] = {
    {XCB_MOD_MASK_1, 0xff0d, menu}
};
static void (*eventHandlers[MAX_EVENT_CODE + 1])(XConnection*, xcb_generic_event_t*) = {
    [XCB_KEY_PRESS] = keyPress,
    [XCB_EXPOSE] = expose,
    [XCB_MAP_REQUEST] = mapRequest,
    [XCB_CONFIGURE_REQUEST] = configureRequest,
};

static Decoration testDecoration;
static Font font;
static int quit = 0;

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

void configureRequest(XConnection* x, xcb_generic_event_t* genericEvent)
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
    xcb_configure_window(x->con, x->screen->root, mask, values);
    printf("configured window %d %d %d %d\n", event->x, event->y, event->width, event->height);
    xcb_flush(x->con);
}

void mapRequest(XConnection* x, xcb_generic_event_t* genericEvent)
{
    xcb_map_request_event_t* event = (xcb_map_request_event_t*)genericEvent;
    xcb_map_window(x->con, event->window);
    xcb_flush(x->con);
}

void keyPress(XConnection* x, xcb_generic_event_t* genericEvent)
{
    xcb_key_press_event_t* event;
    xcb_keysym_t keysym;

    event = (xcb_key_press_event_t*)genericEvent;
    keysym = XConnection_keycodeToKeysym(x, event->detail);

    for(int i = 0; i < sizeof(keyShortcuts) / sizeof(keyShortcuts[0]); i++) {
        if(keyShortcuts[i].mod == event->state 
        && keyShortcuts[i].keysym == keysym) {
            keyShortcuts[i].func();
        }
    }
}

void expose(XConnection* x, xcb_generic_event_t* genericEvent)
{
    Decoration_expose(&testDecoration, x->con, x->screen);
    xcb_flush(x->con);
}

int main()
{
    XConnection x;
    XCursor cursor;
    xcb_keycode_t keycode;

    XConnection_init(&x);

    xcb_ungrab_key(x.con, XCB_GRAB_ANY, x.screen->root, XCB_MOD_MASK_ANY);
    for(int i = 0; i < sizeof(keyShortcuts) / sizeof(keyShortcuts[i]); i++) {
        keycode = XConnection_keysymToKeycode(&x, keyShortcuts[i].keysym);
        if(keycode == 0) {
            fprintf(stderr, "failed to register key shortcut\n");
        } else {
            xcb_grab_key(
                x.con,
                1,
                x.screen->root,
                keyShortcuts[i].mod,
                keycode,
                XCB_GRAB_MODE_ASYNC,
                XCB_GRAB_MODE_ASYNC);
        }
    }

    XCursor_init(&cursor, &x);

    /* DECORATION */
    Font_init(&font, x.con, "fixed");
    Decoration_init(&testDecoration, x.con, x.screen, 400, 600, 800, 200);
    Decoration_drawRect(&testDecoration, x.con, 0, 0, 800, 200, x.screen->white_pixel);
    Decoration_drawText(
        &testDecoration,
        x.con,
        0, 0,
        "Hiy AAAAAAA",
        &font,
        x.screen->white_pixel,
        x.screen->black_pixel);

    /* WINDOW ATTRIBS */
    uint32_t values[] = {
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
        | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
        | XCB_EVENT_MASK_STRUCTURE_NOTIFY
        | XCB_EVENT_MASK_KEY_PRESS
        | XCB_EVENT_MASK_EXPOSURE
    };
    xcb_void_cookie_t cookie;
    cookie = xcb_change_window_attributes_checked(
        x.con,
        x.screen->root,
        XCB_CW_EVENT_MASK,
        values);
    xcb_generic_error_t* err;
    if((err = xcb_request_check(x.con, cookie))) {
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
        xcb_disconnect(x.con);
        exit(1);
    }
    xcb_flush(x.con);

    /* EVENT LOOP */
    while(quit == 0) {
        int conErr;
        if((conErr = xcb_connection_has_error(x.con))) {
            fprintf(
                stderr,
                "x server connection unexpectedly closed. error code %d\n",
                conErr);
            exit(1);
        }
        xcb_generic_event_t* event = xcb_wait_for_event(x.con);

        unsigned char eventCode = event->response_type & ~0x80;
        if(eventCode > MAX_EVENT_CODE)
        {
            fprintf(stderr, "received event code greater than MAX_EVENT_CODE");
            eventCode = 0;
        }
        void (*handlerFunc)(XConnection*, xcb_generic_event_t*) = eventHandlers[eventCode];
        if(handlerFunc)
            handlerFunc(&x, event);
    }

    /* CLEANUP */

    XCursor_destroy(&cursor, &x);
    XConnection_destroy(&x);

    return 0;
}
