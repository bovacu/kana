#ifndef KANA_HEADER
#define KANA_HEADER

#include "rde.h"
#include "widgets/glyph.h"

// ===========================================================================
// A screen's header, and its badges: a character in the accent, written from its
// strokes on the accent's tint — the screen's own mark (試 exams, 語 words) or a
// line's label that is itself Japanese (音 訓 部 似 記).
//
//   kana_header_draw    the badge, the title, a caption under it, a count at the right
//   kana_header_title   a screen's large title alone (Statistics, the album, Check)
// ===========================================================================

#define KANA_HEADER_BADGE      40.0f    // a header's badge
#define KANA_HEADER_LABEL      32.0f    // a line's label
#define KANA_HEADER_TEXT_LEFT  54.0f    // the title and caption, this far from the left (past the badge)
#define KANA_HEADER_TITLE_PX   17.0f
#define KANA_HEADER_CAPTION_PX 10.0f

// A badge of side _box, its top-left at _tl.
void kana_header_badge(kana_glyph* _glyph, u32 _codepoint, rde_vec_2F _tl, f32 _box);
// A line's label, from _left, centred on _mid.
void kana_header_label(kana_glyph* _glyph, u32 _codepoint, f32 _left, f32 _mid);
// The badge (_codepoint), _title and _caption (NULL: none) after it — each
// shrunk to fit before _right_text (NULL: none), at the right.
void kana_header_draw(kana_glyph* _glyph, u32 _codepoint, rde_font* _font, f32 _font_px, f32 _left, f32 _right, f32 _top,
                      const c8* _title, const c8* _caption, const c8* _right_text);
// A large title, its line's top at _top.
void kana_header_title(rde_font* _font, f32 _font_px, const c8* _title, f32 _left, f32 _top);

#endif
