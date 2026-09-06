/* Exercise host input and layout without DRM, a controller, or the live Pi. */
#define main unused_host_main
#include "../src/host.c"
#undef main
#include <assert.h>

static unsigned char framebuffer[240][320][3], clear_colour[3];
static int scissor_x,scissor_y,scissor_w,scissor_h;
static bool scissor;
void glDisable(GLenum cap) { (void)cap; scissor=false; }
void glEnable(GLenum cap) { (void)cap; scissor=true; }
void glClearColor(GLfloat r,GLfloat g,GLfloat b,GLfloat a)
{ (void)a;clear_colour[0]=(unsigned char)(r*255);clear_colour[1]=(unsigned char)(g*255);clear_colour[2]=(unsigned char)(b*255); }
void glScissor(GLint x,GLint y,GLsizei w,GLsizei h)
{ scissor_x=x;scissor_y=y;scissor_w=w;scissor_h=h; }
void glClear(GLbitfield mask)
{
    (void)mask;
    int left=scissor?scissor_x:0,bottom=scissor?scissor_y:0;
    int right=scissor?left+scissor_w:320,top=scissor?bottom+scissor_h:240;
    assert(left>=0 && bottom>=0 && right<=320 && top<=240);
    for(int y=bottom;y<top;y++) for(int x=left;x<right;x++) memcpy(framebuffer[y][x],clear_colour,3);
}
void glReadPixels(GLint x,GLint y,GLsizei w,GLsizei h,GLenum format,GLenum type,void *pixels)
{ (void)x;(void)y;(void)w;(void)h;(void)format;(void)type;(void)pixels; }

static void event(struct host *host,int device,int type,int code,int value)
{
    struct input_event input={.type=(unsigned short)type,.code=(unsigned short)code,.value=value};
    process_input_event(host,&host->inputs.devices[device],&input);
}
static void tap(struct host *host,int device,int code)
{ event(host,device,EV_KEY,code,1);event(host,device,EV_KEY,code,0);update_host(host); }
static void write_preview(const char *path)
{
    FILE *file=fopen(path,"wb");assert(file);fputs("P6\n320 240\n255\n",file);
    for(int y=239;y>=0;y--) fwrite(framebuffer[y],1,sizeof(framebuffer[y]),file);
    fclose(file);
}
static unsigned int game_updates;
static void fake_update(const struct two_forty_input *input) { (void)input;game_updates++; }
static void fake_shutdown(void) {}

int main(int argc,char **argv)
{
    assert(argc==2);
    char directory[]="/tmp/two-forty-host-test-XXXXXX";assert(mkdtemp(directory));assert(chdir(directory)==0);
    assert(mkdir("config",0700)==0);
    FILE *config=fopen(HOST_CONFIG_PATH,"w");assert(config);
    fputs("boot_game=launcher\n# Keep this comment\nbind_confirm=key:313\n",config);fclose(config);
    static struct host host;
    host.mode.hdisplay=320;host.mode.vdisplay=240;
    load_host_config(&host);update_safe_area(&host);
    assert(host.bindings[TWO_FORTY_ACTION_CONFIRM].code==BTN_EAST);
    assert(host.api.screen_width==288 && host.api.screen_height==216);
    host.inputs.count=2;host.inputs.devices[0].controller=true;
    strcpy(host.inputs.devices[0].name,"GP2040");
    for(int axis=ABS_HAT0X;axis<=ABS_HAT0Y;axis++) {
        host.inputs.devices[0].abs_minimums[axis]=-1;host.inputs.devices[0].abs_maximums[axis]=1;
    }
    clear_screen();fill_rect(&host,-10,-10,400,400,255,255,255);
    assert(scissor_x==16 && scissor_y==12 && scissor_w==288 && scissor_h==216);
    /* Every launcher item uses exactly the same confirmation policy. */
    tap(&host,0,BTN_TR2);assert(!host.controller_settings);
    tap(&host,0,BTN_SOUTH);assert(!host.controller_settings);
    tap(&host,0,BTN_EAST);assert(host.controller_settings);
    tap(&host,0,BTN_EAST);assert(host.setup.active && !host.setup.keyboard);
    update_host(&host);assert(!host.setup.wait_release);
    /* D-pad must return to neutral, and another event cannot overwrite capture. */
    event(&host,0,EV_ABS,ABS_HAT0X,-1);event(&host,0,EV_KEY,BTN_SOUTH,1);update_host(&host);
    assert(host.setup.step==0 && host.setup.candidate.code==ABS_HAT0X);
    event(&host,0,EV_KEY,BTN_SOUTH,0);update_host(&host);assert(host.setup.step==0);
    event(&host,0,EV_ABS,ABS_HAT0X,0);update_host(&host);assert(host.setup.step==1);
    event(&host,0,EV_ABS,ABS_HAT0X,1);update_host(&host);
    event(&host,0,EV_ABS,ABS_HAT0X,0);update_host(&host);
    event(&host,0,EV_ABS,ABS_HAT0Y,-1);event(&host,0,EV_ABS,ABS_HAT0Y,0);update_host(&host);
    event(&host,0,EV_ABS,ABS_HAT0Y,1);event(&host,0,EV_ABS,ABS_HAT0Y,0);update_host(&host);
    assert(host.setup.step==4);
    tap(&host,0,BTN_SOUTH);assert(host.setup.step==5);
    tap(&host,0,BTN_SOUTH);assert(host.setup.step==5 && *host.setup.message);
    tap(&host,0,BTN_EAST);assert(host.setup.step==6);
    tap(&host,0,BTN_TR2);assert(host.setup.step==7 && host.setup.active);
    tap(&host,0,BTN_TL2);assert(!host.setup.active && host.controller_settings);
    assert(host.bindings[TWO_FORTY_ACTION_CONFIRM].code==BTN_TR2);
    update_host(&host);
    /* Keyboard setup captures Escape as a mapping; only F1 cancels. */
    host.selected_option=1;tap(&host,1,KEY_ENTER);update_host(&host);
    assert(host.setup.active && host.setup.keyboard);
    const int keyboard[]={KEY_A,KEY_D,KEY_W,KEY_S,KEY_R,KEY_X,KEY_SPACE,KEY_ESC};
    for(int i=0;i<TWO_FORTY_ACTION_COUNT;i++) tap(&host,1,keyboard[i]);
    assert(!host.setup.active && host.keyboard_bindings[TWO_FORTY_ACTION_JUMP].code==KEY_R);
    update_host(&host);
    /* Keyboard and controller states are independent; quick taps survive polling. */
    event(&host,1,EV_KEY,KEY_R,1);event(&host,1,EV_KEY,KEY_R,0);update_controller_actions(&host);
    assert(host.inputs.state.action_pressed[TWO_FORTY_ACTION_JUMP]);
    event(&host,0,EV_KEY,KEY_ESC,1);assert(!host.inputs.state.pressed[KEY_ESC]);event(&host,0,EV_KEY,KEY_ESC,0);
    update_host(&host);
    tap(&host,1,KEY_ENTER);update_host(&host);tap(&host,1,KEY_Q);tap(&host,1,KEY_F1);
    assert(!host.setup.active && host.keyboard_bindings[TWO_FORTY_ACTION_LEFT].code==KEY_A);
    update_host(&host);
    struct host reloaded={0};load_host_config(&reloaded);
    assert(reloaded.keyboard_bindings[TWO_FORTY_ACTION_JUMP].code==KEY_R);
    assert(reloaded.bindings[TWO_FORTY_ACTION_CONFIRM].code==BTN_TR2);
    /* A controller-only user can cancel without consuming Start/Select as back. */
    setup_begin(&host.setup,false);update_host(&host);
    event(&host,0,EV_KEY,BTN_EAST,1);event(&host,0,EV_KEY,BTN_SOUTH,1);
    for(int i=0;i<60;i++) update_host(&host);
    assert(!host.setup.active && host.bindings[TWO_FORTY_ACTION_CONFIRM].code==BTN_TR2);
    event(&host,0,EV_KEY,BTN_EAST,0);event(&host,0,EV_KEY,BTN_SOUTH,0);update_host(&host);
    /* Failed persistence must not change the live mappings. */
    assert(rename(HOST_CONFIG_PATH,"config/saved.conf")==0);assert(mkdir(HOST_CONFIG_PATH,0700)==0);
    setup_begin(&host.setup,true);host.setup.complete=true;
    default_keyboard_bindings(host.setup.pending);update_host(&host);
    assert(!host.setup.active && host.keyboard_bindings[TWO_FORTY_ACTION_JUMP].code==KEY_R);
    assert(rmdir(HOST_CONFIG_PATH)==0);assert(rename("config/saved.conf",HOST_CONFIG_PATH)==0);update_host(&host);
    host.controller_settings=false;host.selected_game=1;
    tap(&host,0,BTN_TR2);assert(host.display_settings);
    tap(&host,1,KEY_D);assert(host.safe_x==17);
    tap(&host,1,KEY_ESC);assert(!host.display_settings && host.safe_x==16);
    tap(&host,0,BTN_TR2);tap(&host,1,KEY_D);tap(&host,1,KEY_S);tap(&host,1,KEY_D);
    assert(host.safe_x==17 && host.safe_y==13);
    tap(&host,1,KEY_S);tap(&host,0,BTN_TR2);assert(!host.display_settings);
    load_host_config(&reloaded);assert(reloaded.safe_x==17 && reloaded.safe_y==13);
    /* Gameplay chord is Start+Select, not ordinary jump+dash. */
    const struct two_forty_game_api fake={.update=fake_update,.shutdown=fake_shutdown};
    host.active_game=&host.games[0];host.game_api=&fake;
    event(&host,0,EV_KEY,BTN_SOUTH,1);event(&host,0,EV_KEY,BTN_EAST,1);
    for(int i=0;i<65;i++) update_host(&host);
    assert(host.active_game && game_updates==65);
    event(&host,0,EV_KEY,BTN_SOUTH,0);event(&host,0,EV_KEY,BTN_EAST,0);
    host.active_game=NULL;host.game_api=NULL;
    /* Render fixtures at the maximum margins as well as the default. */
    host.safe_x=32;host.safe_y=24;update_safe_area(&host);host.game_count=6;host.selected_game=8;
    for(int i=0;i<6;i++) snprintf(host.games[i].name,sizeof(host.games[i].name),"GAME %d",i+1);
    draw_launcher(&host);char output[512];snprintf(output,sizeof(output),"%s/launcher.ppm",argv[1]);write_preview(output);
    setup_begin(&host.setup,false);host.setup.step=6;host.setup.wait_release=false;
    memcpy(host.setup.pending,host.bindings,sizeof(host.bindings));
    draw_controller_settings(&host);snprintf(output,sizeof(output),"%s/button-setup.ppm",argv[1]);write_preview(output);
    host.settings_message="";
    draw_display_settings(&host);snprintf(output,sizeof(output),"%s/display-area.ppm",argv[1]);write_preview(output);
    assert(remove(HOST_CONFIG_PATH)==0);assert(rmdir("config")==0);
    assert(remove("run/status.json")==0);assert(rmdir("run")==0);
    assert(chdir("/tmp")==0);assert(rmdir(directory)==0);
    puts("Host: safe viewport, consistent confirm, controller/keyboard setup, release gating, cancellation and persistence passed.");
}
