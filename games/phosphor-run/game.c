#include "two_forty.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TILE 8
#define MAX_PARTICLES 96
#define PLAYER_WIDTH 12
#define PLAYER_HEIGHT 14

enum phase { PHASE_TITLE, PHASE_PLAY, PHASE_DEAD, PHASE_WIN };

struct colour { unsigned char r, g, b; };

struct settings {
    float move_accel, max_run_speed, ground_friction, air_drag;
    float gravity, max_fall_speed, jump_speed, dash_speed, camera_lag;
    int coyote_frames, jump_buffer_frames, dash_frames, title_frames;
    char level[512], sprite[512], sound_device[128];
    char jump_sound[512], dash_sound[512], shard_sound[512];
    char checkpoint_sound[512], death_sound[512], win_sound[512];
    struct colour background, deep, platform, edge, phosphor, amber, hazard, paper;
};

struct level {
    int width, height;
    char *tiles;
    char *original;
    int total_shards;
    float initial_x, initial_y;
};

struct sprite {
    int width, height;
    char *pixels;
};

struct particle {
    float x, y, vx, vy;
    int life;
    struct colour colour;
};

static const struct two_forty_host_api *host;
static struct settings settings;
static struct level level;
static struct sprite sprite;
static struct particle particles[MAX_PARTICLES];
static unsigned int random_state = 0x51a7u;
static enum phase phase;
static float player_x, player_y, velocity_x, velocity_y, camera_x;
static float respawn_x, respawn_y;
static int collected_shards, deaths, coyote_timer, jump_buffer, dash_timer;
static int death_timer, title_timer, win_timer, frame_number, facing;
static bool on_ground, touching_left, touching_right, dash_available;

static float absolute(float value) { return value < 0 ? -value : value; }
static float clampf(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}
static unsigned int random_value(void) {
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}

static void copy_text(char *destination, size_t capacity, const char *source)
{
    if (capacity > 0) snprintf(destination, capacity, "%s", source);
}

static char *trim(char *text)
{
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') ++text;
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\r' || end[-1] == '\n')) --end;
    *end = '\0';
    return text;
}

static struct colour colour_from_hex(const char *text, struct colour fallback)
{
    unsigned int value;
    if (strlen(text) != 6 || sscanf(text, "%x", &value) != 1) return fallback;
    return (struct colour){(value >> 16) & 255u, (value >> 8) & 255u, value & 255u};
}

static void default_settings(void)
{
    settings = (struct settings){
        .move_accel=.34f,.max_run_speed=2.35f,.ground_friction=.72f,.air_drag=.95f,
        .gravity=.34f,.max_fall_speed=5.8f,.jump_speed=5.35f,.dash_speed=6.2f,
        .camera_lag=.12f,.coyote_frames=7,.jump_buffer_frames=7,.dash_frames=8,
        .title_frames=240,
        .background={5,10,20},.deep={9,26,43},.platform={22,60,85},
        .edge={53,215,211},.phosphor={156,255,87},.amber={255,180,59},
        .hazard={255,91,79},.paper={244,241,207}
    };
    copy_text(settings.level, sizeof(settings.level), "games/phosphor-run/assets/level-01.txt");
    copy_text(settings.sprite, sizeof(settings.sprite), "games/phosphor-run/assets/player.sprite");
    copy_text(settings.sound_device, sizeof(settings.sound_device), "plughw:0,0");
    copy_text(settings.jump_sound, sizeof(settings.jump_sound), "games/phosphor-run/assets/jump.wav");
    copy_text(settings.dash_sound, sizeof(settings.dash_sound), "games/phosphor-run/assets/dash.wav");
    copy_text(settings.shard_sound, sizeof(settings.shard_sound), "games/phosphor-run/assets/shard.wav");
    copy_text(settings.checkpoint_sound, sizeof(settings.checkpoint_sound), "games/phosphor-run/assets/checkpoint.wav");
    copy_text(settings.death_sound, sizeof(settings.death_sound), "games/phosphor-run/assets/death.wav");
    copy_text(settings.win_sound, sizeof(settings.win_sound), "games/phosphor-run/assets/win.wav");
}

static bool load_settings(const char *path)
{
    default_settings();
    FILE *file = fopen(path, "r");
    if (file == NULL) return false;
    char line[1024];
    while (fgets(line, sizeof(line), file) != NULL) {
        char *entry = trim(line);
        if (!*entry || *entry == '#' || *entry == ';') continue;
        char *separator = strchr(entry, '=');
        if (separator == NULL) continue;
        *separator = '\0';
        char *key = trim(entry), *value = trim(separator + 1);
        if (!strcmp(key,"move_accel")) settings.move_accel = strtof(value,NULL);
        else if (!strcmp(key,"max_run_speed")) settings.max_run_speed = strtof(value,NULL);
        else if (!strcmp(key,"ground_friction")) settings.ground_friction = strtof(value,NULL);
        else if (!strcmp(key,"air_drag")) settings.air_drag = strtof(value,NULL);
        else if (!strcmp(key,"gravity")) settings.gravity = strtof(value,NULL);
        else if (!strcmp(key,"max_fall_speed")) settings.max_fall_speed = strtof(value,NULL);
        else if (!strcmp(key,"jump_speed")) settings.jump_speed = strtof(value,NULL);
        else if (!strcmp(key,"dash_speed")) settings.dash_speed = strtof(value,NULL);
        else if (!strcmp(key,"camera_lag")) settings.camera_lag = strtof(value,NULL);
        else if (!strcmp(key,"coyote_frames")) settings.coyote_frames = atoi(value);
        else if (!strcmp(key,"jump_buffer_frames")) settings.jump_buffer_frames = atoi(value);
        else if (!strcmp(key,"dash_frames")) settings.dash_frames = atoi(value);
        else if (!strcmp(key,"title_frames")) settings.title_frames = atoi(value);
        else if (!strcmp(key,"level")) copy_text(settings.level,sizeof(settings.level),value);
        else if (!strcmp(key,"player_sprite")) copy_text(settings.sprite,sizeof(settings.sprite),value);
        else if (!strcmp(key,"sound_device")) copy_text(settings.sound_device,sizeof(settings.sound_device),value);
        else if (!strcmp(key,"sound_jump")) copy_text(settings.jump_sound,sizeof(settings.jump_sound),value);
        else if (!strcmp(key,"sound_dash")) copy_text(settings.dash_sound,sizeof(settings.dash_sound),value);
        else if (!strcmp(key,"sound_shard")) copy_text(settings.shard_sound,sizeof(settings.shard_sound),value);
        else if (!strcmp(key,"sound_checkpoint")) copy_text(settings.checkpoint_sound,sizeof(settings.checkpoint_sound),value);
        else if (!strcmp(key,"sound_death")) copy_text(settings.death_sound,sizeof(settings.death_sound),value);
        else if (!strcmp(key,"sound_win")) copy_text(settings.win_sound,sizeof(settings.win_sound),value);
        else if (!strcmp(key,"background")) settings.background=colour_from_hex(value,settings.background);
        else if (!strcmp(key,"deep_machinery")) settings.deep=colour_from_hex(value,settings.deep);
        else if (!strcmp(key,"platform")) settings.platform=colour_from_hex(value,settings.platform);
        else if (!strcmp(key,"platform_edge")) settings.edge=colour_from_hex(value,settings.edge);
        else if (!strcmp(key,"phosphor")) settings.phosphor=colour_from_hex(value,settings.phosphor);
        else if (!strcmp(key,"amber")) settings.amber=colour_from_hex(value,settings.amber);
        else if (!strcmp(key,"hazard")) settings.hazard=colour_from_hex(value,settings.hazard);
        else if (!strcmp(key,"paper")) settings.paper=colour_from_hex(value,settings.paper);
    }
    fclose(file);
    return true;
}

static bool load_level(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) { fprintf(stderr,"phosphor-run: %s: %s\n",path,strerror(errno)); return false; }
    char **rows = NULL;
    int count = 0, width = 0;
    char line[2048];
    while (fgets(line, sizeof(line), file) != NULL) {
        char *newline = strpbrk(line, "\r\n");
        if (newline) *newline = '\0';
        if (line[0] == '#' && line[1] == ' ') continue;
        if (!line[0]) continue;
        char **next = realloc(rows, (size_t)(count + 1) * sizeof(*rows));
        if (next == NULL) { fclose(file); return false; }
        rows = next;
        rows[count] = strdup(line);
        if (rows[count] == NULL) { fclose(file); return false; }
        int length = (int)strlen(line);
        if (length > width) width = length;
        count++;
    }
    fclose(file);
    if (count == 0 || width == 0) return false;
    level.width = width; level.height = count;
    size_t size = (size_t)width * count;
    level.tiles = malloc(size); level.original = malloc(size);
    if (level.tiles == NULL || level.original == NULL) return false;
    memset(level.tiles, '.', size);
    for (int row = 0; row < count; ++row) {
        memcpy(level.tiles + (size_t)row * width, rows[row], strlen(rows[row]));
        free(rows[row]);
    }
    free(rows);
    for (int row = 0; row < count; ++row) {
        for (int column = 0; column < width; ++column) {
            char *tile = &level.tiles[(size_t)row * width + column];
            if (*tile == 'S') {
                level.initial_x = column * TILE;
                level.initial_y = (count - 1 - row) * TILE;
                *tile = '.';
            } else if (*tile == 'o') level.total_shards++;
        }
    }
    memcpy(level.original, level.tiles, size);
    return true;
}

static bool load_sprite(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL) return false;
    char **rows = NULL;
    char line[256];
    int count = 0, width = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        char *newline = strpbrk(line, "\r\n");
        if (newline) *newline = '\0';
        if (!line[0] || line[0] == '#') continue;
        int length = (int)strlen(line);
        if (!width) width = length;
        if (length != width) { fclose(file); return false; }
        char **next = realloc(rows, (size_t)(count + 1) * sizeof(*rows));
        if (next == NULL) { fclose(file); return false; }
        rows = next; rows[count++] = strdup(line);
    }
    fclose(file);
    if (count == 0 || width == 0) return false;
    sprite.width = width; sprite.height = count;
    sprite.pixels = malloc((size_t)width * count);
    if (sprite.pixels == NULL) return false;
    for (int row = 0; row < count; ++row) {
        memcpy(sprite.pixels + (size_t)row * width, rows[row], (size_t)width);
        free(rows[row]);
    }
    free(rows);
    return true;
}

static char *tile_pointer(int tile_x, int tile_y)
{
    if (tile_x < 0 || tile_x >= level.width || tile_y < 0 || tile_y >= level.height) return NULL;
    int row = level.height - 1 - tile_y;
    return &level.tiles[(size_t)row * level.width + tile_x];
}

static char tile_at(int tile_x, int tile_y)
{
    char *tile = tile_pointer(tile_x, tile_y);
    if (tile == NULL) return tile_x < 0 || tile_x >= level.width ? '#' : '.';
    return *tile;
}

static bool solid_at_rect(float x, float y, int width, int height)
{
    int left = (int)x / TILE;
    int right = (int)(x + width - .01f) / TILE;
    int bottom = (int)y / TILE;
    int top = (int)(y + height - .01f) / TILE;
    for (int ty = bottom; ty <= top; ++ty)
        for (int tx = left; tx <= right; ++tx)
            if (tile_at(tx, ty) == '#') return true;
    return false;
}

static bool touching_tile(char wanted)
{
    int left = (int)player_x / TILE;
    int right = (int)(player_x + PLAYER_WIDTH - 1) / TILE;
    int bottom = (int)player_y / TILE;
    int top = (int)(player_y + PLAYER_HEIGHT - 1) / TILE;
    for (int ty = bottom; ty <= top; ++ty)
        for (int tx = left; tx <= right; ++tx)
            if (tile_at(tx,ty) == wanted) return true;
    return false;
}

static void add_particle(float x, float y, float vx, float vy, int life, struct colour colour)
{
    for (int index = 0; index < MAX_PARTICLES; ++index) {
        if (particles[index].life <= 0) {
            particles[index] = (struct particle){x,y,vx,vy,life,colour};
            return;
        }
    }
}

static void burst(float x, float y, int count, struct colour colour)
{
    for (int index = 0; index < count; ++index) {
        float vx = ((int)(random_value() % 200) - 100) / 70.0f;
        float vy = ((int)(random_value() % 150) + 30) / 70.0f;
        add_particle(x, y, vx, vy, 18 + (int)(random_value() % 18), colour);
    }
}

static void update_particles(void)
{
    for (int index = 0; index < MAX_PARTICLES; ++index) {
        struct particle *particle = &particles[index];
        if (particle->life <= 0) continue;
        particle->x += particle->vx; particle->y += particle->vy;
        particle->vy -= .05f; particle->life--;
    }
}

static bool move_axis(float amount, bool horizontal)
{
    int steps = (int)absolute(amount) + 1;
    float movement = amount / steps;
    for (int step = 0; step < steps; ++step) {
        float next_x = player_x + (horizontal ? movement : 0);
        float next_y = player_y + (horizontal ? 0 : movement);
        if (solid_at_rect(next_x, next_y, PLAYER_WIDTH, PLAYER_HEIGHT)) return false;
        player_x = next_x; player_y = next_y;
    }
    return true;
}

static void play(const char *path)
{
    host->play_sound(host->context, settings.sound_device, path);
}

static void respawn(void)
{
    player_x = respawn_x; player_y = respawn_y;
    velocity_x = velocity_y = 0;
    dash_timer = jump_buffer = coyote_timer = 0;
    dash_available = true;
    phase = PHASE_PLAY; death_timer = 0;
}

static void new_run(void)
{
    memcpy(level.tiles, level.original, (size_t)level.width * level.height);
    collected_shards = deaths = 0;
    respawn_x = level.initial_x; respawn_y = level.initial_y;
    camera_x = clampf(respawn_x - 60, 0, level.width * TILE - host->screen_width);
    memset(particles, 0, sizeof(particles));
    respawn();
}

static void die(void)
{
    if (phase != PHASE_PLAY) return;
    phase = PHASE_DEAD; death_timer = 34; deaths++;
    burst(player_x + 6, player_y + 7, 20, settings.hazard);
    play(settings.death_sound);
}

static void collect_world_items(void)
{
    int left = (int)player_x / TILE;
    int right = (int)(player_x + PLAYER_WIDTH - 1) / TILE;
    int bottom = (int)player_y / TILE;
    int top = (int)(player_y + PLAYER_HEIGHT - 1) / TILE;
    for (int ty = bottom; ty <= top; ++ty) {
        for (int tx = left; tx <= right; ++tx) {
            char *tile = tile_pointer(tx,ty);
            if (tile == NULL) continue;
            if (*tile == 'o') {
                *tile = '.'; collected_shards++;
                burst(tx*TILE+4,ty*TILE+4,10,settings.phosphor);
                play(settings.shard_sound);
                dash_available = true;
            } else if (*tile == 'C' && (respawn_x != tx*TILE || respawn_y != (ty+1)*TILE)) {
                respawn_x = tx*TILE - 2; respawn_y = (ty+1)*TILE;
                burst(tx*TILE+4,ty*TILE+8,18,settings.amber);
                play(settings.checkpoint_sound);
            } else if (*tile == 'E' && collected_shards == level.total_shards) {
                phase = PHASE_WIN; win_timer = 0;
                burst(tx*TILE+4,ty*TILE+8,32,settings.phosphor);
                play(settings.win_sound);
            }
        }
    }
}

static bool pressed_jump(const struct two_forty_input *input)
{
    return input->pressed[KEY_Z] || input->pressed[KEY_SPACE] ||
           input->pressed[KEY_UP] || input->pressed[KEY_W];
}

static bool held_jump(const struct two_forty_input *input)
{
    return input->keys[KEY_Z] || input->keys[KEY_SPACE] ||
           input->keys[KEY_UP] || input->keys[KEY_W];
}

static void update_play(const struct two_forty_input *input)
{
    if (input->pressed[KEY_R]) { respawn(); return; }
    int direction = 0;
    if (input->keys[KEY_LEFT] || input->keys[KEY_A]) direction--;
    if (input->keys[KEY_RIGHT] || input->keys[KEY_D]) direction++;
    if (direction) facing = direction;

    if (pressed_jump(input)) jump_buffer = settings.jump_buffer_frames;
    else if (jump_buffer > 0) jump_buffer--;
    if (on_ground) coyote_timer = settings.coyote_frames;
    else if (coyote_timer > 0) coyote_timer--;

    if (jump_buffer > 0 && (coyote_timer > 0 || touching_left || touching_right)) {
        velocity_y = settings.jump_speed;
        if (!on_ground && (touching_left || touching_right))
            velocity_x = touching_left ? settings.max_run_speed * 1.25f : -settings.max_run_speed * 1.25f;
        jump_buffer = coyote_timer = 0;
        play(settings.jump_sound);
        burst(player_x+6,player_y,5,settings.edge);
    }

    bool dash_pressed = input->pressed[KEY_X] || input->pressed[KEY_LEFTSHIFT] ||
                        input->pressed[KEY_RIGHTSHIFT];
    if (dash_pressed && dash_available && dash_timer == 0) {
        dash_timer = settings.dash_frames;
        dash_available = false;
        velocity_x = facing * settings.dash_speed;
        velocity_y = 0;
        play(settings.dash_sound);
        burst(player_x+6,player_y+7,10,settings.amber);
    }

    if (dash_timer > 0) {
        dash_timer--;
        velocity_x = facing * settings.dash_speed;
        velocity_y = 0;
        if ((frame_number & 1) == 0)
            add_particle(player_x + (facing < 0 ? PLAYER_WIDTH : 0), player_y+7,
                         -facing*.3f, 0, 10, settings.edge);
    } else {
        velocity_x += direction * settings.move_accel;
        velocity_x = clampf(velocity_x, -settings.max_run_speed, settings.max_run_speed);
        velocity_x *= on_ground && direction == 0 ? settings.ground_friction : settings.air_drag;
        velocity_y -= settings.gravity;
        if (velocity_y < -settings.max_fall_speed) velocity_y = -settings.max_fall_speed;
        if (!held_jump(input) && velocity_y > 2.0f) velocity_y *= .58f;
    }

    if (!move_axis(velocity_x, true)) velocity_x = 0;
    if (!move_axis(velocity_y, false)) velocity_y = 0;
    on_ground = solid_at_rect(player_x,player_y-1,PLAYER_WIDTH,PLAYER_HEIGHT);
    touching_left = solid_at_rect(player_x-1,player_y,PLAYER_WIDTH,PLAYER_HEIGHT);
    touching_right = solid_at_rect(player_x+1,player_y,PLAYER_WIDTH,PLAYER_HEIGHT);
    if (on_ground) dash_available = true;
    if ((touching_left || touching_right) && velocity_y < -1.2f) velocity_y = -1.2f;

    if (touching_tile('^') || player_y < -24) die();
    else collect_world_items();

    float maximum_camera = level.width * TILE - host->screen_width;
    float target = clampf(player_x - host->screen_width*.42f, 0, maximum_camera);
    camera_x += (target-camera_x)*settings.camera_lag;
}

static struct colour sprite_colour(char value)
{
    if (value == 'n') return settings.deep;
    if (value == 's') return settings.platform;
    if (value == 'c') return settings.edge;
    if (value == 'a') return settings.amber;
    if (value == 'w') return settings.paper;
    if (value == 'r') return settings.hazard;
    return settings.background;
}

static void rectangle(int x, int y, int width, int height, struct colour colour)
{
    host->fill_rect(host->context,x,y,width,height,colour.r,colour.g,colour.b);
}

static void text(int x, int y, const char *value, int scale, struct colour colour)
{
    host->draw_text(host->context,x,y,value,scale,colour.r,colour.g,colour.b);
}

static void render_background(void)
{
    rectangle(0,0,host->screen_width,host->screen_height,settings.background);
    int slow = (int)(camera_x*.18f);
    for (int index = -1; index < 8; ++index) {
        int x = index*52-(slow%52);
        int height = 58 + ((index*37+113)&63);
        rectangle(x,0,36,height,settings.deep);
        rectangle(x+7,height,3,70,settings.deep);
        rectangle(x+25,height-8,2,54,settings.deep);
        for (int lamp=0;lamp<3;++lamp)
            if (((index+lamp+frame_number/45)&3)==0)
                rectangle(x+13,height-14-lamp*13,3,2,settings.amber);
    }
    for (int index=0;index<18;++index) {
        int x=(index*71-(int)(camera_x*.05f))%360;
        if (x<0) x+=360;
        int y=52+(index*43)%165;
        struct colour glow=(index+frame_number/30)%4==0?settings.edge:settings.platform;
        rectangle(x,y,1,1,glow);
    }
}

static void render_shard(int x, int y)
{
    int pulse=(frame_number/6)%3;
    rectangle(x+3,y+1,2,6,settings.phosphor);
    rectangle(x+2,y+2,4,4,settings.phosphor);
    rectangle(x+1,y+3,6,2,settings.paper);
    if (pulse==0) { rectangle(x,y+3,1,1,settings.edge); rectangle(x+7,y+3,1,1,settings.edge); }
}

static void render_portal(int x, int y, bool active)
{
    struct colour colour=active?settings.phosphor:settings.platform;
    int pulse=(frame_number/8)%3;
    rectangle(x-3-pulse,y-4-pulse,14+pulse*2,24+pulse*2,settings.deep);
    rectangle(x-1,y-2,10,20,colour);
    rectangle(x+1,y,6,16,settings.background);
    if (active) rectangle(x+3,y+3,2,10,settings.paper);
}

static void render_world(void)
{
    int first=(int)camera_x/TILE-1;
    int last=first+host->screen_width/TILE+3;
    for (int ty=0;ty<level.height;++ty) {
        for (int tx=first;tx<=last;++tx) {
            char tile=tile_at(tx,ty);
            int x=tx*TILE-(int)camera_x, y=ty*TILE;
            if (tile=='#') {
                rectangle(x,y,TILE,TILE,settings.platform);
                if (tile_at(tx,ty+1)!='#') rectangle(x,y+6,TILE,2,settings.edge);
                if (((tx*13+ty*7)&3)==0) rectangle(x+2,y+2,1,1,settings.amber);
            } else if (tile=='^') {
                rectangle(x,y,8,2,settings.hazard);
                rectangle(x+1,y+2,2,3,settings.hazard);
                rectangle(x+3,y+2,2,6,settings.hazard);
                rectangle(x+5,y+2,2,4,settings.hazard);
            } else if (tile=='o') render_shard(x,y);
            else if (tile=='C') {
                rectangle(x+2,y,4,8,settings.platform);
                rectangle(x,y+7,8,2,settings.amber);
                if (((frame_number/5)&1)==0) rectangle(x+3,y+10,2,2,settings.paper);
            } else if (tile=='E') render_portal(x,y,collected_shards==level.total_shards);
        }
    }
}

static void render_particles(void)
{
    for (int index=0;index<MAX_PARTICLES;++index) {
        if (particles[index].life<=0) continue;
        int x=(int)(particles[index].x-camera_x), y=(int)particles[index].y;
        rectangle(x,y,2,2,particles[index].colour);
    }
}

static void render_player(void)
{
    int base_x=(int)(player_x-camera_x), base_y=(int)player_y;
    if (dash_timer>0) {
        struct colour trail=settings.edge;
        rectangle(base_x-facing*7,base_y+4,8,6,trail);
    }
    for (int row=0;row<sprite.height;++row) {
        for (int column=0;column<sprite.width;++column) {
            int source_column=facing<0?sprite.width-1-column:column;
            char pixel=sprite.pixels[(size_t)row*sprite.width+source_column];
            if (pixel=='.') continue;
            int bob=on_ground && absolute(velocity_x)>.3f ? (frame_number/5)&1 : 0;
            rectangle(base_x+column,base_y+sprite.height-1-row+bob,1,1,sprite_colour(pixel));
        }
    }
}

static void render_hud(void)
{
    char line[64];
    snprintf(line,sizeof(line),"SIGNAL %02d/%02d",collected_shards,level.total_shards);
    rectangle(6,218,118,17,settings.background);
    text(10,230,line,1,settings.paper);
    snprintf(line,sizeof(line),"FALLS %02d",deaths);
    text(246,230,line,1,settings.amber);
    if (dash_available) rectangle(112,221,8,3,settings.edge);
}

static void render_title(void)
{
    render_background();
    int blink=(title_timer/28)&1;
    text(27,184,"PHOSPHOR",4,settings.phosphor);
    text(86,145,"RUN",5,settings.amber);
    rectangle(48,118,224,2,settings.edge);
    text(56,93,"RESTORE THE LAST SIGNAL",1,settings.paper);
    text(56,73,"MOVE  ARROWS OR A D",1,settings.edge);
    text(56,58,"JUMP  Z OR SPACE",1,settings.edge);
    text(56,43,"DASH  X OR SHIFT",1,settings.edge);
    if (blink) text(92,18,"PRESS ENTER",2,settings.paper);
}

static void render_game(void)
{
    render_background(); render_world(); render_particles();
    if (phase!=PHASE_DEAD || (death_timer&3)<2) render_player();
    render_hud();
    if (phase==PHASE_DEAD) {
        rectangle(75,102,170,32,settings.background);
        text(94,123,"SIGNAL LOST",2,settings.hazard);
    } else if (phase==PHASE_WIN) {
        rectangle(25,65,270,102,settings.background);
        text(45,145,"TRANSMISSION",3,settings.phosphor);
        text(105,114,"RESTORED",2,settings.paper);
        text(74,87,"ENTER  RUN AGAIN",1,settings.amber);
    }
}

static bool game_init(const struct two_forty_host_api *host_api, const char *config_path)
{
    host=host_api;
    if (host->abi_version!=TWO_FORTY_ABI_VERSION || !load_settings(config_path) ||
        !load_level(settings.level) || !load_sprite(settings.sprite)) return false;
    phase=PHASE_TITLE; title_timer=0; facing=1;
    respawn_x=level.initial_x; respawn_y=level.initial_y;
    player_x=respawn_x; player_y=respawn_y;
    return true;
}

static void game_shutdown(void)
{
    free(level.tiles); free(level.original); free(sprite.pixels);
    memset(&level,0,sizeof(level)); memset(&sprite,0,sizeof(sprite)); host=NULL;
}

static void game_update(const struct two_forty_input *input)
{
    frame_number++; update_particles();
    if (phase==PHASE_TITLE) {
        title_timer++;
        if (input->pressed[KEY_ENTER] || pressed_jump(input) ||
            (settings.title_frames > 0 && title_timer >= settings.title_frames)) new_run();
    } else if (phase==PHASE_PLAY) update_play(input);
    else if (phase==PHASE_DEAD) {
        if (--death_timer<=0) respawn();
    } else if (phase==PHASE_WIN) {
        win_timer++;
        if (input->pressed[KEY_ENTER]) new_run();
    }
}

static void game_render(void)
{
    if (phase==PHASE_TITLE) render_title(); else render_game();
}

static const struct two_forty_game_api api={
    .abi_version=TWO_FORTY_ABI_VERSION,.init=game_init,.shutdown=game_shutdown,
    .update=game_update,.render=game_render
};

const struct two_forty_game_api *two_forty_game_entry(void) { return &api; }
