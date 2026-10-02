#ifndef FUDE_CATALOG
#define FUDE_CATALOG

#include "rde.h"
#include "study/chars/kanji.h"

// ===========================================================================
// The catalog: every kana and kanji, ready to filter, sort and search — what
// Browse lists, and the start of the "Register" section of docs/design.md.
//
// SEARCH takes what a learner would type, no Japanese keyboard needed:
//   - an English meaning, "tree";
//   - a reading in romaji, "moku", "ki", "shita" — converted to kana;
//   - a reading in kana, either script: "もく", "モク".
// Readings are compared in hiragana, so on (katakana in KANJIDIC2) and kun
// (hiragana) match alike; the okurigana marks ('.') and affix dashes ('-') are
// ignored, and a kun's stem alone ("あ" of "あ.げる") counts as a match too.
// Results come best match first: exact reading or meaning, then prefix, then
// anywhere inside.
//
// Kana have no KANJIDIC2 entry; their reading is themselves, so "ka" finds か
// and カ.
// ===========================================================================

typedef enum {
    FUDE_FILTER_ALL = 0,
    FUDE_FILTER_HIRAGANA,
    FUDE_FILTER_KATAKANA,
    FUDE_FILTER_KANJI,
    FUDE_FILTER_N5,
    FUDE_FILTER_N4,
    FUDE_FILTER_N3,
    FUDE_FILTER_N2,
    FUDE_FILTER_N1,
    FUDE_FILTER_STUDYING,    // marked (marks.h)
    FUDE_FILTER_KNOWN,
    FUDE_FILTER_COUNT
} FUDE_FILTER_;

typedef enum {
    FUDE_SORT_DEFAULT = 0,   // hiragana, katakana, then kanji by school grade and frequency
    FUDE_SORT_STROKES,
    FUDE_SORT_ON,            // gojūon order of the first on reading (none: last)
    FUDE_SORT_KUN,           // ...of the first kun reading
    FUDE_SORT_MEANING,       // A-Z by the first English meaning
    FUDE_SORT_COUNT
} FUDE_SORT_;

typedef enum {
    FUDE_GROUP_HIRAGANA = 0,
    FUDE_GROUP_KATAKANA,
    FUDE_GROUP_KANJI
} FUDE_GROUP_;

RDE_STRUCT {
    u32 record;       // in the kanji db
    u32 codepoint;
    u8  group;        // FUDE_GROUP_
    u8  strokes;
    u8  grade;
    u8  jlpt_n;
    u16 frequency;
    u32 on;           // into the key pool: on readings in hiragana, "ぼく、もく"
    u32 kun;          // kun readings in hiragana, marks removed, "き、こ"
    u32 kun_stem;     // the stems of kun readings with okurigana, "あ" of "あ.げる"
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
