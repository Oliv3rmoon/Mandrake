/*
 * SwanStationPS5 - imports the PS1 disc in the USB CD drive: dumps it to <games>/<Title [serial]>/<Title [serial]>.bin
 * and .cue, which the library then picks up like any other game. Runs in a thread; the screen shows a bar.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef SwanStationPS5_DISCIMPORT_H
#define SwanStationPS5_DISCIMPORT_H

#include "SwanStationPS5.h"

typedef enum
{
    DI_IDLE,
    DI_RUNNING,
    DI_OK,          /* imported: the library has a new game */
    DI_OK_INCOMPLETE, /* imported, but some sectors could not be read and are zeros: discimport_bad() */
    DI_ALREADY,     /* it was already in the games folder */
    DI_NO_DRIVE,    /* no CD drive found, or the app may not open it */
    DI_NO_DISC,     /* the drive is empty or not ready */
    DI_NOT_PS1,     /* the first track is not data, or the disc has no ISO 9660 volume */
    DI_UNREADABLE,  /* too many sectors could not be read */
    DI_DRIVE_LOST,  /* the disc was taken out or the drive unplugged halfway */
    DI_WRITE_FAILED, /* the games folder could not be written */
    DI_STALLED      /* nothing was read for 30 seconds: stopped (discimport_check_stall) */
} DiscImportState;

/* Starts the import into `games_dir`; false if one is already running (or its result has not been taken). */
bool discimport_start(const char *games_dir);
/* DI_RUNNING while it works, then the result until discimport_ack(). */
DiscImportState discimport_state(void);
void discimport_ack(void);
int discimport_permille(void);
/* The game's name, once the disc has been identified; "" before. */
const char *discimport_title(void);
/* Call every frame while it runs: true (once) if it made no progress for 30 seconds and was given up. The state is
 * then DI_STALLED, whatever the thread is still blocked on; it deletes its partial files when it comes back. */
bool discimport_check_stall(void);
/* How many sectors could not be read (zeros in the image). */
int discimport_bad(void);
/* At start: removes what an interrupted import left (*.part in the games folder's subfolders). */
void discimport_cleanup(const char *games_dir);

#endif
