/*
 * Mandrake - the picture of a core that draws on the CPU, made ready for the GPU's path (cpu_frame.h).
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "cpu_frame.h"

#include "../SwanStationPS5.h"
#include "xbr.h"

#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- prescalers */

void cpu_frame_sharp(const uint32_t *src, int w, int h, size_t pitch_px, uint32_t *dst, int k)
{
    int dw = w * k;
    for (int y = 0; y < h; ++y)
    {
        uint32_t *row = dst + (size_t)y * k * dw;
        const uint32_t *s = src + (size_t)y * pitch_px;
        for (int x = 0; x < w; ++x)
            for (int i = 0; i < k; ++i)
                row[x * k + i] = s[x];
        for (int r = 1; r < k; ++r)
            memcpy(row + (size_t)r * dw, row, (size_t)dw * 4);
    }
}

void cpu_frame_scale2x(const uint32_t *src, int w, int h, size_t pitch_px, uint32_t *dst)
{
    int dw = w * 2;
    for (int y = 0; y < h; ++y)
    {
        const uint32_t *up = src + (size_t)(y > 0 ? y - 1 : y) * pitch_px;
        const uint32_t *row = src + (size_t)y * pitch_px;
        const uint32_t *dn = src + (size_t)(y < h - 1 ? y + 1 : y) * pitch_px;
        uint32_t *o0 = dst + (size_t)y * 2 * dw, *o1 = o0 + dw;
        for (int x = 0; x < w; ++x)
        {
            uint32_t B = up[x], D = row[x > 0 ? x - 1 : x], E = row[x];
            uint32_t F = row[x < w - 1 ? x + 1 : x], H = dn[x];
            bool edge = B != H && D != F;
            o0[x * 2] = edge && D == B ? D : E;
            o0[x * 2 + 1] = edge && B == F ? F : E;
            o1[x * 2] = edge && D == H ? D : E;
            o1[x * 2 + 1] = edge && H == F ? F : E;
        }
    }
}

void cpu_frame_scale3x(const uint32_t *src, int w, int h, size_t pitch_px, uint32_t *dst)
{
    int dw = w * 3;
    for (int y = 0; y < h; ++y)
    {
        const uint32_t *up = src + (size_t)(y > 0 ? y - 1 : y) * pitch_px;
        const uint32_t *row = src + (size_t)y * pitch_px;
        const uint32_t *dn = src + (size_t)(y < h - 1 ? y + 1 : y) * pitch_px;
        uint32_t *o0 = dst + (size_t)y * 3 * dw, *o1 = o0 + dw, *o2 = o1 + dw;
        for (int x = 0; x < w; ++x)
        {
            int l = x > 0 ? x - 1 : x, r = x < w - 1 ? x + 1 : x;
            uint32_t A = up[l], B = up[x], C = up[r], D = row[l], E = row[x], F = row[r];
            uint32_t G = dn[l], H = dn[x], I = dn[r];
            uint32_t *p0 = &o0[x * 3], *p1 = &o1[x * 3], *p2 = &o2[x * 3];
            if (B != H && D != F)
            {
                p0[0] = D == B ? D : E;
                p0[1] = (D == B && E != C) || (B == F && E != A) ? B : E;
                p0[2] = B == F ? F : E;
                p1[0] = (D == B && E != G) || (D == H && E != A) ? D : E;
                p1[1] = E;
                p1[2] = (B == F && E != I) || (H == F && E != C) ? F : E;
                p2[0] = D == H ? D : E;
                p2[1] = (D == H && E != I) || (H == F && E != G) ? H : E;
                p2[2] = H == F ? F : E;
            }
            else
                p0[0] = p0[1] = p0[2] = p1[0] = p1[1] = p1[2] = p2[0] = p2[1] = p2[2] = E;
        }
    }
}

/* ---------------------------------------------------------------- the picture for the GPU */

/* [0] the picture in 32 bits, [1] prescaled, [2] Scale2x's first pass of a 4x */
static uint32_t *buffers[3];
static size_t buffer_pixels[3];

static uint32_t *buffer(int which, size_t pixels)
{
    if (buffer_pixels[which] < pixels)
    {
        free(buffers[which]);
        buffers[which] = malloc(pixels * 4);
        buffer_pixels[which] = buffers[which] ? pixels : 0;
    }
    return buffers[which];
}

/* One row of the core's pixels in XRGB8888. */
static void row_to_xrgb(const void *src, int w, int format, uint32_t *dst)
{
    if (format == 1)
    {
        memcpy(dst, src, (size_t)w * 4);
        return;
    }
    const uint16_t *s = src;
    for (int x = 0; x < w; ++x)
    {
        const uint32_t c = s[x];
        uint32_t r, g, b;
        if (format == 2) /* RGB565 */
        {
            r = c >> 11 & 31, g = c >> 5 & 63, b = c & 31;
            g = g << 2 | g >> 4;
        }
        else /* 0RGB1555 */
        {
            r = c >> 10 & 31, g = c >> 5 & 31, b = c & 31;
            g = g << 3 | g >> 2;
        }
        r = r << 3 | r >> 2;
        b = b << 3 | b >> 2;
        dst[x] = r << 16 | g << 8 | b;
    }
}

bool cpu_frame_prepare(const void *pixels, int w, int h, size_t pitch_bytes, int format, int upscale, int filter,
                       CpuFrame *out)
{
    if (!pixels || w <= 0 || h <= 0 || (format != 0 && format != 1 && format != 2))
        return false;
    uint32_t *flat = buffer(0, (size_t)w * h);
    if (!flat)
        return false;
    for (int y = 0; y < h; ++y)
        row_to_xrgb((const uint8_t *)pixels + (size_t)y * pitch_bytes, w, format, flat + (size_t)y * w);
    out->pixels = flat;
    out->w = w;
    out->h = h;
    out->scale = 1;
    int k = upscale < 1 ? 1 : upscale > 4 ? 4 : upscale;
    if (k == 1 || w > CPU_FRAME_PRESCALE_MAX_W)
        return true;
    if (filter == UPSCALE_XBR)
        k = 2; /* one pass: the final scale to the screen does the rest */
    else if (filter != UPSCALE_SMOOTH_PIXELS)
        filter = UPSCALE_SHARP;
    uint32_t *big = buffer(1, (size_t)w * k * h * k);
    if (!big)
        return true; /* shown as drawn */
    if (filter == UPSCALE_XBR)
        xbr2x(flat, w, h, (size_t)w, big);
    else if (filter == UPSCALE_SMOOTH_PIXELS && k == 2)
        cpu_frame_scale2x(flat, w, h, (size_t)w, big);
    else if (filter == UPSCALE_SMOOTH_PIXELS && k == 3)
        cpu_frame_scale3x(flat, w, h, (size_t)w, big);
    else if (filter == UPSCALE_SMOOTH_PIXELS)
    {
        uint32_t *mid = buffer(2, (size_t)w * 2 * h * 2);
        if (!mid)
            return true;
        cpu_frame_scale2x(flat, w, h, (size_t)w, mid);
        cpu_frame_scale2x(mid, w * 2, h * 2, (size_t)w * 2, big);
    }
    else
        cpu_frame_sharp(flat, w, h, (size_t)w, big, k);
    out->pixels = big;
    out->w = w * k;
    out->h = h * k;
    out->scale = k;
    return true;
}

/* ---------------------------------------------------------------- repeated pictures */

static uint8_t *last;
static size_t last_size;
static unsigned last_w, last_h, last_bpp;
static bool have_last;

bool cpu_frame_repeats(const void *pixels, unsigned w, unsigned h, size_t pitch_bytes, unsigned bytes_per_pixel)
{
    const size_t row = (size_t)w * bytes_per_pixel;
    if (!pixels || !w || !h || pitch_bytes < row)
        return false;
    const uint8_t *src = pixels;
    if (have_last && w == last_w && h == last_h && bytes_per_pixel == last_bpp)
    {
        unsigned y = 0;
        while (y < h && !memcmp(last + (size_t)y * row, src + (size_t)y * pitch_bytes, row))
            ++y;
        if (y == h)
            return true;
        for (; y < h; ++y) /* the rows before were the same */
            memcpy(last + (size_t)y * row, src + (size_t)y * pitch_bytes, row);
        return false;
    }
    if (last_size < row * h)
    {
        free(last);
        last = malloc(row * h);
        last_size = last ? row * h : 0;
    }
    have_last = last != NULL;
    if (!have_last)
        return false;
    for (unsigned y = 0; y < h; ++y)
        memcpy(last + (size_t)y * row, src + (size_t)y * pitch_bytes, row);
    last_w = w;
    last_h = h;
    last_bpp = bytes_per_pixel;
    return false;
}

void cpu_frame_forget(void)
{
    have_last = false;
}
