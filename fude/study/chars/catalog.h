#ifndef FUDE_CATALOG
#define FUDE_CATALOG

#include "rde.h"
#include "study/chars/kanji.h"

// ===========================================================================
// The catalog: every character of the language to learn (lang.h's groups —
// Japanese: kana and kanji), ready to filter, sort and search — what Browse
// lists, and the start of the "Register" section of docs/design.md.
//
// SEARCH takes what a learner would type, no keyboard for the script needed:
//   - an English meaning, "tree";
//   - a reading in Latin letters, "moku", "ki", "shita" — converted as the
//     language writes readings (lang.h: Japanese romaji to kana);
//   - a reading in the script, "もく", "モク".
// Readings are compared as the language folds them (Japanese: in hiragana, so
// on, katakana in KANJIDIC2, and kun, hiragana, match alike); the stem marks
// ('.') and affix dashes ('-') are ignored, and a reading's stem alone ("あ" of
// "あ.げる") counts as a match too. Results come best match first: exact reading
// or meaning, then prefix, then anywhere inside.
//
// A script's letters (lang.h's FUDE_LANG_GROUP_SCRIPT: kana) have no readings
// in the data; their reading is themselves, so "ka" finds か and カ.
// ===========================================================================

// A filter (Browse's chips, an exam's source, what handwriting may be read as):
// all, one of the language's groups, one of its levels, or the learner's marks,
// numbered in that order — Japanese: 0 all, 1-3 hiragana, katakana, kanji, 4-8
// N5..N1, 9 studying, 10 known. Exam logs keep sources made of these numbers.
typedef u32 FUDE_FILTER_;
#define FUDE_FILTER_ALL 0u
FUDE_FILTER_ fude_filter_group(u32 _group);   // lang.h's group
FUDE_FILTER_ fude_filter_level(u32 _level);   // lang.h's level, easiest first
FUDE_FILTER_ fude_filter_studying(void);      // marked (marks.h)
FUDE_FILTER_ fude_filter_known(void);
u32          fude_filter_count(void);

// A sort: the default (by group, then school grade and frequency), by stroke
// count, by the first reading of each of the language's kinds (lang.h; none:
// last), by the first English meaning (A-Z) — Japanese: 0 default, 1 strokes,
// 2 on, 3 kun, 4 meaning.
typedef u32 FUDE_SORT_;
#define FUDE_SORT_DEFAULT 0u
#define FUDE_SORT_STROKES 1u
FUDE_SORT_ fude_sort_reading(u32 _kind);
FUDE_SORT_ fude_sort_meaning(void);
u32        fude_sort_count(void);

#define FUDE_CATALOG_READINGS 2u   // reading kinds kept (lang.h: at most two)

RDE_STRUCT {
    u32 record;       // in the kanji db
    u32 codepoint;
    u8  group;        // lang.h's group
    u8  strokes;
    u8  grade;
    u8  level;        // lang.h's level value (0: none)
    u16 frequency;
    u32 reading[FUDE_CATALOG_READINGS];   // into the key pool: each kind's readings folded, "ぼく、もく", "き、こ"
    u32 stem;         // the stems of readings with a stem mark, "あ" of "あ.げる"
    u32 meaning;      // meanings in lower case, "tree, wood"
} fude_catalog_entry;

RDE_STRUCT {
    const fude_kanji_db*             db;
    rde_arr TYPE(fude_catalog_entry) entries;   // kana and kanji only (not punctuation or Latin)
    rde_arr TYPE(c8)                 keys;      // the key strings, NUL-terminated, back to back
    rde_arr TYPE(u32)                results;   // record indices of the last query, in order

    rde_arr TYPE(u8)                 _sort;      // scratch for query (element: an internal sort item)
    rde_arr TYPE(u32)                _by_record; // record → entry index, UINT32_MAX when none
} fude_catalog;

void fude_catalog_init(fude_catalog* _catalog, const fude_kanji_db* _db);
// _text lower case with the Latin accents off (á → a, ç → c) into _out: how
// meanings are kept, and a search compared with them.
void fude_catalog_fold(const c8* _text, c8* _out, usize _size);
void fude_catalog_destroy(fude_catalog* _catalog);

// Filters, then either searches (_search non-empty: best match first) or sorts.
// The record indices land in _catalog->results; returns how many.
u32  fude_catalog_query(fude_catalog* _catalog, FUDE_FILTER_ _filter, FUDE_SORT_ _sort, const c8* _search);

// The results as an array (valid until the next query).
const u32* fude_catalog_results(const fude_catalog* _catalog);
u32        fude_catalog_result_count(const fude_catalog* _catalog);

// Does a record pass a filter? (Draw-search filters its candidates with this.)
b8   fude_catalog_passes(const fude_catalog* _catalog, u32 _record, FUDE_FILTER_ _filter);
// The catalog entry of a record, or NULL (punctuation, Latin).
const fude_catalog_entry* fude_catalog_entry_of(const fude_catalog* _catalog, u32 _record);
const c8* fude_catalog_key(const fude_catalog* _catalog, u32 _offset);


#endif
