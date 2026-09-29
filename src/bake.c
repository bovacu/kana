#include "bake.h"

#include <string.h>

#if !defined(RDE_PLATFORM_MOBILE)

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "kfile.h"
#include "kanji.h"

// ===========================================================================
// See bake.h. Parse both sources into memory (RDE's XML parser, on the standard
// heap — the trees are tens of MB), keep what the app needs, write one file.
// ===========================================================================

#define KANA_BAKE_MAX_STROKES   64u     // more than any character has (the most is ~30s)
#define KANA_BAKE_MAX_SEGMENTS  255u    // a stroke's segment count is a u8

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
} kana_bake_char;

typedef struct {
    rde_arr TYPE(kana_bake_char) chars;
    kana_bytes                   geometry;
    kana_bytes                   text;
    kana_bytes                   parts;

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

        kana_bake_char _char = { .codepoint = _cp, .geometry = kana_bytes_size(&_bake->geometry), .text = UINT32_MAX, .strokes = (u8)_count, .parts = UINT32_MAX };

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
    }

    if(_rc == 0) {
        const u32   _count = (u32)rde_arr_length(&_bake.chars);
        kana_bytes  _file  = kana_bytes_new(KANA_FILE_HEADER_SIZE + 16u + _count * KANA_KANJI_RECORD_SIZE + 8u + kana_bytes_size(&_bake.geometry) + 8u + kana_bytes_size(&_bake.text) +
                                            12u + _count * 4u + kana_bytes_size(&_bake.parts) + 1u);
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
                          "  geometry %.2f MB, text %.2f MB; coordinates %.2f .. %.2f",
                          _out, (f64)_bytes / (1024.0 * 1024.0), rde_engine_get_time_now() - _t0,
                          _count, _bake.skipped, _bake.with_info, _bake.count_mismatch,
                          _bake.jlpt_listed[5], _bake.jlpt_listed[4], _bake.jlpt_listed[3], _bake.jlpt_listed[2], _bake.jlpt_listed[1], _bake.jlpt_missing,
                          _bake.strokes, _bake.segments, _bake.max_segments,
                          _bake.with_parts, _bake.part_refs,
                          (f64)_geometry_bytes / (1024.0 * 1024.0), (f64)_text_bytes / (1024.0 * 1024.0),
                          (f64)_bake.min_coord, (f64)_bake.max_coord);
        }
    }

    rde_arr_free(&_bake.chars);
    rde_arr_free(&_bake.geometry);
    rde_arr_free(&_bake.text);
    rde_arr_free(&_bake.parts);
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
