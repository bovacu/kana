#ifndef FUDE_WORDSPLIT
#define FUDE_WORDSPLIT

#include "rde.h"
#include "study/chars/kanji.h"

// ===========================================================================
// The words in a line of Japanese, from Kana's own dictionary (kanji.h 'WORD':
// JMdict's common words with kanji, each with its reading and meaning) — for
// Text from a photo and Translate with Google, where each word of a line can be
// added to the learner's vocabulary (vocab.h). No ML, no network.
//
// Longest match first, from the left: at each character the longest stretch the
// dictionary has as a word (written form) — or as a verb's or an adjective's
// STEM followed by its conjugation (食べました: 食べる; 高かった: 高い). Only words
// with a kanji: particles and kana words are passed over (the learner reads
// those; the dictionary has none of them). Each word once a line.
// ===========================================================================

#define FUDE_WORDSPLIT_MAX 12u   // words of one line, at most

// The words of _text (UTF-8): word numbers into _out (fude_kanji_word_at), at
// most _max, in order. How many.
u32 fude_wordsplit(const fude_kanji_db* _db, const c8* _text, u32* _out, u32 _max);

// The kanji a word is filed under among the learner's words: the first of its
// written form the data has. 0 when none.
u32 fude_wordsplit_kanji(const fude_kanji_db* _db, const c8* _written);

#endif
