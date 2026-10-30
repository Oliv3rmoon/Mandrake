/*
 * Mandrake - the picture of a core that draws on the CPU (Beetle PSX), made ready for the GPU's path.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * SwanStation hands the screen a Vulkan image; Beetle PSX hands it pixels in memory. cpu_frame_prepare turns those
 * pixels into what the GPU path takes in place of a core's image (vk_present.c, vkp_set_game_pixels): XRGB8888 rows
 * packed one after the other, prescaled first when Settings > Graphics > Beetle PSX upscale filter asks for it. From
 * there the shaders, AMD FSR and CAS, generated frames and supersampling see no difference between the two cores.
 *
 * cpu_frame_repeats tells the frames that show the picture of the one before (a 30 fps game on the 60 fps console),
 * which SwanStation says itself: frame generation and Show FPS count the game's own frames by it.
 *
 * Plain C, no SDL and no Vulkan: tests/core/beetle_headless.c runs Beetle PSX's frames through this same code.
 */
#ifndef MANDRAKE_CPU_FRAME_H
#define MANDRAKE_CPU_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A picture ready for the GPU path: w x h XRGB8888 pixels, w per row. */
typedef struct
{
    const uint32_t *pixels;
    int w, h;
    int scale; /* the prescale applied, 1 when none */
} CpuFrame;

/* Pictures wider than this are a 2x picture or the PS1's high-resolution modes already: no prescaler for them. */
#define CPU_FRAME_PRESCALE_MAX_W 400

/* The prescalers, XRGB8888: src w x h with pitch_px pixels a row, dst packed. Sharp repeats each pixel k times;
 * Scale2x / Scale3x are the AdvanceMAME edge rules, which round off staircase edges without blurring flat areas. */
void cpu_frame_sharp(const uint32_t *src, int w, int h, size_t pitch_px, uint32_t *dst, int k);
void cpu_frame_scale2x(const uint32_t *src, int w, int h, size_t pitch_px, uint32_t *dst);
void cpu_frame_scale3x(const uint32_t *src, int w, int h, size_t pitch_px, uint32_t *dst);

/* The core's picture (format: libretro's 0 for 0RGB1555, 1 XRGB8888, 2 RGB565, pitch in bytes) as the GPU path takes
 * it. upscale 2 to 4 with filter (enum UpscaleFilter of SwanStationPS5.h) prescales a picture no wider than
 * CPU_FRAME_PRESCALE_MAX_W: xBR always doubles it, Scale2x / Scale3x and Sharp by upscale. A wider picture, or upscale
 * 1, is only copied. The result stays in this module's memory until the next call. False with no memory for it. */
bool cpu_frame_prepare(const void *pixels, int w, int h, size_t pitch_bytes, int format, int upscale, int filter,
                       CpuFrame *out);

/* Is this picture, pixel for pixel, the one given last time? It is remembered when it is not. bytes_per_pixel: 2 or 4. */
bool cpu_frame_repeats(const void *pixels, unsigned w, unsigned h, size_t pitch_bytes, unsigned bytes_per_pixel);
/* Forget the last picture (a game was loaded): the next one is new whatever it shows. */
void cpu_frame_forget(void);

#endif
