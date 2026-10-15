// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"

// ===========================================================================
// See lang.h. Thai's words (fude_lang_words). Thai is written without spaces between words and does not inflect, so the
// study's splitter takes, from the left, the longest stretch that is a word (study/chars/wordsplit.h). A word never
// ends where a syllable goes on: a vowel sign or a tone mark that follows a consonant belongs to it, and a vowel
// written before its consonant (เ แ โ ใ ไ) starts the next syllable, so a word cannot end right after it.
// ===========================================================================

// Returns whether _cp is a letter a word is looked up for: any of the Thai block's letters and signs.
RDE_INTERNAL b8 fude_th_letter(u32 _cp) {
    return _cp >= 0x0E01u && _cp <= 0x0E5Bu;   // The Thai block's letters and signs.
}

// Returns whether a word may end between _last and _next: not after a vowel written before its consonant, and not
// before a sign that belongs to the letter before it.
RDE_INTERNAL b8 fude_th_ends(u32 _last, u32 _next) {
    // After a vowel written before the consonant it goes with (เ แ โ ใ ไ), the word goes on.
    if(_last >= 0x0E40u && _last <= 0x0E44u) {
        return false;
    }
    // Before a vowel sign above or below, sara am (ำ), lakkhang yao (ๅ) or a tone mark, the letter before is not the
    // word's last.
    return !(_next == 0x0E31u || _next == 0x0E33u || (_next >= 0x0E34u && _next <= 0x0E3Au) || _next == 0x0E45u ||
             (_next >= 0x0E47u && _next <= 0x0E4Eu));
}

// Returns Thai's rules for finding words, a static table. Up to 3 words written alike are offered.
const fude_lang_words_def* fude_lang_words(void) {
    static const fude_lang_words_def _def = {
        .letter = fude_th_letter,
        .ends   = fude_th_ends,
        .look   = 24u,   // Code points, so the signs and marks count too.
        .alike  = 3u,
    };
    return &_def;
}
