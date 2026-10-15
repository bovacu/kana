// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_KO_HANGUL_H
#define FUDE_KO_HANGUL_H

#include "rde.h"

// ===========================================================================
// Hangul: Korean's letters and syllables as Unicode arranges them. A syllable is one code point made of its letters,
// FUDE_KO_SYLLABLE_FIRST + (initial × 21 + medial) × 28 + final, where a final of 0 means none. A letter written on its
// own is a compatibility jamo.
//
// Where it sits: it is Korean's own, in the language layer (fude/lang/ko), and only Korean's files use it: lang.c (its
// groups, the letters' Latin names, romanization typed back into Hangul), wordsplit.c, chart.c and bake.c.
//
// Threads: the app calls it on its main thread. The functions are only arithmetic, so any thread could call them.
//
// Design: letters are numbered in Unicode's own order, so a syllable is put together and taken apart by arithmetic
// alone, with no table.
// ===========================================================================

#define FUDE_KO_SYLLABLE_FIRST 0xAC00u   // 가, the first syllable.
#define FUDE_KO_SYLLABLE_LAST  0xD7A3u   // 힣, the last syllable.
#define FUDE_KO_JAMO_FIRST     0x3131u   // ㄱ, the first compatibility jamo, which is how text writes a letter alone.
#define FUDE_KO_JAMO_LAST      0x3163u   // ㅣ, the last compatibility jamo.
// How many initials (consonants), medials (vowels) and finals Unicode's syllables are made of.
#define FUDE_KO_INITIALS       19u
#define FUDE_KO_MEDIALS        21u
#define FUDE_KO_FINALS         28u       // Final 0, the first, means the syllable has none.

// Whether _cp is a syllable (가..힣).
b8   fude_ko_is_syllable(u32 _cp);
// Returns the syllable made of an initial, a medial and a final (0 for none), each given by its number in Unicode's
// order. The numbers are not checked, so each must be below FUDE_KO_INITIALS, FUDE_KO_MEDIALS or FUDE_KO_FINALS.
u32  fude_ko_compose(u32 _initial, u32 _medial, u32 _final);
// Takes a syllable apart into its letters, each given by its number in Unicode's order (*_final is 0 when it has
// none). _syllable is not checked, so it must be a syllable (fude_ko_is_syllable).
void fude_ko_decompose(u32 _syllable, u32* _initial, u32* _medial, u32* _final);

#endif
