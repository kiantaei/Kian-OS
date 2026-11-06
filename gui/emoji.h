// Kian OS GUI - colored emoji set, drawn entirely in code (Windows-style
// flat colors, no image files). They can be used standalone via
// emoji_draw() or inline inside any text string with the escape
//   '\x01' + emoji-id byte
// which draw_text/draw_text_mono and the width functions understand.
#pragma once
#include <kian_abi.h>

enum {
    EMOJI_SMILE = 1,   // yellow smiley
    EMOJI_HEART,       // red heart
    EMOJI_STAR,        // yellow star
    EMOJI_BOLT,        // yellow lightning
    EMOJI_SUN,         // sun with rays
    EMOJI_MOON,        // crescent moon
    EMOJI_CLOUD,       // white cloud
    EMOJI_FOLDER,      // amber folder
    EMOJI_GEAR,        // gray gear
    EMOJI_GLOBE,       // blue globe
    EMOJI_ROCKET,      // rocket
    EMOJI_FIRE,        // fire
    EMOJI_OK,          // green check badge
    EMOJI_WARN,        // warning triangle
    EMOJI_LOCK,        // padlock
    EMOJI_MUSIC,       // music note
    EMOJI_N            // count
};

void emoji_draw(int id, int x, int y, int size);      // size x size box
int  emoji_advance_ui(int scale);                     // inline UI text advance
int  emoji_size_ui(int scale);                        // ink size for UI text
int  emoji_advance_mono(int scale);                   // 2 mono cells
int  emoji_size_mono(int scale);                      // ink size for mono text
int  emoji_size_valid(int id);                        // 1 if id is a known emoji
