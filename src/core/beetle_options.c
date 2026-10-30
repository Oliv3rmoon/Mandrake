/*
 * Mandrake - the core options Mandrake gives Beetle PSX (beetle_options.h).
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "beetle_options.h"

#include <stddef.h>
#include <string.h>

const BeetleOption BEETLE_OPTIONS[] = {
    /* the picture: the software renderer draws at 1x or 2x; dithering at the PS1's own size, or none */
    {"beetle_psx_internal_resolution", {"1x(native)", "2x", NULL}},
    {"beetle_psx_dither_mode", {"1x(native)", "disabled", NULL}},
    {"beetle_psx_widescreen_hack", {"disabled", "enabled", NULL}},
    {"beetle_psx_widescreen_hack_aspect_ratio", {"16:9", NULL}},
    /* each frame drawn, even when the game shows the last one again: the frontend shows what it is given */
    {"beetle_psx_frame_duping", {"disabled", NULL}},
    /* the console: the region of content without one (a disc says its own), loading speed, the BIOS's start-up */
    {"beetle_psx_region", {"auto", "ntsc-j", "ntsc-u", "pal", NULL}},
    {"beetle_psx_cd_fastload", {"2x(native)", "4x", "6x", "8x", "10x", "12x", "14x", NULL}},
    {"beetle_psx_skip_bios", {"disabled", "enabled", NULL}},
    /* the disc is read ahead on a thread of the core's: a game on a network share doesn't stall the frame */
    {"beetle_psx_cd_access_method", {"async", NULL}},
    /* memory card 1 is the frontend's (SAVE_RAM), kept in saves/beetle/<serial>_1.mcd; no second card */
    {"beetle_psx_use_mednafen_memcard0_method", {"libretro", NULL}},
    {"beetle_psx_enable_memcard1", {"disabled", NULL}},
    /* controllers: the DualShock's ANALOG button by its combination, the multitap, no cursor of the core's (the
     * frontend draws the light gun's crosshair) */
    {"beetle_psx_analog_toggle", {"enabled", "enabled-analog", NULL}},
    {"beetle_psx_analog_toggle_combo",
     {"l1+l2+r1+r2+start+select", "l1+r1+select", "l1+r1+start", "l1+r1+l3", "l1+r1+r3", "l2+r2+select", "l2+r2+start",
      "l2+r2+l3", "l2+r2+r3", "l3+r3", NULL}},
    {"beetle_psx_enable_multitap_port1", {"disabled", "enabled", NULL}},
    {"beetle_psx_gun_cursor", {"off", NULL}},
    /* its messages: the DualShock's mode among them, which the light bar and Left stick as D-pad follow (host.c) */
    {"beetle_psx_display_osd", {"enabled", NULL}},
};

const int BEETLE_OPTION_COUNT = (int)(sizeof(BEETLE_OPTIONS) / sizeof(BEETLE_OPTIONS[0]));

/* SwanStation's Analog Mode Combo, 0..10, in Beetle PSX's words */
static const char *const COMBOS[] = {"l1+r1+select", "l1+l2+r1+r2+start+select", "l1+r1+select", "l1+r1+start",
                                     "l1+r1+l3", "l1+r1+r3", "l2+r2+select", "l2+r2+start", "l2+r2+l3",
                                     "l2+r2+r3", "l3+r3"};

int beetle_analog_combo(int swanstation_combo)
{
    return swanstation_combo >= 1 && swanstation_combo <= 10 ? swanstation_combo : 2;
}

void beetle_options_apply(const BeetleChoice *c, void (*set)(const char *key, const char *value))
{
    static const char *const regions[] = {"auto", "ntsc-j", "ntsc-u", "pal"};
    /* Fast CD loading 1 (off) to 10 over the PS1's 2x drive: 2x, 4x... up to the core's 14x */
    static const char *const speeds[] = {"2x(native)", "4x", "6x", "8x", "10x", "12x", "14x"};
    int speed = c->cd_speed >= 1 ? c->cd_speed - 1 : 0;
    if (speed > 6)
        speed = 6;
    set("beetle_psx_internal_resolution", c->internal_res >= 1 ? "2x" : "1x(native)");
    set("beetle_psx_dither_mode", c->dithering ? "1x(native)" : "disabled");
    set("beetle_psx_widescreen_hack", c->widescreen ? "enabled" : "disabled");
    set("beetle_psx_widescreen_hack_aspect_ratio", "16:9");
    set("beetle_psx_frame_duping", "disabled");
    set("beetle_psx_region", regions[c->region >= 0 && c->region < 4 ? c->region : 0]);
    set("beetle_psx_cd_fastload", speeds[speed]);
    set("beetle_psx_skip_bios", c->boot_intro ? "disabled" : "enabled");
    set("beetle_psx_cd_access_method", "async");
    set("beetle_psx_use_mednafen_memcard0_method", "libretro");
    set("beetle_psx_enable_memcard1", "disabled");
    set("beetle_psx_analog_toggle", c->analog_from_start ? "enabled-analog" : "enabled");
    set("beetle_psx_analog_toggle_combo", COMBOS[beetle_analog_combo(c->analog_combo)]);
    set("beetle_psx_enable_multitap_port1", c->multitap ? "enabled" : "disabled");
    set("beetle_psx_gun_cursor", "off");
    set("beetle_psx_display_osd", "enabled");
}

int beetle_analog_message(const char *message, int *port)
{
    /* "DualShock on port 1: Analog Mode" (mednafen/psx/frontio.c), "...: Digital Mode" */
    static const char prefix[] = "DualShock on port ";
    if (strncmp(message, prefix, sizeof(prefix) - 1) != 0)
        return -1;
    const char *p = message + sizeof(prefix) - 1;
    int n = 0, digits = 0;
    for (; *p >= '0' && *p <= '9' && digits < 2; ++p, ++digits)
        n = n * 10 + (*p - '0');
    if (!digits)
        return -1;
    *port = n;
    if (!strcmp(p, ": Analog Mode"))
        return 1;
    if (!strcmp(p, ": Digital Mode"))
        return 0;
    return -1;
}
