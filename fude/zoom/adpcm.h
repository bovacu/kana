// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_ADPCM
#define FUDE_ZOOM_ADPCM

#include "rde.h"

// ===========================================================================
// IMA ADPCM, mono: compresses 16-bit samples to 4 bits each, for a canvas's recordings (0.1.93; audio.h, voice.h).
//
// Where it sits: zoom, the canvas apps' layer. voice.c encodes as it records and decodes as it plays. The page
// decodes a recording to hand it to speech to text (page_speech.c), and encodes the recordings the developer's look
// harness makes up (page_look.c's fude_zoom_page_look_audio: a tone, or a WAV file).
// Its main types and their lifecycle: fude_zoom_adpcm_state is a plain value the caller keeps for one recording.
// Threads: none of its own, and no shared state: it may be called on any thread with the caller's own buffers.
// Files: the blocks are laid out as a WAV file's IMA ADPCM (format 0x11): each block starts with a 4-byte header
// (the first sample as a little-endian i16, then the step index, then a zero byte), followed by two samples a byte,
// the first in the low nibble. The recording files themselves are described in audio.h.
// Design: every block stands on its own, and FUDE_ZOOM_ADPCM_BLOCK bytes always hold FUDE_ZOOM_ADPCM_FRAMES samples, so
// a block can be found and decoded from its number alone: playing can seek anywhere without decoding from the start.
// ===========================================================================

#define FUDE_ZOOM_ADPCM_BLOCK  256u                                    // A block's size in bytes.
#define FUDE_ZOOM_ADPCM_FRAMES (1u + (FUDE_ZOOM_ADPCM_BLOCK - 4u) * 2u)   // The samples in a block: 505.

// An encoder's state, kept for one recording.
typedef struct {
    i32 index;   // The step index carried from one block to the next: where the next block starts.
} fude_zoom_adpcm_state;

// Encodes _frames samples from _in into one block at _out (FUDE_ZOOM_ADPCM_BLOCK bytes), and updates _state's step
// index for the next block. _frames is 1 to FUDE_ZOOM_ADPCM_FRAMES; with fewer, the last sample is held to fill the
// block, so its tail is quiet. With 0, the block is all zeros.
void fude_zoom_adpcm_encode(fude_zoom_adpcm_state* _state, const i16* _in, u32 _frames, u8* _out);
// Decodes one block at _in into FUDE_ZOOM_ADPCM_FRAMES samples at _out.
void fude_zoom_adpcm_decode(const u8* _in, i16* _out);

#endif
