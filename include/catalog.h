#ifndef KANA_CATALOG
#define KANA_CATALOG

#include "rde.h"
#include "kanji.h"

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
    KANA_FILTER_ALL = 0,
    KANA_FILTER_HIRAGANA,
    KANA_FILTER_KATAKANA,
    KANA_FILTER_KANJI,
    KANA_FILTER_N5,
    KANA_FILTER_N4,
    KANA_FILTER_N3,
    KANA_FILTER_N2,
    KANA_FILTER_N1,
    KANA_FILTER_STUDYING,    // marked (marks.h)
    KANA_FILTER_KNOWN,
    KANA_FILTER_COUNT
} KANA_FILTER_;

typedef enum {
    KANA_SORT_DEFAULT = 0,   // hiragana, katakana, then kanji by school grade and frequency
    KANA_SORT_STROKES,
    KANA_SORT_ON,            // gojūon order of the first on reading (none: last)
    KANA_SORT_KUN,           // ...of the first kun reading
    KANA_SORT_MEANING,       // A-Z by the first English meaning
    KANA_SORT_COUNT
} KANA_SORT_;

typedef enum {
    KANA_GROUP_HIRAGANA = 0,
    KANA_GROUP_KATAKANA,
    KANA_GROUP_KANJI
} KANA_GROUP_;

RDE_STRUCT {
    u32 record;       // in the kanji db
    u32 codepoint;
    u8  group;        // KANA_GROUP_
    u8  strokes;
    u8  grade;
    u8  jlpt_n;
    u16 frequency;
    u32 on;           // into the key pool: on readings in hiragana, "ぼく、もく"
    u32 kun;          // kun readings in hiragana, marks removed, "き、こ"
    u32 kun_stem;     // the stems of kun readings with okurigana, "あ" of "あ.げる"
    u32 meaning;      // meanings in lower case, "tree, wood"
} kana_catalog_entry;

RDE_STRUCT {
    const kana_kanji_db*             db;
    rde_arr TYPE(kana_catalog_entry) entries;   // kana and kanji only (not punctuation or Latin)
    rde_arr TYPE(c8)                 keys;      // the key strings, NUL-terminated, back to back
    rde_arr TYPE(u32)                results;   // record indices of the last query, in order

    rde_arr TYPE(u8)                 _sort;      // scratch for query (element: an internal sort item)
    rde_arr TYPE(u32)                _by_record; // record → entry index, UINT32_MAX when none
} kana_catalog;

void kana_catalog_init(kana_catalog* _catalog, const kana_kanji_db* _db);
void kana_catalog_destroy(kana_catalog* _catalog);

// Filters, then either searches (_search non-empty: best match first) or sorts.
// The record indices land in _catalog->results; returns how many.
u32  kana_catalog_query(kana_catalog* _catalog, KANA_FILTER_ _filter, KANA_SORT_ _sort, const c8* _search);

// The results as an array (valid until the next query).
const u32* kana_catalog_results(const kana_catalog* _catalog);
u32        kana_catalog_result_count(const kana_catalog* _catalog);

// Does a record pass a filter? (Draw-search filters its candidates with this.)
b8   kana_catalog_passes(const kana_catalog* _catalog, u32 _record, KANA_FILTER_ _filter);
// The catalog entry of a record, or NULL (punctuation, Latin).
const kana_catalog_entry* kana_catalog_entry_of(const kana_catalog* _catalog, u32 _record);
const c8* kana_catalog_key(const kana_catalog* _catalog, u32 _offset);

// Romaji → hiragana (Hepburn and Kunrei spellings, doubled consonants → っ,
// n/nn/n' → ん, "-" → ー). False if some letters could not be read; _out then
// holds what could. _out is UTF-8, NUL-terminated.
b8   kana_romaji_to_hiragana(const c8* _romaji, c8* _out, usize _size);

#endif
