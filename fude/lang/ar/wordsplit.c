// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"

// ===========================================================================
// See lang.h. Arabic's words (fude_lang_words). Arabic puts spaces between words, but its little words join the next
// one (و "and", ب "with", ل "for", ال "the": والكتاب). The study's splitter takes, from the left, the longest stretch
// that is a word (study/chars/wordsplit.h), so those split off. Words and text are both compared bare, without their
// vowel marks (harakat) or tatweel and with أ إ آ ٱ as ا, because most text is written without them while the
// dictionary's words keep theirs.
// ===========================================================================

// Returns whether _cp is a letter a word is looked up for: the Arabic letters (U+0621..U+064A), the extended ones
// (U+0671..U+06D3) and the presentation forms (U+FE70..U+FEFC).
RDE_INTERNAL b8 fude_ar_letter(u32 _cp) {
    return (_cp >= 0x0621u && _cp <= 0x064Au) || (_cp >= 0x0671u && _cp <= 0x06D3u) || (_cp >= 0xFE70u && _cp <= 0xFEFCu);   // Letters and their forms.
}

// Returns what _cp is compared as: 0 (left out) for a vowel mark, the superscript alif or tatweel; a bare alif (ا) for
// an alif with a hamza or a madda, or alif wasla; any other character as it is.
RDE_INTERNAL u32 fude_ar_bare(u32 _cp) {
    if((_cp >= 0x064Bu && _cp <= 0x065Fu) || _cp == 0x0670u || _cp == 0x0640u) {
        return 0u;   // A vowel mark, the superscript alif or tatweel is left out.
    }
    if(_cp == 0x0622u || _cp == 0x0623u || _cp == 0x0625u || _cp == 0x0671u) {
        return 0x0627u;   // آ أ إ ٱ are compared as ا.
    }
    return _cp;
}

// Returns Arabic's rules for finding words, a static table. Up to 3 words written alike are offered.
const fude_lang_words_def* fude_lang_words(void) {
    static const fude_lang_words_def _def = {
        .letter = fude_ar_letter,
        .fold   = fude_ar_bare,
        .look   = 20u,   // Letters; the vowel marks are left out and do not count.
        .alike  = 3u,
    };
    return &_def;
}
