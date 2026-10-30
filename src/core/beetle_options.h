/*
 * Mandrake - the core options Mandrake gives Beetle PSX, from the player's settings.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * One place for the keys and values, shared by src/core/host.c (which sets them before a Beetle PSX game starts and
 * while it runs) and by the host check (tests/core/beetle_headless.c), which fails when the pinned core no longer
 * offers one of them. Plain C with no other part of the app, so the check can build it alone.
 */
#ifndef MANDRAKE_BEETLE_OPTIONS_H
#define MANDRAKE_BEETLE_OPTIONS_H

#include <stdbool.h>

typedef struct
{
    const char *key;
    const char *values[12]; /* every value Mandrake may give it, NULL after the last */
} BeetleOption;

extern const BeetleOption BEETLE_OPTIONS[];
extern const int BEETLE_OPTION_COUNT;

/* What the player chose, in the frontend's terms. */
typedef struct
{
    int internal_res;       /* 0 native (320x240), 1 twice that: Settings > Graphics > Beetle PSX */
    bool dithering;         /* the PS1's dot pattern in 16-bit colour, at its own size */
    bool widescreen;        /* the widescreen hack: 3D drawn for 16:9 */
    int region;             /* 0 auto, 1 NTSC-J, 2 NTSC-U, 3 PAL (enum RegionMode) */
    int cd_speed;           /* 1..10 as Settings > System > Fast CD loading (1 off) */
    bool boot_intro;        /* the BIOS's own start-up (off: skipped, with a BIOS of the player's) */
    bool multitap;          /* 4 players through a multitap in port 1 */
    bool analog_from_start; /* the DualShock starts in analog mode (games known to want it) */
    int analog_combo;       /* the ANALOG button's combination, as SwanStation's Analog Mode Combo numbers it (0..10) */
} BeetleChoice;

/* The combination analog_combo gives Beetle PSX, as SwanStation's numbering: Beetle PSX has all but the first
 * (L1 + R1 + L3 + R3), which becomes L1 + R1 + Select, both cores' default (2). */
int beetle_analog_combo(int swanstation_combo);

/* Calls set(key, value) for every option of BEETLE_OPTIONS, with the value for this choice. */
void beetle_options_apply(const BeetleChoice *choice, void (*set)(const char *key, const char *value));

#endif
