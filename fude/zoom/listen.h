// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_LISTEN
#define FUDE_ZOOM_LISTEN

#include "rde.h"
#include "zoom/transcript.h"

// ===========================================================================
// Listen: speech to text (0.1.95). It reads what a recording says, on the device, with whisper.cpp (MIT licence;
// built by tools/whisper/build.sh) and OpenAI's Whisper base model (MIT licence). The model is downloaded the first
// time it is wanted (fetch.h) into the save folder and kept there.
//
// Where it sits: zoom, the canvas apps' layer. The page (page_speech.c) cuts a recording into pieces, hands them in one
// at a time, polls once a frame, and keeps the phrases in the recording's transcript (transcript.h). Builds: it is only
// built in where the app was built with -DFUDE_WHISPER (InfiNote, Sketching). Elsewhere fude_listen_built returns false
// and nothing is read.
// Its main types and their lifecycle: none for the caller. The module keeps one piece at a time and the loaded model
// in its own statics; fude_listen_shutdown lets them go.
// Threads: call every function on the main thread. The reading runs on a thread of its own, started by the first
// fude_listen_submit: the piece is handed to it under a mutex, it reads (and loads the model the first time, which
// takes a second or two), then marks the piece done under the mutex for fude_listen_take to collect. How far a piece is
// goes in volatile numbers the thread writes as it reads and fude_listen_progress reads without the lock (each written
// whole by one thread, a moment old at worst); the guesses they are weighed by (what the encoding takes, its share)
// change as a piece ends, under the mutex, and are read under it.
// Files: reads the model, FUDE_LISTEN_MODEL_FILE in the downloads folder (save.h: fude_save_downloads_dir, the
// platform's cache, which no backup takes in and the system may empty: then it is downloaded again); writes nothing.
// Design: the page hands in pieces of 16 kHz mono samples at most FUDE_LISTEN_PIECE_MS long, because whisper reads 30
// seconds at a time; the phrases come back with their times already measured into the whole recording. Only one piece
// is read at a time, and a piece read must be taken before the next is handed in. The model is loaded once and kept
// until fude_listen_shutdown. On Android whisper's CPU code is a library for each set of CPU features, and the best
// one this CPU can run is loaded from beside libwhisper.so before the first reading (docs/infinite_canvas_design.md
// §75 and §76).
// ===========================================================================

#define FUDE_LISTEN_RATE        16000u   // The sample rate it reads: the recordings' own (audio.h).
#define FUDE_LISTEN_PIECE_MS    28000u   // A piece's length at most, in ms (whisper reads 30 s at once).
#define FUDE_LISTEN_MODEL_FILE  "ggml-base-q5_1.bin"
#define FUDE_LISTEN_MODEL_URL   "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base-q5_1.bin"
// The model file's exact size: a file of any other size is taken as not downloaded whole.
#define FUDE_LISTEN_MODEL_BYTES 59707625ull

// Returns whether speech to text is in this build.
b8   fude_listen_built(void);
// fude_listen_model_path writes where the model is kept (save.h: fude_save_downloads_dir, then ggml-base-q5_1.bin)
// into _out (_size bytes).
// fude_listen_model_ready returns whether the model is there and whole (exactly FUDE_LISTEN_MODEL_BYTES).
void fude_listen_model_path(c8* _out, usize _size);
b8   fude_listen_model_ready(void);

// Hands a piece to the reading thread; its _frames samples at _pcm are copied, so the caller may free them at once.
// _offset_ms: where the piece starts in its recording. _language: "en", "es"... or "" to let whisper find it. _prompt:
// the words said just before the piece, or "", which help it carry on (at most 511 bytes are kept). _threads: the CPU
// threads to read with (0 means 2). _tag is the caller's own, given back by fude_listen_take (the page passes the
// recording's number). Returns false when a piece is being read or waits to be taken, or there is nothing to read,
// no model, or no whisper in this build.
b8   fude_listen_submit(u32 _tag, const f32* _pcm, u32 _frames, u32 _offset_ms, const c8* _language, const c8* _prompt, u32 _threads);
// fude_listen_busy returns whether a piece is waiting, being read, or read and not taken yet. fude_listen_progress
// returns how far the piece being read is, 0 to 1: it never goes back within a piece, and shows 1 only once it is done.
b8   fude_listen_busy(void);
f32  fude_listen_progress(void);
// How a piece went (fude_listen_take).
typedef enum {
    FUDE_LISTEN_OK = 0,
    FUDE_LISTEN_DAMAGED,   // The model file is there but could not be loaded: it should be downloaded again.
    FUDE_LISTEN_CANNOT,    // It cannot be read on this device: no CPU code it can run, or the reading failed.
} FUDE_LISTEN_;

// Takes the piece read, if there is one: its tag into *_tag, how it went into *_failed (FUDE_LISTEN_), its phrases
// appended to _into (their times in ms into the recording) when it went well, and the language whisper found into
// _language (_size bytes; NULL to skip). Returns false when no piece is done yet. Taking it lets the next be submitted.
b8   fude_listen_take(u32* _tag, fude_zoom_transcript* _into, c8* _language, usize _size, u8* _failed);
// Stops the reading thread and lets go of the model, as the app goes away or before the model is downloaded again.
// It waits, on the calling thread, for a piece being read to finish.
void fude_listen_shutdown(void);

#endif
