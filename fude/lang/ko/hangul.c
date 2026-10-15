// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/ko/hangul.h"

// ===========================================================================
// See hangul.h. Every function is arithmetic on Unicode's order of the syllables: a syllable's number from
// FUDE_KO_SYLLABLE_FIRST is (initial × 21 + medial) × 28 + final.
// ===========================================================================

// Returns whether _cp is in the syllables' block, 가 (U+AC00) to 힣 (U+D7A3).
b8 fude_ko_is_syllable(u32 _cp) {
    return _cp >= FUDE_KO_SYLLABLE_FIRST && _cp <= FUDE_KO_SYLLABLE_LAST;
}

// Returns the syllable of an initial, a medial and a final (0 for none), each numbered in Unicode's order. The numbers
// are not checked.
u32 fude_ko_compose(u32 _initial, u32 _medial, u32 _final) {
    return FUDE_KO_SYLLABLE_FIRST + (_initial * FUDE_KO_MEDIALS + _medial) * FUDE_KO_FINALS + _final;
}

// Takes syllable _syllable apart into its initial, medial and final numbers, the inverse of fude_ko_compose. It must
// be a syllable; it is not checked.
void fude_ko_decompose(u32 _syllable, u32* _initial, u32* _medial, u32* _final) {
    const u32 _n = _syllable - FUDE_KO_SYLLABLE_FIRST;
    *_initial = _n / (FUDE_KO_MEDIALS * FUDE_KO_FINALS);
    *_medial  = (_n / FUDE_KO_FINALS) % FUDE_KO_MEDIALS;
    *_final   = _n % FUDE_KO_FINALS;
}
