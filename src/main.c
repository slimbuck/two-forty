#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
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

/*
 * Raspberry Pi OS Lite ships the runtime libraries used below, but not their
 * development headers. These are the small, stable ABI declarations needed by
 * this test. Keeping them here makes the first hardware proof build offline.
 */

#define DRM_DISPLAY_MODE_LEN 32
#define DRM_MODE_CONNECTED 1
#define DRM_MODE_PAGE_FLIP_EVENT 0x01
#define DRM_EVENT_CONTEXT_VERSION 2
#define DRM_FORMAT_XRGB8888 0x34325258u

typedef struct drm_mode_modeinfo {
    uint32_t clock;
    uint16_t hdisplay;
    uint16_t hsync_start;
    uint16_t hsync_end;
    uint16_t htotal;
    uint16_t hskew;
    uint16_t vdisplay;
    uint16_t vsync_start;
    uint16_t vsync_end;
    uint16_t vtotal;
    uint16_t vscan;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
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
    uint32_t min_width;
    uint32_t max_width;
    uint32_t min_height;
    uint32_t max_height;
} drmModeRes;

typedef struct drm_mode_connector {
    uint32_t connector_id;
    uint32_t encoder_id;
    uint32_t connector_type;
    uint32_t connector_type_id;
    int connection;
    uint32_t mmWidth;
    uint32_t mmHeight;
    int subpixel;
    int count_modes;
    drmModeModeInfo *modes;
    int count_props;
    uint32_t *props;
    uint64_t *prop_values;
    int count_encoders;
    uint32_t *encoders;
} drmModeConnector;

typedef struct drm_mode_encoder {
    uint32_t encoder_id;
    uint32_t encoder_type;
    uint32_t crtc_id;
    uint32_t possible_crtcs;
    uint32_t possible_clones;
} drmModeEncoder;

typedef struct drm_mode_crtc {
    uint32_t crtc_id;
    uint32_t buffer_id;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
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

union gbm_bo_handle {
    void *ptr;
    int32_t s32;
    uint32_t u32;
    uint64_t u64;
};

extern struct gbm_device *gbm_create_device(int fd);
extern void gbm_device_destroy(struct gbm_device *gbm);
extern struct gbm_surface *gbm_surface_create(struct gbm_device *gbm,
                                               uint32_t width, uint32_t height,
                                               uint32_t format, uint32_t flags);
extern void gbm_surface_destroy(struct gbm_surface *surface);
extern struct gbm_bo *gbm_surface_lock_front_buffer(struct gbm_surface *surface);
extern void gbm_surface_release_buffer(struct gbm_surface *surface,
                                        struct gbm_bo *bo);
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

#define EGL_FALSE 0
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
                                    EGLContext share_context,
                                    const EGLint *attrib_list);
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

struct framebuffer {
    int drm_fd;
    uint32_t fb_id;
};

struct input_set {
    int fds[MAX_INPUTS];
    int count;
    bool keys[KEY_MAX + 1];
};

struct game_config {
    int box_width;
    int box_height;
    int speed_x;
    int speed_y;
    char pattern_path[512];
    char bounce_sound_path[512];
    char sound_device[128];
    char snapshot_dir[512];
};

struct pattern {
    int width;
    int height;
    GLubyte *pixels;
};

struct app {
    int drm_fd;
    uint32_t connector_id;
    uint32_t crtc_id;
    drmModeModeInfo mode;
    drmModeCrtc *saved_crtc;
    struct gbm_device *gbm;
    struct gbm_surface *gbm_surface;
    struct gbm_bo *front_bo;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    struct input_set inputs;
    bool flip_pending;
    bool running;
    bool auto_move;
    int x;
    int y;
    int vx;
    int vy;
    int box_width;
    int box_height;
    struct game_config config;
    struct pattern pattern;
    pid_t sound_pid;
    unsigned int snapshot_sequence;
};

static volatile sig_atomic_t stop_requested;
static volatile sig_atomic_t snapshot_requested;

static void on_signal(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static void on_snapshot_signal(int signal_number)
{
    (void)signal_number;
    snapshot_requested = 1;
}

static void copy_string(char *destination, size_t capacity, const char *source)
{
    if (capacity > 0) {
        snprintf(destination, capacity, "%s", source);
    }
}

static char *trim(char *text)
{
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        ++text;
    }
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\r' || end[-1] == '\n')) {
        --end;
    }
    *end = '\0';
    return text;
}

static void default_config(struct game_config *config)
{
    config->box_width = 168;
    config->box_height = 120;
    config->speed_x = 2;
    config->speed_y = 1;
    copy_string(config->pattern_path, sizeof(config->pattern_path),
                "games/hardware-test/assets/colour-bars.ppm");
    copy_string(config->bounce_sound_path, sizeof(config->bounce_sound_path),
                "games/hardware-test/assets/edge-beep.wav");
    copy_string(config->sound_device, sizeof(config->sound_device), "plughw:0,0");
    copy_string(config->snapshot_dir, sizeof(config->snapshot_dir), "snapshots");
}

static bool load_config(struct game_config *config, const char *path)
{
    default_config(config);
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "Unable to open config %s: %s\n", path, strerror(errno));
        return false;
    }

    char line[1024];
    while (fgets(line, sizeof(line), file) != NULL) {
        char *entry = trim(line);
        if (*entry == '\0' || *entry == '#' || *entry == ';') {
            continue;
        }
        char *separator = strchr(entry, '=');
        if (separator == NULL) {
            continue;
        }
        *separator = '\0';
        char *key = trim(entry);
        char *value = trim(separator + 1);
        if (strcmp(key, "box_width") == 0) {
            config->box_width = atoi(value);
        } else if (strcmp(key, "box_height") == 0) {
            config->box_height = atoi(value);
        } else if (strcmp(key, "speed_x") == 0) {
            config->speed_x = atoi(value);
        } else if (strcmp(key, "speed_y") == 0) {
            config->speed_y = atoi(value);
        } else if (strcmp(key, "pattern") == 0) {
            copy_string(config->pattern_path, sizeof(config->pattern_path), value);
        } else if (strcmp(key, "bounce_sound") == 0) {
            copy_string(config->bounce_sound_path,
                        sizeof(config->bounce_sound_path), value);
        } else if (strcmp(key, "sound_device") == 0) {
            copy_string(config->sound_device, sizeof(config->sound_device), value);
        } else if (strcmp(key, "snapshot_dir") == 0) {
            copy_string(config->snapshot_dir, sizeof(config->snapshot_dir), value);
        }
    }
    fclose(file);
    if (config->box_width < 8 || config->box_height < 8 ||
        config->speed_x == 0 || config->speed_y == 0) {
        fprintf(stderr, "Invalid size or speed in %s.\n", path);
        return false;
    }
    return true;
}

static bool ppm_token(FILE *file, char *token, size_t capacity)
{
    int character;
    do {
        character = fgetc(file);
        if (character == '#') {
            while (character != '\n' && character != EOF) {
                character = fgetc(file);
            }
        }
    } while (character != EOF && (character == ' ' || character == '\t' ||
                                   character == '\r' || character == '\n'));
    if (character == EOF) {
        return false;
    }
    size_t length = 0;
    while (character != EOF && character != ' ' && character != '\t' &&
           character != '\r' && character != '\n' && character != '#') {
        if (length + 1 < capacity) {
            token[length++] = (char)character;
        }
        character = fgetc(file);
    }
    token[length] = '\0';
    return length > 0;
}

static bool load_pattern(struct pattern *pattern, const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "Unable to open pattern %s: %s\n", path, strerror(errno));
        return false;
    }

    char token[64];
    if (!ppm_token(file, token, sizeof(token)) || strcmp(token, "P3") != 0 ||
        !ppm_token(file, token, sizeof(token))) {
        fprintf(stderr, "%s is not a plain-text P3 PPM image.\n", path);
        fclose(file);
        return false;
    }
    pattern->width = atoi(token);
    if (!ppm_token(file, token, sizeof(token))) {
        fclose(file);
        return false;
    }
    pattern->height = atoi(token);
    if (!ppm_token(file, token, sizeof(token))) {
        fclose(file);
        return false;
    }
    const int maximum = atoi(token);
    if (pattern->width <= 0 || pattern->height <= 0 || maximum <= 0) {
        fprintf(stderr, "Invalid PPM dimensions or maximum in %s.\n", path);
        fclose(file);
        return false;
    }

    const size_t byte_count = (size_t)pattern->width * pattern->height * 3u;
    pattern->pixels = malloc(byte_count);
    if (pattern->pixels == NULL) {
        fclose(file);
        return false;
    }
    for (size_t i = 0; i < byte_count; ++i) {
        if (!ppm_token(file, token, sizeof(token))) {
            fprintf(stderr, "Unexpected end of PPM data in %s.\n", path);
            free(pattern->pixels);
            pattern->pixels = NULL;
            fclose(file);
            return false;
        }
        int value = atoi(token);
        if (value < 0) value = 0;
        if (value > maximum) value = maximum;
        pattern->pixels[i] = (GLubyte)((value * 255) / maximum);
    }
    fclose(file);
    printf("Pattern: %s (%dx%d source cells)\n", path,
           pattern->width, pattern->height);
    return true;
}

static void play_bounce_sound(struct app *app)
{
    if (app->sound_pid > 0) {
        if (waitpid(app->sound_pid, NULL, WNOHANG) == 0) {
            return;
        }
        app->sound_pid = 0;
    }

    pid_t child = fork();
    if (child == 0) {
        execlp("aplay", "aplay", "-q", "-D", app->config.sound_device,
               app->config.bounce_sound_path, (char *)NULL);
        _exit(127);
    }
    if (child > 0) {
        app->sound_pid = child;
    }
}

static void save_snapshot(struct app *app)
{
    const int width = app->mode.hdisplay;
    const int height = app->mode.vdisplay;
    const size_t row_bytes = (size_t)width * 3u;
    GLubyte *pixels = malloc(row_bytes * (size_t)height);
    if (pixels == NULL) {
        return;
    }
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    if (mkdir(app->config.snapshot_dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "Unable to create snapshot directory: %s\n", strerror(errno));
        free(pixels);
        return;
    }

    time_t now = time(NULL);
    struct tm timestamp;
    localtime_r(&now, &timestamp);
    char path[768];
    snprintf(path, sizeof(path),
             "%s/hardware-test-%04d%02d%02d-%02d%02d%02d-%03u.ppm",
             app->config.snapshot_dir, timestamp.tm_year + 1900,
             timestamp.tm_mon + 1, timestamp.tm_mday, timestamp.tm_hour,
             timestamp.tm_min, timestamp.tm_sec, app->snapshot_sequence++);
    FILE *file = fopen(path, "wb");
    if (file != NULL) {
        fprintf(file, "P6\n%d %d\n255\n", width, height);
        for (int y = height - 1; y >= 0; --y) {
            fwrite(pixels + (size_t)y * row_bytes, 1, row_bytes, file);
        }
        fclose(file);
        printf("Snapshot: %s\n", path);
    } else {
        fprintf(stderr, "Unable to write snapshot %s: %s\n", path, strerror(errno));
    }
    free(pixels);
}

static void destroy_framebuffer(struct gbm_bo *bo, void *data)
{
    (void)bo;
    struct framebuffer *fb = data;
    if (fb != NULL) {
        if (fb->fb_id != 0) {
            drmModeRmFB(fb->drm_fd, fb->fb_id);
        }
        free(fb);
    }
}

static uint32_t framebuffer_for_bo(struct app *app, struct gbm_bo *bo)
{
    struct framebuffer *fb = gbm_bo_get_user_data(bo);
    if (fb != NULL) {
        return fb->fb_id;
    }

    fb = calloc(1, sizeof(*fb));
    if (fb == NULL) {
        return 0;
    }

    fb->drm_fd = app->drm_fd;
    uint32_t handles[4] = {gbm_bo_get_handle(bo).u32, 0, 0, 0};
    uint32_t pitches[4] = {gbm_bo_get_stride(bo), 0, 0, 0};
    uint32_t offsets[4] = {0, 0, 0, 0};

    if (drmModeAddFB2(app->drm_fd, gbm_bo_get_width(bo),
                      gbm_bo_get_height(bo), DRM_FORMAT_XRGB8888,
                      handles, pitches, offsets, &fb->fb_id, 0) != 0) {
        fprintf(stderr, "drmModeAddFB2 failed: %s\n", strerror(errno));
        free(fb);
        return 0;
    }

    gbm_bo_set_user_data(bo, fb, destroy_framebuffer);
    return fb->fb_id;
}

static bool choose_display(struct app *app)
{
    drmModeRes *resources = drmModeGetResources(app->drm_fd);
    if (resources == NULL) {
        fprintf(stderr, "drmModeGetResources failed: %s\n", strerror(errno));
        return false;
    }

    drmModeConnector *connector = NULL;
    for (int i = 0; i < resources->count_connectors; ++i) {
        drmModeConnector *candidate =
            drmModeGetConnector(app->drm_fd, resources->connectors[i]);
        if (candidate != NULL && candidate->connection == DRM_MODE_CONNECTED &&
            candidate->count_modes > 0) {
            connector = candidate;
            break;
        }
        drmModeFreeConnector(candidate);
    }

    if (connector == NULL) {
        fprintf(stderr, "No connected DRM connector with a mode was found.\n");
        drmModeFreeResources(resources);
        return false;
    }

    drmModeEncoder *encoder = NULL;
    if (connector->encoder_id != 0) {
        encoder = drmModeGetEncoder(app->drm_fd, connector->encoder_id);
    }
    if (encoder == NULL) {
        for (int i = 0; i < connector->count_encoders; ++i) {
            encoder = drmModeGetEncoder(app->drm_fd, connector->encoders[i]);
            if (encoder != NULL) {
                break;
            }
        }
    }

    uint32_t crtc_id = encoder != NULL ? encoder->crtc_id : 0;
    if (crtc_id == 0 && encoder != NULL) {
        for (int i = 0; i < resources->count_crtcs; ++i) {
            if ((encoder->possible_crtcs & (1u << i)) != 0) {
                crtc_id = resources->crtcs[i];
                break;
            }
        }
    }

    if (crtc_id == 0) {
        fprintf(stderr, "No compatible CRTC was found.\n");
        drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        return false;
    }

    app->connector_id = connector->connector_id;
    app->crtc_id = crtc_id;
    app->mode = connector->modes[0];
    app->saved_crtc = drmModeGetCrtc(app->drm_fd, crtc_id);

    printf("DRM mode: %s %ux%u @ %u Hz, pixel clock %.3f MHz\n",
           app->mode.name, app->mode.hdisplay, app->mode.vdisplay,
           app->mode.vrefresh, app->mode.clock / 1000.0);

    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);
    return true;
}

static bool init_graphics(struct app *app)
{
    app->gbm = gbm_create_device(app->drm_fd);
    if (app->gbm == NULL) {
        fprintf(stderr, "gbm_create_device failed.\n");
        return false;
    }

    app->gbm_surface = gbm_surface_create(
        app->gbm, app->mode.hdisplay, app->mode.vdisplay,
        DRM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (app->gbm_surface == NULL) {
        fprintf(stderr, "gbm_surface_create failed.\n");
        return false;
    }

    app->egl_display = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, app->gbm, NULL);
    if (app->egl_display == EGL_NO_DISPLAY) {
        app->egl_display = eglGetDisplay((EGLNativeDisplayType)app->gbm);
    }
    if (app->egl_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "Unable to get an EGL display (0x%x).\n", eglGetError());
        return false;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (!eglInitialize(app->egl_display, &major, &minor)) {
        fprintf(stderr, "eglInitialize failed (0x%x).\n", eglGetError());
        return false;
    }
    printf("EGL version: %d.%d\n", major, minor);

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        fprintf(stderr, "eglBindAPI failed (0x%x).\n", eglGetError());
        return false;
    }

    const EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 0,
        EGL_NONE
    };
    EGLConfig config = NULL;
    EGLint config_count = 0;
    if (!eglChooseConfig(app->egl_display, config_attributes, &config, 1,
                         &config_count) || config_count != 1) {
        fprintf(stderr, "No matching EGL configuration (0x%x).\n", eglGetError());
        return false;
    }

    const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    app->egl_context = eglCreateContext(app->egl_display, config,
                                         EGL_NO_CONTEXT, context_attributes);
    if (app->egl_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "eglCreateContext failed (0x%x).\n", eglGetError());
        return false;
    }

    app->egl_surface = eglCreateWindowSurface(
        app->egl_display, config, (EGLNativeWindowType)app->gbm_surface, NULL);
    if (app->egl_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "eglCreateWindowSurface failed (0x%x).\n", eglGetError());
        return false;
    }

    if (!eglMakeCurrent(app->egl_display, app->egl_surface, app->egl_surface,
                        app->egl_context)) {
        fprintf(stderr, "eglMakeCurrent failed (0x%x).\n", eglGetError());
        return false;
    }

    eglSwapInterval(app->egl_display, 0);
    glViewport(0, 0, app->mode.hdisplay, app->mode.vdisplay);
    return true;
}

static void open_inputs(struct input_set *inputs)
{
    inputs->count = 0;
    memset(inputs->keys, 0, sizeof(inputs->keys));

    for (int index = 0; index < MAX_INPUTS; ++index) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/input/event%d", index);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }

        char name[128] = "unknown";
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        inputs->fds[inputs->count++] = fd;
        printf("Input: %s (%s)\n", path, name);
    }
}

static void close_inputs(struct input_set *inputs)
{
    for (int i = 0; i < inputs->count; ++i) {
        close(inputs->fds[i]);
    }
    inputs->count = 0;
}

static void process_input_fd(struct app *app, int fd)
{
    struct input_event events[32];
    ssize_t bytes = 0;
    while ((bytes = read(fd, events, sizeof(events))) > 0) {
        size_t count = (size_t)bytes / sizeof(events[0]);
        for (size_t i = 0; i < count; ++i) {
            if (events[i].type != EV_KEY || events[i].code > KEY_MAX) {
                continue;
            }

            const bool down = events[i].value != 0;
            const bool newly_pressed = events[i].value == 1;
            app->inputs.keys[events[i].code] = down;

            if (newly_pressed &&
                (events[i].code == KEY_ESC || events[i].code == KEY_Q)) {
                app->running = false;
            } else if (newly_pressed && events[i].code == KEY_SPACE) {
                play_bounce_sound(app);
            } else if (newly_pressed && events[i].code == KEY_ENTER) {
                app->auto_move = !app->auto_move;
            } else if (newly_pressed && events[i].code == KEY_P) {
                snapshot_requested = 1;
            }
        }
    }
}

static bool update_scene(struct app *app)
{
    const int speed = 3;
    int dx = 0;
    int dy = 0;

    if (app->inputs.keys[KEY_LEFT] || app->inputs.keys[KEY_A]) {
        dx -= speed;
    }
    if (app->inputs.keys[KEY_RIGHT] || app->inputs.keys[KEY_D]) {
        dx += speed;
    }
    if (app->inputs.keys[KEY_UP] || app->inputs.keys[KEY_W]) {
        dy += speed;
    }
    if (app->inputs.keys[KEY_DOWN] || app->inputs.keys[KEY_S]) {
        dy -= speed;
    }

    if (dx != 0 || dy != 0) {
        app->auto_move = false;
        app->x += dx;
        app->y += dy;
    } else if (app->auto_move) {
        app->x += app->vx;
        app->y += app->vy;
    }

    bool bounced = false;
    const int max_x = (int)app->mode.hdisplay - app->box_width;
    const int max_y = (int)app->mode.vdisplay - app->box_height;
    if (app->x < 0) {
        app->x = 0;
        app->vx = abs(app->vx);
        bounced = true;
    } else if (app->x > max_x) {
        app->x = max_x;
        app->vx = -abs(app->vx);
        bounced = true;
    }
    if (app->y < 0) {
        app->y = 0;
        app->vy = abs(app->vy);
        bounced = true;
    } else if (app->y > max_y) {
        app->y = max_y;
        app->vy = -abs(app->vy);
        bounced = true;
    }
    return bounced;
}

static void draw_pattern(const struct app *app)
{
    for (int row = 0; row < app->pattern.height; ++row) {
        const int y0 = app->y + (row * app->box_height) / app->pattern.height;
        const int y1 = app->y + ((row + 1) * app->box_height) / app->pattern.height;
        for (int column = 0; column < app->pattern.width; ++column) {
            const int x0 = app->x + (column * app->box_width) / app->pattern.width;
            const int x1 = app->x + ((column + 1) * app->box_width) / app->pattern.width;
            const int source_row = app->pattern.height - 1 - row;
            const GLubyte *rgb = app->pattern.pixels +
                ((size_t)source_row * app->pattern.width + column) * 3u;
            glScissor(x0, y0, x1 - x0, y1 - y0);
            glClearColor(rgb[0] / 255.0f, rgb[1] / 255.0f,
                         rgb[2] / 255.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }
}

static void draw_scene(struct app *app)
{
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.005f, 0.005f, 0.008f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_SCISSOR_TEST);
    glScissor(app->x - 2, app->y - 2, app->box_width + 4, app->box_height + 4);
    glClearColor(0.85f, 0.85f, 0.85f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_pattern(app);
    glDisable(GL_SCISSOR_TEST);

    if (snapshot_requested) {
        snapshot_requested = 0;
        save_snapshot(app);
    }
}

static void page_flip_handler(int fd, unsigned int sequence,
                              unsigned int tv_sec, unsigned int tv_usec,
                              void *user_data)
{
    (void)fd;
    (void)sequence;
    (void)tv_sec;
    (void)tv_usec;
    struct app *app = user_data;
    app->flip_pending = false;
}

static void wait_for_flip_and_input(struct app *app)
{
    struct pollfd poll_fds[MAX_INPUTS + 1];
    poll_fds[0].fd = app->drm_fd;
    poll_fds[0].events = POLLIN;
    for (int i = 0; i < app->inputs.count; ++i) {
        poll_fds[i + 1].fd = app->inputs.fds[i];
        poll_fds[i + 1].events = POLLIN;
    }

    drmEventContext event_context = {
        .version = DRM_EVENT_CONTEXT_VERSION,
        .vblank_handler = NULL,
        .page_flip_handler = page_flip_handler
    };

    while (app->flip_pending && app->running && !stop_requested) {
        int result = poll(poll_fds, (nfds_t)(app->inputs.count + 1), -1);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "poll failed: %s\n", strerror(errno));
            app->running = false;
            break;
        }

        if ((poll_fds[0].revents & POLLIN) != 0) {
            drmHandleEvent(app->drm_fd, &event_context);
        }
        for (int i = 0; i < app->inputs.count; ++i) {
            if ((poll_fds[i + 1].revents & POLLIN) != 0) {
                process_input_fd(app, poll_fds[i + 1].fd);
            }
        }
    }
}

static bool present_first_frame(struct app *app)
{
    draw_scene(app);
    if (!eglSwapBuffers(app->egl_display, app->egl_surface)) {
        fprintf(stderr, "eglSwapBuffers failed (0x%x).\n", eglGetError());
        return false;
    }

    app->front_bo = gbm_surface_lock_front_buffer(app->gbm_surface);
    if (app->front_bo == NULL) {
        fprintf(stderr, "Unable to lock the first GBM buffer.\n");
        return false;
    }
    uint32_t fb_id = framebuffer_for_bo(app, app->front_bo);
    if (fb_id == 0) {
        return false;
    }

    if (drmModeSetCrtc(app->drm_fd, app->crtc_id, fb_id, 0, 0,
                       &app->connector_id, 1, &app->mode) != 0) {
        fprintf(stderr, "drmModeSetCrtc failed: %s\n", strerror(errno));
        return false;
    }
    return true;
}

static bool present_next_frame(struct app *app)
{
    if (update_scene(app)) {
        play_bounce_sound(app);
    }
    draw_scene(app);
    if (!eglSwapBuffers(app->egl_display, app->egl_surface)) {
        fprintf(stderr, "eglSwapBuffers failed (0x%x).\n", eglGetError());
        return false;
    }

    struct gbm_bo *next_bo = gbm_surface_lock_front_buffer(app->gbm_surface);
    if (next_bo == NULL) {
        fprintf(stderr, "Unable to lock the next GBM buffer.\n");
        return false;
    }
    uint32_t fb_id = framebuffer_for_bo(app, next_bo);
    if (fb_id == 0) {
        gbm_surface_release_buffer(app->gbm_surface, next_bo);
        return false;
    }

    app->flip_pending = true;
    if (drmModePageFlip(app->drm_fd, app->crtc_id, fb_id,
                        DRM_MODE_PAGE_FLIP_EVENT, app) != 0) {
        fprintf(stderr, "drmModePageFlip failed: %s\n", strerror(errno));
        app->flip_pending = false;
        gbm_surface_release_buffer(app->gbm_surface, next_bo);
        return false;
    }

    wait_for_flip_and_input(app);
    if (!app->flip_pending) {
        gbm_surface_release_buffer(app->gbm_surface, app->front_bo);
        app->front_bo = next_bo;
    } else {
        gbm_surface_release_buffer(app->gbm_surface, next_bo);
    }
    return true;
}

static void cleanup(struct app *app)
{
    close_inputs(&app->inputs);

    if (app->saved_crtc != NULL && app->drm_fd >= 0) {
        drmModeSetCrtc(app->drm_fd, app->saved_crtc->crtc_id,
                       app->saved_crtc->buffer_id, app->saved_crtc->x,
                       app->saved_crtc->y, &app->connector_id, 1,
                       app->saved_crtc->mode_valid ? &app->saved_crtc->mode : NULL);
    }

    if (app->front_bo != NULL && app->gbm_surface != NULL) {
        gbm_surface_release_buffer(app->gbm_surface, app->front_bo);
        app->front_bo = NULL;
    }
    if (app->egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(app->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        if (app->egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(app->egl_display, app->egl_surface);
        }
        if (app->egl_context != EGL_NO_CONTEXT) {
            eglDestroyContext(app->egl_display, app->egl_context);
        }
        eglTerminate(app->egl_display);
    }
    if (app->gbm_surface != NULL) {
        gbm_surface_destroy(app->gbm_surface);
    }
    if (app->gbm != NULL) {
        gbm_device_destroy(app->gbm);
    }
    drmModeFreeCrtc(app->saved_crtc);
    if (app->drm_fd >= 0) {
        drmDropMaster(app->drm_fd);
        close(app->drm_fd);
    }
    if (app->sound_pid > 0) {
        waitpid(app->sound_pid, NULL, WNOHANG);
    }
    free(app->pattern.pixels);
}

int main(int argc, char **argv)
{
    struct app app;
    memset(&app, 0, sizeof(app));
    app.drm_fd = -1;
    app.egl_display = EGL_NO_DISPLAY;
    app.egl_context = EGL_NO_CONTEXT;
    app.egl_surface = EGL_NO_SURFACE;
    app.running = true;
    app.auto_move = true;
    const char *config_path = argc > 1 ? argv[1] :
        "games/hardware-test/game.conf";
    if (!load_config(&app.config, config_path) ||
        !load_pattern(&app.pattern, app.config.pattern_path)) {
        cleanup(&app);
        return EXIT_FAILURE;
    }
    app.vx = app.config.speed_x;
    app.vy = app.config.speed_y;
    app.box_width = app.config.box_width;
    app.box_height = app.config.box_height;

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGUSR1, on_snapshot_signal);

    app.drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (app.drm_fd < 0) {
        fprintf(stderr, "Unable to open /dev/dri/card0: %s\n", strerror(errno));
        cleanup(&app);
        return EXIT_FAILURE;
    }
    if (drmSetMaster(app.drm_fd) != 0 && errno != EINVAL) {
        fprintf(stderr, "Warning: drmSetMaster failed: %s\n", strerror(errno));
    }

    if (!choose_display(&app)) {
        cleanup(&app);
        return EXIT_FAILURE;
    }

    if (app.box_width > (int)app.mode.hdisplay ||
        app.box_height > (int)app.mode.vdisplay) {
        fprintf(stderr, "Configured test card is larger than the display.\n");
        cleanup(&app);
        return EXIT_FAILURE;
    }
    app.x = ((int)app.mode.hdisplay - app.box_width) / 2;
    app.y = ((int)app.mode.vdisplay - app.box_height) / 2;

    if (!init_graphics(&app)) {
        cleanup(&app);
        return EXIT_FAILURE;
    }
    open_inputs(&app.inputs);

    if (!present_first_frame(&app)) {
        cleanup(&app);
        return EXIT_FAILURE;
    }

    printf("Hardware test running. Audio: %s; snapshots: %s\n",
           app.config.sound_device, app.config.snapshot_dir);
    puts("Arrows/WASD move, Enter toggles motion, Space tests sound, P snapshots, Esc/Q quits.");
    while (app.running && !stop_requested) {
        if (!present_next_frame(&app)) {
            app.running = false;
        }
    }

    cleanup(&app);
    puts("Console framebuffer restored.");
    return EXIT_SUCCESS;
}
