#ifndef TWO_FORTY_H
#define TWO_FORTY_H

#include <linux/input.h>
#include <stdbool.h>

#define TWO_FORTY_ABI_VERSION 4

enum two_forty_action {
    TWO_FORTY_ACTION_LEFT,
    TWO_FORTY_ACTION_RIGHT,
    TWO_FORTY_ACTION_UP,
    TWO_FORTY_ACTION_DOWN,
    TWO_FORTY_ACTION_JUMP,
    TWO_FORTY_ACTION_DASH,
    TWO_FORTY_ACTION_CONFIRM,
    TWO_FORTY_ACTION_MENU,
    TWO_FORTY_ACTION_COUNT
};

struct two_forty_input {
    bool keys[KEY_MAX + 1];
    bool pressed[KEY_MAX + 1];
    bool actions[TWO_FORTY_ACTION_COUNT];
    bool action_pressed[TWO_FORTY_ACTION_COUNT];
    bool controller_pressed;
};

struct two_forty_host_api {
    unsigned int abi_version;
    int screen_width;
    int screen_height;
    void *context;
    void (*fill_rect)(void *context, int x, int y, int width, int height,
                      unsigned char red, unsigned char green,
                      unsigned char blue);
    void (*play_sound)(void *context, const char *device, const char *path);
    void (*draw_text)(void *context, int x, int y, const char *text, int scale,
                      unsigned char red, unsigned char green,
                      unsigned char blue);
};

struct two_forty_game_api {
    unsigned int abi_version;
    bool (*init)(const struct two_forty_host_api *host, const char *config_path);
    void (*shutdown)(void);
    void (*update)(const struct two_forty_input *input);
    void (*render)(void);
};

typedef const struct two_forty_game_api *(*two_forty_game_entry_fn)(void);

#endif
