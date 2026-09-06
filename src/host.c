#include "two_forty.h"
#include "input_bindings.h"

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
    bool pending_actions[TWO_FORTY_ACTION_COUNT];
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
    struct controller_binding keyboard_bindings[TWO_FORTY_ACTION_COUNT];
    struct binding_setup setup;
    bool controller_settings, display_settings, ui_wait_release, last_keyboard;
    int selected_option, display_option, safe_x, safe_y, saved_safe_x, saved_safe_y;
    const char *settings_message;
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

static const char *const keyboard_config_keys[TWO_FORTY_ACTION_COUNT] = {
    "key_left", "key_right", "key_up", "key_down",
    "key_jump", "key_dash", "key_confirm", "key_menu"
};

static int binding_action_for_key(const char *key)
{
    for (int i=0;i<TWO_FORTY_ACTION_COUNT;i++) {
        if (!strcmp(key,action_config_keys[i])) return i;
        if (!strcmp(key,keyboard_config_keys[i])) return i+TWO_FORTY_ACTION_COUNT;
    }
    return -1;
}

static void write_binding(FILE *file, const char *key, const struct controller_binding *binding)
{
    if (binding->kind==BINDING_KEY) fprintf(file,"%s=key:%u\n",key,binding->code);
    else if (binding->kind==BINDING_ABS) fprintf(file,"%s=abs:%u:%d\n",key,binding->code,binding->direction);
}

static bool save_bindings(const struct host *host)
{
    FILE *source=fopen(HOST_CONFIG_PATH,"r"), *target=fopen(HOST_CONFIG_PATH ".tmp","w");
    if (!target) { if (source) fclose(source); return false; }
    char line[512];
    while (source && fgets(line,sizeof(line),source)) {
        char parsed[512]; copy_text(parsed,sizeof(parsed),line);
        char *key=trim(parsed), *separator=strchr(key,'=');
        if (separator) *separator=0;
        key=trim(key);
        if (binding_action_for_key(key)<0 && strcmp(key,"safe_x") && strcmp(key,"safe_y") && strcmp(key,"input_version"))
            fputs(line,target);
    }
    bool failed=source && ferror(source);
    if (source) fclose(source);
    fputs("\ninput_version=2\n",target);
    fprintf(target,"safe_x=%d\nsafe_y=%d\n",host->safe_x,host->safe_y);
    for (int i=0;i<TWO_FORTY_ACTION_COUNT;i++) {
        write_binding(target,action_config_keys[i],&host->bindings[i]);
        write_binding(target,keyboard_config_keys[i],&host->keyboard_bindings[i]);
    }
    if (fflush(target)!=0 || fsync(fileno(target))!=0) failed=true;
    if (fclose(target)!=0) failed=true;
    if (!failed && rename(HOST_CONFIG_PATH ".tmp",HOST_CONFIG_PATH)!=0) failed=true;
    if (failed) remove(HOST_CONFIG_PATH ".tmp");
    return !failed;
}

static void update_safe_area(struct host *host)
{
    host->api.screen_width=host->mode.hdisplay-host->safe_x*2;
    host->api.screen_height=host->mode.vdisplay-host->safe_y*2;
}

static void keyboard_name(const struct controller_binding *binding, char *name, size_t capacity)
{
    const char *label=NULL;
    switch (binding->code) {
        case KEY_UP: label="UP"; break; case KEY_DOWN: label="DOWN"; break;
        case KEY_LEFT: label="LEFT"; break; case KEY_RIGHT: label="RIGHT"; break;
        case KEY_ENTER: label="ENTER"; break; case KEY_ESC: label="ESC"; break;
        case KEY_SPACE: label="SPACE"; break; case KEY_TAB: label="TAB"; break;
        case KEY_LEFTSHIFT: label="LEFT SHIFT"; break; case KEY_RIGHTSHIFT: label="RIGHT SHIFT"; break;
        case KEY_LEFTCTRL: label="LEFT CTRL"; break; case KEY_RIGHTCTRL: label="RIGHT CTRL"; break;
    }
    static const unsigned int letters[]={KEY_A,KEY_B,KEY_C,KEY_D,KEY_E,KEY_F,KEY_G,KEY_H,KEY_I,KEY_J,KEY_K,KEY_L,KEY_M,KEY_N,KEY_O,KEY_P,KEY_Q,KEY_R,KEY_S,KEY_T,KEY_U,KEY_V,KEY_W,KEY_X,KEY_Y,KEY_Z};
    if (label) { copy_text(name,capacity,label); return; }
    for (int i=0;i<26;i++) if (binding->code==letters[i]) { snprintf(name,capacity,"%c",'A'+i); return; }
    snprintf(name,capacity,"KEY %u",binding->code);
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
    bool controller=false;
    for (int i=0;i<host->inputs.count;i++) controller |= host->inputs.devices[i].controller;
    if (host->last_keyboard || !controller) keyboard_name(&host->keyboard_bindings[action],text,capacity);
    else binding_name(host,&host->bindings[action],text,capacity);
}

static void fill_rect(void *context, int x, int y, int width, int height,
                      unsigned char red, unsigned char green, unsigned char blue)
{
    struct host *host = context;
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > host->api.screen_width) width = host->api.screen_width - x;
    if (y + height > host->api.screen_height) height = host->api.screen_height - y;
    if (width <= 0 || height <= 0) return;
    glEnable(GL_SCISSOR_TEST);
    glScissor(x+host->safe_x, y+host->safe_y, width, height);
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
                  "  \"width\": %u,\n  \"height\": %u,\n  \"refresh\": %u,\n"
                  "  \"viewport_width\": %d,\n  \"viewport_height\": %d\n}\n",
            (long)getpid(), host->active_game ? "game" : "launcher",
            host->active_game ? host->active_game->id : "",
            host->mode.hdisplay, host->mode.vdisplay, host->mode.vrefresh,
            host->api.screen_width, host->api.screen_height);
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
    default_keyboard_bindings(host->keyboard_bindings);
    host->safe_x=16; host->safe_y=12;
    int input_version=0;
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
        } else if (!strcmp(key,"safe_x")) {
            int n=atoi(value); if (n>=0 && n<=32) host->safe_x=n;
        } else if (!strcmp(key,"safe_y")) {
            int n=atoi(value); if (n>=0 && n<=24) host->safe_y=n;
        } else if (!strcmp(key,"input_version")) input_version=atoi(value);
        else {
            int action=binding_action_for_key(key);
            struct controller_binding parsed;
            if (action>=0 && parse_binding(value,&parsed)) {
                if (action<TWO_FORTY_ACTION_COUNT) host->bindings[action]=parsed;
                else if (parsed.kind==BINDING_KEY && parsed.code<BTN_MISC && parsed.code!=KEY_F1 && parsed.code!=KEY_F12)
                    host->keyboard_bindings[action-TWO_FORTY_ACTION_COUNT]=parsed;
            }
        }
    }
    fclose(file);
    if (input_version<2 && host->bindings[TWO_FORTY_ACTION_CONFIRM].kind==BINDING_KEY &&
        host->bindings[TWO_FORTY_ACTION_CONFIRM].code==BTN_TR2)
        host->bindings[TWO_FORTY_ACTION_CONFIRM].code=BTN_EAST;

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
    if (host->display_settings) {
        host->safe_x=host->saved_safe_x; host->safe_y=host->saved_safe_y;
        update_safe_area(host);
    }
    host->controller_settings=host->display_settings=host->setup.active=false;
    host->ui_wait_release=true;
    host->controller_menu_chord_frames=0;
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
        if (host->display_settings) {
            host->safe_x=host->saved_safe_x; host->safe_y=host->saved_safe_y;
            update_safe_area(host);
        }
        host->controller_settings=host->display_settings=host->setup.active=false;
        host->ui_wait_release=true;
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
    static const uint8_t slash[7] = {1,2,2,4,8,8,16};
    static const uint8_t question[7] = {14,17,1,2,4,0,4};
    static const uint8_t blank[7] = {0,0,0,0,0,0,0};
    if (character >= 'a' && character <= 'z') character -= 32;
    if (character >= 'A' && character <= 'Z') return alphabet[character - 'A'];
    if (character >= '0' && character <= '9') return digits[character - '0'];
    if (character == '-') return dash;
    if (character == '/') return slash;
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

static void clear_screen(void)
{
    glDisable(GL_SCISSOR_TEST); glClearColor(.012f,.019f,.032f,1); glClear(GL_COLOR_BUFFER_BIT);
}

static void menu_text(struct host *host, int x, int y, const char *value, int scale,
                      unsigned char r, unsigned char g, unsigned char b)
{
    char clipped[128];
    int columns=(host->api.screen_width-x-8)/(6*scale);
    if (columns<0) columns=0;
    if (columns>127) columns=127;
    snprintf(clipped,sizeof(clipped),"%.*s",columns,value);
    draw_text(host,x,y,clipped,scale,r,g,b);
}

static void menu_row(struct host *host, int y, const char *label, bool selected)
{
    fill_rect(host,8,y-13,host->api.screen_width-16,20,selected?28:14,selected?74:30,selected?84:40);
    fill_rect(host,12,y-9,3,10,selected?244:70,selected?194:110,70);
    menu_text(host,22,y,label,1,selected?250:170,selected?248:185,selected?236:190);
}

static void menu_footer(struct host *host, bool can_go_back)
{
    char confirm[32],back[32],line[80]; action_label(host,TWO_FORTY_ACTION_CONFIRM,confirm,sizeof(confirm));
    action_label(host,TWO_FORTY_ACTION_MENU,back,sizeof(back));
    if (can_go_back) snprintf(line,sizeof(line),"%s CHOOSE - %s BACK",confirm,back);
    else snprintf(line,sizeof(line),"%s CHOOSE - UP DOWN MOVE",confirm);
    menu_text(host,10,14,line,1,112,160,170);
}

static void draw_launcher(struct host *host)
{
    clear_screen();
    int height=host->api.screen_height;
    fill_rect(host,8,height-7,host->api.screen_width-16,3,40,175,212);
    menu_text(host,10,height-23,"TWO FORTY",3,238,240,232);
    menu_text(host,10,height-55,"GAMES AND SETTINGS",1,112,160,170);
    int count=host->game_count+3, first=host->selected_game<4?0:host->selected_game-3;
    for (int row=0;row<4 && first+row<count;row++) {
        int index=first+row;
        const char *label=index<host->game_count?host->games[index].name:
            index==host->game_count?"INPUT SETTINGS":index==host->game_count+1?"DISPLAY AREA":"POWER OFF";
        menu_row(host,height-83-row*24,label,index==host->selected_game);
    }
    if (first+4<count) menu_text(host,10,30,"MORE BELOW",1,112,160,170);
    else if (first>0) menu_text(host,10,30,"MORE ABOVE",1,112,160,170);
    menu_footer(host,false);
}

static void draw_controller_settings(struct host *host)
{
    clear_screen();
    int height=host->api.screen_height;
    if (host->setup.active) {
        struct binding_setup *setup=&host->setup;
        menu_text(host,10,height-20,setup->keyboard?"CONFIGURE KEYBOARD":"CONFIGURE BUTTONS",2,238,240,232);
        char line[80];
        snprintf(line,sizeof(line),"%s",action_names[setup->step<TWO_FORTY_ACTION_COUNT?setup->step:TWO_FORTY_ACTION_COUNT-1]);
        menu_text(host,10,height-52,line,3,244,194,70);
        snprintf(line,sizeof(line),"STEP %d OF %d - %s",setup->step+1,TWO_FORTY_ACTION_COUNT,
            setup->wait_release?"RELEASE":"PRESS NOW");
        menu_text(host,10,height-79,line,1,112,180,190);
        int first=setup->step>2?setup->step-2:0;
        for (int i=first;i<setup->step;i++) {
            char name[32];
            if (setup->keyboard) keyboard_name(&setup->pending[i],name,sizeof(name));
            else binding_name(host,&setup->pending[i],name,sizeof(name));
            snprintf(line,sizeof(line),"%s - %s",action_names[i],name);
            menu_text(host,10,height-108-(i-first)*14,line,1,150,180,170);
        }
        menu_text(host,10,49,setup->message,1,244,160,70);
        menu_text(host,10,31,"SAVES AFTER ALL EIGHT",1,112,160,170);
        menu_text(host,10,15,setup->keyboard?"F1 CANCEL":"F1 OR HOLD 2 BUTTONS TO CANCEL",1,112,160,170);
    } else {
        menu_text(host,10,height-22,"INPUT SETTINGS",2,238,240,232);
        menu_text(host,10,height-51,"FOLLOW EACH PROMPT IN ORDER",1,112,160,170);
        const char *labels[]={"CONFIGURE BUTTONS","CONFIGURE KEYBOARD","BACK"};
        for (int i=0;i<3;i++) menu_row(host,height-81-i*25,labels[i],host->selected_option==i);
        menu_text(host,10,38,host->settings_message?host->settings_message:"",1,244,194,70);
        menu_footer(host,true);
    }
}

static void draw_display_settings(struct host *host)
{
    clear_screen();
    int width=host->api.screen_width,height=host->api.screen_height;
    fill_rect(host,0,0,width,1,40,175,212); fill_rect(host,0,height-1,width,1,40,175,212);
    fill_rect(host,0,0,1,height,40,175,212); fill_rect(host,width-1,0,1,height,40,175,212);
    menu_text(host,10,height-20,"DISPLAY AREA",2,238,240,232);
    menu_text(host,10,height-47,"KEEP ALL FOUR EDGES VISIBLE",1,112,160,170);
    char line[64];
    snprintf(line,sizeof(line),"SIDE MARGIN - %d",host->safe_x); menu_row(host,height-74,line,host->display_option==0);
    snprintf(line,sizeof(line),"TOP BOTTOM MARGIN - %d",host->safe_y); menu_row(host,height-98,line,host->display_option==1);
    menu_row(host,height-122,"SAVE",host->display_option==2);
    menu_row(host,height-146,"CANCEL",host->display_option==3);
    menu_text(host,10,25,host->settings_message && *host->settings_message?host->settings_message:"LEFT RIGHT ADJUST - UP DOWN MOVE",1,112,160,170);
    menu_footer(host,true);
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

static bool capture_axis(unsigned int code)
{
    return code==ABS_X || code==ABS_Y || code==ABS_RX || code==ABS_RY ||
        (code>=ABS_HAT0X && code<=ABS_HAT3Y);
}

static bool buttons_released(const struct input_set *inputs, bool keyboard)
{
    for (int i=0;i<inputs->count;i++) {
        const struct input_device *device=&inputs->devices[i];
        if (device->controller==keyboard) continue;
        for (unsigned int code=0;code<=KEY_MAX;code++) if (device->keys[code]) return false;
        if (!keyboard) for (unsigned int code=0;code<=ABS_MAX;code++)
            if (capture_axis(code) && axis_direction(device,code,device->abs_values[code])) return false;
    }
    return true;
}

static int controller_buttons_down(const struct input_set *inputs)
{
    int count=0;
    for (int i=0;i<inputs->count;i++) {
        const struct input_device *device=&inputs->devices[i];
        if (!device->controller) continue;
        for (unsigned int code=0;code<=KEY_MAX;code++)
            if (device->keys[code] && !controller_direction_code(code)) count++;
    }
    return count;
}

static bool binding_down(const struct input_set *inputs, const struct controller_binding *binding, bool keyboard)
{
    for (int i=0;i<inputs->count;i++) {
        const struct input_device *device=&inputs->devices[i];
        if (device->controller==keyboard) continue;
        if (binding->kind==BINDING_KEY && binding->code<=KEY_MAX && device->keys[binding->code]) return true;
        if (!keyboard && binding->kind==BINDING_ABS && binding->code<=ABS_MAX &&
            axis_direction(device,binding->code,device->abs_values[binding->code])==binding->direction) return true;
    }
    return false;
}

static bool action_down(const struct host *host, int action)
{
    return binding_down(&host->inputs,&host->bindings[action],false) ||
        binding_down(&host->inputs,&host->keyboard_bindings[action],true);
}

static void update_controller_actions(struct host *host)
{
    for (int i=0;i<TWO_FORTY_ACTION_COUNT;i++) {
        bool down=action_down(host,i);
        host->inputs.state.actions[i]=down;
        host->inputs.state.action_pressed[i]=host->inputs.pending_actions[i] || (down && !host->inputs.previous_actions[i]);
        host->inputs.previous_actions[i]=down;
        host->inputs.pending_actions[i]=false;
    }
}

static void process_input_event(struct host *host, struct input_device *device, const struct input_event *event)
{
    bool before[TWO_FORTY_ACTION_COUNT];
    for (int i=0;i<TWO_FORTY_ACTION_COUNT;i++) before[i]=action_down(host,i);
    if (event->type==EV_KEY && event->code<=KEY_MAX) {
        bool pressed=event->value==1 && !device->keys[event->code];
        device->keys[event->code]=event->value!=0;
        if (!device->controller) {
            if (pressed) host->inputs.state.pressed[event->code]=true;
            bool down=false;
            for (int i=0;i<host->inputs.count;i++) if (!host->inputs.devices[i].controller)
                down |= host->inputs.devices[i].keys[event->code];
            host->inputs.state.keys[event->code]=down;
        }
        if (pressed) {
            host->last_keyboard=!device->controller;
            if (device->controller) {
                if (event->code==KEY_UP || event->code==BTN_DPAD_UP) host->inputs.controller_up_pressed=true;
                if (event->code==KEY_DOWN || event->code==BTN_DPAD_DOWN) host->inputs.controller_down_pressed=true;
                if (!controller_direction_code(event->code)) host->inputs.state.controller_pressed=true;
            }
            if (host->setup.keyboard!=device->controller)
                setup_offer(&host->setup,(struct controller_binding){BINDING_KEY,event->code,0});
        }
    } else if (event->type==EV_ABS && event->code<=ABS_MAX) {
        int old=axis_direction(device,event->code,device->abs_values[event->code]);
        device->abs_values[event->code]=event->value;
        int direction=axis_direction(device,event->code,event->value);
        if (device->controller && old==0 && direction!=0) {
            host->last_keyboard=false;
            if (event->code==ABS_Y || event->code==ABS_HAT0Y) {
                if (direction<0) host->inputs.controller_up_pressed=true;
                else host->inputs.controller_down_pressed=true;
            }
            if (!host->setup.keyboard && capture_axis(event->code))
                setup_offer(&host->setup,(struct controller_binding){BINDING_ABS,event->code,direction});
        }
    }
    for (int i=0;i<TWO_FORTY_ACTION_COUNT;i++)
        if (!before[i] && action_down(host,i)) host->inputs.pending_actions[i]=true;
}

static void process_input(struct host *host, int fd)
{
    struct input_device *device=NULL;
    for (int i=0;i<host->inputs.count;i++) if (host->inputs.devices[i].fd==fd) device=&host->inputs.devices[i];
    if (!device) return;
    struct input_event events[32]; ssize_t bytes;
    while ((bytes=read(fd,events,sizeof(events)))>0)
        for (size_t i=0;i<(size_t)bytes/sizeof(events[0]);i++) process_input_event(host,device,&events[i]);
}

static bool menu_confirmed(const struct two_forty_input *input)
{
    return input->pressed[KEY_ENTER] || input->action_pressed[TWO_FORTY_ACTION_CONFIRM];
}

static int menu_direction(const struct host *host)
{
    const struct two_forty_input *input=&host->inputs.state;
    bool up=input->pressed[KEY_UP] || input->action_pressed[TWO_FORTY_ACTION_UP] || host->inputs.controller_up_pressed;
    bool down=input->pressed[KEY_DOWN] || input->action_pressed[TWO_FORTY_ACTION_DOWN] || host->inputs.controller_down_pressed;
    return (int)down-(int)up;
}

static bool recovery_chord(const struct input_set *inputs)
{
    for (int i=0;i<inputs->count;i++) {
        const struct input_device *d=&inputs->devices[i];
        if (d->controller && ((d->keys[BTN_START] && d->keys[BTN_SELECT]) || (d->keys[BTN_TR2] && d->keys[BTN_TL2]))) return true;
    }
    return false;
}

static void update_setup(struct host *host)
{
    if (controller_buttons_down(&host->inputs)>=2) host->controller_menu_chord_frames++;
    else host->controller_menu_chord_frames=0;
    if (host->inputs.state.pressed[KEY_F1] || host->controller_menu_chord_frames>=60) {
        host->setup.active=false; host->ui_wait_release=true;
        host->settings_message="CANCELLED - NOTHING CHANGED";
        host->controller_menu_chord_frames=0; return;
    }
    setup_release(&host->setup,buttons_released(&host->inputs,host->setup.keyboard));
    if (host->setup.complete) {
        struct controller_binding *target=host->setup.keyboard?host->keyboard_bindings:host->bindings;
        struct controller_binding original[TWO_FORTY_ACTION_COUNT];
        memcpy(original,target,sizeof(original)); memcpy(target,host->setup.pending,sizeof(original));
        if (save_bindings(host)) host->settings_message="BUTTONS SAVED";
        else { memcpy(target,original,sizeof(original)); host->settings_message="SAVE FAILED - NOTHING CHANGED"; }
        host->setup.active=false; host->ui_wait_release=true;
    }
}

static void update_host(struct host *host)
{
    struct two_forty_input *input=&host->inputs.state;
    update_controller_actions(host);
    if (!host->setup.active && input->pressed[KEY_F12]) snapshot_requested=1;
    int direction=menu_direction(host);
    bool confirm=menu_confirmed(input);
    bool back=input->pressed[KEY_F1] || input->action_pressed[TWO_FORTY_ACTION_MENU];
    if (host->setup.active) update_setup(host);
    else if (host->ui_wait_release) {
        if (buttons_released(&host->inputs,false) && buttons_released(&host->inputs,true)) host->ui_wait_release=false;
    } else if (host->active_game) {
        if (recovery_chord(&host->inputs)) host->controller_menu_chord_frames++;
        else host->controller_menu_chord_frames=0;
        if (back || host->controller_menu_chord_frames>=60) {
            unload_game(host); host->controller_settings=false;
            host->controller_menu_chord_frames=0; host->ui_wait_release=true;
        } else host->game_api->update(input);
    } else if (host->display_settings) {
        if (direction) host->display_option=(host->display_option+direction+4)%4;
        int delta=(int)(input->pressed[KEY_RIGHT] || input->action_pressed[TWO_FORTY_ACTION_RIGHT])-
                  (int)(input->pressed[KEY_LEFT] || input->action_pressed[TWO_FORTY_ACTION_LEFT]);
        int *margin=host->display_option==0?&host->safe_x:&host->safe_y;
        int maximum=host->display_option==0?32:24;
        if (host->display_option<2 && delta) { *margin+=delta; if (*margin<0) *margin=0; if (*margin>maximum) *margin=maximum; update_safe_area(host); }
        if (back || (!direction && confirm && host->display_option==3)) {
            host->safe_x=host->saved_safe_x; host->safe_y=host->saved_safe_y;
            update_safe_area(host); host->display_settings=false;
            write_status(host);
        } else if (!direction && confirm && host->display_option==2) {
            if (save_bindings(host)) { host->display_settings=false; write_status(host); }
            else host->settings_message="SAVE FAILED - TRY AGAIN";
        }
    } else if (host->controller_settings) {
        if (direction) host->selected_option=(host->selected_option+direction+3)%3;
        else if (back) host->controller_settings=false;
        else if (confirm) {
            if (host->selected_option==2) host->controller_settings=false;
            else { setup_begin(&host->setup,host->selected_option==1); host->settings_message=""; }
        }
    } else {
        int count=host->game_count+3;
        if (direction) host->selected_game=(host->selected_game+direction+count)%count;
        else if (confirm) {
            if (host->selected_game<host->game_count) load_game(host,host->selected_game);
            else if (host->selected_game==host->game_count) { host->controller_settings=true; host->selected_option=0; host->settings_message=""; }
            else if (host->selected_game==host->game_count+1) {
                host->display_settings=true; host->display_option=0; host->settings_message="";
                host->saved_safe_x=host->safe_x; host->saved_safe_y=host->safe_y;
            } else power_down_pi();
        }
    }
    memset(input->pressed,0,sizeof(input->pressed));
    input->controller_pressed=false; host->inputs.controller_up_pressed=false; host->inputs.controller_down_pressed=false;
}

static void draw_host(struct host *host)
{
    clear_screen();
    if (host->active_game != NULL) host->game_api->render();
    else if (host->display_settings) draw_display_settings(host);
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
    update_safe_area(&host);
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
