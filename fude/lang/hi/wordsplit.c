// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"

// ===========================================================================
// See lang.h. Hindi's words (fude_lang_words). Hindi puts spaces between words, but its postpositions and compounds can
// join them, so the study's splitter takes, from the left, the longest stretch that is a word
// (study/chars/wordsplit.h). A word never ends where a syllable goes on: a vowel sign, ं ँ ः ़ or a virama that follows
// a consonant belongs to it, and a consonant followed by a virama is the first half of a conjunct.
// ===========================================================================

// Returns whether _cp is a letter a word is looked up for: anything in the Devanagari block.
RDE_INTERNAL b8 fude_hi_letter(u32 _cp) {
    return _cp >= 0x0900u && _cp <= 0x097Fu;   // The Devanagari block.
}

// Returns whether a word may end between _last and _next: not after a virama, and not before a sign that belongs to
// the letter before it.
RDE_INTERNAL b8 fude_hi_ends(u32 _last, u32 _next) {
    // After a virama (्) the consonant before it is half a conjunct, so the word goes on.
    if(_last == 0x094Du) {
        return false;
    }
    // Before a vowel sign, a nasal sign (ँ ं ः), the nukta (़) or the virama, the letter before is not the word's last.
    // The avagraha (ऽ, U+093D) sits in that range but is a letter of its own, so it is let through.
    return !((_next >= 0x0900u && _next <= 0x0903u) || (_next >= 0x093Au && _next <= 0x094Fu && _next != 0x093Du) ||
             (_next >= 0x0951u && _next <= 0x0957u) || _next == 0x0962u || _next == 0x0963u);
}

// Returns Hindi's rules for finding words, a static table. Up to 3 words written alike are offered.
const fude_lang_words_def* fude_lang_words(void) {
    static const fude_lang_words_def _def = {
        .letter = fude_hi_letter,
        .ends   = fude_hi_ends,
        .look   = 24u,   // Code points, so the signs and marks count too.
        .alike  = 3u,
    };
    return &_def;
}
