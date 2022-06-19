#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define XK_MISCELLANY
#define XK_LATIN1
#include <X11/keysymdef.h>

#include "./decoration.h"

#define MAX_EVENT_CODE 34

typedef struct {
    int monX, monY, monW, monH;
    int winX, winY, winW, winH;
} Monitor;

typedef union {
    char** v;
} ShortcutArg;

typedef struct {
    unsigned int mod;
    xcb_keysym_t keysym;
    void (*func)(ShortcutArg);
    ShortcutArg arg;
} KeyboardShortcut;

typedef struct {
    xcb_connection_t* xcon;
    xcb_screen_t* screen;
    xcb_font_t cursorFont;
    xcb_cursor_t cursor;
    int screenNum, quit;
    xcb_keycode_t minKeycode, maxKeycode;
    xcb_keysym_t* keymap;
    Font font;
    Decoration testDecor;
} WMCtx;

void spawn(ShortcutArg arg);
void configureRequest(WMCtx* ctx, xcb_generic_event_t* genericEvent);
void mapRequest(WMCtx* ctx, xcb_generic_event_t* genericEvent);
void keyPress(WMCtx* ctx, xcb_generic_event_t* genericEvent);
void expose(WMCtx* ctx, xcb_generic_event_t* genericEvent);

static void (*eventHandlers[MAX_EVENT_CODE + 1])(WMCtx* ctx, xcb_generic_event_t*) = {
    [XCB_KEY_PRESS] = keyPress,
    [XCB_EXPOSE] = expose,
    [XCB_MAP_REQUEST] = mapRequest,
    [XCB_CONFIGURE_REQUEST] = configureRequest,
};

static char* menuArgs[] = {"dmenu_run", NULL};
#define INCLUDE_MOD_KEYS (XCB_MOD_MASK_1 | XCB_MOD_MASK_SHIFT | XCB_MOD_MASK_CONTROL)
#define MOD_KEY XCB_MOD_MASK_1

static KeyboardShortcut keys[] = {
    {MOD_KEY, XK_p, spawn, menuArgs}
};

void spawn(ShortcutArg arg)
{
    if(fork() == 0)
    {
        setsid();
        execvp(arg.v[0], arg.v);
        exit(0);
    }
}

void configureRequest(WMCtx* ctx, xcb_generic_event_t* genericEvent)
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

    xcb_configure_window(ctx->xcon, ctx->screen->root, mask, values);
    printf("configured window %d %d %d %d\n", event->x, event->y, event->width, event->height);
    xcb_flush(ctx->xcon);
}

void mapRequest(WMCtx* ctx, xcb_generic_event_t* genericEvent)
{
    xcb_map_request_event_t* event = (xcb_map_request_event_t*)genericEvent;
    xcb_map_window(ctx->xcon, event->window);
    xcb_flush(ctx->xcon);
}

void keyPress(WMCtx* ctx, xcb_generic_event_t* genericEvent)
{
    xcb_key_press_event_t* event;
    xcb_keysym_t eventKeysym;
    int i;

    event = (xcb_key_press_event_t*)genericEvent;
    if(event->detail < ctx->minKeycode || event->detail > ctx->maxKeycode)
        return;
    eventKeysym =  ctx->keymap[event->detail - ctx->minKeycode];

    printf("keypress %d %d\n", event->state, eventKeysym);

    event->state &= INCLUDE_MOD_KEYS;
    for(i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        printf("%x %x | %x %x\n", event->state, keys[i].mod, eventKeysym, keys[i].keysym);
        if(event->state == keys[i].mod && eventKeysym == keys[i].keysym) {
            printf("b\n");
            keys[i].func(keys[i].arg);
        }
    }
}

void expose(WMCtx* ctx, xcb_generic_event_t* genericEvent) {
    Decoration_expose(&ctx->testDecor, ctx->xcon, ctx->screen);
    xcb_flush(ctx->xcon);
}

static void initCtx(WMCtx* ctx)
{
    /* connect to x11 */
    {
        int conErr;
        /* use DISPLAY enviroment varable for screen number */
        ctx->xcon = xcb_connect(NULL, &ctx->screenNum);
        if((conErr = xcb_connection_has_error(ctx->xcon))) {
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

        setup = xcb_get_setup(ctx->xcon);

        screenIter = xcb_setup_roots_iterator(setup);
        for(i = 0; i < ctx->screenNum; i++)
            xcb_screen_next(&screenIter);
        ctx->screen = screenIter.data;

        ctx->minKeycode = setup->min_keycode;
        ctx->maxKeycode = setup->max_keycode;
    }

    /* keymap */
    {
        xcb_get_keyboard_mapping_cookie_t cookie;
        xcb_generic_error_t* error;
        xcb_get_keyboard_mapping_reply_t* reply;
        int keycodeCount, i;

        keycodeCount = ctx->maxKeycode - ctx->minKeycode + 1;
        cookie = xcb_get_keyboard_mapping(
            ctx->xcon,
            ctx->minKeycode,
            keycodeCount);
        reply = xcb_get_keyboard_mapping_reply(
            ctx->xcon,
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
        ctx->keymap = malloc(keycodeCount * sizeof(xcb_keysym_t));
        for(i = 0; i < keycodeCount; i++) {
            ctx->keymap[i] = replyKeymap[i * reply->keysyms_per_keycode];
        }

        free(reply);
    }

    /* cursor */
    {
        ctx->cursorFont = xcb_generate_id(ctx->xcon);
        xcb_open_font(ctx->xcon, ctx->cursorFont, strlen("cursor"), "cursor");

        ctx->cursor = xcb_generate_id(ctx->xcon);
        xcb_create_glyph_cursor(
            ctx->xcon,
            ctx->cursor,
            ctx->cursorFont,
            ctx->cursorFont, 
            58,
            58 + 1,
            0, 0, 0,
            0, 0, 0);
    }

    /* decoration */
    {
        Font_init(&ctx->font, ctx->xcon, "fixed");
        Decoration_init(&ctx->testDecor, ctx->xcon, ctx->screen, 400, 600, 800, 200);
        Decoration_drawRect(&ctx->testDecor, ctx->xcon, 0, 0, 800, 200, ctx->screen->white_pixel);
        Decoration_drawText(
            &ctx->testDecor,
            ctx->xcon,
            0, 0,
            "Hiy AAAAAAA",
            &ctx->font,
            ctx->screen->white_pixel,
            ctx->screen->black_pixel);
    }

    /* events */
    {
        xcb_void_cookie_t cookie;
        xcb_generic_error_t* err;
        uint32_t values[] = {
            XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
            | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
            | XCB_EVENT_MASK_STRUCTURE_NOTIFY
            | XCB_EVENT_MASK_KEY_PRESS
            | XCB_EVENT_MASK_EXPOSURE
        };

        cookie = xcb_change_window_attributes_checked(
            ctx->xcon,
            ctx->screen->root,
            XCB_CW_EVENT_MASK,
            values);
        if((err = xcb_request_check(ctx->xcon, cookie))) {
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
            xcb_disconnect(ctx->xcon);
            exit(1);
        }
    }

    xcb_flush(ctx->xcon);

    /* keysym to keycode
    for(int keycode = x->minKeycode; keycode <= x->maxKeycode; keycode++) {
        if(XConnection_keycodeToKeysym(x, keycode) == keysym) {
            return keycode;
        }
    }
    return 0;
    */
}

void destroyCtx(WMCtx* ctx)
{
    Decoration_destroy(&ctx->testDecor, ctx->xcon);
    Font_destroy(&ctx->font, ctx->xcon);
    xcb_free_cursor(ctx->xcon, ctx->cursor);
    xcb_close_font(ctx->xcon, ctx->cursorFont);
    xcb_disconnect(ctx->xcon);
    free(ctx->keymap);
}

void eventHandler(WMCtx* ctx, xcb_generic_event_t* event)
{
    unsigned char eventCode;
    void (*handlerFunc)(WMCtx*, xcb_generic_event_t*);

    eventCode = event->response_type & ~0x80;
    if(eventCode > MAX_EVENT_CODE)
    {
        fprintf(stderr, "received event code greater than MAX_EVENT_CODE");
        eventCode = 0;
    }
    handlerFunc = eventHandlers[eventCode];
    if(handlerFunc)
        handlerFunc(ctx, event);
}

int main()
{
    WMCtx ctx;
    xcb_generic_event_t* event;
    int conErr;

    initCtx(&ctx);

    xcb_ungrab_key(ctx.xcon, XCB_GRAB_ANY, ctx.screen->root, XCB_MOD_MASK_ANY);
    xcb_grab_key(
        ctx.xcon,
        1,
        ctx.screen->root,
        XCB_MOD_MASK_1,
        XCB_GRAB_ANY,
        XCB_GRAB_MODE_ASYNC,
        XCB_GRAB_MODE_ASYNC);

    while(ctx.quit == 0) {
        if((conErr = xcb_connection_has_error(ctx.xcon))) {
            fprintf(
                stderr,
                "x server connection unexpectedly closed. error code %d\n",
                conErr);
            exit(1);
        }
        event = xcb_wait_for_event(ctx.xcon);
        eventHandler(&ctx, event);
    }

    destroyCtx(&ctx);

    return 0;
}
