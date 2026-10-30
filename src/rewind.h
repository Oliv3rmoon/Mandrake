/*
 * Mandrake - rewind's ring of save states, kept in memory: one every REWIND_STEP frames, the last REWIND_SLOTS of them
 * (about 8 seconds at 60 fps). Holding the touchpad and L2 steps back through them (main.c, play.c).
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * It works through the core's save states alone (the functions in RewindCore), so it is the same for SwanStation and
 * Beetle PSX, and the host check (tests/core/beetle_headless.c) runs it on Beetle PSX: recorded, stepped back and
 * played forward again, the frames must come out as they did the first time.
 */
#ifndef MANDRAKE_REWIND_H
#define MANDRAKE_REWIND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define REWIND_SLOTS 40
#define REWIND_STEP 12

/* The running core's save states. */
typedef struct
{
    size_t (*state_size)(void);                       /* what a state takes, 0 when there is no game */
    bool (*save)(void *buffer, size_t size);          /* the core's state into buffer */
    bool (*load)(const void *buffer, size_t size);    /* and back */
} RewindCore;

typedef struct
{
    uint8_t *ring;     /* REWIND_SLOTS states of state_size bytes, allocated with the first one */
    size_t state_size;
    int head;          /* the slot the next state goes in */
    int filled;        /* the states there are to go back to */
    int counter;       /* frames since the last state */
} Rewind;

/* Empty, its memory given back (another game, rewind turned off). */
void rewind_reset(Rewind *r);
/* After every frame of the game: every REWIND_STEP frames a state goes in the ring, over the oldest. False when there
 * is no memory for the ring (rewind then can't work); true otherwise, also when the state could not be taken. */
bool rewind_record(Rewind *r, const RewindCore *core);
/* One step back: the latest state is loaded and taken out of the ring. False when there is none left. */
bool rewind_step(Rewind *r, const RewindCore *core);

#endif
