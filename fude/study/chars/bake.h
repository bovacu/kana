// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_STUDY_BAKE_H
#define FUDE_STUDY_BAKE_H

#include "rde.h"

// ===========================================================================
// The character data's bake, the part every language shares: what a study app's
// --bake (a DESKTOP mode of the app itself, fude/lang/<code>/bake.c) fills and
// writes into the file the app ships (format: kanji.h).
//
//   - strokes from a KanjiVG-form XML (KanjiVG itself; Hanzi's and Hangul's
//     are made in that form by their tools): a character per <kanji>, its
//     strokes the <path>s in "-sN" order, its parts every group's kvg:element;
//   - each character's readings, meanings and level, and its words, filled by
//     the language's own readers — or read from PREPARED tables (below);
//   - the example words each character keeps, chosen from the candidates;
//   - the file written, its strokes in it or in a file of their own (a licence
//     can ask for that: Hanzi's are the Arphic Public License's);
//   - the look-alikes, by the matcher (match.h) over the file just written.
//
// PREPARED tables (fude_bake_prepared), UTF-8, tab-separated, one record a line,
// '#' lines skipped — what a language's tools make from its sources:
//   chars.tsv     code point (hex) · grade · level · radical · frequency rank ·
//                 readings of kind 0 (joined by 、) · readings of kind 1 · English
//                 meanings (joined by ", ") · then a meaning per other language,
//                 in the bake's language order ("" for none)
//   words.tsv     written · reading · English meaning · a meaning per other
//                 language · how common (u16, lower the commoner) · common (0/1)
//                 · its sentence (a line of sentences.tsv, -1 none)
//   lists.tsv     code point (hex) · how many are examples · word lines (of
//                 words.tsv) best first
//   sentences.tsv the language's sentence · English · Spanish · French ·
//                 Portuguese ("" where none)
// A word not on any list is kept too (text is read with it: wordsplit.h).
// ===========================================================================

#if !defined(RDE_PLATFORM_MOBILE)

#include "drawing/base/kfile.h"
#include "study/chars/kanji.h"

#define FUDE_BAKE_MAX_STROKES   64u     // more than any character has (the most is ~30s)
#define FUDE_BAKE_MAX_SEGMENTS  255u    // a stroke's segment count is a u8

// Example words.
#define FUDE_BAKE_WORDS_PER     6u      // example words kept per character (shown)
#define FUDE_BAKE_WORDS_ALL     20u     // words kept per character in all: after the examples, more to add (the viewer's Add)
#define FUDE_BAKE_WORD_CHARS    5u      // longer words are not kept (the bake's word_chars, unless the language says)
#define FUDE_BAKE_EXAMPLE_CHARS 4u      // ...and examples are shorter still (example_chars)
#define FUDE_BAKE_WORD_CHARS_MAX 20u    // the most a language may keep: Thai's and Hindi's signs and marks count

// Meanings in other languages than English (kanji.h's 'LNxx'), at most.
#define FUDE_BAKE_LANGS 4u

typedef struct {
    u32 codepoint;
    u32 geometry;      // offset into the GEOM bytes
    u32 text;          // offset into the TEXT bytes, or UINT32_MAX
    u16 frequency;
    u8  strokes;
    u8  grade;
    u8  jlpt;          // Japanese's old JLPT level (kanji.h); 0 elsewhere
    u8  radical;
    u8  level;         // the language's level (lang.h)
    u32 parts;         // offset into the PART lists, or UINT32_MAX
    u32 words;         // offset into the WORD lists, or UINT32_MAX
    u32 meaning_in[FUDE_BAKE_LANGS];   // its meanings in another language: offset into that one's text, or UINT32_MAX
} fude_bake_char;

// A word that can be an example: its three strings.
typedef struct {
    c8  written[4u * FUDE_BAKE_WORD_CHARS_MAX + 1u];
    c8  reading[64];
    c8  meaning[128];
    c8  meaning_in[FUDE_BAKE_LANGS][128];   // in the other languages ("": none)
    u32 index;         // in the file, UINT32_MAX until a character keeps it (or, common, it is kept for reading text)
    u8  chars;
    b8  common;        // on a common list: can be an example
    u16 freq;          // how common: lower the commoner ('WFRQ')
} fude_bake_word;

// A word as a candidate example for one of its characters; lower scores first.
typedef struct {
    u32 codepoint;
    u32 word;
    i32 score;
} fude_bake_word_ref;

typedef struct {
    rde_arr TYPE(fude_bake_char) chars;
    // Words longer (in code points) are not kept, and examples are at most so
    // long: FUDE_BAKE_WORD_CHARS and FUDE_BAKE_EXAMPLE_CHARS after fude_bake_init,
    // more for a language whose vowel signs and marks are code points of their
    // own (Thai, Hindi: up to FUDE_BAKE_WORD_CHARS_MAX).
    u32                          word_chars;
    u32                          example_chars;
    fude_bytes                   geometry;
    fude_bytes                   text;
    fude_bytes                   parts;
    rde_arr TYPE(fude_bake_word)     words;
    rde_arr TYPE(fude_bake_word_ref) word_refs;
    fude_bytes                   word_lists;
    u32                          word_count;   // words kept (numbered in the file)
    fude_bytes                   word_text;
    fude_bytes                   word_freqs;   // a u16 per word kept, in number order ('WFRQ')
    // Example sentences ('SENT'): each kept once (the language's, then en, es,
    // fr, pt, NUL-terminated, "" when there is none), and each word's (or UINT32_MAX).
    fude_bytes                   sentences;
    u32                          sentence_count;
    u32*                         sentence_words;
    u32                          words_unlisted;   // common words kept on no character's list (for reading text)
    // The other languages ('LNxx', in this order): each one's code, its texts,
    // and its words' (word number, text offset) pairs, in number order.
    u32                          lang_count;
    c8                           lang_code[FUDE_BAKE_LANGS][3];
    fude_bytes                   lang_text[FUDE_BAKE_LANGS];
    fude_bytes                   lang_words[FUDE_BAKE_LANGS];
    u32                          lang_word_count[FUDE_BAKE_LANGS];
    u32                          lang_kanji_count[FUDE_BAKE_LANGS];

    // Report.
    u32 strokes;
    u32 segments;
    u32 skipped;            // characters dropped for an unparsable path
    u32 with_info;          // given readings and meanings
    u32 count_mismatch;     // the strokes and the dictionary disagree on the stroke count
    u32 level_listed[16];   // characters per level value (lang.h)
    u32 level_missing;      // listed, but without strokes
    u32 max_segments;
    u32 with_parts;         // characters with at least one part
    u32 part_refs;          // parts over all characters
    u32 word_entries;       // dictionary entries read
    u32 with_words;         // characters with at least one example word
    u32 example_refs;       // examples over all characters (the rest: to add)
    f32 min_coord;
    f32 max_coord;
} fude_bake;

// --- the bake --------------------------------------------------------------------------

// Empty, for meanings in these other languages (two-letter codes: "es", "pt"...).
void  fude_bake_init(fude_bake* _bake, const c8* const* _lang_codes, u32 _lang_count);
void  fude_bake_free(fude_bake* _bake);

// The strokes and parts of a KanjiVG-form XML file: a character each, sorted by
// code point. False when it cannot be read.
b8    fude_bake_kanjivg(fude_bake* _bake, const c8* _path);

// The prepared tables in _dir (see the top): readings, meanings, levels, words,
// their lists and sentences. False when chars.tsv is missing.
b8    fude_bake_prepared(fude_bake* _bake, const c8* _dir);

// Every candidate in (fude_bake_word_ref), each character keeps its best, the
// words kept are numbered — then every other common word, on no list.
void  fude_bake_choose_words(fude_bake* _bake);
// A word kept: numbered, its texts written.
void  fude_bake_number_word(fude_bake* _bake, fude_bake_word* _w);

// The file written to _out. With _strokes_out, the strokes go to a file of their
// own (kanji.h: FUDE_KANJI_STROKES_FILE beside it), headed by _notice — how and
// when they were changed, as a licence may ask. Its report on the log; _t0 the
// bake's start. False on failure.
b8    fude_bake_write(fude_bake* _bake, const c8* _out, const c8* _strokes_out, const c8* _notice, f64 _t0);

// The look-alikes, appended to the file at _path (which the matcher reads).
typedef struct {
    b8  (*script)(u32 _codepoint);                 // a script's letters (kana): compared only among themselves
    b8  (*common)(const fude_kanji_info* _info);   // worth look-alikes, and fit to be one
    f32 cost;                                      // the matcher's cost (a 100-unit box) at most
    f32 cost_script;                               // ...for a script's letters
    u32 strokes;                                   // strokes apart, at most
} fude_bake_look;
b8    fude_bake_lookalikes(const c8* _path, const fude_bake_look* _look);

// --- helpers ------------------------------------------------------------------------------

const c8*       fude_xml_attr(const rde_xml_entry* _e, const c8* _name);
b8              fude_xml_is(const rde_xml_entry* _e, const c8* _name);
const c8*       fude_xml_text(const rde_xml_entry* _e);
u32             fude_bake_decode_utf8(const c8* _s, u32* _len);
// A whole file on the standard heap (the sources are tens of MB: more than the
// engine's pools hold), freed with free(). NULL when it cannot be read.
u8*             fude_bake_slurp(const c8* _path, u32* _size);
// The value of "--name=value" (_prefix "--name="), _default when absent.
const c8*       fude_bake_arg(i32 _argc, c8** _argv, const c8* _prefix, const c8* _default);
// Appends _s to _out, after _sep unless it is the first; XML's five entities decoded.
void            fude_bake_join(c8* _out, usize _size, const c8* _sep, const c8* _s);
int             fude_bake_compare(const void* _a, const void* _b);   // fude_bake_chars by code point
fude_bake_char* fude_bake_find(fude_bake* _bake, u32 _cp);
b8              fude_bake_is_kanji(u32 _cp);   // a CJK ideograph (kanji, hanzi, hanja)
// How hard a character is to meet (by its school grade), for ranking words by
// their other characters.
i32             fude_bake_difficulty(const fude_bake_char* _c);
// Does _written hold a numeral other than _cp? (一月 二月 三月: one example, not four.)
b8              fude_bake_counts(const c8* _written, u32 _cp);

#endif

#endif
