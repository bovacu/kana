#ifndef KANA_NOTICE
#define KANA_NOTICE

#include "rde.h"

// ===========================================================================
// A notice: one line in a pill near the bottom of the screen, a moment, then it
// fades — what an action did (Copy as text: what it copied; a sheet: where it
// went; the hand: who writes now). One at a time: a new one replaces it.
// Drawn over whatever is on screen, over the row of buttons if one is there.
// ===========================================================================

void kana_notice_show(const c8* _text);
// Once a frame, last: the pill (nothing when none is showing). _above: how far
// over the bottom of the safe area it sits (clear of a screen's row).
void kana_notice_render(rde_window* _window, rde_font* _font, f32 _font_px, f32 _above);

#endif
