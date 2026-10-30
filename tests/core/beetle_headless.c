/*
 * Mandrake - Beetle PSX's libretro core, headless: a frontend with no screen and no sound card that starts a disc,
 * runs it for some frames and reports what came out. tests/test_beetle.py runs it on the test disc of
 * tools/make-test-disc.py; it fails unless the disc boots and draws its picture, as the app would see it.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   beetle-headless <disc .cue> <system folder> <save folder> [frames] [2x]
 *
 * The system folder is where the core looks for a BIOS (the app's BIOS folder); with none in it the core starts
 * the disc with its built-in OpenBIOS. The options are the ones the app sets (src/core/beetle_options.c), at the
 * app's defaults, or at 2x internal resolution with "2x". Besides the picture it checks what the app relies on: every
 * option the app sets is one the core offers, the memory card is the frontend's, a save state taken and loaded again
 * replays the same frames (run-ahead and rewind depend on it), and a second game starts in the same process.
 */
#include "beetle_options.h"
#include "libretro.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- the core's options */

#define MAX_VARS 256
static struct
{
    char key[96];
    char values[1024]; /* "a|b|c" */
    char chosen[96];
} vars[MAX_VARS];
static int var_count;
static bool options_changed;

static int find_var(const char *key)
{
    for (int i = 0; i < var_count; ++i)
        if (!strcmp(vars[i].key, key))
            return i;
    return -1;
}

/* Legacy variables look like "Description; default|other|..." (the app asks for them the same way). */
static void register_variables(const struct retro_variable *v)
{
    for (; v && v->key && var_count < MAX_VARS; ++v)
    {
        if (find_var(v->key) >= 0)
            continue;
        const char *semi = strchr(v->value, ';');
        const char *list = semi ? semi + 1 : v->value;
        while (*list == ' ')
            ++list;
        snprintf(vars[var_count].key, sizeof(vars[0].key), "%s", v->key);
        snprintf(vars[var_count].values, sizeof(vars[0].values), "%s", list);
        size_t n = strcspn(list, "|");
        snprintf(vars[var_count].chosen, sizeof(vars[0].chosen), "%.*s", (int)n, list);
        ++var_count;
    }
}

static bool offers(int i, const char *value)
{
    const char *p = vars[i].values;
    size_t n = strlen(value);
    while (*p)
    {
        size_t len = strcspn(p, "|");
        if (len == n && !strncmp(p, value, n))
            return true;
        p += len + (p[len] == '|');
    }
    return false;
}

static int option_problems;

static void set_option(const char *key, const char *value)
{
    int i = find_var(key);
    if (i < 0)
    {
        printf("FAIL: the core has no option %s\n", key);
        ++option_problems;
        return;
    }
    if (!offers(i, value))
    {
        printf("FAIL: the core's option %s has no value %s (it offers %s)\n", key, value, vars[i].values);
        ++option_problems;
        return;
    }
    if (strcmp(vars[i].chosen, value))
    {
        snprintf(vars[i].chosen, sizeof(vars[0].chosen), "%s", value);
        options_changed = true;
    }
}

/* ---------------------------------------------------------------- the frontend */

static const char *system_dir, *save_dir;
static enum retro_pixel_format pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;
static struct retro_system_av_info av;
static bool disk_interface, memory_maps;
static unsigned frames_drawn, frames_null, last_w, last_h;
static uint64_t last_hash;
static uint64_t audio_frames;
static int verbose;
static const char *shot_path; /* BEETLE_HEADLESS_SHOT: the last picture, as a PPM */

static void log_printf(enum retro_log_level level, const char *fmt, ...)
{
    char line[1024];
    va_list va;
    va_start(va, fmt);
    vsnprintf(line, sizeof(line), fmt, va);
    va_end(va);
    /* what it says about the BIOS goes to the report; the rest only when asked for, or when it is an error */
    if (strstr(line, "irmware") && !strstr(line, "Searching") && !strstr(line, "Checking"))
        printf("bios: %s", line);
    else if (level >= RETRO_LOG_ERROR || verbose)
        fprintf(stderr, "core: %s", line);
}

static bool RETRO_CALLCONV rumble(unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
    (void)port, (void)effect, (void)strength;
    return true;
}

static bool environment(unsigned cmd, void *data)
{
    switch (cmd)
    {
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *(unsigned *)data = 0; /* as the app: the legacy variables */
        return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
        register_variables(data);
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE:
    {
        struct retro_variable *v = data;
        int i = find_var(v->key);
        v->value = i >= 0 ? vars[i].chosen : NULL;
        return i >= 0;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *(bool *)data = options_changed;
        options_changed = false;
        return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    {
        enum retro_pixel_format f = *(const enum retro_pixel_format *)data;
        if (f > RETRO_PIXEL_FORMAT_RGB565)
            return false;
        pixel_format = f;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *(const char **)data = system_dir;
        return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)data = save_dir;
        return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        ((struct retro_log_callback *)data)->log = log_printf;
        return true;
    case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
        ((struct retro_rumble_interface *)data)->set_rumble_state = rumble;
        return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *(bool *)data = true;
        return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
        return true;
    case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION:
        *(unsigned *)data = 1;
        return true;
    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
        disk_interface = true;
        return true;
    case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
        if (data) /* as the app: the core gives its real state size */
            *(uint64_t *)data |= RETRO_SERIALIZATION_QUIRK_FRONT_VARIABLE_SIZE;
        return true;
    case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION:
        *(unsigned *)data = 1;
        return true;
    case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
        if (verbose)
            fprintf(stderr, "message: %s\n", ((const struct retro_message_ext *)data)->msg);
        return true;
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
        memory_maps = ((const struct retro_memory_map *)data)->num_descriptors > 0;
        return true;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        av = *(const struct retro_system_av_info *)data;
        return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
        av.geometry = *(const struct retro_game_geometry *)data;
        return true;
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
    case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
        return true;
    default:
        return false; /* as the app: the VFS, the HW renderers, the perf interface... the core does without */
    }
}

/* ---------------------------------------------------------------- the picture */

/* What the test program draws: one of four colours over the whole picture, and a white 32x32 square. */
static int colour_class(uint32_t c)
{
    const int r = (int)(c >> 16 & 0xff), g = (int)(c >> 8 & 0xff), b = (int)(c & 0xff);
    if (r > 150 && g > 150 && b > 150)
        return 4; /* white */
    if (r > 150 && g < 60 && b < 60)
        return 0;
    if (g > 150 && r < 60 && b < 60)
        return 1;
    if (b > 150 && r < 60 && g < 60)
        return 2;
    if (r > 150 && g > 150 && b < 60)
        return 3;
    return -1;
}

static struct
{
    unsigned frames;     /* frames whose picture is the test program's */
    bool colours[4];     /* the background colours seen */
    int xs[64];          /* the square's positions seen */
    int x_count;
    int square_w, square_h, square_y;
} seen;

static uint32_t pixel(const void *data, size_t pitch, unsigned x, unsigned y)
{
    const uint8_t *row = (const uint8_t *)data + y * pitch;
    if (pixel_format == RETRO_PIXEL_FORMAT_XRGB8888)
        return ((const uint32_t *)row)[x];
    const uint16_t c = ((const uint16_t *)row)[x];
    if (pixel_format == RETRO_PIXEL_FORMAT_RGB565)
        return (uint32_t)((c >> 11) << 19 | ((c >> 5) & 63) << 10 | (c & 31) << 3);
    return (uint32_t)(((c >> 10) & 31) << 19 | ((c >> 5) & 31) << 11 | (c & 31) << 3);
}

static void look(const void *data, unsigned w, unsigned h, size_t pitch)
{
    unsigned count[5] = {0};
    unsigned x0 = w, x1 = 0, y0 = h, y1 = 0;
    for (unsigned y = 0; y < h; ++y)
        for (unsigned x = 0; x < w; ++x)
        {
            int k = colour_class(pixel(data, pitch, x, y));
            if (k < 0)
                continue;
            ++count[k];
            if (k == 4)
            {
                x0 = x < x0 ? x : x0, x1 = x > x1 ? x : x1;
                y0 = y < y0 ? y : y0, y1 = y > y1 ? y : y1;
            }
        }
    int bg = 0;
    for (int k = 1; k < 4; ++k)
        bg = count[k] > count[bg] ? k : bg;
    /* the test program's picture: a background colour covers most of it and the white is one square */
    if (count[bg] < w * h / 2 || !count[4] || count[4] != (x1 - x0 + 1) * (y1 - y0 + 1))
        return;
    ++seen.frames;
    seen.colours[bg] = true;
    seen.square_w = (int)(x1 - x0 + 1);
    seen.square_h = (int)(y1 - y0 + 1);
    seen.square_y = (int)y0;
    bool known = false;
    for (int i = 0; i < seen.x_count; ++i)
        known |= seen.xs[i] == (int)x0;
    if (!known && seen.x_count < 64)
        seen.xs[seen.x_count++] = (int)x0;
}

static void video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
    if (!data)
    {
        ++frames_null;
        return;
    }
    ++frames_drawn;
    last_w = width;
    last_h = height;
    uint64_t hash = 1469598103934665603ull;
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x)
            hash = (hash ^ pixel(data, pitch, x, y)) * 1099511628211ull;
    last_hash = hash;
    look(data, width, height, pitch);
    FILE *f = shot_path ? fopen(shot_path, "wb") : NULL;
    if (f)
    {
        fprintf(f, "P6\n%u %u\n255\n", width, height);
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x)
            {
                const uint32_t c = pixel(data, pitch, x, y);
                const unsigned char rgb[3] = {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c};
                fwrite(rgb, 1, 3, f);
            }
        fclose(f);
    }
}

static void audio_sample(int16_t left, int16_t right)
{
    (void)left, (void)right;
    ++audio_frames;
}

static size_t audio_batch(const int16_t *data, size_t frames)
{
    (void)data;
    audio_frames += frames;
    return frames;
}

static void input_poll(void)
{
}

static int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
    (void)port, (void)device, (void)index, (void)id;
    return 0;
}

/* ---------------------------------------------------------------- the check */

static int failures;

static void check(bool ok, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    printf("%s ", ok ? "ok  " : "FAIL");
    vprintf(fmt, va);
    printf("\n");
    va_end(va);
    failures += !ok;
}

static bool start(const char *disc, const BeetleChoice *choice)
{
    retro_set_environment(environment);
    retro_set_video_refresh(video_refresh);
    retro_set_audio_sample(audio_sample);
    retro_set_audio_sample_batch(audio_batch);
    retro_set_input_poll(input_poll);
    retro_set_input_state(input_state);
    retro_init();
    option_problems = 0;
    beetle_options_apply(choice, set_option); /* before the game, as the app does */
    /* the controllers before the game boots, as the app does: a DualShock in both ports */
    retro_set_controller_port_device(0, RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1));
    retro_set_controller_port_device(1, RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1));
    struct retro_game_info game = {disc, NULL, 0, NULL};
    if (!retro_load_game(&game))
        return false;
    retro_get_system_av_info(&av);
    return true;
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "usage: %s <disc .cue> <system folder> <save folder> [frames] [2x]\n", argv[0]);
        return 2;
    }
    const char *disc = argv[1];
    system_dir = argv[2];
    save_dir = argv[3];
    const int frames = argc > 4 ? atoi(argv[4]) : 900;
    const int scale = argc > 5 && !strcmp(argv[5], "2x") ? 2 : 1;
    verbose = getenv("BEETLE_HEADLESS_VERBOSE") != NULL;
    shot_path = getenv("BEETLE_HEADLESS_SHOT");
    setvbuf(stdout, NULL, _IOLBF, 0);

    struct retro_system_info info;
    retro_get_system_info(&info);
    printf("core: %s %s, extensions %s, api %u\n", info.library_name, info.library_version, info.valid_extensions,
           retro_api_version());

    /* the app's defaults (Settings > Graphics > Beetle PSX: native, dithering on, widescreen hack off) */
    BeetleChoice choice = {scale - 1, true, false, 0, 1, true, false, false, 2};
    if (!start(disc, &choice))
    {
        printf("FAIL: the core did not load %s\n", disc);
        return 1;
    }
    /* every value the app may give (beetle_options.c), not only today's defaults */
    int values = 0;
    for (int i = 0; i < BEETLE_OPTION_COUNT; ++i)
        for (int k = 0; BEETLE_OPTIONS[i].values[k]; ++k, ++values)
            set_option(BEETLE_OPTIONS[i].key, BEETLE_OPTIONS[i].values[k]);
    beetle_options_apply(&choice, set_option); /* back to the choice the game was loaded with */
    check(option_problems == 0, "the %d options Mandrake sets, %d values, are all offered by the core (%d registered)",
          BEETLE_OPTION_COUNT, values, var_count);
    printf("av: %ux%u (max %ux%u), aspect %.3f, %.3f fps, %.0f Hz sound\n", av.geometry.base_width,
           av.geometry.base_height, av.geometry.max_width, av.geometry.max_height, av.geometry.aspect_ratio,
           av.timing.fps, av.timing.sample_rate);

    /* the run, with a save state in the middle: what follows it must come out the same when it is loaded again */
    const int at = frames - 60;
    size_t state_size = 0;
    uint8_t *state = NULL;
    uint64_t hash_after = 0;
    for (int f = 0; f < frames; ++f)
    {
        retro_run();
        if (f == at)
        {
            state_size = retro_serialize_size();
            /* as the app: a buffer with room to spare (host_state_size), the whole buffer given back */
            state = calloc(1, state_size + 512 * 1024);
            check(state && state_size > 0 && retro_serialize(state, state_size + 512 * 1024),
                  "a save state of %zu KB is taken at frame %d", state_size / 1024, f);
        }
        if (f == at + 30)
            hash_after = last_hash;
    }
    const unsigned drawn = frames_drawn;
    check(pixel_format == RETRO_PIXEL_FORMAT_XRGB8888, "the picture is XRGB8888");
    check(drawn >= (unsigned)frames * 9 / 10, "%u of %d frames have a picture (%u repeated)", drawn, frames,
          frames_null);
    check(last_h == 240u * scale && last_w >= 300u * scale && last_w <= 368u * scale,
          "the picture is %ux%u (%dx internal resolution)", last_w, last_h, scale);
    check(seen.frames >= 300, "%u frames show the test program's picture: it booted", seen.frames);
    check(seen.colours[0] && seen.colours[1] && seen.colours[2] && seen.colours[3],
          "its background went through the four colours (%s%s%s%s)", seen.colours[0] ? "red " : "",
          seen.colours[1] ? "green " : "", seen.colours[2] ? "blue " : "", seen.colours[3] ? "yellow" : "");
    check(seen.square_w == 32 * scale && seen.square_h == 32 * scale,
          "its white square is %dx%d at line %d", seen.square_w, seen.square_h, seen.square_y);
    check(seen.x_count >= 8, "the square moved: %d places", seen.x_count);
    const double seconds = frames / (av.timing.fps > 1 ? av.timing.fps : 60.0);
    check(audio_frames > seconds * av.timing.sample_rate * 0.9 && audio_frames < seconds * av.timing.sample_rate * 1.1,
          "%llu frames of sound for %.1f s", (unsigned long long)audio_frames, seconds);
    check(retro_get_memory_size(RETRO_MEMORY_SAVE_RAM) == 128 * 1024 && retro_get_memory_data(RETRO_MEMORY_SAVE_RAM),
          "memory card 1 is the frontend's: %zu bytes of SAVE_RAM", retro_get_memory_size(RETRO_MEMORY_SAVE_RAM));
    check(retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM) == 2 * 1024 * 1024 && memory_maps,
          "its RAM and memory map are there (RetroAchievements, cheats)");
    check(disk_interface, "it has the disc control interface (multi-disc games)");

    /* the save state again: the same frames follow */
    bool loaded = state && retro_unserialize(state, state_size + 512 * 1024);
    for (int f = 0; f < 31 && loaded; ++f)
        retro_run();
    check(loaded && last_hash == hash_after, "the state loaded again replays the same picture 30 frames later");
    free(state);

    /* Settings > Graphics > Beetle PSX resolution applies at once: the next frames are the other size */
    BeetleChoice other = choice;
    other.internal_res = !choice.internal_res;
    beetle_options_apply(&other, set_option);
    for (int f = 0; f < 10; ++f)
        retro_run();
    check(last_h == 240u * (other.internal_res + 1), "the resolution changed while it ran: %ux%u", last_w, last_h);
    beetle_options_apply(&choice, set_option);
    for (int f = 0; f < 10; ++f)
        retro_run();
    printf("frames: %d run, %u with a picture; %d places of the square, line %d\n", frames, drawn, seen.x_count,
           seen.square_y);

    /* the next game, as the app starts it: unloaded, the core shut down and started again */
    retro_unload_game();
    retro_deinit();
    var_count = 0;
    memset(&seen, 0, sizeof(seen));
    frames_drawn = 0;
    if (!start(disc, &choice))
        check(false, "a second game starts in the same process");
    else
    {
        for (int f = 0; f < 700; ++f)
            retro_run();
        check(seen.frames >= 100, "a second game starts in the same process and draws (%u frames)", seen.frames);
        retro_unload_game();
    }
    retro_deinit();
    printf(failures ? "FAIL\n" : "PASS\n");
    return failures ? 1 : 0;
}
