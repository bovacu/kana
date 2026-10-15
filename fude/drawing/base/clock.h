// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_CLOCK
#define FUDE_CLOCK

#include "rde.h"

// ===========================================================================
// Clock: the time at which everything a person sees changing is drawn (animations, stroke-order playback, guided
// practice's steps, fades), in seconds, only ever going forward.
//
// Where it sits: drawing/base. Every screen and widget that animates asks it for the time; the look harness
// (--shot, fude/drawing/app/look.h) switches it to counting frames.
// Threads: main thread only.
// Design: normally it is the engine's monotonic clock (rde_engine_get_time_now). While the look harness takes a
// screenshot it is the frame count at 60 frames a second instead, so a picture taken at frame N shows the same thing
// on every run. tools/check/shots.sh compares screens before and after a change byte for byte, and an animation drawn
// at whatever real time a frame happened to land on would defeat that.
// Whatever measures real elapsed time (performance numbers, a background job's progress, a timeout waiting on the
// network) calls rde_engine_get_time_now itself, because that time must stay real even while a screenshot is taken.
// ===========================================================================

// Returns the current time in seconds: the engine's clock, or the frames' clock while a screenshot is being taken.
f64  fude_clock_now(void);

// Called by the look harness: from now on the clock counts frames, starting at 0 (fude_clock_frame advances it by
// 1/60 s each frame). There is no switching back.
void fude_clock_use_frames(void);

// Marks that one frame has passed. The look harness calls it once a frame; it does nothing unless the clock counts
// frames.
void fude_clock_frame(void);

#endif
