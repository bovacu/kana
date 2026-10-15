// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_CATALOG
#define FUDE_CATALOG

#include "rde.h"
#include "study/chars/kanji.h"

// ===========================================================================
// Catalog: every character of the language to learn (lang.h's groups; for Japanese, kana and kanji), ready to
// filter, sort and search. It is what Browse lists, and the start of the "Register" section of docs/design.md.
//
// Where it sits: the study layer's character data (study/chars). Browse owns the app's catalog (the study points to
// it: study.h); the exams, Check, Statistics, the word exam and the handwriting reader (textink.h) use it, and
// handwriting search filters its candidates with fude_catalog_passes.
// Its main types and their lifecycle: a fude_catalog is made by fude_catalog_init from the character data, which
// must outlive it, and freed by fude_catalog_destroy. It copies what it searches (readings and meanings, as keys)
// into its own pool when it is made, so it is made again when the language of the meanings changes.
// Threads: call it on the app's main thread only. A query writes the catalog's own scratch and results, and the
// Studying and Known filters read the learner's marks (marks.h).
// Files: none.
//
// SEARCH takes what a learner would type, with no keyboard for the script needed:
//   - an English meaning, "tree" (or one in the language shown, without its accents: "arbol" finds árbol);
//   - a reading in Latin letters, "moku", "ki", "shita", converted as the language writes readings (lang.h: for
//     Japanese, romaji to kana);
//   - a reading in the script, "もく", "モク".
// Readings are compared as the language folds them (for Japanese, in hiragana, so that on readings, in katakana in
// KANJIDIC2, and kun readings, in hiragana, match alike). The stem marks ('.') and affix dashes ('-') are ignored,
// and a reading's stem alone ("あ" of "あ.げる") counts as a match too. Results come best match first: an exact
// reading or meaning, then one that starts with the search, then one with it anywhere inside.
//
// A script's letters (lang.h's FUDE_LANG_GROUP_SCRIPT: kana) have no readings in the data; their reading is
// themselves, so "ka" finds か and カ.
// ===========================================================================

// A filter (Browse's chips, an exam's source, what handwriting may be read as): all, one of the language's groups,
// one of its levels, or one of the learner's marks, numbered in that order. For Japanese: 0 all, 1-3 hiragana,
// katakana and kanji, 4-8 N5..N1, 9 studying, 10 known. Exam logs keep sources made of these numbers, so the
// numbering must not change.
typedef u32 FUDE_FILTER_;
#define FUDE_FILTER_ALL 0u
// The filter numbers: for one of lang.h's groups, for one of its levels (easiest first), for the characters marked
// Studying or Known (marks.h); and how many filters there are.
FUDE_FILTER_ fude_filter_group(u32 _group);   // lang.h's group
FUDE_FILTER_ fude_filter_level(u32 _level);   // lang.h's level, easiest first
FUDE_FILTER_ fude_filter_studying(void);      // marked Studying (marks.h)
FUDE_FILTER_ fude_filter_known(void);
u32          fude_filter_count(void);

// The language's sets (lang.h: a group with FUDE_LANG_GROUP_SET, learnt whole: Japanese's hiragana and katakana) and
// its levels, as the screens that list them name them (an exam's sources, Statistics' Levels card, Browse's chips).
// fude_catalog_set_count returns how many groups are sets; fude_catalog_set_group returns the group of the _set-th
// set (FUDE_LANG_NO_GROUP when there is none); fude_catalog_level_label returns level _level's short name ("N5"),
// easiest first ("" past the last), from a buffer of this module's for each level, valid until it is asked again.
u32       fude_catalog_set_count(void);
u32       fude_catalog_set_group(u32 _set);
const c8* fude_catalog_level_label(u32 _level);

// A sort: the default (by group, then school grade and frequency), by stroke count, by the first reading of each
// of the language's kinds (lang.h; characters without one go last), or by the first English meaning (A-Z). For
// Japanese: 0 default, 1 strokes, 2 on, 3 kun, 4 meaning.
typedef u32 FUDE_SORT_;
#define FUDE_SORT_DEFAULT 0u
#define FUDE_SORT_STROKES 1u
// The sort numbers: by the first reading of kind _kind (0 or 1, lang.h), by the first meaning; and how many sorts
// there are.
FUDE_SORT_ fude_sort_reading(u32 _kind);
FUDE_SORT_ fude_sort_meaning(void);
u32        fude_sort_count(void);

#define FUDE_CATALOG_READINGS 2u   // the reading kinds kept (lang.h has at most two)

RDE_STRUCT {
    u32 record;       // its record in the character data (kanji.h)
    u32 codepoint;
    u8  group;        // lang.h's group
    u8  strokes;
    u8  grade;
    u8  level;        // lang.h's level value (0: none)
    u16 frequency;
    u32 reading[FUDE_CATALOG_READINGS];   // into the key pool: each kind's readings folded, "ぼく、もく", "き、こ"
    u32 stem;         // the stems of the readings that have a stem mark, "あ" of "あ.げる"
    u32 meaning;      // the meanings in lower case, "tree, wood" (in the language shown, then in English)
} fude_catalog_entry;

RDE_STRUCT {
    const fude_kanji_db*             db;
    rde_arr TYPE(fude_catalog_entry) entries;   // the characters to learn only (not punctuation or Latin)
    rde_arr TYPE(c8)                 keys;      // the key strings, NUL-terminated, back to back
    rde_arr TYPE(u32)                results;   // the record indices of the last query, in order

    rde_arr TYPE(u8)                 _sort;      // scratch for query (each element an internal sort item)
    rde_arr TYPE(u32)                _by_record; // record → entry index, UINT32_MAX when none
} fude_catalog;

// Makes the catalog of _db's characters (with _db NULL it is empty but usable); and frees it.
void fude_catalog_init(fude_catalog* _catalog, const fude_kanji_db* _db);
// Writes _text into _out in lower case with the Latin accents taken off (á → a, ç → c, ß → ss, œ → oe), cut to
// _size. It is how meanings are kept, and how a search is made comparable with them.
void fude_catalog_fold(const c8* _text, c8* _out, usize _size);
void fude_catalog_destroy(fude_catalog* _catalog);

// Filters, then either searches (when _search is not empty: best match first) or sorts. The record indices land in
// _catalog->results; returns how many.
u32  fude_catalog_query(fude_catalog* _catalog, FUDE_FILTER_ _filter, FUDE_SORT_ _sort, const c8* _search);

// Returns the last query's results as an array, valid until the next query, and how many there are.
const u32* fude_catalog_results(const fude_catalog* _catalog);
u32        fude_catalog_result_count(const fude_catalog* _catalog);

// Returns true when a record passes a filter (handwriting search filters its candidates with this). A filter number
// out of range lets everything pass; a record that is not in the catalog never passes.
b8   fude_catalog_passes(const fude_catalog* _catalog, u32 _record, FUDE_FILTER_ _filter);
// Returns the catalog entry of a record, or NULL when it is not one to learn (punctuation, Latin). The entry stays
// valid as long as the catalog does.
const fude_catalog_entry* fude_catalog_entry_of(const fude_catalog* _catalog, u32 _record);
// Returns the key string at _offset in the key pool (an entry's reading, stem or meaning), or "" when out of range.
const c8* fude_catalog_key(const fude_catalog* _catalog, u32 _offset);


#endif
