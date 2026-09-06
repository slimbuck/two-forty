#include "input_bindings.h"
#include <stdio.h>
#include <string.h>

void default_bindings(struct controller_binding *bindings)
{
    bindings[TWO_FORTY_ACTION_LEFT]=(struct controller_binding){BINDING_ABS,ABS_HAT0X,-1};
    bindings[TWO_FORTY_ACTION_RIGHT]=(struct controller_binding){BINDING_ABS,ABS_HAT0X,1};
    bindings[TWO_FORTY_ACTION_UP]=(struct controller_binding){BINDING_ABS,ABS_HAT0Y,-1};
    bindings[TWO_FORTY_ACTION_DOWN]=(struct controller_binding){BINDING_ABS,ABS_HAT0Y,1};
    bindings[TWO_FORTY_ACTION_JUMP]=(struct controller_binding){BINDING_KEY,BTN_SOUTH,0};
    bindings[TWO_FORTY_ACTION_DASH]=(struct controller_binding){BINDING_KEY,BTN_EAST,0};
    bindings[TWO_FORTY_ACTION_CONFIRM]=(struct controller_binding){BINDING_KEY,BTN_EAST,0};
    bindings[TWO_FORTY_ACTION_MENU]=(struct controller_binding){BINDING_KEY,BTN_TL2,0};
}

void default_keyboard_bindings(struct controller_binding *bindings)
{
    const unsigned int codes[]={KEY_LEFT,KEY_RIGHT,KEY_UP,KEY_DOWN,KEY_Z,KEY_X,KEY_ENTER,KEY_ESC};
    for (int i=0;i<TWO_FORTY_ACTION_COUNT;i++) bindings[i]=(struct controller_binding){BINDING_KEY,codes[i],0};
}

bool parse_binding(const char *text, struct controller_binding *binding)
{
    unsigned int code=0; int direction=0; char extra;
    if (sscanf(text,"key:%u%c",&code,&extra)==1 && code<=KEY_MAX) {
        *binding=(struct controller_binding){BINDING_KEY,code,0}; return true;
    }
    if (sscanf(text,"abs:%u:%d%c",&code,&direction,&extra)==2 && code<=ABS_MAX && (direction==-1 || direction==1)) {
        *binding=(struct controller_binding){BINDING_ABS,code,direction}; return true;
    }
    return false;
}

void setup_begin(struct binding_setup *setup, bool keyboard)
{
    *setup=(struct binding_setup){.active=true,.keyboard=keyboard,.wait_release=true,.message=""};
}

void setup_offer(struct binding_setup *setup, struct controller_binding binding)
{
    if (!setup->active || setup->wait_release || setup->ready || setup->complete) return;
    if (setup->keyboard && (binding.kind!=BINDING_KEY || binding.code>=BTN_MISC || binding.code==KEY_F1 || binding.code==KEY_F12)) return;
    setup->candidate=binding; setup->ready=true; setup->wait_release=true;
}

void setup_release(struct binding_setup *setup, bool released)
{
    if (!setup->active || !setup->wait_release || !released) return;
    setup->wait_release=false;
    if (!setup->ready) return;
    setup->ready=false;
    for (int i=0;i<setup->step;i++) {
        const struct controller_binding *old=&setup->pending[i], *next=&setup->candidate;
        /* Confirm is a menu action; sharing a gameplay face button is useful. */
        if (setup->step==TWO_FORTY_ACTION_CONFIRM && (i==TWO_FORTY_ACTION_JUMP || i==TWO_FORTY_ACTION_DASH)) continue;
        if (old->kind==next->kind && old->code==next->code && old->direction==next->direction) {
            setup->message="ALREADY USED - TRY ANOTHER"; return;
        }
    }
    setup->pending[setup->step++]=setup->candidate; setup->message="";
    if (setup->step==TWO_FORTY_ACTION_COUNT) setup->complete=true;
}
