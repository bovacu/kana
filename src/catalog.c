#include "catalog.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See catalog.h.
// ===========================================================================

#define KANA_CATALOG_SEPARATOR 0x3001u      // 、 between readings
#define KANA_CATALOG_NO_MATCH  UINT32_MAX

// --- keys ------------------------------------------------------------------------

RDE_INTERNAL u32 kana_catalog_hiragana(u32 _cp) {
    return (_cp >= 0x30A1u && _cp <= 0x30F6u) ? _cp - 0x60u : _cp;   // katakana → hiragana
}

RDE_INTERNAL void kana_catalog_put_cp(rde_arr* _pool, u32 _cp) {
    c8 _utf8[5];
    kana_kanji_utf8(_cp, _utf8);
    for(u32 _i = 0; _utf8[_i] != 0; _i++) {
        rde_arr_add(_pool, &_utf8[_i]);
    }
}

RDE_INTERNAL u32 kana_catalog_begin_key(rde_arr* _pool) {
    return (u32)rde_arr_length(_pool);
}

RDE_INTERNAL void kana_catalog_end_key(rde_arr* _pool) {
    c8 _nul = 0;
    rde_arr_add(_pool, &_nul);
}

// Does the reading starting at _r (up to the next 、) have an okurigana mark?
RDE_INTERNAL b8 kana_catalog_has_okurigana(const c8* _r) {
    const c8* _end = strstr(_r, "、");
    const c8* _dot = strchr(_r, '.');
    return _dot != NULL && (_end == NULL || _dot < _end);
}

// A reading list as a key: hiragana, marks removed. _stems: only the part before
// the okurigana mark, and only of readings that have one.
RDE_INTERNAL u32 kana_catalog_reading_key(rde_arr* _pool, const c8* _text, b8 _stems) {
    const u32 _at      = kana_catalog_begin_key(_pool);
    b8        _written = false;   // this reading has put something out
    b8        _in_okuri = false;
    b8        _has_okuri = false;

    // Stems need to know whether a reading HAS a '.', before writing it.
    const c8* _reading_start = _text;

    for(;;) {
        const c8* _before = _text;
        const u32 _cp     = kana_kanji_utf8_next(&_text);

        if(_cp == 0 || _cp == KANA_CATALOG_SEPARATOR) {
            if(_cp == 0) {
                break;
            }
            _in_okuri = false;
            _reading_start = _text;
            // Separator only between readings that were written.
            if(_written) {
                kana_catalog_put_cp(_pool, KANA_CATALOG_SEPARATOR);
                _written = false;
            }
            continue;
        }

        if(_stems && _before == _reading_start) {
            _has_okuri = kana_catalog_has_okurigana(_before);   // a new reading
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

        kana_catalog_put_cp(_pool, kana_catalog_hiragana(_cp));
        _written = true;
    }

    // A trailing separator (the last reading wrote nothing) is harmless: it just
    // yields an empty reading, which never matches.
    kana_catalog_end_key(_pool);
    return _at;
}

RDE_INTERNAL u32 kana_catalog_meaning_key(rde_arr* _pool, const c8* _text) {
    const u32 _at = kana_catalog_begin_key(_pool);
    for(; *_text != 0; _text++) {
        c8 _c = (c8)tolower((unsigned char)*_text);
        rde_arr_add(_pool, &_c);
    }
    kana_catalog_end_key(_pool);
    return _at;
}

// --- building --------------------------------------------------------------------

RDE_INTERNAL u32 kana_catalog_group(u32 _cp) {
    if(_cp >= 0x3041u && _cp <= 0x3096u) { return KANA_GROUP_HIRAGANA; }
    if(_cp >= 0x30A1u && _cp <= 0x30FAu) { return KANA_GROUP_KATAKANA; }
    if((_cp >= 0x4E00u && _cp <= 0x9FFFu) || (_cp >= 0x3400u && _cp <= 0x4DBFu) ||
       (_cp >= 0xF900u && _cp <= 0xFAFFu) || (_cp >= 0x20000u && _cp <= 0x2FFFFu)) {
        return KANA_GROUP_KANJI;
    }
    return UINT32_MAX;
}

typedef struct {
    const kana_catalog_entry* entry;
    const c8*                 key;     // sort key, or NULL
    u32                       score;   // search score (lower is better)
} kana_catalog_sort_item;

void kana_catalog_init(kana_catalog* _catalog, const kana_kanji_db* _db) {
    memset(_catalog, 0, sizeof(*_catalog));
    _catalog->db = _db;

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _catalog->entries    = rde_arr_new(sizeof(kana_catalog_entry), _heap);
    _catalog->keys       = rde_arr_new(sizeof(c8), _heap);
    _catalog->results    = rde_arr_new(sizeof(u32), _heap);
    _catalog->_sort      = rde_arr_new(sizeof(kana_catalog_sort_item), _heap);
    _catalog->_by_record = rde_arr_new(sizeof(u32), _heap);

    if(_db == NULL) {
        return;
    }

    u32 _none = UINT32_MAX;
    for(u32 _r = 0; _r < _db->count; _r++) {
        rde_arr_add(&_catalog->_by_record, &_none);

        kana_kanji_info _info;
        if(!kana_kanji_at(_db, _r, &_info)) {
            continue;
        }
        const u32 _group = kana_catalog_group(_info.codepoint);
        if(_group == UINT32_MAX) {
            continue;   // punctuation, Latin: not something to learn
        }

        kana_catalog_entry _e = {
            .record    = _r,
            .codepoint = _info.codepoint,
            .group     = (u8)_group,
            .strokes   = _info.strokes,
            .grade     = _info.grade,
            .jlpt_n    = _info.jlpt_n,
            .frequency = _info.frequency,
        };

        if(_group == KANA_GROUP_KANJI) {
            _e.on       = kana_catalog_reading_key(&_catalog->keys, kana_kanji_on(_db, &_info), false);
            _e.kun      = kana_catalog_reading_key(&_catalog->keys, kana_kanji_kun(_db, &_info), false);
            _e.kun_stem = kana_catalog_reading_key(&_catalog->keys, kana_kanji_kun(_db, &_info), true);
            _e.meaning  = kana_catalog_meaning_key(&_catalog->keys, kana_kanji_meanings(_db, &_info));
        } else {
            // A kana's reading is itself.
            c8 _self[5];
            kana_kanji_utf8(_info.codepoint, _self);
            _e.on       = kana_catalog_reading_key(&_catalog->keys, _self, false);
            _e.kun      = _e.on;
            _e.kun_stem = kana_catalog_reading_key(&_catalog->keys, "", false);
            _e.meaning  = _e.kun_stem;
        }

        ((u32*)_catalog->_by_record.memory)[_r] = (u32)rde_arr_length(&_catalog->entries);
        rde_arr_add(&_catalog->entries, &_e);
    }
}

void kana_catalog_destroy(kana_catalog* _catalog) {
    rde_arr* _arrays[] = { &_catalog->entries, &_catalog->keys, &_catalog->results, &_catalog->_sort, &_catalog->_by_record };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    memset(_catalog, 0, sizeof(*_catalog));
}

const c8* kana_catalog_key(const kana_catalog* _catalog, u32 _offset) {
    return _offset < rde_arr_length(&_catalog->keys) ? &((const c8*)_catalog->keys.memory)[_offset] : "";
}

const kana_catalog_entry* kana_catalog_entry_of(const kana_catalog* _catalog, u32 _record) {
    if(_record >= rde_arr_length(&_catalog->_by_record)) {
        return NULL;
    }
    const u32 _i = ((const u32*)_catalog->_by_record.memory)[_record];
    return _i == UINT32_MAX ? NULL : &((const kana_catalog_entry*)_catalog->entries.memory)[_i];
}

const u32* kana_catalog_results(const kana_catalog* _catalog) {
    return (const u32*)_catalog->results.memory;
}

u32 kana_catalog_result_count(const kana_catalog* _catalog) {
    return (u32)rde_arr_length(&_catalog->results);
}

// --- filtering -------------------------------------------------------------------

RDE_INTERNAL b8 kana_catalog_entry_passes(const kana_catalog_entry* _e, KANA_FILTER_ _filter) {
    switch(_filter) {
        case KANA_FILTER_HIRAGANA: return _e->group == KANA_GROUP_HIRAGANA;
        case KANA_FILTER_KATAKANA: return _e->group == KANA_GROUP_KATAKANA;
        case KANA_FILTER_KANJI:    return _e->group == KANA_GROUP_KANJI;
        case KANA_FILTER_N5:       return _e->jlpt_n == 5;
        case KANA_FILTER_N4:       return _e->jlpt_n == 4;
        case KANA_FILTER_N3:       return _e->jlpt_n == 3;
        case KANA_FILTER_N2:       return _e->jlpt_n == 2;
        case KANA_FILTER_N1:       return _e->jlpt_n == 1;
        default:                   return true;
    }
}

b8 kana_catalog_passes(const kana_catalog* _catalog, u32 _record, KANA_FILTER_ _filter) {
    const kana_catalog_entry* _e = kana_catalog_entry_of(_catalog, _record);
    return _e != NULL && kana_catalog_entry_passes(_e, _filter);
}

// --- sorting ---------------------------------------------------------------------

RDE_INTERNAL u32 kana_catalog_grade_key(u8 _grade) {
    if(_grade >= 1 && _grade <= 6)  { return _grade; }
    if(_grade == 8)                  { return 7; }
    if(_grade == 9 || _grade == 10)  { return 8; }
    return 9;
}

// Hiragana, katakana, then kanji by grade (1-6, rest of the Jouyou, Jinmeiyou,
// ungraded) and frequency.
RDE_INTERNAL int kana_catalog_default_order(const kana_catalog_entry* _a, const kana_catalog_entry* _b) {
    if(_a->group != _b->group) { return _a->group < _b->group ? -1 : 1; }
    const u32 _ga = kana_catalog_grade_key(_a->grade), _gb = kana_catalog_grade_key(_b->grade);
    if(_ga != _gb) { return _ga < _gb ? -1 : 1; }
    const u32 _fa = _a->frequency ? _a->frequency : 100000u, _fb = _b->frequency ? _b->frequency : 100000u;
    if(_fa != _fb) { return _fa < _fb ? -1 : 1; }
    return _a->codepoint < _b->codepoint ? -1 : (_a->codepoint > _b->codepoint ? 1 : 0);
}

RDE_INTERNAL int kana_catalog_cmp_default(const void* _x, const void* _y) {
    return kana_catalog_default_order(((const kana_catalog_sort_item*)_x)->entry, ((const kana_catalog_sort_item*)_y)->entry);
}

RDE_INTERNAL int kana_catalog_cmp_strokes(const void* _x, const void* _y) {
    const kana_catalog_sort_item* _a = _x;
    const kana_catalog_sort_item* _b = _y;
    if(_a->entry->strokes != _b->entry->strokes) { return _a->entry->strokes < _b->entry->strokes ? -1 : 1; }
    return kana_catalog_default_order(_a->entry, _b->entry);
}

// By key (UTF-8 byte order is code point order: gojūon for hiragana, A-Z for
// lower-case Latin); those without one go last.
RDE_INTERNAL int kana_catalog_cmp_key(const void* _x, const void* _y) {
    const kana_catalog_sort_item* _a = _x;
    const kana_catalog_sort_item* _b = _y;
    const b8 _ea = _a->key == NULL || _a->key[0] == 0;
    const b8 _eb = _b->key == NULL || _b->key[0] == 0;
    if(_ea != _eb) { return _ea ? 1 : -1; }
    if(!_ea) {
        const int _c = strcmp(_a->key, _b->key);
        if(_c != 0) { return _c; }
    }
    return kana_catalog_default_order(_a->entry, _b->entry);
}

RDE_INTERNAL int kana_catalog_cmp_score(const void* _x, const void* _y) {
    const kana_catalog_sort_item* _a = _x;
    const kana_catalog_sort_item* _b = _y;
    if(_a->score != _b->score) { return _a->score < _b->score ? -1 : 1; }
    return kana_catalog_default_order(_a->entry, _b->entry);
}

// The first item of a list ("ぼく、もく" → "ぼく"; "tree, wood" → "tree") into
// _out, for sorting by it.
RDE_INTERNAL void kana_catalog_first(const c8* _list, const c8* _separator, c8* _out, usize _size) {
    const c8*   _end = strstr(_list, _separator);
    const usize _len = _end != NULL ? (usize)(_end - _list) : strlen(_list);
    const usize _n   = _len < _size - 1 ? _len : _size - 1;
    memcpy(_out, _list, _n);
    _out[_n] = 0;
}

// --- searching -------------------------------------------------------------------

// How well _q matches a list of items separated by _sep: 0 an item equals it,
// _prefix an item starts with it, _inside it is somewhere inside (only if
// _allow_inside). KANA_CATALOG_NO_MATCH otherwise.
RDE_INTERNAL u32 kana_catalog_match_list(const c8* _list, const c8* _sep, const c8* _q, u32 _prefix, u32 _inside, b8 _allow_inside) {
    const usize _ql   = strlen(_q);
    const usize _sl   = strlen(_sep);
    u32         _best = KANA_CATALOG_NO_MATCH;

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
RDE_INTERNAL u32 kana_catalog_match_meaning(const c8* _meanings, const c8* _q) {
    u32 _best = kana_catalog_match_list(_meanings, ", ", _q, 2, 5, strlen(_q) >= 3);
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

RDE_INTERNAL u32 kana_catalog_score(const kana_catalog* _catalog, const kana_catalog_entry* _e, const c8* _kana, const c8* _english) {
    u32 _best = KANA_CATALOG_NO_MATCH;

    if(_kana != NULL && _kana[0] != 0) {
        const c8* _sep = "、";
        u32 _s = kana_catalog_match_list(kana_catalog_key(_catalog, _e->on),  _sep, _kana, 2, 4, true);
        _best  = _s < _best ? _s : _best;
        _s     = kana_catalog_match_list(kana_catalog_key(_catalog, _e->kun), _sep, _kana, 2, 4, true);
        _best  = _s < _best ? _s : _best;
        _s     = kana_catalog_match_list(kana_catalog_key(_catalog, _e->kun_stem), _sep, _kana, KANA_CATALOG_NO_MATCH, KANA_CATALOG_NO_MATCH, false);
        _best  = (_s == 0 && _best > 1u) ? 1u : _best;   // a stem is almost as good as a reading
    }

    if(_english != NULL && _english[0] != 0) {
        const u32 _s = kana_catalog_match_meaning(kana_catalog_key(_catalog, _e->meaning), _english);
        _best = _s < _best ? _s : _best;
    }

    return _best;
}

u32 kana_catalog_query(kana_catalog* _catalog, KANA_FILTER_ _filter, KANA_SORT_ _sort, const c8* _search) {
    rde_arr_clear(&_catalog->results);
    rde_arr_clear(&_catalog->_sort);

    // The search, as English (lower case, trimmed) and as kana (hiragana): typed
    // kana directly, or romaji that converts completely.
    c8 _english[128] = "";
    c8 _kana[256]    = "";
    if(_search != NULL) {
        while(*_search == ' ') { _search++; }
        usize _n = 0;
        b8    _ascii = true;
        for(const c8* _p = _search; *_p != 0 && _n + 1 < sizeof(_english); _p++) {
            if((unsigned char)*_p >= 0x80u) { _ascii = false; }
            _english[_n++] = (c8)tolower((unsigned char)*_p);
        }
        while(_n > 0 && _english[_n - 1] == ' ') { _n--; }
        _english[_n] = 0;

        if(!_ascii) {
            // Kana typed directly: katakana → hiragana. Not English.
            rde_arr _tmp = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
            const u32 _at = kana_catalog_reading_key(&_tmp, _english, false);
            snprintf(_kana, sizeof(_kana), "%s", &((const c8*)_tmp.memory)[_at]);
            rde_arr_free(&_tmp);
            _english[0] = 0;
        } else if(_english[0] != 0 && !kana_romaji_to_hiragana(_english, _kana, sizeof(_kana))) {
            _kana[0] = 0;   // not a reading, only English
        }
    }
    const b8 _searching = _english[0] != 0 || _kana[0] != 0;

    const kana_catalog_entry* _entries = (const kana_catalog_entry*)_catalog->entries.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_catalog->entries); _i++) {
        const kana_catalog_entry* _e = &_entries[_i];
        if(!kana_catalog_entry_passes(_e, _filter)) {
            continue;
        }

        kana_catalog_sort_item _item = { .entry = _e, .key = NULL, .score = 0 };
        if(_searching) {
            _item.score = kana_catalog_score(_catalog, _e, _kana, _english);
            if(_item.score == KANA_CATALOG_NO_MATCH) {
                continue;
            }
        }
        rde_arr_add(&_catalog->_sort, &_item);
    }

    kana_catalog_sort_item* _items = (kana_catalog_sort_item*)_catalog->_sort.memory;
    const u32               _count = (u32)rde_arr_length(&_catalog->_sort);

    // Sort keys: the first reading / meaning, copied out (the keys hold lists).
    // They live in a scratch pool that outlives the sort.
    rde_arr _firsts = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    if(!_searching && (_sort == KANA_SORT_ON || _sort == KANA_SORT_KUN || _sort == KANA_SORT_MEANING)) {
        rde_arr TYPE(u32) _offsets = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        for(u32 _i = 0; _i < _count; _i++) {
            const kana_catalog_entry* _e = _items[_i].entry;
            const u32 _key = _sort == KANA_SORT_ON ? _e->on : _sort == KANA_SORT_KUN ? _e->kun : _e->meaning;
            const c8* _list = kana_catalog_key(_catalog, _key);
            if(_sort == KANA_SORT_MEANING) {
                // "(boiled) rice" sorts under r, not before a.
                while(*_list != 0 && !isalnum((unsigned char)*_list)) {
                    _list++;
                }
            }
            c8 _first[128];
            kana_catalog_first(_list, _sort == KANA_SORT_MEANING ? ", " : "、", _first, sizeof(_first));
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

    int (*_cmp)(const void*, const void*) = kana_catalog_cmp_default;
    if(_searching)                     { _cmp = kana_catalog_cmp_score; }
    else if(_sort == KANA_SORT_STROKES) { _cmp = kana_catalog_cmp_strokes; }
    else if(_sort != KANA_SORT_DEFAULT) { _cmp = kana_catalog_cmp_key; }
    qsort(_items, _count, sizeof(kana_catalog_sort_item), _cmp);

    for(u32 _i = 0; _i < _count; _i++) {
        u32 _record = _items[_i].entry->record;
        rde_arr_add(&_catalog->results, &_record);
    }

    rde_arr_free(&_firsts);
    return _count;
}

// --- romaji ------------------------------------------------------------------------

typedef struct {
    const c8* romaji;
    const c8* kana;
} kana_romaji_pair;

// Longest spellings first within each length is not needed: the matcher tries
// 4, 3, 2, then 1 letters.
RDE_INTERNAL const kana_romaji_pair kana_romaji_table[] = {
    { "a", "あ" }, { "i", "い" }, { "u", "う" }, { "e", "え" }, { "o", "お" },
    { "ka", "か" }, { "ki", "き" }, { "ku", "く" }, { "ke", "け" }, { "ko", "こ" },
    { "kya", "きゃ" }, { "kyu", "きゅ" }, { "kyo", "きょ" },
    { "ga", "が" }, { "gi", "ぎ" }, { "gu", "ぐ" }, { "ge", "げ" }, { "go", "ご" },
    { "gya", "ぎゃ" }, { "gyu", "ぎゅ" }, { "gyo", "ぎょ" },
    { "sa", "さ" }, { "shi", "し" }, { "si", "し" }, { "su", "す" }, { "se", "せ" }, { "so", "そ" },
    { "sha", "しゃ" }, { "shu", "しゅ" }, { "sho", "しょ" }, { "she", "しぇ" },
    { "sya", "しゃ" }, { "syu", "しゅ" }, { "syo", "しょ" },
    { "za", "ざ" }, { "ji", "じ" }, { "zi", "じ" }, { "zu", "ず" }, { "ze", "ぜ" }, { "zo", "ぞ" },
    { "ja", "じゃ" }, { "ju", "じゅ" }, { "jo", "じょ" }, { "je", "じぇ" },
    { "jya", "じゃ" }, { "jyu", "じゅ" }, { "jyo", "じょ" },
    { "zya", "じゃ" }, { "zyu", "じゅ" }, { "zyo", "じょ" },
    { "ta", "た" }, { "chi", "ち" }, { "ti", "ち" }, { "tsu", "つ" }, { "tu", "つ" }, { "te", "て" }, { "to", "と" },
    { "cha", "ちゃ" }, { "chu", "ちゅ" }, { "cho", "ちょ" }, { "che", "ちぇ" },
    { "tya", "ちゃ" }, { "tyu", "ちゅ" }, { "tyo", "ちょ" },
    { "da", "だ" }, { "di", "ぢ" }, { "du", "づ" }, { "dzu", "づ" }, { "de", "で" }, { "do", "ど" },
    { "dya", "ぢゃ" }, { "dyu", "ぢゅ" }, { "dyo", "ぢょ" },
    { "na", "な" }, { "ni", "に" }, { "nu", "ぬ" }, { "ne", "ね" }, { "no", "の" },
    { "nya", "にゃ" }, { "nyu", "にゅ" }, { "nyo", "にょ" },
    { "ha", "は" }, { "hi", "ひ" }, { "fu", "ふ" }, { "hu", "ふ" }, { "he", "へ" }, { "ho", "ほ" },
    { "hya", "ひゃ" }, { "hyu", "ひゅ" }, { "hyo", "ひょ" },
    { "fa", "ふぁ" }, { "fi", "ふぃ" }, { "fe", "ふぇ" }, { "fo", "ふぉ" },
    { "ba", "ば" }, { "bi", "び" }, { "bu", "ぶ" }, { "be", "べ" }, { "bo", "ぼ" },
    { "bya", "びゃ" }, { "byu", "びゅ" }, { "byo", "びょ" },
    { "pa", "ぱ" }, { "pi", "ぴ" }, { "pu", "ぷ" }, { "pe", "ぺ" }, { "po", "ぽ" },
    { "pya", "ぴゃ" }, { "pyu", "ぴゅ" }, { "pyo", "ぴょ" },
    { "ma", "ま" }, { "mi", "み" }, { "mu", "む" }, { "me", "め" }, { "mo", "も" },
    { "mya", "みゃ" }, { "myu", "みゅ" }, { "myo", "みょ" },
    { "ya", "や" }, { "yu", "ゆ" }, { "yo", "よ" },
    { "ra", "ら" }, { "ri", "り" }, { "ru", "る" }, { "re", "れ" }, { "ro", "ろ" },
    { "rya", "りゃ" }, { "ryu", "りゅ" }, { "ryo", "りょ" },
    { "wa", "わ" }, { "wi", "ゐ" }, { "we", "ゑ" }, { "wo", "を" },
    { "vu", "ゔ" },
    { "xa", "ぁ" }, { "xi", "ぃ" }, { "xu", "ぅ" }, { "xe", "ぇ" }, { "xo", "ぉ" },
    { "xya", "ゃ" }, { "xyu", "ゅ" }, { "xyo", "ょ" }, { "xtsu", "っ" }, { "xtu", "っ" },
};

RDE_INTERNAL b8 kana_romaji_vowel(c8 _c) {
    return _c == 'a' || _c == 'i' || _c == 'u' || _c == 'e' || _c == 'o';
}

RDE_INTERNAL void kana_romaji_append(c8* _out, usize _size, usize* _len, const c8* _kana) {
    const usize _n = strlen(_kana);
    if(*_len + _n + 1 <= _size) {
        memcpy(_out + *_len, _kana, _n);
        *_len += _n;
        _out[*_len] = 0;
    }
}

b8 kana_romaji_to_hiragana(const c8* _romaji, c8* _out, usize _size) {
    usize _len = 0;
    b8    _ok  = true;
    _out[0] = 0;

    // Lower case, no spaces.
    c8    _s[256];
    usize _n = 0;
    for(const c8* _p = _romaji; *_p != 0 && _n + 1 < sizeof(_s); _p++) {
        if(*_p != ' ') {
            _s[_n++] = (c8)tolower((unsigned char)*_p);
        }
    }
    _s[_n] = 0;

    for(usize _i = 0; _i < _n;) {
        const c8 _c = _s[_i];

        if(_c == '-') {
            kana_romaji_append(_out, _size, &_len, "ー");
            _i++;
            continue;
        }

        if(_c == 'n') {
            const c8 _next = _s[_i + 1];
            if(_next == '\'') {                                  // n' — ん, and nothing merges
                kana_romaji_append(_out, _size, &_len, "ん");
                _i += 2;
                continue;
            }
            if(_next == 'n' && !kana_romaji_vowel(_s[_i + 2]) && _s[_i + 2] != 'y') {
                kana_romaji_append(_out, _size, &_len, "ん");    // nn — ん
                _i += 2;
                continue;
            }
            if(_next != 0 && !kana_romaji_vowel(_next) && _next != 'y') {
                kana_romaji_append(_out, _size, &_len, "ん");    // n before a consonant
                _i++;
                continue;
            }
            if(_next == 0) {
                kana_romaji_append(_out, _size, &_len, "ん");    // n at the end
                _i++;
                continue;
            }
        }

        // A doubled consonant is a small っ ("kitte", "matcha").
        if(_c >= 'a' && _c <= 'z' && !kana_romaji_vowel(_c) && _c != 'n' &&
           (_s[_i + 1] == _c || (_c == 't' && _s[_i + 1] == 'c' && _s[_i + 2] == 'h'))) {
            kana_romaji_append(_out, _size, &_len, "っ");
            _i++;
            continue;
        }

        b8 _matched = false;
        for(usize _try = 4; _try >= 1 && !_matched; _try--) {
            if(_i + _try > _n) {
                continue;
            }
            for(u32 _t = 0; _t < sizeof(kana_romaji_table) / sizeof(kana_romaji_table[0]); _t++) {
                if(strlen(kana_romaji_table[_t].romaji) == _try && strncmp(&_s[_i], kana_romaji_table[_t].romaji, _try) == 0) {
                    kana_romaji_append(_out, _size, &_len, kana_romaji_table[_t].kana);
                    _i += _try;
                    _matched = true;
                    break;
                }
            }
        }

        if(!_matched) {
            _ok = false;
            _i++;
        }
    }

    return _ok && _len > 0;
}
