// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_LISTEN
#define FUDE_ZOOM_LISTEN

#include "rde.h"
#include "zoom/transcript.h"

// ===========================================================================
// SPEECH TO TEXT (0.1.95): what a recording says, read on the device by
// whisper.cpp (MIT; tools/whisper/build.sh) with OpenAI's Whisper base model (MIT),
// downloaded the first time it is wanted (fetch.h) into the save folder and kept.
// Built in only where the app was built with -DFUDE_WHISPER (InfiNote, Sketching);
// elsewhere fude_listen_built is false and nothing is read.
//
// One piece of a recording at a time (16 kHz mono samples, at most 30 s: whisper's
// window), on a thread of its own: submitted, its progress polled, its phrases
// taken once it is done — their times already into the recording.
// ===========================================================================

#define FUDE_LISTEN_RATE        16000u   // the samples it reads (the recordings': audio.h)
#define FUDE_LISTEN_PIECE_MS    28000u   // a piece, at most (whisper reads 30 s at once)
#define FUDE_LISTEN_MODEL_FILE  "ggml-base-q5_1.bin"
#define FUDE_LISTEN_MODEL_URL   "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base-q5_1.bin"
#define FUDE_LISTEN_MODEL_BYTES 59707625ull

// Is it in this build?
b8   fude_listen_built(void);
// Where its model is kept (<save>models/ggml-base-q5_1.bin); is it there, whole?
void fude_listen_model_path(c8* _out, usize _size);
b8   fude_listen_model_ready(void);

// A piece to read (its samples copied): _offset_ms into its recording, in _language ("": found by itself), the words
// said just before it (_prompt, "" none: they help it go on), on _threads threads. _tag is the caller's (which
// recording). False: one is being read already, or it cannot be.
b8   fude_listen_submit(u32 _tag, const f32* _pcm, u32 _frames, u32 _offset_ms, const c8* _language, const c8* _prompt, u32 _threads);
// Is one being read? How far (0 .. 1)?
b8   fude_listen_busy(void);
f32  fude_listen_progress(void);
// How a piece went (fude_listen_take).
typedef enum {
    FUDE_LISTEN_OK = 0,
    FUDE_LISTEN_DAMAGED,   // the model there but not readable: to be downloaded again
    FUDE_LISTEN_CANNOT,    // it cannot be read on this device (no CPU code it can run; the reading failed)
} FUDE_LISTEN_;

// A piece read: its tag, its phrases added to _into (their times into the recording), the language it found into
// _language, *_failed how it went (FUDE_LISTEN_). False: none done.
b8   fude_listen_take(u32* _tag, fude_zoom_transcript* _into, c8* _language, usize _size, u8* _failed);
// The thread stopped, the model let go (the app going away).
void fude_listen_shutdown(void);

#endif
