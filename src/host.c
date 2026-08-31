#include "two_forty.h"

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Minimal ABI declarations keep the Raspberry Pi OS Lite build offline. */
#define DRM_DISPLAY_MODE_LEN 32
#define DRM_MODE_CONNECTED 1
#define DRM_MODE_PAGE_FLIP_EVENT 0x01
#define DRM_EVENT_CONTEXT_VERSION 2
#define DRM_FORMAT_XRGB8888 0x34325258u

typedef struct drm_mode_modeinfo {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
    uint32_t vrefresh, flags, type;
    char name[DRM_DISPLAY_MODE_LEN];
} drmModeModeInfo;

typedef struct drm_mode_res {
    int count_fbs;
    uint32_t *fbs;
    int count_crtcs;
    uint32_t *crtcs;
    int count_connectors;
    uint32_t *connectors;
    int count_encoders;
    uint32_t *encoders;
    uint32_t min_width, max_width, min_height, max_height;
} drmModeRes;

typedef struct drm_mode_connector {
    uint32_t connector_id, encoder_id, connector_type, connector_type_id;
    int connection;
    uint32_t mmWidth, mmHeight;
    int subpixel, count_modes;
    drmModeModeInfo *modes;
    int count_props;
    uint32_t *props;
    uint64_t *prop_values;
    int count_encoders;
    uint32_t *encoders;
} drmModeConnector;

typedef struct drm_mode_encoder {
    uint32_t encoder_id, encoder_type, crtc_id, possible_crtcs, possible_clones;
} drmModeEncoder;

typedef struct drm_mode_crtc {
    uint32_t crtc_id, buffer_id, x, y, width, height;
    int mode_valid;
    drmModeModeInfo mode;
    int gamma_size;
} drmModeCrtc;

typedef struct drm_event_context {
    int version;
    void (*vblank_handler)(int, unsigned int, unsigned int, unsigned int, void *);
    void (*page_flip_handler)(int, unsigned int, unsigned int, unsigned int, void *);
} drmEventContext;

extern drmModeRes *drmModeGetResources(int fd);
extern void drmModeFreeResources(drmModeRes *ptr);
extern drmModeConnector *drmModeGetConnector(int fd, uint32_t connector_id);
extern void drmModeFreeConnector(drmModeConnector *ptr);
extern drmModeEncoder *drmModeGetEncoder(int fd, uint32_t encoder_id);
extern void drmModeFreeEncoder(drmModeEncoder *ptr);
extern drmModeCrtc *drmModeGetCrtc(int fd, uint32_t crtc_id);
extern void drmModeFreeCrtc(drmModeCrtc *ptr);
extern int drmModeAddFB2(int fd, uint32_t width, uint32_t height,
                         uint32_t pixel_format, const uint32_t bo_handles[4],
                         const uint32_t pitches[4], const uint32_t offsets[4],
                         uint32_t *buf_id, uint32_t flags);
extern int drmModeRmFB(int fd, uint32_t buffer_id);
extern int drmModeSetCrtc(int fd, uint32_t crtc_id, uint32_t buffer_id,
                          uint32_t x, uint32_t y, uint32_t *connectors,
                          int count, drmModeModeInfo *mode);
extern int drmModePageFlip(int fd, uint32_t crtc_id, uint32_t fb_id,
                           uint32_t flags, void *user_data);
extern int drmHandleEvent(int fd, drmEventContext *evctx);
extern int drmSetMaster(int fd);
extern int drmDropMaster(int fd);

#define GBM_BO_USE_SCANOUT (1u << 0)
#define GBM_BO_USE_RENDERING (1u << 2)
struct gbm_device;
struct gbm_surface;
struct gbm_bo;
union gbm_bo_handle { void *ptr; int32_t s32; uint32_t u32; uint64_t u64; };
extern struct gbm_device *gbm_create_device(int fd);
extern void gbm_device_destroy(struct gbm_device *gbm);
extern struct gbm_surface *gbm_surface_create(struct gbm_device *gbm,
                                               uint32_t width, uint32_t height,
                                               uint32_t format, uint32_t flags);
extern void gbm_surface_destroy(struct gbm_surface *surface);
extern struct gbm_bo *gbm_surface_lock_front_buffer(struct gbm_surface *surface);
extern void gbm_surface_release_buffer(struct gbm_surface *surface, struct gbm_bo *bo);
extern uint32_t gbm_bo_get_width(struct gbm_bo *bo);
extern uint32_t gbm_bo_get_height(struct gbm_bo *bo);
extern uint32_t gbm_bo_get_stride(struct gbm_bo *bo);
extern union gbm_bo_handle gbm_bo_get_handle(struct gbm_bo *bo);
extern void *gbm_bo_get_user_data(struct gbm_bo *bo);
extern void gbm_bo_set_user_data(struct gbm_bo *bo, void *data,
                                  void (*destroy_user_data)(struct gbm_bo *, void *));

typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLSurface;
typedef void *EGLNativeDisplayType;
typedef void *EGLNativeWindowType;
typedef int32_t EGLint;
typedef uint32_t EGLBoolean;
typedef uint32_t EGLenum;
#define EGL_NONE 0x3038
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_ALPHA_SIZE 0x3021
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 0x0004
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES2_BIT 0x0004
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_OPENGL_ES_API 0x30A0
#define EGL_PLATFORM_GBM_KHR 0x31D7
#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
extern EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id);
extern EGLDisplay eglGetPlatformDisplay(EGLenum platform, void *native_display,
                                         const EGLint *attrib_list);
extern EGLBoolean eglInitialize(EGLDisplay display, EGLint *major, EGLint *minor);
extern EGLBoolean eglTerminate(EGLDisplay display);
extern EGLBoolean eglBindAPI(EGLenum api);
extern EGLBoolean eglChooseConfig(EGLDisplay display, const EGLint *attrib_list,
                                   EGLConfig *configs, EGLint config_size,
                                   EGLint *num_config);
extern EGLContext eglCreateContext(EGLDisplay display, EGLConfig config,
                                    EGLContext share_context, const EGLint *attrib_list);
extern EGLBoolean eglDestroyContext(EGLDisplay display, EGLContext context);
extern EGLSurface eglCreateWindowSurface(EGLDisplay display, EGLConfig config,
                                          EGLNativeWindowType win,
                                          const EGLint *attrib_list);
extern EGLBoolean eglDestroySurface(EGLDisplay display, EGLSurface surface);
extern EGLBoolean eglMakeCurrent(EGLDisplay display, EGLSurface draw,
                                  EGLSurface read, EGLContext context);
extern EGLBoolean eglSwapInterval(EGLDisplay display, EGLint interval);
extern EGLBoolean eglSwapBuffers(EGLDisplay display, EGLSurface surface);
extern EGLint eglGetError(void);

typedef int32_t GLsizei;
typedef int32_t GLint;
typedef uint32_t GLenum;
typedef uint32_t GLbitfield;
typedef float GLfloat;
typedef unsigned char GLubyte;
#define GL_COLOR_BUFFER_BIT 0x00004000u
#define GL_SCISSOR_TEST 0x0C11u
#define GL_RGB 0x1907u
#define GL_UNSIGNED_BYTE 0x1401u
extern void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
extern void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
extern void glClear(GLbitfield mask);
extern void glEnable(GLenum cap);
extern void glDisable(GLenum cap);
extern void glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
extern void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                         GLenum format, GLenum type, void *pixels);

#define MAX_INPUTS 32
#define MAX_GAMES 32
#define HOST_CONFIG_PATH "config/host.conf"

enum binding_kind { BINDING_NONE, BINDING_KEY, BINDING_ABS };

struct controller_binding {
    enum binding_kind kind;
    unsigned int code;
    int direction;
};

struct input_device {
    int fd;
    bool controller;
    char name[128];
    bool keys[KEY_MAX + 1];
    int abs_values[ABS_MAX + 1];
    int abs_minimums[ABS_MAX + 1];
    int abs_maximums[ABS_MAX + 1];
    int abs_flats[ABS_MAX + 1];
};

struct framebuffer { int drm_fd; uint32_t fb_id; };

struct input_set {
    struct input_device devices[MAX_INPUTS];
    int count;
    bool previous_actions[TWO_FORTY_ACTION_COUNT];
    bool controller_up_pressed;
    bool controller_down_pressed;
    struct two_forty_input state;
};

struct game_record {
    char id[64];
    char name[128];
    char description[256];
    char module_path[512];
    char config_path[512];
};

struct host {
    int drm_fd;
    uint32_t connector_id, crtc_id;
    drmModeModeInfo mode;
    drmModeCrtc *saved_crtc;
    struct gbm_device *gbm;
    struct gbm_surface *gbm_surface;
    struct gbm_bo *front_bo;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    struct input_set inputs;
    struct game_record games[MAX_GAMES];
    int game_count;
    int selected_game;
    void *game_library;
    const struct two_forty_game_api *game_api;
    const struct game_record *active_game;
    struct two_forty_host_api api;
    int control_fd;
    bool flip_pending;
    bool running;
    pid_t sound_pid;
    unsigned int snapshot_sequence;
    unsigned long frame_number;
    char boot_game_id[64];
    struct controller_binding bindings[TWO_FORTY_ACTION_COUNT];
    bool controller_settings;
    int selected_action;
    bool binding_capture;
    bool capture_wait_release;
    bool captured_binding_ready;
    struct controller_binding captured_binding;
    unsigned int controller_menu_chord_frames;
};

static volatile sig_atomic_t stop_requested;
static volatile sig_atomic_t snapshot_requested;

static void on_stop(int signal_number) { (void)signal_number; stop_requested = 1; }
static void on_snapshot(int signal_number) { (void)signal_number; snapshot_requested = 1; }

static char *trim(char *text)
{
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') ++text;
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\r' || end[-1] == '\n')) --end;
    *end = '\0';
    return text;
}

static void copy_text(char *destination, size_t capacity, const char *source)
{
    if (capacity > 0) snprintf(destination, capacity, "%s", source);
}

static const char *const action_config_keys[TWO_FORTY_ACTION_COUNT] = {
    "bind_left", "bind_right", "bind_up", "bind_down",
    "bind_jump", "bind_dash", "bind_confirm", "bind_menu"
};

static const char *const action_names[TWO_FORTY_ACTION_COUNT] = {
    "LEFT", "RIGHT", "UP", "DOWN", "JUMP", "DASH", "CONFIRM", "MENU"
};

static void default_bindings(struct controller_binding *bindings)
{
    bindings[TWO_FORTY_ACTION_LEFT] =
        (struct controller_binding){BINDING_ABS, ABS_HAT0X, -1};
    bindings[TWO_FORTY_ACTION_RIGHT] =
        (struct controller_binding){BINDING_ABS, ABS_HAT0X, 1};
    bindings[TWO_FORTY_ACTION_UP] =
        (struct controller_binding){BINDING_ABS, ABS_HAT0Y, -1};
    bindings[TWO_FORTY_ACTION_DOWN] =
        (struct controller_binding){BINDING_ABS, ABS_HAT0Y, 1};
    bindings[TWO_FORTY_ACTION_JUMP] =
        (struct controller_binding){BINDING_KEY, BTN_SOUTH, 0};
    bindings[TWO_FORTY_ACTION_DASH] =
        (struct controller_binding){BINDING_KEY, BTN_WEST, 0};
    bindings[TWO_FORTY_ACTION_CONFIRM] =
        (struct controller_binding){BINDING_KEY, BTN_START, 0};
    bindings[TWO_FORTY_ACTION_MENU] =
        (struct controller_binding){BINDING_KEY, BTN_SELECT, 0};
}

static bool parse_binding(const char *text, struct controller_binding *binding)
{
    unsigned int code = 0;
    int direction = 0;
    char trailing = '\0';
    if (sscanf(text, "key:%u%c", &code, &trailing) == 1 && code <= KEY_MAX) {
        *binding = (struct controller_binding){BINDING_KEY, code, 0};
        return true;
    }
    if (sscanf(text, "abs:%u:%d%c", &code, &direction, &trailing) == 2 &&
        code <= ABS_MAX && (direction == -1 || direction == 1)) {
        *binding = (struct controller_binding){BINDING_ABS, code, direction};
        return true;
    }
    return false;
}

static int binding_action_for_key(const char *key)
{
    for (int action = 0; action < TWO_FORTY_ACTION_COUNT; ++action)
        if (strcmp(key, action_config_keys[action]) == 0) return action;
    return -1;
}

static void write_binding(FILE *file, int action,
                          const struct controller_binding *binding)
{
    if (binding->kind == BINDING_KEY)
        fprintf(file, "%s=key:%u\n", action_config_keys[action], binding->code);
    else if (binding->kind == BINDING_ABS)
        fprintf(file, "%s=abs:%u:%d\n", action_config_keys[action],
                binding->code, binding->direction);
}

static bool save_bindings(const struct host *host)
{
    FILE *source = fopen(HOST_CONFIG_PATH, "r");
    FILE *target = fopen(HOST_CONFIG_PATH ".tmp", "w");
    if (target == NULL) {
        fprintf(stderr, "Cannot save controller bindings: %s\n", strerror(errno));
        if (source != NULL) fclose(source);
        return false;
    }
    bool written[TWO_FORTY_ACTION_COUNT] = {false};
    bool saw_binding = false;
    char line[512];
    while (source != NULL && fgets(line, sizeof(line), source) != NULL) {
        char parsed[sizeof(line)];
        copy_text(parsed, sizeof(parsed), line);
        char *entry = trim(parsed);
        char *separator = strchr(entry, '=');
        int action = -1;
        if (separator != NULL) {
            *separator = '\0';
            action = binding_action_for_key(trim(entry));
        }
        if (action >= 0) {
            if (!written[action]) {
                write_binding(target, action, &host->bindings[action]);
                written[action] = true;
            }
            saw_binding = true;
        } else {
            fputs(line, target);
        }
    }
    if (source != NULL) fclose(source);
    if (!saw_binding) fputs("\n# Controller bindings use Linux evdev key and axis codes.\n", target);
    for (int action = 0; action < TWO_FORTY_ACTION_COUNT; ++action)
        if (!written[action]) write_binding(target, action, &host->bindings[action]);
    bool failed = fflush(target) != 0;
    if (!failed && fsync(fileno(target)) != 0) failed = true;
    if (fclose(target) != 0) failed = true;
    if (!failed && rename(HOST_CONFIG_PATH ".tmp", HOST_CONFIG_PATH) != 0)
        failed = true;
    if (failed) {
        fprintf(stderr, "Cannot finish saving controller bindings: %s\n",
                strerror(errno));
        remove(HOST_CONFIG_PATH ".tmp");
        return false;
    }
    puts("Saved controller bindings to " HOST_CONFIG_PATH ".");
    return true;
}

static bool has_gp2040_controller(const struct host *host)
{
    for (int index = 0; index < host->inputs.count; ++index) {
        const struct input_device *device = &host->inputs.devices[index];
        if (device->controller && strcasestr(device->name, "gp2040") != NULL)
            return true;
    }
    return false;
}

static void binding_name(const struct host *host,
                         const struct controller_binding *binding,
                         char *name, size_t capacity)
{
    bool gp2040 = has_gp2040_controller(host);
    if (gp2040 && binding->kind == BINDING_KEY && binding->code == BTN_SOUTH)
        copy_text(name, capacity, "Y");
    else if (gp2040 && binding->kind == BINDING_KEY &&
             binding->code == BTN_EAST)
        copy_text(name, capacity, "B");
    else if (gp2040 && binding->kind == BINDING_KEY &&
             binding->code == BTN_TL2)
        copy_text(name, capacity, "SELECT");
    else if (gp2040 && binding->kind == BINDING_KEY &&
             binding->code == BTN_TR2)
        copy_text(name, capacity, "START");
    else if (binding->kind == BINDING_ABS && binding->code == ABS_HAT0X)
        copy_text(name, capacity, binding->direction < 0 ? "DPAD LEFT" : "DPAD RIGHT");
    else if (binding->kind == BINDING_ABS && binding->code == ABS_HAT0Y)
        copy_text(name, capacity, binding->direction < 0 ? "DPAD UP" : "DPAD DOWN");
    else if (binding->kind == BINDING_KEY && binding->code == BTN_SOUTH)
        copy_text(name, capacity, "B");
    else if (binding->kind == BINDING_KEY && binding->code == BTN_WEST)
        copy_text(name, capacity, "Y");
    else if (binding->kind == BINDING_KEY && binding->code == BTN_START)
        copy_text(name, capacity, "START");
    else if (binding->kind == BINDING_KEY && binding->code == BTN_SELECT)
        copy_text(name, capacity, "SELECT");
    else if (binding->kind == BINDING_KEY)
        snprintf(name, capacity, "BUTTON %u", binding->code);
    else if (binding->kind == BINDING_ABS)
        snprintf(name, capacity, "AXIS %u %s", binding->code,
                 binding->direction < 0 ? "NEG" : "POS");
    else
        copy_text(name, capacity, "UNBOUND");
}

static void action_label(void *context, enum two_forty_action action,
                         char *text, size_t capacity)
{
    struct host *host = context;
    if (action < 0 || action >= TWO_FORTY_ACTION_COUNT) {
        copy_text(text, capacity, "UNBOUND");
        return;
    }
    binding_name(host, &host->bindings[action], text, capacity);
}

static void fill_rect(void *context, int x, int y, int width, int height,
                      unsigned char red, unsigned char green, unsigned char blue)
{
    struct host *host = context;
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > host->mode.hdisplay) width = host->mode.hdisplay - x;
    if (y + height > host->mode.vdisplay) height = host->mode.vdisplay - y;
    if (width <= 0 || height <= 0) return;
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);
    glClearColor(red / 255.0f, green / 255.0f, blue / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

static void play_sound(void *context, const char *device, const char *path)
{
    struct host *host = context;
    if (host->sound_pid > 0) {
        if (waitpid(host->sound_pid, NULL, WNOHANG) == 0) return;
        host->sound_pid = 0;
    }
    pid_t child = fork();
    if (child == 0) {
        execlp("aplay", "aplay", "-q", "-D", device, path, (char *)NULL);
        _exit(127);
    }
    if (child > 0) host->sound_pid = child;
}

static void power_down_pi(void)
{
    pid_t child = fork();
    if (child == 0) {
        execlp("sudo", "sudo", "-n", "systemctl", "poweroff", (char *)NULL);
        _exit(127);
    }
}

static void write_status(const struct host *host)
{
    mkdir("run", 0755);
    FILE *file = fopen("run/status.json.tmp", "w");
    if (file == NULL) return;
    fprintf(file, "{\n  \"pid\": %ld,\n  \"mode\": \"%s\",\n  \"game\": \"%s\",\n"
                  "  \"width\": %u,\n  \"height\": %u,\n  \"refresh\": %u\n}\n",
            (long)getpid(), host->active_game ? "game" : "launcher",
            host->active_game ? host->active_game->id : "",
            host->mode.hdisplay, host->mode.vdisplay, host->mode.vrefresh);
    fclose(file);
    rename("run/status.json.tmp", "run/status.json");
}

static bool load_manifest(const char *directory, struct game_record *game)
{
    snprintf(game->config_path, sizeof(game->config_path), "games/%s/game.conf", directory);
    FILE *file = fopen(game->config_path, "r");
    if (file == NULL) return false;
    char line[1024];
    while (fgets(line, sizeof(line), file) != NULL) {
        char *entry = trim(line);
        if (*entry == '\0' || *entry == '#' || *entry == ';') continue;
        char *separator = strchr(entry, '=');
        if (separator == NULL) continue;
        *separator = '\0';
        char *key = trim(entry);
        char *value = trim(separator + 1);
        if (strcmp(key, "id") == 0) copy_text(game->id, sizeof(game->id), value);
        else if (strcmp(key, "name") == 0) copy_text(game->name, sizeof(game->name), value);
        else if (strcmp(key, "description") == 0)
            copy_text(game->description, sizeof(game->description), value);
        else if (strcmp(key, "module") == 0)
            copy_text(game->module_path, sizeof(game->module_path), value);
    }
    fclose(file);
    return game->id[0] && game->name[0] && game->module_path[0];
}

static void discover_games(struct host *host)
{
    DIR *games = opendir("games");
    if (games == NULL) return;
    struct dirent *entry;
    while (host->game_count < MAX_GAMES && (entry = readdir(games)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        struct game_record candidate = {0};
        if (load_manifest(entry->d_name, &candidate))
            host->games[host->game_count++] = candidate;
    }
    closedir(games);
    printf("Discovered %d game(s).\n", host->game_count);
}

static bool load_game(struct host *host, int index);

static void load_host_config(struct host *host)
{
    default_bindings(host->bindings);
    FILE *file = fopen(HOST_CONFIG_PATH, "r");
    if (file == NULL) return;
    char line[512];
    while (fgets(line, sizeof(line), file) != NULL) {
        char *entry = trim(line);
        if (!*entry || *entry == '#' || *entry == ';') continue;
        char *separator = strchr(entry, '=');
        if (separator == NULL) continue;
        *separator = '\0';
        char *key = trim(entry);
        char *value = trim(separator + 1);
        if (strcmp(key, "boot_game") == 0) {
            copy_text(host->boot_game_id, sizeof(host->boot_game_id), value);
        } else {
            int action = binding_action_for_key(key);
            struct controller_binding parsed;
            if (action >= 0 && parse_binding(value, &parsed))
                host->bindings[action] = parsed;
            else if (action >= 0)
                fprintf(stderr, "Ignoring invalid controller binding %s=%s\n",
                        key, value);
        }
    }
    fclose(file);
}

static bool load_boot_game(struct host *host)
{
    if (!host->boot_game_id[0] || strcmp(host->boot_game_id, "launcher") == 0)
        return false;
    for (int index = 0; index < host->game_count; ++index) {
        if (strcmp(host->games[index].id, host->boot_game_id) == 0) {
            host->selected_game = index;
            return load_game(host, index);
        }
    }
    fprintf(stderr, "Configured boot game was not found: %s\n", host->boot_game_id);
    return false;
}

static void unload_game(struct host *host)
{
    if (host->game_api != NULL) host->game_api->shutdown();
    host->game_api = NULL;
    host->active_game = NULL;
    if (host->game_library != NULL) dlclose(host->game_library);
    host->game_library = NULL;
    write_status(host);
}

static bool load_game(struct host *host, int index)
{
    if (index < 0 || index >= host->game_count) return false;
    unload_game(host);
    struct game_record *game = &host->games[index];
    host->game_library = dlopen(game->module_path, RTLD_NOW | RTLD_LOCAL);
    if (host->game_library == NULL) {
        fprintf(stderr, "Cannot load %s: %s\n", game->module_path, dlerror());
        return false;
    }
    dlerror();
    void *symbol = dlsym(host->game_library, "two_forty_game_entry");
    two_forty_game_entry_fn entry = NULL;
    memcpy(&entry, &symbol, sizeof(entry));
    const char *error = dlerror();
    if (error != NULL || entry == NULL) {
        fprintf(stderr, "Invalid game module %s: %s\n", game->module_path,
                error != NULL ? error : "entry point missing");
        unload_game(host);
        return false;
    }
    host->game_api = entry();
    if (host->game_api == NULL ||
        host->game_api->abi_version != TWO_FORTY_ABI_VERSION ||
        !host->game_api->init(&host->api, game->config_path)) {
        fprintf(stderr, "Game initialization failed: %s\n", game->id);
        unload_game(host);
        return false;
    }
    host->active_game = game;
    printf("Started game: %s\n", game->name);
    write_status(host);
    return true;
}

static void process_control(struct host *host)
{
    char command[256];
    ssize_t length = read(host->control_fd, command, sizeof(command) - 1);
    if (length <= 0) return;
    command[length] = '\0';
    char *line = trim(command);
    char *newline = strchr(line, '\n');
    if (newline != NULL) *newline = '\0';

    if (strcmp(line, "menu") == 0) {
        unload_game(host);
    } else if (strcmp(line, "poweroff") == 0) {
        power_down_pi();
    } else if (strcmp(line, "snapshot") == 0) {
        snapshot_requested = 1;
    } else if (strcmp(line, "quit") == 0) {
        host->running = false;
    } else if (strcmp(line, "reload") == 0 && host->active_game != NULL) {
        int active = (int)(host->active_game - host->games);
        load_game(host, active);
    } else if (strncmp(line, "launch ", 7) == 0) {
        const char *id = trim(line + 7);
        for (int index = 0; index < host->game_count; ++index) {
            if (strcmp(host->games[index].id, id) == 0) {
                host->selected_game = index;
                load_game(host, index);
                break;
            }
        }
    }
}

static const uint8_t *glyph(char character)
{
    static const uint8_t alphabet[26][7] = {
        {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
        {14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
        {31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
        {14,4,4,4,4,4,14},{7,2,2,2,18,18,12},
        {17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},
        {14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
        {14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
        {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},
        {17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
    };
    static const uint8_t digits[10][7] = {
        {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},
        {14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
        {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14},{14,17,17,15,1,1,14}
    };
    static const uint8_t dash[7] = {0,0,0,31,0,0,0};
    static const uint8_t question[7] = {14,17,1,2,4,0,4};
    static const uint8_t blank[7] = {0,0,0,0,0,0,0};
    if (character >= 'a' && character <= 'z') character -= 32;
    if (character >= 'A' && character <= 'Z') return alphabet[character - 'A'];
    if (character >= '0' && character <= '9') return digits[character - '0'];
    if (character == '-') return dash;
    if (character == ' ') return blank;
    return question;
}

static void draw_text(void *context, int x, int y, const char *text,
                      int scale, unsigned char red, unsigned char green,
                      unsigned char blue)
{
    struct host *host = context;
    int cursor = x;
    for (const char *character = text; *character; ++character) {
        const uint8_t *rows = glyph(*character);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((rows[row] & (1u << (4 - column))) != 0)
                    fill_rect(host, cursor + column * scale, y - row * scale,
                              scale, scale, red, green, blue);
            }
        }
        cursor += 6 * scale;
    }
}

static void draw_launcher(struct host *host)
{
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.015f, 0.022f, 0.035f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    static const unsigned char bars[7][3] = {
        {210,210,210},{210,210,20},{20,210,210},{20,190,40},
        {210,30,190},{220,35,30},{35,55,220}
    };
    int drift = (int)((host->frame_number / 180) % 5) - 2;
    for (int index = 0; index < 7; ++index)
        fill_rect(host, 18 + drift + index * 40, 208, 40, 8,
                  bars[index][0], bars[index][1], bars[index][2]);
    draw_text(host, 20 + drift, 188, "TWO FORTY", 3, 238, 240, 232);
    draw_text(host, 20 + drift, 160, "CHOOSE A GAME", 1, 112, 160, 170);

    if (host->game_count == 0)
        draw_text(host, 24 + drift, 134, "NO GAMES FOUND", 1, 230, 80, 70);
    for (int index = 0; index < host->game_count && index < 4; ++index) {
        int item_y = 132 - index * 26;
        bool selected = index == host->selected_game;
        fill_rect(host, 18 + drift, item_y - 15, 284, 21,
                  selected ? 28 : 14, selected ? 74 : 30, selected ? 84 : 40);
        fill_rect(host, 24 + drift, item_y - 11, selected ? 5 : 2, 12,
                  selected ? 244 : 70, selected ? 194 : 110, selected ? 70 : 120);
        draw_text(host, 38 + drift, item_y, host->games[index].name, 2,
                  selected ? 250 : 170, selected ? 248 : 185,
                  selected ? 236 : 190);
    }

    int settings_y = 132 - host->game_count * 26;
    bool settings_selected = host->selected_game == host->game_count;
    fill_rect(host, 18 + drift, settings_y - 15, 284, 21,
              settings_selected ? 42 : 20, settings_selected ? 65 : 35,
              settings_selected ? 92 : 50);
    fill_rect(host, 24 + drift, settings_y - 11, settings_selected ? 5 : 2, 12,
              80, 170, 220);
    draw_text(host, 38 + drift, settings_y, "CONTROLLER SETTINGS", 1,
              settings_selected ? 250 : 165, settings_selected ? 248 : 185,
              settings_selected ? 236 : 195);

    int power_y = settings_y - 26;
    bool power_selected = host->selected_game == host->game_count + 1;
    fill_rect(host, 18 + drift, power_y - 15, 284, 21,
              power_selected ? 82 : 34, power_selected ? 31 : 23,
              power_selected ? 29 : 25);
    fill_rect(host, 24 + drift, power_y - 11, power_selected ? 5 : 2, 12,
              236, 84, 68);
    draw_text(host, 38 + drift, power_y, "POWER OFF", 1,
              power_selected ? 255 : 190, power_selected ? 220 : 125,
              power_selected ? 210 : 120);

    char footer[64], confirm[32];
    if (settings_selected) {
        copy_text(footer, sizeof(footer), "ANY BUTTON SETTINGS");
    } else {
        binding_name(host, &host->bindings[TWO_FORTY_ACTION_CONFIRM],
                     confirm, sizeof(confirm));
        snprintf(footer, sizeof(footer), "DPAD MOVE  %s CHOOSE", confirm);
    }
    draw_text(host, 20 + drift, 12, footer, 1, 105, 125, 130);
    glDisable(GL_SCISSOR_TEST);
}

static void draw_controller_settings(struct host *host)
{
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.012f, 0.019f, 0.032f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    fill_rect(host, 12, 222, 296, 5, 40, 175, 212);
    draw_text(host, 18, 207, "CONTROLLER SETTINGS", 2, 238, 240, 232);
    draw_text(host, 18, 184, host->binding_capture ?
              "RELEASE THEN PRESS A BUTTON" : "CHOOSE AN ACTION",
              1, 110, 160, 174);

    for (int action = 0; action < TWO_FORTY_ACTION_COUNT; ++action) {
        int item_y = 163 - action * 19;
        bool selected = action == host->selected_action;
        fill_rect(host, 16, item_y - 13, 288, 17,
                  selected ? 25 : 12, selected ? 68 : 29, selected ? 78 : 39);
        fill_rect(host, 21, item_y - 10, selected ? 4 : 2, 10,
                  selected ? 244 : 65, selected ? 194 : 115, selected ? 70 : 125);
        draw_text(host, 31, item_y, action_names[action], 1,
                  selected ? 250 : 165, selected ? 248 : 185,
                  selected ? 236 : 190);
        char physical[64];
        if (selected && host->binding_capture)
            copy_text(physical, sizeof(physical), host->capture_wait_release ?
                      "RELEASE" : "PRESS NOW");
        else
            binding_name(host, &host->bindings[action], physical, sizeof(physical));
        draw_text(host, 112, item_y, physical, 1,
                  selected ? 120 : 92, selected ? 210 : 145,
                  selected ? 235 : 165);
    }
    draw_text(host, 18, 9, "ANY BUTTON BIND  SELECT BACK", 1,
              95, 118, 126);
    glDisable(GL_SCISSOR_TEST);
}

static void save_snapshot(struct host *host)
{
    int width = host->mode.hdisplay;
    int height = host->mode.vdisplay;
    size_t row_bytes = (size_t)width * 3u;
    GLubyte *pixels = malloc(row_bytes * (size_t)height);
    if (pixels == NULL) return;
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    if (mkdir("snapshots", 0755) != 0 && errno != EEXIST) { free(pixels); return; }
    time_t now = time(NULL);
    struct tm timestamp;
    localtime_r(&now, &timestamp);
    char path[512];
    snprintf(path, sizeof(path), "snapshots/two-forty-%04d%02d%02d-%02d%02d%02d-%03u.ppm",
             timestamp.tm_year + 1900, timestamp.tm_mon + 1, timestamp.tm_mday,
             timestamp.tm_hour, timestamp.tm_min, timestamp.tm_sec,
             host->snapshot_sequence++);
    FILE *file = fopen(path, "wb");
    if (file != NULL) {
        fprintf(file, "P6\n%d %d\n255\n", width, height);
        for (int row = height - 1; row >= 0; --row)
            fwrite(pixels + (size_t)row * row_bytes, 1, row_bytes, file);
        fclose(file);
        printf("Snapshot: %s\n", path);
    }
    free(pixels);
}

static void destroy_framebuffer(struct gbm_bo *bo, void *data)
{
    (void)bo;
    struct framebuffer *framebuffer = data;
    if (framebuffer != NULL) {
        if (framebuffer->fb_id) drmModeRmFB(framebuffer->drm_fd, framebuffer->fb_id);
        free(framebuffer);
    }
}

static uint32_t framebuffer_for_bo(struct host *host, struct gbm_bo *bo)
{
    struct framebuffer *framebuffer = gbm_bo_get_user_data(bo);
    if (framebuffer != NULL) return framebuffer->fb_id;
    framebuffer = calloc(1, sizeof(*framebuffer));
    if (framebuffer == NULL) return 0;
    framebuffer->drm_fd = host->drm_fd;
    uint32_t handles[4] = {gbm_bo_get_handle(bo).u32, 0, 0, 0};
    uint32_t pitches[4] = {gbm_bo_get_stride(bo), 0, 0, 0};
    uint32_t offsets[4] = {0, 0, 0, 0};
    if (drmModeAddFB2(host->drm_fd, gbm_bo_get_width(bo), gbm_bo_get_height(bo),
                      DRM_FORMAT_XRGB8888, handles, pitches, offsets,
                      &framebuffer->fb_id, 0) != 0) {
        free(framebuffer);
        return 0;
    }
    gbm_bo_set_user_data(bo, framebuffer, destroy_framebuffer);
    return framebuffer->fb_id;
}

static bool choose_display(struct host *host)
{
    drmModeRes *resources = drmModeGetResources(host->drm_fd);
    if (resources == NULL) return false;
    drmModeConnector *connector = NULL;
    for (int index = 0; index < resources->count_connectors; ++index) {
        drmModeConnector *candidate = drmModeGetConnector(host->drm_fd,
                                                           resources->connectors[index]);
        if (candidate != NULL && candidate->connection == DRM_MODE_CONNECTED &&
            candidate->count_modes > 0) { connector = candidate; break; }
        drmModeFreeConnector(candidate);
    }
    if (connector == NULL) { drmModeFreeResources(resources); return false; }
    drmModeEncoder *encoder = connector->encoder_id ?
        drmModeGetEncoder(host->drm_fd, connector->encoder_id) : NULL;
    if (encoder == NULL) {
        for (int index = 0; index < connector->count_encoders; ++index) {
            encoder = drmModeGetEncoder(host->drm_fd, connector->encoders[index]);
            if (encoder != NULL) break;
        }
    }
    uint32_t crtc_id = encoder != NULL ? encoder->crtc_id : 0;
    if (!crtc_id && encoder != NULL) {
        for (int index = 0; index < resources->count_crtcs; ++index) {
            if (encoder->possible_crtcs & (1u << index)) {
                crtc_id = resources->crtcs[index]; break;
            }
        }
    }
    if (!crtc_id) {
        drmModeFreeEncoder(encoder); drmModeFreeConnector(connector);
        drmModeFreeResources(resources); return false;
    }
    host->connector_id = connector->connector_id;
    host->crtc_id = crtc_id;
    host->mode = connector->modes[0];
    host->saved_crtc = drmModeGetCrtc(host->drm_fd, crtc_id);
    printf("DRM mode: %s %ux%u @ %u Hz\n", host->mode.name,
           host->mode.hdisplay, host->mode.vdisplay, host->mode.vrefresh);
    drmModeFreeEncoder(encoder); drmModeFreeConnector(connector);
    drmModeFreeResources(resources);
    return true;
}

static bool init_graphics(struct host *host)
{
    host->gbm = gbm_create_device(host->drm_fd);
    if (host->gbm == NULL) return false;
    host->gbm_surface = gbm_surface_create(host->gbm, host->mode.hdisplay,
        host->mode.vdisplay, DRM_FORMAT_XRGB8888,
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (host->gbm_surface == NULL) return false;
    host->egl_display = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, host->gbm, NULL);
    if (host->egl_display == EGL_NO_DISPLAY)
        host->egl_display = eglGetDisplay((EGLNativeDisplayType)host->gbm);
    EGLint major = 0, minor = 0;
    if (host->egl_display == EGL_NO_DISPLAY ||
        !eglInitialize(host->egl_display, &major, &minor) ||
        !eglBindAPI(EGL_OPENGL_ES_API)) return false;
    const EGLint attributes[] = {EGL_SURFACE_TYPE,EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,
        EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,0,EGL_NONE};
    EGLConfig config = NULL;
    EGLint count = 0;
    if (!eglChooseConfig(host->egl_display, attributes, &config, 1, &count) || count != 1)
        return false;
    const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    host->egl_context = eglCreateContext(host->egl_display, config, EGL_NO_CONTEXT,
                                          context_attributes);
    host->egl_surface = eglCreateWindowSurface(host->egl_display, config,
        (EGLNativeWindowType)host->gbm_surface, NULL);
    if (host->egl_context == EGL_NO_CONTEXT || host->egl_surface == EGL_NO_SURFACE ||
        !eglMakeCurrent(host->egl_display, host->egl_surface, host->egl_surface,
                        host->egl_context)) return false;
    eglSwapInterval(host->egl_display, 0);
    glViewport(0, 0, host->mode.hdisplay, host->mode.vdisplay);
    printf("EGL version: %d.%d\n", major, minor);
    return true;
}

static bool input_bit(const unsigned char *bits, unsigned int code)
{
    return (bits[code / 8u] & (1u << (code % 8u))) != 0;
}

static bool controller_button_code(unsigned int code)
{
    return (code >= BTN_MISC && code <= BTN_9) ||
           (code >= BTN_JOYSTICK && code < BTN_DIGI) ||
           (code >= BTN_DPAD_UP && code <= BTN_DPAD_RIGHT) ||
           (code >= BTN_TRIGGER_HAPPY1 && code <= BTN_TRIGGER_HAPPY40);
}

static bool controller_direction_code(unsigned int code)
{
    return code == KEY_UP || code == KEY_DOWN || code == KEY_LEFT ||
           code == KEY_RIGHT ||
           (code >= BTN_DPAD_UP && code <= BTN_DPAD_RIGHT);
}

static int axis_direction(const struct input_device *device, unsigned int code,
                          int value)
{
    int minimum = device->abs_minimums[code];
    int maximum = device->abs_maximums[code];
    int centre = minimum + (maximum - minimum) / 2;
    int threshold = (maximum - minimum) / 4;
    if (device->abs_flats[code] > threshold) threshold = device->abs_flats[code];
    if (value < centre - threshold) return -1;
    if (value > centre + threshold) return 1;
    return 0;
}

static void open_inputs(struct input_set *inputs)
{
    memset(inputs, 0, sizeof(*inputs));
    for (int index = 0; index < MAX_INPUTS; ++index) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/event%d", index);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        char name[128] = "unknown";
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);

        unsigned char key_bits[(KEY_MAX + 8) / 8] = {0};
        unsigned char abs_bits[(ABS_MAX + 8) / 8] = {0};
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits);
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits);

        struct input_device *device = &inputs->devices[inputs->count];
        device->fd = fd;
        copy_text(device->name, sizeof(device->name), name);
        for (unsigned int code = 0; code <= KEY_MAX; ++code) {
            if (input_bit(key_bits, code) && controller_button_code(code))
                device->controller = true;
        }
        for (unsigned int code = 0; code <= ABS_MAX; ++code) {
            if (!input_bit(abs_bits, code)) continue;
            struct input_absinfo info;
            if (ioctl(fd, EVIOCGABS(code), &info) == 0) {
                device->abs_values[code] = info.value;
                device->abs_minimums[code] = info.minimum;
                device->abs_maximums[code] = info.maximum;
                device->abs_flats[code] = info.flat;
            }
            if (code >= ABS_HAT0X && code <= ABS_HAT3Y)
                device->controller = true;
        }
        if (strcasestr(name, "gamepad") != NULL ||
            strcasestr(name, "controller") != NULL ||
            strcasestr(name, "joystick") != NULL ||
            strcasestr(name, "stick") != NULL ||
            strcasestr(name, "gp2040") != NULL)
            device->controller = true;
        ++inputs->count;
        printf("Input: %s (%s)%s\n", path, name,
               device->controller ? " [controller]" : "");
    }
}

static bool controller_buttons_released(const struct input_set *inputs)
{
    for (int index = 0; index < inputs->count; ++index) {
        const struct input_device *device = &inputs->devices[index];
        if (!device->controller) continue;
        for (unsigned int code = 0; code <= KEY_MAX; ++code)
            if (device->keys[code]) return false;
    }
    return true;
}

static int controller_buttons_down(const struct input_set *inputs)
{
    int count = 0;
    for (int index = 0; index < inputs->count; ++index) {
        const struct input_device *device = &inputs->devices[index];
        if (!device->controller) continue;
        for (unsigned int code = 0; code <= KEY_MAX; ++code)
            if (device->keys[code] && !controller_direction_code(code)) ++count;
    }
    return count;
}

static bool binding_down(const struct input_set *inputs,
                         const struct controller_binding *binding)
{
    for (int index = 0; index < inputs->count; ++index) {
        const struct input_device *device = &inputs->devices[index];
        if (!device->controller) continue;
        if (binding->kind == BINDING_KEY && binding->code <= KEY_MAX &&
            device->keys[binding->code]) return true;
        if (binding->kind == BINDING_ABS && binding->code <= ABS_MAX &&
            axis_direction(device, binding->code,
                           device->abs_values[binding->code]) == binding->direction)
            return true;
    }
    return false;
}

static void update_controller_actions(struct host *host)
{
    for (int action = 0; action < TWO_FORTY_ACTION_COUNT; ++action) {
        bool down = binding_down(&host->inputs, &host->bindings[action]);
        host->inputs.state.actions[action] = down;
        host->inputs.state.action_pressed[action] =
            down && !host->inputs.previous_actions[action];
        host->inputs.previous_actions[action] = down;
    }
}

static void process_input(struct host *host, int fd)
{
    struct input_device *device = NULL;
    for (int index = 0; index < host->inputs.count; ++index)
        if (host->inputs.devices[index].fd == fd) device = &host->inputs.devices[index];
    if (device == NULL) return;

    struct input_event events[32];
    ssize_t bytes;
    while ((bytes = read(fd, events, sizeof(events))) > 0) {
        size_t count = (size_t)bytes / sizeof(events[0]);
        for (size_t index = 0; index < count; ++index) {
            struct input_event *event = &events[index];
            if (event->type == EV_KEY && event->code <= KEY_MAX) {
                bool down = event->value != 0;
                if (event->value == 1) host->inputs.state.pressed[event->code] = true;
                host->inputs.state.keys[event->code] = down;
                device->keys[event->code] = down;
                if (device->controller && event->value == 1) {
                    if (event->code == KEY_UP || event->code == BTN_DPAD_UP)
                        host->inputs.controller_up_pressed = true;
                    else if (event->code == KEY_DOWN ||
                             event->code == BTN_DPAD_DOWN)
                        host->inputs.controller_down_pressed = true;
                    if (!controller_direction_code(event->code))
                        host->inputs.state.controller_pressed = true;
                }
                if (device->controller && event->value == 1 &&
                    host->binding_capture && !host->capture_wait_release) {
                    host->captured_binding = (struct controller_binding){
                        BINDING_KEY, event->code, 0};
                    host->captured_binding_ready = true;
                }
            } else if (event->type == EV_ABS && event->code <= ABS_MAX) {
                int old_direction = axis_direction(device, event->code,
                                                   device->abs_values[event->code]);
                device->abs_values[event->code] = event->value;
                int new_direction = axis_direction(device, event->code, event->value);
                if (device->controller && old_direction == 0 &&
                    new_direction != 0 &&
                    (event->code == ABS_Y || event->code == ABS_HAT0Y)) {
                    if (new_direction < 0)
                        host->inputs.controller_up_pressed = true;
                    else
                        host->inputs.controller_down_pressed = true;
                }
                if (device->controller && old_direction == 0 && new_direction != 0 &&
                    host->binding_capture && !host->capture_wait_release) {
                    host->captured_binding = (struct controller_binding){
                        BINDING_ABS, event->code, new_direction};
                    host->captured_binding_ready = true;
                }
            }
        }
    }
}

static void update_host(struct host *host)
{
    struct two_forty_input *input = &host->inputs.state;
    update_controller_actions(host);
    if (input->pressed[KEY_F12]) snapshot_requested = 1;
    if (host->active_game != NULL) {
        if (controller_buttons_down(&host->inputs) >= 2) {
            if (host->controller_menu_chord_frames < 60)
                ++host->controller_menu_chord_frames;
        } else {
            host->controller_menu_chord_frames = 0;
        }
        if (input->pressed[KEY_ESC] || input->pressed[KEY_F1] ||
            input->action_pressed[TWO_FORTY_ACTION_MENU] ||
            host->controller_menu_chord_frames >= 60) {
            unload_game(host);
            host->controller_settings = false;
            host->controller_menu_chord_frames = 0;
        } else {
            host->game_api->update(input);
        }
    } else if (host->controller_settings) {
        if (host->binding_capture) {
            if (input->pressed[KEY_ESC]) {
                host->binding_capture = false;
                host->capture_wait_release = false;
                host->captured_binding_ready = false;
            } else if (host->captured_binding_ready) {
                host->bindings[host->selected_action] = host->captured_binding;
                host->binding_capture = false;
                host->captured_binding_ready = false;
                host->capture_wait_release = true;
                save_bindings(host);
            } else if (host->capture_wait_release &&
                       controller_buttons_released(&host->inputs)) {
                host->capture_wait_release = false;
            }
        } else if (host->capture_wait_release) {
            if (controller_buttons_released(&host->inputs))
                host->capture_wait_release = false;
        } else {
            bool moved = false;
            if (input->pressed[KEY_UP] || input->pressed[KEY_W] ||
                input->action_pressed[TWO_FORTY_ACTION_UP] ||
                host->inputs.controller_up_pressed) {
                host->selected_action = (host->selected_action +
                    TWO_FORTY_ACTION_COUNT - 1) % TWO_FORTY_ACTION_COUNT;
                moved = true;
            }
            if (input->pressed[KEY_DOWN] || input->pressed[KEY_S] ||
                input->action_pressed[TWO_FORTY_ACTION_DOWN] ||
                host->inputs.controller_down_pressed) {
                host->selected_action = (host->selected_action + 1) %
                    TWO_FORTY_ACTION_COUNT;
                moved = true;
            }
            if (!moved && (input->pressed[KEY_ESC] ||
                           input->action_pressed[TWO_FORTY_ACTION_MENU])) {
                host->controller_settings = false;
            } else if (!moved && (input->pressed[KEY_ENTER] ||
                                  input->action_pressed[TWO_FORTY_ACTION_CONFIRM] ||
                                  input->controller_pressed)) {
                host->binding_capture = true;
                host->capture_wait_release = true;
                host->captured_binding_ready = false;
            }
        }
    } else {
        int item_count = host->game_count + 2;
        bool moved = false;
        if (input->pressed[KEY_UP] || input->pressed[KEY_W] ||
            input->action_pressed[TWO_FORTY_ACTION_UP] ||
            host->inputs.controller_up_pressed) {
            host->selected_game = (host->selected_game + item_count - 1) % item_count;
            moved = true;
        }
        if (input->pressed[KEY_DOWN] || input->pressed[KEY_S] ||
            input->action_pressed[TWO_FORTY_ACTION_DOWN] ||
            host->inputs.controller_down_pressed) {
            host->selected_game = (host->selected_game + 1) % item_count;
            moved = true;
        }
        bool confirmed = input->pressed[KEY_ENTER] ||
            input->action_pressed[TWO_FORTY_ACTION_CONFIRM];
        if (!moved && confirmed) {
            if (host->selected_game < host->game_count)
                load_game(host, host->selected_game);
            else if (host->selected_game == host->game_count)
                host->controller_settings = true;
            else
                power_down_pi();
        } else if (!moved && input->controller_pressed &&
                   host->selected_game == host->game_count)
            host->controller_settings = true;
        if (input->pressed[KEY_Q] || input->pressed[KEY_ESC]) host->running = false;
    }
    memset(input->pressed, 0, sizeof(input->pressed));
    input->controller_pressed = false;
    host->inputs.controller_up_pressed = false;
    host->inputs.controller_down_pressed = false;
}

static void draw_host(struct host *host)
{
    if (host->active_game != NULL) host->game_api->render();
    else if (host->controller_settings) draw_controller_settings(host);
    else draw_launcher(host);
    glDisable(GL_SCISSOR_TEST);
    if (snapshot_requested) { snapshot_requested = 0; save_snapshot(host); }
}

static void flip_handler(int fd, unsigned int sequence, unsigned int tv_sec,
                         unsigned int tv_usec, void *user_data)
{
    (void)fd; (void)sequence; (void)tv_sec; (void)tv_usec;
    ((struct host *)user_data)->flip_pending = false;
}

static void wait_for_events(struct host *host)
{
    struct pollfd fds[MAX_INPUTS + 2];
    fds[0].fd = host->drm_fd; fds[0].events = POLLIN;
    fds[1].fd = host->control_fd; fds[1].events = POLLIN;
    for (int index = 0; index < host->inputs.count; ++index) {
        fds[index + 2].fd = host->inputs.devices[index].fd; fds[index + 2].events = POLLIN;
    }
    drmEventContext context = {.version=DRM_EVENT_CONTEXT_VERSION,
        .vblank_handler=NULL,.page_flip_handler=flip_handler};
    while (host->flip_pending && host->running && !stop_requested) {
        int result = poll(fds, (nfds_t)(host->inputs.count + 2), -1);
        if (result < 0) { if (errno == EINTR) continue; host->running = false; break; }
        if (fds[0].revents & POLLIN) drmHandleEvent(host->drm_fd, &context);
        if (fds[1].revents & POLLIN) process_control(host);
        for (int index = 0; index < host->inputs.count; ++index)
            if (fds[index + 2].revents & POLLIN) process_input(host, fds[index + 2].fd);
    }
}

static bool first_frame(struct host *host)
{
    draw_host(host);
    if (!eglSwapBuffers(host->egl_display, host->egl_surface)) return false;
    host->front_bo = gbm_surface_lock_front_buffer(host->gbm_surface);
    if (host->front_bo == NULL) return false;
    uint32_t fb_id = framebuffer_for_bo(host, host->front_bo);
    return fb_id && drmModeSetCrtc(host->drm_fd, host->crtc_id, fb_id, 0, 0,
        &host->connector_id, 1, &host->mode) == 0;
}

static bool next_frame(struct host *host)
{
    update_host(host);
    draw_host(host);
    host->frame_number++;
    if (!eglSwapBuffers(host->egl_display, host->egl_surface)) return false;
    struct gbm_bo *next = gbm_surface_lock_front_buffer(host->gbm_surface);
    if (next == NULL) return false;
    uint32_t fb_id = framebuffer_for_bo(host, next);
    if (!fb_id) { gbm_surface_release_buffer(host->gbm_surface, next); return false; }
    host->flip_pending = true;
    if (drmModePageFlip(host->drm_fd, host->crtc_id, fb_id,
                        DRM_MODE_PAGE_FLIP_EVENT, host) != 0) {
        host->flip_pending = false;
        gbm_surface_release_buffer(host->gbm_surface, next);
        return false;
    }
    wait_for_events(host);
    if (!host->flip_pending) {
        gbm_surface_release_buffer(host->gbm_surface, host->front_bo);
        host->front_bo = next;
    } else gbm_surface_release_buffer(host->gbm_surface, next);
    return true;
}

static void cleanup(struct host *host)
{
    unload_game(host);
    for (int index = 0; index < host->inputs.count; ++index) close(host->inputs.devices[index].fd);
    if (host->saved_crtc != NULL && host->drm_fd >= 0)
        drmModeSetCrtc(host->drm_fd, host->saved_crtc->crtc_id,
            host->saved_crtc->buffer_id, host->saved_crtc->x, host->saved_crtc->y,
            &host->connector_id, 1,
            host->saved_crtc->mode_valid ? &host->saved_crtc->mode : NULL);
    if (host->front_bo != NULL && host->gbm_surface != NULL)
        gbm_surface_release_buffer(host->gbm_surface, host->front_bo);
    if (host->egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(host->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (host->egl_surface != EGL_NO_SURFACE)
            eglDestroySurface(host->egl_display, host->egl_surface);
        if (host->egl_context != EGL_NO_CONTEXT)
            eglDestroyContext(host->egl_display, host->egl_context);
        eglTerminate(host->egl_display);
    }
    if (host->gbm_surface != NULL) gbm_surface_destroy(host->gbm_surface);
    if (host->gbm != NULL) gbm_device_destroy(host->gbm);
    drmModeFreeCrtc(host->saved_crtc);
    if (host->drm_fd >= 0) { drmDropMaster(host->drm_fd); close(host->drm_fd); }
    if (host->control_fd >= 0) close(host->control_fd);
    unlink("run/control.fifo");
    remove("run/status.json");
}

int main(void)
{
    struct host host;
    memset(&host, 0, sizeof(host));
    host.drm_fd = -1;
    host.control_fd = -1;
    host.egl_display = EGL_NO_DISPLAY;
    host.egl_context = EGL_NO_CONTEXT;
    host.egl_surface = EGL_NO_SURFACE;
    host.running = true;
    signal(SIGINT, on_stop);
    signal(SIGTERM, on_stop);
    signal(SIGUSR1, on_snapshot);

    discover_games(&host);
    load_host_config(&host);
    host.drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (host.drm_fd < 0 || (drmSetMaster(host.drm_fd) != 0 && errno != EINVAL) ||
        !choose_display(&host) || !init_graphics(&host)) {
        fprintf(stderr, "Two Forty host initialization failed: %s\n", strerror(errno));
        cleanup(&host);
        return EXIT_FAILURE;
    }
    host.api = (struct two_forty_host_api){.abi_version=TWO_FORTY_ABI_VERSION,
        .screen_width=host.mode.hdisplay,.screen_height=host.mode.vdisplay,
        .context=&host,.fill_rect=fill_rect,.play_sound=play_sound,
        .draw_text=draw_text,.action_label=action_label};
    open_inputs(&host.inputs);
    mkdir("run", 0755);
    if (mkfifo("run/control.fifo", 0600) != 0 && errno != EEXIST) {
        fprintf(stderr, "Cannot create control channel: %s\n", strerror(errno));
        cleanup(&host);
        return EXIT_FAILURE;
    }
    host.control_fd = open("run/control.fifo", O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (host.control_fd < 0) {
        fprintf(stderr, "Cannot open control channel: %s\n", strerror(errno));
        cleanup(&host);
        return EXIT_FAILURE;
    }
    load_boot_game(&host);
    write_status(&host);
    if (!first_frame(&host)) {
        cleanup(&host);
        return EXIT_FAILURE;
    }
    puts("Two Forty host running. Enter starts, F1/Esc returns, F12 snapshots.");
    while (host.running && !stop_requested) {
        if (!next_frame(&host)) host.running = false;
    }
    cleanup(&host);
    puts("Console framebuffer restored.");
    return EXIT_SUCCESS;
}
