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

void configureRequest(XConnection* x, xcb_generic_event_t* genericEvent);
void mapRequest(XConnection* x, xcb_generic_event_t* genericEvent);
void keyPress(XConnection* x, xcb_generic_event_t* genericEvent);
void expose(XConnection* x, xcb_generic_event_t* genericEvent);

static void (*eventHandlers[MAX_EVENT_CODE + 1])(XConnection*, xcb_generic_event_t*) = {
    [XCB_KEY_PRESS] = keyPress,
    [XCB_EXPOSE] = expose,
    [XCB_MAP_REQUEST] = mapRequest,
    [XCB_CONFIGURE_REQUEST] = configureRequest,
};

static lua_State* luaState;
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

int l_spawn(lua_State* luaState)
{
    int tableLen, i;
    char** spawnArgs;
    size_t strLen;
    char* str;

    if(!lua_istable(luaState, 1)) {
        luaL_error(luaState, "spawn takes a table as an argument");
        return 0;
    }
    
    tableLen = 0;
    lua_pushnil(luaState); /* push first key */
    /* pop key; push next key and value */
    while(lua_next(luaState, 1)) {
        tableLen++;
        lua_pop(luaState, 1); /* pop value */
    }

    spawnArgs = malloc((tableLen + 1) * sizeof(char*));
    i = 0;
    lua_pushnil(luaState); /* push first key */
    /* pop key; push next key and value */
    while(lua_next(luaState, 1)) {
        str = (char*)lua_tolstring(luaState, -1, &strLen);
        spawnArgs[i++] = str;
        lua_pop(luaState, 1); /* pop value */
    }
    spawnArgs[i] = NULL;
    spawn(spawnArgs);

    return 0;
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
    xcb_keysym_t eventKeysym;
    int keysModFlags, keysKeySym;

    event = (xcb_key_press_event_t*)genericEvent;
    eventKeysym = XConnection_keycodeToKeysym(x, event->detail);

    lua_getglobal(luaState, "keys"); /* push keys table */
    if(lua_istable(luaState, -1) == 0)
        return;

    lua_pushnil(luaState); /* push first key */
    /* pop key; push next key and value */
    while(lua_next(luaState, -2)) {
        lua_pushinteger(luaState, 1); /* push table index */
        lua_gettable(luaState, -2); /* pop table index; push mod flags */
        keysModFlags = lua_tointeger(luaState, -1);
        lua_pop(luaState, 1); /* pop mod flags */

        lua_pushinteger(luaState, 2); /* push table index */
        lua_gettable(luaState, -2); /* pop table index; push keysym */
        keysKeySym = lua_tointeger(luaState, -1);
        lua_pop(luaState, 1); /* pop keysym */

        if(event->state == keysModFlags && eventKeysym == keysKeySym) {
            lua_pushinteger(luaState, 3); /* push table index */
            lua_gettable(luaState, -2); /* pop table index; push lua function */
            if(lua_isfunction(luaState, -1))
                lua_call(luaState, 0, 0); /* pop and call lua function */
            else
                lua_pop(luaState, 1); /* pop invalid function */
        }

        lua_pop(luaState, 1); /* pop value */
    }

    lua_pop(luaState, 1); /* pop keys table */
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
    int luaError;

    luaState = luaL_newstate();
    luaL_openlibs(luaState);
    lua_register(luaState, "spawn", l_spawn);
    luaError = luaL_dofile(luaState, "mwmrc.lua");
    if(luaError != LUA_OK) {
        fprintf(stderr, "[LUA ERROR]: %s\n", lua_tostring(luaState, -1));
        lua_pop(luaState, 1);
    }

    XConnection_init(&x);

    xcb_ungrab_key(x.con, XCB_GRAB_ANY, x.screen->root, XCB_MOD_MASK_ANY);
    xcb_grab_key(
        x.con,
        1,
        x.screen->root,
        XCB_MOD_MASK_1,
        XCB_GRAB_ANY,
        XCB_GRAB_MODE_ASYNC,
        XCB_GRAB_MODE_ASYNC);

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

    lua_close(luaState);

    return 0;
}
