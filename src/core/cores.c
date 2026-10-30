/*
 * Mandrake - the cores' table (cores.h).
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "cores.h"

#include "../SwanStationPS5.h"

#include <stdio.h>
#include <string.h>

/* The libretro API of the core whose archive renamed it <prefix>retro_*. */
#define CORE_DECLARE(p)                                                                                            \
    void p##retro_set_environment(retro_environment_t);                                                            \
    void p##retro_set_video_refresh(retro_video_refresh_t);                                                        \
    void p##retro_set_audio_sample(retro_audio_sample_t);                                                          \
    void p##retro_set_audio_sample_batch(retro_audio_sample_batch_t);                                              \
    void p##retro_set_input_poll(retro_input_poll_t);                                                              \
    void p##retro_set_input_state(retro_input_state_t);                                                            \
    void p##retro_init(void);                                                                                      \
    void p##retro_deinit(void);                                                                                    \
    void p##retro_get_system_info(struct retro_system_info *);                                                     \
    bool p##retro_load_game(const struct retro_game_info *);                                                       \
    void p##retro_unload_game(void);                                                                               \
    void p##retro_get_system_av_info(struct retro_system_av_info *);                                               \
    void p##retro_set_controller_port_device(unsigned, unsigned);                                                  \
    void p##retro_run(void);                                                                                       \
    void p##retro_reset(void);                                                                                     \
    size_t p##retro_serialize_size(void);                                                                          \
    bool p##retro_serialize(void *, size_t);                                                                       \
    bool p##retro_unserialize(const void *, size_t);                                                               \
    void *p##retro_get_memory_data(unsigned);                                                                      \
    size_t p##retro_get_memory_size(unsigned);                                                                     \
    void p##retro_cheat_reset(void);                                                                               \
    void p##retro_cheat_set(unsigned, bool, const char *);

#define CORE_FUNCTIONS(p)                                                                                          \
    p##retro_set_environment, p##retro_set_video_refresh, p##retro_set_audio_sample,                               \
        p##retro_set_audio_sample_batch, p##retro_set_input_poll, p##retro_set_input_state, p##retro_init,         \
        p##retro_deinit, p##retro_get_system_info, p##retro_load_game, p##retro_unload_game,                       \
        p##retro_get_system_av_info, p##retro_set_controller_port_device, p##retro_run, p##retro_reset,            \
        p##retro_serialize_size, p##retro_serialize, p##retro_unserialize, p##retro_get_memory_data,               \
        p##retro_get_memory_size, p##retro_cheat_reset, p##retro_cheat_set

/* tools/build-swanstation.sh and tools/build-beetle.sh */
CORE_DECLARE(swanstation_)
CORE_DECLARE(beetle_)

const CoreInfo CORES[CORE_COUNT] = {
    [CORE_SWANSTATION] = {"swanstation", "SwanStation", "GPL-3.0", "", true, true, CORE_FUNCTIONS(swanstation_)},
    /* its saves and states in a folder of their own; it needs a disc to start (no BIOS-only boot) */
    [CORE_BEETLE] = {"beetle", "Beetle PSX", "GPL-2.0-or-later", "beetle", false, false, CORE_FUNCTIONS(beetle_)},
};

const CoreInfo *core_by_id(const char *id)
{
    for (int i = 0; i < CORE_COUNT; ++i)
        if (id && !strcmp(CORES[i].id, id))
            return &CORES[i];
    return NULL;
}

int core_index(int value)
{
    return value >= 0 && value < CORE_COUNT ? value : CORE_SWANSTATION;
}

static void core_dir(int core, const char *base, char *out, size_t size)
{
    const char *folder = CORES[core_index(core)].folder;
    if (folder[0])
        path_join(out, size, base, folder);
    else
        str_copy(out, size, base);
}

void core_states_dir(int core, const char *states, char *out, size_t size)
{
    core_dir(core, states, out, size);
}

void core_saves_dir(int core, const char *saves, char *out, size_t size)
{
    core_dir(core, saves, out, size);
}
