// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_STUDY_BAKE_H
#define FUDE_STUDY_BAKE_H

#include "rde.h"

// ===========================================================================
// Bake: the part of the character data's bake that every language shares. A study app's --bake (a DESKTOP mode of
// the app itself, run by fude/lang/<code>/bake.c) fills a fude_bake and writes it into the file the app ships (its
// format is described in kanji.h):
//
//   - the strokes, from an XML file in KanjiVG's form (KanjiVG itself; Chinese and Korean ones are made in that form
//     by their tools): a character per <kanji>, its strokes the <path>s in "-sN" order, its parts every group's
//     kvg:element;
//   - each character's readings, meanings and level, and its words, filled in by the language's own readers, or
//     read from PREPARED tables (below);
//   - the example words each character keeps, chosen from the candidates;
//   - the file written, with its strokes in it or in a file of their own (a licence can ask for that: Chinese ones are
//     under the Arphic Public License);
//   - the look-alikes, found by the matcher (match.h) in the file just written.
//
// PREPARED tables (fude_bake_prepared) are what a language's tools make from its sources: UTF-8, tab-separated, one
// record a line, with '#' lines skipped.
//   chars.tsv     code point (hex) · grade · level · radical · frequency rank · readings of kind 0 (joined by 、) ·
//                 readings of kind 1 · English meanings (joined by ", ") · then a meaning per other language, in the
//                 bake's language order ("" for none)
//   words.tsv     written · reading · English meaning · a meaning per other language · how common (u16, lower for
//                 commoner words) · common (0 or 1) · its sentence (a line of sentences.tsv, -1 for none)
//   lists.tsv     code point (hex) · how many are examples · word lines (of words.tsv), best first
//   sentences.tsv the language's sentence · English · Spanish · French · Portuguese ("" where there is none)
// A common word that is on no list is kept too, because text is read with it (wordsplit.h).
//
// A language whose tools prepare its tables (Chinese, Korean, Thai, Hindi, Arabic) describes its bake
// (fude_bake_prepared_def) and runs it with fude_bake_run_prepared; Japanese reads its sources itself
// (lang/ja/bake.c).
//
// Where it sits: the study layer's character data (study/chars), used by the study shell (shell.h: --bake) and by
// each language's bake (lang/<code>/bake.c). The app itself never bakes on a phone or tablet: there only
// fude_bake_requested exists, and it is always false.
// Its main types and their lifecycle: a fude_bake lives for one bake, made by fude_bake_init and freed by
// fude_bake_free. The sources are parsed into memory on the standard heap (the XML trees are tens of MB, more than
// the engine's pools hold).
// Threads: the bake runs on the app's main thread, inside its init, with nothing else running; it uses large static
// buffers, so it is not for another thread.
// Files: it reads the stroke XML and the prepared tables, and writes the character data (FUDE_KANJI_FILE's format,
// kanji.h) and, for some languages, the strokes' own file beside it.
// ===========================================================================

// Returns true when --bake is on the command line. It is always false on a phone or tablet, which has no bake:
// there the language's fude_bake_run (lang.h) is this file's, and fails.
b8 fude_bake_requested(i32 _argc, c8** _argv);

// What a study app gives its bake (study/app/shell.h: fude_study_shell_def.bake), so that a language's bake
// (lang/<code>/bake.c) names no app: the app's own paths, from the project root where the bake runs, and the notice
// its changed strokes carry.
typedef struct fude_bake_app {
    const c8* config;           // its config ("apps/<app>/assets/config.rdef"): its memory sizes hold the sources whole
    const c8* out;              // where the character data goes unless --out= says (NULL: the language's default)
    const c8* tools;            // what makes the sources, named when they are missing (NULL: nothing named)
    // The notice heading the strokes' own file, beside the character data: where the strokes come from and how the
    // app's tools changed them, as their licence can ask (the Arphic Public License does). NULL keeps the strokes in
    // the character data.
    const c8* strokes_notice;
} fude_bake_app;

#if !defined(RDE_PLATFORM_MOBILE)

#include "drawing/base/kfile.h"
#include "study/chars/kanji.h"

#define FUDE_BAKE_MAX_STROKES   64u     // more than any character has (the most any has is about 30)
#define FUDE_BAKE_MAX_SEGMENTS  255u    // a stroke's segment count is a u8

// Example words. After a character's examples come more words to add (the viewer's Add), up to FUDE_BAKE_WORDS_ALL.
#define FUDE_BAKE_WORDS_PER     6u      // example words kept per character (shown)
#define FUDE_BAKE_WORDS_ALL     20u     // words kept per character in all, examples first
#define FUDE_BAKE_WORD_CHARS    5u      // longer words are not kept (the bake's word_chars, unless the language says)
#define FUDE_BAKE_EXAMPLE_CHARS 4u      // ...and examples are shorter still (example_chars)
#define FUDE_BAKE_WORD_CHARS_MAX 20u    // the most a language may keep: Thai's and Hindi's signs and marks count

// Meanings are kept in at most this many languages besides English (kanji.h's 'LNxx').
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

// A word that can be an example: its three strings, and what the choice of examples needs to know.
typedef struct {
    c8  written[4u * FUDE_BAKE_WORD_CHARS_MAX + 1u];
    c8  reading[64];
    c8  meaning[128];
    c8  meaning_in[FUDE_BAKE_LANGS][128];   // in the other languages ("": none)
    u32 index;         // its number in the file: UINT32_MAX until a character keeps it, or it is kept for reading text
    u8  chars;
    b8  common;        // on a list of common words, so it can be an example
    u16 freq;          // how common it is: lower for commoner words ('WFRQ')
} fude_bake_word;

// A word as a candidate example for one of its characters; lower scores come first.
typedef struct {
    u32 codepoint;
    u32 word;
    i32 score;
} fude_bake_word_ref;

typedef struct {
    rde_arr TYPE(fude_bake_char) chars;
    // Words longer than word_chars (in code points) are not kept, and examples are at most example_chars long.
    // fude_bake_init sets them to FUDE_BAKE_WORD_CHARS and FUDE_BAKE_EXAMPLE_CHARS; a language whose vowel signs and
    // marks are code points of their own (Thai, Hindi) sets more, up to FUDE_BAKE_WORD_CHARS_MAX.
    u32                          word_chars;
    u32                          example_chars;
    fude_bytes                   geometry;
    fude_bytes                   text;
    fude_bytes                   parts;
    rde_arr TYPE(fude_bake_word)     words;
    rde_arr TYPE(fude_bake_word_ref) word_refs;
    fude_bytes                   word_lists;
    u32                          word_count;   // the words kept (numbered in the file)
    fude_bytes                   word_text;
    fude_bytes                   word_freqs;   // a u16 per word kept, in number order ('WFRQ')
    // Example sentences ('SENT'), each kept once: the language's sentence, then en, es, fr and pt, NUL-terminated,
    // "" where there is none. sentence_words holds each word's sentence (or UINT32_MAX).
    fude_bytes                   sentences;
    u32                          sentence_count;
    rde_arr TYPE(u32)            sentence_words;
    u32                          words_unlisted;   // common words kept on no character's list (for reading text)
    // The other languages ('LNxx', in this order): each one's code, its texts, and its words' (word number, text
    // offset) pairs, in number order.
    u32                          lang_count;
    c8                           lang_code[FUDE_BAKE_LANGS][3];
    fude_bytes                   lang_text[FUDE_BAKE_LANGS];
    fude_bytes                   lang_words[FUDE_BAKE_LANGS];
    u32                          lang_word_count[FUDE_BAKE_LANGS];
    u32                          lang_kanji_count[FUDE_BAKE_LANGS];

    // The report written to the log.
    u32 strokes;
    u32 segments;
    u32 skipped;            // characters dropped: no strokes, too many, or an unparsable path
    u32 with_info;          // given readings and meanings
    u32 count_mismatch;     // the strokes and the dictionary disagree on the stroke count
    u32 level_listed[16];   // characters per level value (lang.h)
    u32 level_missing;      // given a level, but without strokes
    u32 max_segments;
    u32 with_parts;         // characters with at least one part
    u32 part_refs;          // parts over all characters
    u32 word_entries;       // dictionary entries read
    u32 with_words;         // characters with at least one example word
    u32 example_refs;       // examples over all characters (the rest are words to add)
    f32 min_coord;
    f32 max_coord;
} fude_bake;

// --- the bake --------------------------------------------------------------------------

// Makes an empty bake that keeps meanings in these other languages (two-letter codes: "es", "pt"...; at most
// FUDE_BAKE_LANGS); and frees everything it holds.
void  fude_bake_init(fude_bake* _bake, const c8* const* _lang_codes, u32 _lang_count);
void  fude_bake_free(fude_bake* _bake);

// Reads the strokes and parts of an XML file in KanjiVG's form into a character each, sorted by code point. Returns
// false when the file cannot be read or parsed; a character it cannot read is skipped, and counted.
b8    fude_bake_kanjivg(fude_bake* _bake, const c8* _path);

// Reads the prepared tables in _dir (see the top): readings, meanings, levels, words, their lists and sentences,
// for the characters already read. Returns false when chars.tsv is missing; the other tables may be.
b8    fude_bake_prepared(fude_bake* _bake, const c8* _dir);

// Once every candidate is in (fude_bake_word_ref): each character keeps its best words, and the words kept are
// numbered; then every other common word, on no list, is numbered too.
void  fude_bake_choose_words(fude_bake* _bake);
// Keeps a word: gives it the next number, and writes its texts (and its meanings in the other languages, and how
// common it is).
void  fude_bake_number_word(fude_bake* _bake, fude_bake_word* _w);

// Writes the file to _out. With _strokes_out, the strokes go to a file of their own (kanji.h: FUDE_KANJI_STROKES_FILE
// beside it), headed by _notice, which says how and when they were changed, as a licence may ask. The report goes
// to the log, timed from _t0, the bake's start. Returns false when a file cannot be written.
b8    fude_bake_write(fude_bake* _bake, const c8* _out, const c8* _strokes_out, const c8* _notice, f64 _t0);

// The language's rules for look-alikes: which characters are a script's letters, which are worth look-alikes, and
// how close is close enough.
typedef struct {
    b8  (*script)(u32 _codepoint);                 // a script's letters (kana), compared only among themselves
    b8  (*common)(const fude_kanji_info* _info);   // worth look-alikes, and fit to be one
    f32 cost;                                      // the highest matcher's cost (in a 100-unit box) that counts
    f32 cost_script;                               // ...for a script's letters
    u32 strokes;                                   // how many strokes apart they may be, at most
} fude_bake_look;
// Finds each common character's look-alikes with the matcher and appends them to the file at _path as 'LOOK' (last,
// because the matcher reads that very file). Keeps up to FUDE_KANJI_LOOKALIKES common ones of the same kind (a
// script's letters, or not), of nearly as many strokes, and close enough; the others are looked for in their own
// group (lang.h). Returns false when the file cannot be read or written again.
b8    fude_bake_lookalikes(const c8* _path, const fude_bake_look* _look);

// --- a language's bake from its prepared tables ------------------------------------------

typedef struct {
    // Where its sources are: each a default that --strokes= and --prepared= replace. Where the file goes, what makes
    // the sources and the strokes' notice are the app's (fude_bake_app).
    const c8*             strokes;          // the strokes, KanjiVG's form ("data/raw/hi/devanagarivg.xml")
    const c8*             prepared;         // the folder of prepared tables ("data/raw/hi/prepared")
    const c8* const*      languages;        // the meanings' languages, as its prepare.py wrote them ("es", "pt"...)
    u32                   language_count;
    u32                   word_chars;       // the bake's word_chars and example_chars (0: its defaults): more for a
    u32                   example_chars;    // script whose vowel signs and marks are code points of their own
    const fude_bake_look* look;             // its look-alikes
} fude_bake_prepared_def;

// Bakes a language from its prepared tables for the app _app describes: the strokes, the tables, the file (at the
// app's out, with the strokes apart under its notice when it gives one), then the look-alikes. _argc and _argv are
// the app's command line (--strokes=, --prepared=, --out=). Returns 0 on success, or 1 with the log saying why.
i32   fude_bake_run_prepared(const fude_bake_prepared_def* _def, const fude_bake_app* _app, i32 _argc, c8** _argv);

// --- helpers ------------------------------------------------------------------------------

// XML: an attribute's value (NULL when missing: rde_xml_get_attribute would abort, and KanjiVG leaves most of them
// optional), whether an element has a name, and its text ("" for none).
const c8*       fude_xml_attr(const rde_xml_entry* _e, const c8* _name);
b8              fude_xml_is(const rde_xml_entry* _e, const c8* _name);
const c8*       fude_xml_text(const rde_xml_entry* _e);
// Returns the value of "--name=value" on the command line (_prefix is "--name="), or _default when it is absent.
const c8*       fude_bake_arg(i32 _argc, c8** _argv, const c8* _prefix, const c8* _default);
// Appends _s to _out, after _sep unless it is the first, decoding XML's five predefined entities (RDE's XML parser
// hands text over raw).
void            fude_bake_join(c8* _out, usize _size, const c8* _sep, const c8* _s);
// Orders fude_bake_chars by code point (for sorting); finds a character by its code point in the sorted list (NULL
// when it is not there). (Whether a code point is a CJK ideograph is utf8.h's fude_utf8_is_han.)
int             fude_bake_compare(const any _a, const any _b);   // fude_bake_chars by code point
fude_bake_char* fude_bake_find(fude_bake* _bake, u32 _cp);
// Returns how hard a character is to meet, by its school grade (1..6 the grades, 7 the rest of the Jouyou, 9 names
// only, 11 none), for ranking words by their other characters.
i32             fude_bake_difficulty(const fude_bake_char* _c);
// Returns true when _written holds a numeral other than _cp, so that 一月 二月 三月 make one example, not four.
b8              fude_bake_counts(const c8* _written, u32 _cp);

#endif

#endif
