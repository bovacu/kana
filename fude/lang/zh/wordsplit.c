// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"

#include "drawing/base/utf8.h"

// ===========================================================================
// See lang.h. Chinese's words (fude_lang_words). Chinese does not inflect and puts no spaces between words, so words
// are looked up as they are written: the study's splitter takes, from the left, the longest stretch that is a word
// (study/chars/wordsplit.h). Only stretches with a hanzi are looked up, because the dictionary has no word without one.
// No character is folded and a word may end anywhere, so fold, ends and match are left NULL.
// ===========================================================================

// Returns Chinese's rules for finding words, a static table.
const fude_lang_words_def* fude_lang_words(void) {
    // Up to 10 characters are tried from each place, since Chinese words are short, and up to 3 words written alike
    // are offered (行 is xíng and háng).
    static const fude_lang_words_def _def = {
        .letter = fude_utf8_is_han,
        .look   = 10u,   // Characters; Chinese words are short.
        .alike  = 3u,
    };
    return &_def;
}
