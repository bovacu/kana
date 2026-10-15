// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_AUDIO
#define FUDE_ZOOM_AUDIO

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Audio: a canvas's recordings (0.1.93: audio synced to ink, docs/product_split.md §4.4). It keeps what was said while
// the canvas was written on, beside it, and when each thing on the canvas was begun, as a time into the recording
// then running. So a tap on ink plays from when it was written, and a recording played shows the ink coming as it was
// written.
//
// Where it sits: zoom, the canvas apps' layer. The page keeps one fude_zoom_audio for the open canvas (page_audio.c);
// voice.h records and plays the recordings' sound, and adpcm.h encodes it.
// Its main types and their lifecycle: the page makes a fude_zoom_audio with fude_zoom_audio_init and frees it with
// fude_zoom_audio_free. fude_zoom_recording and fude_zoom_audio_link are plain values inside it.
// Threads: main thread only.
// Files: two kinds, both beside the canvas's own file.
//   <canvas>.audio      this list: the recordings (each one's number, when it began, its rate and length) and the
//                       links (an object's id, the recording, its time into it in milliseconds). It is a kfile.h file:
//                       header "KANA", u32 version, u32 kind 'AUDI'; u32 the next number; u32 the count of recordings,
//                       then each (u32 number, u32 started low half, u32 started high half, u32 rate, u32 frames);
//                       u32 the count of links, then each (u32 id low half, u32 id high half, u32 recording, u32 ms).
//   <canvas>.rec<n>     recording n's sound: IMA ADPCM blocks (adpcm.h), mono. The file is only ever added to, so a
//                       recording cut short by the app stopping keeps all its whole blocks, and its length is read
//                       from the file's size (fude_zoom_audio_frames_in).
// Design: see docs/infinite_canvas_design.md §74.
// ===========================================================================

// The sample rate recordings are made at: speech's. As ADPCM it takes a quarter of a phone call's bytes.
#define FUDE_ZOOM_AUDIO_RATE 16000u   // Samples a second.

// One recording.
typedef struct {
    u32 number;    // Its number, which names its file: <canvas>.rec<number>.
    u64 started;   // When it began, in Unix seconds.
    u32 rate;      // Samples a second.
    // 0 while it is still being written, or when the app stopped before it ended: its file's size says then.
    u32 frames;    // Its length in samples.
} fude_zoom_recording;

// When an object was begun, as a time into a recording.
typedef struct {
    fude_zoom_id object;      // A scene object's id.
    u32          recording;   // The number of the recording it is linked to.
    u32          at_ms;       // When the object was begun, in milliseconds into the recording.
} fude_zoom_audio_link;

// A canvas's recordings and links: what <canvas>.audio holds. The arrays are read directly; they are changed only
// through the calls below, which keep link_at in step with links.
typedef struct {
    rde_arr TYPE(fude_zoom_recording)  recordings;   // The oldest first.
    rde_arr TYPE(fude_zoom_audio_link) links;
    u32                                next;          // The number the next recording will get.
    // Where each linked object's link is in links, by the object's id: its newest, should it have more than one (only a
    // file could hold that), as searching from the newest found. Made again from links when they are read or a
    // recording is removed (which closes up the array), added to with each link; as many entries as linked objects.
    rde_hash_map TYPE(fude_zoom_id, u32) link_at;
} fude_zoom_audio;

// fude_zoom_audio_init makes an empty list (the next number 1); fude_zoom_audio_free frees its arrays.
// fude_zoom_audio_clear empties it again, for another canvas (the next number 1).
void fude_zoom_audio_init(fude_zoom_audio* _a);
void fude_zoom_audio_free(fude_zoom_audio* _a);
void fude_zoom_audio_clear(fude_zoom_audio* _a);
// fude_zoom_audio_save writes the list to _path (atomically, kfile.h); false when it could not be written.
// fude_zoom_audio_load reads it back; false when there is no file or it is not one (bad header, a rate out of
// 1..192000, cut short), and then the list is left empty. The next number is set past every recording read.
b8   fude_zoom_audio_save(const fude_zoom_audio* _a, const c8* _path);
b8   fude_zoom_audio_load(fude_zoom_audio* _a, const c8* _path);

// Adds a recording begun now (_started in Unix seconds, _rate samples a second) with no length yet. Returns its number.
u32  fude_zoom_audio_begin(fude_zoom_audio* _a, u64 _started, u32 _rate);
// Returns recording _number, or NULL when there is none. The pointer is valid until the list next changes.
fude_zoom_recording* fude_zoom_audio_find(fude_zoom_audio* _a, u32 _number);
// Removes recording _number from the list, with all its links. Its file is the caller's to delete.
void fude_zoom_audio_remove(fude_zoom_audio* _a, u32 _number);

// Links object _object to recording _number, begun _at_ms milliseconds into it. An object already linked keeps its
// first link: this one is ignored. One map lookup, however many links there are.
void fude_zoom_audio_add_link(fude_zoom_audio* _a, fude_zoom_id _object, u32 _number, u32 _at_ms);
// Finds object _object's link (its newest, should a file have given it more than one): its recording into *_number and
// its time into *_at_ms. Returns false when it has none. One map lookup.
b8   fude_zoom_audio_link_of(const fude_zoom_audio* _a, fude_zoom_id _object, u32* _number, u32* _at_ms);
// Returns how many objects are linked to recording _number.
u32  fude_zoom_audio_link_count(const fude_zoom_audio* _a, u32 _number);

// How long a stroke may take, from the pen going down to the step it makes, and still be linked to when the pen went
// down (seconds); a step made later than that after the last pen-down is linked to when it was made.
#define FUDE_ZOOM_AUDIO_DOWN_MOST 120.0
// Returns when a thing made at _now was begun, for its link (seconds, the clock _down, _started and _now share): when
// the pen last went down (_down), if that began it, else _now. The pen-down began it when it came after the recording
// started (_started), no earlier step was linked to it (_down_linked: the pen-down the last step was linked to; any
// time before _started when none was), and it is at most FUDE_ZOOM_AUDIO_DOWN_MOST before _now. So the stroke the pen
// drew is linked to when it began, and a text, a shape or a paste made after it to its own time, not the stroke's.
// The caller sets _down_linked to _down when the answer is _down.
f64  fude_zoom_audio_begun(f64 _down, f64 _down_linked, f64 _started, f64 _now);

// Returns a recording's length in samples from its file's size in bytes: its whole blocks only.
u32  fude_zoom_audio_frames_in(u64 _file_bytes);

#endif
