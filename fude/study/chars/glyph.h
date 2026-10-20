// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_GLYPH
#define FUDE_GLYPH

#include "rde.h"
#include "study/chars/kanji.h"

// ===========================================================================
// Glyph: draws characters from their strokes (kanji.h): a whole character, part of one stroke (the viewer's animation),
// a character writing itself, or a line of kana. KanjiVG has every kana, so Japanese text is drawn as handwriting, with
// no Japanese font needed.
//
// Where it sits: the study layer's character data (study/chars). The study screens (the viewer, Browse, the chart,
// Practice, the exams, Check, the album, Statistics, the Vocabulary, Into Japanese) and the header widget
// (header.h) draw with it.
// Its main types and their lifecycle: each screen that draws characters owns a fude_glyph, made with
// fude_glyph_init and freed with fude_glyph_destroy; it only points to the character data, which must outlive it.
// The whole characters drawn are kept on the GPU for all of them (below): the shell counts the frames for them
// (fude_glyph_frame) and lets them go when the app is trimmed and before the data is unloaded (fude_glyph_forget).
// Threads: call it on the app's main thread, while a frame is being drawn. A fude_glyph keeps scratch arrays, and the
// kept characters are the module's, so none of it may be used by two threads at once.
// Files: none.
// Design: positions are in screen space (centre origin, Y up), while KanjiVG's units run Y down, in a
// FUDE_KANJI_BOX-unit square whose top-left goes at _origin. A character is never mirrored: on a right-to-left
// screen its box goes where that screen puts it, but the strokes keep their own direction. A sign that is written
// on another letter, drawn alone, gets a dotted circle where that letter goes, as fonts show one.
//
// KEPT CHARACTERS. A whole character (fude_glyph_character, and the ghost under a character writing itself) is drawn
// from a mesh of all its strokes kept on the GPU (RDE's kept stroke meshes, rde_rendering_2d_keep_stroke_mesh), made
// the first time it is drawn and then only moved and coloured: a screen of characters drawn again is not tessellated
// again, nor sent to the GPU again. Browse's grid on a Galaxy Tab S6 Lite drew 364 strokes (18,777 vertices) every
// frame that way, about 2 ms of a big core's time and most of the 624 KB the frame uploaded. The key is everything the
// mesh's shape follows from: the character data (its pointer: the shell's one, loaded for the app's life), the
// character, its size on screen (which sets the pen's radius and how finely its curves are flattened: the stroke
// style) and the size of a target pixel (its soft edge is one pixel wide); not its place or colour, which each draw
// gives. So a kept character is never stale while its key is the same, and a new size, a display of another density
// or a zoom makes another. FUDE_GLYPH_KEPT are kept, the one drawn longest ago making room (rde_lru) — but never one
// drawn in the current frame: a screen with more characters than that draws the rest from a mesh made each time, as
// before, instead of making and dropping kept ones every frame. docs/architecture.md, "Kept characters", has the
// numbers.
// ===========================================================================

#define FUDE_GLYPH_TOLERANCE  0.25f     // KanjiVG units: the precision of flattening (and what the finest drawing uses)
// Drawing flattens to this many SCREEN units instead, when that is coarser. A 14-unit kana in a caption, flattened at
// FUDE_GLYPH_TOLERANCE, has points a fifth of a pixel apart, and every point costs the stroke tessellator a section
// of vertices.
#define FUDE_GLYPH_SCREEN_TOLERANCE 0.2f
#define FUDE_GLYPH_MAX_POINTS 1024u     // per stroke (the most any stroke has is under 100)
#define FUDE_GLYPH_WIDTH      3.2f      // stroke width, KanjiVG units (its own drawings use 3)
// How many whole characters are kept on the GPU (see Design): more than any screen draws at once (Browse's grid with
// its readings is under 200 on a 13-inch iPad), so going from screen to screen keeps most of them; at a few KB each
// (12 bytes a vertex, 2 an index), a few MB in all.
#define FUDE_GLYPH_KEPT       512u

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
// Draws a whole character, _size screen units square, from its kept mesh (see Design: made now the first time).
// Returns false, drawing nothing, when the data has no strokes for it.
b8   fude_glyph_character(fude_glyph* _glyph, u32 _codepoint, rde_vec_2F _origin, f32 _size, rde_color _color);

// Counts a frame, so the kept characters a frame draws are never let go during it (see Design). The shell calls it
// once a frame, before drawing. Without it the characters still draw right, but once FUDE_GLYPH_KEPT are kept the
// others are made again each time they are drawn.
void fude_glyph_frame(void);
// Lets every kept character go (their GPU memory with them) and frees the meshes they were made in. The shell calls
// it when the app is trimmed (in the background, or short of memory: what is drawn next is made again) and before it
// unloads the character data, whose pointer is part of every key.
void fude_glyph_forget(void);

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
