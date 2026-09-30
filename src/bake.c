#include "bake.h"

#include <string.h>

#if !defined(RDE_PLATFORM_MOBILE)

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "kfile.h"
#include "kanji.h"
#include "chart.h"

// ===========================================================================
// See bake.h. Parse both sources into memory (RDE's XML parser, on the standard
// heap — the trees are tens of MB), keep what the app needs, write one file.
// ===========================================================================

#define KANA_BAKE_MAX_STROKES   64u     // more than any character has (the most is ~30s)
#define KANA_BAKE_MAX_SEGMENTS  255u    // a stroke's segment count is a u8

// JMdict: example words.
#define KANA_BAKE_WORDS_PER     6u      // example words kept per kanji (shown)
#define KANA_BAKE_WORDS_ALL     20u     // words kept per kanji in all: after the examples, more to add (the viewer's Add)
#define KANA_BAKE_WORD_CHARS    5u      // longer words are not kept
#define KANA_BAKE_EXAMPLE_CHARS 4u      // ...and examples are shorter still
#define KANA_BAKE_UNCOMMON      1000    // an uncommon word's score starts here: after every common one
#define KANA_BAKE_WORD_MEANING  48u     // glosses after the first are added while the meaning stays this short
#define KANA_BAKE_ENTRY_KEBS    8u      // written forms read per entry (more are rare, and uncommon)
#define KANA_BAKE_ENTRY_REBS    8u
#define KANA_BAKE_ENTRY_SENSES  4u
#define KANA_BAKE_ENTRY_RESTR   4u

typedef struct {
    u32 codepoint;
    u32 geometry;      // offset into the GEOM bytes
    u32 text;          // offset into the TEXT bytes, or UINT32_MAX
    u16 frequency;
    u8  strokes;
    u8  grade;
    u8  jlpt;
    u8  radical;
    u8  jlpt_n;
    u32 parts;         // offset into the PART lists, or UINT32_MAX
    u32 words;         // offset into the WORD lists, or UINT32_MAX
} kana_bake_char;

// A common JMdict word that can be an example: its three strings.
typedef struct {
    c8  written[4u * KANA_BAKE_WORD_CHARS + 1u];
    c8  reading[64];
    c8  meaning[128];
    u32 index;         // in the file, UINT32_MAX until a kanji keeps it
    u8  chars;
    b8  common;        // on a common list: can be an example
} kana_bake_word;

// A word as a candidate example for one of its kanji; lower scores first.
typedef struct {
    u32 codepoint;
    u32 word;
    i32 score;
} kana_bake_word_ref;

typedef struct {
    rde_arr TYPE(kana_bake_char) chars;
    kana_bytes                   geometry;
    kana_bytes                   text;
    kana_bytes                   parts;
    rde_arr TYPE(kana_bake_word)     words;
    rde_arr TYPE(kana_bake_word_ref) word_refs;
    kana_bytes                   word_lists;
    u32                          word_count;   // words kept (numbered in the file)
    kana_bytes                   word_text;

    // Report.
    u32 strokes;
    u32 segments;
    u32 skipped;            // characters dropped for an unparsable path
    u32 with_info;          // matched to a KANJIDIC2 entry
    u32 count_mismatch;     // KanjiVG and KANJIDIC2 disagree on the stroke count
    u32 jlpt_listed[6];     // characters per N-level (index 1..5)
    u32 jlpt_missing;       // listed, but KanjiVG has no strokes for it
    u32 max_segments;
    u32 with_parts;         // characters with at least one part
    u32 part_refs;          // parts over all characters
    u32 word_entries;       // JMdict entries read
    u32 with_words;         // kanji with at least one example word
    u32 example_refs;       // examples over all kanji (the rest: to add)
    f32 min_coord;
    f32 max_coord;
} kana_bake;

// --- XML helpers -------------------------------------------------------------------
//
// Attributes are looked up by walking the element's list: rde_xml_get_attribute
// aborts on a missing one, and KanjiVG leaves most of them optional.

RDE_INTERNAL const c8* kana_xml_attr(const rde_xml_entry* _e, const c8* _name) {
    for(const rde_xml_entry* _a = _e->attr; _a != NULL; _a = _a->next) {
        if(_a->name != NULL && strcmp(_a->name, _name) == 0) {
            return _a->value;
        }
    }
    return NULL;
}

RDE_INTERNAL b8 kana_xml_is(const rde_xml_entry* _e, const c8* _name) {
    return _e->name != NULL && strcmp(_e->name, _name) == 0;
}

RDE_INTERNAL const c8* kana_xml_text(const rde_xml_entry* _e) {
    return _e->value != NULL ? _e->value : "";
}

// --- UTF-8 -------------------------------------------------------------------------

// The first code point of _s; *_len gets its byte length. 0 for an empty string.
RDE_INTERNAL u32 kana_bake_decode_utf8(const c8* _s, u32* _len) {
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
RDE_INTERNAL u8 kana_bake_type_code(u32 _cp) {
    return (_cp >= KANA_KANJI_STROKE_BASE && _cp < KANA_KANJI_STROKE_BASE + 0x30u) ? (u8)(_cp - KANA_KANJI_STROKE_BASE + 1u) : 0u;
}

// "㇔", "㇐a", "㇔/㇏" → type, variant letter, alternative.
RDE_INTERNAL void kana_bake_parse_type(const c8* _s, u8* _type, u8* _variant, u8* _alternative) {
    *_type = *_variant = *_alternative = 0;
    if(_s == NULL) {
        return;
    }

    u32 _len = 0;
    *_type = kana_bake_type_code(kana_bake_decode_utf8(_s, &_len));
    _s += _len;

    if(*_s >= 'a' && *_s <= 'z') {
        *_variant = (u8)*_s++;
    }

    if(*_s == '/') {
        _s++;
        *_alternative = kana_bake_type_code(kana_bake_decode_utf8(_s, &_len));
    }
}

// --- SVG paths -----------------------------------------------------------------------
//
// KanjiVG uses only M/m (once, first), C/c and S/s — cubic curves — with a
// command letter repeated implicitly for further coordinate groups.

typedef struct {
    rde_vec_2F start;
    rde_vec_2F segments[KANA_BAKE_MAX_SEGMENTS][3];   // c1, c2, end
    u32        count;
} kana_bake_path;

RDE_INTERNAL void kana_bake_skip_separators(const c8** _s) {
    while(**_s == ' ' || **_s == ',' || **_s == '\t' || **_s == '\n' || **_s == '\r') {
        (*_s)++;
    }
}

RDE_INTERNAL b8 kana_bake_number(const c8** _s, f32* _out) {
    kana_bake_skip_separators(_s);
    c8* _end = NULL;
    const f32 _v = strtof(*_s, &_end);
    if(_end == *_s) {
        return false;
    }
    *_s   = _end;
    *_out = _v;
    return isfinite(_v) != 0;
}

RDE_INTERNAL b8 kana_bake_numbers(const c8** _s, f32* _out, u32 _n) {
    for(u32 _i = 0; _i < _n; _i++) {
        if(!kana_bake_number(_s, &_out[_i])) {
            return false;
        }
    }
    return true;
}

RDE_INTERNAL b8 kana_bake_parse_path(const c8* _d, kana_bake_path* _path) {
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
        kana_bake_skip_separators(&_d);
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
                if(_started || !kana_bake_numbers(&_d, _v, 2)) {
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
                if(!_started || _path->count >= KANA_BAKE_MAX_SEGMENTS || !kana_bake_numbers(&_d, _v, 6)) {
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
                if(!_started || _path->count >= KANA_BAKE_MAX_SEGMENTS || !kana_bake_numbers(&_d, _v, 4)) {
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
} kana_bake_stroke_ref;

RDE_INTERNAL void kana_bake_collect(const rde_xml_entry* _e, kana_bake_stroke_ref* _out, u32* _count, b8* _overflow) {
    for(const rde_xml_entry* _c = _e->child; _c != NULL; _c = _c->next) {
        if(kana_xml_is(_c, "path")) {
            if(*_count >= KANA_BAKE_MAX_STROKES) {
                *_overflow = true;
                return;
            }
            const c8* _id  = kana_xml_attr(_c, "id");
            const c8* _num = _id != NULL ? strstr(_id, "-s") : NULL;
            _out[*_count] = (kana_bake_stroke_ref){ .path = _c, .number = _num != NULL ? (u32)atoi(_num + 2) : *_count + 1u };
            (*_count)++;
        } else if(kana_xml_is(_c, "g")) {
            kana_bake_collect(_c, _out, _count, _overflow);
        }
    }
}

// A part: one code point, not the character itself, not seen yet.
RDE_INTERNAL void kana_bake_add_part(u32* _parts, u32* _count, const c8* _s, u32 _self) {
    if(_s == NULL || _s[0] == 0) {
        return;
    }
    u32       _len = 0;
    const u32 _cp  = kana_bake_decode_utf8(_s, &_len);
    if(_cp == 0 || _cp == _self || _s[_len] != 0) {
        return;
    }
    for(u32 _i = 0; _i < *_count; _i++) {
        if(_parts[_i] == _cp) {
            return;
        }
    }
    if(*_count < KANA_KANJI_MAX_PARTS) {
        _parts[(*_count)++] = _cp;
    }
}

// Every element of the group tree, at any depth, and a variant's original.
RDE_INTERNAL void kana_bake_collect_parts(const rde_xml_entry* _e, u32* _parts, u32* _count, u32 _self) {
    for(const rde_xml_entry* _c = _e->child; _c != NULL; _c = _c->next) {
        if(kana_xml_is(_c, "g")) {
            kana_bake_add_part(_parts, _count, kana_xml_attr(_c, "kvg:element"), _self);
            kana_bake_add_part(_parts, _count, kana_xml_attr(_c, "kvg:original"), _self);
            kana_bake_collect_parts(_c, _parts, _count, _self);
        }
    }
}

RDE_INTERNAL i16 kana_bake_fixed(kana_bake* _bake, f32 _v) {
    _bake->min_coord = _v < _bake->min_coord ? _v : _bake->min_coord;
    _bake->max_coord = _v > _bake->max_coord ? _v : _bake->max_coord;
    const f32 _f = roundf(_v * KANA_KANJI_FIXED);
    return (i16)(_f < -32768.0f ? -32768.0f : (_f > 32767.0f ? 32767.0f : _f));
}

RDE_INTERNAL void kana_bake_put_point(kana_bake* _bake, rde_vec_2F _p) {
    kana_put_i16(&_bake->geometry, kana_bake_fixed(_bake, _p.x));
    kana_put_i16(&_bake->geometry, kana_bake_fixed(_bake, _p.y));
}

RDE_INTERNAL b8 kana_bake_kanjivg(kana_bake* _bake, const c8* _path) {
    const f64 _t0 = rde_engine_get_time_now();
    rde_xml_entry* _root = rde_xml_load_from_file(_path, rde_memory_allocator_get_default_std());
    if(_root == NULL) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: could not parse %s", _path);
        return false;
    }
    rde_log_level(RDE_LOG_LEVEL_INFO, "bake: parsed %s in %.2f s", _path, rde_engine_get_time_now() - _t0);

    static kana_bake_path _parsed[KANA_BAKE_MAX_STROKES];   // big; one character at a time

    for(const rde_xml_entry* _k = _root->child; _k != NULL; _k = _k->next) {
        if(!kana_xml_is(_k, "kanji")) {
            continue;
        }

        const c8* _id  = kana_xml_attr(_k, "id");                  // "kvg:kanji_06728"
        const c8* _hex = _id != NULL ? strstr(_id, "kanji_") : NULL;
        if(_hex == NULL || strchr(_hex, '-') != NULL) {             // a variant, not the character
            continue;
        }
        const u32 _cp = (u32)strtoul(_hex + 6, NULL, 16);

        kana_bake_stroke_ref _refs[KANA_BAKE_MAX_STROKES];
        u32                  _count    = 0;
        b8                   _overflow = false;
        kana_bake_collect(_k, _refs, &_count, &_overflow);

        // Writing order by the stroke number in the id (document order normally
        // agrees; this does not rely on it). Insertion sort: a few dozen at most.
        for(u32 _i = 1; _i < _count; _i++) {
            kana_bake_stroke_ref _r = _refs[_i];
            u32 _j = _i;
            while(_j > 0 && _refs[_j - 1].number > _r.number) {
                _refs[_j] = _refs[_j - 1];
                _j--;
            }
            _refs[_j] = _r;
        }

        b8 _ok = _count > 0 && !_overflow;
        for(u32 _i = 0; _ok && _i < _count; _i++) {
            _ok = kana_bake_parse_path(kana_xml_attr(_refs[_i].path, "d"), &_parsed[_i]);
        }

        if(!_ok) {
            rde_log_level(RDE_LOG_LEVEL_WARNING, "bake: U+%04X skipped (%s)", _cp, _count == 0 ? "no strokes" : _overflow ? "too many strokes" : "unparsable path");
            _bake->skipped++;
            continue;
        }

        kana_bake_char _char = { .codepoint = _cp, .geometry = kana_bytes_size(&_bake->geometry), .text = UINT32_MAX, .strokes = (u8)_count, .parts = UINT32_MAX, .words = UINT32_MAX };

        u32 _parts[KANA_KANJI_MAX_PARTS];
        u32 _part_count = 0;
        kana_bake_collect_parts(_k, _parts, &_part_count, _cp);
        if(_part_count > 0) {
            _char.parts = kana_bytes_size(&_bake->parts);
            kana_put_u8(&_bake->parts, (u8)_part_count);
            for(u32 _i = 0; _i < _part_count; _i++) {
                kana_put_u32(&_bake->parts, _parts[_i]);
            }
            _bake->with_parts++;
            _bake->part_refs += _part_count;
        }

        for(u32 _i = 0; _i < _count; _i++) {
            const kana_bake_path* _p = &_parsed[_i];
            u8 _type, _variant, _alternative;
            kana_bake_parse_type(kana_xml_attr(_refs[_i].path, "kvg:type"), &_type, &_variant, &_alternative);

            kana_put_u8(&_bake->geometry, (u8)_p->count);
            kana_put_u8(&_bake->geometry, _type);
            kana_put_u8(&_bake->geometry, _variant);
            kana_put_u8(&_bake->geometry, _alternative);
            kana_bake_put_point(_bake, _p->start);
            for(u32 _s = 0; _s < _p->count; _s++) {
                kana_bake_put_point(_bake, _p->segments[_s][0]);
                kana_bake_put_point(_bake, _p->segments[_s][1]);
                kana_bake_put_point(_bake, _p->segments[_s][2]);
            }

            _bake->strokes++;
            _bake->segments += _p->count;
            _bake->max_segments = _p->count > _bake->max_segments ? _p->count : _bake->max_segments;
        }

        rde_arr_add(&_bake->chars, &_char);
    }

    rde_xml_unload(_root, rde_memory_allocator_get_default_std());
    return true;
}

// --- KANJIDIC2 -------------------------------------------------------------------------

RDE_INTERNAL int kana_bake_compare(const void* _a, const void* _b) {
    const u32 _x = ((const kana_bake_char*)_a)->codepoint;
    const u32 _y = ((const kana_bake_char*)_b)->codepoint;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

RDE_INTERNAL kana_bake_char* kana_bake_find(kana_bake* _bake, u32 _cp) {
    const kana_bake_char _key = { .codepoint = _cp };
    return bsearch(&_key, _bake->chars.memory, rde_arr_length(&_bake->chars), sizeof(kana_bake_char), kana_bake_compare);
}

// Appends _s to _out, preceded by _sep unless it is the first. RDE's XML parser
// hands text over raw, so the five predefined entities are decoded here
// (KANJIDIC2 has a few "&amp;"; nothing else).
RDE_INTERNAL void kana_bake_join(c8* _out, usize _size, const c8* _sep, const c8* _s) {
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

RDE_INTERNAL b8 kana_bake_kanjidic(kana_bake* _bake, const c8* _path) {
    const f64 _t0 = rde_engine_get_time_now();
    rde_xml_entry* _root = rde_xml_load_from_file(_path, rde_memory_allocator_get_default_std());
    if(_root == NULL) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: could not parse %s", _path);
        return false;
    }
    rde_log_level(RDE_LOG_LEVEL_INFO, "bake: parsed %s in %.2f s", _path, rde_engine_get_time_now() - _t0);

    for(const rde_xml_entry* _c = _root->child; _c != NULL; _c = _c->next) {
        if(!kana_xml_is(_c, "character")) {
            continue;
        }

        u32 _cp = 0;
        u32 _strokes = 0, _grade = 0, _jlpt = 0, _freq = 0, _radical = 0;
        c8  _on[512] = "", _kun[512] = "", _meanings[1024] = "";

        for(const rde_xml_entry* _f = _c->child; _f != NULL; _f = _f->next) {
            if(kana_xml_is(_f, "literal")) {
                u32 _len;
                _cp = kana_bake_decode_utf8(kana_xml_text(_f), &_len);
            } else if(kana_xml_is(_f, "radical")) {
                for(const rde_xml_entry* _r = _f->child; _r != NULL; _r = _r->next) {
                    const c8* _type = kana_xml_attr(_r, "rad_type");
                    if(kana_xml_is(_r, "rad_value") && _type != NULL && strcmp(_type, "classical") == 0) {
                        _radical = (u32)atoi(kana_xml_text(_r));
                    }
                }
            } else if(kana_xml_is(_f, "misc")) {
                for(const rde_xml_entry* _m = _f->child; _m != NULL; _m = _m->next) {
                    if(kana_xml_is(_m, "grade"))                         { _grade = (u32)atoi(kana_xml_text(_m)); }
                    else if(kana_xml_is(_m, "stroke_count") && !_strokes) { _strokes = (u32)atoi(kana_xml_text(_m)); }   // the first is the accepted one
                    else if(kana_xml_is(_m, "freq"))                     { _freq = (u32)atoi(kana_xml_text(_m)); }
                    else if(kana_xml_is(_m, "jlpt"))                     { _jlpt = (u32)atoi(kana_xml_text(_m)); }
                }
            } else if(kana_xml_is(_f, "reading_meaning")) {
                for(const rde_xml_entry* _g = _f->child; _g != NULL; _g = _g->next) {
                    if(!kana_xml_is(_g, "rmgroup")) {
                        continue;
                    }
                    for(const rde_xml_entry* _x = _g->child; _x != NULL; _x = _x->next) {
                        if(kana_xml_is(_x, "reading")) {
                            const c8* _type = kana_xml_attr(_x, "r_type");
                            if(_type != NULL && strcmp(_type, "ja_on") == 0)  { kana_bake_join(_on,  sizeof(_on),  "、", kana_xml_text(_x)); }
                            if(_type != NULL && strcmp(_type, "ja_kun") == 0) { kana_bake_join(_kun, sizeof(_kun), "、", kana_xml_text(_x)); }
                        } else if(kana_xml_is(_x, "meaning") && kana_xml_attr(_x, "m_lang") == NULL) {   // no m_lang: English
                            kana_bake_join(_meanings, sizeof(_meanings), ", ", kana_xml_text(_x));
                        }
                    }
                }
            }
        }

        kana_bake_char* _char = _cp != 0 ? kana_bake_find(_bake, _cp) : NULL;
        if(_char == NULL) {
            continue;   // no strokes for it: nothing to practise
        }

        _char->grade     = (u8)(_grade   < 256u ? _grade   : 0u);
        _char->jlpt      = (u8)(_jlpt    < 256u ? _jlpt    : 0u);
        _char->radical   = (u8)(_radical < 256u ? _radical : 0u);
        _char->frequency = (u16)(_freq < 65536u ? _freq : 0u);
        _char->text      = kana_bytes_size(&_bake->text);
        kana_put_data(&_bake->text, _on,       (u32)strlen(_on) + 1u);
        kana_put_data(&_bake->text, _kun,      (u32)strlen(_kun) + 1u);
        kana_put_data(&_bake->text, _meanings, (u32)strlen(_meanings) + 1u);

        _bake->with_info++;
        if(_strokes != 0 && _strokes != _char->strokes) {
            _bake->count_mismatch++;
        }
    }

    rde_xml_unload(_root, rde_memory_allocator_get_default_std());
    return true;
}

// --- JLPT N-levels ---------------------------------------------------------------------
//
// Jonathan Waller's lists (tanos.co.uk, CC BY), as the Mnemosyne decks he
// publishes: a Python pickle in plain ASCII. Each card's question is the kanji,
// on the line after "S'q'" as V"\uXXXX" (or a literal character).

RDE_INTERNAL u32 kana_bake_hex(const c8* _s, u32 _digits) {
    u32 _v = 0;
    for(u32 _i = 0; _i < _digits; _i++) {
        const c8  _c = _s[_i];
        const u32 _d = (_c >= '0' && _c <= '9') ? (u32)(_c - '0')
                     : (_c >= 'a' && _c <= 'f') ? (u32)(_c - 'a' + 10)
                     : (_c >= 'A' && _c <= 'F') ? (u32)(_c - 'A' + 10) : 16u;
        if(_d > 15u) {
            return 0;
        }
        _v = (_v << 4) | _d;
    }
    return _v;
}

RDE_INTERNAL void kana_bake_jlpt_level(kana_bake* _bake, const c8* _dir, u32 _level) {
    c8 _path[RDE_MAX_PATH];
    snprintf(_path, sizeof(_path), "%s/n%u-kanji-char-eng.mem", _dir, _level);
    if(!rde_file_exists(_path)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "bake: %s not found; N%u left unlisted", _path, _level);
        return;
    }

    u32 _size = 0;
    u8* _data = kana_file_read(_path, &_size);
    if(_data == NULL) {
        return;
    }

    const c8* _s   = (const c8*)_data;
    const c8* _end = _s + _size;
    while(_s < _end) {
        const c8* _nl   = memchr(_s, '\n', (usize)(_end - _s));
        const c8* _next = _nl != NULL ? _nl + 1 : _end;

        // "S'q'" or "sS'q'" alone on its line: the next line is the question.
        const usize _len  = (usize)((_nl != NULL ? _nl : _end) - _s);
        const b8    _is_q = (_len == 4 && strncmp(_s, "S'q'", 4) == 0) || (_len == 5 && strncmp(_s, "sS'q'", 5) == 0);
        if(_is_q && _next < _end && *_next == 'V') {
            const c8* _v = _next + 1;
            if(*_v == '"') {
                _v++;
            }

            u32 _cp = 0;
            if(_v[0] == '\\' && _v[1] == 'u')      { _cp = kana_bake_hex(_v + 2, 4); }
            else if(_v[0] == '\\' && _v[1] == 'U') { _cp = kana_bake_hex(_v + 2, 8); }
            else                                   { u32 _n; _cp = kana_bake_decode_utf8(_v, &_n); }

            kana_bake_char* _char = _cp != 0 ? kana_bake_find(_bake, _cp) : NULL;
            if(_char == NULL) {
                _bake->jlpt_missing++;
            } else if(_char->jlpt_n == 0) {   // the lists are disjoint; if not, the easier level wins
                _char->jlpt_n = (u8)_level;
                _bake->jlpt_listed[_level]++;
            }
        }
        _s = _next;
    }

    kana_file_free(_data);
}

// --- JMdict: example words -------------------------------------------------------------
//
// EDRDG's JMdict (CC BY-SA 4.0, like KANJIDIC2), the English-only file. 60 MB
// of XML with one element a line, so it is read a line at a time instead of as a
// tree. Only COMMON written forms are kept (a priority mark from the newspaper
// lists, Ichimango or EDRDG's own: news1, ichi1, spec1, spec2), up to four
// characters, whose kanji all have strokes. Each kanji then keeps its best few:
// the most frequent first, the ones whose other kanji are no harder than it, not
// ones usually written in kana, and not a longer form of one already kept
// (日本人 after 日本).

typedef struct {
    c8  keb[KANA_BAKE_ENTRY_KEBS][4u * KANA_BAKE_WORD_CHARS + 1u];
    b8  keb_long[KANA_BAKE_ENTRY_KEBS];     // too long to keep
    b8  keb_bad[KANA_BAKE_ENTRY_KEBS];      // irregular, outdated, rare or search-only
    u32 keb_common[KANA_BAKE_ENTRY_KEBS];   // how many of the common lists have it
    u32 keb_nf[KANA_BAKE_ENTRY_KEBS];       // newspaper frequency band 1..48 (0: none)
    b8  keb_ichi[KANA_BAKE_ENTRY_KEBS];     // on Ichimango's list of everyday words
    u32 kebs;

    c8  reb[KANA_BAKE_ENTRY_REBS][64];
    b8  reb_bad[KANA_BAKE_ENTRY_REBS];      // no kanji, irregular, outdated or search-only
    b8  reb_common[KANA_BAKE_ENTRY_REBS];
    c8  reb_restr[KANA_BAKE_ENTRY_REBS][KANA_BAKE_ENTRY_RESTR][4u * KANA_BAKE_WORD_CHARS + 1u];   // only for these written forms
    u32 reb_restrs[KANA_BAKE_ENTRY_REBS];
    u32 rebs;

    c8  sense[KANA_BAKE_ENTRY_SENSES][128];
    b8  sense_kana[KANA_BAKE_ENTRY_SENSES];  // "usually written in kana"
    c8  sense_stagk[KANA_BAKE_ENTRY_SENSES][KANA_BAKE_ENTRY_RESTR][4u * KANA_BAKE_WORD_CHARS + 1u];
    u32 sense_stagks[KANA_BAKE_ENTRY_SENSES];
    u32 senses;

    u32 element;       // what is open: 0 none, 1 k_ele, 2 r_ele, 3 sense
    b8  sense_full;    // the open sense's glosses are all in
} kana_bake_entry;

// The text of "<tag ...>text</tag>" on _line, into _out (entities decoded). False
// when the line is not that element.
RDE_INTERNAL b8 kana_bake_element(const c8* _line, const c8* _tag, c8* _out, usize _size) {
    const usize _n = strlen(_tag);
    if(_line[0] != '<' || strncmp(_line + 1, _tag, _n) != 0 || (_line[_n + 1] != '>' && _line[_n + 1] != ' ')) {
        return false;
    }
    const c8* _start = strchr(_line, '>');
    const c8* _end   = _start != NULL ? strstr(_start, "</") : NULL;
    if(_end == NULL) {
        return false;
    }
    c8 _raw[512];
    const usize _len = (usize)(_end - _start - 1) < sizeof(_raw) - 1u ? (usize)(_end - _start - 1) : sizeof(_raw) - 1u;
    memcpy(_raw, _start + 1, _len);
    _raw[_len] = 0;
    _out[0] = 0;
    kana_bake_join(_out, _size, "", _raw);
    return true;
}

RDE_INTERNAL b8 kana_bake_is_kanji(u32 _cp) {
    return (_cp >= 0x4E00u && _cp <= 0x9FFFu) || (_cp >= 0x3400u && _cp <= 0x4DBFu);
}

// 1..6 the school grades, 7 the rest of the Jouyou, 9 names only, 11 none: how
// hard a kanji is to meet, for ranking words by their other kanji.
RDE_INTERNAL i32 kana_bake_difficulty(const kana_bake_char* _c) {
    if(_c->grade >= 1 && _c->grade <= 6) { return (i32)_c->grade; }
    if(_c->grade == 8)                   { return 7; }
    if(_c->grade == 9 || _c->grade == 10) { return 9; }
    return 11;
}

// Is every character of _written one an example can hold (kanji with strokes,
// kana, 々), with at least one kanji? Its length in characters into *_chars.
RDE_INTERNAL b8 kana_bake_word_usable(kana_bake* _bake, const c8* _written, u32* _chars) {
    b8  _kanji = false;
    u32 _n     = 0;
    for(const c8* _p = _written; *_p != 0; _n++) {
        u32       _len = 0;
        const u32 _cp  = kana_bake_decode_utf8(_p, &_len);
        if(_cp == 0) {
            return false;
        }
        _p += _len;
        if(kana_bake_is_kanji(_cp)) {
            if(kana_bake_find(_bake, _cp) == NULL) {
                return false;
            }
            _kanji = true;
        } else if(!((_cp >= 0x3041u && _cp <= 0x3096u) || (_cp >= 0x30A1u && _cp <= 0x30FAu) || _cp == 0x30FCu || _cp == 0x3005u)) {
            return false;
        }
    }
    *_chars = _n;
    return _kanji && _n <= KANA_BAKE_WORD_CHARS;
}

// One kanji, at the start, and kana after it: 見る, 読み (and 月 alone).
RDE_INTERNAL b8 kana_bake_own_word(const c8* _written) {
    u32 _kanji = 0;
    u32 _i     = 0;
    for(const c8* _p = _written; *_p != 0; _i++) {
        u32       _len = 0;
        const u32 _cp  = kana_bake_decode_utf8(_p, &_len);
        if(_len == 0) {
            break;
        }
        _p += _len;
        if(kana_bake_is_kanji(_cp)) {
            if(_i != 0) {
                return false;
            }
            _kanji++;
        }
    }
    return _kanji == 1u;
}

// Numbers: 一月 二月 三月 or 一人 二人 are one example, not four.
RDE_INTERNAL b8 kana_bake_is_numeral(u32 _cp) {
    static const u32 _numerals[] = { 0x4E00, 0x4E8C, 0x4E09, 0x56DB, 0x4E94, 0x516D, 0x4E03, 0x516B, 0x4E5D, 0x5341, 0x767E, 0x5343, 0x4E07 };   // 一..十 百 千 万
    for(u32 _i = 0; _i < sizeof(_numerals) / sizeof(_numerals[0]); _i++) {
        if(_numerals[_i] == _cp) {
            return true;
        }
    }
    return false;
}

// Does _written hold a numeral other than _cp?
RDE_INTERNAL b8 kana_bake_counts(const c8* _written, u32 _cp) {
    for(const c8* _p = _written; *_p != 0;) {
        u32       _len = 0;
        const u32 _c   = kana_bake_decode_utf8(_p, &_len);
        if(_len == 0) {
            break;
        }
        _p += _len;
        if(_c != _cp && kana_bake_is_numeral(_c)) {
            return true;
        }
    }
    return false;
}

// Kana → plain romaji, long vowels folded (とうきょう → tokyo), into _out.
RDE_INTERNAL void kana_bake_romanize(const c8* _kana, c8* _out, usize _size) {
    usize _n      = 0;
    b8    _double = false;   // after っ
    _out[0] = 0;
    for(const c8* _p = _kana; *_p != 0;) {
        u32       _len = 0;
        const u32 _cp  = kana_bake_decode_utf8(_p, &_len);
        if(_len == 0) {
            break;
        }
        _p += _len;
        const u32 _h = (_cp >= 0x30A1u && _cp <= 0x30F6u) ? _cp - 0x60u : _cp;   // katakana as hiragana
        if(_h == 0x3063u) { _double = true; continue; }                           // っ
        if(_h == 0x30FCu) { continue; }                                            // ー
        const c8* _r = kana_chart_romaji(_h);
        if(_r == NULL) {
            continue;
        }
        c8 _syllable[8];
        usize _k = 0;
        for(const c8* _q = _r; *_q != 0 && _k + 1u < sizeof(_syllable); _q++) {
            if(*_q != '(' && *_q != ')') {
                _syllable[_k++] = *_q;
            }
        }
        _syllable[_k] = 0;
        // ゃ ゅ ょ fold into the kana before: き+ゃ kya, し+ゃ sha.
        if((_h == 0x3083u || _h == 0x3085u || _h == 0x3087u) && _n > 0 && _out[_n - 1] == 'i') {
            const b8 _palatal = _n >= 2 && (_out[_n - 2] == 'h' || _out[_n - 2] == 'j');
            _n--;
            if(_palatal) {
                snprintf(_syllable, sizeof(_syllable), "%c", _h == 0x3083u ? 'a' : _h == 0x3085u ? 'u' : 'o');
            }
        }
        if(_double && _n + 1u < _size) {
            _out[_n++] = _syllable[0];
        }
        _double = false;
        for(usize _i = 0; _syllable[_i] != 0 && _n + 1u < _size; _i++) {
            _out[_n++] = _syllable[_i];
        }
        _out[_n] = 0;
    }
    // Long vowels: ou, oo, uu, aa, ii to one.
    usize _w = 0;
    for(usize _i = 0; _i < _n; _i++) {
        if(_w > 0 && (_out[_i] == 'u' || _out[_i] == 'o' || _out[_i] == 'a' || _out[_i] == 'i') &&
           ((_out[_w - 1] == 'o' && (_out[_i] == 'u' || _out[_i] == 'o')) || (_out[_w - 1] == _out[_i] && (_out[_i] == 'u' || _out[_i] == 'a' || _out[_i] == 'i')))) {
            continue;
        }
        _out[_w++] = _out[_i];
    }
    _out[_w] = 0;
}

// A name, not a word: the meaning is the reading itself, romanised and
// capitalised — 山形 "Yamagata (city, prefecture)", 読売 "Yomiuri (newspaper…)".
RDE_INTERNAL b8 kana_bake_is_name(const c8* _reading, const c8* _meaning) {
    if(!(_meaning[0] >= 'A' && _meaning[0] <= 'Z')) {
        return false;
    }
    c8    _word[64];
    usize _n = 0;
    for(const c8* _p = _meaning; *_p != 0 && _n + 1u < sizeof(_word); _p++) {
        const c8 _c = *_p;
        if(_c >= 'A' && _c <= 'Z')      { _word[_n++] = (c8)(_c - 'A' + 'a'); }
        else if(_c >= 'a' && _c <= 'z') { _word[_n++] = _c; }
        else if((u8)_c == 0xC5u && (u8)_p[1] == 0x8Du) { _word[_n++] = 'o'; _p++; }   // ō
        else if((u8)_c == 0xC5u && (u8)_p[1] == 0xABu) { _word[_n++] = 'u'; _p++; }   // ū
        else { break; }
    }
    _word[_n] = 0;
    c8 _romaji[128];
    kana_bake_romanize(_reading, _romaji, sizeof(_romaji));
    c8 _folded[64];   // the meaning's word with its long vowels folded the same way
    usize _w = 0;
    for(usize _i = 0; _i < _n; _i++) {
        if(_w > 0 && ((_folded[_w - 1] == 'o' && (_word[_i] == 'u' || _word[_i] == 'o')) || (_folded[_w - 1] == _word[_i] && (_word[_i] == 'u' || _word[_i] == 'a' || _word[_i] == 'i')))) {
            continue;
        }
        _folded[_w++] = _word[_i];
    }
    _folded[_w] = 0;
    return _n >= 3u && strcmp(_folded, _romaji) == 0;
}

RDE_INTERNAL b8 kana_bake_listed(const c8 _list[][4u * KANA_BAKE_WORD_CHARS + 1u], u32 _count, const c8* _s) {
    for(u32 _i = 0; _i < _count; _i++) {
        if(strcmp(_list[_i], _s) == 0) {
            return true;
        }
    }
    return false;
}

// A whole entry read: each common written form becomes a word, a candidate for
// each of its kanji.
RDE_INTERNAL void kana_bake_entry_done(kana_bake* _bake, const kana_bake_entry* _e) {
    _bake->word_entries++;
    for(u32 _k = 0; _k < _e->kebs; _k++) {
        u32 _chars = 0;
        if(_e->keb_bad[_k] || _e->keb_long[_k] || !kana_bake_word_usable(_bake, _e->keb[_k], &_chars)) {
            continue;
        }

        // Its reading: the first that goes with this form, a common one if any.
        i32 _reading = -1;
        for(u32 _r = 0; _r < _e->rebs; _r++) {
            if(_e->reb_bad[_r] || (_e->reb_restrs[_r] > 0 && !kana_bake_listed(_e->reb_restr[_r], _e->reb_restrs[_r], _e->keb[_k]))) {
                continue;
            }
            if(_reading < 0 || (_e->reb_common[_r] && !_e->reb_common[_reading])) {
                _reading = (i32)_r;
            }
        }
        // Its meaning: the first sense that is not for other forms only.
        i32 _sense = -1;
        for(u32 _s = 0; _s < _e->senses && _sense < 0; _s++) {
            if(_e->sense_stagks[_s] == 0 || kana_bake_listed(_e->sense_stagk[_s], _e->sense_stagks[_s], _e->keb[_k])) {
                _sense = (i32)_s;
            }
        }
        if(_reading < 0 || _sense < 0 || _e->sense[_sense][0] == 0) {
            continue;
        }

        kana_bake_word _word = { .index = UINT32_MAX, .chars = (u8)_chars, .common = _e->keb_common[_k] > 0 };
        snprintf(_word.written, sizeof(_word.written), "%s", _e->keb[_k]);
        snprintf(_word.reading, sizeof(_word.reading), "%s", _e->reb[_reading]);
        snprintf(_word.meaning, sizeof(_word.meaning), "%s", _e->sense[_sense]);
        const u32 _index = (u32)rde_arr_length(&_bake->words);
        rde_arr_add(&_bake->words, &_word);

        // How common: the newspaper band when it has one, else about where the
        // other lists sit — everyday words (上 うえ) rarer in the news than its
        // fragments (上げ); a word on more lists is more certainly common. An
        // uncommon word after all of those (the viewer's Add offers them).
        i32 _base;
        if(_word.common) {
            _base = _e->keb_nf[_k] != 0 ? (i32)_e->keb_nf[_k] : _e->keb_ichi[_k] ? 18 : 26;
            _base -= 3 * ((i32)_e->keb_common[_k] - 1);
            // The kanji on its own (月 つき, 上 うえ) first, whatever the news says:
            // its own reading and meaning. Of those, the everyday one (下 した over
            // 下 もと, which the news prefers). With kana after it (見る), a little ahead.
            _base -= _chars == 1u ? (_e->keb_ichi[_k] ? 120 : 100) : kana_bake_own_word(_word.written) ? 6 : 0;
        } else {
            _base = KANA_BAKE_UNCOMMON + (_e->keb_nf[_k] != 0 ? (i32)_e->keb_nf[_k] : 60);
        }
        _base += 3 * ((i32)_chars > 2 ? (i32)_chars - 2 : 0);   // short words first
        _base += _e->sense_kana[_sense] ? 20 : 0;                 // usually kana: a poor example of its kanji
        _base += kana_bake_is_name(_word.reading, _word.meaning) ? 20 : 0;
        _base += _e->reb_common[_reading] ? 0 : 2;                // 下 した (a common reading) before 下 もと

        // A candidate for each of its kanji (once each), less so the harder its
        // other kanji are than that one.
        u32 _seen[KANA_BAKE_WORD_CHARS];
        u32 _seen_count = 0;
        for(const c8* _p = _word.written; *_p != 0;) {
            u32       _len = 0;
            const u32 _cp  = kana_bake_decode_utf8(_p, &_len);
            _p += _len;
            b8 _dup = !kana_bake_is_kanji(_cp);
            for(u32 _i = 0; !_dup && _i < _seen_count; _i++) {
                _dup = _seen[_i] == _cp;
            }
            if(_dup) {
                continue;
            }
            _seen[_seen_count++] = _cp;

            const kana_bake_char* _self  = kana_bake_find(_bake, _cp);
            i32                   _score = _base;
            for(const c8* _q = _word.written; *_q != 0;) {
                u32       _l2 = 0;
                const u32 _o  = kana_bake_decode_utf8(_q, &_l2);
                _q += _l2;
                if(_o == _cp || !kana_bake_is_kanji(_o)) {
                    continue;
                }
                const i32 _harder = kana_bake_difficulty(kana_bake_find(_bake, _o)) - kana_bake_difficulty(_self);
                _score += _harder > 0 ? 2 * _harder : 0;
            }
            const kana_bake_word_ref _ref = { .codepoint = _cp, .word = _index, .score = _score };
            rde_arr_add(&_bake->word_refs, &_ref);
        }
    }
}

RDE_INTERNAL int kana_bake_compare_refs(const void* _a, const void* _b) {
    const kana_bake_word_ref* _x = (const kana_bake_word_ref*)_a;
    const kana_bake_word_ref* _y = (const kana_bake_word_ref*)_b;
    if(_x->codepoint != _y->codepoint) { return _x->codepoint < _y->codepoint ? -1 : 1; }
    if(_x->score != _y->score)         { return _x->score < _y->score ? -1 : 1; }
    return _x->word < _y->word ? -1 : (_x->word > _y->word ? 1 : 0);   // JMdict's own order breaks ties: stable across bakes
}

// Every candidate in, each kanji keeps its best (see above) and the words kept
// are numbered.
RDE_INTERNAL void kana_bake_choose_words(kana_bake* _bake) {
    const u32           _refs  = (u32)rde_arr_length(&_bake->word_refs);
    kana_bake_word_ref* _ref   = (kana_bake_word_ref*)_bake->word_refs.memory;
    kana_bake_word*     _words = (kana_bake_word*)_bake->words.memory;
    qsort(_ref, _refs, sizeof(kana_bake_word_ref), kana_bake_compare_refs);

    for(u32 _i = 0; _i < _refs;) {
        u32 _end = _i;
        while(_end < _refs && _ref[_end].codepoint == _ref[_i].codepoint) {
            _end++;
        }

        // The examples first: common, short, not the same number word again nor a
        // longer form of one kept. Then more, up to KANA_BAKE_WORDS_ALL, in their
        // order: anything not kept yet (the viewer's Add offers those).
        u32 _kept[KANA_BAKE_WORDS_ALL];
        u32 _count   = 0;
        b8  _counted = false;   // a word with a number kept already
        for(u32 _j = _i; _j < _end && _count < KANA_BAKE_WORDS_PER; _j++) {
            const kana_bake_word* _w       = &_words[_ref[_j].word];
            const b8              _numeral = kana_bake_counts(_w->written, _ref[_j].codepoint);
            b8                    _ok      = _w->common && _w->chars <= KANA_BAKE_EXAMPLE_CHARS && !(_numeral && _counted);
            for(u32 _k = 0; _ok && _k < _count; _k++) {
                const kana_bake_word* _other = &_words[_kept[_k]];
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
        for(u32 _j = _i; _j < _end && _count < KANA_BAKE_WORDS_ALL; _j++) {
            const kana_bake_word* _w  = &_words[_ref[_j].word];
            b8                    _ok = true;
            for(u32 _k = 0; _ok && _k < _count; _k++) {
                _ok = _kept[_k] != _ref[_j].word && strcmp(_w->written, _words[_kept[_k]].written) != 0;
            }
            if(_ok) {
                _kept[_count++] = _ref[_j].word;
            }
        }

        kana_bake_char* _char = kana_bake_find(_bake, _ref[_i].codepoint);
        if(_char != NULL && _count > 0) {
            _char->words = kana_bytes_size(&_bake->word_lists);
            kana_put_u8(&_bake->word_lists, (u8)_count);
            kana_put_u8(&_bake->word_lists, (u8)_examples);
            _bake->example_refs += _examples;
            for(u32 _k = 0; _k < _count; _k++) {
                kana_bake_word* _w = &_words[_kept[_k]];
                if(_w->index == UINT32_MAX) {
                    _w->index = _bake->word_count++;
                    kana_put_data(&_bake->word_text, _w->written, (u32)strlen(_w->written) + 1u);
                    kana_put_data(&_bake->word_text, _w->reading, (u32)strlen(_w->reading) + 1u);
                    kana_put_data(&_bake->word_text, _w->meaning, (u32)strlen(_w->meaning) + 1u);
                }
                kana_put_u32(&_bake->word_lists, _w->index);
            }
            _bake->with_words++;
        }
        _i = _end;
    }
}

RDE_INTERNAL b8 kana_bake_jmdict(kana_bake* _bake, const c8* _path) {
    if(!rde_file_exists(_path)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "bake: %s not found; no example words", _path);
        return true;
    }
    const f64 _t0   = rde_engine_get_time_now();
    u32       _size = 0;
    u8*       _data = kana_file_read(_path, &_size);
    if(_data == NULL) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: could not read %s", _path);
        return false;
    }

    static kana_bake_entry _e;   // big; one entry at a time
    memset(&_e, 0, sizeof(_e));

    c8        _line[1024];
    c8        _text[512];
    const c8* _s   = (const c8*)_data;
    const c8* _end = _s + _size;
    while(_s < _end) {
        const c8*   _nl   = memchr(_s, '\n', (usize)(_end - _s));
        const usize _len  = (usize)((_nl != NULL ? _nl : _end) - _s);
        const usize _copy = _len < sizeof(_line) - 1u ? _len : sizeof(_line) - 1u;
        memcpy(_line, _s, _copy);
        _line[_copy] = 0;
        _s = _nl != NULL ? _nl + 1 : _end;

        if(strcmp(_line, "<entry>") == 0) {
            memset(&_e, 0, sizeof(_e));
        } else if(strcmp(_line, "</entry>") == 0) {
            kana_bake_entry_done(_bake, &_e);
        } else if(strcmp(_line, "<k_ele>") == 0) {
            _e.element = _e.kebs < KANA_BAKE_ENTRY_KEBS ? 1u : 0u;
        } else if(strcmp(_line, "<r_ele>") == 0) {
            _e.element = _e.rebs < KANA_BAKE_ENTRY_REBS ? 2u : 0u;
        } else if(strcmp(_line, "<sense>") == 0) {
            _e.element    = _e.senses < KANA_BAKE_ENTRY_SENSES ? 3u : 0u;
            _e.sense_full = false;
        } else if(strcmp(_line, "</k_ele>") == 0) {
            _e.kebs += _e.element == 1u ? 1u : 0u;
            _e.element = 0;
        } else if(strcmp(_line, "</r_ele>") == 0) {
            _e.rebs += _e.element == 2u ? 1u : 0u;
            _e.element = 0;
        } else if(strcmp(_line, "</sense>") == 0) {
            _e.senses += _e.element == 3u ? 1u : 0u;
            _e.element = 0;
        } else if(_e.element == 1u) {
            const u32 _k = _e.kebs;
            if(kana_bake_element(_line, "keb", _text, sizeof(_text))) {
                _e.keb_long[_k] = strlen(_text) >= sizeof(_e.keb[_k]);
                snprintf(_e.keb[_k], sizeof(_e.keb[_k]), "%s", _e.keb_long[_k] ? "" : _text);
            } else if(kana_bake_element(_line, "ke_inf", _text, sizeof(_text))) {
                _e.keb_bad[_k] = _e.keb_bad[_k] || strcmp(_text, "&iK;") == 0 || strcmp(_text, "&oK;") == 0 ||
                                 strcmp(_text, "&rK;") == 0 || strcmp(_text, "&sK;") == 0 || strcmp(_text, "&io;") == 0;
            } else if(kana_bake_element(_line, "ke_pri", _text, sizeof(_text))) {
                if(strcmp(_text, "news1") == 0 || strcmp(_text, "ichi1") == 0 || strcmp(_text, "spec1") == 0 || strcmp(_text, "spec2") == 0) {
                    _e.keb_common[_k]++;
                    _e.keb_ichi[_k] = _e.keb_ichi[_k] || strcmp(_text, "ichi1") == 0;
                } else if(strncmp(_text, "nf", 2) == 0) {
                    _e.keb_nf[_k] = (u32)atoi(_text + 2);
                }
            }
        } else if(_e.element == 2u) {
            const u32 _r = _e.rebs;
            if(kana_bake_element(_line, "reb", _text, sizeof(_text))) {
                snprintf(_e.reb[_r], sizeof(_e.reb[_r]), "%s", _text);
            } else if(strncmp(_line, "<re_nokanji", 11) == 0) {
                _e.reb_bad[_r] = true;
            } else if(kana_bake_element(_line, "re_inf", _text, sizeof(_text))) {
                _e.reb_bad[_r] = _e.reb_bad[_r] || strcmp(_text, "&ik;") == 0 || strcmp(_text, "&ok;") == 0 || strcmp(_text, "&sk;") == 0;
            } else if(kana_bake_element(_line, "re_pri", _text, sizeof(_text))) {
                _e.reb_common[_r] = _e.reb_common[_r] || strcmp(_text, "news1") == 0 || strcmp(_text, "ichi1") == 0 ||
                                    strcmp(_text, "spec1") == 0 || strcmp(_text, "spec2") == 0;
            } else if(kana_bake_element(_line, "re_restr", _text, sizeof(_text))) {
                if(_e.reb_restrs[_r] < KANA_BAKE_ENTRY_RESTR && strlen(_text) < sizeof(_e.reb_restr[_r][0])) {
                    snprintf(_e.reb_restr[_r][_e.reb_restrs[_r]++], sizeof(_e.reb_restr[_r][0]), "%s", _text);
                }
            }
        } else if(_e.element == 3u) {
            const u32 _n = _e.senses;
            if(kana_bake_element(_line, "gloss", _text, sizeof(_text))) {
                // The first gloss always; more while the meaning stays short.
                const usize _have = strlen(_e.sense[_n]);
                if(_have == 0) {
                    snprintf(_e.sense[_n], sizeof(_e.sense[_n]), "%s", _text);
                } else if(!_e.sense_full && _have + 2u + strlen(_text) <= KANA_BAKE_WORD_MEANING) {
                    kana_bake_join(_e.sense[_n], sizeof(_e.sense[_n]), "; ", _text);
                } else {
                    _e.sense_full = true;
                }
            } else if(kana_bake_element(_line, "misc", _text, sizeof(_text))) {
                _e.sense_kana[_n] = _e.sense_kana[_n] || strcmp(_text, "&uk;") == 0;
            } else if(kana_bake_element(_line, "stagk", _text, sizeof(_text))) {
                if(_e.sense_stagks[_n] < KANA_BAKE_ENTRY_RESTR && strlen(_text) < sizeof(_e.sense_stagk[_n][0])) {
                    snprintf(_e.sense_stagk[_n][_e.sense_stagks[_n]++], sizeof(_e.sense_stagk[_n][0]), "%s", _text);
                }
            }
        }
    }

    kana_file_free(_data);
    rde_log_level(RDE_LOG_LEVEL_INFO, "bake: read %s in %.2f s", _path, rde_engine_get_time_now() - _t0);
    kana_bake_choose_words(_bake);
    return true;
}

// --- the bake --------------------------------------------------------------------------

RDE_INTERNAL const c8* kana_bake_arg(i32 _argc, c8** _argv, const c8* _prefix, const c8* _default) {
    const usize _len = strlen(_prefix);
    for(i32 _i = 1; _i < _argc; _i++) {
        if(_argv[_i] != NULL && strncmp(_argv[_i], _prefix, _len) == 0) {
            return _argv[_i] + _len;
        }
    }
    return _default;
}

b8 kana_bake_requested(i32 _argc, c8** _argv) {
    for(i32 _i = 1; _i < _argc; _i++) {
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--bake") == 0) {
            return true;
        }
    }
    return false;
}

i32 kana_bake_run(i32 _argc, c8** _argv) {
    const c8* _kanjivg  = kana_bake_arg(_argc, _argv, "--kanjivg=",  "data/raw/kanjivg.xml");
    const c8* _kanjidic = kana_bake_arg(_argc, _argv, "--kanjidic=", "data/raw/kanjidic2.xml");
    const c8* _jlpt     = kana_bake_arg(_argc, _argv, "--jlpt=",     "data/raw/jlpt");
    const c8* _jmdict   = kana_bake_arg(_argc, _argv, "--jmdict=",   "data/raw/JMdict_e.xml");
    const c8* _out      = kana_bake_arg(_argc, _argv, "--out=",      KANA_KANJI_FILE);

    for(u32 _i = 0; _i < 2; _i++) {
        const c8* _src = _i == 0 ? _kanjivg : _kanjidic;
        if(!rde_file_exists(_src)) {
            rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: %s not found (see COMMANDS.txt for where to get it; run from the project root)", _src);
            return 1;
        }
    }

    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    kana_bake _bake = {
        .chars     = rde_arr_new(sizeof(kana_bake_char), _heap),
        .geometry  = kana_bytes_new(4u * 1024u * 1024u),
        .text      = kana_bytes_new(512u * 1024u),
        .parts     = kana_bytes_new(256u * 1024u),
        .words      = rde_arr_new(sizeof(kana_bake_word), _heap),
        .word_refs  = rde_arr_new(sizeof(kana_bake_word_ref), _heap),
        .word_lists = kana_bytes_new(128u * 1024u),
        .word_text  = kana_bytes_new(512u * 1024u),
        .min_coord = 1e9f,
        .max_coord = -1e9f,
    };

    const f64 _t0 = rde_engine_get_time_now();
    i32       _rc = 0;

    if(!kana_bake_kanjivg(&_bake, _kanjivg)) {
        _rc = 1;
    } else {
        // KanjiVG is in code point order already; sort anyway — the lookups below
        // and the app's binary search both depend on it.
        qsort(_bake.chars.memory, rde_arr_length(&_bake.chars), sizeof(kana_bake_char), kana_bake_compare);

        if(!kana_bake_kanjidic(&_bake, _kanjidic)) {
            _rc = 1;
        }

        for(u32 _level = 5; _level >= 1; _level--) {
            kana_bake_jlpt_level(&_bake, _jlpt, _level);
        }

        // After KANJIDIC2: the words are ranked by their kanji's grades.
        if(!kana_bake_jmdict(&_bake, _jmdict)) {
            _rc = 1;
        }
    }

    if(_rc == 0) {
        const u32   _count = (u32)rde_arr_length(&_bake.chars);
        kana_bytes  _file  = kana_bytes_new(KANA_FILE_HEADER_SIZE + 16u + _count * KANA_KANJI_RECORD_SIZE + 8u + kana_bytes_size(&_bake.geometry) + 8u + kana_bytes_size(&_bake.text) +
                                            12u + _count * 4u + kana_bytes_size(&_bake.parts) +
                                            20u + _count * 4u + kana_bytes_size(&_bake.word_lists) + kana_bytes_size(&_bake.word_text) + 1u);
        kana_put_header(&_file, KANA_KANJI_VERSION, KANA_KANJI_KIND);

        u32 _chunk = kana_chunk_begin(&_file, KANA_KANJI_CHUNK_CHARS);
        kana_put_u32(&_file, _count);
        kana_put_u32(&_file, KANA_KANJI_RECORD_SIZE);
        const kana_bake_char* _chars = (const kana_bake_char*)_bake.chars.memory;
        for(u32 _i = 0; _i < _count; _i++) {
            kana_put_u32(&_file, _chars[_i].codepoint);
            kana_put_u32(&_file, _chars[_i].geometry);
            kana_put_u32(&_file, _chars[_i].text);
            kana_put_u16(&_file, _chars[_i].frequency);
            kana_put_u8(&_file, _chars[_i].strokes);
            kana_put_u8(&_file, _chars[_i].grade);
            kana_put_u8(&_file, _chars[_i].jlpt);
            kana_put_u8(&_file, _chars[_i].radical);
            kana_put_u8(&_file, _chars[_i].jlpt_n);
            kana_put_u8(&_file, 0u);
        }
        kana_chunk_end(&_file, _chunk);

        _chunk = kana_chunk_begin(&_file, KANA_KANJI_CHUNK_GEOM);
        kana_put_data(&_file, _bake.geometry.memory, kana_bytes_size(&_bake.geometry));
        kana_chunk_end(&_file, _chunk);

        _chunk = kana_chunk_begin(&_file, KANA_KANJI_CHUNK_TEXT);
        kana_put_data(&_file, _bake.text.memory, kana_bytes_size(&_bake.text));
        kana_chunk_end(&_file, _chunk);

        _chunk = kana_chunk_begin(&_file, KANA_KANJI_CHUNK_PARTS);
        kana_put_u32(&_file, _count);
        for(u32 _i = 0; _i < _count; _i++) {
            kana_put_u32(&_file, _chars[_i].parts);
        }
        kana_put_data(&_file, _bake.parts.memory, kana_bytes_size(&_bake.parts));
        kana_chunk_end(&_file, _chunk);

        if(_bake.word_count > 0) {
            _chunk = kana_chunk_begin(&_file, KANA_KANJI_CHUNK_WORDS);
            kana_put_u32(&_file, _count);
            for(u32 _i = 0; _i < _count; _i++) {
                kana_put_u32(&_file, _chars[_i].words);
            }
            kana_put_u32(&_file, _bake.word_count);
            kana_put_u32(&_file, kana_bytes_size(&_bake.word_lists));
            kana_put_data(&_file, _bake.word_lists.memory, kana_bytes_size(&_bake.word_lists));
            kana_put_data(&_file, _bake.word_text.memory, kana_bytes_size(&_bake.word_text));
            kana_chunk_end(&_file, _chunk);
        }

        const u32 _geometry_bytes = kana_bytes_size(&_bake.geometry);
        const u32 _text_bytes     = kana_bytes_size(&_bake.text);
        rde_file_create_missing_dirs(_out);
        u32 _bytes = 0;
        if(!kana_bytes_write_and_free(&_file, _out, &_bytes)) {
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
                          "  %u characters (%u skipped), %u with KANJIDIC2 info, %u stroke-count disagreements\n"
                          "  JLPT N5 %u, N4 %u, N3 %u, N2 %u, N1 %u (%u listed but without strokes)\n"
                          "  %u strokes, %u curve segments (max %u in one stroke)\n"
                          "  %u characters with parts, %u parts in all\n"
                          "  %u words for %u kanji, %u of them examples (from %u JMdict entries): %.2f MB\n"
                          "  geometry %.2f MB, text %.2f MB; coordinates %.2f .. %.2f",
                          _out, (f64)_bytes / (1024.0 * 1024.0), rde_engine_get_time_now() - _t0,
                          _count, _bake.skipped, _bake.with_info, _bake.count_mismatch,
                          _bake.jlpt_listed[5], _bake.jlpt_listed[4], _bake.jlpt_listed[3], _bake.jlpt_listed[2], _bake.jlpt_listed[1], _bake.jlpt_missing,
                          _bake.strokes, _bake.segments, _bake.max_segments,
                          _bake.with_parts, _bake.part_refs,
                          _bake.word_count, _bake.with_words, _bake.example_refs, _bake.word_entries,
                          (f64)(kana_bytes_size(&_bake.word_lists) + kana_bytes_size(&_bake.word_text)) / (1024.0 * 1024.0),
                          (f64)_geometry_bytes / (1024.0 * 1024.0), (f64)_text_bytes / (1024.0 * 1024.0),
                          (f64)_bake.min_coord, (f64)_bake.max_coord);
        }
    }

    rde_arr_free(&_bake.chars);
    rde_arr_free(&_bake.geometry);
    rde_arr_free(&_bake.text);
    rde_arr_free(&_bake.parts);
    rde_arr_free(&_bake.words);
    rde_arr_free(&_bake.word_refs);
    rde_arr_free(&_bake.word_lists);
    rde_arr_free(&_bake.word_text);
    return _rc;
}

#else

b8 kana_bake_requested(i32 _argc, c8** _argv) {
    RDE_UNUSED(_argc);
    RDE_UNUSED(_argv);
    return false;
}

i32 kana_bake_run(i32 _argc, c8** _argv) {
    RDE_UNUSED(_argc);
    RDE_UNUSED(_argv);
    return 1;
}

#endif
