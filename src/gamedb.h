/*
 * SwanStationPS5 - what DuckStation's game database knows about each disc (downloaded
 * once on the console, never part of SwanStationPS5).
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef SwanStationPS5_GAMEDB_H
#define SwanStationPS5_GAMEDB_H

#include "SwanStationPS5.h"

enum
{
    /* controllers it takes */
    GDB_GUNCON = 1u << 0,
    GDB_JUSTIFIER = 1u << 1,
    GDB_NEGCON = 1u << 2, /* mostly racing games */
    GDB_ANALOG = 1u << 3,
    GDB_MOUSE = 1u << 4,
    GDB_MULTITAP = 1u << 5,
    GDB_VIBRATION = 1u << 6,
    /* settings it's known to break with */
    GDB_NO_WIDESCREEN = 1u << 8,
    GDB_NO_PGXP = 1u << 9,
    GDB_PGXP_CPU = 1u << 10, /* PGXP needs CPU mode */
    GDB_NO_UPSCALING = 1u << 11,
    GDB_NO_TEXTURE_FILTER = 1u << 12,
    GDB_NO_SPRITE_FILTER = 1u << 13,
    GDB_DEINTERLACE = 1u << 14, /* needs deinterlacing */
    GDB_NO_MULTITAP = 1u << 15,
    GDB_NO_AUTO_ANALOG = 1u << 16, /* must start in digital mode */
    GDB_NO_CD_SPEEDUP = 1u << 17,  /* breaks with Fast CD loading */
    GDB_ICACHE = 1u << 18,         /* needs the recompiler's instruction cache */
    GDB_PGXP_VCACHE = 1u << 19,    /* PGXP needs the vertex cache */
    GDB_NO_PGXP_CULL = 1u << 20,   /* PGXP culling correction breaks it */
    GDB_NO_TRUE_COLOR = 1u << 21,
    GDB_SW_READBACKS = 1u << 22,   /* needs the software renderer for VRAM readbacks */
};
enum
{
    /* values of the database's own settings for a disc, in GameInfo.set */
    GDT_ACTIVE_START = 1u << 0,
    GDT_ACTIVE_END = 1u << 1,
    GDT_LINE_START = 1u << 2,
    GDT_DMA_SLICE = 1u << 3,
    GDT_DMA_HALT = 1u << 4,
    GDT_PGXP_TOLERANCE = 1u << 5,
    GDT_PGXP_PROJ = 1u << 6,
};
#define GDB_FIXES (GDB_NO_WIDESCREEN | GDB_NO_PGXP | GDB_PGXP_CPU | GDB_NO_UPSCALING | GDB_NO_TEXTURE_FILTER | \
                   GDB_NO_SPRITE_FILTER | GDB_DEINTERLACE | GDB_NO_MULTITAP | GDB_NO_CD_SPEEDUP | GDB_ICACHE | GDB_PGXP_VCACHE | \
                   GDB_NO_PGXP_CULL | GDB_NO_TRUE_COLOR | GDB_SW_READBACKS)

typedef struct
{
    char serial[16];
    char name[128], genre[64], developer[64], publisher[64];
    int year, min_players, max_players; /* 0 when unknown */
    unsigned flags;                     /* GDB_* */
    unsigned set;                       /* GDT_*: which of the values below the database gives */
    int active_start, active_end, line_start, dma_slice, dma_halt;
    float pgxp_tolerance;
    bool pgxp_proj;
} GameInfo;

/* The index the app keeps (the payload reads it too). */
#define GAMEDB_INDEX_FILE "gamedb.txt"

/* At start: downloads the database when there's no index yet (or it's a month
 * old), on a thread. */
void gamedb_start(void);
/* NULL when the disc isn't known (or the index isn't there yet). */
const GameInfo *gamedb_get(const char *serial);

#endif
