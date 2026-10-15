// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_GLYPH
#define FUDE_GLYPH

#include "rde.h"
#include "study/chars/kanji.h"

// ===========================================================================
// Glyph: draws characters from their strokes (kanji.h): a whole character, part of one stroke (the viewer's
// animation), a character writing itself, or a line of kana. Kana has no Japanese font, and KanjiVG has every kana,
// so Japanese text is drawn as handwriting.
//
// Where it sits: the study layer's character data (study/chars). The study screens (the viewer, Browse, the chart,
// Practice, the exams, Check, the album, Statistics, the Vocabulary, Into Japanese) and the header widget
// (header.h) draw with it.
// Its main types and their lifecycle: each screen that draws characters owns a fude_glyph, made with
// fude_glyph_init and freed with fude_glyph_destroy; it only points to the character data, which must outlive it.
// Threads: call it on the app's main thread, while a frame is being drawn. A fude_glyph keeps scratch arrays, so one
// must not be used by two threads at once.
// Files: none.
// Design: positions are in screen space (centre origin, Y up), while KanjiVG's units run Y down, in a
// FUDE_KANJI_BOX-unit square whose top-left goes at _origin. A character is never mirrored: on a right-to-left
// screen its box goes where that screen puts it, but the strokes keep their own direction. A sign that is written
// on another letter, drawn alone, gets a dotted circle where that letter goes, as fonts show one.
// ===========================================================================

#define FUDE_GLYPH_TOLERANCE  0.25f     // KanjiVG units: the precision of flattening (and what the finest drawing uses)
// Drawing flattens to this many SCREEN units instead, when that is coarser. A 14-unit kana in a caption, flattened at
// FUDE_GLYPH_TOLERANCE, has points a fifth of a pixel apart, and every point costs the stroke tessellator a section
// of vertices.
#define FUDE_GLYPH_SCREEN_TOLERANCE 0.2f
#define FUDE_GLYPH_MAX_POINTS 1024u     // per stroke (the most any stroke has is under 100)
#define FUDE_GLYPH_WIDTH      3.2f      // stroke width, KanjiVG units (its own drawings use 3)

// The timing of a character writing itself, in seconds.
#define FUDE_GLYPH_STROKE_BASE 0.25     // every stroke takes at least this
#define FUDE_GLYPH_STROKE_PER  0.008    // ...plus this per KanjiVG unit of its length
#define FUDE_GLYPH_STROKE_GAP  0.15     // the pause between strokes

RDE_STRUCT {
    const fude_kanji_db*     db;
    rde_arr TYPE(rde_vec_2F) _points;
    rde_arr TYPE(f32)        _radii;
} fude_glyph;

// Sets up _glyph to draw the characters of _db (which may be NULL: then nothing is drawn); and frees its arrays.
void fude_glyph_init(fude_glyph* _glyph, const fude_kanji_db* _db);
void fude_glyph_destroy(fude_glyph* _glyph);

// Flattens a stroke into points (KanjiVG units) in _out, which must hold FUDE_GLYPH_MAX_POINTS; returns how many.
// _length (may be NULL) gets the stroke's length along the curve.
u32  fude_glyph_stroke_points(const fude_kanji_stroke* _stroke, rde_vec_2F* _out, f32* _length);

// Draws the first _fraction (0 to 1) of a stroke's length, at _scale screen units per KanjiVG unit, with a pen of
// radius _radius. Returns where the drawn part ends, on screen.
rde_vec_2F fude_glyph_stroke(fude_glyph* _glyph, const fude_kanji_stroke* _stroke, rde_vec_2F _origin,
                             f32 _scale, f32 _radius, f32 _fraction, rde_color _color);

// Sets the font for what the data has no strokes for (the app's, once it is loaded). It is one setting for the
// whole app, kept until it is set again.
void fude_glyph_set_text_font(rde_font* _font, f32 _font_px);
// Draws a whole character, _size screen units square. Returns false, drawing nothing, when the data has no strokes
// for it.
b8   fude_glyph_character(fude_glyph* _glyph, u32 _codepoint, rde_vec_2F _origin, f32 _size, rde_color _color);

// Draws a practice-sheet square: white, outlined, with dashed centre guides. _tl is its top-left corner.
void fude_glyph_box(rde_vec_2F _tl, f32 _size);

// Draws a character WRITING ITSELF, _elapsed seconds in: faint ghosts of every stroke first, then the ink stroke by
// stroke with a red pen tip, and each stroke's number at its start (text _number_px tall; 0 for none). Returns true
// once it has finished writing.
b8   fude_glyph_writing(fude_glyph* _glyph, const fude_kanji_info* _info, rde_vec_2F _tl, f32 _size, f64 _elapsed,
                        rde_font* _font, f32 _font_px, f32 _number_px);

// Draws a reading as the data writes it: kana, with "、" between readings, '.' before okurigana (drawn in _soft
// until the next reading) and '-' for an affix. Its characters are drawn from their strokes, and those the data has
// none for (pinyin's letters) in the text font (fude_glyph_set_text_font). It runs left to right even on a
// right-to-left screen, and stops before passing _max_x. Returns the x where it ended.
f32  fude_glyph_reading(fude_glyph* _glyph, const c8* _text, rde_vec_2F _origin, f32 _size, f32 _max_x, rde_color _ink, rde_color _soft);

#endif
