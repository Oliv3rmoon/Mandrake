/*
 * SwanStationPS5 - probe of the USB CD drive. See discprobe.h.
 * The SCSI pass-through follows ps-disc-detect (Al-Azif, GPL-3.0) and PS5SX2 (GPL-3.0): the command counts as
 * done only when the status is CAM_REQ_CMP and nothing is left untransferred (resid == 0).
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "discprobe.h"

#include <SDL2/SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cdscsi.h"

static SDL_atomic_t state;

int discprobe_state(void)
{
    return SDL_AtomicGet(&state);
}

#define P(...) SwanStationPS5_log("discprobe: " __VA_ARGS__)

static void hex(const char *what, const uint8_t *b, int n)
{
    char line[3 * 32 + 1];
    int m = n < 32 ? n : 32;
    for (int i = 0; i < m; ++i)
        snprintf(line + 3 * i, 4, "%02x ", b[i]);
    line[3 * m] = '\0';
    P("%s: %s", what, line);
}

static void probe_drive(unsigned unit)
{
    CdPass p;
    if (!cdscsi_open(unit, &p))
        return;
    uint8_t buf[2048 + 64];
    uint32_t sense;

    static const uint8_t inquiry[6] = {0x12, 0, 0, 0, 36, 0};
    memset(buf, 0, sizeof(buf));
    if (cdscsi_cmd(&p, inquiry, 6, buf, 36, 5000, &sense, NULL) == 0)
        P("INQUIRY: type %u vendor '%.8s' product '%.16s' revision '%.4s'", buf[0] & 0x1f, buf + 8, buf + 16,
          buf + 32);
    else
        P("INQUIRY failed (sense %06x)", sense);

    /* the disc must be ready: a USB drive takes a few seconds to spin up (02/04 = becoming ready) */
    static const uint8_t tur[6] = {0};
    int ready = -1;
    for (int i = 0; i < 20; ++i)
    {
        ready = cdscsi_cmd(&p, tur, 6, NULL, 0, 5000, &sense, NULL);
        if (ready == 0 || (sense != 0x020400 && (sense >> 16) != 6))
            break;
        SDL_Delay(250);
    }
    P("TEST UNIT READY: %s (sense %06x)", ready == 0 ? "ready" : "not ready", sense);

    static const uint8_t getconf[12] = {0x46, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0};
    memset(buf, 0, 8);
    if (cdscsi_cmd(&p, getconf, 12, buf, 8, 5000, &sense, NULL) == 0)
        P("GET CONFIGURATION: current profile 0x%04x (0x08 CD-ROM, 0x09 CD-R, 0x0a CD-RW)", buf[6] << 8 | buf[7]);
    else
        P("GET CONFIGURATION failed (sense %06x)", sense);

    if (ready != 0)
    {
        P("no disc ready: put a PS1 disc in and run the probe again");
        cdscsi_close(&p);
        return;
    }

    static const uint8_t toc[10] = {0x43, 0, 0, 0, 0, 0, 0, 0x08, 0x00, 0};
    uint8_t tocbuf[2048];
    memset(tocbuf, 0, sizeof(tocbuf));
    uint32_t first_audio_lba = 0, toc_got = 0;
    if (cdscsi_cmd(&p, toc, 10, tocbuf, sizeof(tocbuf), 10000, &sense, &toc_got) == 0)
    {
        int total = tocbuf[0] << 8 | tocbuf[1];
        int entries = (total - 2) / 8;
        P("READ TOC: %u bytes came, the table says %d", toc_got, total + 2);
        if ((uint32_t)(total + 2) > toc_got)
            entries = ((int)toc_got - 4) / 8;
        P("READ TOC: first track %u, last %u, %d entries", tocbuf[2], tocbuf[3], entries);
        for (int i = 0; i < entries && 4 + 8 * i + 8 <= (int)sizeof(tocbuf); ++i)
        {
            const uint8_t *e = tocbuf + 4 + 8 * i;
            uint32_t lba = (uint32_t)e[4] << 24 | e[5] << 16 | e[6] << 8 | e[7];
            bool data = (e[1] & 0x04) != 0;
            P("  track %02x: %s, starts at LBA %u", e[2], data ? "data" : "audio", lba);
            if (!data && e[2] != 0xaa && !first_audio_lba)
                first_audio_lba = lba;
        }
    }
    else
        P("READ TOC failed (sense %06x)", sense);

    /* 2352 raw bytes of the primary volume descriptor (LBA 16): 12 sync, 4 header, 8 subheader, then 2048 of data
     * that start with 01 "CD001" */
    uint8_t raw[2352 + 64];
    memset(raw, 0, sizeof(raw));
    uint8_t rd[12] = {0xbe, 0, 0, 0, 0, 16, 0, 0, 1, 0xf8, 0, 0};
    if (cdscsi_cmd(&p, rd, 12, raw, 2352, 10000, &sense, NULL) == 0)
    {
        static const uint8_t sync[12] = {0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0};
        bool has_sync = memcmp(raw, sync, 12) == 0;
        bool has_cd001 = memcmp(raw + 25, "CD001", 5) == 0;
        P("READ CD LBA 16 (F8): sync %s, mode %u, 'CD001' %s", has_sync ? "yes" : "NO", raw[15],
          has_cd001 ? "yes" : "NO");
        hex("  first bytes", raw, 32);
        hex("  at 24", raw + 24, 32);
        if (has_cd001)
            P("  volume id: '%.32s'", raw + 24 + 40);
    }
    else
        P("READ CD LBA 16 failed (sense %06x)", sense);

    if (first_audio_lba)
    {
        rd[2] = (uint8_t)(first_audio_lba >> 24), rd[3] = (uint8_t)(first_audio_lba >> 16),
        rd[4] = (uint8_t)(first_audio_lba >> 8), rd[5] = (uint8_t)first_audio_lba;
        rd[9] = 0x10; /* the 2352 bytes of audio */
        memset(raw, 0, sizeof(raw));
        if (cdscsi_cmd(&p, rd, 12, raw, 2352, 10000, &sense, NULL) == 0)
            P("READ CD on an audio track (LBA %u): ok", first_audio_lba);
        else
            P("READ CD on an audio track (LBA %u) failed (sense %06x)", first_audio_lba, sense);
    }
    cdscsi_close(&p);
}

static int probe_thread(void *unused)
{
    (void)unused;
    P("start");
    int xpt = open("/dev/xpt0", O_RDWR);
    P("open /dev/xpt0 O_RDWR: %s", xpt >= 0 ? "ok" : strerror(errno));
    if (xpt >= 0)
        close(xpt);
    int found = 0;
    for (unsigned i = 0; i < 8; ++i)
    {
        char path[24];
        struct stat st;
        snprintf(path, sizeof(path), "/dev/cd%u", i);
        if (stat(path, &st) != 0)
            continue;
        int fd = open(path, O_RDONLY);
        P("%s: %s, open: %s", path, S_ISCHR(st.st_mode) ? "character device" : "not a character device",
          fd >= 0 ? "ok" : strerror(errno));
        if (fd >= 0)
            close(fd);
        ++found;
        probe_drive(i);
    }
    if (!found)
        P("no /dev/cd0..7: no CD drive is plugged in, or the app cannot see it");
    P("end");
    SDL_AtomicSet(&state, 2);
    return 0;
}

bool discprobe_start(void)
{
    if (SDL_AtomicGet(&state) == 1)
        return false;
    SDL_AtomicSet(&state, 1);
    SDL_Thread *t = SDL_CreateThread(probe_thread, "discprobe", NULL);
    if (!t)
    {
        SDL_AtomicSet(&state, 2);
        return false;
    }
    SDL_DetachThread(t);
    return true;
}
