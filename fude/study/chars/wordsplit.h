// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_WORDSPLIT
#define FUDE_WORDSPLIT

#include "rde.h"
#include "lang/lang.h"
#include "study/chars/kanji.h"

// ===========================================================================
// Word split: finds the words of a line of text in the language an app teaches, from the app's own dictionary
// (kanji.h 'WORD': each word with its reading and meaning), with no machine learning and no network. Text from a
// photo, the page read as text, Translate and the word card use it, so that each word of a line can be added to the
// learner's vocabulary (vocab.h).
//
// Where it sits: the study layer's character data (study/chars). The viewer, Text from a photo and Into Japanese
// screens, the page text and word card widgets, and the developer's look flags call it; each language's matcher
// (lang.h: fude_lang_words_def.match) calls fude_wordsplit_find back while a split runs.
// Its main types and their lifecycle: the index (every word's written form, compared as the language compares
// them) is an rde_hash_map in this module's statics, built the first time a database is split (tens of thousands
// of words: 2 to 4 MB, a few ms), built again when another database is split, and kept until the process ends. It
// is of one database, known by its address: an app loads its one at start and keeps it.
// Threads: call it on the app's main thread only; the index and its scratch are statics.
// Files: none; it reads the loaded character data.
// Design: it works the same for every language, and what differs is the language's (lang.h, fude_lang_words):
//   - The line is made ready (fude_lang_line): its lines are joined, and each character is written as the language
//     compares it (Arabic: without its vowel marks).
//   - From the left, the longest stretch that is a word of the dictionary is taken, and the search goes on after
//     it. This serves Chinese, Thai, Hindi and Arabic, whose words are written as they are (Thai and Hindi never end
//     one inside a syllable). A language that inflects finds the word at each place itself, with
//     fude_wordsplit_find: Japanese undoes a conjugation (食べました: 食べる), and Korean takes a chunk's longest
//     start (학교에서: 학교).
//   - A word spelt the same as others that are as common (行: xíng, háng; 本: ほん, もと) brings them too, as many as
//     the language says (fude_lang_words_def.alike): the data cannot tell them apart, but the learner can.
// The index keys each written form by its bytes, compared whole, so two words are spelt alike only when their forms
// are equal, never merely because their hashes are.
// ===========================================================================

#define FUDE_WORDSPLIT_MAX 12u   // words of one line, at most

// Finds the words of _text (UTF-8): their word numbers (fude_kanji_word_at) go into _out, at most _max, in order and
// each once. Returns how many: 0 when _text is NULL or _db is NULL or has no words.
u32 fude_wordsplit(const fude_kanji_db* _db, const c8* _text, u32* _out, u32 _max);

// Returns the commonest word written _written (the whole of it, compared as the language compares:
// fude_lang_words_def.fold), or UINT32_MAX when the dictionary has no such word.
u32 fude_wordsplit_word(const fude_kanji_db* _db, const c8* _written);

// For a language's matcher (fude_lang_words_def.match), while fude_wordsplit runs: returns the commonest word
// written as the _bytes bytes at _key (already written as the language compares them), or UINT32_MAX when there is
// none. It needs the index, so it finds nothing before the first split.
u32 fude_wordsplit_find(const c8* _key, usize _bytes);

#endif
