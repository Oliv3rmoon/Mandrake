/*
 * SwanStationPS5 - probe of the USB CD drive (Phase 0 of reading PS1 discs): what the app may open under /dev,
 * and what the drive answers. Read-only; the result goes to SwanStationPS5.log.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef SwanStationPS5_DISCPROBE_H
#define SwanStationPS5_DISCPROBE_H

#include "SwanStationPS5.h"

/* Starts the probe in a background thread; false if one is already running. */
bool discprobe_start(void);
/* 0 not started, 1 running, 2 finished (also after a failure: the log says why) */
int discprobe_state(void);

#endif
