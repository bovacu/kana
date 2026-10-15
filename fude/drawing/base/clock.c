// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/clock.h"

// ===========================================================================
// See clock.h.
//
// The frames' clock is kept as a whole number of frames and turned into seconds only when it is read, so the time it
// gives is exact and identical on every run (adding 1/60 to a float each frame would drift).
// ===========================================================================

// The length of one frame of the frames' clock, in seconds: the look harness counts 60 frames a second.
#define FUDE_CLOCK_FRAME_SECONDS (1.0 / 60.0)

// True once the look harness has switched the clock to counting frames; it never goes back to false.
RDE_INTERNAL b8  fude_clock_frames_on = false;
// How many frames have passed since the clock started counting frames.
RDE_INTERNAL u64 fude_clock_frames    = 0;

// Returns the time in seconds: the frame count turned into seconds while counting frames, the engine's clock otherwise.
f64 fude_clock_now(void) {
    return fude_clock_frames_on ? (f64)fude_clock_frames * FUDE_CLOCK_FRAME_SECONDS : rde_engine_get_time_now();
}

// Switches the clock to counting frames, from 0, for the rest of the run.
void fude_clock_use_frames(void) {
    fude_clock_frames_on = true;
}

// Advances the frames' clock by one frame; does nothing while the clock follows the engine's time.
void fude_clock_frame(void) {
    if(fude_clock_frames_on) {
        fude_clock_frames++;
    }
}
