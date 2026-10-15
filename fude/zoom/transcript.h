// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_TRANSCRIPT
#define FUDE_ZOOM_TRANSCRIPT

#include "rde.h"

// ===========================================================================
// Transcript: what a recording said (0.1.95, listen.h). Its phrases, each with when it was said, are kept beside the
// recording and can be written out on the page as a text or as handwriting, each line at the moment its first words
// were said, so with audio.h's links the ink comes as it was spoken.
//
// Where it sits: zoom, the canvas apps' layer. The page (page_speech.c) keeps one for each recording it reads; listen.c
// fills one with each piece it reads.
// Its main types and their lifecycle: the owner makes a fude_zoom_transcript with fude_zoom_transcript_init and frees
// it with fude_zoom_transcript_free; fude_zoom_transcript_clear empties it for reuse.
// Threads: no shared state. listen.c's reading thread fills its own transcript; the page's are used on the main
// thread. Never use one transcript on two threads at once.
// Files: <canvas>.rec<n>.text, beside the recording, in kfile.h's format:
//   header  "KANA", u32 version, u32 kind 'TRNS'
//   u8      the language code's length, then its bytes ("en", "es"...: whisper's; length 0 when not known)
//   u32     the recording's samples read so far; then u8 1 when all of it has been read, 0 when not yet
//   u32     the number of phrases, then each: u32 from (ms), u32 to (ms), u32 its byte count, its bytes (UTF-8)
// Design: a recording is read in pieces of up to 28 seconds (FUDE_LISTEN_PIECE_MS, inside whisper's 30-second window),
// each cut where it is quietest near its end (fude_zoom_transcript_cut), so a word is never cut in two.
// ===========================================================================

// One phrase.
typedef struct {
    u32 t0_ms, t1_ms;   // When it was said, in ms into the recording: from and to.
    u32 at, len;        // Its words in the transcript's text: the first byte and the byte count.
} fude_zoom_transcript_segment;

// A recording's transcript: its phrases, its language, and how much of the recording has been read.
typedef struct {
    rde_arr TYPE(fude_zoom_transcript_segment) segments;   // In the order they were said.
    // The phrases' words, one after another with nothing between them (not NUL-ended).
    rde_arr TYPE(c8)                           text;
    c8                                         language[8];
    u32                                        done;        // The recording's samples read so far.
    b8                                         complete;    // Whether all of the recording has been read.
} fude_zoom_transcript;

// A line of the transcript as it is written out (fude_zoom_transcript_lines).
typedef struct {
    u32 at, len;   // The line in the text written out: its first byte and byte count.
    u32 t0_ms;     // When its first words were said, in ms into the recording.
} fude_zoom_transcript_line;

// fude_zoom_transcript_init makes an empty transcript; fude_zoom_transcript_free frees its arrays;
// fude_zoom_transcript_clear empties it (no phrases, no language, nothing read) and keeps its arrays.
void fude_zoom_transcript_init(fude_zoom_transcript* _t);
void fude_zoom_transcript_free(fude_zoom_transcript* _t);
void fude_zoom_transcript_clear(fude_zoom_transcript* _t);

// Adds a phrase as the recogniser gave it, said from _t0_ms to _t1_ms, with the spaces round it trimmed. Returns false,
// adding nothing, when nothing is left or it is only a sound's tag ([BLANK_AUDIO], [Music], (laughs), *applause*, ♪).
b8   fude_zoom_transcript_add(fude_zoom_transcript* _t, u32 _t0_ms, u32 _t1_ms, const c8* _words);
// Adds every phrase of _more to _t, its times as they are.
void fude_zoom_transcript_append(fude_zoom_transcript* _t, const fude_zoom_transcript* _more);

// Writes the whole transcript as one text into _out (an rde_arr of c8, cleared first, NUL-ended): its phrases one
// after another, a space between those said close together, and an empty line (a new paragraph) after a pause of
// _pause_ms or more.
void fude_zoom_transcript_join(const fude_zoom_transcript* _t, u32 _pause_ms, rde_arr* _out);
// The same as lines of at most _width characters, broken at spaces (a longer word gets a line of its own), with a new
// paragraph after a pause as in fude_zoom_transcript_join. The text goes into _out (c8, '\n' between lines, NUL-ended)
// and each line's place and time into _lines (fude_zoom_transcript_line; a paragraph's empty line is one of length 0).
// Both are cleared first. Returns the number of lines.
u32  fude_zoom_transcript_lines(const fude_zoom_transcript* _t, u32 _width, u32 _pause_ms, rde_arr* _out, rde_arr* _lines);
// Writes its last words into _out (_size bytes, NUL-ended): at most _most bytes, starting at a word's start. This is
// what the next piece is told came before it (listen.h's _prompt).
void fude_zoom_transcript_tail(const fude_zoom_transcript* _t, u32 _most, c8* _out, usize _size);

// fude_zoom_transcript_save writes it to _path (atomically, kfile.h); false when it could not be written.
// fude_zoom_transcript_load reads it back; false when there is no file or it is not one, and then it is left empty.
b8   fude_zoom_transcript_save(const fude_zoom_transcript* _t, const c8* _path);
b8   fude_zoom_transcript_load(fude_zoom_transcript* _t, const c8* _path);

// Returns where to end a piece of _n samples at _rate samples a second: the middle of the quietest tenth of a second in
// its last _window_ms, so a word is not cut in two. Returns _n itself when the piece is no longer than that window.
u32  fude_zoom_transcript_cut(const f32* _pcm, u32 _n, u32 _rate, u32 _window_ms);

#endif
