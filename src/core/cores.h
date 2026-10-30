/*
 * Mandrake - the PlayStation emulators linked into the app, one table entry each.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Each core is a static archive built by tools/build-<core>.sh with its libretro API renamed <id>_retro_*, so both
 * live in one eboot.bin without their symbols meeting. The frontend calls a core only through its CoreInfo; host.c
 * picks the entry for the game (Settings.core: Settings > System > Emulator, for every game or for one).
 *
 * What differs between them, for the rest of the app:
 *   - their save states and memory cards are kept apart (core_states_dir, core_saves_dir): SwanStation's where they
 *     always were, Beetle PSX's in a folder of its own, so a state of one is never loaded into the other;
 *   - SwanStation renders on the GPU (Vulkan) and hands the screen its image. Beetle PSX is built with its software
 *     renderer and hands the frontend a picture in memory, which the screen uploads into an image of its own
 *     (vkp_set_game_pixels): the shaders, FSR, CAS, generated frames and supersampling work on both;
 *   - SwanStation's own core options (swanstation_options.h) are its alone; Beetle PSX's are in beetle_options.h.
 */
#ifndef MANDRAKE_CORES_H
#define MANDRAKE_CORES_H

#include "libretro.h"

#include <stdbool.h>
#include <stddef.h>

enum
{
    CORE_SWANSTATION = 0, /* the default: libretro's DuckStation fork, Vulkan renderer, recompiler */
    CORE_BEETLE,          /* Beetle PSX: Mednafen's PlayStation, software renderer, interpreter */
    CORE_COUNT
};

typedef struct
{
    const char *id;      /* "swanstation": the archive's prefix, the log's name for it */
    const char *name;    /* "SwanStation": what the interface shows */
    const char *license; /* for the notices */
    const char *folder;  /* the folder of its save states and memory cards under states/ and saves/ ("" the top) */
    bool gpu;            /* renders through Vulkan: its picture is a Vulkan image of its own */
    bool boots_bios;     /* starts the console's BIOS with no disc (Settings > System > Start BIOS) */
    bool tells_repeats;  /* says when a frame shows the last one's picture (video_refresh with no data); else the host
                            compares the pictures (cpu_frame_repeats) */
    /* the libretro API, as tools/build-<id>.sh renamed it */
    void (*set_environment)(retro_environment_t);
    void (*set_video_refresh)(retro_video_refresh_t);
    void (*set_audio_sample)(retro_audio_sample_t);
    void (*set_audio_sample_batch)(retro_audio_sample_batch_t);
    void (*set_input_poll)(retro_input_poll_t);
    void (*set_input_state)(retro_input_state_t);
    void (*init)(void);
    void (*deinit)(void);
    void (*get_system_info)(struct retro_system_info *);
    bool (*load_game)(const struct retro_game_info *);
    void (*unload_game)(void);
    void (*get_system_av_info)(struct retro_system_av_info *);
    void (*set_controller_port_device)(unsigned, unsigned);
    void (*run)(void);
    void (*reset)(void);
    size_t (*serialize_size)(void);
    bool (*serialize)(void *, size_t);
    bool (*unserialize)(const void *, size_t);
    void *(*get_memory_data)(unsigned);
    size_t (*get_memory_size)(unsigned);
    void (*cheat_reset)(void);
    void (*cheat_set)(unsigned, bool, const char *);
} CoreInfo;

extern const CoreInfo CORES[CORE_COUNT];

/* The core with this id ("beetle"), or NULL. */
const CoreInfo *core_by_id(const char *id);
/* A Settings.core value read from a file, made valid: SwanStation for anything that is not a core. */
int core_index(int value);
/* <states>/<folder> and <saves>/<folder> of the core: where its save states and its memory cards are kept. */
void core_states_dir(int core, const char *states, char *out, size_t size);
void core_saves_dir(int core, const char *saves, char *out, size_t size);

#endif
