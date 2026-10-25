// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/spoken.h"

// ===========================================================================
// See spoken.h.
// ===========================================================================

// How many voices play now.
RDE_INTERNAL u32                 fude_spoken_voices = 0u;
// The app's own category, kept while a voice plays.
RDE_INTERNAL RDE_AUDIO_CATEGORY_ fude_spoken_app    = RDE_AUDIO_CATEGORY_AMBIENT;

// See the header.
void fude_spoken_start(void) {
    if(fude_spoken_voices++ == 0u) {
        fude_spoken_app = rde_audio_get_category();
        rde_audio_set_category(RDE_AUDIO_CATEGORY_SPOKEN);
    }
}

// See the header.
void fude_spoken_end(void) {
    if(--fude_spoken_voices == 0u) {
        rde_audio_set_category(fude_spoken_app);
    }
}
