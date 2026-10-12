// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_AUDIO
#define FUDE_ZOOM_AUDIO

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// A CANVAS'S RECORDINGS (0.1.93: audio synced to ink, docs/product_split.md
// §4.4). What was said while the canvas was written on, kept beside it, and when
// each thing on it was begun into the recording then running — so a tap on ink
// plays from when it was written, and a recording played shows the ink coming as
// it was written.
//
//   <canvas>.audio      this: its recordings (each one's number, when it began,
//                       its rate and length) and the links (an object's id, the
//                       recording, its time into it in milliseconds) — header
//                       "KANA" u32 version u32 kind 'AUDI', u32 next number,
//                       u32 recordings and each (u32 number, u32 started low, high,
//                       u32 rate, u32 frames), u32 links and each (u32 id low, high,
//                       u32 recording, u32 ms)
//   <canvas>.rec<n>     recording n: IMA ADPCM blocks (adpcm.h), mono, only ever
//                       added to — one cut short by the app stopping is as long as
//                       its whole blocks (fude_zoom_audio_frames_in)
// ===========================================================================

#define FUDE_ZOOM_AUDIO_RATE 16000u   // samples a second: speech's, a quarter of a phone call's bytes as ADPCM

typedef struct {
    u32 number;    // its file: <canvas>.rec<number>
    u64 started;   // Unix seconds
    u32 rate;      // samples a second
    u32 frames;    // its samples (0: still being written, or cut short — its file says)
} fude_zoom_recording;

typedef struct {
    fude_zoom_id object;      // a scene object's id
    u32          recording;   // its number
    u32          at_ms;       // when the object was begun, into it
} fude_zoom_audio_link;

typedef struct {
    rde_arr TYPE(fude_zoom_recording)  recordings;   // oldest first
    rde_arr TYPE(fude_zoom_audio_link) links;
    u32                                next;          // the next one's number
} fude_zoom_audio;

void fude_zoom_audio_init(fude_zoom_audio* _a);
void fude_zoom_audio_free(fude_zoom_audio* _a);
// Kept (false: not written); read back (false: none, or not one — then empty).
b8   fude_zoom_audio_save(const fude_zoom_audio* _a, const c8* _path);
b8   fude_zoom_audio_load(fude_zoom_audio* _a, const c8* _path);

// A recording begun now (_started Unix seconds, _rate): its number.
u32  fude_zoom_audio_begin(fude_zoom_audio* _a, u64 _started, u32 _rate);
// Recording _number (NULL: none such).
fude_zoom_recording* fude_zoom_audio_find(fude_zoom_audio* _a, u32 _number);
// Recording _number taken out, its links with it.
void fude_zoom_audio_remove(fude_zoom_audio* _a, u32 _number);

// Object _object begun _at_ms into recording _number (an object linked again: its first link kept).
void fude_zoom_audio_add_link(fude_zoom_audio* _a, fude_zoom_id _object, u32 _number, u32 _at_ms);
// Object _object's link: its recording and time. False: none.
b8   fude_zoom_audio_link_of(const fude_zoom_audio* _a, fude_zoom_id _object, u32* _number, u32* _at_ms);
// How many things recording _number has linked.
u32  fude_zoom_audio_link_count(const fude_zoom_audio* _a, u32 _number);

// A recording's samples by its file's size (its whole blocks).
u32  fude_zoom_audio_frames_in(u64 _file_bytes);

#endif
