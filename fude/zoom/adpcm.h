// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_ADPCM
#define FUDE_ZOOM_ADPCM

#include "rde.h"

// ===========================================================================
// IMA ADPCM, mono (0.1.93: a canvas's recordings, audio.h): 16-bit samples as 4
// bits each, in blocks as a WAV file's (format 0x11) — each block on its own: its
// first sample and step index in a 4-byte header, then two samples a byte, the
// first in the low nibble. FUDE_ZOOM_ADPCM_BLOCK bytes hold
// FUDE_ZOOM_ADPCM_FRAMES samples, so a block can be found and decoded by its
// number alone (a seek: no decoding from the start).
// ===========================================================================

#define FUDE_ZOOM_ADPCM_BLOCK  256u                                    // a block's bytes
#define FUDE_ZOOM_ADPCM_FRAMES (1u + (FUDE_ZOOM_ADPCM_BLOCK - 4u) * 2u)   // its samples: 505

typedef struct {
    i32 index;   // the step index carried from block to block (where the next block starts)
} fude_zoom_adpcm_state;

// _frames samples (1 .. FUDE_ZOOM_ADPCM_FRAMES; fewer: the rest of the block the last one held) into a block _out
// (FUDE_ZOOM_ADPCM_BLOCK bytes).
void fude_zoom_adpcm_encode(fude_zoom_adpcm_state* _state, const i16* _in, u32 _frames, u8* _out);
// A block back into FUDE_ZOOM_ADPCM_FRAMES samples.
void fude_zoom_adpcm_decode(const u8* _in, i16* _out);

#endif
