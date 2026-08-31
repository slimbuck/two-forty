#ifndef TWO_FORTY_H
#define TWO_FORTY_H

#include <linux/input.h>
#include <stdbool.h>

#define TWO_FORTY_ABI_VERSION 2

struct two_forty_input {
    bool keys[KEY_MAX + 1];
    bool pressed[KEY_MAX + 1];
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
