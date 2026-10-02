#include "bake.h"

#include <string.h>

#if !defined(RDE_PLATFORM_MOBILE)

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "kfile.h"
#include "kanji.h"
#include "chart.h"
#include "catalog.h"
#include "match.h"

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

// Meanings in other languages: KANJIDIC2's m_lang and full JMdict's xml:lang
// (JMdict_e has English only; JMdict has them all). No JMdict gloss is in
// Portuguese: its words show in English (kanji.h's fallback).
#define KANA_BAKE_LANGS 3u
static const struct { const c8* code; const c8* kanjidic; const c8* jmdict; } KANA_BAKE_LANG_LIST[KANA_BAKE_LANGS] = {
    { "es", "es", "spa" },
    { "pt", "pt", NULL  },
    { "fr", "fr", "fre" },
};

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
    u32 meaning_in[KANA_BAKE_LANGS];   // its meanings in another language: offset into that one's text, or UINT32_MAX
} kana_bake_char;

// A common JMdict word that can be an example: its three strings.
typedef struct {
    c8  written[4u * KANA_BAKE_WORD_CHARS + 1u];
    c8  reading[64];
    c8  meaning[128];
    c8  meaning_in[KANA_BAKE_LANGS][128];   // in the other languages ("": none)
    u32 index;         // in the file, UINT32_MAX until a kanji keeps it (or, common, it is kept for reading text)
    u8  chars;
    b8  common;        // on a common list: can be an example
    u16 freq;          // how common: lower the commoner ('WFRQ')
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
    kana_bytes                   word_freqs;   // a u16 per word kept, in number order ('WFRQ')
    // Tatoeba's example sentences ('SENT'): each kept once (Japanese, then en, es,
    // fr, pt, NUL-terminated, "" when there is none), and each word's (or UINT32_MAX).
    kana_bytes                   sentences;
    u32                          sentence_count;
    u32*                         sentence_words;
    u32                          words_unlisted;   // common words kept on no kanji's list (for reading text)
    // The other languages (KANA_BAKE_LANG_LIST): each one's texts, and its words'
    // (word number, text offset) pairs, in number order.
    kana_bytes                   lang_text[KANA_BAKE_LANGS];
    kana_bytes                   lang_words[KANA_BAKE_LANGS];
    u32                          lang_word_count[KANA_BAKE_LANGS];
    u32                          lang_kanji_count[KANA_BAKE_LANGS];

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

        kana_bake_char _char = { .codepoint = _cp, .geometry = kana_bytes_size(&_bake->geometry), .text = UINT32_MAX, .strokes = (u8)_count, .parts = UINT32_MAX, .words = UINT32_MAX,
                                 .meaning_in = { UINT32_MAX, UINT32_MAX, UINT32_MAX } };

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
        c8  _meanings_in[KANA_BAKE_LANGS][1024];
        memset(_meanings_in, 0, sizeof(_meanings_in));

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
                        } else if(kana_xml_is(_x, "meaning")) {
                            const c8* _lang = kana_xml_attr(_x, "m_lang");
                            for(u32 _l = 0; _l < KANA_BAKE_LANGS; _l++) {
                                if(strcmp(_lang, KANA_BAKE_LANG_LIST[_l].kanjidic) == 0) {
                                    kana_bake_join(_meanings_in[_l], sizeof(_meanings_in[_l]), ", ", kana_xml_text(_x));
                                }
                            }
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
        for(u32 _l = 0; _l < KANA_BAKE_LANGS; _l++) {
            if(_meanings_in[_l][0] != 0) {
                _char->meaning_in[_l] = kana_bytes_size(&_bake->lang_text[_l]);
                kana_put_data(&_bake->lang_text[_l], _meanings_in[_l], (u32)strlen(_meanings_in[_l]) + 1u);
                _bake->lang_kanji_count[_l]++;
            }
        }

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
    // Full JMdict: a sense in another language (its glosses xml:lang) is not an
    // English one; the entry's first sense in each language is its meaning there.
    b8  sense_foreign;
    i32 sense_lang;    // KANA_BAKE_LANG_LIST index, -1 another language
    c8  foreign[KANA_BAKE_LANGS][128];
    b8  foreign_full[KANA_BAKE_LANGS];   // that language's first sense is in
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
        // How common, plainly (no example's preferences): the newspaper band,
        // else about where the other lists sit; more lists, more common.
        {
            i32 _freq = _word.common ? (_e->keb_nf[_k] != 0 ? (i32)_e->keb_nf[_k] : _e->keb_ichi[_k] ? 18 : 26) - 3 * ((i32)_e->keb_common[_k] - 1)
                                     : 1000 + (_e->keb_nf[_k] != 0 ? (i32)_e->keb_nf[_k] : 60);
            _freq     += _e->reb_common[_reading] ? 0 : 2;
            _freq     += _e->keb_common[0] > _e->keb_common[_k] ? 1 : 0;   // a word usually written otherwise (本 for 元 もと; not 七月 for ７月)
            _word.freq = (u16)(_freq < 0 ? 0 : _freq > 0xFFFE ? 0xFFFE : _freq);
        }
        snprintf(_word.written, sizeof(_word.written), "%s", _e->keb[_k]);
        snprintf(_word.reading, sizeof(_word.reading), "%s", _e->reb[_reading]);
        snprintf(_word.meaning, sizeof(_word.meaning), "%s", _e->sense[_sense]);
        for(u32 _l = 0; _l < KANA_BAKE_LANGS; _l++) {
            snprintf(_word.meaning_in[_l], sizeof(_word.meaning_in[_l]), "%s", _e->foreign[_l]);
        }
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
        _base += _e->keb_common[0] > _e->keb_common[_k] ? 1 : 0;  // 本 ほん before 本 もと, whose usual form is 元

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

// A word kept: numbered, its text (and its meanings in the other languages,
// and how common it is) written.
RDE_INTERNAL void kana_bake_number_word(kana_bake* _bake, kana_bake_word* _w) {
    _w->index = _bake->word_count++;
    kana_put_data(&_bake->word_text, _w->written, (u32)strlen(_w->written) + 1u);
    kana_put_data(&_bake->word_text, _w->reading, (u32)strlen(_w->reading) + 1u);
    kana_put_data(&_bake->word_text, _w->meaning, (u32)strlen(_w->meaning) + 1u);
    kana_put_u16(&_bake->word_freqs, _w->freq);
    for(u32 _l = 0; _l < KANA_BAKE_LANGS; _l++) {
        if(_w->meaning_in[_l][0] != 0) {
            kana_put_u32(&_bake->lang_words[_l], _w->index);
            kana_put_u32(&_bake->lang_words[_l], kana_bytes_size(&_bake->lang_text[_l]));
            kana_put_data(&_bake->lang_text[_l], _w->meaning_in[_l], (u32)strlen(_w->meaning_in[_l]) + 1u);
            _bake->lang_word_count[_l]++;
        }
    }
}

// Every candidate in, each kanji keeps its best (see above) and the words kept
// are numbered — then every other common word, on no kanji's list: what text
// is read with (wordsplit.h: 食べる, 行く, 本 ほん...).
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
                    kana_bake_number_word(_bake, _w);
                }
                kana_put_u32(&_bake->word_lists, _w->index);
            }
            _bake->with_words++;
        }
        _i = _end;
    }

    // Every other common word, on no kanji's list (in JMdict's order).
    const u32 _all = (u32)rde_arr_length(&_bake->words);
    for(u32 _w = 0; _w < _all; _w++) {
        if(_words[_w].index == UINT32_MAX && _words[_w].common) {
            kana_bake_number_word(_bake, &_words[_w]);
            _bake->words_unlisted++;
        }
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
            _e.element       = 3u;   // an English sense past the kept ones is read for nothing: another language may follow
            _e.sense_full    = false;
            _e.sense_foreign = false;
            _e.sense_lang    = -1;
        } else if(strcmp(_line, "</k_ele>") == 0) {
            _e.kebs += _e.element == 1u ? 1u : 0u;
            _e.element = 0;
        } else if(strcmp(_line, "</r_ele>") == 0) {
            _e.rebs += _e.element == 2u ? 1u : 0u;
            _e.element = 0;
        } else if(strcmp(_line, "</sense>") == 0) {
            if(_e.element == 3u && _e.sense_foreign) {
                if(_e.sense_lang >= 0 && _e.foreign[_e.sense_lang][0] != 0) {
                    _e.foreign_full[_e.sense_lang] = true;
                }
                if(_e.senses < KANA_BAKE_ENTRY_SENSES) {   // not an English one: its slot free again
                    _e.sense[_e.senses][0]     = 0;
                    _e.sense_kana[_e.senses]   = false;
                    _e.sense_stagks[_e.senses] = 0;
                }
            } else if(_e.element == 3u && _e.senses < KANA_BAKE_ENTRY_SENSES) {
                _e.senses++;
            }
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
        } else if(_e.element == 3u && strncmp(_line, "<gloss xml:lang=\"", 17) == 0 && strncmp(_line + 17, "eng", 3) != 0) {
            // Another language's gloss: this sense is that language's.
            _e.sense_foreign = true;
            _e.sense_lang    = -1;
            for(u32 _l = 0; _l < KANA_BAKE_LANGS; _l++) {
                if(KANA_BAKE_LANG_LIST[_l].jmdict != NULL && strncmp(_line + 17, KANA_BAKE_LANG_LIST[_l].jmdict, 3) == 0) {
                    _e.sense_lang = (i32)_l;
                }
            }
            if(_e.sense_lang >= 0 && !_e.foreign_full[_e.sense_lang] && kana_bake_element(_line, "gloss", _text, sizeof(_text))) {
                c8*         _to   = _e.foreign[_e.sense_lang];
                const usize _have = strlen(_to);
                if(_have == 0) {
                    snprintf(_to, sizeof(_e.foreign[0]), "%s", _text);
                } else if(_have + 2u + strlen(_text) <= KANA_BAKE_WORD_MEANING) {
                    kana_bake_join(_to, sizeof(_e.foreign[0]), "; ", _text);
                }
            }
        } else if(_e.element == 3u && _e.senses < KANA_BAKE_ENTRY_SENSES) {
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

// --- Tatoeba: example sentences -----------------------------------------------------------
// Japanese sentences from Tatoeba (CC BY 2.0 FR) with their translations: English
// (required), Spanish, French and Portuguese where there are. Each word kept gets
// the best sentence with it in. Tatoeba's word index (jpn_indices.csv: each
// sentence's words as dictionary forms, with the reading where it is not the
// usual one) says which word a sentence has — 行 read ぎょう is not the 行 of
// 行って: the word with that form and reading, or the commonest with that form,
// its kanji all in the sentence as written. A sentence not indexed is searched as
// text — a word as written, or a verb's or adjective's stem followed by its
// conjugation (食べ + ました), never one kanji alone (whose reading the text does
// not say) — and loses to any indexed one. Best: a comfortable length (about 14
// characters, between KANA_BAKE_SENT_MIN and _MAX), then translated into more of
// the languages; one the index marks as a good example (~) first. kanji.h 'SENT'.

#define KANA_BAKE_SENT_MIN   6u
#define KANA_BAKE_SENT_MAX   40u
#define KANA_BAKE_SENT_IDEAL 14
#define KANA_BAKE_SENT_LANGS 4u     // en, es, fr, pt: their order in the file
#define KANA_BAKE_SENT_GOOD  10     // a score's bonus: the index says it is a good example
#define KANA_BAKE_SENT_TEXT  40     // ...its malus: found as text, not in the index
static const c8* const KANA_BAKE_SENT_CODES[KANA_BAKE_SENT_LANGS] = { "eng", "spa", "fra", "por" };

typedef struct { u32 id; u32 at; u32 len; } kana_bake_sentence;    // a sentence: its id, where its text is in its file
typedef struct { u32 from; u32 to; } kana_bake_link;               // a Japanese sentence's id, a translation's id

RDE_INTERNAL int kana_bake_by_sentence_id(const void* _a, const void* _b) {
    const u32 _x = ((const kana_bake_sentence*)_a)->id, _y = ((const kana_bake_sentence*)_b)->id;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

RDE_INTERNAL int kana_bake_by_link(const void* _a, const void* _b) {
    const kana_bake_link* _x = (const kana_bake_link*)_a;
    const kana_bake_link* _y = (const kana_bake_link*)_b;
    if(_x->from != _y->from) {
        return _x->from < _y->from ? -1 : 1;
    }
    return _x->to < _y->to ? -1 : (_x->to > _y->to ? 1 : 0);
}

RDE_INTERNAL int kana_bake_by_u32(const void* _a, const void* _b) {
    const u32 _x = *(const u32*)_a, _y = *(const u32*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// The kept words by written form (word numbers, alike ones together): for the index.
static const kana_bake_word* kana_bake_sort_words;
RDE_INTERNAL int kana_bake_by_written(const void* _a, const void* _b) {
    return strcmp(kana_bake_sort_words[*(const u32*)_a].written, kana_bake_sort_words[*(const u32*)_b].written);
}

// Tatoeba's index line for a sentence ("word(reading)[sense]{as written}~ ..."):
// each word it names that is kept, scored _score (better when marked ~), as that
// word's best sentence _i when better than its best so far.
RDE_INTERNAL void kana_bake_index_words(const c8* _line, u32 _size, const kana_bake_word* _words, const u32* _sorted, u32 _sorted_count, u32 _i, i32 _score,
                                        u32* _best, i32* _best_score) {
    u32 _p = 0;
    while(_p < _size) {
        while(_p < _size && _line[_p] == ' ') {
            _p++;
        }
        u32 _end = _p;
        while(_end < _size && _line[_end] != ' ') {
            _end++;
        }
        // The dictionary form, then its marks.
        u32 _h = _p;
        while(_h < _end && _line[_h] != '(' && _line[_h] != '[' && _line[_h] != '{' && _line[_h] != '~') {
            _h++;
        }
        c8  _written[4u * KANA_BAKE_WORD_CHARS + 1u];
        c8  _reading[64]   = "";
        c8  _as[128]       = "";
        b8  _good          = false;
        b8  _fits          = _h - _p > 0 && _h - _p < sizeof(_written);
        if(_fits) {
            memcpy(_written, &_line[_p], _h - _p);
            _written[_h - _p] = 0;
        }
        for(u32 _m = _h; _m < _end;) {
            const c8 _open = _line[_m];
            if(_open == '~') {
                _good = true;
                _m++;
                continue;
            }
            const c8 _close = _open == '(' ? ')' : _open == '[' ? ']' : '}';
            u32      _q     = _m + 1u;
            while(_q < _end && _line[_q] != _close) {
                _q++;
            }
            const u32 _n = _q - (_m + 1u);
            if(_open == '(' && _n > 0 && _line[_m + 1u] != '#' && _n < sizeof(_reading)) {
                memcpy(_reading, &_line[_m + 1u], _n);
                _reading[_n] = 0;
            } else if(_open == '{' && _n < sizeof(_as)) {
                memcpy(_as, &_line[_m + 1u], _n);
                _as[_n] = 0;
            }
            _m = _q + 1u;
        }
        _p = _end;
        if(!_fits) {
            continue;
        }
        // Its kanji all in the sentence as written (直ぐに written すぐに has none).
        const c8* _shown = _as[0] != 0 ? _as : _written;
        b8        _all   = true;
        for(const c8* _c = _written; *_c != 0 && _all;) {
            u32       _len = 0;
            const u32 _cp  = kana_bake_decode_utf8(_c, &_len);
            if(kana_bake_is_kanji(_cp)) {
                c8 _one[8] = { 0 };
                memcpy(_one, _c, _len < 7u ? _len : 7u);
                _all = strstr(_shown, _one) != NULL;
            }
            _c += _len > 0 ? _len : 1u;
        }
        if(!_all) {
            continue;
        }
        // The word: with that reading when one is given, else the commonest of that
        // form (none when two are as common: which, the index does not say).
        u32 _lo = 0, _hi = _sorted_count;
        while(_lo < _hi) {
            const u32 _mid = (_lo + _hi) / 2u;
            if(strcmp(_words[_sorted[_mid]].written, _written) < 0) {
                _lo = _mid + 1u;
            } else {
                _hi = _mid;
            }
        }
        u32 _word = UINT32_MAX;
        b8  _tied = false;
        for(u32 _k = _lo; _k < _sorted_count && strcmp(_words[_sorted[_k]].written, _written) == 0; _k++) {
            const kana_bake_word* _w = &_words[_sorted[_k]];
            if(_reading[0] != 0) {
                _word = strcmp(_w->reading, _reading) == 0 ? _sorted[_k] : _word;
            } else if(_word == UINT32_MAX || _w->freq < _words[_word].freq) {
                _word = _sorted[_k];
                _tied = false;
            } else if(_w->freq == _words[_word].freq) {
                _tied = true;
            }
        }
        if(_tied) {
            continue;
        }
        const i32 _s = _score - (_good ? KANA_BAKE_SENT_GOOD : 0);
        if(_word != UINT32_MAX && _s < _best_score[_word]) {
            _best[_word]       = _i;
            _best_score[_word] = _s;
        }
    }
}

// A Tatoeba export's lines, "id <tab> lang <tab> text": each sentence (whose id
// is in _wanted, sorted, when given) into _out.
RDE_INTERNAL void kana_bake_read_sentences(const u8* _data, u32 _size, const u32* _wanted, u32 _wanted_count, rde_arr* _out) {
    u32 _at = 0;
    while(_at < _size) {
        u32 _end = _at;
        while(_end < _size && _data[_end] != '\n') {
            _end++;
        }
        // id
        u32 _id = 0;
        u32 _p  = _at;
        while(_p < _end && _data[_p] >= '0' && _data[_p] <= '9') {
            _id = _id * 10u + (u32)(_data[_p++] - '0');
        }
        // lang, then the text
        if(_p < _end && _data[_p] == '\t') {
            _p++;
            while(_p < _end && _data[_p] != '\t') {
                _p++;
            }
            if(_p < _end && _data[_p] == '\t' && (_wanted == NULL || bsearch(&_id, _wanted, _wanted_count, sizeof(u32), kana_bake_by_u32) != NULL)) {
                u32 _len = _end - (_p + 1u);
                if(_len > 0 && _data[_p + 1u + _len - 1u] == '\r') {
                    _len--;
                }
                const kana_bake_sentence _s = { _id, _p + 1u, _len };
                rde_arr_add(_out, (any)&_s);
            }
        }
        _at = _end + 1u;
    }
    qsort(_out->memory, rde_arr_length(_out), sizeof(kana_bake_sentence), kana_bake_by_sentence_id);
}

// "from <tab> to" lines, sorted.
RDE_INTERNAL void kana_bake_read_links(const u8* _data, u32 _size, rde_arr* _out) {
    u32 _at = 0;
    while(_at < _size) {
        kana_bake_link _l = { 0, 0 };
        while(_at < _size && _data[_at] >= '0' && _data[_at] <= '9') {
            _l.from = _l.from * 10u + (u32)(_data[_at++] - '0');
        }
        if(_at < _size && _data[_at] == '\t') {
            _at++;
            while(_at < _size && _data[_at] >= '0' && _data[_at] <= '9') {
                _l.to = _l.to * 10u + (u32)(_data[_at++] - '0');
            }
            rde_arr_add(_out, (any)&_l);
        }
        while(_at < _size && _data[_at] != '\n') {
            _at++;
        }
        _at++;
    }
    qsort(_out->memory, rde_arr_length(_out), sizeof(kana_bake_link), kana_bake_by_link);
}

// The first translation of Japanese sentence _from among _links: its id, or 0.
RDE_INTERNAL u32 kana_bake_link_of(const rde_arr* _links, u32 _from) {
    const kana_bake_link* _l  = (const kana_bake_link*)_links->memory;
    u32                   _lo = 0, _hi = (u32)rde_arr_length(_links);
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(_l[_mid].from < _from) { _lo = _mid + 1u; } else { _hi = _mid; }
    }
    return _lo < (u32)rde_arr_length(_links) && _l[_lo].from == _from ? _l[_lo].to : 0u;
}

RDE_INTERNAL const kana_bake_sentence* kana_bake_sentence_of(const rde_arr* _sentences, u32 _id) {
    const kana_bake_sentence _key = { _id, 0, 0 };
    return (const kana_bake_sentence*)bsearch(&_key, _sentences->memory, rde_arr_length(_sentences), sizeof(kana_bake_sentence), kana_bake_by_sentence_id);
}

// The words kept, by their written form (or a conjugating one's stem): an
// open-addressed table of word numbers.
typedef struct { u32 hash; u32 word; u16 length; u8 stem; } kana_bake_word_key;

RDE_INTERNAL u32 kana_bake_hash_bytes(const u8* _s, u32 _n) {
    u32 _h = 2166136261u;
    for(u32 _i = 0; _i < _n; _i++) {
        _h = (_h ^ _s[_i]) * 16777619u;
    }
    return _h != 0u ? _h : 1u;
}

// A whole (big) file, outside RDE's pool: Tatoeba's English alone is ~100 MB.
// Free with free(). NULL when it cannot be read.
RDE_INTERNAL u8* kana_bake_slurp(const c8* _path, u32* _size) {
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

RDE_INTERNAL b8 kana_bake_tatoeba(kana_bake* _bake, const c8* _dir) {
    c8 _path[RDE_MAX_PATH];
    snprintf(_path, sizeof(_path), "%s/jpn_sentences.tsv", _dir);
    if(!rde_file_exists(_path)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "bake: %s not found; no example sentences", _path);
        return true;
    }
    const f64 _t0 = rde_engine_get_time_now();
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();

    // The Japanese sentences, and each language's links from them.
    u32 _jsize = 0;
    u8* _jdata = kana_bake_slurp(_path, &_jsize);
    rde_arr _japanese = rde_arr_new(sizeof(kana_bake_sentence), _heap);
    kana_bake_read_sentences(_jdata, _jsize, NULL, 0, &_japanese);
    // The word index: "id <tab> meaning id <tab> words", read as the sentences are.
    snprintf(_path, sizeof(_path), "%s/jpn_indices.csv", _dir);
    u32     _isize = 0;
    u8*     _idata = rde_file_exists(_path) ? kana_bake_slurp(_path, &_isize) : NULL;
    rde_arr _index = rde_arr_new(sizeof(kana_bake_sentence), _heap);
    if(_idata != NULL) {
        kana_bake_read_sentences(_idata, _isize, NULL, 0, &_index);
    } else {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "bake: %s not found; sentences found as text only", _path);
    }
    rde_arr _links[KANA_BAKE_SENT_LANGS];
    rde_arr _texts[KANA_BAKE_SENT_LANGS];
    u8*     _tdata[KANA_BAKE_SENT_LANGS];
    for(u32 _l = 0; _l < KANA_BAKE_SENT_LANGS; _l++) {
        _links[_l] = rde_arr_new(sizeof(kana_bake_link), _heap);
        _texts[_l] = rde_arr_new(sizeof(kana_bake_sentence), _heap);
        _tdata[_l] = NULL;
        snprintf(_path, sizeof(_path), "%s/jpn-%s_links.tsv", _dir, KANA_BAKE_SENT_CODES[_l]);
        u32 _size = 0;
        u8* _data = rde_file_exists(_path) ? kana_bake_slurp(_path, &_size) : NULL;
        if(_data != NULL) {
            kana_bake_read_links(_data, _size, &_links[_l]);
            free(_data);
        }
        // Only the sentences linked to: their ids, then their texts.
        const u32 _n      = (u32)rde_arr_length(&_links[_l]);
        u32*      _wanted = (u32*)malloc(sizeof(u32) * (_n > 0 ? _n : 1u));
        for(u32 _i = 0; _i < _n; _i++) {
            _wanted[_i] = ((const kana_bake_link*)_links[_l].memory)[_i].to;
        }
        qsort(_wanted, _n, sizeof(u32), kana_bake_by_u32);
        snprintf(_path, sizeof(_path), "%s/%s_sentences.tsv", _dir, KANA_BAKE_SENT_CODES[_l]);
        u32 _size2 = 0;
        _tdata[_l] = rde_file_exists(_path) ? kana_bake_slurp(_path, &_size2) : NULL;
        if(_tdata[_l] != NULL) {
            kana_bake_read_sentences(_tdata[_l], _size2, _wanted, _n, &_texts[_l]);
        }
        free(_wanted);
    }

    // The words kept, by written form, and by stem where they conjugate.
    kana_bake_word* _words = (kana_bake_word*)_bake->words.memory;
    const u32       _all   = (u32)rde_arr_length(&_bake->words);
    u32             _tsize = 1u;
    while(_tsize < _bake->word_count * 4u) {
        _tsize <<= 1u;
    }
    kana_bake_word_key* _table = (kana_bake_word_key*)calloc(_tsize, sizeof(kana_bake_word_key));
    for(u32 _w = 0; _w < _all; _w++) {
        if(_words[_w].index == UINT32_MAX) {
            continue;
        }
        const u8* _s = (const u8*)_words[_w].written;
        const u32 _n = (u32)strlen(_words[_w].written);
        // The written form; and, ending in a verb's or an i-adjective's kana, its stem.
        u32 _last_at = 0, _last = 0, _cps = 0;
        for(u32 _i = 0; _i < _n;) {
            u32       _len = 0;
            const u32 _cp  = kana_bake_decode_utf8((const c8*)_s + _i, &_len);
            _last_at = _i;
            _last    = _cp;
            _i      += _len > 0 ? _len : 1u;
            _cps++;
        }
        static const u32 _endings[] = { 0x3046u, 0x304Fu, 0x3050u, 0x3059u, 0x3064u, 0x306Cu, 0x3076u, 0x3080u, 0x308Bu, 0x3044u };
        b8 _conjugates = false;
        for(u32 _e = 0; _e < sizeof(_endings) / sizeof(_endings[0]); _e++) {
            _conjugates |= _last == _endings[_e];
        }
        for(u32 _k = 0; _k < (_conjugates && _cps >= 2u ? 2u : 1u); _k++) {
            const u32 _len = _k == 0 ? _n : _last_at;
            const u32 _h   = kana_bake_hash_bytes(_s, _len);
            for(u32 _i = _h & (_tsize - 1u);; _i = (_i + 1u) & (_tsize - 1u)) {
                if(_table[_i].hash == 0u) {
                    _table[_i] = (kana_bake_word_key){ _h, _w, (u16)_len, (u8)_k };
                    break;
                }
                if(_table[_i].hash == _h && _table[_i].length == _len && _table[_i].stem == _k &&
                   memcmp(_words[_table[_i].word].written, _s, _len) == 0) {
                    break;   // spelt alike: the first keeps the place (both are numbered: the first is enough here)
                }
            }
        }
    }

    // ...and by written form, every one, for the index.
    u32* _sorted       = (u32*)malloc(sizeof(u32) * (_all > 0 ? _all : 1u));
    u32  _sorted_count = 0;
    for(u32 _w = 0; _w < _all; _w++) {
        if(_words[_w].index != UINT32_MAX) {
            _sorted[_sorted_count++] = _w;
        }
    }
    kana_bake_sort_words = _words;
    qsort(_sorted, _sorted_count, sizeof(u32), kana_bake_by_written);

    // Each word's best sentence so far: its index in _japanese, and its score.
    u32* _best       = (u32*)malloc(sizeof(u32) * _all);
    i32* _best_score = (i32*)malloc(sizeof(i32) * _all);
    for(u32 _w = 0; _w < _all; _w++) {
        _best[_w]       = UINT32_MAX;
        _best_score[_w] = 0x7FFFFFFF;
    }
    const kana_bake_sentence* _js = (const kana_bake_sentence*)_japanese.memory;
    u32 _candidates = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_japanese); _i++) {
        // As code points (and where each is), within the lengths kept.
        u32 _cps[KANA_BAKE_SENT_MAX + 1u];
        u32 _at[KANA_BAKE_SENT_MAX + 2u];
        u32 _n = 0;
        b8  _fits = true;
        for(u32 _p = 0; _p < _js[_i].len;) {
            if(_n == KANA_BAKE_SENT_MAX + 1u) {
                _fits = false;
                break;
            }
            u32       _len = 0;
            const u32 _cp  = kana_bake_decode_utf8((const c8*)_jdata + _js[_i].at + _p, &_len);
            _at[_n]    = _p;
            _cps[_n++] = _cp;
            _p += _len > 0 ? _len : 1u;
        }
        if(!_fits || _n < KANA_BAKE_SENT_MIN || _n > KANA_BAKE_SENT_MAX) {
            continue;
        }
        _at[_n] = _js[_i].len;
        // English it must have; each other language is a point in its favour.
        const u32 _en = kana_bake_link_of(&_links[0], _js[_i].id);
        if(_en == 0 || kana_bake_sentence_of(&_texts[0], _en) == NULL) {
            continue;
        }
        _candidates++;
        i32 _score = (i32)(_n > KANA_BAKE_SENT_IDEAL ? _n - KANA_BAKE_SENT_IDEAL : KANA_BAKE_SENT_IDEAL - _n) * 4;
        for(u32 _l = 1; _l < KANA_BAKE_SENT_LANGS; _l++) {
            const u32 _o = kana_bake_link_of(&_links[_l], _js[_i].id);
            _score += _o != 0 && kana_bake_sentence_of(&_texts[_l], _o) != NULL ? 0 : 6;
        }
        // Indexed: the words the index names.
        const kana_bake_sentence* _ix = kana_bake_sentence_of(&_index, _js[_i].id);
        if(_ix != NULL) {
            kana_bake_index_words((const c8*)_idata + _ix->at, _ix->len, _words, _sorted, _sorted_count, _i, _score, _best, _best_score);
            continue;
        }
        // Not: every stretch of two to five characters — a word as written, or a stem with kana after it.
        _score += KANA_BAKE_SENT_TEXT;
        for(u32 _a = 0; _a < _n; _a++) {
            for(u32 _len = 1; _len <= 5u && _a + _len <= _n; _len++) {
                const u8* _s     = _jdata + _js[_i].at + _at[_a];
                const u32 _bytes = _at[_a + _len] - _at[_a];
                const u32 _h     = kana_bake_hash_bytes(_s, _bytes);
                for(u32 _k = 0; _k < 2u; _k++) {
                    if(_k == 1u && !(_a + _len < _n && _cps[_a + _len] >= 0x3041u && _cps[_a + _len] <= 0x309Fu)) {
                        continue;   // a stem only with kana after it
                    }
                    for(u32 _t = _h & (_tsize - 1u); _table[_t].hash != 0u; _t = (_t + 1u) & (_tsize - 1u)) {
                        if(_table[_t].hash != _h || _table[_t].length != _bytes || _table[_t].stem != _k ||
                           memcmp(_words[_table[_t].word].written, _s, _bytes) != 0) {
                            continue;
                        }
                        if(_len == 1u) {
                            break;   // one kanji alone: which word, the text does not say
                        }
                        const u32 _w = _table[_t].word;
                        if(_score < _best_score[_w]) {
                            _best[_w]       = _i;
                            _best_score[_w] = _score;
                        }
                        break;
                    }
                }
            }
        }
    }

    // The sentences chosen, each once, numbered in order; then each word's.
    u32* _number = (u32*)malloc(sizeof(u32) * (rde_arr_length(&_japanese) > 0 ? rde_arr_length(&_japanese) : 1u));
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_japanese); _i++) {
        _number[_i] = UINT32_MAX;
    }
    _bake->sentence_words = (u32*)malloc(sizeof(u32) * (_bake->word_count > 0 ? _bake->word_count : 1u));
    for(u32 _i = 0; _i < _bake->word_count; _i++) {
        _bake->sentence_words[_i] = UINT32_MAX;
    }
    u32 _with = 0;
    for(u32 _w = 0; _w < _all; _w++) {
        if(_words[_w].index == UINT32_MAX || _best[_w] == UINT32_MAX) {
            continue;
        }
        const u32 _i = _best[_w];
        if(_number[_i] == UINT32_MAX) {
            _number[_i] = _bake->sentence_count++;
            kana_put_data(&_bake->sentences, _jdata + _js[_i].at, _js[_i].len);
            kana_put_u8(&_bake->sentences, 0u);
            for(u32 _l = 0; _l < KANA_BAKE_SENT_LANGS; _l++) {
                const u32                 _o = kana_bake_link_of(&_links[_l], _js[_i].id);
                const kana_bake_sentence* _t = _o != 0 ? kana_bake_sentence_of(&_texts[_l], _o) : NULL;
                if(_t != NULL) {
                    kana_put_data(&_bake->sentences, _tdata[_l] + _t->at, _t->len);
                }
                kana_put_u8(&_bake->sentences, 0u);
            }
        }
        _bake->sentence_words[_words[_w].index] = _number[_i];
        _with++;
    }
    rde_log_level(RDE_LOG_LEVEL_INFO, "bake: Tatoeba: %u Japanese sentences, %u with English and a fitting length; %u words with an example sentence, %u sentences kept (%.2f MB), %.2f s",
                  (u32)rde_arr_length(&_japanese), _candidates, _with, _bake->sentence_count, (f64)kana_bytes_size(&_bake->sentences) / (1024.0 * 1024.0),
                  rde_engine_get_time_now() - _t0);

    free(_number);
    free(_sorted);
    rde_arr_free(&_index);
    free(_idata);
    free(_best);
    free(_best_score);
    free(_table);
    for(u32 _l = 0; _l < KANA_BAKE_SENT_LANGS; _l++) {
        rde_arr_free(&_links[_l]);
        rde_arr_free(&_texts[_l]);
        free(_tdata[_l]);
    }
    rde_arr_free(&_japanese);
    free(_jdata);
    return true;
}

RDE_INTERNAL const c8* kana_bake_arg(i32 _argc, c8** _argv, const c8* _prefix, const c8* _default) {
    const usize _len = strlen(_prefix);
    for(i32 _i = 1; _i < _argc; _i++) {
        if(_argv[_i] != NULL && strncmp(_argv[_i], _prefix, _len) == 0) {
            return _argv[_i] + _len;
        }
    }
    return _default;
}

// --- look-alikes ----------------------------------------------------------------------------
// Each common character's closest by the matcher (match.h: its reference strokes
// ranked against the rest), appended to the file just written as 'LOOK' — the
// matcher reads the file, so it comes last. Common: a school grade, the rest of
// the Jouyou, or a JLPT level; and every kana. Kept: up to KANA_KANJI_LOOKALIKES
// common ones of the same script, of nearly as many strokes, close enough.

#define KANA_BAKE_LOOK_COST      11.0f   // the matcher's cost (a 100-unit box): 未 末 4, 土 士 7, 待 持 10; 人 大 13 is too far
#define KANA_BAKE_LOOK_COST_KANA 21.0f   // kana, simpler, cost more apart: わ れ 8, ツ ソ 15, シ ン 18
#define KANA_BAKE_LOOK_STROKES   3u      // strokes apart, at most

RDE_INTERNAL b8 kana_bake_look_kana(u32 _cp) {
    return (_cp >= 0x3041u && _cp <= 0x3096u) || (_cp >= 0x30A1u && _cp <= 0x30FAu);
}

// A plain kana (no small one, no dakuten): シ and ジ, ツ and ッ are the same
// character to a learner, not a mix-up.
RDE_INTERNAL b8 kana_bake_look_plain(u32 _cp) {
    static const c8 _plain[] = "あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわをん"
                               "アイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワヲン";
    for(const c8* _p = _plain; *_p != 0;) {
        if(kana_kanji_utf8_next(&_p) == _cp) {
            return true;
        }
    }
    return false;
}

RDE_INTERNAL b8 kana_bake_look_common(const kana_kanji_info* _info) {
    return kana_bake_look_kana(_info->codepoint) ? kana_bake_look_plain(_info->codepoint) : (_info->grade >= 1u && _info->grade <= 8u) || _info->jlpt_n != 0u;
}

RDE_INTERNAL b8 kana_bake_lookalikes(const c8* _path) {
    const f64     _t0 = rde_engine_get_time_now();
    kana_kanji_db _db;
    if(!kana_kanji_load(&_db, _path)) {
        return false;
    }
    kana_catalog _catalog;
    kana_catalog_init(&_catalog, &_db);
    kana_bytes _chunk = kana_bytes_new(_db.count * KANA_KANJI_LOOKALIKES * 4u + 16u);
    const u32  _at    = kana_chunk_begin(&_chunk, KANA_KANJI_CHUNK_LOOK);
    kana_put_u32(&_chunk, _db.count);
    u32 _with = 0;
    for(u32 _r = 0; _r < _db.count; _r++) {
        kana_kanji_info _info;
        u32             _kept[KANA_KANJI_LOOKALIKES] = { 0 };
        u32             _n = 0;
        if(kana_kanji_at(&_db, _r, &_info) && _info.strokes > 0u && kana_bake_look_common(&_info)) {
            const b8          _kana = kana_bake_look_kana(_info.codepoint);
            kana_match_stroke _strokes[64];
            const u32         _count = kana_match_reference(&_db, &_info, _strokes, 64u);
            kana_match_result _ranked[16];
            const u32         _m = kana_match_rank_strokes(&_db, &_catalog, _kana ? KANA_FILTER_ALL : KANA_FILTER_KANJI, _strokes, _count, _ranked, 16u);
            for(u32 _i = 0; _i < _m && _n < KANA_KANJI_LOOKALIKES && _ranked[_i].cost <= (_kana ? KANA_BAKE_LOOK_COST_KANA : KANA_BAKE_LOOK_COST); _i++) {
                kana_kanji_info _other;
                if(_ranked[_i].record == _r || !kana_kanji_at(&_db, _ranked[_i].record, &_other) || !kana_bake_look_common(&_other) ||
                   kana_bake_look_kana(_other.codepoint) != _kana ||
                   (_other.strokes > _info.strokes ? _other.strokes - _info.strokes : _info.strokes - _other.strokes) > KANA_BAKE_LOOK_STROKES) {
                    continue;
                }
                _kept[_n++] = _other.codepoint;
            }
        }
        for(u32 _k = 0; _k < KANA_KANJI_LOOKALIKES; _k++) {
            kana_put_u32(&_chunk, _kept[_k]);
        }
        _with += _n > 0u ? 1u : 0u;
    }
    kana_chunk_end(&_chunk, _at);
    kana_match_release();
    kana_catalog_destroy(&_catalog);
    kana_kanji_unload(&_db);

    // The file again, the chunk at its end.
    u32 _size = 0;
    u8* _data = kana_file_read(_path, &_size);
    if(_data == NULL) {
        rde_arr_free(&_chunk);
        return false;
    }
    kana_bytes _file = kana_bytes_new(_size + kana_bytes_size(&_chunk));
    kana_put_data(&_file, _data, _size);
    kana_put_data(&_file, _chunk.memory, kana_bytes_size(&_chunk));
    kana_file_free(_data);
    rde_arr_free(&_chunk);
    u32 _bytes = 0;
    if(!kana_bytes_write_and_free(&_file, _path, &_bytes)) {
        return false;
    }
    c8 _bak[RDE_MAX_PATH];
    snprintf(_bak, sizeof(_bak), "%s.bak", _path);
    remove(_bak);
    rde_log_color(RDE_LOG_COLOR_GREEN, "bake: look-alikes for %u characters (%.2f s); %s now %.2f MB", _with, rde_engine_get_time_now() - _t0, _path,
                  (f64)_bytes / (1024.0 * 1024.0));
    return true;
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
    // The full JMdict when it is there (English and the other languages), else JMdict_e (English).
    const c8* _jmdict   = kana_bake_arg(_argc, _argv, "--jmdict=",   rde_file_exists("data/raw/JMdict.xml") ? "data/raw/JMdict.xml" : "data/raw/JMdict_e.xml");
    const c8* _out      = kana_bake_arg(_argc, _argv, "--out=",      KANA_KANJI_FILE);
    const c8* _tatoeba  = kana_bake_arg(_argc, _argv, "--tatoeba=",  "data/raw/tatoeba");

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
        .word_freqs = kana_bytes_new(128u * 1024u),
        .sentences  = kana_bytes_new(1024u * 1024u),
        .min_coord = 1e9f,
        .max_coord = -1e9f,
    };
    for(u32 _l = 0; _l < KANA_BAKE_LANGS; _l++) {
        _bake.lang_text[_l]  = kana_bytes_new(512u * 1024u);
        _bake.lang_words[_l] = kana_bytes_new(128u * 1024u);
    }

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
        // After the words: their example sentences.
        if(_rc == 0 && !kana_bake_tatoeba(&_bake, _tatoeba)) {
            _rc = 1;
        }
    }

    if(_rc == 0) {
        const u32   _count = (u32)rde_arr_length(&_bake.chars);
        kana_bytes  _file  = kana_bytes_new(KANA_FILE_HEADER_SIZE + 16u + _count * KANA_KANJI_RECORD_SIZE + 8u + kana_bytes_size(&_bake.geometry) + 8u + kana_bytes_size(&_bake.text) +
                                            12u + _count * 4u + kana_bytes_size(&_bake.parts) +
                                            20u + _count * 4u + kana_bytes_size(&_bake.word_lists) + kana_bytes_size(&_bake.word_text) +
                                            kana_bytes_size(&_bake.word_freqs) + 16u + kana_bytes_size(&_bake.sentences) + _bake.word_count * 4u + 32u + 1u);
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
            // How common each word is (kanji.h 'WFRQ').
            _chunk = kana_chunk_begin(&_file, KANA_KANJI_CHUNK_WORD_FREQ);
            kana_put_u32(&_file, _bake.word_count);
            kana_put_data(&_file, _bake.word_freqs.memory, kana_bytes_size(&_bake.word_freqs));
            kana_chunk_end(&_file, _chunk);
            // Their example sentences (kanji.h 'SENT').
            if(_bake.sentence_count > 0 && _bake.sentence_words != NULL) {
                _chunk = kana_chunk_begin(&_file, KANA_KANJI_CHUNK_SENTENCES);
                kana_put_u32(&_file, _bake.sentence_count);
                kana_put_u32(&_file, kana_bytes_size(&_bake.sentences));
                kana_put_data(&_file, _bake.sentences.memory, kana_bytes_size(&_bake.sentences));
                kana_put_u32(&_file, _bake.word_count);
                for(u32 _w = 0; _w < _bake.word_count; _w++) {
                    kana_put_u32(&_file, _bake.sentence_words[_w]);
                }
                kana_chunk_end(&_file, _chunk);
            }
        }

        // The other languages: a chunk each (see kanji.h's 'LNxx').
        for(u32 _l = 0; _l < KANA_BAKE_LANGS; _l++) {
            if(_bake.lang_kanji_count[_l] == 0 && _bake.lang_word_count[_l] == 0) {
                continue;
            }
            const c8* _code = KANA_BAKE_LANG_LIST[_l].code;
            _chunk = kana_chunk_begin(&_file, KANA_TAG('L', 'N', _code[0], _code[1]));
            kana_put_u32(&_file, _bake.lang_kanji_count[_l]);
            for(u32 _i = 0; _i < _count; _i++) {
                if(_chars[_i].meaning_in[_l] != UINT32_MAX) {
                    kana_put_u32(&_file, _chars[_i].codepoint);
                    kana_put_u32(&_file, _chars[_i].meaning_in[_l]);
                }
            }
            kana_put_u32(&_file, _bake.lang_word_count[_l]);
            kana_put_data(&_file, _bake.lang_words[_l].memory, kana_bytes_size(&_bake.lang_words[_l]));
            kana_put_u32(&_file, kana_bytes_size(&_bake.lang_text[_l]));
            kana_put_data(&_file, _bake.lang_text[_l].memory, kana_bytes_size(&_bake.lang_text[_l]));
            kana_chunk_end(&_file, _chunk);
            rde_log_color(RDE_LOG_COLOR_GREEN, "  %s: %u kanji meanings, %u word meanings, %.2f MB", _code, _bake.lang_kanji_count[_l], _bake.lang_word_count[_l],
                          (f64)(kana_bytes_size(&_bake.lang_text[_l]) + kana_bytes_size(&_bake.lang_words[_l])) / (1024.0 * 1024.0));
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
        rde_log_color(RDE_LOG_COLOR_GREEN, "  of the words, %u common ones on no kanji's list (kept for reading text: wordsplit.h)", _bake.words_unlisted);
            if(!kana_bake_lookalikes(_out)) {
                rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: the look-alikes could not be added to %s", _out);
                _rc = 1;
            }
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
    rde_arr_free(&_bake.word_freqs);
    rde_arr_free(&_bake.sentences);
    free(_bake.sentence_words);
    for(u32 _l = 0; _l < KANA_BAKE_LANGS; _l++) {
        rde_arr_free(&_bake.lang_text[_l]);
        rde_arr_free(&_bake.lang_words[_l]);
    }
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
