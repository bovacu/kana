// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_HAND_H
#define FUDE_ZOOM_HAND_H

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Text written out by hand: what is typed, as pen strokes in a joined-up
// script — Dr. A. V. Hershey's Script Simplex (zoom/hand_glyphs.h), a line a
// letter, the letters' own joins kept.
//
// Each letter a little uneven, as a hand's are: its slant, its size, where it
// sits on the line and how far on the next one starts, each varied a touch;
// every stroke smoothed through its points (the font's are straight between
// them) and wavering slightly along. Seeded: the same text and seed write the
// same way. A letter whose last stroke ends where the next one's first begins
// runs into it, one stroke (a word written without lifting the pen).
//
// Letters: ASCII, and the accented ones of Spanish, Portuguese and French (the
// letter and its mark drawn: á é í ó ú à è ù â ê î ô û ã õ ä ë ï ö ü ÿ ñ ç å),
// ¿ and ¡ (? and ! turned over), and typographic quotes and dashes as their
// plain ones. Anything else: a space's width. A new line: the next line down.
// ===========================================================================

// _text's strokes: their points into _points (fude_zoom_v2: x right, y up; a
// capital _cap tall, the first line's baseline along y = 0 from x = 0, each
// next line _cap × 1.9 lower) and each one's end into _ends (u32: how many
// points up to and with its last). How many strokes.
u32 fude_zoom_hand_write(const c8* _text, f64 _cap, u32 _seed, rde_arr* _points, rde_arr* _ends);

#endif
