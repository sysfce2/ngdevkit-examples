/*
 * Copyright (c) 2020 Damien Ciabrini
 * This file is part of ngdevkit-examples
 *
 * ngdevkit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * ngdevkit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with ngdevkit.  If not, see <http://www.gnu.org/licenses/>.
 */

// Additional resources for sound
// https://wiki.neogeodev.org/index.php?title=YM2610_registers

#include <ngdevkit/neogeo.h>
#include <ngdevkit/bios-ram.h>
#include <ngdevkit/ng-fix.h>
#include <ngdevkit/ng-video.h>
#include <stdio.h>
#include <stdbool.h>

// The sound commands for this ROM, as generated by soundtool
// ---
// Note: nullsound allows you to add as many custom commands
//   as you want before the commands that play back music or sfx.
// TODO generate macro for the z80 driver automatically
#define CMD_EYE_CATCHER 2
#define CMD_RESET_DRIVER 3
#define STREAM_STOP 4
#define STREAM_VOLUME_DOWN 5
#define STREAM_VOLUME_UP 6
#define VOLUME_FADE_OUT 7
//   For this ROM, we use custom commands, so the first play back
//   command is command 8.
//   Configure it below before including the command header.
#define _SND_CMD_BASE_GENERATED 8
#include "snd_commands.h"
#define MUSIC_CMD_START (_SND_CMD_BASE_GENERATED)
#define MUSIC_CMD_END (_SND_CMD_BASE_GENERATED+SND_NB_MUSIC-1)
#define SFX_CMD_START (_SND_CMD_BASE_GENERATED+SND_NB_MUSIC)
#define SFX_CMD_END (_SND_CMD_BASE_GENERATED+SND_NB_MUSIC+SND_NB_FX-1)

static const char *cmdinfo[] = {
    // this is just a generated header based on sound assets that are
    // currently used in this demo
    #include "snd_commands_info.h"
};

// A variable in the m68k address space that mirrors the current
// configuration of nullsound/ym2610 for music volume.
u8 music_vol;


/// controller's current state, and change on button press (not release)
extern u8 bios_p1current;
extern u8 bios_p1change;
#define A_PRESSED 0x10
#define B_PRESSED 0x20
#define C_PRESSED 0x40


/// z80 sound driver communication
#define REG_SOUND ((volatile u8*)0x320000)

const u16 clut[][16]= {
    /// first 16 colors palette for the fix tiles
    {0x8000, 0x0fff, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x8000, 0x0fff, 0x0a40, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x8000, 0x0fff, 0x004a, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x8000, 0x0888, 0x0444, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x8000, 0x0f00, 0x0800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
     0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}

};

void init_palette() {
    /// Initialize the two palettes in the first palette bank
    u16 *p=(u16*)clut;
    for (u16 i=0;i<sizeof(clut)/2; i++) {
        MMAP_PALBANK1[i]=p[i];
    }
    *((volatile u16*)0x401ffe)=0x0333;
}


typedef struct _selection {
    char label[8];
    u8 y;
    u8 val;
    u8 valmin;
    u8 valmax;
} selection_t;

selection_t selections[] = {
    // musics
    {"MUSIC", 15, MUSIC_CMD_START, MUSIC_CMD_START, MUSIC_CMD_END},
    {"level", 17, 15, 0, 15},
    // sfx
    {"SFX", 20, SFX_CMD_START, SFX_CMD_START, SFX_CMD_END},
    // nullsound
    {"COMMAND", 24, 3, 1, _SND_CMD_BASE_GENERATED-1}
};


void display_selection(selection_t *c, bool active) {
    char str[16];
    u8 pal;
    u8 descpal;

    if (!active) {
        pal = 3;
        descpal = 3;
    } else {
        pal = (bios_p1current&A_PRESSED)?1:2;
        switch (c->val) {
        case 1:
        case 3:
            descpal=1;
            break;
        case 2:
            descpal=3;
            break;
        }
    }

    bool sel_level = c->label[0] == 'l';
    sprintf(str, "%c %s: %02X %c", active?'<':' ',
            c->label, sel_level?music_vol:c->val, active?'>':' ');
    ng_center_text(c->y, pal, str);
    if (!sel_level) {
        ng_center_text(c->y+1, descpal, "                                     ");
        ng_center_text(c->y+1, descpal, cmdinfo[c->val]);
    }
}


void process_input_for_selection(selection_t *c, bool active) {
    bool sel_level = c->label[0] == 'l';
    if (active) {
        if (sel_level) {
            if (bios_p1change & (CNT_LEFT|CNT_RIGHT)) {
                if (c->val > music_vol) *REG_SOUND = STREAM_VOLUME_UP;
                if (c->val < music_vol) *REG_SOUND = STREAM_VOLUME_DOWN;
                music_vol = c->val;
            }
        } else if (bios_p1change & CNT_A) {
            *REG_SOUND=c->val;
            // For the demo, keep the music level up to date
            // if cmds have been executed directly
            switch(c->val) {
            case STREAM_VOLUME_DOWN:
                music_vol--;
                break;
            case STREAM_VOLUME_UP:
                music_vol++;
                break;
            case CMD_EYE_CATCHER:
            case CMD_RESET_DRIVER:
                music_vol = 15;
                if (sel_level) c->val = 15;
            }
        }
    }
 }


int main(void) {
    u8 cmd=5;
    char strcmd[3];

    u8 inc = 1;

    // start from loudest volume for music
    music_vol = 15;

    // Command 3: reset z80 sound driver
    *REG_SOUND = CMD_RESET_DRIVER;

    ng_cls();
    init_palette();

#define TOP 2
    ng_center_text_tall(TOP+4, 0, "Z80 DRIVER SOUND TEST");
    ng_center_text(TOP+7, 0, "up/down: select section");
    ng_center_text(TOP+8, 0, "left/right: section value");
    ng_center_text(TOP+9, 0, "button A: play music");

    int sel = 0;

    for(;;) {
        if ((bios_p1change & CNT_UP) && sel>0) sel-=1;
        else if ((bios_p1change & CNT_DOWN) && sel<3) sel+=1;

        for (int i=0;i<4;i++) {
            selection_t *c = &selections[i];
            bool selected = sel==i;

            if (selected) {
                if ((bios_p1change & CNT_LEFT) && (c->val > c->valmin)) c->val--;
                else if ((bios_p1change & CNT_RIGHT) && (c->val < c->valmax)) c->val++;
            }

            display_selection(c, selected);
            process_input_for_selection(c, selected);
        }

        ng_wait_vblank();
    }
    return 0;
}
