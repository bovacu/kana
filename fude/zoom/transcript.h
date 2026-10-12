// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_TRANSCRIPT
#define FUDE_ZOOM_TRANSCRIPT

#include "rde.h"

// ===========================================================================
// WHAT A RECORDING SAID (0.1.95, listen.h): its phrases, each with when it was
// said, kept beside it (<canvas>.rec<n>.text) — written out on the page as a text
// or as handwriting, each line at the moment its first words were said (audio.h's
// links: the ink comes as it was spoken).
//
//   header  "KANA" u32 version u32 kind 'TRNS'
//   u8      its language's length, then it ("en", "es"…: whisper's; none: not known)
//   u32     the recording's samples read so far; u8 all of it (1) or not yet (0)
//   u32     phrases, each: u32 from (ms), u32 to (ms), u32 bytes, the bytes (UTF-8)
//
// It is read in pieces of up to 30 s (whisper's window), each cut where it is
// quietest near its end (fude_zoom_transcript_cut), so a word is never cut in two.
// ===========================================================================

typedef struct {
    u32 t0_ms, t1_ms;   // when it was said, into the recording
    u32 at, len;        // its words in the transcript's text
} fude_zoom_transcript_segment;

typedef struct {
    rde_arr TYPE(fude_zoom_transcript_segment) segments;   // in order
    rde_arr TYPE(c8)                           text;
    c8                                         language[8];
    u32                                        done;        // the recording's samples read so far
    b8                                         complete;    // all of it
} fude_zoom_transcript;

// A line of the transcript as it is written out (fude_zoom_transcript_lines).
typedef struct {
    u32 at, len;   // in the text written out
    u32 t0_ms;     // when its first words were said
} fude_zoom_transcript_line;

void fude_zoom_transcript_init(fude_zoom_transcript* _t);
void fude_zoom_transcript_free(fude_zoom_transcript* _t);
void fude_zoom_transcript_clear(fude_zoom_transcript* _t);

// A phrase as the recogniser gave it, from _t0_ms to _t1_ms: trimmed, and left out (false) when nothing is left or only
// a sound's tag ([BLANK_AUDIO], [Music], (laughs), *applause*…).
b8   fude_zoom_transcript_add(fude_zoom_transcript* _t, u32 _t0_ms, u32 _t1_ms, const c8* _words);
// Every phrase of _more added to _t (their times as they are).
void fude_zoom_transcript_append(fude_zoom_transcript* _t, const fude_zoom_transcript* _more);

// The whole of it as one text into _out (c8, NUL-ended, cleared first): its phrases one after another, a space between
// those said close together, a new paragraph (an empty line) after a pause of _pause_ms or more.
void fude_zoom_transcript_join(const fude_zoom_transcript* _t, u32 _pause_ms, rde_arr* _out);
// ...as lines of at most _width characters (broken at spaces; a longer word on its own), a new paragraph after a pause
// as join's: their text into _out (c8, '\n' between, NUL-ended), each line's place and time into _lines. How many.
u32  fude_zoom_transcript_lines(const fude_zoom_transcript* _t, u32 _width, u32 _pause_ms, rde_arr* _out, rde_arr* _lines);
// Its last words (at most _most bytes, from a word's start): what the next piece is told came before it.
void fude_zoom_transcript_tail(const fude_zoom_transcript* _t, u32 _most, c8* _out, usize _size);

// Kept (false: not written); read back (false: none, or not one — then empty).
b8   fude_zoom_transcript_save(const fude_zoom_transcript* _t, const c8* _path);
b8   fude_zoom_transcript_load(fude_zoom_transcript* _t, const c8* _path);

// Where to end a piece of _n samples at _rate: at the quietest tenth of a second of its last _window_ms (so a word is
// not cut in two). _n itself when it is no longer than that window.
u32  fude_zoom_transcript_cut(const f32* _pcm, u32 _n, u32 _rate, u32 _window_ms);

#endif
