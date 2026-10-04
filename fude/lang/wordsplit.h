// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_WORDSPLIT
#define FUDE_WORDSPLIT

#include "rde.h"
#include "study/chars/kanji.h"

// ===========================================================================
// The words in a line of the language an app teaches, from its own dictionary
// (kanji.h 'WORD': each word with its reading and meaning) — for Text from a
// photo and Translate with Google, where each word of a line can be added to the
// learner's vocabulary (vocab.h). No ML, no network. Each language has its own
// (fude/lang/<code>/wordsplit.c), as lang.h says.
//
// Japanese (fude/lang/ja/wordsplit.c): longest match first, from the left: at
// each character the longest stretch the dictionary has as a word (written
// form) — or as a verb's or an adjective's STEM followed by its conjugation
// (食べました: 食べる; 高かった: 高い). Only words with a kanji: particles and
// kana words are passed over (the learner reads those; the dictionary has none
// of them). Each word once a line.
// ===========================================================================

#define FUDE_WORDSPLIT_MAX 12u   // words of one line, at most

// The words of _text (UTF-8): word numbers into _out (fude_kanji_word_at), at
// most _max, in order. How many.
u32 fude_wordsplit(const fude_kanji_db* _db, const c8* _text, u32* _out, u32 _max);

// The character a word is filed under among the learner's words: the first of
// its written form the data has (Japanese: its first kanji). 0 when none.
u32 fude_wordsplit_kanji(const fude_kanji_db* _db, const c8* _written);

#endif
