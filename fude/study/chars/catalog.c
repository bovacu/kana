#include "study/chars/catalog.h"
#include "lang/lang.h"
#include "study/models/marks.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See catalog.h.
// ===========================================================================

#define FUDE_CATALOG_SEPARATOR 0x3001u      // 、 between readings
#define FUDE_CATALOG_NO_MATCH  UINT32_MAX

// --- keys ------------------------------------------------------------------------

// --- filters and sorts: numbered from the language's groups, levels and readings ---

FUDE_FILTER_ fude_filter_group(u32 _group)   { return 1u + _group; }
FUDE_FILTER_ fude_filter_level(u32 _level)   { return 1u + fude_lang_group_count() + _level; }
FUDE_FILTER_ fude_filter_studying(void)      { return 1u + fude_lang_group_count() + fude_lang_level_count(); }
FUDE_FILTER_ fude_filter_known(void)         { return fude_filter_studying() + 1u; }
u32          fude_filter_count(void)         { return fude_filter_known() + 1u; }

FUDE_SORT_ fude_sort_reading(u32 _kind) { return 2u + _kind; }
FUDE_SORT_ fude_sort_meaning(void)      { return 2u + fude_lang_reading_kinds(); }
u32        fude_sort_count(void)        { return fude_sort_meaning() + 1u; }

RDE_INTERNAL void fude_catalog_put_cp(rde_arr* _pool, u32 _cp) {
    c8 _utf8[5];
    fude_utf8_put(_cp, _utf8);
    for(u32 _i = 0; _utf8[_i] != 0; _i++) {
        rde_arr_add(_pool, &_utf8[_i]);
    }
}

RDE_INTERNAL u32 fude_catalog_begin_key(rde_arr* _pool) {
    return (u32)rde_arr_length(_pool);
}

RDE_INTERNAL void fude_catalog_end_key(rde_arr* _pool) {
    c8 _nul = 0;
    rde_arr_add(_pool, &_nul);
}

// Does the reading starting at _r (up to the next 、) have an okurigana mark?
RDE_INTERNAL b8 fude_catalog_has_okurigana(const c8* _r) {
    const c8* _end = strstr(_r, "、");
    const c8* _dot = strchr(_r, '.');
    return _dot != NULL && (_end == NULL || _dot < _end);
}

// A reading list as a key: folded as the language compares readings (lang.h;
// Japanese: hiragana), marks removed. _stems: only the part before the stem mark
// ('.', Japanese okurigana), and only of readings that have one.
RDE_INTERNAL u32 fude_catalog_reading_key(rde_arr* _pool, const c8* _text, b8 _stems) {
    const u32 _at      = fude_catalog_begin_key(_pool);
    b8        _written = false;   // this reading has put something out
    b8        _in_okuri = false;
    b8        _has_okuri = false;

    // Stems need to know whether a reading HAS a '.', before writing it.
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
            // Separator only between readings that were written.
            if(_written) {
                fude_catalog_put_cp(_pool, FUDE_CATALOG_SEPARATOR);
                _written = false;
            }
            continue;
        }

        if(_stems && _before == _reading_start) {
            _has_okuri = fude_catalog_has_okurigana(_before);   // a new reading
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

        fude_catalog_put_cp(_pool, fude_lang_reading_fold(_cp));
        _written = true;
    }

    // A trailing separator (the last reading wrote nothing) is harmless: it just
    // yields an empty reading, which never matches.
    fude_catalog_end_key(_pool);
    return _at;
}

// Lower case with the accents off (Latin letters: á → a, ç → c, ß → ss, œ → oe),
// so "arbol" finds árbol and "Été" été. Anything else is kept as it is.
void fude_catalog_fold(const c8* _text, c8* _out, usize _size) {
    usize _n = 0;
    for(const c8* _p = _text; *_p != 0 && _n + 3u < _size;) {
        const c8* _from = _p;
        const u32 _cp   = fude_utf8_next(&_p);
        if(_cp == 0) {
            break;
        }
        const c8* _to = NULL;
        if(_cp < 0x80u)                                              { _out[_n++] = (c8)tolower((int)_cp); continue; }
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
            for(; _from < _p && _n + 1u < _size; _from++) { _out[_n++] = *_from; }   // as it is
        }
    }
    _out[_n] = 0;
}

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

typedef struct {
    const fude_catalog_entry* entry;
    const c8*                 key;     // sort key, or NULL
    u32                       score;   // search score (lower is better)
} fude_catalog_sort_item;

void fude_catalog_init(fude_catalog* _catalog, const fude_kanji_db* _db) {
    memset(_catalog, 0, sizeof(*_catalog));
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
            continue;   // punctuation, Latin: not something to learn
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
            snprintf(_all, sizeof(_all), "%s\xE3\x80\x81%s", fude_kanji_reading(_db, &_info, 0u), fude_kanji_reading(_db, &_info, 1u));   // 、
            for(u32 _k = 0; _k < FUDE_CATALOG_READINGS; _k++) {
                _e.reading[_k] = fude_catalog_reading_key(&_catalog->keys, fude_kanji_reading(_db, &_info, _k), false);
            }
            _e.stem     = fude_catalog_reading_key(&_catalog->keys, _all, true);
            // In the language shown first (it sorts by it), then English: either finds it.
            const c8* _shown   = fude_kanji_meanings(_db, &_info);
            const c8* _english = fude_kanji_meanings_english(_db, &_info);
            c8        _both[2048];
            snprintf(_both, sizeof(_both), _shown != _english ? "%s, %s" : "%s", _shown, _english);
            _e.meaning  = fude_catalog_meaning_key(&_catalog->keys, _both);
        } else {
            // A script letter's reading is itself.
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

void fude_catalog_destroy(fude_catalog* _catalog) {
    rde_arr* _arrays[] = { &_catalog->entries, &_catalog->keys, &_catalog->results, &_catalog->_sort, &_catalog->_by_record };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    memset(_catalog, 0, sizeof(*_catalog));
}

const c8* fude_catalog_key(const fude_catalog* _catalog, u32 _offset) {
    return _offset < rde_arr_length(&_catalog->keys) ? &((const c8*)_catalog->keys.memory)[_offset] : "";
}

const fude_catalog_entry* fude_catalog_entry_of(const fude_catalog* _catalog, u32 _record) {
    if(_record >= rde_arr_length(&_catalog->_by_record)) {
        return NULL;
    }
    const u32 _i = ((const u32*)_catalog->_by_record.memory)[_record];
    return _i == UINT32_MAX ? NULL : &((const fude_catalog_entry*)_catalog->entries.memory)[_i];
}

const u32* fude_catalog_results(const fude_catalog* _catalog) {
    return (const u32*)_catalog->results.memory;
}

u32 fude_catalog_result_count(const fude_catalog* _catalog) {
    return (u32)rde_arr_length(&_catalog->results);
}

// --- filtering -------------------------------------------------------------------

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

b8 fude_catalog_passes(const fude_catalog* _catalog, u32 _record, FUDE_FILTER_ _filter) {
    const fude_catalog_entry* _e = fude_catalog_entry_of(_catalog, _record);
    return _e != NULL && fude_catalog_entry_passes(_e, _filter);
}

// --- sorting ---------------------------------------------------------------------

RDE_INTERNAL u32 fude_catalog_grade_key(u8 _grade) {
    if(_grade >= 1 && _grade <= 6)  { return _grade; }
    if(_grade == 8)                  { return 7; }
    if(_grade == 9 || _grade == 10)  { return 8; }
    return 9;
}

// Hiragana, katakana, then kanji by grade (1-6, rest of the Jouyou, Jinmeiyou,
// ungraded) and frequency.
RDE_INTERNAL int fude_catalog_default_order(const fude_catalog_entry* _a, const fude_catalog_entry* _b) {
    if(_a->group != _b->group) { return _a->group < _b->group ? -1 : 1; }
    const u32 _ga = fude_catalog_grade_key(_a->grade), _gb = fude_catalog_grade_key(_b->grade);
    if(_ga != _gb) { return _ga < _gb ? -1 : 1; }
    const u32 _fa = _a->frequency ? _a->frequency : 100000u, _fb = _b->frequency ? _b->frequency : 100000u;
    if(_fa != _fb) { return _fa < _fb ? -1 : 1; }
    return _a->codepoint < _b->codepoint ? -1 : (_a->codepoint > _b->codepoint ? 1 : 0);
}

RDE_INTERNAL int fude_catalog_cmp_default(const void* _x, const void* _y) {
    return fude_catalog_default_order(((const fude_catalog_sort_item*)_x)->entry, ((const fude_catalog_sort_item*)_y)->entry);
}

RDE_INTERNAL int fude_catalog_cmp_strokes(const void* _x, const void* _y) {
    const fude_catalog_sort_item* _a = _x;
    const fude_catalog_sort_item* _b = _y;
    if(_a->entry->strokes != _b->entry->strokes) { return _a->entry->strokes < _b->entry->strokes ? -1 : 1; }
    return fude_catalog_default_order(_a->entry, _b->entry);
}

// By key (UTF-8 byte order is code point order: gojūon for hiragana, A-Z for
// lower-case Latin); those without one go last.
RDE_INTERNAL int fude_catalog_cmp_key(const void* _x, const void* _y) {
    const fude_catalog_sort_item* _a = _x;
    const fude_catalog_sort_item* _b = _y;
    const b8 _ea = _a->key == NULL || _a->key[0] == 0;
    const b8 _eb = _b->key == NULL || _b->key[0] == 0;
    if(_ea != _eb) { return _ea ? 1 : -1; }
    if(!_ea) {
        const int _c = strcmp(_a->key, _b->key);
        if(_c != 0) { return _c; }
    }
    return fude_catalog_default_order(_a->entry, _b->entry);
}

RDE_INTERNAL int fude_catalog_cmp_score(const void* _x, const void* _y) {
    const fude_catalog_sort_item* _a = _x;
    const fude_catalog_sort_item* _b = _y;
    if(_a->score != _b->score) { return _a->score < _b->score ? -1 : 1; }
    return fude_catalog_default_order(_a->entry, _b->entry);
}

// The first item of a list ("ぼく、もく" → "ぼく"; "tree, wood" → "tree") into
// _out, for sorting by it.
RDE_INTERNAL void fude_catalog_first(const c8* _list, const c8* _separator, c8* _out, usize _size) {
    const c8*   _end = strstr(_list, _separator);
    const usize _len = _end != NULL ? (usize)(_end - _list) : strlen(_list);
    const usize _n   = _len < _size - 1 ? _len : _size - 1;
    memcpy(_out, _list, _n);
    _out[_n] = 0;
}

// --- searching -------------------------------------------------------------------

// How well _q matches a list of items separated by _sep: 0 an item equals it,
// _prefix an item starts with it, _inside it is somewhere inside (only if
// _allow_inside). FUDE_CATALOG_NO_MATCH otherwise.
RDE_INTERNAL u32 fude_catalog_match_list(const c8* _list, const c8* _sep, const c8* _q, u32 _prefix, u32 _inside, b8 _allow_inside) {
    const usize _ql   = strlen(_q);
    const usize _sl   = strlen(_sep);
    u32         _best = FUDE_CATALOG_NO_MATCH;

    while(*_list != 0) {
        const c8*   _end = strstr(_list, _sep);
        const usize _len = _end != NULL ? (usize)(_end - _list) : strlen(_list);

        if(_len == _ql && strncmp(_list, _q, _ql) == 0) {
            return 0;
        }
        if(_len > _ql && strncmp(_list, _q, _ql) == 0) {
            _best = _prefix < _best ? _prefix : _best;
        } else if(_allow_inside && _len > _ql) {
            for(usize _i = 1; _i + _ql <= _len; _i++) {
                if(strncmp(_list + _i, _q, _ql) == 0) {
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

// English: a whole meaning 0, a whole word of one 1, a meaning starting with it
// 2, a word starting with it 3, anywhere (3+ letters) 5.
RDE_INTERNAL u32 fude_catalog_match_meaning(const c8* _meanings, const c8* _q) {
    u32 _best = fude_catalog_match_list(_meanings, ", ", _q, 2, 5, strlen(_q) >= 3);
    if(_best <= 1) {
        return _best;
    }

    // Word by word.
    const usize _ql = strlen(_q);
    for(const c8* _p = _meanings; *_p != 0; _p++) {
        const b8 _word_start = _p == _meanings || !isalnum((unsigned char)_p[-1]);
        if(!_word_start || strncmp(_p, _q, _ql) != 0) {
            continue;
        }
        const b8 _whole = !isalnum((unsigned char)_p[_ql]);
        _best = _whole ? 1u : (_best < 3u ? _best : 3u);
        if(_best == 1u) {
            break;
        }
    }
    return _best;
}

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

// Latin letters as a reading (lang.h: as the language writes readings, then folded
// as they are compared). False when they do not read as one.
RDE_INTERNAL b8 fude_catalog_latin_reading(const c8* _latin, c8* _out, usize _size) {
    c8 _written[256];
    if(!fude_lang_reading_from_latin(_latin, _written, sizeof(_written))) {
        return false;
    }
    usize _n = 0;
    for(const c8* _p = _written; *_p != 0;) {
        c8 _one[5];
        fude_utf8_put(fude_lang_reading_fold(fude_utf8_next(&_p)), _one);
        const usize _len = strlen(_one);
        if(_n + _len >= _size) {
            break;
        }
        memcpy(_out + _n, _one, _len);
        _n += _len;
    }
    _out[_n] = 0;
    return true;
}

u32 fude_catalog_query(fude_catalog* _catalog, FUDE_FILTER_ _filter, FUDE_SORT_ _sort, const c8* _search) {
    rde_arr_clear(&_catalog->results);
    rde_arr_clear(&_catalog->_sort);

    // The search, as English (lower case, trimmed) and as a reading (folded as
    // the language compares them): typed in the script, or Latin letters that
    // read completely as one (lang.h; Japanese: romaji to kana).
    c8 _english[128] = "";
    c8 _kana[256]    = "";
    if(_search != NULL) {
        while(*_search == ' ') { _search++; }
        // Accents off first: "árbol" is a meaning, not kana.
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
            // The script typed directly: folded. Not English.
            rde_arr _tmp = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
            const u32 _at = fude_catalog_reading_key(&_tmp, _english, false);
            snprintf(_kana, sizeof(_kana), "%s", &((const c8*)_tmp.memory)[_at]);
            rde_arr_free(&_tmp);
            _english[0] = 0;
        } else if(_english[0] != 0 && !fude_catalog_latin_reading(_english, _kana, sizeof(_kana))) {
            _kana[0] = 0;   // not a reading, only English
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

    // Sort keys: the first reading / meaning, copied out (the keys hold lists).
    // They live in a scratch pool that outlives the sort.
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
                while(*_list != 0 && !isalnum((unsigned char)*_list)) {
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
        // Pointers only once the pool has stopped growing.
        for(u32 _i = 0; _i < _count; _i++) {
            _items[_i].key = &((const c8*)_firsts.memory)[((const u32*)_offsets.memory)[_i]];
        }
        rde_arr_free(&_offsets);
    }

    int (*_cmp)(const void*, const void*) = fude_catalog_cmp_default;
    if(_searching)                     { _cmp = fude_catalog_cmp_score; }
    else if(_sort == FUDE_SORT_STROKES) { _cmp = fude_catalog_cmp_strokes; }
    else if(_sort != FUDE_SORT_DEFAULT) { _cmp = fude_catalog_cmp_key; }
    qsort(_items, _count, sizeof(fude_catalog_sort_item), _cmp);

    for(u32 _i = 0; _i < _count; _i++) {
        u32 _record = _items[_i].entry->record;
        rde_arr_add(&_catalog->results, &_record);
    }

    rde_arr_free(&_firsts);
    return _count;
}

