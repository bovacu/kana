// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_HEADER
#define FUDE_HEADER

#include "rde.h"
#include "study/chars/glyph.h"

// ===========================================================================
// Header: a study screen's header, and its badges. A badge is a character drawn from its own strokes in the accent
// colour, on the accent's tint: the screen's own mark (試 for the exams, 語 for the words), or a line's label that is
// itself in the language taught (音 訓 部 似 記).
//
//   fude_header_draw    the badge, the title, a caption under it, and a count at the right
//   fude_header_title   a screen's large title alone (Statistics, the album, Check)
//
// Where it sits: the study layer's widgets (study/widgets), drawn by the study screens (the viewer, the exams,
// Statistics, the album, Check, the Vocabulary and its word exam, Into Japanese, Text from a photo).
// Its main types and their lifecycle: none; it only draws, with the caller's glyph cache (glyph.h) and font.
// Threads: call it on the app's main thread, while a frame is being drawn.
// Files: none.
// Design: a badge's character is drawn from its strokes, as glyph.h draws the language's text (Kana has no Japanese
// font); a character the data has no strokes for leaves the badge's tint empty.
// ===========================================================================

#define FUDE_HEADER_BADGE      40.0f    // a header's badge
#define FUDE_HEADER_LABEL      32.0f    // a line's label
#define FUDE_HEADER_TEXT_LEFT  54.0f    // the title and caption, this far from the left (past the badge)
#define FUDE_HEADER_TITLE_PX   17.0f
#define FUDE_HEADER_CAPTION_PX 10.0f

// Draws a badge of _codepoint, a square of side _box with its top-left at _tl.
void fude_header_badge(fude_glyph* _glyph, u32 _codepoint, rde_vec_2F _tl, f32 _box);
// Draws a line's label (a badge FUDE_HEADER_LABEL across) of _codepoint, from _left, centred on _mid.
void fude_header_label(fude_glyph* _glyph, u32 _codepoint, f32 _left, f32 _mid);
// Draws a header from _left to _right, its top at _top: the badge of _codepoint, then _title, with _caption under it
// (NULL: none), each shrunk to fit before _right_text (NULL: none), which goes at the right.
void fude_header_draw(fude_glyph* _glyph, u32 _codepoint, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top,
                      const c8* _title, const c8* _caption, const c8* _right_text);
// Draws a large title alone, its line's top at _top.
void fude_header_title(rde_font* _font, f32 _font_px, const c8* _title, f32 _left, f32 _top);

#endif
