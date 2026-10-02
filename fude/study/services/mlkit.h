#ifndef FUDE_MLKIT
#define FUDE_MLKIT

#include "rde.h"
#include "drawing/ink/ink.h"

// ===========================================================================
// Google ML Kit Digital Ink Recognition — on iOS only; nothing elsewhere.
// (A trial, branch ml-kit: see tools/mlkit/setup.py for how it is built in.)
//
// It reads handwriting with a model trained on many writers: what a stroke
// sequence says, as text (its candidates, best first) — not which strokes are
// which character; segment.h's reading-as does that, and score.h the rest.
//
// The Japanese model (~20 MB) is downloaded the first time — a network once —
// then it works offline. Everything is asynchronous: ask, then poll each frame.
// ===========================================================================

typedef enum {
    FUDE_MLKIT_UNAVAILABLE = 0,     // not in this build (desktop, Android)
    FUDE_MLKIT_IDLE,                // not prepared yet
    FUDE_MLKIT_DOWNLOADING,         // the model is on its way
    FUDE_MLKIT_READY,
    FUDE_MLKIT_FAILED               // the model could not be had (fude_mlkit_prepare tries again)
} FUDE_MLKIT_;

FUDE_MLKIT_ fude_mlkit_state(void);

// The writer's choice (Settings, saved): off, ML Kit is never called — nothing is
// downloaded and nothing reaches Google — and Kana reads on its own. On by default.
void fude_mlkit_set_enabled(b8 _enabled);
b8   fude_mlkit_enabled(void);

// Gets the model ready: downloads it when missing (asynchronous).
void fude_mlkit_prepare(void);

// Starts reading the alive strokes of _ink (any units, Y up); _pre_context, the
// text written just before (may be NULL). False when not READY or already busy.
b8   fude_mlkit_recognize(const fude_ink* _ink, const c8* _pre_context);

// True once the answer is in (then again idle): its candidates, best first,
// one per line (UTF-8); how long it took in milliseconds. An empty answer is an
// answer too — nothing could be read.
b8   fude_mlkit_poll(c8* _out, usize _size, f64* _ms);

#endif
