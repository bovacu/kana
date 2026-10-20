// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/chars/catalog.h"
#include "lang/lang.h"
#include "study/models/marks.h"

// ===========================================================================
// See catalog.h.
//
// When it is made, the catalog keeps an entry for each character the language teaches, and turns what search
// compares into KEYS: NUL-terminated strings in one pool (keys), each found by its offset. A reading key is the
// readings folded as the language compares them, joined by "、", with the marks taken out; the stem key holds only
// the readings' stems; the meaning key is the meanings folded to lower case without accents, in the language shown
// and then in English. A query scores each entry that passes the filter (lower is better, FUDE_CATALOG_NO_MATCH
// for none), then sorts them; sorting by reading or meaning first copies each entry's first item into a scratch pool.
// ===========================================================================

#define FUDE_CATALOG_SEPARATOR 0x3001u      // "、" between readings
// A score for an entry the search does not match.
#define FUDE_CATALOG_NO_MATCH  UINT32_MAX

// --- filters and sorts: numbered from the language's groups, levels and readings ---
//
// The filters run: all, then one for each group, one for each level, Studying and Known. The sorts run: the default
// order, by strokes, one for each kind of reading, then by meaning.

FUDE_FILTER_ fude_filter_group(u32 _group)   { return 1u + _group; }
FUDE_FILTER_ fude_filter_level(u32 _level)   { return 1u + fude_lang_group_count() + _level; }
FUDE_FILTER_ fude_filter_studying(void)      { return 1u + fude_lang_group_count() + fude_lang_level_count(); }
FUDE_FILTER_ fude_filter_known(void)         { return fude_filter_studying() + 1u; }
u32          fude_filter_count(void)         { return fude_filter_known() + 1u; }

// See the header.
u32 fude_catalog_set_count(void) {
    u32 _n = 0;
    for(u32 _g = 0; _g < fude_lang_group_count(); _g++) {
        _n += (fude_lang_group_flags(_g) & FUDE_LANG_GROUP_SET) != 0u ? 1u : 0u;
    }
    return _n;
}

// See the header.
u32 fude_catalog_set_group(u32 _set) {
    for(u32 _g = 0; _g < fude_lang_group_count(); _g++) {
        if((fude_lang_group_flags(_g) & FUDE_LANG_GROUP_SET) != 0u && _set-- == 0u) {
            return _g;
        }
    }
    return FUDE_LANG_NO_GROUP;
}

// See the header. A buffer for each level, so that a screen holding several (a row of chips) keeps each one's.
const c8* fude_catalog_level_label(u32 _level) {
    static c8 _labels[FUDE_LANG_LEVELS][16];
    if(_level >= fude_lang_level_count() || _level >= FUDE_LANG_LEVELS) {
        return "";
    }
    fude_lang_level_name(fude_lang_level_value(_level), false, _labels[_level], sizeof(_labels[_level]));
    return _labels[_level];
}

FUDE_SORT_ fude_sort_reading(u32 _kind) { return 2u + _kind; }
FUDE_SORT_ fude_sort_meaning(void)      { return 2u + fude_lang_reading_kinds(); }
u32        fude_sort_count(void)        { return fude_sort_meaning() + 1u; }

// --- keys ------------------------------------------------------------------------

// Adds a code point to the pool, in UTF-8.
RDE_INTERNAL void fude_catalog_put_cp(rde_arr* _pool, u32 _cp) {
    c8 _utf8[5];
    fude_utf8_put(_cp, _utf8);
    for(u32 _i = 0; _utf8[_i] != 0; _i++) {
        rde_arr_add(_pool, &_utf8[_i]);
    }
}

// Starts a key: returns its offset in the pool.
RDE_INTERNAL u32 fude_catalog_begin_key(rde_arr* _pool) {
    return (u32)rde_arr_length(_pool);
}

// Ends a key with its NUL.
RDE_INTERNAL void fude_catalog_end_key(rde_arr* _pool) {
    c8 _nul = 0;
    rde_arr_add(_pool, &_nul);
}

// True when the reading that starts at _r (and runs up to the next "、") has an okurigana mark ('.').
RDE_INTERNAL b8 fude_catalog_has_okurigana(const c8* _r) {
    const c8* _end = rde_strstr(_r, "、");
    const c8* _dot = rde_strchr(_r, '.');
    return _dot != NULL && (_end == NULL || _dot < _end);
}

// Adds a list of readings to the pool as a key, and returns its offset. The readings are folded as the language
// compares them (lang.h; Japanese: into hiragana), with the marks taken out. With _stems, the key holds only the part
// of each reading before its stem mark ('.', Japanese okurigana), and only for the readings that have one.
RDE_INTERNAL u32 fude_catalog_reading_key(rde_arr* _pool, const c8* _text, b8 _stems) {
    const u32 _at      = fude_catalog_begin_key(_pool);
    b8        _written = false;   // whether this reading has added anything yet
    b8        _in_okuri = false;
    b8        _has_okuri = false;

    // For stems, it must be known whether a reading HAS a '.' before it is written.
    const c8* _reading_start = _text;

    for(;;) {
        const c8* _before = _text;
        const u32 _cp     = fude_utf8_next(&_text);

        if(_cp == 0 || _cp == FUDE_CATALOG_SEPARATOR) {
            if(_cp == 0) {
                break;
            }
            _in_okuri = false;
            _reading_start = _text;
            // A separator only between readings that were written.
            if(_written) {
                fude_catalog_put_cp(_pool, FUDE_CATALOG_SEPARATOR);
                _written = false;
            }
            continue;
        }

        if(_stems && _before == _reading_start) {
            _has_okuri = fude_catalog_has_okurigana(_before);   // a new reading starts
        }

        if(_cp == '.') {
            _in_okuri = true;
            continue;
        }
        if(_cp == '-' || _cp == ' ') {
            continue;
        }
        if(_stems && (!_has_okuri || _in_okuri)) {
            continue;
        }

        const u32 _folded = fude_lang_reading_fold(_cp);
        if(_folded == 0u) {
            continue;   // the language leaves it out (lang.h)
        }
        fude_catalog_put_cp(_pool, _folded);
        _written = true;
    }

    // A separator at the end (when the last reading wrote nothing) does no harm: it only makes an empty reading, which
    // never matches.
    fude_catalog_end_key(_pool);
    return _at;
}

// Folds _text into _out (_size bytes): lower case, with the accents off Latin letters (á → a, ç → c, ß → ss, œ → oe),
// so that "arbol" finds árbol and "Été" finds été. Anything else is kept as it is. A text too long is cut before a
// whole character, never inside one.
void fude_catalog_fold(const c8* _text, c8* _out, usize _size) {
    usize _n = 0;
    for(const c8* _p = _text; *_p != 0 && _n + 3u < _size;) {
        const c8* _from = _p;
        const u32 _cp   = fude_utf8_next(&_p);
        if(_cp == 0) {
            break;
        }
        const c8* _to = NULL;
        if(_cp < 0x80u)                                              { _out[_n++] = (c8)rde_char_to_lower((int)_cp); continue; }
        else if((_cp >= 0xC0u && _cp <= 0xC5u) || (_cp >= 0xE0u && _cp <= 0xE5u)) { _to = "a"; }
        else if(_cp == 0xC7u || _cp == 0xE7u)                        { _to = "c"; }
        else if((_cp >= 0xC8u && _cp <= 0xCBu) || (_cp >= 0xE8u && _cp <= 0xEBu)) { _to = "e"; }
        else if((_cp >= 0xCCu && _cp <= 0xCFu) || (_cp >= 0xECu && _cp <= 0xEFu)) { _to = "i"; }
        else if(_cp == 0xD1u || _cp == 0xF1u)                        { _to = "n"; }
        else if((_cp >= 0xD2u && _cp <= 0xD6u) || _cp == 0xD8u || (_cp >= 0xF2u && _cp <= 0xF6u) || _cp == 0xF8u) { _to = "o"; }
        else if((_cp >= 0xD9u && _cp <= 0xDCu) || (_cp >= 0xF9u && _cp <= 0xFCu)) { _to = "u"; }
        else if(_cp == 0xDDu || _cp == 0xFDu || _cp == 0xFFu)        { _to = "y"; }
        else if(_cp == 0xDFu)                                        { _to = "ss"; }
        else if(_cp == 0xC6u || _cp == 0xE6u)                        { _to = "ae"; }
        else if(_cp == 0x152u || _cp == 0x153u)                      { _to = "oe"; }
        if(_to != NULL) {
            for(; *_to != 0 && _n + 1u < _size; _to++) { _out[_n++] = *_to; }
        } else {
            if(_n + (usize)(_p - _from) + 1u > _size) {
                break;   // it does not fit whole
            }
            for(; _from < _p; _from++) { _out[_n++] = *_from; }   // anything else is kept as it is
        }
    }
    _out[_n] = 0;
}

// Adds meanings to the pool as a key, folded (fude_catalog_fold), and returns its offset. Meanings are cut at 2047
// bytes, a bound on the data the app ships, not on the user's: one character's meanings are at most about 200 bytes
// in every app's data (Japanese's longest: 201), so the cut is never reached, and a key needs no allocation.
RDE_INTERNAL u32 fude_catalog_meaning_key(rde_arr* _pool, const c8* _text) {
    c8 _folded[2048];
    fude_catalog_fold(_text, _folded, sizeof(_folded));
    const u32 _at = fude_catalog_begin_key(_pool);
    for(const c8* _p = _folded; *_p != 0; _p++) {
        rde_arr_add(_pool, _p);
    }
    fude_catalog_end_key(_pool);
    return _at;
}

// --- building --------------------------------------------------------------------

// An entry as it is sorted.
typedef struct {
    const fude_catalog_entry* entry;
    const c8*                 key;     // the sort key, or NULL
    u32                       score;   // the search's score (lower is better)
} fude_catalog_sort_item;

// Makes the catalog from _db (see the header): an entry and its keys for every character of a group the language
// teaches. _by_record maps each record to its entry, or to UINT32_MAX when it has none.
void fude_catalog_init(fude_catalog* _catalog, const fude_kanji_db* _db) {
    rde_memset(_catalog, 0, sizeof(*_catalog));
    _catalog->db = _db;

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _catalog->entries    = rde_arr_new(sizeof(fude_catalog_entry), _heap);
    _catalog->keys       = rde_arr_new(sizeof(c8), _heap);
    _catalog->results    = rde_arr_new(sizeof(u32), _heap);
    _catalog->_sort      = rde_arr_new(sizeof(fude_catalog_sort_item), _heap);
    _catalog->_by_record = rde_arr_new(sizeof(u32), _heap);

    if(_db == NULL) {
        return;
    }

    u32 _none = UINT32_MAX;
    for(u32 _r = 0; _r < _db->count; _r++) {
        rde_arr_add(&_catalog->_by_record, &_none);

        fude_kanji_info _info;
        if(!fude_kanji_at(_db, _r, &_info)) {
            continue;
        }
        const u8 _group = fude_lang_group(_info.codepoint);
        if(_group == FUDE_LANG_NO_GROUP) {
            continue;   // punctuation or Latin letters: nothing to learn
        }

        fude_catalog_entry _e = {
            .record    = _r,
            .codepoint = _info.codepoint,
            .group     = _group,
            .strokes   = _info.strokes,
            .grade     = _info.grade,
            .level     = _info.level,
            .frequency = _info.frequency,
        };

        if((fude_lang_group_flags(_group) & FUDE_LANG_GROUP_SCRIPT) == 0u) {
            c8 _all[1024];
            rde_snprintf(_all, sizeof(_all), "%s\xE3\x80\x81%s", fude_kanji_reading(_db, &_info, 0u), fude_kanji_reading(_db, &_info, 1u));   // "、" joins the two kinds of reading
            for(u32 _k = 0; _k < FUDE_CATALOG_READINGS; _k++) {
                _e.reading[_k] = fude_catalog_reading_key(&_catalog->keys, fude_kanji_reading(_db, &_info, _k), false);
            }
            _e.stem     = fude_catalog_reading_key(&_catalog->keys, _all, true);
            // The meanings in the language shown come first (they are sorted by), then English, so that either finds
            // it.
            const c8* _shown   = fude_kanji_meanings(_db, &_info);
            const c8* _english = fude_kanji_meanings_english(_db, &_info);
            c8        _both[2048];
            rde_snprintf(_both, sizeof(_both), _shown != _english ? "%s, %s" : "%s", _shown, _english);
            _e.meaning  = fude_catalog_meaning_key(&_catalog->keys, _both);
        } else {
            // A script letter's reading is the letter itself.
            c8 _self[5];
            fude_utf8_put(_info.codepoint, _self);
            _e.reading[0] = fude_catalog_reading_key(&_catalog->keys, _self, false);
            _e.reading[1] = _e.reading[0];
            _e.stem       = fude_catalog_reading_key(&_catalog->keys, "", false);
            _e.meaning    = _e.stem;
        }

        ((u32*)_catalog->_by_record.memory)[_r] = (u32)rde_arr_length(&_catalog->entries);
        rde_arr_add(&_catalog->entries, &_e);
    }
}

// Frees the catalog's arrays and clears it.
void fude_catalog_destroy(fude_catalog* _catalog) {
    rde_arr* _arrays[] = { &_catalog->entries, &_catalog->keys, &_catalog->results, &_catalog->_sort, &_catalog->_by_record };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    rde_memset(_catalog, 0, sizeof(*_catalog));
}

// Returns the key at _offset in the pool, or "" when it is out of range.
const c8* fude_catalog_key(const fude_catalog* _catalog, u32 _offset) {
    return _offset < rde_arr_length(&_catalog->keys) ? &((const c8*)_catalog->keys.memory)[_offset] : "";
}

// Returns the entry of record _record, or NULL when it has none.
const fude_catalog_entry* fude_catalog_entry_of(const fude_catalog* _catalog, u32 _record) {
    if(_record >= rde_arr_length(&_catalog->_by_record)) {
        return NULL;
    }
    const u32 _i = ((const u32*)_catalog->_by_record.memory)[_record];
    return _i == UINT32_MAX ? NULL : &((const fude_catalog_entry*)_catalog->entries.memory)[_i];
}

// The results of the last query: record numbers, in order.
const u32* fude_catalog_results(const fude_catalog* _catalog) {
    return (const u32*)_catalog->results.memory;
}

// How many results the last query found.
u32 fude_catalog_result_count(const fude_catalog* _catalog) {
    return (u32)rde_arr_length(&_catalog->results);
}

// --- filtering -------------------------------------------------------------------

// True when entry _e passes _filter (numbered as above). A filter out of range lets everything through.
RDE_INTERNAL b8 fude_catalog_entry_passes(const fude_catalog_entry* _e, FUDE_FILTER_ _filter) {
    const u32 _groups = fude_lang_group_count();
    const u32 _levels = fude_lang_level_count();
    if(_filter == FUDE_FILTER_ALL || _filter >= fude_filter_count()) {
        return true;
    }
    if(_filter <= _groups) {
        return _e->group == _filter - 1u;
    }
    if(_filter <= _groups + _levels) {
        return _e->level == fude_lang_level_value(_filter - 1u - _groups);
    }
    return fude_marks_get(_e->codepoint) == (_filter == fude_filter_studying() ? FUDE_MARK_STUDYING : FUDE_MARK_KNOWN);
}

// True when record _record has an entry and it passes _filter.
b8 fude_catalog_passes(const fude_catalog* _catalog, u32 _record, FUDE_FILTER_ _filter) {
    const fude_catalog_entry* _e = fude_catalog_entry_of(_catalog, _record);
    return _e != NULL && fude_catalog_entry_passes(_e, _filter);
}

// --- sorting ---------------------------------------------------------------------

// A grade's place in the default order: 1 to 6 (Kyouiku), then 8 (the rest of the Jouyou), then 9 and 10
// (Jinmeiyou), then none.
RDE_INTERNAL u32 fude_catalog_grade_key(u8 _grade) {
    if(_grade >= 1 && _grade <= 6)  { return _grade; }
    if(_grade == 8)                  { return 7; }
    if(_grade == 9 || _grade == 10)  { return 8; }
    return 9;
}

// The default order: by the language's groups in order (Japanese: hiragana, katakana, then kanji), then by grade
// (1 to 6, the rest of the Jouyou, Jinmeiyou, ungraded), then by frequency, then by code point.
RDE_INTERNAL int fude_catalog_default_order(const fude_catalog_entry* _a, const fude_catalog_entry* _b) {
    if(_a->group != _b->group) { return _a->group < _b->group ? -1 : 1; }
    const u32 _ga = fude_catalog_grade_key(_a->grade), _gb = fude_catalog_grade_key(_b->grade);
    if(_ga != _gb) { return _ga < _gb ? -1 : 1; }
    const u32 _fa = _a->frequency ? _a->frequency : 100000u, _fb = _b->frequency ? _b->frequency : 100000u;
    if(_fa != _fb) { return _fa < _fb ? -1 : 1; }
    return _a->codepoint < _b->codepoint ? -1 : (_a->codepoint > _b->codepoint ? 1 : 0);
}

// Compares two sort items in the default order (rde_sort's comparer).
RDE_INTERNAL int fude_catalog_cmp_default(const any _x, const any _y) {
    return fude_catalog_default_order(((const fude_catalog_sort_item*)_x)->entry, ((const fude_catalog_sort_item*)_y)->entry);
}

// Compares two sort items by stroke count, then in the default order.
RDE_INTERNAL int fude_catalog_cmp_strokes(const any _x, const any _y) {
    const fude_catalog_sort_item* _a = _x;
    const fude_catalog_sort_item* _b = _y;
    if(_a->entry->strokes != _b->entry->strokes) { return _a->entry->strokes < _b->entry->strokes ? -1 : 1; }
    return fude_catalog_default_order(_a->entry, _b->entry);
}

// Compares two sort items by their key (UTF-8 byte order is code point order: gojūon for hiragana, A-Z for
// lower-case Latin), then in the default order. Those without a key go last.
RDE_INTERNAL int fude_catalog_cmp_key(const any _x, const any _y) {
    const fude_catalog_sort_item* _a = _x;
    const fude_catalog_sort_item* _b = _y;
    const b8 _ea = _a->key == NULL || _a->key[0] == 0;
    const b8 _eb = _b->key == NULL || _b->key[0] == 0;
    if(_ea != _eb) { return _ea ? 1 : -1; }
    if(!_ea) {
        const int _c = rde_strcmp(_a->key, _b->key);
        if(_c != 0) { return _c; }
    }
    return fude_catalog_default_order(_a->entry, _b->entry);
}

// Compares two sort items by their search score, best first, then in the default order.
RDE_INTERNAL int fude_catalog_cmp_score(const any _x, const any _y) {
    const fude_catalog_sort_item* _a = _x;
    const fude_catalog_sort_item* _b = _y;
    if(_a->score != _b->score) { return _a->score < _b->score ? -1 : 1; }
    return fude_catalog_default_order(_a->entry, _b->entry);
}

// Copies the first item of a list into _out, for sorting by it ("ぼく、もく" gives "ぼく"; "tree, wood" gives
// "tree").
RDE_INTERNAL void fude_catalog_first(const c8* _list, const c8* _separator, c8* _out, usize _size) {
    const c8*   _end = rde_strstr(_list, _separator);
    const usize _len = _end != NULL ? (usize)(_end - _list) : rde_strlen(_list);
    const usize _n   = _len < _size - 1 ? _len : _size - 1;
    rde_memcpy(_out, _list, _n);
    _out[_n] = 0;
}

// --- searching -------------------------------------------------------------------

// Scores how well _q matches a list of items separated by _sep: 0 when an item equals it, _prefix when an item starts
// with it, _inside when it is somewhere inside one (only with _allow_inside). FUDE_CATALOG_NO_MATCH otherwise.
RDE_INTERNAL u32 fude_catalog_match_list(const c8* _list, const c8* _sep, const c8* _q, u32 _prefix, u32 _inside, b8 _allow_inside) {
    const usize _ql   = rde_strlen(_q);
    const usize _sl   = rde_strlen(_sep);
    u32         _best = FUDE_CATALOG_NO_MATCH;

    while(*_list != 0) {
        const c8*   _end = rde_strstr(_list, _sep);
        const usize _len = _end != NULL ? (usize)(_end - _list) : rde_strlen(_list);

        if(_len == _ql && rde_strncmp(_list, _q, _ql) == 0) {
            return 0;
        }
        if(_len > _ql && rde_strncmp(_list, _q, _ql) == 0) {
            _best = _prefix < _best ? _prefix : _best;
        } else if(_allow_inside && _len > _ql) {
            for(usize _i = 1; _i + _ql <= _len; _i++) {
                if(rde_strncmp(_list + _i, _q, _ql) == 0) {
                    _best = _inside < _best ? _inside : _best;
                    break;
                }
            }
        }

        if(_end == NULL) {
            break;
        }
        _list = _end + _sl;
    }

    return _best;
}

// Scores a search against the meanings: 0 for a whole meaning, 1 for a whole word of one, 2 for a meaning that
// starts with it, 3 for a word that starts with it, and 5 for anywhere inside (only for 3 letters or more).
RDE_INTERNAL u32 fude_catalog_match_meaning(const c8* _meanings, const c8* _q) {
    u32 _best = fude_catalog_match_list(_meanings, ", ", _q, 2, 5, rde_strlen(_q) >= 3);
    if(_best <= 1) {
        return _best;
    }

    // Then word by word.
    const usize _ql = rde_strlen(_q);
    for(const c8* _p = _meanings; *_p != 0; _p++) {
        const b8 _word_start = _p == _meanings || !rde_char_is_alnum((unsigned char)_p[-1]);
        if(!_word_start || rde_strncmp(_p, _q, _ql) != 0) {
            continue;
        }
        const b8 _whole = !rde_char_is_alnum((unsigned char)_p[_ql]);
        _best = _whole ? 1u : (_best < 3u ? _best : 3u);
        if(_best == 1u) {
            break;
        }
    }
    return _best;
}

// Scores entry _e against the search as a reading (_kana, folded) and as a meaning (_english), and returns the best.
// A reading scores 0 when it is equal, 2 when it starts with the search and 4 when it holds it.
RDE_INTERNAL u32 fude_catalog_score(const fude_catalog* _catalog, const fude_catalog_entry* _e, const c8* _kana, const c8* _english) {
    u32 _best = FUDE_CATALOG_NO_MATCH;

    if(_kana != NULL && _kana[0] != 0) {
        const c8* _sep = "、";
        for(u32 _k = 0; _k < FUDE_CATALOG_READINGS; _k++) {
            const u32 _s = fude_catalog_match_list(fude_catalog_key(_catalog, _e->reading[_k]), _sep, _kana, 2, 4, true);
            _best = _s < _best ? _s : _best;
        }
        const u32 _s = fude_catalog_match_list(fude_catalog_key(_catalog, _e->stem), _sep, _kana, FUDE_CATALOG_NO_MATCH, FUDE_CATALOG_NO_MATCH, false);
        _best = (_s == 0 && _best > 1u) ? 1u : _best;   // a stem is almost as good as a reading
    }

    if(_english != NULL && _english[0] != 0) {
        const u32 _s = fude_catalog_match_meaning(fude_catalog_key(_catalog, _e->meaning), _english);
        _best = _s < _best ? _s : _best;
    }

    return _best;
}

// Turns Latin letters into a reading (lang.h: as the language writes readings, then folded as they are compared).
// Returns false when they do not read as one.
RDE_INTERNAL b8 fude_catalog_latin_reading(const c8* _latin, c8* _out, usize _size) {
    c8 _written[256];
    if(!fude_lang_reading_from_latin(_latin, _written, sizeof(_written))) {
        return false;
    }
    usize _n = 0;
    for(const c8* _p = _written; *_p != 0;) {
        c8 _one[5];
        const u32 _folded = fude_lang_reading_fold(fude_utf8_next(&_p));
        if(_folded == 0u) {
            continue;   // the language leaves it out (lang.h)
        }
        fude_utf8_put(_folded, _one);
        const usize _len = rde_strlen(_one);
        if(_n + _len >= _size) {
            break;
        }
        rde_memcpy(_out + _n, _one, _len);
        _n += _len;
    }
    _out[_n] = 0;
    return true;
}

// Runs a query (see the header): filters, scores against the search, and sorts. The search's results come best match
// first; without a search, they come in the sort asked for.
u32 fude_catalog_query(fude_catalog* _catalog, FUDE_FILTER_ _filter, FUDE_SORT_ _sort, const c8* _search) {
    rde_arr_clear(&_catalog->results);
    rde_arr_clear(&_catalog->_sort);

    // The search, both as English (lower case, trimmed) and as a reading (folded as the language compares readings):
    // typed in the script, or Latin letters that read completely as one (lang.h; Japanese: romaji to kana). The search
    // is cut at 127 bytes.
    c8 _english[128] = "";
    c8 _kana[256]    = "";
    if(_search != NULL) {
        while(*_search == ' ') { _search++; }
        // The accents come off first, so that "árbol" is taken as a meaning, not as the script.
        c8 _folded[128];
        fude_catalog_fold(_search, _folded, sizeof(_folded));
        usize _n = 0;
        b8    _ascii = true;
        for(const c8* _p = _folded; *_p != 0 && _n + 1 < sizeof(_english); _p++) {
            if((unsigned char)*_p >= 0x80u) { _ascii = false; }
            _english[_n++] = *_p;
        }
        while(_n > 0 && _english[_n - 1] == ' ') { _n--; }
        _english[_n] = 0;

        if(!_ascii) {
            // The script typed directly is folded as a reading, and is not English.
            rde_arr _tmp = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
            const u32 _at = fude_catalog_reading_key(&_tmp, _english, false);
            rde_snprintf(_kana, sizeof(_kana), "%s", &((const c8*)_tmp.memory)[_at]);
            rde_arr_free(&_tmp);
            _english[0] = 0;
        } else if(_english[0] != 0 && !fude_catalog_latin_reading(_english, _kana, sizeof(_kana))) {
            _kana[0] = 0;   // not a reading, so only English
        }
    }
    const b8 _searching = _english[0] != 0 || _kana[0] != 0;

    const fude_catalog_entry* _entries = (const fude_catalog_entry*)_catalog->entries.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_catalog->entries); _i++) {
        const fude_catalog_entry* _e = &_entries[_i];
        if(!fude_catalog_entry_passes(_e, _filter)) {
            continue;
        }

        fude_catalog_sort_item _item = { .entry = _e, .key = NULL, .score = 0 };
        if(_searching) {
            _item.score = fude_catalog_score(_catalog, _e, _kana, _english);
            if(_item.score == FUDE_CATALOG_NO_MATCH) {
                continue;
            }
        }
        rde_arr_add(&_catalog->_sort, &_item);
    }

    fude_catalog_sort_item* _items = (fude_catalog_sort_item*)_catalog->_sort.memory;
    const u32               _count = (u32)rde_arr_length(&_catalog->_sort);

    // The sort keys: the first reading or meaning, copied out, because the keys hold lists. They live in a scratch pool
    // that outlives the sort.
    rde_arr _firsts = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    const b8 _by_meaning = _sort == fude_sort_meaning();
    const b8 _by_reading = _sort >= fude_sort_reading(0u) && _sort < fude_sort_meaning();
    if(!_searching && (_by_reading || _by_meaning)) {
        rde_arr TYPE(u32) _offsets = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        for(u32 _i = 0; _i < _count; _i++) {
            const fude_catalog_entry* _e = _items[_i].entry;
            const u32 _key = _by_reading ? _e->reading[_sort - fude_sort_reading(0u)] : _e->meaning;
            const c8* _list = fude_catalog_key(_catalog, _key);
            if(_by_meaning) {
                // "(boiled) rice" sorts under r, not before a.
                while(*_list != 0 && !rde_char_is_alnum((unsigned char)*_list)) {
                    _list++;
                }
            }
            c8 _first[128];
            fude_catalog_first(_list, _by_meaning ? ", " : "、", _first, sizeof(_first));
            u32 _at = (u32)rde_arr_length(&_firsts);
            rde_arr_add(&_offsets, &_at);
            for(u32 _c = 0; _first[_c] != 0; _c++) {
                rde_arr_add(&_firsts, &_first[_c]);
            }
            c8 _nul = 0;
            rde_arr_add(&_firsts, &_nul);
        }
        // The pointers are taken only once the pool has stopped growing, because growing moves it.
        for(u32 _i = 0; _i < _count; _i++) {
            _items[_i].key = &((const c8*)_firsts.memory)[((const u32*)_offsets.memory)[_i]];
        }
        rde_arr_free(&_offsets);
    }

    int (*_cmp)(const any, const any) = fude_catalog_cmp_default;
    if(_searching)                     { _cmp = fude_catalog_cmp_score; }
    else if(_sort == FUDE_SORT_STROKES) { _cmp = fude_catalog_cmp_strokes; }
    else if(_sort != FUDE_SORT_DEFAULT) { _cmp = fude_catalog_cmp_key; }
    rde_sort(_items, _count, sizeof(fude_catalog_sort_item), _cmp);

    for(u32 _i = 0; _i < _count; _i++) {
        u32 _record = _items[_i].entry->record;
        rde_arr_add(&_catalog->results, &_record);
    }

    rde_arr_free(&_firsts);
    return _count;
}

