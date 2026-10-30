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
 * replays the same frames (run-ahead depends on it), rewind's ring (src/rewind.c) steps back and plays the same frames
 * forward again, the DualShock's mode is told in the words host.c reads, the pictures that repeat the last one are
 * found as the app finds them (src/platform/cpu_frame.c), and a second game starts in the same process.
 *
 * The picture's way to the screen, as the app takes it since 1.8.0: through cpu_frame_prepare (the prescalers) into
 * the GPU path's image. Built with MANDRAKE_HEADLESS_VULKAN (tools/build-beetle.sh does when the computer has Vulkan,
 * as the CI's runner has with lavapipe), the picture is uploaded through vk_cpu_image.inc, the presenter's own code,
 * on a Vulkan device of this computer, and read back: it must be the core's picture, pixel for pixel.
 */
#include "beetle_options.h"
#include "libretro.h"

#include "../../src/SwanStationPS5.h"
#include "../../src/platform/blit.h"
#include "../../src/platform/cpu_frame.h"
#include "../../src/rewind.h"

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
static int analog_mode[8] = {-1, -1, -1, -1, -1, -1, -1, -1}; /* each port's DualShock mode, as Beetle PSX told it */
static int analog_messages;

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
    {
        const char *msg = ((const struct retro_message_ext *)data)->msg;
        if (verbose)
            fprintf(stderr, "message: %s\n", msg);
        int port, mode = beetle_analog_message(msg, &port); /* as host.c reads it */
        if (mode >= 0 && port >= 1 && port <= 8)
        {
            analog_mode[port - 1] = mode;
            ++analog_messages;
        }
        return true;
    }
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

/* The last picture, as the core gave it (for the picture's way to the screen, below) */
static uint8_t *last_data;
static size_t last_pitch, last_capacity;
/* Repeated pictures: told by cpu_frame_repeats as the app tells them, and by the pictures' hashes here */
static unsigned repeats_app, repeats_hash, repeat_mismatches;
static uint64_t hash_before;
static unsigned w_before, h_before;
static bool have_before;
/* Rewind's check: each frame's hash, by the frame's number */
#define TRACK_FRAMES 1024
static uint64_t track[TRACK_FRAMES];
static int track_at = -1; /* the frame being run, -1: not tracked */
static bool replaying;    /* the frames are compared with the ones tracked before */
static unsigned replayed, replay_mismatches;

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
    if (track_at >= 0 && track_at < TRACK_FRAMES)
    {
        if (replaying)
            replay_mismatches += track[track_at] != hash, ++replayed;
        else
            track[track_at] = hash;
    }
    const unsigned bytes = pixel_format == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2;
    const bool same_app = cpu_frame_repeats(data, width, height, pitch, bytes);
    const bool same_hash = have_before && hash == hash_before && width == w_before && height == h_before;
    repeats_app += same_app;
    repeats_hash += same_hash;
    repeat_mismatches += same_app != same_hash;
    hash_before = hash;
    w_before = width;
    h_before = height;
    have_before = true;
    if (last_capacity < (size_t)height * pitch)
    {
        free(last_data);
        last_data = malloc((size_t)height * pitch);
        last_capacity = last_data ? (size_t)height * pitch : 0;
    }
    if (last_data)
    {
        memcpy(last_data, data, (size_t)height * pitch);
        last_pitch = pitch;
    }
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

static uint16_t held; /* RetroPad buttons held on port 1 */

static int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
    (void)index;
    if (port != 0 || (device & RETRO_DEVICE_MASK) != RETRO_DEVICE_JOYPAD)
        return 0;
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
        return (int16_t)held;
    return id < 16 ? (int16_t)((held >> id) & 1) : 0;
}

/* xbr.c's rows run through the app's worker threads (blit.c, on SDL): here, one after the other */
void blit_parallel(void (*fn)(void *ctx, int row_begin, int row_end), void *ctx, int rows)
{
    fn(ctx, 0, rows);
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

/* ---------------------------------------------------------------- the GPU path's image, on a Vulkan device */

#if defined(MANDRAKE_HEADLESS_VULKAN)
#include <vulkan/vulkan.h>
/* the Vulkan structures are started as vk_present.c starts them, {type} and the rest zero */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

/* What vk_cpu_image.inc expects of vk_present.c: its state V, FRAMES, allocate() and barrier(), the log */
#define FRAMES 2
static struct
{
    VkInstance instance;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue queue;
    uint32_t family;
    VkCommandPool pool;
    VkSwapchainKHR swapchain; /* none here: vkp_set_game_pixels only asks that the screen be open */
    VkImage game_image;
    VkImageView game_view;
    VkImageLayout game_layout;
    uint32_t core_frames, new_frames;
} V;

void SwanStationPS5_log(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "log: ");
    vfprintf(stderr, fmt, va);
    fprintf(stderr, "\n");
    va_end(va);
}

static int memory_type(uint32_t bits, VkMemoryPropertyFlags want)
{
    VkPhysicalDeviceMemoryProperties m;
    vkGetPhysicalDeviceMemoryProperties(V.gpu, &m);
    for (uint32_t i = 0; i < m.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (m.memoryTypes[i].propertyFlags & want) == want)
            return (int)i;
    return -1;
}

static bool allocate(VkMemoryRequirements req, VkMemoryPropertyFlags want, VkDeviceMemory *out)
{
    int type = memory_type(req.memoryTypeBits, want);
    if (type < 0)
        return false;
    VkMemoryAllocateInfo info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, req.size, (uint32_t)type};
    return vkAllocateMemory(V.device, &info, NULL, out) == VK_SUCCESS;
}

static void barrier(VkCommandBuffer cb, VkImage image, VkImageLayout from, VkImageLayout to,
                    VkAccessFlags src_access, VkAccessFlags dst_access, VkPipelineStageFlags src,
                    VkPipelineStageFlags dst)
{
    VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout = from;
    b.newLayout = to;
    b.srcAccessMask = src_access;
    b.dstAccessMask = dst_access;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cb, src, dst, 0, 0, NULL, 0, NULL, 1, &b);
}

#include "../../src/platform/vk/vk_cpu_image.inc"

/* A device of this computer's (lavapipe on the CI's runner): false, with why, when there is none */
static bool vk_open(char *why, size_t size)
{
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "mandrake-headless";
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo ii = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ii.pApplicationInfo = &app;
    VkResult r = vkCreateInstance(&ii, NULL, &V.instance);
    if (r != VK_SUCCESS)
    {
        snprintf(why, size, "no Vulkan instance (%d)", (int)r);
        return false;
    }
    uint32_t count = 1;
    r = vkEnumeratePhysicalDevices(V.instance, &count, &V.gpu);
    if ((r != VK_SUCCESS && r != VK_INCOMPLETE) || count == 0)
    {
        snprintf(why, size, "no Vulkan device");
        return false;
    }
    VkQueueFamilyProperties families[16];
    uint32_t n = 16;
    vkGetPhysicalDeviceQueueFamilyProperties(V.gpu, &n, families);
    V.family = UINT32_MAX;
    for (uint32_t i = 0; i < n && V.family == UINT32_MAX; ++i)
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            V.family = i;
    if (V.family == UINT32_MAX)
    {
        snprintf(why, size, "no graphics queue");
        return false;
    }
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo qi = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex = V.family;
    qi.queueCount = 1;
    qi.pQueuePriorities = &priority;
    VkDeviceCreateInfo di = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    di.queueCreateInfoCount = 1;
    di.pQueueCreateInfos = &qi;
    if (vkCreateDevice(V.gpu, &di, NULL, &V.device) != VK_SUCCESS)
    {
        snprintf(why, size, "no Vulkan device could be made");
        return false;
    }
    vkGetDeviceQueue(V.device, V.family, 0, &V.queue);
    VkCommandPoolCreateInfo cp = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cp.queueFamilyIndex = V.family;
    if (vkCreateCommandPool(V.device, &cp, NULL, &V.pool) != VK_SUCCESS)
    {
        snprintf(why, size, "no command pool");
        return false;
    }
    V.swapchain = (VkSwapchainKHR)(uintptr_t)1; /* "the screen is open" */
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(V.gpu, &props);
    snprintf(why, size, "%s", props.deviceName);
    return true;
}

static void vk_close(void)
{
    if (V.device)
    {
        vkDeviceWaitIdle(V.device);
        cpu_image_free(true);
        vkDestroyCommandPool(V.device, V.pool, NULL);
        vkDestroyDevice(V.device, NULL);
    }
    if (V.instance)
        vkDestroyInstance(V.instance, NULL);
    memset(&V, 0, sizeof(V));
}

/* A host-visible buffer the GPU copies into */
static bool readback_buffer(VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory)
{
    VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkMemoryRequirements req;
    if (vkCreateBuffer(V.device, &bi, NULL, buffer) != VK_SUCCESS)
        return false;
    vkGetBufferMemoryRequirements(V.device, *buffer, &req);
    return allocate(req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memory) &&
           vkBindBufferMemory(V.device, *buffer, *memory, 0) == VK_SUCCESS;
}

/* The picture through the presenter's code (vkp_set_game_pixels, then the frame's upload, cpu_upload), and back: the
 * image's own bytes, and the image blitted into an RGBA one, which is how a shader's sampler reads its colours. */
static void vk_upload_check(const CpuFrame *frame, int slot, const char *what)
{
    const uint32_t w = (uint32_t)frame->w, h = (uint32_t)frame->h;
    const bool set = vkp_set_game_pixels(frame->pixels, frame->w, frame->h);
    check(set && V.game_view == C.view && V.game_image == C.image && C.w == w && C.h == h && C.pending,
          "%s: the GPU path takes the %ux%u picture as the game's image", what, w, h);
    if (!set)
        return;
    VkBuffer raw = VK_NULL_HANDLE, rgba = VK_NULL_HANDLE;
    VkDeviceMemory raw_memory = VK_NULL_HANDLE, rgba_memory = VK_NULL_HANDLE, image_memory = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_R8G8B8A8_UNORM;
    ii.extent = (VkExtent3D){w, h, 1};
    ii.mipLevels = ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkMemoryRequirements req;
    bool ok = readback_buffer((VkDeviceSize)w * h * 4, &raw, &raw_memory) &&
              readback_buffer((VkDeviceSize)w * h * 4, &rgba, &rgba_memory) &&
              vkCreateImage(V.device, &ii, NULL, &image) == VK_SUCCESS;
    if (ok)
    {
        vkGetImageMemoryRequirements(V.device, image, &req);
        ok = allocate(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &image_memory) &&
             vkBindImageMemory(V.device, image, image_memory, 0) == VK_SUCCESS;
    }
    VkCommandBuffer cb = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo ca = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ca.commandPool = V.pool;
    ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ca.commandBufferCount = 1;
    ok = ok && vkAllocateCommandBuffers(V.device, &ca, &cb) == VK_SUCCESS;
    if (ok)
    {
        VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(cb, &begin);
        cpu_upload(cb, slot); /* as present_frame does at the start of a frame */
        C.pending = false;
        barrier(cb, C.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy copy = {0};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = (VkExtent3D){w, h, 1};
        vkCmdCopyImageToBuffer(cb, C.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, raw, 1, &copy);
        barrier(cb, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkImageBlit blit = {0};
        blit.srcSubresource.aspectMask = blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.layerCount = blit.dstSubresource.layerCount = 1;
        blit.srcOffsets[1] = blit.dstOffsets[1] = (VkOffset3D){(int32_t)w, (int32_t)h, 1};
        vkCmdBlitImage(cb, C.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, VK_FILTER_NEAREST);
        barrier(cb, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);
        vkCmdCopyImageToBuffer(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rgba, 1, &copy);
        barrier(cb, C.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        vkEndCommandBuffer(cb);
        VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cb;
        ok = vkQueueSubmit(V.queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS && vkQueueWaitIdle(V.queue) == VK_SUCCESS;
    }
    unsigned raw_wrong = 0, colour_wrong = 0;
    const uint8_t *bytes = NULL, *colours = NULL;
    if (ok && vkMapMemory(V.device, raw_memory, 0, VK_WHOLE_SIZE, 0, (void **)&bytes) == VK_SUCCESS &&
        vkMapMemory(V.device, rgba_memory, 0, VK_WHOLE_SIZE, 0, (void **)&colours) == VK_SUCCESS)
        for (size_t i = 0; i < (size_t)w * h; ++i)
        {
            uint32_t got;
            memcpy(&got, bytes + i * 4, 4);
            const uint32_t want = frame->pixels[i];
            raw_wrong += got != want;
            const uint8_t *c = colours + i * 4;
            colour_wrong += c[0] != (uint8_t)(want >> 16) || c[1] != (uint8_t)(want >> 8) || c[2] != (uint8_t)want;
        }
    else
        ok = false;
    check(ok && raw_wrong == 0 && colour_wrong == 0,
          "%s: uploaded to the GPU and read back, the %ux%u picture is the same, pixel for pixel (%u differ), and its "
          "colours read as red, green and blue (%u differ)", what, w, h, raw_wrong, colour_wrong);
    if (cb)
        vkFreeCommandBuffers(V.device, V.pool, 1, &cb);
    if (image)
        vkDestroyImage(V.device, image, NULL);
    if (image_memory)
        vkFreeMemory(V.device, image_memory, NULL);
    if (raw)
        vkDestroyBuffer(V.device, raw, NULL);
    if (raw_memory)
        vkFreeMemory(V.device, raw_memory, NULL);
    if (rgba)
        vkDestroyBuffer(V.device, rgba, NULL);
    if (rgba_memory)
        vkFreeMemory(V.device, rgba_memory, NULL);
}
#endif


/* ---------------------------------------------------------------- what the app adds around the core */

/* The last picture through cpu_frame_prepare, as the app hands it to the GPU path: as it is, and with Beetle PSX's
 * upscale filter (xBR). On a Vulkan device, it goes up into the presenter's image too and is read back. */
static void picture_path_check(void)
{
    if (!last_data || !last_w)
    {
        check(false, "a picture to send the GPU's way");
        return;
    }
    CpuFrame plain, prescaled;
    bool ok = cpu_frame_prepare(last_data, (int)last_w, (int)last_h, last_pitch, pixel_format, 1, UPSCALE_SHARP, &plain);
    unsigned wrong = 0;
    for (unsigned y = 0; ok && y < last_h; ++y)
        for (unsigned x = 0; x < last_w; ++x)
            wrong += (plain.pixels[(size_t)y * last_w + x] & 0xffffffu) != (pixel(last_data, last_pitch, x, y) & 0xffffffu);
    check(ok && plain.w == (int)last_w && plain.h == (int)last_h && plain.scale == 1 && wrong == 0,
          "the GPU path's picture is the core's %ux%u, pixel for pixel (%u differ)", last_w, last_h, wrong);
    /* the copy must not change when the next picture is prepared: hold this one apart */
    uint32_t *held_plain = malloc((size_t)plain.w * plain.h * 4);
    if (held_plain)
        memcpy(held_plain, plain.pixels, (size_t)plain.w * plain.h * 4);
    ok = cpu_frame_prepare(last_data, (int)last_w, (int)last_h, last_pitch, pixel_format, 2, UPSCALE_XBR, &prescaled);
    if (last_w <= CPU_FRAME_PRESCALE_MAX_W)
    {
        /* the white square, twice as big; xBR keeps a square's straight sides */
        unsigned x0 = (unsigned)prescaled.w, x1 = 0, y0 = (unsigned)prescaled.h, y1 = 0;
        for (int y = 0; ok && y < prescaled.h; ++y)
            for (int x = 0; x < prescaled.w; ++x)
                if (colour_class(prescaled.pixels[(size_t)y * prescaled.w + x]) == 4)
                {
                    x0 = (unsigned)x < x0 ? (unsigned)x : x0, x1 = (unsigned)x > x1 ? (unsigned)x : x1;
                    y0 = (unsigned)y < y0 ? (unsigned)y : y0, y1 = (unsigned)y > y1 ? (unsigned)y : y1;
                }
        check(ok && prescaled.scale == 2 && prescaled.w == 2 * (int)last_w && prescaled.h == 2 * (int)last_h &&
                  x1 - x0 + 1 == 64 && y1 - y0 + 1 == 64,
              "Beetle PSX upscale filter, xBR: %ux%u becomes %dx%d, its white square %ux%u", last_w, last_h,
              prescaled.w, prescaled.h, x1 - x0 + 1, y1 - y0 + 1);
    }
    else
        check(ok && prescaled.scale == 1 && prescaled.w == (int)last_w && prescaled.h == (int)last_h,
              "Beetle PSX upscale filter, xBR: a %ux%u picture is wide enough already and kept as it is", last_w,
              last_h);
#if defined(MANDRAKE_HEADLESS_VULKAN)
    char device[256];
    if (vk_open(device, sizeof(device)))
    {
        printf("vulkan: %s\n", device);
        if (held_plain)
        {
            const CpuFrame as_drawn = {held_plain, plain.w, plain.h, 1};
            vk_upload_check(&prescaled, 0, "the upscaled picture");
            vk_upload_check(&as_drawn, 1, "the picture as drawn"); /* another size: the image is made again */
            vk_upload_check(&as_drawn, 0, "the same picture again, through the other frame's staging buffer");
        }
        vk_close();
    }
    else
        check(!getenv("MANDRAKE_REQUIRE_VULKAN"), "Vulkan: %s%s", device,
              getenv("MANDRAKE_REQUIRE_VULKAN") ? "" : " (the upload to the GPU is checked where there is one)");
#else
    check(!getenv("MANDRAKE_REQUIRE_VULKAN"), "Vulkan: built without it%s",
          getenv("MANDRAKE_REQUIRE_VULKAN") ? "" : " (the upload to the GPU is checked where there is one)");
#endif
    free(held_plain);
}

/* src/rewind.c, as play.c uses it: the app's state size, with its room to spare */
static size_t rewind_state_size(void)
{
    return retro_serialize_size() + 512 * 1024;
}

static void run_tracked(int first, int count, bool compare, Rewind *ring, const RewindCore *core)
{
    replaying = compare;
    for (int f = first; f < first + count; ++f)
    {
        track_at = f;
        retro_run();
        if (ring)
            rewind_record(ring, core);
    }
    track_at = -1;
    replaying = false;
}

/* Recorded as the game runs, stepped back, played forward again: the same frames. Then all the way back, and the ring
 * holds no more than its REWIND_SLOTS states. */
static void rewind_check(void)
{
    const RewindCore core = {rewind_state_size, retro_serialize, retro_unserialize};
    Rewind ring = {0};
    const int frames = 25 * REWIND_STEP; /* a state after each REWIND_STEP frames: 25 of them */
    run_tracked(0, frames, false, &ring, &core);
    const int steps = 5;
    bool stepped = true;
    for (int i = 0; i < steps; ++i)
        stepped = rewind_step(&ring, &core) && stepped;
    /* the latest state is the one after the last frame; each step before it, REWIND_STEP frames earlier */
    const int from = frames - (steps - 1) * REWIND_STEP;
    replayed = replay_mismatches = 0;
    run_tracked(from, frames - from, true, NULL, NULL);
    check(stepped && replayed == (unsigned)(frames - from) && replay_mismatches == 0,
          "rewind: %d steps back (%.1f s), the %u frames played again are the ones played the first time (%u differ)",
          steps, (steps - 1) * REWIND_STEP / 59.94, replayed, replay_mismatches);
    int more = 0;
    while (rewind_step(&ring, &core))
        ++more;
    check(more == 25 - steps && ring.filled == 0, "rewind: %d more steps reach the oldest state, and then no more", more);
    run_tracked(0, (REWIND_SLOTS + 10) * REWIND_STEP, false, &ring, &core);
    check(ring.filled == REWIND_SLOTS, "rewind: the ring keeps the last %d states (%d s), %zu KB each", ring.filled,
          (int)(REWIND_SLOTS * REWIND_STEP / 59.94 + 0.5), ring.state_size / 1024);
    rewind_reset(&ring);
}

/* The DualShock's ANALOG combination (L1 + R1 + Select, held), as a player presses it: Beetle PSX says the new mode in
 * the words host.c reads (the light bar and Left stick as D-pad follow them). */
static void analog_check(void)
{
    const uint16_t combo = 1u << RETRO_DEVICE_ID_JOYPAD_L | 1u << RETRO_DEVICE_ID_JOYPAD_R |
                           1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
    const int before = analog_mode[0];
    int seen[2] = {-1, -1};
    for (int press = 0; press < 2; ++press)
    {
        held = combo;
        for (int f = 0; f < 150; ++f) /* the core's hold time, 1 s */
            retro_run();
        held = 0;
        for (int f = 0; f < 30; ++f)
            retro_run();
        seen[press] = analog_mode[0];
    }
    check(before != 1 && seen[0] == 1 && seen[1] == 0,
          "the DualShock's ANALOG combination: Beetle PSX says port 1 went to analog mode, then back to digital "
          "(%d messages read)", analog_messages);
}

static bool start(const char *disc, const BeetleChoice *choice);

/* A PAL disc (the test program on a European disc, which the core takes for PAL content by its serial): 50 Hz, and
 * 60 Hz while SwanStation's Force NTSC Timings, which Beetle PSX follows (beetle_psx_pal_video_timing_override), is on,
 * at once both ways. Only the timing is looked at: the program itself sets an NTSC screen. */
static int pal_check(const char *disc, const BeetleChoice *choice)
{
    if (!start(disc, choice))
    {
        printf("FAIL: the core did not load %s\n", disc);
        return 1;
    }
    for (int f = 0; f < 120; ++f)
        retro_run();
    const double pal = av.timing.fps;
    check(pal > 49.0 && pal < 51.0, "a PAL disc runs at %.3f Hz", pal);
    BeetleChoice sixty = *choice;
    sixty.pal_60 = true;
    beetle_options_apply(&sixty, set_option);
    for (int f = 0; f < 10; ++f)
        retro_run();
    const double ntsc = av.timing.fps;
    check(ntsc > 59.0 && ntsc < 61.0, "Force NTSC Timings on: the PAL disc runs at %.3f Hz, at once", ntsc);
    beetle_options_apply(choice, set_option);
    for (int f = 0; f < 10; ++f)
        retro_run();
    check(av.timing.fps > 49.0 && av.timing.fps < 51.0, "and off again: %.3f Hz", av.timing.fps);
    retro_unload_game();
    retro_deinit();
    printf(failures ? "FAIL\n" : "PASS\n");
    return failures ? 1 : 0;
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
        fprintf(stderr, "usage: %s <disc .cue> <system folder> <save folder> [frames] [2x]\n"
                        "       %s <PAL disc .cue> <system folder> <save folder> pal\n", argv[0], argv[0]);
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
    BeetleChoice choice = {scale - 1, true, false, 0, 1, true, false, false, 2, false};
    if (argc > 4 && !strcmp(argv[4], "pal"))
        return pal_check(disc, &choice);
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

    /* what the app adds: the picture's way to the GPU, the repeated pictures, rewind, the ANALOG button's mode */
    picture_path_check();
    check(repeat_mismatches == 0 && repeats_app == repeats_hash,
          "%u of %u pictures repeat the one before: the app finds the same ones (%u differ)", repeats_app,
          frames_drawn, repeat_mismatches);
    rewind_check();
    analog_check();

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
