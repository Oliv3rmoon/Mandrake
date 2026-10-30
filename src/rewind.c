/*
 * Mandrake - rewind's ring of save states (rewind.h).
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "rewind.h"

#include <stdlib.h>

void rewind_reset(Rewind *r)
{
    free(r->ring);
    r->ring = NULL;
    r->state_size = 0;
    r->head = r->filled = r->counter = 0;
}

bool rewind_record(Rewind *r, const RewindCore *core)
{
    if (++r->counter < REWIND_STEP)
        return true;
    r->counter = 0;
    if (!r->ring)
    {
        r->state_size = core->state_size();
        r->ring = r->state_size ? malloc(r->state_size * REWIND_SLOTS) : NULL;
        if (!r->ring)
        {
            r->state_size = 0;
            return false;
        }
    }
    if (core->save(r->ring + (size_t)r->head * r->state_size, r->state_size))
    {
        r->head = (r->head + 1) % REWIND_SLOTS;
        if (r->filled < REWIND_SLOTS)
            ++r->filled;
    }
    return true;
}

bool rewind_step(Rewind *r, const RewindCore *core)
{
    if (!r->ring || r->filled == 0)
        return false;
    r->head = (r->head + REWIND_SLOTS - 1) % REWIND_SLOTS;
    --r->filled;
    r->counter = 0;
    return core->load(r->ring + (size_t)r->head * r->state_size, r->state_size);
}
