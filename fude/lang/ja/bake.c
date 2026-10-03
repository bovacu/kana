#include "lang/ja/bake.h"
#include "lang/lang.h"

#include <string.h>

#if !defined(RDE_PLATFORM_MOBILE)

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "drawing/base/kfile.h"
#include "drawing/base/utf8.h"
#include "study/chars/bake.h"
#include "study/chars/kanji.h"
#include "lang/ja/chart.h"
#include "lang/ja/romaji.h"
#include "study/chars/catalog.h"
#include "study/handwriting/match.h"

// ===========================================================================
// See bake.h: Japanese's readers — KANJIDIC2, the JLPT lists, JMdict, Tatoeba's
// Japanese — over the study's bake (study/chars/bake.h: KanjiVG, the words'
// choice, the file, the look-alikes).
// ===========================================================================

// JMdict: example words.
#define FUDE_BAKE_UNCOMMON      1000    // an uncommon word's score starts here: after every common one
#define FUDE_BAKE_WORD_MEANING  48u     // glosses after the first are added while the meaning stays this short
#define FUDE_BAKE_ENTRY_KEBS    8u      // written forms read per entry (more are rare, and uncommon)
#define FUDE_BAKE_ENTRY_REBS    8u
#define FUDE_BAKE_ENTRY_SENSES  4u
#define FUDE_BAKE_ENTRY_RESTR   4u

// Meanings in other languages: KANJIDIC2's m_lang and full JMdict's xml:lang
// (JMdict_e has English only; JMdict has them all). No JMdict gloss is in
// Portuguese: its words show in English (kanji.h's fallback).
#define FUDE_BAKE_JA_LANGS 3u
static const struct { const c8* code; const c8* kanjidic; const c8* jmdict; } FUDE_BAKE_LANG_LIST[FUDE_BAKE_JA_LANGS] = {
    { "es", "es", "spa" },
    { "pt", "pt", NULL  },
    { "fr", "fr", "fre" },
};

// --- KANJIDIC2 -------------------------------------------------------------------------

RDE_INTERNAL b8 fude_bake_kanjidic(fude_bake* _bake, const c8* _path) {
    const f64 _t0 = rde_engine_get_time_now();
    rde_xml_entry* _root = rde_xml_load_from_file(_path, rde_memory_allocator_get_default_std());
    if(_root == NULL) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: could not parse %s", _path);
        return false;
    }
    rde_log_level(RDE_LOG_LEVEL_INFO, "bake: parsed %s in %.2f s", _path, rde_engine_get_time_now() - _t0);

    for(const rde_xml_entry* _c = _root->child; _c != NULL; _c = _c->next) {
        if(!fude_xml_is(_c, "character")) {
            continue;
        }

        u32 _cp = 0;
        u32 _strokes = 0, _grade = 0, _jlpt = 0, _freq = 0, _radical = 0;
        c8  _on[512] = "", _kun[512] = "", _meanings[1024] = "";
        c8  _meanings_in[FUDE_BAKE_JA_LANGS][1024];
        memset(_meanings_in, 0, sizeof(_meanings_in));

        for(const rde_xml_entry* _f = _c->child; _f != NULL; _f = _f->next) {
            if(fude_xml_is(_f, "literal")) {
                u32 _len;
                _cp = fude_bake_decode_utf8(fude_xml_text(_f), &_len);
            } else if(fude_xml_is(_f, "radical")) {
                for(const rde_xml_entry* _r = _f->child; _r != NULL; _r = _r->next) {
                    const c8* _type = fude_xml_attr(_r, "rad_type");
                    if(fude_xml_is(_r, "rad_value") && _type != NULL && strcmp(_type, "classical") == 0) {
                        _radical = (u32)atoi(fude_xml_text(_r));
                    }
                }
            } else if(fude_xml_is(_f, "misc")) {
                for(const rde_xml_entry* _m = _f->child; _m != NULL; _m = _m->next) {
                    if(fude_xml_is(_m, "grade"))                         { _grade = (u32)atoi(fude_xml_text(_m)); }
                    else if(fude_xml_is(_m, "stroke_count") && !_strokes) { _strokes = (u32)atoi(fude_xml_text(_m)); }   // the first is the accepted one
                    else if(fude_xml_is(_m, "freq"))                     { _freq = (u32)atoi(fude_xml_text(_m)); }
                    else if(fude_xml_is(_m, "jlpt"))                     { _jlpt = (u32)atoi(fude_xml_text(_m)); }
                }
            } else if(fude_xml_is(_f, "reading_meaning")) {
                for(const rde_xml_entry* _g = _f->child; _g != NULL; _g = _g->next) {
                    if(!fude_xml_is(_g, "rmgroup")) {
                        continue;
                    }
                    for(const rde_xml_entry* _x = _g->child; _x != NULL; _x = _x->next) {
                        if(fude_xml_is(_x, "reading")) {
                            const c8* _type = fude_xml_attr(_x, "r_type");
                            if(_type != NULL && strcmp(_type, "ja_on") == 0)  { fude_bake_join(_on,  sizeof(_on),  "、", fude_xml_text(_x)); }
                            if(_type != NULL && strcmp(_type, "ja_kun") == 0) { fude_bake_join(_kun, sizeof(_kun), "、", fude_xml_text(_x)); }
                        } else if(fude_xml_is(_x, "meaning") && fude_xml_attr(_x, "m_lang") == NULL) {   // no m_lang: English
                            fude_bake_join(_meanings, sizeof(_meanings), ", ", fude_xml_text(_x));
                        } else if(fude_xml_is(_x, "meaning")) {
                            const c8* _lang = fude_xml_attr(_x, "m_lang");
                            for(u32 _l = 0; _l < FUDE_BAKE_JA_LANGS; _l++) {
                                if(strcmp(_lang, FUDE_BAKE_LANG_LIST[_l].kanjidic) == 0) {
                                    fude_bake_join(_meanings_in[_l], sizeof(_meanings_in[_l]), ", ", fude_xml_text(_x));
                                }
                            }
                        }
                    }
                }
            }
        }

        fude_bake_char* _char = _cp != 0 ? fude_bake_find(_bake, _cp) : NULL;
        if(_char == NULL) {
            continue;   // no strokes for it: nothing to practise
        }

        _char->grade     = (u8)(_grade   < 256u ? _grade   : 0u);
        _char->jlpt      = (u8)(_jlpt    < 256u ? _jlpt    : 0u);
        _char->radical   = (u8)(_radical < 256u ? _radical : 0u);
        _char->frequency = (u16)(_freq < 65536u ? _freq : 0u);
        _char->text      = fude_bytes_size(&_bake->text);
        fude_put_data(&_bake->text, _on,       (u32)strlen(_on) + 1u);
        fude_put_data(&_bake->text, _kun,      (u32)strlen(_kun) + 1u);
        fude_put_data(&_bake->text, _meanings, (u32)strlen(_meanings) + 1u);
        for(u32 _l = 0; _l < FUDE_BAKE_JA_LANGS; _l++) {
            if(_meanings_in[_l][0] != 0) {
                _char->meaning_in[_l] = fude_bytes_size(&_bake->lang_text[_l]);
                fude_put_data(&_bake->lang_text[_l], _meanings_in[_l], (u32)strlen(_meanings_in[_l]) + 1u);
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

RDE_INTERNAL u32 fude_bake_hex(const c8* _s, u32 _digits) {
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

RDE_INTERNAL void fude_bake_jlpt_level(fude_bake* _bake, const c8* _dir, u32 _level) {
    c8 _path[RDE_MAX_PATH];
    snprintf(_path, sizeof(_path), "%s/n%u-kanji-char-eng.mem", _dir, _level);
    if(!rde_file_exists(_path)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "bake: %s not found; N%u left unlisted", _path, _level);
        return;
    }

    u32 _size = 0;
    u8* _data = fude_bake_slurp(_path, &_size);
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
            if(_v[0] == '\\' && _v[1] == 'u')      { _cp = fude_bake_hex(_v + 2, 4); }
            else if(_v[0] == '\\' && _v[1] == 'U') { _cp = fude_bake_hex(_v + 2, 8); }
            else                                   { u32 _n; _cp = fude_bake_decode_utf8(_v, &_n); }

            fude_bake_char* _char = _cp != 0 ? fude_bake_find(_bake, _cp) : NULL;
            if(_char == NULL) {
                _bake->level_missing++;
            } else if(_char->level == 0) {   // the lists are disjoint; if not, the easier level wins
                _char->level = (u8)_level;
                _bake->level_listed[_level]++;
            }
        }
        _s = _next;
    }

    free(_data);
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
    c8  keb[FUDE_BAKE_ENTRY_KEBS][4u * FUDE_BAKE_WORD_CHARS + 1u];
    b8  keb_long[FUDE_BAKE_ENTRY_KEBS];     // too long to keep
    b8  keb_bad[FUDE_BAKE_ENTRY_KEBS];      // irregular, outdated, rare or search-only
    u32 keb_common[FUDE_BAKE_ENTRY_KEBS];   // how many of the common lists have it
    u32 keb_nf[FUDE_BAKE_ENTRY_KEBS];       // newspaper frequency band 1..48 (0: none)
    b8  keb_ichi[FUDE_BAKE_ENTRY_KEBS];     // on Ichimango's list of everyday words
    u32 kebs;

    c8  reb[FUDE_BAKE_ENTRY_REBS][64];
    b8  reb_bad[FUDE_BAKE_ENTRY_REBS];      // no kanji, irregular, outdated or search-only
    b8  reb_common[FUDE_BAKE_ENTRY_REBS];
    c8  reb_restr[FUDE_BAKE_ENTRY_REBS][FUDE_BAKE_ENTRY_RESTR][4u * FUDE_BAKE_WORD_CHARS + 1u];   // only for these written forms
    u32 reb_restrs[FUDE_BAKE_ENTRY_REBS];
    u32 rebs;

    c8  sense[FUDE_BAKE_ENTRY_SENSES][128];
    b8  sense_kana[FUDE_BAKE_ENTRY_SENSES];  // "usually written in kana"
    c8  sense_stagk[FUDE_BAKE_ENTRY_SENSES][FUDE_BAKE_ENTRY_RESTR][4u * FUDE_BAKE_WORD_CHARS + 1u];
    u32 sense_stagks[FUDE_BAKE_ENTRY_SENSES];
    u32 senses;

    u32 element;       // what is open: 0 none, 1 k_ele, 2 r_ele, 3 sense
    b8  sense_full;    // the open sense's glosses are all in
    // Full JMdict: a sense in another language (its glosses xml:lang) is not an
    // English one; the entry's first sense in each language is its meaning there.
    b8  sense_foreign;
    i32 sense_lang;    // FUDE_BAKE_LANG_LIST index, -1 another language
    c8  foreign[FUDE_BAKE_LANGS][128];
    b8  foreign_full[FUDE_BAKE_LANGS];   // that language's first sense is in
} fude_bake_entry;

// The text of "<tag ...>text</tag>" on _line, into _out (entities decoded). False
// when the line is not that element.
RDE_INTERNAL b8 fude_bake_element(const c8* _line, const c8* _tag, c8* _out, usize _size) {
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
    fude_bake_join(_out, _size, "", _raw);
    return true;
}

// Is every character of _written one an example can hold (kanji with strokes,
// kana, 々), with at least one kanji? Its length in characters into *_chars.
RDE_INTERNAL b8 fude_bake_word_usable(fude_bake* _bake, const c8* _written, u32* _chars) {
    b8  _kanji = false;
    u32 _n     = 0;
    for(const c8* _p = _written; *_p != 0; _n++) {
        u32       _len = 0;
        const u32 _cp  = fude_bake_decode_utf8(_p, &_len);
        if(_cp == 0) {
            return false;
        }
        _p += _len;
        if(fude_bake_is_kanji(_cp)) {
            if(fude_bake_find(_bake, _cp) == NULL) {
                return false;
            }
            _kanji = true;
        } else if(!((_cp >= 0x3041u && _cp <= 0x3096u) || (_cp >= 0x30A1u && _cp <= 0x30FAu) || _cp == 0x30FCu || _cp == 0x3005u)) {
            return false;
        }
    }
    *_chars = _n;
    return _kanji && _n <= FUDE_BAKE_WORD_CHARS;
}

// One kanji, at the start, and kana after it: 見る, 読み (and 月 alone).
RDE_INTERNAL b8 fude_bake_own_word(const c8* _written) {
    u32 _kanji = 0;
    u32 _i     = 0;
    for(const c8* _p = _written; *_p != 0; _i++) {
        u32       _len = 0;
        const u32 _cp  = fude_bake_decode_utf8(_p, &_len);
        if(_len == 0) {
            break;
        }
        _p += _len;
        if(fude_bake_is_kanji(_cp)) {
            if(_i != 0) {
                return false;
            }
            _kanji++;
        }
    }
    return _kanji == 1u;
}

// Kana → plain romaji, long vowels folded (とうきょう → tokyo), into _out.
RDE_INTERNAL void fude_bake_romanize(const c8* _kana, c8* _out, usize _size) {
    usize _n      = 0;
    b8    _double = false;   // after っ
    _out[0] = 0;
    for(const c8* _p = _kana; *_p != 0;) {
        u32       _len = 0;
        const u32 _cp  = fude_bake_decode_utf8(_p, &_len);
        if(_len == 0) {
            break;
        }
        _p += _len;
        const u32 _h = (_cp >= 0x30A1u && _cp <= 0x30F6u) ? _cp - 0x60u : _cp;   // katakana as hiragana
        if(_h == 0x3063u) { _double = true; continue; }                           // っ
        if(_h == 0x30FCu) { continue; }                                            // ー
        const c8* _r = fude_romaji(_h);
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
RDE_INTERNAL b8 fude_bake_is_name(const c8* _reading, const c8* _meaning) {
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
    fude_bake_romanize(_reading, _romaji, sizeof(_romaji));
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

RDE_INTERNAL b8 fude_bake_listed(const c8 _list[][4u * FUDE_BAKE_WORD_CHARS + 1u], u32 _count, const c8* _s) {
    for(u32 _i = 0; _i < _count; _i++) {
        if(strcmp(_list[_i], _s) == 0) {
            return true;
        }
    }
    return false;
}

// A whole entry read: each common written form becomes a word, a candidate for
// each of its kanji.
RDE_INTERNAL void fude_bake_entry_done(fude_bake* _bake, const fude_bake_entry* _e) {
    _bake->word_entries++;
    for(u32 _k = 0; _k < _e->kebs; _k++) {
        u32 _chars = 0;
        if(_e->keb_bad[_k] || _e->keb_long[_k] || !fude_bake_word_usable(_bake, _e->keb[_k], &_chars)) {
            continue;
        }

        // Its reading: the first that goes with this form, a common one if any.
        i32 _reading = -1;
        for(u32 _r = 0; _r < _e->rebs; _r++) {
            if(_e->reb_bad[_r] || (_e->reb_restrs[_r] > 0 && !fude_bake_listed(_e->reb_restr[_r], _e->reb_restrs[_r], _e->keb[_k]))) {
                continue;
            }
            if(_reading < 0 || (_e->reb_common[_r] && !_e->reb_common[_reading])) {
                _reading = (i32)_r;
            }
        }
        // Its meaning: the first sense that is not for other forms only.
        i32 _sense = -1;
        for(u32 _s = 0; _s < _e->senses && _sense < 0; _s++) {
            if(_e->sense_stagks[_s] == 0 || fude_bake_listed(_e->sense_stagk[_s], _e->sense_stagks[_s], _e->keb[_k])) {
                _sense = (i32)_s;
            }
        }
        if(_reading < 0 || _sense < 0 || _e->sense[_sense][0] == 0) {
            continue;
        }

        fude_bake_word _word = { .index = UINT32_MAX, .chars = (u8)_chars, .common = _e->keb_common[_k] > 0 };
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
        for(u32 _l = 0; _l < FUDE_BAKE_JA_LANGS; _l++) {
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
            _base -= _chars == 1u ? (_e->keb_ichi[_k] ? 120 : 100) : fude_bake_own_word(_word.written) ? 6 : 0;
        } else {
            _base = FUDE_BAKE_UNCOMMON + (_e->keb_nf[_k] != 0 ? (i32)_e->keb_nf[_k] : 60);
        }
        _base += 3 * ((i32)_chars > 2 ? (i32)_chars - 2 : 0);   // short words first
        _base += _e->sense_kana[_sense] ? 20 : 0;                 // usually kana: a poor example of its kanji
        _base += fude_bake_is_name(_word.reading, _word.meaning) ? 20 : 0;
        _base += _e->reb_common[_reading] ? 0 : 2;                // 下 した (a common reading) before 下 もと
        _base += _e->keb_common[0] > _e->keb_common[_k] ? 1 : 0;  // 本 ほん before 本 もと, whose usual form is 元

        // A candidate for each of its kanji (once each), less so the harder its
        // other kanji are than that one.
        u32 _seen[FUDE_BAKE_WORD_CHARS];
        u32 _seen_count = 0;
        for(const c8* _p = _word.written; *_p != 0;) {
            u32       _len = 0;
            const u32 _cp  = fude_bake_decode_utf8(_p, &_len);
            _p += _len;
            b8 _dup = !fude_bake_is_kanji(_cp);
            for(u32 _i = 0; !_dup && _i < _seen_count; _i++) {
                _dup = _seen[_i] == _cp;
            }
            if(_dup) {
                continue;
            }
            _seen[_seen_count++] = _cp;

            const fude_bake_char* _self  = fude_bake_find(_bake, _cp);
            i32                   _score = _base;
            for(const c8* _q = _word.written; *_q != 0;) {
                u32       _l2 = 0;
                const u32 _o  = fude_bake_decode_utf8(_q, &_l2);
                _q += _l2;
                if(_o == _cp || !fude_bake_is_kanji(_o)) {
                    continue;
                }
                const i32 _harder = fude_bake_difficulty(fude_bake_find(_bake, _o)) - fude_bake_difficulty(_self);
                _score += _harder > 0 ? 2 * _harder : 0;
            }
            const fude_bake_word_ref _ref = { .codepoint = _cp, .word = _index, .score = _score };
            rde_arr_add(&_bake->word_refs, &_ref);
        }
    }
}

RDE_INTERNAL b8 fude_bake_jmdict(fude_bake* _bake, const c8* _path) {
    if(!rde_file_exists(_path)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "bake: %s not found; no example words", _path);
        return true;
    }
    const f64 _t0   = rde_engine_get_time_now();
    u32       _size = 0;
    u8*       _data = fude_bake_slurp(_path, &_size);
    if(_data == NULL) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: could not read %s", _path);
        return false;
    }

    static fude_bake_entry _e;   // big; one entry at a time
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
            fude_bake_entry_done(_bake, &_e);
        } else if(strcmp(_line, "<k_ele>") == 0) {
            _e.element = _e.kebs < FUDE_BAKE_ENTRY_KEBS ? 1u : 0u;
        } else if(strcmp(_line, "<r_ele>") == 0) {
            _e.element = _e.rebs < FUDE_BAKE_ENTRY_REBS ? 2u : 0u;
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
                if(_e.senses < FUDE_BAKE_ENTRY_SENSES) {   // not an English one: its slot free again
                    _e.sense[_e.senses][0]     = 0;
                    _e.sense_kana[_e.senses]   = false;
                    _e.sense_stagks[_e.senses] = 0;
                }
            } else if(_e.element == 3u && _e.senses < FUDE_BAKE_ENTRY_SENSES) {
                _e.senses++;
            }
            _e.element = 0;
        } else if(_e.element == 1u) {
            const u32 _k = _e.kebs;
            if(fude_bake_element(_line, "keb", _text, sizeof(_text))) {
                _e.keb_long[_k] = strlen(_text) >= sizeof(_e.keb[_k]);
                snprintf(_e.keb[_k], sizeof(_e.keb[_k]), "%s", _e.keb_long[_k] ? "" : _text);
            } else if(fude_bake_element(_line, "ke_inf", _text, sizeof(_text))) {
                _e.keb_bad[_k] = _e.keb_bad[_k] || strcmp(_text, "&iK;") == 0 || strcmp(_text, "&oK;") == 0 ||
                                 strcmp(_text, "&rK;") == 0 || strcmp(_text, "&sK;") == 0 || strcmp(_text, "&io;") == 0;
            } else if(fude_bake_element(_line, "ke_pri", _text, sizeof(_text))) {
                if(strcmp(_text, "news1") == 0 || strcmp(_text, "ichi1") == 0 || strcmp(_text, "spec1") == 0 || strcmp(_text, "spec2") == 0) {
                    _e.keb_common[_k]++;
                    _e.keb_ichi[_k] = _e.keb_ichi[_k] || strcmp(_text, "ichi1") == 0;
                } else if(strncmp(_text, "nf", 2) == 0) {
                    _e.keb_nf[_k] = (u32)atoi(_text + 2);
                }
            }
        } else if(_e.element == 2u) {
            const u32 _r = _e.rebs;
            if(fude_bake_element(_line, "reb", _text, sizeof(_text))) {
                snprintf(_e.reb[_r], sizeof(_e.reb[_r]), "%s", _text);
            } else if(strncmp(_line, "<re_nokanji", 11) == 0) {
                _e.reb_bad[_r] = true;
            } else if(fude_bake_element(_line, "re_inf", _text, sizeof(_text))) {
                _e.reb_bad[_r] = _e.reb_bad[_r] || strcmp(_text, "&ik;") == 0 || strcmp(_text, "&ok;") == 0 || strcmp(_text, "&sk;") == 0;
            } else if(fude_bake_element(_line, "re_pri", _text, sizeof(_text))) {
                _e.reb_common[_r] = _e.reb_common[_r] || strcmp(_text, "news1") == 0 || strcmp(_text, "ichi1") == 0 ||
                                    strcmp(_text, "spec1") == 0 || strcmp(_text, "spec2") == 0;
            } else if(fude_bake_element(_line, "re_restr", _text, sizeof(_text))) {
                if(_e.reb_restrs[_r] < FUDE_BAKE_ENTRY_RESTR && strlen(_text) < sizeof(_e.reb_restr[_r][0])) {
                    snprintf(_e.reb_restr[_r][_e.reb_restrs[_r]++], sizeof(_e.reb_restr[_r][0]), "%s", _text);
                }
            }
        } else if(_e.element == 3u && strncmp(_line, "<gloss xml:lang=\"", 17) == 0 && strncmp(_line + 17, "eng", 3) != 0) {
            // Another language's gloss: this sense is that language's.
            _e.sense_foreign = true;
            _e.sense_lang    = -1;
            for(u32 _l = 0; _l < FUDE_BAKE_JA_LANGS; _l++) {
                if(FUDE_BAKE_LANG_LIST[_l].jmdict != NULL && strncmp(_line + 17, FUDE_BAKE_LANG_LIST[_l].jmdict, 3) == 0) {
                    _e.sense_lang = (i32)_l;
                }
            }
            if(_e.sense_lang >= 0 && !_e.foreign_full[_e.sense_lang] && fude_bake_element(_line, "gloss", _text, sizeof(_text))) {
                c8*         _to   = _e.foreign[_e.sense_lang];
                const usize _have = strlen(_to);
                if(_have == 0) {
                    snprintf(_to, sizeof(_e.foreign[0]), "%s", _text);
                } else if(_have + 2u + strlen(_text) <= FUDE_BAKE_WORD_MEANING) {
                    fude_bake_join(_to, sizeof(_e.foreign[0]), "; ", _text);
                }
            }
        } else if(_e.element == 3u && _e.senses < FUDE_BAKE_ENTRY_SENSES) {
            const u32 _n = _e.senses;
            if(fude_bake_element(_line, "gloss", _text, sizeof(_text))) {
                // The first gloss always; more while the meaning stays short.
                const usize _have = strlen(_e.sense[_n]);
                if(_have == 0) {
                    snprintf(_e.sense[_n], sizeof(_e.sense[_n]), "%s", _text);
                } else if(!_e.sense_full && _have + 2u + strlen(_text) <= FUDE_BAKE_WORD_MEANING) {
                    fude_bake_join(_e.sense[_n], sizeof(_e.sense[_n]), "; ", _text);
                } else {
                    _e.sense_full = true;
                }
            } else if(fude_bake_element(_line, "misc", _text, sizeof(_text))) {
                _e.sense_kana[_n] = _e.sense_kana[_n] || strcmp(_text, "&uk;") == 0;
            } else if(fude_bake_element(_line, "stagk", _text, sizeof(_text))) {
                if(_e.sense_stagks[_n] < FUDE_BAKE_ENTRY_RESTR && strlen(_text) < sizeof(_e.sense_stagk[_n][0])) {
                    snprintf(_e.sense_stagk[_n][_e.sense_stagks[_n]++], sizeof(_e.sense_stagk[_n][0]), "%s", _text);
                }
            }
        }
    }

    free(_data);
    rde_log_level(RDE_LOG_LEVEL_INFO, "bake: read %s in %.2f s", _path, rde_engine_get_time_now() - _t0);
    fude_bake_choose_words(_bake);
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
// characters, between FUDE_BAKE_SENT_MIN and _MAX), then translated into more of
// the languages; one the index marks as a good example (~) first. kanji.h 'SENT'.

#define FUDE_BAKE_SENT_MIN   6u
#define FUDE_BAKE_SENT_MAX   40u
#define FUDE_BAKE_SENT_IDEAL 14
#define FUDE_BAKE_SENT_LANGS 4u     // en, es, fr, pt: their order in the file
#define FUDE_BAKE_SENT_GOOD  10     // a score's bonus: the index says it is a good example
#define FUDE_BAKE_SENT_TEXT  40     // ...its malus: found as text, not in the index
static const c8* const FUDE_BAKE_SENT_CODES[FUDE_BAKE_SENT_LANGS] = { "eng", "spa", "fra", "por" };

typedef struct { u32 id; u32 at; u32 len; } fude_bake_sentence;    // a sentence: its id, where its text is in its file
typedef struct { u32 from; u32 to; } fude_bake_link;               // a Japanese sentence's id, a translation's id

RDE_INTERNAL int fude_bake_by_sentence_id(const void* _a, const void* _b) {
    const u32 _x = ((const fude_bake_sentence*)_a)->id, _y = ((const fude_bake_sentence*)_b)->id;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

RDE_INTERNAL int fude_bake_by_link(const void* _a, const void* _b) {
    const fude_bake_link* _x = (const fude_bake_link*)_a;
    const fude_bake_link* _y = (const fude_bake_link*)_b;
    if(_x->from != _y->from) {
        return _x->from < _y->from ? -1 : 1;
    }
    return _x->to < _y->to ? -1 : (_x->to > _y->to ? 1 : 0);
}

RDE_INTERNAL int fude_bake_by_u32(const void* _a, const void* _b) {
    const u32 _x = *(const u32*)_a, _y = *(const u32*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// The kept words by written form (word numbers, alike ones together): for the index.
static const fude_bake_word* fude_bake_sort_words;
RDE_INTERNAL int fude_bake_by_written(const void* _a, const void* _b) {
    return strcmp(fude_bake_sort_words[*(const u32*)_a].written, fude_bake_sort_words[*(const u32*)_b].written);
}

// Tatoeba's index line for a sentence ("word(reading)[sense]{as written}~ ..."):
// each word it names that is kept, scored _score (better when marked ~), as that
// word's best sentence _i when better than its best so far.
RDE_INTERNAL void fude_bake_index_words(const c8* _line, u32 _size, const fude_bake_word* _words, const u32* _sorted, u32 _sorted_count, u32 _i, i32 _score,
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
        c8  _written[4u * FUDE_BAKE_WORD_CHARS + 1u];
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
            const u32 _cp  = fude_bake_decode_utf8(_c, &_len);
            if(fude_bake_is_kanji(_cp)) {
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
            const fude_bake_word* _w = &_words[_sorted[_k]];
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
        const i32 _s = _score - (_good ? FUDE_BAKE_SENT_GOOD : 0);
        if(_word != UINT32_MAX && _s < _best_score[_word]) {
            _best[_word]       = _i;
            _best_score[_word] = _s;
        }
    }
}

// A Tatoeba export's lines, "id <tab> lang <tab> text": each sentence (whose id
// is in _wanted, sorted, when given) into _out.
RDE_INTERNAL void fude_bake_read_sentences(const u8* _data, u32 _size, const u32* _wanted, u32 _wanted_count, rde_arr* _out) {
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
            if(_p < _end && _data[_p] == '\t' && (_wanted == NULL || bsearch(&_id, _wanted, _wanted_count, sizeof(u32), fude_bake_by_u32) != NULL)) {
                u32 _len = _end - (_p + 1u);
                if(_len > 0 && _data[_p + 1u + _len - 1u] == '\r') {
                    _len--;
                }
                const fude_bake_sentence _s = { _id, _p + 1u, _len };
                rde_arr_add(_out, (any)&_s);
            }
        }
        _at = _end + 1u;
    }
    qsort(_out->memory, rde_arr_length(_out), sizeof(fude_bake_sentence), fude_bake_by_sentence_id);
}

// "from <tab> to" lines, sorted.
RDE_INTERNAL void fude_bake_read_links(const u8* _data, u32 _size, rde_arr* _out) {
    u32 _at = 0;
    while(_at < _size) {
        fude_bake_link _l = { 0, 0 };
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
    qsort(_out->memory, rde_arr_length(_out), sizeof(fude_bake_link), fude_bake_by_link);
}

// The first translation of Japanese sentence _from among _links: its id, or 0.
RDE_INTERNAL u32 fude_bake_link_of(const rde_arr* _links, u32 _from) {
    const fude_bake_link* _l  = (const fude_bake_link*)_links->memory;
    u32                   _lo = 0, _hi = (u32)rde_arr_length(_links);
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(_l[_mid].from < _from) { _lo = _mid + 1u; } else { _hi = _mid; }
    }
    return _lo < (u32)rde_arr_length(_links) && _l[_lo].from == _from ? _l[_lo].to : 0u;
}

RDE_INTERNAL const fude_bake_sentence* fude_bake_sentence_of(const rde_arr* _sentences, u32 _id) {
    const fude_bake_sentence _key = { _id, 0, 0 };
    return (const fude_bake_sentence*)bsearch(&_key, _sentences->memory, rde_arr_length(_sentences), sizeof(fude_bake_sentence), fude_bake_by_sentence_id);
}

// The words kept, by their written form (or a conjugating one's stem): an
// open-addressed table of word numbers.
typedef struct { u32 hash; u32 word; u16 length; u8 stem; } fude_bake_word_key;

RDE_INTERNAL u32 fude_bake_hash_bytes(const u8* _s, u32 _n) {
    u32 _h = 2166136261u;
    for(u32 _i = 0; _i < _n; _i++) {
        _h = (_h ^ _s[_i]) * 16777619u;
    }
    return _h != 0u ? _h : 1u;
}

// A whole (big) file, outside RDE's pool: Tatoeba's English alone is ~100 MB.
// Free with free(). NULL when it cannot be read.

RDE_INTERNAL b8 fude_bake_tatoeba(fude_bake* _bake, const c8* _dir) {
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
    u8* _jdata = fude_bake_slurp(_path, &_jsize);
    rde_arr _japanese = rde_arr_new(sizeof(fude_bake_sentence), _heap);
    fude_bake_read_sentences(_jdata, _jsize, NULL, 0, &_japanese);
    // The word index: "id <tab> meaning id <tab> words", read as the sentences are.
    snprintf(_path, sizeof(_path), "%s/jpn_indices.csv", _dir);
    u32     _isize = 0;
    u8*     _idata = rde_file_exists(_path) ? fude_bake_slurp(_path, &_isize) : NULL;
    rde_arr _index = rde_arr_new(sizeof(fude_bake_sentence), _heap);
    if(_idata != NULL) {
        fude_bake_read_sentences(_idata, _isize, NULL, 0, &_index);
    } else {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "bake: %s not found; sentences found as text only", _path);
    }
    rde_arr _links[FUDE_BAKE_SENT_LANGS];
    rde_arr _texts[FUDE_BAKE_SENT_LANGS];
    u8*     _tdata[FUDE_BAKE_SENT_LANGS];
    for(u32 _l = 0; _l < FUDE_BAKE_SENT_LANGS; _l++) {
        _links[_l] = rde_arr_new(sizeof(fude_bake_link), _heap);
        _texts[_l] = rde_arr_new(sizeof(fude_bake_sentence), _heap);
        _tdata[_l] = NULL;
        snprintf(_path, sizeof(_path), "%s/jpn-%s_links.tsv", _dir, FUDE_BAKE_SENT_CODES[_l]);
        u32 _size = 0;
        u8* _data = rde_file_exists(_path) ? fude_bake_slurp(_path, &_size) : NULL;
        if(_data != NULL) {
            fude_bake_read_links(_data, _size, &_links[_l]);
            free(_data);
        }
        // Only the sentences linked to: their ids, then their texts.
        const u32 _n      = (u32)rde_arr_length(&_links[_l]);
        u32*      _wanted = (u32*)malloc(sizeof(u32) * (_n > 0 ? _n : 1u));
        for(u32 _i = 0; _i < _n; _i++) {
            _wanted[_i] = ((const fude_bake_link*)_links[_l].memory)[_i].to;
        }
        qsort(_wanted, _n, sizeof(u32), fude_bake_by_u32);
        snprintf(_path, sizeof(_path), "%s/%s_sentences.tsv", _dir, FUDE_BAKE_SENT_CODES[_l]);
        u32 _size2 = 0;
        _tdata[_l] = rde_file_exists(_path) ? fude_bake_slurp(_path, &_size2) : NULL;
        if(_tdata[_l] != NULL) {
            fude_bake_read_sentences(_tdata[_l], _size2, _wanted, _n, &_texts[_l]);
        }
        free(_wanted);
    }

    // The words kept, by written form, and by stem where they conjugate.
    fude_bake_word* _words = (fude_bake_word*)_bake->words.memory;
    const u32       _all   = (u32)rde_arr_length(&_bake->words);
    u32             _tsize = 1u;
    while(_tsize < _bake->word_count * 4u) {
        _tsize <<= 1u;
    }
    fude_bake_word_key* _table = (fude_bake_word_key*)calloc(_tsize, sizeof(fude_bake_word_key));
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
            const u32 _cp  = fude_bake_decode_utf8((const c8*)_s + _i, &_len);
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
            const u32 _h   = fude_bake_hash_bytes(_s, _len);
            for(u32 _i = _h & (_tsize - 1u);; _i = (_i + 1u) & (_tsize - 1u)) {
                if(_table[_i].hash == 0u) {
                    _table[_i] = (fude_bake_word_key){ _h, _w, (u16)_len, (u8)_k };
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
    fude_bake_sort_words = _words;
    qsort(_sorted, _sorted_count, sizeof(u32), fude_bake_by_written);

    // Each word's best sentence so far: its index in _japanese, and its score.
    u32* _best       = (u32*)malloc(sizeof(u32) * _all);
    i32* _best_score = (i32*)malloc(sizeof(i32) * _all);
    for(u32 _w = 0; _w < _all; _w++) {
        _best[_w]       = UINT32_MAX;
        _best_score[_w] = 0x7FFFFFFF;
    }
    const fude_bake_sentence* _js = (const fude_bake_sentence*)_japanese.memory;
    u32 _candidates = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_japanese); _i++) {
        // As code points (and where each is), within the lengths kept.
        u32 _cps[FUDE_BAKE_SENT_MAX + 1u];
        u32 _at[FUDE_BAKE_SENT_MAX + 2u];
        u32 _n = 0;
        b8  _fits = true;
        for(u32 _p = 0; _p < _js[_i].len;) {
            if(_n == FUDE_BAKE_SENT_MAX + 1u) {
                _fits = false;
                break;
            }
            u32       _len = 0;
            const u32 _cp  = fude_bake_decode_utf8((const c8*)_jdata + _js[_i].at + _p, &_len);
            _at[_n]    = _p;
            _cps[_n++] = _cp;
            _p += _len > 0 ? _len : 1u;
        }
        if(!_fits || _n < FUDE_BAKE_SENT_MIN || _n > FUDE_BAKE_SENT_MAX) {
            continue;
        }
        _at[_n] = _js[_i].len;
        // English it must have; each other language is a point in its favour.
        const u32 _en = fude_bake_link_of(&_links[0], _js[_i].id);
        if(_en == 0 || fude_bake_sentence_of(&_texts[0], _en) == NULL) {
            continue;
        }
        _candidates++;
        i32 _score = (i32)(_n > FUDE_BAKE_SENT_IDEAL ? _n - FUDE_BAKE_SENT_IDEAL : FUDE_BAKE_SENT_IDEAL - _n) * 4;
        for(u32 _l = 1; _l < FUDE_BAKE_SENT_LANGS; _l++) {
            const u32 _o = fude_bake_link_of(&_links[_l], _js[_i].id);
            _score += _o != 0 && fude_bake_sentence_of(&_texts[_l], _o) != NULL ? 0 : 6;
        }
        // Indexed: the words the index names.
        const fude_bake_sentence* _ix = fude_bake_sentence_of(&_index, _js[_i].id);
        if(_ix != NULL) {
            fude_bake_index_words((const c8*)_idata + _ix->at, _ix->len, _words, _sorted, _sorted_count, _i, _score, _best, _best_score);
            continue;
        }
        // Not: every stretch of two to five characters — a word as written, or a stem with kana after it.
        _score += FUDE_BAKE_SENT_TEXT;
        for(u32 _a = 0; _a < _n; _a++) {
            for(u32 _len = 1; _len <= 5u && _a + _len <= _n; _len++) {
                const u8* _s     = _jdata + _js[_i].at + _at[_a];
                const u32 _bytes = _at[_a + _len] - _at[_a];
                const u32 _h     = fude_bake_hash_bytes(_s, _bytes);
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
            fude_put_data(&_bake->sentences, _jdata + _js[_i].at, _js[_i].len);
            fude_put_u8(&_bake->sentences, 0u);
            for(u32 _l = 0; _l < FUDE_BAKE_SENT_LANGS; _l++) {
                const u32                 _o = fude_bake_link_of(&_links[_l], _js[_i].id);
                const fude_bake_sentence* _t = _o != 0 ? fude_bake_sentence_of(&_texts[_l], _o) : NULL;
                if(_t != NULL) {
                    fude_put_data(&_bake->sentences, _tdata[_l] + _t->at, _t->len);
                }
                fude_put_u8(&_bake->sentences, 0u);
            }
        }
        _bake->sentence_words[_words[_w].index] = _number[_i];
        _with++;
    }
    rde_log_level(RDE_LOG_LEVEL_INFO, "bake: Tatoeba: %u Japanese sentences, %u with English and a fitting length; %u words with an example sentence, %u sentences kept (%.2f MB), %.2f s",
                  (u32)rde_arr_length(&_japanese), _candidates, _with, _bake->sentence_count, (f64)fude_bytes_size(&_bake->sentences) / (1024.0 * 1024.0),
                  rde_engine_get_time_now() - _t0);

    free(_number);
    free(_sorted);
    rde_arr_free(&_index);
    free(_idata);
    free(_best);
    free(_best_score);
    free(_table);
    for(u32 _l = 0; _l < FUDE_BAKE_SENT_LANGS; _l++) {
        rde_arr_free(&_links[_l]);
        rde_arr_free(&_texts[_l]);
        free(_tdata[_l]);
    }
    rde_arr_free(&_japanese);
    free(_jdata);
    return true;
}

// --- look-alikes: Japanese's rules (bake.h's fude_bake_look) ------------------------------
// Common: a school grade, the rest of the Jouyou, or a JLPT level; and every plain
// kana. Kana are compared among themselves, kanji among the kanji.

#define FUDE_BAKE_LOOK_COST      11.0f   // the matcher's cost (a 100-unit box): 未 末 4, 土 士 7, 待 持 10; 人 大 13 is too far
#define FUDE_BAKE_LOOK_COST_KANA 21.0f   // kana, simpler, cost more apart: わ れ 8, ツ ソ 15, シ ン 18
#define FUDE_BAKE_LOOK_STROKES   3u      // strokes apart, at most

RDE_INTERNAL b8 fude_bake_look_kana(u32 _cp) {
    return (_cp >= 0x3041u && _cp <= 0x3096u) || (_cp >= 0x30A1u && _cp <= 0x30FAu);
}

// A plain kana (no small one, no dakuten): シ and ジ, ツ and ッ are the same
// character to a learner, not a mix-up.
RDE_INTERNAL b8 fude_bake_look_plain(u32 _cp) {
    static const c8 _plain[] = "あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわをん"
                               "アイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨラリルレロワヲン";
    for(const c8* _p = _plain; *_p != 0;) {
        if(fude_utf8_next(&_p) == _cp) {
            return true;
        }
    }
    return false;
}

RDE_INTERNAL b8 fude_bake_look_common(const fude_kanji_info* _info) {
    return fude_bake_look_kana(_info->codepoint) ? fude_bake_look_plain(_info->codepoint) : (_info->grade >= 1u && _info->grade <= 8u) || _info->level != 0u;
}

static const fude_bake_look FUDE_BAKE_LOOK = {
    .script      = fude_bake_look_kana,
    .common      = fude_bake_look_common,
    .cost        = FUDE_BAKE_LOOK_COST,
    .cost_script = FUDE_BAKE_LOOK_COST_KANA,
    .strokes     = FUDE_BAKE_LOOK_STROKES,
};

b8 fude_bake_requested(i32 _argc, c8** _argv) {
    for(i32 _i = 1; _i < _argc; _i++) {
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--bake") == 0) {
            return true;
        }
    }
    return false;
}

i32 fude_bake_run(i32 _argc, c8** _argv) {
    const c8* _kanjivg  = fude_bake_arg(_argc, _argv, "--kanjivg=",  "data/raw/kanjivg.xml");
    const c8* _kanjidic = fude_bake_arg(_argc, _argv, "--kanjidic=", "data/raw/kanjidic2.xml");
    const c8* _jlpt     = fude_bake_arg(_argc, _argv, "--jlpt=",     "data/raw/jlpt");
    // The full JMdict when it is there (English and the other languages), else JMdict_e (English).
    const c8* _jmdict   = fude_bake_arg(_argc, _argv, "--jmdict=",   rde_file_exists("data/raw/JMdict.xml") ? "data/raw/JMdict.xml" : "data/raw/JMdict_e.xml");
    const c8* _out      = fude_bake_arg(_argc, _argv, "--out=",      FUDE_KANJI_FILE);
    const c8* _tatoeba  = fude_bake_arg(_argc, _argv, "--tatoeba=",  "data/raw/tatoeba");

    for(u32 _i = 0; _i < 2; _i++) {
        const c8* _src = _i == 0 ? _kanjivg : _kanjidic;
        if(!rde_file_exists(_src)) {
            rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: %s not found (see COMMANDS.txt for where to get it; run from the project root)", _src);
            return 1;
        }
    }

    static const c8* const _langs[FUDE_BAKE_JA_LANGS] = { "es", "pt", "fr" };   // FUDE_BAKE_LANG_LIST's
    fude_bake _bake;
    fude_bake_init(&_bake, _langs, FUDE_BAKE_JA_LANGS);

    const f64 _t0 = rde_engine_get_time_now();
    i32       _rc = 0;

    if(!fude_bake_kanjivg(&_bake, _kanjivg)) {
        _rc = 1;
    } else {
        if(!fude_bake_kanjidic(&_bake, _kanjidic)) {
            _rc = 1;
        }

        for(u32 _level = 5; _level >= 1; _level--) {
            fude_bake_jlpt_level(&_bake, _jlpt, _level);
        }

        // After KANJIDIC2: the words are ranked by their kanji's grades.
        if(!fude_bake_jmdict(&_bake, _jmdict)) {
            _rc = 1;
        }
        // After the words: their example sentences.
        if(_rc == 0 && !fude_bake_tatoeba(&_bake, _tatoeba)) {
            _rc = 1;
        }
    }

    if(_rc == 0 && !fude_bake_write(&_bake, _out, NULL, NULL, _t0)) {
        _rc = 1;
    }
    if(_rc == 0 && !fude_bake_lookalikes(_out, &FUDE_BAKE_LOOK)) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: the look-alikes could not be added to %s", _out);
        _rc = 1;
    }
    fude_bake_free(&_bake);
    return _rc;
}

#else

b8 fude_bake_requested(i32 _argc, c8** _argv) {
    RDE_UNUSED(_argc);
    RDE_UNUSED(_argv);
    return false;
}

i32 fude_bake_run(i32 _argc, c8** _argv) {
    RDE_UNUSED(_argc);
    RDE_UNUSED(_argv);
    return 1;
}

#endif
