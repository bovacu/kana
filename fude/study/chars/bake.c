#include "study/chars/bake.h"

#if !defined(RDE_PLATFORM_MOBILE)

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lang/lang.h"
#include "study/chars/catalog.h"
#include "study/handwriting/match.h"

// ===========================================================================
// See bake.h. The sources are parsed into memory (RDE's XML parser on the standard
// heap — the trees are tens of MB), what the app needs kept, one file written.
// ===========================================================================

// --- XML helpers -------------------------------------------------------------------

// --- XML helpers -------------------------------------------------------------------
//
// Attributes are looked up by walking the element's list: rde_xml_get_attribute
// aborts on a missing one, and KanjiVG leaves most of them optional.

const c8* fude_xml_attr(const rde_xml_entry* _e, const c8* _name) {
    for(const rde_xml_entry* _a = _e->attr; _a != NULL; _a = _a->next) {
        if(_a->name != NULL && strcmp(_a->name, _name) == 0) {
            return _a->value;
        }
    }
    return NULL;
}

b8 fude_xml_is(const rde_xml_entry* _e, const c8* _name) {
    return _e->name != NULL && strcmp(_e->name, _name) == 0;
}

const c8* fude_xml_text(const rde_xml_entry* _e) {
    return _e->value != NULL ? _e->value : "";
}

// --- UTF-8 -------------------------------------------------------------------------

// The first code point of _s; *_len gets its byte length. 0 for an empty string.
u32 fude_bake_decode_utf8(const c8* _s, u32* _len) {
    const u8* _u = (const u8*)_s;
    *_len = 0;

    if(_u[0] == 0)            { return 0; }
    if(_u[0] < 0x80u)         { *_len = 1; return _u[0]; }
    if((_u[0] & 0xE0u) == 0xC0u && _u[1]) { *_len = 2; return ((u32)(_u[0] & 0x1Fu) << 6) | (u32)(_u[1] & 0x3Fu); }
    if((_u[0] & 0xF0u) == 0xE0u && _u[1] && _u[2]) { *_len = 3; return ((u32)(_u[0] & 0x0Fu) << 12) | ((u32)(_u[1] & 0x3Fu) << 6) | (u32)(_u[2] & 0x3Fu); }
    if((_u[0] & 0xF8u) == 0xF0u && _u[1] && _u[2] && _u[3]) { *_len = 4; return ((u32)(_u[0] & 0x07u) << 18) | ((u32)(_u[1] & 0x3Fu) << 12) | ((u32)(_u[2] & 0x3Fu) << 6) | (u32)(_u[3] & 0x3Fu); }
    return 0;
}

// --- stroke types --------------------------------------------------------------------

// A CJK Strokes block code point as stored (code - base + 1), 0 when outside it.
RDE_INTERNAL u8 fude_bake_type_code(u32 _cp) {
    return (_cp >= FUDE_KANJI_STROKE_BASE && _cp < FUDE_KANJI_STROKE_BASE + 0x30u) ? (u8)(_cp - FUDE_KANJI_STROKE_BASE + 1u) : 0u;
}

// "㇔", "㇐a", "㇔/㇏" → type, variant letter, alternative.
RDE_INTERNAL void fude_bake_parse_type(const c8* _s, u8* _type, u8* _variant, u8* _alternative) {
    *_type = *_variant = *_alternative = 0;
    if(_s == NULL) {
        return;
    }

    u32 _len = 0;
    *_type = fude_bake_type_code(fude_bake_decode_utf8(_s, &_len));
    _s += _len;

    if(*_s >= 'a' && *_s <= 'z') {
        *_variant = (u8)*_s++;
    }

    if(*_s == '/') {
        _s++;
        *_alternative = fude_bake_type_code(fude_bake_decode_utf8(_s, &_len));
    }
}

// --- SVG paths -----------------------------------------------------------------------
//
// KanjiVG uses only M/m (once, first), C/c and S/s — cubic curves — with a
// command letter repeated implicitly for further coordinate groups.

typedef struct {
    rde_vec_2F start;
    rde_vec_2F segments[FUDE_BAKE_MAX_SEGMENTS][3];   // c1, c2, end
    u32        count;
} fude_bake_path;

RDE_INTERNAL void fude_bake_skip_separators(const c8** _s) {
    while(**_s == ' ' || **_s == ',' || **_s == '\t' || **_s == '\n' || **_s == '\r') {
        (*_s)++;
    }
}

RDE_INTERNAL b8 fude_bake_number(const c8** _s, f32* _out) {
    fude_bake_skip_separators(_s);
    c8* _end = NULL;
    const f32 _v = strtof(*_s, &_end);
    if(_end == *_s) {
        return false;
    }
    *_s   = _end;
    *_out = _v;
    return isfinite(_v) != 0;
}

RDE_INTERNAL b8 fude_bake_numbers(const c8** _s, f32* _out, u32 _n) {
    for(u32 _i = 0; _i < _n; _i++) {
        if(!fude_bake_number(_s, &_out[_i])) {
            return false;
        }
    }
    return true;
}

RDE_INTERNAL b8 fude_bake_parse_path(const c8* _d, fude_bake_path* _path) {
    _path->count = 0;
    if(_d == NULL) {
        return false;
    }

    c8         _cmd     = 0;
    b8         _started = false;
    rde_vec_2F _cur     = { 0.0f, 0.0f };
    rde_vec_2F _last_c2 = { 0.0f, 0.0f };
    b8         _curve   = false;     // the previous segment was a curve (for S)

    for(;;) {
        fude_bake_skip_separators(&_d);
        if(*_d == 0) {
            break;
        }

        if((*_d >= 'A' && *_d <= 'Z') || (*_d >= 'a' && *_d <= 'z')) {
            _cmd = *_d++;
        } else if(_cmd == 0) {
            return false;
        }

        const b8 _rel = _cmd >= 'a' && _cmd <= 'z';
        const rde_vec_2F _o = _rel ? _cur : (rde_vec_2F){ 0.0f, 0.0f };
        f32 _v[6];

        switch(_cmd) {
            case 'M': case 'm': {
                // Only as the first command: a stroke is one continuous line.
                if(_started || !fude_bake_numbers(&_d, _v, 2)) {
                    return false;
                }
                _cur          = (rde_vec_2F){ _o.x + _v[0], _o.y + _v[1] };
                _path->start  = _cur;
                _started      = true;
                _curve        = false;
                // SVG: coordinates after a moveto are linetos. KanjiVG has none.
                _cmd = 0;
            } break;

            case 'C': case 'c': {
                if(!_started || _path->count >= FUDE_BAKE_MAX_SEGMENTS || !fude_bake_numbers(&_d, _v, 6)) {
                    return false;
                }
                const rde_vec_2F _c1 = { _o.x + _v[0], _o.y + _v[1] };
                const rde_vec_2F _c2 = { _o.x + _v[2], _o.y + _v[3] };
                const rde_vec_2F _p  = { _o.x + _v[4], _o.y + _v[5] };
                _path->segments[_path->count][0] = _c1;
                _path->segments[_path->count][1] = _c2;
                _path->segments[_path->count][2] = _p;
                _path->count++;
                _last_c2 = _c2;
                _cur     = _p;
                _curve   = true;
            } break;

            case 'S': case 's': {
                if(!_started || _path->count >= FUDE_BAKE_MAX_SEGMENTS || !fude_bake_numbers(&_d, _v, 4)) {
                    return false;
                }
                // The first control point mirrors the previous curve's second.
                const rde_vec_2F _c1 = _curve ? (rde_vec_2F){ 2.0f * _cur.x - _last_c2.x, 2.0f * _cur.y - _last_c2.y } : _cur;
                const rde_vec_2F _c2 = { _o.x + _v[0], _o.y + _v[1] };
                const rde_vec_2F _p  = { _o.x + _v[2], _o.y + _v[3] };
                _path->segments[_path->count][0] = _c1;
                _path->segments[_path->count][1] = _c2;
                _path->segments[_path->count][2] = _p;
                _path->count++;
                _last_c2 = _c2;
                _cur     = _p;
                _curve   = true;
            } break;

            default: return false;
        }
    }

    return _started;
}

// --- KanjiVG ---------------------------------------------------------------------------

typedef struct {
    const rde_xml_entry* path;
    u32                  number;   // from the id "...-sN": the writing order
} fude_bake_stroke_ref;

RDE_INTERNAL void fude_bake_collect(const rde_xml_entry* _e, fude_bake_stroke_ref* _out, u32* _count, b8* _overflow) {
    for(const rde_xml_entry* _c = _e->child; _c != NULL; _c = _c->next) {
        if(fude_xml_is(_c, "path")) {
            if(*_count >= FUDE_BAKE_MAX_STROKES) {
                *_overflow = true;
                return;
            }
            const c8* _id  = fude_xml_attr(_c, "id");
            const c8* _num = _id != NULL ? strstr(_id, "-s") : NULL;
            _out[*_count] = (fude_bake_stroke_ref){ .path = _c, .number = _num != NULL ? (u32)atoi(_num + 2) : *_count + 1u };
            (*_count)++;
        } else if(fude_xml_is(_c, "g")) {
            fude_bake_collect(_c, _out, _count, _overflow);
        }
    }
}

// A part: one code point, not the character itself, not seen yet.
RDE_INTERNAL void fude_bake_add_part(u32* _parts, u32* _count, const c8* _s, u32 _self) {
    if(_s == NULL || _s[0] == 0) {
        return;
    }
    u32       _len = 0;
    const u32 _cp  = fude_bake_decode_utf8(_s, &_len);
    if(_cp == 0 || _cp == _self || _s[_len] != 0) {
        return;
    }
    for(u32 _i = 0; _i < *_count; _i++) {
        if(_parts[_i] == _cp) {
            return;
        }
    }
    if(*_count < FUDE_KANJI_MAX_PARTS) {
        _parts[(*_count)++] = _cp;
    }
}

// Every element of the group tree, at any depth, and a variant's original.
RDE_INTERNAL void fude_bake_collect_parts(const rde_xml_entry* _e, u32* _parts, u32* _count, u32 _self) {
    for(const rde_xml_entry* _c = _e->child; _c != NULL; _c = _c->next) {
        if(fude_xml_is(_c, "g")) {
            fude_bake_add_part(_parts, _count, fude_xml_attr(_c, "kvg:element"), _self);
            fude_bake_add_part(_parts, _count, fude_xml_attr(_c, "kvg:original"), _self);
            fude_bake_collect_parts(_c, _parts, _count, _self);
        }
    }
}

RDE_INTERNAL i16 fude_bake_fixed(fude_bake* _bake, f32 _v) {
    _bake->min_coord = _v < _bake->min_coord ? _v : _bake->min_coord;
    _bake->max_coord = _v > _bake->max_coord ? _v : _bake->max_coord;
    const f32 _f = roundf(_v * FUDE_KANJI_FIXED);
    return (i16)(_f < -32768.0f ? -32768.0f : (_f > 32767.0f ? 32767.0f : _f));
}

RDE_INTERNAL void fude_bake_put_point(fude_bake* _bake, rde_vec_2F _p) {
    fude_put_i16(&_bake->geometry, fude_bake_fixed(_bake, _p.x));
    fude_put_i16(&_bake->geometry, fude_bake_fixed(_bake, _p.y));
}

b8 fude_bake_kanjivg(fude_bake* _bake, const c8* _path) {
    const f64 _t0 = rde_engine_get_time_now();
    rde_xml_entry* _root = rde_xml_load_from_file(_path, rde_memory_allocator_get_default_std());
    if(_root == NULL) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: could not parse %s", _path);
        return false;
    }
    rde_log_level(RDE_LOG_LEVEL_INFO, "bake: parsed %s in %.2f s", _path, rde_engine_get_time_now() - _t0);

    static fude_bake_path _parsed[FUDE_BAKE_MAX_STROKES];   // big; one character at a time

    for(const rde_xml_entry* _k = _root->child; _k != NULL; _k = _k->next) {
        if(!fude_xml_is(_k, "kanji")) {
            continue;
        }

        const c8* _id  = fude_xml_attr(_k, "id");                  // "kvg:kanji_06728"
        const c8* _hex = _id != NULL ? strstr(_id, "kanji_") : NULL;
        if(_hex == NULL || strchr(_hex, '-') != NULL) {             // a variant, not the character
            continue;
        }
        const u32 _cp = (u32)strtoul(_hex + 6, NULL, 16);

        fude_bake_stroke_ref _refs[FUDE_BAKE_MAX_STROKES];
        u32                  _count    = 0;
        b8                   _overflow = false;
        fude_bake_collect(_k, _refs, &_count, &_overflow);

        // Writing order by the stroke number in the id (document order normally
        // agrees; this does not rely on it). Insertion sort: a few dozen at most.
        for(u32 _i = 1; _i < _count; _i++) {
            fude_bake_stroke_ref _r = _refs[_i];
            u32 _j = _i;
            while(_j > 0 && _refs[_j - 1].number > _r.number) {
                _refs[_j] = _refs[_j - 1];
                _j--;
            }
            _refs[_j] = _r;
        }

        b8 _ok = _count > 0 && !_overflow;
        for(u32 _i = 0; _ok && _i < _count; _i++) {
            _ok = fude_bake_parse_path(fude_xml_attr(_refs[_i].path, "d"), &_parsed[_i]);
        }

        if(!_ok) {
            rde_log_level(RDE_LOG_LEVEL_WARNING, "bake: U+%04X skipped (%s)", _cp, _count == 0 ? "no strokes" : _overflow ? "too many strokes" : "unparsable path");
            _bake->skipped++;
            continue;
        }

        fude_bake_char _char = { .codepoint = _cp, .geometry = fude_bytes_size(&_bake->geometry), .text = UINT32_MAX, .strokes = (u8)_count, .parts = UINT32_MAX, .words = UINT32_MAX };
        for(u32 _l = 0; _l < FUDE_BAKE_LANGS; _l++) {
            _char.meaning_in[_l] = UINT32_MAX;
        }

        u32 _parts[FUDE_KANJI_MAX_PARTS];
        u32 _part_count = 0;
        fude_bake_collect_parts(_k, _parts, &_part_count, _cp);
        if(_part_count > 0) {
            _char.parts = fude_bytes_size(&_bake->parts);
            fude_put_u8(&_bake->parts, (u8)_part_count);
            for(u32 _i = 0; _i < _part_count; _i++) {
                fude_put_u32(&_bake->parts, _parts[_i]);
            }
            _bake->with_parts++;
            _bake->part_refs += _part_count;
        }

        for(u32 _i = 0; _i < _count; _i++) {
            const fude_bake_path* _p = &_parsed[_i];
            u8 _type, _variant, _alternative;
            fude_bake_parse_type(fude_xml_attr(_refs[_i].path, "kvg:type"), &_type, &_variant, &_alternative);

            fude_put_u8(&_bake->geometry, (u8)_p->count);
            fude_put_u8(&_bake->geometry, _type);
            fude_put_u8(&_bake->geometry, _variant);
            fude_put_u8(&_bake->geometry, _alternative);
            fude_bake_put_point(_bake, _p->start);
            for(u32 _s = 0; _s < _p->count; _s++) {
                fude_bake_put_point(_bake, _p->segments[_s][0]);
                fude_bake_put_point(_bake, _p->segments[_s][1]);
                fude_bake_put_point(_bake, _p->segments[_s][2]);
            }

            _bake->strokes++;
            _bake->segments += _p->count;
            _bake->max_segments = _p->count > _bake->max_segments ? _p->count : _bake->max_segments;
        }

        rde_arr_add(&_bake->chars, &_char);
    }

    rde_xml_unload(_root, rde_memory_allocator_get_default_std());
    // KanjiVG is in code point order already; sorted anyway — the lookups and the
    // app's binary search both depend on it.
    qsort(_bake->chars.memory, rde_arr_length(&_bake->chars), sizeof(fude_bake_char), fude_bake_compare);
    return true;
}

// --- files -----------------------------------------------------------------------------

// A whole file on the standard heap (the sources are tens of MB: more than the
// engine's pools hold), freed with free(). NULL when it cannot be read.
u8* fude_bake_slurp(const c8* _path, u32* _size) {
    *_size = 0;
    FILE* _f = fopen(_path, "rb");
    if(_f == NULL) {
        return NULL;
    }
    fseek(_f, 0, SEEK_END);
    const long _n = ftell(_f);
    fseek(_f, 0, SEEK_SET);
    u8* _data = _n > 0 && _n < 0x7FFFFFFFL ? (u8*)malloc((usize)_n) : NULL;
    if(_data != NULL && fread(_data, 1, (usize)_n, _f) != (usize)_n) {
        free(_data);
        _data = NULL;
    }
    fclose(_f);
    *_size = _data != NULL ? (u32)_n : 0u;
    return _data;
}

// --- characters ------------------------------------------------------------------------

int fude_bake_compare(const void* _a, const void* _b) {
    const u32 _x = ((const fude_bake_char*)_a)->codepoint;
    const u32 _y = ((const fude_bake_char*)_b)->codepoint;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

fude_bake_char* fude_bake_find(fude_bake* _bake, u32 _cp) {
    const fude_bake_char _key = { .codepoint = _cp };
    return bsearch(&_key, _bake->chars.memory, rde_arr_length(&_bake->chars), sizeof(fude_bake_char), fude_bake_compare);
}

// Appends _s to _out, preceded by _sep unless it is the first. RDE's XML parser
// hands text over raw, so the five predefined entities are decoded here
// (KANJIDIC2 has a few "&amp;"; nothing else).
void fude_bake_join(c8* _out, usize _size, const c8* _sep, const c8* _s) {
    usize _len = strlen(_out);
    if(_len > 0) {
        snprintf(_out + _len, _size - _len, "%s", _sep);
        _len = strlen(_out);
    }

    static const struct { const c8* entity; c8 c; } _entities[] = {
        { "&amp;", '&' }, { "&lt;", '<' }, { "&gt;", '>' }, { "&quot;", '"' }, { "&apos;", '\'' },
    };

    while(*_s != 0 && _len + 1 < _size) {
        c8 _c = *_s;
        usize _skip = 1;
        if(_c == '&') {
            for(u32 _i = 0; _i < sizeof(_entities) / sizeof(_entities[0]); _i++) {
                const usize _n = strlen(_entities[_i].entity);
                if(strncmp(_s, _entities[_i].entity, _n) == 0) {
                    _c    = _entities[_i].c;
                    _skip = _n;
                    break;
                }
            }
        }
        _out[_len++] = _c;
        _s += _skip;
    }
    _out[_len] = 0;
}

b8 fude_bake_is_kanji(u32 _cp) {
    return (_cp >= 0x4E00u && _cp <= 0x9FFFu) || (_cp >= 0x3400u && _cp <= 0x4DBFu);
}

// 1..6 the school grades, 7 the rest of the Jouyou, 9 names only, 11 none: how
// hard a kanji is to meet, for ranking words by their other kanji.
i32 fude_bake_difficulty(const fude_bake_char* _c) {
    if(_c->grade >= 1 && _c->grade <= 6) { return (i32)_c->grade; }
    if(_c->grade == 8)                   { return 7; }
    if(_c->grade == 9 || _c->grade == 10) { return 9; }
    return 11;
}

// --- example words ---------------------------------------------------------------------

// Numbers: 一月 二月 三月 or 一人 二人 are one example, not four.
RDE_INTERNAL b8 fude_bake_is_numeral(u32 _cp) {
    static const u32 _numerals[] = { 0x4E00, 0x4E8C, 0x4E09, 0x56DB, 0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D, 0x5341, 0x767E, 0x5343, 0x4E07 };   // 一..十 百 千 万
    for(u32 _i = 0; _i < sizeof(_numerals) / sizeof(_numerals[0]); _i++) {
        if(_numerals[_i] == _cp) {
            return true;
        }
    }
    return false;
}

// Does _written hold a numeral other than _cp?
b8 fude_bake_counts(const c8* _written, u32 _cp) {
    for(const c8* _p = _written; *_p != 0;) {
        u32       _len = 0;
        const u32 _c   = fude_bake_decode_utf8(_p, &_len);
        if(_len == 0) {
            break;
        }
        _p += _len;
        if(_c != _cp && fude_bake_is_numeral(_c)) {
            return true;
        }
    }
    return false;
}

RDE_INTERNAL int fude_bake_compare_refs(const void* _a, const void* _b) {
    const fude_bake_word_ref* _x = (const fude_bake_word_ref*)_a;
    const fude_bake_word_ref* _y = (const fude_bake_word_ref*)_b;
    if(_x->codepoint != _y->codepoint) { return _x->codepoint < _y->codepoint ? -1 : 1; }
    if(_x->score != _y->score)         { return _x->score < _y->score ? -1 : 1; }
    return _x->word < _y->word ? -1 : (_x->word > _y->word ? 1 : 0);   // JMdict's own order breaks ties: stable across bakes
}

// A word kept: numbered, its text (and its meanings in the other languages,
// and how common it is) written.
void fude_bake_number_word(fude_bake* _bake, fude_bake_word* _w) {
    _w->index = _bake->word_count++;
    fude_put_data(&_bake->word_text, _w->written, (u32)strlen(_w->written) + 1u);
    fude_put_data(&_bake->word_text, _w->reading, (u32)strlen(_w->reading) + 1u);
    fude_put_data(&_bake->word_text, _w->meaning, (u32)strlen(_w->meaning) + 1u);
    fude_put_u16(&_bake->word_freqs, _w->freq);
    for(u32 _l = 0; _l < _bake->lang_count; _l++) {
        if(_w->meaning_in[_l][0] != 0) {
            fude_put_u32(&_bake->lang_words[_l], _w->index);
            fude_put_u32(&_bake->lang_words[_l], fude_bytes_size(&_bake->lang_text[_l]));
            fude_put_data(&_bake->lang_text[_l], _w->meaning_in[_l], (u32)strlen(_w->meaning_in[_l]) + 1u);
            _bake->lang_word_count[_l]++;
        }
    }
}

// Every candidate in, each kanji keeps its best (see above) and the words kept
// are numbered — then every other common word, on no kanji's list: what text
// is read with (wordsplit.h: 食べる, 行く, 本 ほん...).
void fude_bake_choose_words(fude_bake* _bake) {
    const u32           _refs  = (u32)rde_arr_length(&_bake->word_refs);
    fude_bake_word_ref* _ref   = (fude_bake_word_ref*)_bake->word_refs.memory;
    fude_bake_word*     _words = (fude_bake_word*)_bake->words.memory;
    qsort(_ref, _refs, sizeof(fude_bake_word_ref), fude_bake_compare_refs);

    for(u32 _i = 0; _i < _refs;) {
        u32 _end = _i;
        while(_end < _refs && _ref[_end].codepoint == _ref[_i].codepoint) {
            _end++;
        }

        // The examples first: common, short, not the same number word again nor a
        // longer form of one kept. Then more, up to FUDE_BAKE_WORDS_ALL, in their
        // order: anything not kept yet (the viewer's Add offers those).
        u32 _kept[FUDE_BAKE_WORDS_ALL];
        u32 _count   = 0;
        b8  _counted = false;   // a word with a number kept already
        for(u32 _j = _i; _j < _end && _count < FUDE_BAKE_WORDS_PER; _j++) {
            const fude_bake_word* _w       = &_words[_ref[_j].word];
            const b8              _numeral = fude_bake_counts(_w->written, _ref[_j].codepoint);
            b8                    _ok      = _w->common && _w->chars <= _bake->example_chars && !(_numeral && _counted);
            for(u32 _k = 0; _ok && _k < _count; _k++) {
                const fude_bake_word* _other = &_words[_kept[_k]];
                // The same form again (another entry: 上手 じょうず and うわて), or a
                // longer form of one kept.
                _ok = strcmp(_w->written, _other->written) != 0 && (strlen(_other->written) <= 3u || strstr(_w->written, _other->written) == NULL);
            }
            if(_ok) {
                _kept[_count++] = _ref[_j].word;
                _counted        = _counted || _numeral;
            }
        }
        const u32 _examples = _count;
        for(u32 _j = _i; _j < _end && _count < FUDE_BAKE_WORDS_ALL; _j++) {
            const fude_bake_word* _w  = &_words[_ref[_j].word];
            b8                    _ok = true;
            for(u32 _k = 0; _ok && _k < _count; _k++) {
                _ok = _kept[_k] != _ref[_j].word && strcmp(_w->written, _words[_kept[_k]].written) != 0;
            }
            if(_ok) {
                _kept[_count++] = _ref[_j].word;
            }
        }

        fude_bake_char* _char = fude_bake_find(_bake, _ref[_i].codepoint);
        if(_char != NULL && _count > 0) {
            _char->words = fude_bytes_size(&_bake->word_lists);
            fude_put_u8(&_bake->word_lists, (u8)_count);
            fude_put_u8(&_bake->word_lists, (u8)_examples);
            _bake->example_refs += _examples;
            for(u32 _k = 0; _k < _count; _k++) {
                fude_bake_word* _w = &_words[_kept[_k]];
                if(_w->index == UINT32_MAX) {
                    fude_bake_number_word(_bake, _w);
                }
                fude_put_u32(&_bake->word_lists, _w->index);
            }
            _bake->with_words++;
        }
        _i = _end;
    }

    // Every other common word, on no kanji's list (in JMdict's order).
    const u32 _all = (u32)rde_arr_length(&_bake->words);
    for(u32 _w = 0; _w < _all; _w++) {
        if(_words[_w].index == UINT32_MAX && _words[_w].common) {
            fude_bake_number_word(_bake, &_words[_w]);
            _bake->words_unlisted++;
        }
    }
}

// --- arguments ---------------------------------------------------------------------------

const c8* fude_bake_arg(i32 _argc, c8** _argv, const c8* _prefix, const c8* _default) {
    const usize _len = strlen(_prefix);
    for(i32 _i = 1; _i < _argc; _i++) {
        if(_argv[_i] != NULL && strncmp(_argv[_i], _prefix, _len) == 0) {
            return _argv[_i] + _len;
        }
    }
    return _default;
}

// --- the bake's lifetime --------------------------------------------------------------------

void fude_bake_init(fude_bake* _bake, const c8* const* _lang_codes, u32 _lang_count) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    memset(_bake, 0, sizeof(*_bake));
    _bake->chars      = rde_arr_new(sizeof(fude_bake_char), _heap);
    _bake->geometry   = fude_bytes_new(4u * 1024u * 1024u);
    _bake->text       = fude_bytes_new(512u * 1024u);
    _bake->parts      = fude_bytes_new(256u * 1024u);
    _bake->words      = rde_arr_new(sizeof(fude_bake_word), _heap);
    _bake->word_refs  = rde_arr_new(sizeof(fude_bake_word_ref), _heap);
    _bake->word_lists = fude_bytes_new(128u * 1024u);
    _bake->word_text  = fude_bytes_new(512u * 1024u);
    _bake->word_freqs = fude_bytes_new(128u * 1024u);
    _bake->sentences  = fude_bytes_new(1024u * 1024u);
    _bake->min_coord  = 1e9f;
    _bake->max_coord  = -1e9f;
    _bake->word_chars    = FUDE_BAKE_WORD_CHARS;
    _bake->example_chars = FUDE_BAKE_EXAMPLE_CHARS;
    _bake->lang_count = _lang_count < FUDE_BAKE_LANGS ? _lang_count : FUDE_BAKE_LANGS;
    for(u32 _l = 0; _l < FUDE_BAKE_LANGS; _l++) {
        if(_l < _bake->lang_count) {
            snprintf(_bake->lang_code[_l], sizeof(_bake->lang_code[_l]), "%s", _lang_codes[_l]);
        }
        _bake->lang_text[_l]  = fude_bytes_new(512u * 1024u);
        _bake->lang_words[_l] = fude_bytes_new(128u * 1024u);
    }
}

void fude_bake_free(fude_bake* _bake) {
    rde_arr_free(&_bake->chars);
    rde_arr_free(&_bake->geometry);
    rde_arr_free(&_bake->text);
    rde_arr_free(&_bake->parts);
    rde_arr_free(&_bake->words);
    rde_arr_free(&_bake->word_refs);
    rde_arr_free(&_bake->word_lists);
    rde_arr_free(&_bake->word_text);
    rde_arr_free(&_bake->word_freqs);
    rde_arr_free(&_bake->sentences);
    free(_bake->sentence_words);
    for(u32 _l = 0; _l < FUDE_BAKE_LANGS; _l++) {
        rde_arr_free(&_bake->lang_text[_l]);
        rde_arr_free(&_bake->lang_words[_l]);
    }
    memset(_bake, 0, sizeof(*_bake));
}

// --- writing -----------------------------------------------------------------------------

b8 fude_bake_write(fude_bake* _bake, const c8* _out, const c8* _strokes_out, const c8* _notice, f64 _t0) {
    i32 _rc = 0;
    const u32   _count = (u32)rde_arr_length(&_bake->chars);
    fude_bytes  _file  = fude_bytes_new(FUDE_FILE_HEADER_SIZE + 16u + _count * FUDE_KANJI_RECORD_SIZE + 8u + fude_bytes_size(&_bake->geometry) + 8u + fude_bytes_size(&_bake->text) +
                                        12u + _count * 4u + fude_bytes_size(&_bake->parts) +
                                        20u + _count * 4u + fude_bytes_size(&_bake->word_lists) + fude_bytes_size(&_bake->word_text) +
                                        fude_bytes_size(&_bake->word_freqs) + 16u + fude_bytes_size(&_bake->sentences) + _bake->word_count * 4u + 32u + 1u);
    fude_put_header(&_file, FUDE_KANJI_VERSION, FUDE_KANJI_KIND);

    u32 _chunk = fude_chunk_begin(&_file, FUDE_KANJI_CHUNK_CHARS);
    fude_put_u32(&_file, _count);
    fude_put_u32(&_file, FUDE_KANJI_RECORD_SIZE);
    const fude_bake_char* _chars = (const fude_bake_char*)_bake->chars.memory;
    for(u32 _i = 0; _i < _count; _i++) {
        fude_put_u32(&_file, _chars[_i].codepoint);
        fude_put_u32(&_file, _chars[_i].geometry);
        fude_put_u32(&_file, _chars[_i].text);
        fude_put_u16(&_file, _chars[_i].frequency);
        fude_put_u8(&_file, _chars[_i].strokes);
        fude_put_u8(&_file, _chars[_i].grade);
        fude_put_u8(&_file, _chars[_i].jlpt);
        fude_put_u8(&_file, _chars[_i].radical);
        fude_put_u8(&_file, _chars[_i].level);
        fude_put_u8(&_file, 0u);
    }
    fude_chunk_end(&_file, _chunk);

    if(_strokes_out == NULL) {
        _chunk = fude_chunk_begin(&_file, FUDE_KANJI_CHUNK_GEOM);
        fude_put_data(&_file, _bake->geometry.memory, fude_bytes_size(&_bake->geometry));
        fude_chunk_end(&_file, _chunk);
    }

    _chunk = fude_chunk_begin(&_file, FUDE_KANJI_CHUNK_TEXT);
    fude_put_data(&_file, _bake->text.memory, fude_bytes_size(&_bake->text));
    fude_chunk_end(&_file, _chunk);

    _chunk = fude_chunk_begin(&_file, FUDE_KANJI_CHUNK_PARTS);
    fude_put_u32(&_file, _count);
    for(u32 _i = 0; _i < _count; _i++) {
        fude_put_u32(&_file, _chars[_i].parts);
    }
    fude_put_data(&_file, _bake->parts.memory, fude_bytes_size(&_bake->parts));
    fude_chunk_end(&_file, _chunk);

    if(_bake->word_count > 0) {
        _chunk = fude_chunk_begin(&_file, FUDE_KANJI_CHUNK_WORDS);
        fude_put_u32(&_file, _count);
        for(u32 _i = 0; _i < _count; _i++) {
            fude_put_u32(&_file, _chars[_i].words);
        }
        fude_put_u32(&_file, _bake->word_count);
        fude_put_u32(&_file, fude_bytes_size(&_bake->word_lists));
        fude_put_data(&_file, _bake->word_lists.memory, fude_bytes_size(&_bake->word_lists));
        fude_put_data(&_file, _bake->word_text.memory, fude_bytes_size(&_bake->word_text));
        fude_chunk_end(&_file, _chunk);
        // How common each word is (kanji.h 'WFRQ').
        _chunk = fude_chunk_begin(&_file, FUDE_KANJI_CHUNK_WORD_FREQ);
        fude_put_u32(&_file, _bake->word_count);
        fude_put_data(&_file, _bake->word_freqs.memory, fude_bytes_size(&_bake->word_freqs));
        fude_chunk_end(&_file, _chunk);
        // Their example sentences (kanji.h 'SENT').
        if(_bake->sentence_count > 0 && _bake->sentence_words != NULL) {
            _chunk = fude_chunk_begin(&_file, FUDE_KANJI_CHUNK_SENTENCES);
            fude_put_u32(&_file, _bake->sentence_count);
            fude_put_u32(&_file, fude_bytes_size(&_bake->sentences));
            fude_put_data(&_file, _bake->sentences.memory, fude_bytes_size(&_bake->sentences));
            fude_put_u32(&_file, _bake->word_count);
            for(u32 _w = 0; _w < _bake->word_count; _w++) {
                fude_put_u32(&_file, _bake->sentence_words[_w]);
            }
            fude_chunk_end(&_file, _chunk);
        }
    }

    // The other languages: a chunk each (see kanji.h's 'LNxx').
    for(u32 _l = 0; _l < _bake->lang_count; _l++) {
        if(_bake->lang_kanji_count[_l] == 0 && _bake->lang_word_count[_l] == 0) {
            continue;
        }
        const c8* _code = _bake->lang_code[_l];
        _chunk = fude_chunk_begin(&_file, FUDE_TAG('L', 'N', _code[0], _code[1]));
        fude_put_u32(&_file, _bake->lang_kanji_count[_l]);
        for(u32 _i = 0; _i < _count; _i++) {
            if(_chars[_i].meaning_in[_l] != UINT32_MAX) {
                fude_put_u32(&_file, _chars[_i].codepoint);
                fude_put_u32(&_file, _chars[_i].meaning_in[_l]);
            }
        }
        fude_put_u32(&_file, _bake->lang_word_count[_l]);
        fude_put_data(&_file, _bake->lang_words[_l].memory, fude_bytes_size(&_bake->lang_words[_l]));
        fude_put_u32(&_file, fude_bytes_size(&_bake->lang_text[_l]));
        fude_put_data(&_file, _bake->lang_text[_l].memory, fude_bytes_size(&_bake->lang_text[_l]));
        fude_chunk_end(&_file, _chunk);
        rde_log_color(RDE_LOG_COLOR_GREEN, "  %s: %u kanji meanings, %u word meanings, %.2f MB", _code, _bake->lang_kanji_count[_l], _bake->lang_word_count[_l],
                      (f64)(fude_bytes_size(&_bake->lang_text[_l]) + fude_bytes_size(&_bake->lang_words[_l])) / (1024.0 * 1024.0));
    }

    const u32 _geometry_bytes = fude_bytes_size(&_bake->geometry);
    const u32 _text_bytes     = fude_bytes_size(&_bake->text);
    rde_file_create_missing_dirs(_out);
    u32 _bytes = 0;
    if(!fude_bytes_write_and_free(&_file, _out, &_bytes)) {
        _rc = 1;
    } else {
        // The atomic write keeps the previous file as .bak; the data ships in
        // the app's assets, where a backup would ship too — and the sources
        // can always bake it again.
        c8 _bak[RDE_MAX_PATH];
        snprintf(_bak, sizeof(_bak), "%s.bak", _out);
        remove(_bak);

        rde_log_color(RDE_LOG_COLOR_GREEN,
                      "bake: wrote %s: %.2f MB in %.2f s\n"
                      "  %u characters (%u skipped), %u with readings and meanings, %u stroke-count disagreements\n"
                      "  levels 1..9: %u %u %u %u %u %u %u %u %u (%u listed but without strokes)\n"
                      "  %u strokes, %u curve segments (max %u in one stroke)\n"
                      "  %u characters with parts, %u parts in all\n"
                      "  %u words for %u kanji, %u of them examples (from %u dictionary entries): %.2f MB\n"
                      "  geometry %.2f MB, text %.2f MB; coordinates %.2f .. %.2f",
                      _out, (f64)_bytes / (1024.0 * 1024.0), rde_engine_get_time_now() - _t0,
                      _count, _bake->skipped, _bake->with_info, _bake->count_mismatch,
                      _bake->level_listed[1], _bake->level_listed[2], _bake->level_listed[3], _bake->level_listed[4], _bake->level_listed[5],
                      _bake->level_listed[6], _bake->level_listed[7], _bake->level_listed[8], _bake->level_listed[9], _bake->level_missing,
                      _bake->strokes, _bake->segments, _bake->max_segments,
                      _bake->with_parts, _bake->part_refs,
                      _bake->word_count, _bake->with_words, _bake->example_refs, _bake->word_entries,
                      (f64)(fude_bytes_size(&_bake->word_lists) + fude_bytes_size(&_bake->word_text)) / (1024.0 * 1024.0),
                      (f64)_geometry_bytes / (1024.0 * 1024.0), (f64)_text_bytes / (1024.0 * 1024.0),
                      (f64)_bake->min_coord, (f64)_bake->max_coord);
    rde_log_color(RDE_LOG_COLOR_GREEN, "  of the words, %u common ones on no kanji's list (kept for reading text: wordsplit.h)", _bake->words_unlisted);
    }

    // The strokes in a file of their own: the licence's notice, then them.
    if(_rc == 0 && _strokes_out != NULL) {
        const u32  _notice_len = _notice != NULL ? (u32)strlen(_notice) + 1u : 1u;
        fude_bytes _strokes    = fude_bytes_new(FUDE_FILE_HEADER_SIZE + 16u + _notice_len + fude_bytes_size(&_bake->geometry));
        fude_put_header(&_strokes, FUDE_KANJI_VERSION, FUDE_KANJI_STROKES_KIND);
        u32 _chunk = fude_chunk_begin(&_strokes, FUDE_KANJI_CHUNK_NOTE);
        fude_put_data(&_strokes, _notice != NULL ? _notice : "", _notice_len);
        fude_chunk_end(&_strokes, _chunk);
        _chunk = fude_chunk_begin(&_strokes, FUDE_KANJI_CHUNK_GEOM);
        fude_put_data(&_strokes, _bake->geometry.memory, fude_bytes_size(&_bake->geometry));
        fude_chunk_end(&_strokes, _chunk);
        rde_file_create_missing_dirs(_strokes_out);
        u32 _bytes = 0;
        if(!fude_bytes_write_and_free(&_strokes, _strokes_out, &_bytes)) {
            _rc = 1;
        } else {
            c8 _bak[RDE_MAX_PATH];
            snprintf(_bak, sizeof(_bak), "%s.bak", _strokes_out);
            remove(_bak);
            rde_log_color(RDE_LOG_COLOR_GREEN, "bake: wrote %s: the strokes, %.2f MB", _strokes_out, (f64)_bytes / (1024.0 * 1024.0));
        }
    }
    return _rc == 0;
}

// --- look-alikes ----------------------------------------------------------------------------
// Each common character's closest by the matcher (match.h: its reference strokes
// ranked against the rest), appended to the file just written as 'LOOK' — the
// matcher reads the file, so it comes last. What is common, and how close is
// close: the language's rules (fude_bake_look). Kept: up to FUDE_KANJI_LOOKALIKES
// common ones of the same kind (a script's letters, or not), of nearly as many
// strokes, close enough — the others looked for in their own group (lang.h).

b8 fude_bake_lookalikes(const c8* _path, const fude_bake_look* _look) {
    const f64     _t0 = rde_engine_get_time_now();
    fude_kanji_db _db;
    if(!fude_kanji_load(&_db, _path)) {
        return false;
    }
    fude_catalog _catalog;
    fude_catalog_init(&_catalog, &_db);
    fude_bytes _chunk = fude_bytes_new(_db.count * FUDE_KANJI_LOOKALIKES * 4u + 16u);
    const u32  _at    = fude_chunk_begin(&_chunk, FUDE_KANJI_CHUNK_LOOK);
    fude_put_u32(&_chunk, _db.count);
    u32 _with = 0;
    for(u32 _r = 0; _r < _db.count; _r++) {
        fude_kanji_info _info;
        u32             _kept[FUDE_KANJI_LOOKALIKES] = { 0 };
        u32             _n = 0;
        if(fude_kanji_at(&_db, _r, &_info) && _info.strokes > 0u && _look->common(&_info)) {
            const b8          _kana = _look->script(_info.codepoint);
            fude_match_stroke _strokes[64];
            const u32         _count = fude_match_reference(&_db, &_info, _strokes, 64u);
            fude_match_result _ranked[16];
            const u32         _m = fude_match_rank_strokes(&_db, &_catalog, _kana ? FUDE_FILTER_ALL : fude_filter_group(fude_lang_group(_info.codepoint)), _strokes, _count, _ranked, 16u);
            for(u32 _i = 0; _i < _m && _n < FUDE_KANJI_LOOKALIKES && _ranked[_i].cost <= (_kana ? _look->cost_script : _look->cost); _i++) {
                fude_kanji_info _other;
                if(_ranked[_i].record == _r || !fude_kanji_at(&_db, _ranked[_i].record, &_other) || !_look->common(&_other) ||
                   _look->script(_other.codepoint) != _kana ||
                   (_other.strokes > _info.strokes ? _other.strokes - _info.strokes : _info.strokes - _other.strokes) > _look->strokes) {
                    continue;
                }
                _kept[_n++] = _other.codepoint;
            }
        }
        for(u32 _k = 0; _k < FUDE_KANJI_LOOKALIKES; _k++) {
            fude_put_u32(&_chunk, _kept[_k]);
        }
        _with += _n > 0u ? 1u : 0u;
    }
    fude_chunk_end(&_chunk, _at);
    fude_match_release();
    fude_catalog_destroy(&_catalog);
    fude_kanji_unload(&_db);

    // The file again, the chunk at its end.
    u32 _size = 0;
    u8* _data = fude_bake_slurp(_path, &_size);
    if(_data == NULL) {
        rde_arr_free(&_chunk);
        return false;
    }
    fude_bytes _file = fude_bytes_new(_size + fude_bytes_size(&_chunk));
    fude_put_data(&_file, _data, _size);
    fude_put_data(&_file, _chunk.memory, fude_bytes_size(&_chunk));
    free(_data);
    rde_arr_free(&_chunk);
    u32 _bytes = 0;
    if(!fude_bytes_write_and_free(&_file, _path, &_bytes)) {
        return false;
    }
    c8 _bak[RDE_MAX_PATH];
    snprintf(_bak, sizeof(_bak), "%s.bak", _path);
    remove(_bak);
    rde_log_color(RDE_LOG_COLOR_GREEN, "bake: look-alikes for %u characters (%.2f s); %s now %.2f MB", _with, rde_engine_get_time_now() - _t0, _path,
                  (f64)_bytes / (1024.0 * 1024.0));
    return true;
}


// --- prepared tables (bake.h) ---------------------------------------------------------------

#define FUDE_BAKE_LINE 16384u   // a table's line, at most (longer: cut)

// The next line of _data from *_at (its '\n', and a '\r' before it, gone) into
// _line; false at the end. '#' lines and empty ones are passed over.
RDE_INTERNAL b8 fude_bake_line(const u8* _data, u32 _size, u32* _at, c8* _line) {
    while(*_at < _size) {
        u32 _end = *_at;
        while(_end < _size && _data[_end] != '\n') {
            _end++;
        }
        u32 _n = _end - *_at;
        if(_n > 0 && _data[*_at + _n - 1u] == '\r') {
            _n--;
        }
        _n = _n < FUDE_BAKE_LINE - 1u ? _n : FUDE_BAKE_LINE - 1u;
        memcpy(_line, &_data[*_at], _n);
        _line[_n] = 0;
        *_at = _end + 1u;
        if(_n > 0 && _line[0] != '#') {
            return true;
        }
    }
    return false;
}

// The tab-separated fields of _line, in place (the tabs become NULs): at most
// _max into _out, the rest "" . How many there were.
RDE_INTERNAL u32 fude_bake_fields(c8* _line, c8** _out, u32 _max) {
    u32 _n = 0;
    c8* _p = _line;
    while(_n < _max) {
        _out[_n++] = _p;
        c8* _tab = strchr(_p, '\t');
        if(_tab == NULL) {
            break;
        }
        *_tab = 0;
        _p    = _tab + 1;
    }
    for(u32 _i = _n; _i < _max; _i++) {
        _out[_i] = (c8*)"";
    }
    return _n;
}

// A UTF-8 string's length in characters.
RDE_INTERNAL u32 fude_bake_utf8_count(const c8* _s) {
    u32 _n = 0;
    for(; *_s != 0; _s++) {
        _n += ((u8)*_s & 0xC0u) != 0x80u ? 1u : 0u;
    }
    return _n;
}

b8 fude_bake_prepared(fude_bake* _bake, const c8* _dir) {
    c8   _path[RDE_MAX_PATH];
    u32  _size = 0;
    u8*  _data = NULL;
    u32  _at   = 0;
    c8*  _line = (c8*)malloc(FUDE_BAKE_LINE);
    c8*  _f[16 + FUDE_BAKE_LANGS];
    const u32 _langs = _bake->lang_count;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();

    // The characters: their readings, meanings and levels.
    snprintf(_path, sizeof(_path), "%s/chars.tsv", _dir);
    _data = fude_bake_slurp(_path, &_size);
    if(_data == NULL || _line == NULL) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: %s not found (the language's tools make it)", _path);
        free(_data);
        free(_line);
        return false;
    }
    while(fude_bake_line(_data, _size, &_at, _line)) {
        fude_bake_fields(_line, _f, 8u + _langs);
        const u32 _level = (u32)atoi(_f[2]);
        fude_bake_char* _char = fude_bake_find(_bake, (u32)strtoul(_f[0], NULL, 16));
        if(_char == NULL) {
            _bake->level_missing += _level != 0u ? 1u : 0u;
            continue;   // no strokes for it: nothing to practise
        }
        _char->grade     = (u8)atoi(_f[1]);
        _char->level     = (u8)(_level < 16u ? _level : 0u);
        _char->radical   = (u8)atoi(_f[3]);
        _char->frequency = (u16)atoi(_f[4]);
        _bake->level_listed[_char->level]++;
        if(_f[5][0] == 0 && _f[6][0] == 0 && _f[7][0] == 0) {
            continue;   // a script's letter (Korean's syllables): no text, as Kana's kana (fude_lang_latin names it)
        }
        _char->text      = fude_bytes_size(&_bake->text);
        fude_put_data(&_bake->text, _f[5], (u32)strlen(_f[5]) + 1u);
        fude_put_data(&_bake->text, _f[6], (u32)strlen(_f[6]) + 1u);
        fude_put_data(&_bake->text, _f[7], (u32)strlen(_f[7]) + 1u);
        for(u32 _l = 0; _l < _langs; _l++) {
            if(_f[8u + _l][0] != 0) {
                _char->meaning_in[_l] = fude_bytes_size(&_bake->lang_text[_l]);
                fude_put_data(&_bake->lang_text[_l], _f[8u + _l], (u32)strlen(_f[8u + _l]) + 1u);
                _bake->lang_kanji_count[_l]++;
            }
        }
        _bake->with_info++;
    }
    free(_data);

    // The words: each table line's word (UINT32_MAX: one not kept: too long).
    rde_arr TYPE(u32) _word_of     = rde_arr_new(sizeof(u32), _heap);
    rde_arr TYPE(i32) _sentence_of = rde_arr_new(sizeof(i32), _heap);   // by bake word
    snprintf(_path, sizeof(_path), "%s/words.tsv", _dir);
    _data = fude_bake_slurp(_path, &_size);
    _at   = 0;
    while(_data != NULL && fude_bake_line(_data, _size, &_at, _line)) {
        fude_bake_fields(_line, _f, 6u + _langs);
        u32 _none = UINT32_MAX;
        const u32 _chars = fude_bake_utf8_count(_f[0]);
        fude_bake_word _word = { .index = UINT32_MAX, .chars = (u8)(_chars < 255u ? _chars : 255u) };
        if(_chars == 0u || strlen(_f[0]) >= sizeof(_word.written) || _chars > _bake->word_chars) {
            rde_arr_add(&_word_of, &_none);
            continue;
        }
        snprintf(_word.written, sizeof(_word.written), "%s", _f[0]);
        snprintf(_word.reading, sizeof(_word.reading), "%s", _f[1]);
        snprintf(_word.meaning, sizeof(_word.meaning), "%s", _f[2]);
        for(u32 _l = 0; _l < _langs; _l++) {
            snprintf(_word.meaning_in[_l], sizeof(_word.meaning_in[_l]), "%s", _f[3u + _l]);
        }
        const i32 _freq = atoi(_f[3u + _langs]);
        _word.freq   = (u16)(_freq < 0 ? 0 : _freq > 0xFFFE ? 0xFFFE : _freq);
        _word.common = atoi(_f[4u + _langs]) != 0;
        const i32 _sentence = _f[5u + _langs][0] != 0 ? atoi(_f[5u + _langs]) : -1;
        const u32 _index    = (u32)rde_arr_length(&_bake->words);
        rde_arr_add(&_bake->words, &_word);
        rde_arr_add(&_word_of, &_index);
        rde_arr_add(&_sentence_of, &_sentence);
        _bake->word_entries++;
    }
    free(_data);
    const u32* _words_by_line = (const u32*)_word_of.memory;
    const u32  _lines         = (u32)rde_arr_length(&_word_of);
    fude_bake_word* _words    = (fude_bake_word*)_bake->words.memory;

    // Each character's list: its examples first, then more to add.
    snprintf(_path, sizeof(_path), "%s/lists.tsv", _dir);
    _data = fude_bake_slurp(_path, &_size);
    _at   = 0;
    while(_data != NULL && fude_bake_line(_data, _size, &_at, _line)) {
        c8* _l[2 + FUDE_BAKE_WORDS_ALL];
        const u32 _n = fude_bake_fields(_line, _l, 2u + FUDE_BAKE_WORDS_ALL);
        fude_bake_char* _char = fude_bake_find(_bake, (u32)strtoul(_l[0], NULL, 16));
        if(_char == NULL || _n < 3u) {
            continue;
        }
        u32 _kept[FUDE_BAKE_WORDS_ALL];
        u32 _count    = 0;
        u32 _examples = (u32)atoi(_l[1]);
        for(u32 _i = 2; _i < _n && _count < FUDE_BAKE_WORDS_ALL; _i++) {
            const u32 _line_at = (u32)atoi(_l[_i]);
            if(_line_at < _lines && _words_by_line[_line_at] != UINT32_MAX) {
                _kept[_count++] = _words_by_line[_line_at];
            } else if(_i - 2u < _examples) {
                _examples--;   // an example not kept: one fewer
            }
        }
        _examples = _examples < _count ? _examples : _count;
        if(_count == 0u) {
            continue;
        }
        _char->words = fude_bytes_size(&_bake->word_lists);
        fude_put_u8(&_bake->word_lists, (u8)_count);
        fude_put_u8(&_bake->word_lists, (u8)_examples);
        _bake->example_refs += _examples;
        for(u32 _k = 0; _k < _count; _k++) {
            fude_bake_word* _w = &_words[_kept[_k]];
            if(_w->index == UINT32_MAX) {
                fude_bake_number_word(_bake, _w);
            }
            fude_put_u32(&_bake->word_lists, _w->index);
        }
        _bake->with_words++;
    }
    free(_data);
    // Every other common word, on no list: what text is read with (wordsplit.h).
    const u32 _all = (u32)rde_arr_length(&_bake->words);
    for(u32 _w = 0; _w < _all; _w++) {
        if(_words[_w].index == UINT32_MAX && _words[_w].common) {
            fude_bake_number_word(_bake, &_words[_w]);
            _bake->words_unlisted++;
        }
    }

    // The sentences, and each kept word's.
    snprintf(_path, sizeof(_path), "%s/sentences.tsv", _dir);
    _data = fude_bake_slurp(_path, &_size);
    _at   = 0;
    while(_data != NULL && fude_bake_line(_data, _size, &_at, _line)) {
        fude_bake_fields(_line, _f, 5u);
        for(u32 _s = 0; _s < 5u; _s++) {
            fude_put_data(&_bake->sentences, _f[_s], (u32)strlen(_f[_s]) + 1u);
        }
        _bake->sentence_count++;
    }
    free(_data);
    if(_bake->sentence_count > 0u && _bake->word_count > 0u) {
        _bake->sentence_words = (u32*)malloc(sizeof(u32) * _bake->word_count);
        const i32* _sentences = (const i32*)_sentence_of.memory;
        for(u32 _w = 0; _bake->sentence_words != NULL && _w < _bake->word_count; _w++) {
            _bake->sentence_words[_w] = UINT32_MAX;
        }
        for(u32 _w = 0; _bake->sentence_words != NULL && _w < _all; _w++) {
            if(_words[_w].index != UINT32_MAX && _sentences[_w] >= 0 && (u32)_sentences[_w] < _bake->sentence_count) {
                _bake->sentence_words[_words[_w].index] = (u32)_sentences[_w];
            }
        }
    }

    rde_arr_free(&_word_of);
    rde_arr_free(&_sentence_of);
    free(_line);
    return true;
}

#endif
