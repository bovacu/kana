// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"

#if !defined(RDE_PLATFORM_MOBILE)


#include "drawing/base/kfile.h"
#include "drawing/base/utf8.h"
#include "study/chars/bake.h"
#include "study/chars/kanji.h"
#include "lang/ja/romaji.h"
#include "study/chars/catalog.h"
#include "study/handwriting/match.h"

// ===========================================================================
// See lang.h (fude_bake_run). Japanese's offline data bake: KanjiVG, KANJIDIC2, the JLPT lists, JMdict and Tatoeba
// become the character data file the app ships (its format is in kanji.h). It is a DESKTOP mode of the app itself, not
// a separate tool:
//
//   <desktop build> --bake [--kanjivg=PATH] [--kanjidic=PATH] [--jlpt=DIR] [--jmdict=PATH] [--tatoeba=DIR] [--out=PATH]
//
// It is run from the project root. The defaults are data/raw/kanjivg.xml, data/raw/kanjidic2.xml, data/raw/jlpt/
// (Waller's n1..n5-kanji-char-eng.mem), data/raw/JMdict.xml when it is there and else data/raw/JMdict_e.xml,
// data/raw/tatoeba/, and the app's out (fude_bake_app), else assets/data/characters.kana (FUDE_KANJI_FILE). The sources
// are NOT in the repository (COMMANDS.txt says where to get them); the baked file is an asset. KanjiVG and KANJIDIC2
// are required. The JLPT lists are optional: without them every character reads "not listed". So are JMdict, without
// which there are no example words, and Tatoeba, without which there are no example sentences.
//
// Every character KanjiVG has strokes for is kept (kana and kanji, and some punctuation), with KANJIDIC2's readings
// and meanings where it has an entry. A KanjiVG path that cannot be parsed drops its character and is reported, so
// the file never holds half a character.
//
// This file holds Japanese's own readers (KANJIDIC2, the JLPT lists, JMdict, Tatoeba's Japanese sentences); the study's
// bake (study/chars/bake.h) does the rest: KanjiVG, the choice of words, writing the file and the look-alikes. It is
// left out of phone and tablet builds.
// ===========================================================================

// JMdict's example words: how they are scored, and how much of an entry is read.
#define FUDE_BAKE_UNCOMMON      1000    // An uncommon word's score starts here, after every common one.
#define FUDE_BAKE_WORD_MEANING  48u     // Glosses after the first are added while the meaning stays this short (bytes).
#define FUDE_BAKE_ENTRY_KEBS    8u      // Written forms read per entry; more are rare, and uncommon.
// Readings, senses, and restrictions (re_restr, stagk) read per entry, reading or sense.
#define FUDE_BAKE_ENTRY_REBS    8u
#define FUDE_BAKE_ENTRY_SENSES  4u
#define FUDE_BAKE_ENTRY_RESTR   4u

// The meanings' other languages, with the code KANJIDIC2's m_lang and the full JMdict's xml:lang give each (JMdict_e
// has English only; the full JMdict has them all). No JMdict gloss is in Portuguese, so words show in English there
// (kanji.h's fallback).
#define FUDE_BAKE_JA_LANGS 3u
static const struct { const c8* code; const c8* kanjidic; const c8* jmdict; } FUDE_BAKE_LANG_LIST[FUDE_BAKE_JA_LANGS] = {
    { "es", "es", "spa" },
    { "pt", "pt", NULL  },
    { "fr", "fr", "fre" },
};

// --- KANJIDIC2 -------------------------------------------------------------------------

// Reads KANJIDIC2 at _path into the characters KanjiVG gave strokes to: each one's grade, JLPT level (the old four),
// classical radical, frequency rank, on and kun readings (joined by 、), and meanings in English and the other
// languages. A character KanjiVG has no strokes for is skipped. Counts the characters with an entry, and those whose
// stroke count differs from KanjiVG's. Returns false when the file cannot be parsed.
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
        rde_memset(_meanings_in, 0, sizeof(_meanings_in));

        for(const rde_xml_entry* _f = _c->child; _f != NULL; _f = _f->next) {
            if(fude_xml_is(_f, "literal")) {
                u32 _len;
                _cp = fude_utf8_decode(fude_xml_text(_f), &_len);
            } else if(fude_xml_is(_f, "radical")) {
                for(const rde_xml_entry* _r = _f->child; _r != NULL; _r = _r->next) {
                    const c8* _type = fude_xml_attr(_r, "rad_type");
                    if(fude_xml_is(_r, "rad_value") && _type != NULL && rde_strcmp(_type, "classical") == 0) {
                        _radical = (u32)(i32)rde_strtol(fude_xml_text(_r), NULL, 10);
                    }
                }
            } else if(fude_xml_is(_f, "misc")) {
                for(const rde_xml_entry* _m = _f->child; _m != NULL; _m = _m->next) {
                    if(fude_xml_is(_m, "grade"))                         { _grade = (u32)(i32)rde_strtol(fude_xml_text(_m), NULL, 10); }
                    else if(fude_xml_is(_m, "stroke_count") && !_strokes) { _strokes = (u32)(i32)rde_strtol(fude_xml_text(_m), NULL, 10); }   // The first is the accepted count.
                    else if(fude_xml_is(_m, "freq"))                     { _freq = (u32)(i32)rde_strtol(fude_xml_text(_m), NULL, 10); }
                    else if(fude_xml_is(_m, "jlpt"))                     { _jlpt = (u32)(i32)rde_strtol(fude_xml_text(_m), NULL, 10); }
                }
            } else if(fude_xml_is(_f, "reading_meaning")) {
                for(const rde_xml_entry* _g = _f->child; _g != NULL; _g = _g->next) {
                    if(!fude_xml_is(_g, "rmgroup")) {
                        continue;
                    }
                    for(const rde_xml_entry* _x = _g->child; _x != NULL; _x = _x->next) {
                        if(fude_xml_is(_x, "reading")) {
                            const c8* _type = fude_xml_attr(_x, "r_type");
                            if(_type != NULL && rde_strcmp(_type, "ja_on") == 0)  { fude_bake_join(_on,  sizeof(_on),  "、", fude_xml_text(_x)); }
                            if(_type != NULL && rde_strcmp(_type, "ja_kun") == 0) { fude_bake_join(_kun, sizeof(_kun), "、", fude_xml_text(_x)); }
                        } else if(fude_xml_is(_x, "meaning") && fude_xml_attr(_x, "m_lang") == NULL) {   // English.
                            fude_bake_join(_meanings, sizeof(_meanings), ", ", fude_xml_text(_x));
                        } else if(fude_xml_is(_x, "meaning")) {
                            const c8* _lang = fude_xml_attr(_x, "m_lang");
                            for(u32 _l = 0; _l < FUDE_BAKE_JA_LANGS; _l++) {
                                if(rde_strcmp(_lang, FUDE_BAKE_LANG_LIST[_l].kanjidic) == 0) {
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
            continue;   // It has no strokes, so there is nothing to practise.
        }

        _char->grade     = (u8)(_grade   < 256u ? _grade   : 0u);
        _char->jlpt      = (u8)(_jlpt    < 256u ? _jlpt    : 0u);
        _char->radical   = (u8)(_radical < 256u ? _radical : 0u);
        _char->frequency = (u16)(_freq < 65536u ? _freq : 0u);
        _char->text      = fude_bytes_size(&_bake->text);
        fude_put_data(&_bake->text, _on,       (u32)rde_strlen(_on) + 1u);
        fude_put_data(&_bake->text, _kun,      (u32)rde_strlen(_kun) + 1u);
        fude_put_data(&_bake->text, _meanings, (u32)rde_strlen(_meanings) + 1u);
        for(u32 _l = 0; _l < FUDE_BAKE_JA_LANGS; _l++) {
            if(_meanings_in[_l][0] != 0) {
                _char->meaning_in[_l] = fude_bytes_size(&_bake->lang_text[_l]);
                fude_put_data(&_bake->lang_text[_l], _meanings_in[_l], (u32)rde_strlen(_meanings_in[_l]) + 1u);
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
// Jonathan Waller's lists (tanos.co.uk, CC BY), read from the Mnemosyne decks he publishes: a Python pickle in plain
// ASCII. Each card's question is the kanji, on the line after "S'q'", written V"\uXXXX" (or as the character itself).

// Returns the value of the _digits hex digits at _s, or 0 when one of them is not a hex digit.
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

// Reads the list of JLPT level N_level from _dir and gives that level to each character on it that has none yet.
// A missing list is logged and leaves the level unlisted; a listed character without strokes is counted as missing.
RDE_INTERNAL void fude_bake_jlpt_level(fude_bake* _bake, const c8* _dir, u32 _level) {
    c8 _path[RDE_MAX_PATH];
    rde_snprintf(_path, sizeof(_path), "%s/n%u-kanji-char-eng.mem", _dir, _level);
    if(!rde_file_exists(_path)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "bake: %s not found; N%u left unlisted", _path, _level);
        return;
    }

    u32 _size = 0;
    u8* _data = fude_file_read(_path, &_size);
    if(_data == NULL) {
        return;
    }

    const c8* _s   = (const c8*)_data;
    const c8* _end = _s + _size;
    while(_s < _end) {
        const c8* _nl   = rde_memchr(_s, '\n', (usize)(_end - _s));
        const c8* _next = _nl != NULL ? _nl + 1 : _end;

        // When "S'q'" or "sS'q'" is alone on its line, the next line is the question.
        const usize _len  = (usize)((_nl != NULL ? _nl : _end) - _s);
        const b8    _is_q = (_len == 4 && rde_strncmp(_s, "S'q'", 4) == 0) || (_len == 5 && rde_strncmp(_s, "sS'q'", 5) == 0);
        if(_is_q && _next < _end && *_next == 'V') {
            const c8* _v = _next + 1;
            if(*_v == '"') {
                _v++;
            }

            u32 _cp = 0;
            if(_v[0] == '\\' && _v[1] == 'u')      { _cp = fude_bake_hex(_v + 2, 4); }
            else if(_v[0] == '\\' && _v[1] == 'U') { _cp = fude_bake_hex(_v + 2, 8); }
            else                                   { u32 _n; _cp = fude_utf8_decode(_v, &_n); }

            fude_bake_char* _char = _cp != 0 ? fude_bake_find(_bake, _cp) : NULL;
            if(_char == NULL) {
                _bake->level_missing++;
            } else if(_char->level == 0) {   // The lists are disjoint; if not, the first read (the easier) wins.
                _char->level = (u8)_level;
                _bake->level_listed[_level]++;
            }
        }
        _s = _next;
    }

    fude_file_free(_data);
}

// --- JMdict: example words -------------------------------------------------------------
//
// EDRDG's JMdict (CC BY-SA 4.0, like KANJIDIC2). It is about 60 MB of XML with one element a line, so it is read a
// line at a time instead of as a tree. Only COMMON written forms are kept (those with a priority mark from the
// newspaper lists, Ichimango or EDRDG's own: news1, ichi1, spec1, spec2), up to FUDE_BAKE_WORD_CHARS characters, whose
// kanji all have strokes. Each kanji then keeps its best few: the most frequent first, the ones whose other kanji are
// no harder than it, not ones usually written in kana, and not a longer form of one already kept (日本人 after 日本).

// One JMdict entry as it is read, line by line: its written forms (k_ele), readings (r_ele) and senses, up to the
// FUDE_BAKE_ENTRY_ limits; anything past them is not kept.
typedef struct {
    c8  keb[FUDE_BAKE_ENTRY_KEBS][4u * FUDE_BAKE_WORD_CHARS + 1u];
    b8  keb_long[FUDE_BAKE_ENTRY_KEBS];     // Too long to keep.
    b8  keb_bad[FUDE_BAKE_ENTRY_KEBS];      // Irregular, outdated, rare or search-only.
    u32 keb_common[FUDE_BAKE_ENTRY_KEBS];   // How many of the common lists have it.
    u32 keb_nf[FUDE_BAKE_ENTRY_KEBS];       // Its newspaper frequency band, 1..48 (0 for none).
    b8  keb_ichi[FUDE_BAKE_ENTRY_KEBS];     // On Ichimango's list of everyday words.
    u32 kebs;

    c8  reb[FUDE_BAKE_ENTRY_REBS][64];
    b8  reb_bad[FUDE_BAKE_ENTRY_REBS];      // Not for the kanji forms, irregular, outdated or search-only.
    b8  reb_common[FUDE_BAKE_ENTRY_REBS];
    // The written forms a reading is restricted to (re_restr); none means it goes with every form.
    c8  reb_restr[FUDE_BAKE_ENTRY_REBS][FUDE_BAKE_ENTRY_RESTR][4u * FUDE_BAKE_WORD_CHARS + 1u];   // Forms it is for.
    u32 reb_restrs[FUDE_BAKE_ENTRY_REBS];
    u32 rebs;

    c8  sense[FUDE_BAKE_ENTRY_SENSES][128];
    b8  sense_kana[FUDE_BAKE_ENTRY_SENSES];  // Marked "usually written in kana".
    // The written forms a sense is restricted to (stagk); none means every form.
    c8  sense_stagk[FUDE_BAKE_ENTRY_SENSES][FUDE_BAKE_ENTRY_RESTR][4u * FUDE_BAKE_WORD_CHARS + 1u];
    u32 sense_stagks[FUDE_BAKE_ENTRY_SENSES];
    u32 senses;

    u32 element;       // What is open: 0 nothing, 1 k_ele, 2 r_ele, 3 sense.
    b8  sense_full;    // The open sense's meaning is as long as it gets; its other glosses are ignored.
    // In the full JMdict, a sense whose glosses are in another language (xml:lang) is not an English one; the entry's
    // first sense in each language is its meaning in that language.
    b8  sense_foreign;
    i32 sense_lang;    // Its FUDE_BAKE_LANG_LIST index, or -1 for a language not kept.
    c8  foreign[FUDE_BAKE_LANGS][128];
    b8  foreign_full[FUDE_BAKE_LANGS];   // That language's first sense is all in.
} fude_bake_entry;

// Writes into _out the text of "<tag ...>text</tag>" on _line, with XML's entities decoded and at most 511 bytes of
// it. Returns false when the line is not that element.
RDE_INTERNAL b8 fude_bake_element(const c8* _line, const c8* _tag, c8* _out, usize _size) {
    const usize _n = rde_strlen(_tag);
    if(_line[0] != '<' || rde_strncmp(_line + 1, _tag, _n) != 0 || (_line[_n + 1] != '>' && _line[_n + 1] != ' ')) {
        return false;
    }
    const c8* _start = rde_strchr(_line, '>');
    const c8* _end   = _start != NULL ? rde_strstr(_start, "</") : NULL;
    if(_end == NULL) {
        return false;
    }
    // A longer text is cut before a whole character (utf8.h), so what is kept is whole UTF-8.
    c8 _raw[512];
    fude_utf8_copy(_raw, sizeof(_raw), _start + 1, (usize)(_end - _start - 1));
    _out[0] = 0;
    fude_bake_join(_out, _size, "", _raw);
    return true;
}

// Returns whether _written can be an example word: at most FUDE_BAKE_WORD_CHARS characters, every one a kanji with
// strokes, a kana, ー or 々, with at least one kanji. Sets *_chars to its length in characters.
RDE_INTERNAL b8 fude_bake_word_usable(fude_bake* _bake, const c8* _written, u32* _chars) {
    b8  _kanji = false;
    u32 _n     = 0;
    for(const c8* _p = _written; *_p != 0; _n++) {
        u32       _len = 0;
        const u32 _cp  = fude_utf8_decode(_p, &_len);
        if(_cp == 0) {
            return false;
        }
        _p += _len;
        if(fude_utf8_is_han(_cp)) {
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

// Returns whether _written is a kanji's own word: one kanji, at the start, and only kana after it (見る, 読み; also 月
// alone).
RDE_INTERNAL b8 fude_bake_own_word(const c8* _written) {
    u32 _kanji = 0;
    u32 _i     = 0;
    for(const c8* _p = _written; *_p != 0; _i++) {
        u32       _len = 0;
        const u32 _cp  = fude_utf8_decode(_p, &_len);
        if(_len == 0) {
            break;
        }
        _p += _len;
        if(fude_utf8_is_han(_cp)) {
            if(_i != 0) {
                return false;
            }
            _kanji++;
        }
    }
    return _kanji == 1u;
}

// Writes kana as plain romaji into _out (_size bytes), with the long vowels folded (とうきょう is tokyo). Katakana is
// read as hiragana; っ doubles the next consonant, ー is dropped, and anything without romaji is skipped.
RDE_INTERNAL void fude_bake_romanize(const c8* _kana, c8* _out, usize _size) {
    usize _n      = 0;
    b8    _double = false;   // Set after っ.
    _out[0] = 0;
    for(const c8* _p = _kana; *_p != 0;) {
        u32       _len = 0;
        const u32 _cp  = fude_utf8_decode(_p, &_len);
        if(_len == 0) {
            break;
        }
        _p += _len;
        const u32 _h = (_cp >= 0x30A1u && _cp <= 0x30F6u) ? _cp - 0x60u : _cp;   // Katakana is read as hiragana.
        if(_h == 0x3063u) { _double = true; continue; }                           // っ doubles what follows.
        if(_h == 0x30FCu) { continue; }                                            // ー is dropped.
        const c8* _r = fude_romaji(_h);
        if(_r == NULL) {
            continue;
        }
        // The kana's romaji without the parentheses the chart puts around a small kana's.
        c8 _syllable[8];
        usize _k = 0;
        for(const c8* _q = _r; *_q != 0 && _k + 1u < sizeof(_syllable); _q++) {
            if(*_q != '(' && *_q != ')') {
                _syllable[_k++] = *_q;
            }
        }
        _syllable[_k] = 0;
        // ゃ ゅ ょ fold into the kana before, replacing its i: き + ゃ is kya, and after sh, ch or j only the vowel is
        // added, so し + ゃ is sha.
        if((_h == 0x3083u || _h == 0x3085u || _h == 0x3087u) && _n > 0 && _out[_n - 1] == 'i') {
            const b8 _palatal = _n >= 2 && (_out[_n - 2] == 'h' || _out[_n - 2] == 'j');
            _n--;
            if(_palatal) {
                rde_snprintf(_syllable, sizeof(_syllable), "%c", _h == 0x3083u ? 'a' : _h == 0x3085u ? 'u' : 'o');
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
    // Fold the long vowels: ou and oo become o; uu, aa and ii become one letter.
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

// Returns whether a word is a name rather than a word: its meaning starts with a capitalised word of at least three
// letters that is its reading romanised, long vowels folded the same way (山形 "Yamagata (city, prefecture)", 読売
// "Yomiuri (newspaper…)"). ō and ū in the meaning count as o and u.
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
    c8 _folded[64];   // The meaning's first word with its long vowels folded the same way.
    usize _w = 0;
    for(usize _i = 0; _i < _n; _i++) {
        if(_w > 0 && ((_folded[_w - 1] == 'o' && (_word[_i] == 'u' || _word[_i] == 'o')) || (_folded[_w - 1] == _word[_i] && (_word[_i] == 'u' || _word[_i] == 'a' || _word[_i] == 'i')))) {
            continue;
        }
        _folded[_w++] = _word[_i];
    }
    _folded[_w] = 0;
    return _n >= 3u && rde_strcmp(_folded, _romaji) == 0;
}

// Returns whether _s is one of the _count written forms in _list.
RDE_INTERNAL b8 fude_bake_listed(const c8 _list[][4u * FUDE_BAKE_WORD_CHARS + 1u], u32 _count, const c8* _s) {
    for(u32 _i = 0; _i < _count; _i++) {
        if(rde_strcmp(_list[_i], _s) == 0) {
            return true;
        }
    }
    return false;
}

// Called when a whole entry has been read: each usable written form becomes a word, with its reading and meaning,
// and a candidate (fude_bake_word_ref) for each of its kanji, scored so that a lower score is a better example.
RDE_INTERNAL void fude_bake_entry_done(fude_bake* _bake, const fude_bake_entry* _e) {
    _bake->word_entries++;
    for(u32 _k = 0; _k < _e->kebs; _k++) {
        u32 _chars = 0;
        if(_e->keb_bad[_k] || _e->keb_long[_k] || !fude_bake_word_usable(_bake, _e->keb[_k], &_chars)) {
            continue;
        }

        // Its reading: the first that goes with this form, or the first common one if there is one.
        i32 _reading = -1;
        for(u32 _r = 0; _r < _e->rebs; _r++) {
            if(_e->reb_bad[_r] || (_e->reb_restrs[_r] > 0 && !fude_bake_listed(_e->reb_restr[_r], _e->reb_restrs[_r], _e->keb[_k]))) {
                continue;
            }
            if(_reading < 0 || (_e->reb_common[_r] && !_e->reb_common[_reading])) {
                _reading = (i32)_r;
            }
        }
        // Its meaning: the first sense that is not restricted to other forms.
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
        // How common the word is, plainly, without the preferences of an example's score below: the newspaper band,
        // or else about where the other lists sit; the more lists, the more common.
        {
            i32 _freq = _word.common ? (_e->keb_nf[_k] != 0 ? (i32)_e->keb_nf[_k] : _e->keb_ichi[_k] ? 18 : 26) - 3 * ((i32)_e->keb_common[_k] - 1)
                                     : 1000 + (_e->keb_nf[_k] != 0 ? (i32)_e->keb_nf[_k] : 60);
            _freq     += _e->reb_common[_reading] ? 0 : 2;
            // A word usually written another way comes a little later (本 for 元 もと; not 七月 for ７月).
            _freq     += _e->keb_common[0] > _e->keb_common[_k] ? 1 : 0;   // Usually written otherwise.
            _word.freq = (u16)(_freq < 0 ? 0 : _freq > 0xFFFE ? 0xFFFE : _freq);
        }
        // A text that is too long is cut before a whole character (utf8.h), never in the middle of one.
        fude_utf8_set(_word.written, sizeof(_word.written), _e->keb[_k]);
        fude_utf8_set(_word.reading, sizeof(_word.reading), _e->reb[_reading]);
        fude_utf8_set(_word.meaning, sizeof(_word.meaning), _e->sense[_sense]);
        for(u32 _l = 0; _l < FUDE_BAKE_JA_LANGS; _l++) {
            fude_utf8_set(_word.meaning_in[_l], sizeof(_word.meaning_in[_l]), _e->foreign[_l]);
        }
        const u32 _index = (u32)rde_arr_length(&_bake->words);
        rde_arr_add(&_bake->words, &_word);

        // The example's base score, lower being better. For a common word, the newspaper band when it has one, or
        // else about where the other lists sit, because everyday words (上 うえ) are rarer in the news than their
        // fragments (上げ); a word on more lists is more certainly common. An uncommon word comes after all of those
        // (the viewer's Add offers them).
        i32 _base;
        if(_word.common) {
            _base = _e->keb_nf[_k] != 0 ? (i32)_e->keb_nf[_k] : _e->keb_ichi[_k] ? 18 : 26;
            _base -= 3 * ((i32)_e->keb_common[_k] - 1);
            // The kanji on its own (月 つき, 上 うえ) comes first, whatever the news says, because it gives the kanji's
            // own reading and meaning; of those, the everyday one first (下 した over 下 もと, which the news
            // prefers). A kanji with kana after it (見る) is a little ahead too.
            _base -= _chars == 1u ? (_e->keb_ichi[_k] ? 120 : 100) : fude_bake_own_word(_word.written) ? 6 : 0;
        } else {
            _base = FUDE_BAKE_UNCOMMON + (_e->keb_nf[_k] != 0 ? (i32)_e->keb_nf[_k] : 60);
        }
        _base += 3 * ((i32)_chars > 2 ? (i32)_chars - 2 : 0);   // Short words first.
        _base += _e->sense_kana[_sense] ? 20 : 0;                 // Usually kana: a poor example of its kanji.
        _base += fude_bake_is_name(_word.reading, _word.meaning) ? 20 : 0;
        _base += _e->reb_common[_reading] ? 0 : 2;                // 下 した (a common reading) before 下 もと.
        _base += _e->keb_common[0] > _e->keb_common[_k] ? 1 : 0;  // 本 ほん before 本 もと, whose usual form is 元.

        // A candidate for each of its kanji, once each, scored worse the harder its other kanji are than that one.
        u32 _seen[FUDE_BAKE_WORD_CHARS];
        u32 _seen_count = 0;
        for(const c8* _p = _word.written; *_p != 0;) {
            u32       _len = 0;
            const u32 _cp  = fude_utf8_decode(_p, &_len);
            _p += _len;
            b8 _dup = !fude_utf8_is_han(_cp);
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
                const u32 _o  = fude_utf8_decode(_q, &_l2);
                _q += _l2;
                if(_o == _cp || !fude_utf8_is_han(_o)) {
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

// Reads JMdict at _path a line at a time, building each entry (fude_bake_entry) and handing it to
// fude_bake_entry_done at its end, then lets the study's bake choose each kanji's words. Lines longer than 1023 bytes
// are cut. A missing file is logged and gives no example words; returns false only when the file cannot be read.
RDE_INTERNAL b8 fude_bake_jmdict(fude_bake* _bake, const c8* _path) {
    if(!rde_file_exists(_path)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "bake: %s not found; no example words", _path);
        return true;
    }
    const f64 _t0   = rde_engine_get_time_now();
    u32       _size = 0;
    u8*       _data = fude_file_read(_path, &_size);
    if(_data == NULL) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: could not read %s", _path);
        return false;
    }

    static fude_bake_entry _e;   // Big, so static; it holds one entry at a time.
    rde_memset(&_e, 0, sizeof(_e));

    c8        _line[1024];
    c8        _text[512];
    const c8* _s   = (const c8*)_data;
    const c8* _end = _s + _size;
    while(_s < _end) {
        const c8*   _nl   = rde_memchr(_s, '\n', (usize)(_end - _s));
        const usize _len  = (usize)((_nl != NULL ? _nl : _end) - _s);
        const usize _copy = _len < sizeof(_line) - 1u ? _len : sizeof(_line) - 1u;
        rde_memcpy(_line, _s, _copy);
        _line[_copy] = 0;
        _s = _nl != NULL ? _nl + 1 : _end;

        if(rde_strcmp(_line, "<entry>") == 0) {
            rde_memset(&_e, 0, sizeof(_e));
        } else if(rde_strcmp(_line, "</entry>") == 0) {
            fude_bake_entry_done(_bake, &_e);
        } else if(rde_strcmp(_line, "<k_ele>") == 0) {
            _e.element = _e.kebs < FUDE_BAKE_ENTRY_KEBS ? 1u : 0u;
        } else if(rde_strcmp(_line, "<r_ele>") == 0) {
            _e.element = _e.rebs < FUDE_BAKE_ENTRY_REBS ? 2u : 0u;
        } else if(rde_strcmp(_line, "<sense>") == 0) {
            _e.element       = 3u;   // Read even past the kept English senses, since another language's may follow.
            _e.sense_full    = false;
            _e.sense_foreign = false;
            _e.sense_lang    = -1;
        } else if(rde_strcmp(_line, "</k_ele>") == 0) {
            _e.kebs += _e.element == 1u ? 1u : 0u;
            _e.element = 0;
        } else if(rde_strcmp(_line, "</r_ele>") == 0) {
            _e.rebs += _e.element == 2u ? 1u : 0u;
            _e.element = 0;
        } else if(rde_strcmp(_line, "</sense>") == 0) {
            if(_e.element == 3u && _e.sense_foreign) {
                if(_e.sense_lang >= 0 && _e.foreign[_e.sense_lang][0] != 0) {
                    _e.foreign_full[_e.sense_lang] = true;
                }
                if(_e.senses < FUDE_BAKE_ENTRY_SENSES) {   // Not an English sense, so its slot is free again.
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
                _e.keb_long[_k] = rde_strlen(_text) >= sizeof(_e.keb[_k]);
                rde_snprintf(_e.keb[_k], sizeof(_e.keb[_k]), "%s", _e.keb_long[_k] ? "" : _text);
            } else if(fude_bake_element(_line, "ke_inf", _text, sizeof(_text))) {
                _e.keb_bad[_k] = _e.keb_bad[_k] || rde_strcmp(_text, "&iK;") == 0 || rde_strcmp(_text, "&oK;") == 0 ||
                                 rde_strcmp(_text, "&rK;") == 0 || rde_strcmp(_text, "&sK;") == 0 || rde_strcmp(_text, "&io;") == 0;
            } else if(fude_bake_element(_line, "ke_pri", _text, sizeof(_text))) {
                if(rde_strcmp(_text, "news1") == 0 || rde_strcmp(_text, "ichi1") == 0 || rde_strcmp(_text, "spec1") == 0 || rde_strcmp(_text, "spec2") == 0) {
                    _e.keb_common[_k]++;
                    _e.keb_ichi[_k] = _e.keb_ichi[_k] || rde_strcmp(_text, "ichi1") == 0;
                } else if(rde_strncmp(_text, "nf", 2) == 0) {
                    _e.keb_nf[_k] = (u32)(i32)rde_strtol(_text + 2, NULL, 10);
                }
            }
        } else if(_e.element == 2u) {
            const u32 _r = _e.rebs;
            if(fude_bake_element(_line, "reb", _text, sizeof(_text))) {
                fude_utf8_set(_e.reb[_r], sizeof(_e.reb[_r]), _text);   // cut, if at all, before a whole character
            } else if(rde_strncmp(_line, "<re_nokanji", 11) == 0) {
                _e.reb_bad[_r] = true;
            } else if(fude_bake_element(_line, "re_inf", _text, sizeof(_text))) {
                _e.reb_bad[_r] = _e.reb_bad[_r] || rde_strcmp(_text, "&ik;") == 0 || rde_strcmp(_text, "&ok;") == 0 || rde_strcmp(_text, "&sk;") == 0;
            } else if(fude_bake_element(_line, "re_pri", _text, sizeof(_text))) {
                _e.reb_common[_r] = _e.reb_common[_r] || rde_strcmp(_text, "news1") == 0 || rde_strcmp(_text, "ichi1") == 0 ||
                                    rde_strcmp(_text, "spec1") == 0 || rde_strcmp(_text, "spec2") == 0;
            } else if(fude_bake_element(_line, "re_restr", _text, sizeof(_text))) {
                if(_e.reb_restrs[_r] < FUDE_BAKE_ENTRY_RESTR && rde_strlen(_text) < sizeof(_e.reb_restr[_r][0])) {
                    rde_snprintf(_e.reb_restr[_r][_e.reb_restrs[_r]++], sizeof(_e.reb_restr[_r][0]), "%s", _text);
                }
            }
        } else if(_e.element == 3u && rde_strncmp(_line, "<gloss xml:lang=\"", 17) == 0 && rde_strncmp(_line + 17, "eng", 3) != 0) {
            // A gloss in another language makes this sense that language's. Its first sense in a kept language is
            // that language's meaning, with more glosses added while it stays short.
            _e.sense_foreign = true;
            _e.sense_lang    = -1;
            for(u32 _l = 0; _l < FUDE_BAKE_JA_LANGS; _l++) {
                if(FUDE_BAKE_LANG_LIST[_l].jmdict != NULL && rde_strncmp(_line + 17, FUDE_BAKE_LANG_LIST[_l].jmdict, 3) == 0) {
                    _e.sense_lang = (i32)_l;
                }
            }
            if(_e.sense_lang >= 0 && !_e.foreign_full[_e.sense_lang] && fude_bake_element(_line, "gloss", _text, sizeof(_text))) {
                c8*         _to   = _e.foreign[_e.sense_lang];
                const usize _have = rde_strlen(_to);
                if(_have == 0) {
                    fude_utf8_set(_to, sizeof(_e.foreign[0]), _text);
                } else if(_have + 2u + rde_strlen(_text) <= FUDE_BAKE_WORD_MEANING) {
                    fude_bake_join(_to, sizeof(_e.foreign[0]), "; ", _text);
                }
            }
        } else if(_e.element == 3u && _e.senses < FUDE_BAKE_ENTRY_SENSES) {
            const u32 _n = _e.senses;
            if(fude_bake_element(_line, "gloss", _text, sizeof(_text))) {
                // The first gloss always, and more while the meaning stays FUDE_BAKE_WORD_MEANING bytes or less.
                const usize _have = rde_strlen(_e.sense[_n]);
                if(_have == 0) {
                    fude_utf8_set(_e.sense[_n], sizeof(_e.sense[_n]), _text);
                } else if(!_e.sense_full && _have + 2u + rde_strlen(_text) <= FUDE_BAKE_WORD_MEANING) {
                    fude_bake_join(_e.sense[_n], sizeof(_e.sense[_n]), "; ", _text);
                } else {
                    _e.sense_full = true;
                }
            } else if(fude_bake_element(_line, "misc", _text, sizeof(_text))) {
                _e.sense_kana[_n] = _e.sense_kana[_n] || rde_strcmp(_text, "&uk;") == 0;
            } else if(fude_bake_element(_line, "stagk", _text, sizeof(_text))) {
                if(_e.sense_stagks[_n] < FUDE_BAKE_ENTRY_RESTR && rde_strlen(_text) < sizeof(_e.sense_stagk[_n][0])) {
                    rde_snprintf(_e.sense_stagk[_n][_e.sense_stagks[_n]++], sizeof(_e.sense_stagk[_n][0]), "%s", _text);
                }
            }
        }
    }

    fude_file_free(_data);
    rde_log_level(RDE_LOG_LEVEL_INFO, "bake: read %s in %.2f s", _path, rde_engine_get_time_now() - _t0);
    fude_bake_choose_words(_bake);
    return true;
}

// --- Tatoeba: example sentences -----------------------------------------------------------
// Japanese sentences from Tatoeba (CC BY 2.0 FR) with their translations: English (required), and Spanish, French and
// Portuguese where there are. Each word kept gets the best sentence that has it. Tatoeba's word index
// (jpn_indices.csv: each sentence's words as dictionary forms, with the reading where it is not the usual one) says
// which word a sentence has, since 行 read ぎょう is not the 行 of 行って: the word with that form and reading, or the
// commonest with that form, with its kanji all in the sentence as written. A sentence that is not indexed is searched
// as text, for a word as written or a verb's or adjective's stem followed by its conjugation (食べ + ました), but never
// for one kanji alone (whose reading the text does not say), and it loses to any indexed one. The best sentence has a
// comfortable length (about 14 characters, between FUDE_BAKE_SENT_MIN and FUDE_BAKE_SENT_MAX), then is translated into
// more of the languages; one the index marks as a good example (~) comes first. The file's format is kanji.h's
// 'SENT'. Scores are lower for better sentences.

// A sentence's length in characters, at least, at most and ideally.
#define FUDE_BAKE_SENT_MIN   6u
#define FUDE_BAKE_SENT_MAX   40u
#define FUDE_BAKE_SENT_IDEAL 14
#define FUDE_BAKE_SENT_LANGS 4u     // English, Spanish, French, Portuguese: their order in the file.
#define FUDE_BAKE_SENT_GOOD  10     // Taken off a score when the index says the sentence is a good example...
#define FUDE_BAKE_SENT_TEXT  40     // ...and added when the word was found as text, not in the index.
// Tatoeba's three-letter code for each language, in the order above; it names the links' and the sentences' files.
static const c8* const FUDE_BAKE_SENT_CODES[FUDE_BAKE_SENT_LANGS] = { "eng", "spa", "fra", "por" };

typedef struct { u32 id; u32 at; u32 len; } fude_bake_sentence;    // Its id, and where its text is in its file.
typedef struct { u32 from; u32 to; } fude_bake_link;               // A Japanese sentence's id, and a translation's id.

// The order for rde_sort and rde_bsearch of sentences: by id.
RDE_INTERNAL int fude_bake_by_sentence_id(const any _a, const any _b) {
    const u32 _x = ((const fude_bake_sentence*)_a)->id, _y = ((const fude_bake_sentence*)_b)->id;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// The order for rde_sort of links: by the Japanese sentence's id, then by the translation's.
RDE_INTERNAL int fude_bake_by_link(const any _a, const any _b) {
    const fude_bake_link* _x = (const fude_bake_link*)_a;
    const fude_bake_link* _y = (const fude_bake_link*)_b;
    if(_x->from != _y->from) {
        return _x->from < _y->from ? -1 : 1;
    }
    return _x->to < _y->to ? -1 : (_x->to > _y->to ? 1 : 0);
}

// The order for rde_sort and rde_bsearch of u32 values.
RDE_INTERNAL int fude_bake_by_u32(const any _a, const any _b) {
    const u32 _x = *(const u32*)_a, _y = *(const u32*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// The words that fude_bake_by_written sorts by: rde_sort passes no context, so they are set here before sorting.
static const fude_bake_word* fude_bake_sort_words;
// The order for rde_sort of word numbers by their words' written forms, so that the index's words can be found by
// bisection and words written alike sit together.
RDE_INTERNAL int fude_bake_by_written(const any _a, const any _b) {
    return rde_strcmp(fude_bake_sort_words[*(const u32*)_a].written, fude_bake_sort_words[*(const u32*)_b].written);
}

// Reads Tatoeba's index line for sentence _i ("word(reading)[sense]{as written}~ ..."): each kept word it names gets
// sentence _i as its best when _score (less FUDE_BAKE_SENT_GOOD when marked ~) beats its best so far. _sorted holds
// the kept words' numbers sorted by written form. A word whose kanji are not all in the sentence as written, or whose
// form names two kept words equally common and no reading, is passed over.
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
        // The dictionary form, then its marks: (reading), [sense], {as written} and ~.
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
            rde_memcpy(_written, &_line[_p], _h - _p);
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
                rde_memcpy(_reading, &_line[_m + 1u], _n);
                _reading[_n] = 0;
            } else if(_open == '{' && _n < sizeof(_as)) {
                rde_memcpy(_as, &_line[_m + 1u], _n);
                _as[_n] = 0;
            }
            _m = _q + 1u;
        }
        _p = _end;
        if(!_fits) {
            continue;
        }
        // Its kanji must all be in the sentence as written (直ぐに written すぐに has none of them).
        const c8* _shown = _as[0] != 0 ? _as : _written;
        b8        _all   = true;
        for(const c8* _c = _written; *_c != 0 && _all;) {
            u32       _len = 0;
            const u32 _cp  = fude_utf8_decode(_c, &_len);
            if(fude_utf8_is_han(_cp)) {
                c8 _one[8] = { 0 };
                rde_memcpy(_one, _c, _len < 7u ? _len : 7u);
                _all = rde_strstr(_shown, _one) != NULL;
            }
            _c += _len > 0 ? _len : 1u;
        }
        if(!_all) {
            continue;
        }
        // The word: the one with that reading when one is given, or else the commonest of that form; none when two
        // are as common, since the index does not say which it is.
        u32 _lo = 0, _hi = _sorted_count;
        while(_lo < _hi) {
            const u32 _mid = (_lo + _hi) / 2u;
            if(rde_strcmp(_words[_sorted[_mid]].written, _written) < 0) {
                _lo = _mid + 1u;
            } else {
                _hi = _mid;
            }
        }
        u32 _word = UINT32_MAX;
        b8  _tied = false;
        for(u32 _k = _lo; _k < _sorted_count && rde_strcmp(_words[_sorted[_k]].written, _written) == 0; _k++) {
            const fude_bake_word* _w = &_words[_sorted[_k]];
            if(_reading[0] != 0) {
                _word = rde_strcmp(_w->reading, _reading) == 0 ? _sorted[_k] : _word;
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

// Reads a Tatoeba export's lines, "id <tab> lang <tab> text", into _out as fude_bake_sentence, sorted by id. When
// _wanted (sorted) is given, only the sentences whose ids are in it are kept. A line's \r is not part of its text.
RDE_INTERNAL void fude_bake_read_sentences(const u8* _data, u32 _size, const u32* _wanted, u32 _wanted_count, rde_arr* _out) {
    u32 _at = 0;
    while(_at < _size) {
        u32 _end = _at;
        while(_end < _size && _data[_end] != '\n') {
            _end++;
        }
        // The id.
        u32 _id = 0;
        u32 _p  = _at;
        while(_p < _end && _data[_p] >= '0' && _data[_p] <= '9') {
            _id = _id * 10u + (u32)(_data[_p++] - '0');
        }
        // The language, skipped, then the text.
        if(_p < _end && _data[_p] == '\t') {
            _p++;
            while(_p < _end && _data[_p] != '\t') {
                _p++;
            }
            if(_p < _end && _data[_p] == '\t' && (_wanted == NULL || rde_bsearch(&_id, _wanted, _wanted_count, sizeof(u32), fude_bake_by_u32) != NULL)) {
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
    rde_sort(_out->memory, rde_arr_length(_out), sizeof(fude_bake_sentence), fude_bake_by_sentence_id);
}

// Reads a links file's "from <tab> to" lines into _out as fude_bake_link, sorted.
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
    rde_sort(_out->memory, rde_arr_length(_out), sizeof(fude_bake_link), fude_bake_by_link);
}

// Returns the id of the first translation of Japanese sentence _from among _links (sorted), or 0 when it has none.
RDE_INTERNAL u32 fude_bake_link_of(const rde_arr* _links, u32 _from) {
    const fude_bake_link* _l  = (const fude_bake_link*)_links->memory;
    u32                   _lo = 0, _hi = (u32)rde_arr_length(_links);
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(_l[_mid].from < _from) { _lo = _mid + 1u; } else { _hi = _mid; }
    }
    return _lo < (u32)rde_arr_length(_links) && _l[_lo].from == _from ? _l[_lo].to : 0u;
}

// Returns sentence _id among _sentences (sorted by id), or NULL.
RDE_INTERNAL const fude_bake_sentence* fude_bake_sentence_of(const rde_arr* _sentences, u32 _id) {
    const fude_bake_sentence _key = { _id, 0, 0 };
    return (const fude_bake_sentence*)rde_bsearch(&_key, _sentences->memory, rde_arr_length(_sentences), sizeof(fude_bake_sentence), fude_bake_by_sentence_id);
}

// A key of the map from the kept words' written forms, and the stems of those that conjugate, to the words' numbers.
// The key's text points into the word's own written form and is not NUL-terminated, because a stem is a start of it.
typedef struct {
    const c8* text;
    u32       bytes;
    u8        stem;   // 1 for the stem (the form without its last kana), 0 for the whole form.
} fude_bake_word_key;

// The map's hash of a key. The map hands keys over unaligned (rde.h: rde_hash_map_hash_fn), so they are copied out
// to be read.
RDE_INTERNAL u64 fude_bake_word_key_hash(const any _key) {
    fude_bake_word_key _k;
    rde_memcpy(&_k, _key, sizeof(_k));
    return rde_hash_combine_64(rde_hash_64((any)_k.text, _k.bytes), (u64)_k.stem);
}

// The map's comparison of two keys: by length, then whether a stem, then the bytes. Copied out for the same reason.
RDE_INTERNAL i32 fude_bake_word_key_cmp(const any _a, const any _b) {
    fude_bake_word_key _ka, _kb;
    rde_memcpy(&_ka, _a, sizeof(_ka));
    rde_memcpy(&_kb, _b, sizeof(_kb));
    if(_ka.bytes != _kb.bytes || _ka.stem != _kb.stem) {
        return _ka.bytes != _kb.bytes ? (_ka.bytes < _kb.bytes ? -1 : 1) : (_ka.stem < _kb.stem ? -1 : 1);
    }
    return rde_memcmp(_ka.text, _kb.text, _ka.bytes);
}

// Tatoeba's files as fude_bake_tatoeba reads them, shared by its steps. Each file's bytes are kept whole, because its
// sentences (fude_bake_sentence) say where their text is in them. fude_bake_tatoeba frees it all at its end.
typedef struct {
    u8*     jdata;                         // jpn_sentences.tsv
    rde_arr japanese;                      // its sentences, sorted by id
    u8*     idata;                         // jpn_indices.csv, or NULL when there is none
    rde_arr index;                         // its lines, read as sentences, so their text is the words; sorted by id
    rde_arr links[FUDE_BAKE_SENT_LANGS];   // each language's links from the Japanese sentences (fude_bake_link), sorted
    rde_arr texts[FUDE_BAKE_SENT_LANGS];   // each language's sentences that are linked to, sorted by id
    u8*     tdata[FUDE_BAKE_SENT_LANGS];   // each language's <code>_sentences.tsv, or NULL when there is none
} fude_bake_tatoeba_files;

// Reads Tatoeba's word index, jpn_indices.csv in _dir, into _files (idata and index), on _heap. A missing index is
// logged, and leaves no lines.
RDE_INTERNAL void fude_bake_tatoeba_read_index(fude_bake_tatoeba_files* _files, const c8* _dir, rde_memory_allocator* _heap) {
    c8 _path[RDE_MAX_PATH];
    rde_snprintf(_path, sizeof(_path), "%s/jpn_indices.csv", _dir);
    u32     _isize = 0;
    _files->idata  = rde_file_exists(_path) ? fude_file_read(_path, &_isize) : NULL;
    _files->index  = rde_arr_new(sizeof(fude_bake_sentence), _heap);
    if(_files->idata != NULL) {
        fude_bake_read_sentences(_files->idata, _isize, NULL, 0, &_files->index);
    } else {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "bake: %s not found; sentences found as text only", _path);
    }
}

// Reads each language's links from the Japanese sentences (jpn-<code>_links.tsv in _dir) and the sentences they link
// to (<code>_sentences.tsv) into _files (links, texts and tdata), on _heap. A missing file leaves that language with
// no links, or no sentences.
RDE_INTERNAL void fude_bake_tatoeba_read_translations(fude_bake_tatoeba_files* _files, const c8* _dir, rde_memory_allocator* _heap) {
    c8 _path[RDE_MAX_PATH];
    for(u32 _l = 0; _l < FUDE_BAKE_SENT_LANGS; _l++) {
        _files->links[_l] = rde_arr_new(sizeof(fude_bake_link), _heap);
        _files->texts[_l] = rde_arr_new(sizeof(fude_bake_sentence), _heap);
        _files->tdata[_l] = NULL;
        rde_snprintf(_path, sizeof(_path), "%s/jpn-%s_links.tsv", _dir, FUDE_BAKE_SENT_CODES[_l]);
        u32 _size = 0;
        u8* _data = rde_file_exists(_path) ? fude_file_read(_path, &_size) : NULL;
        if(_data != NULL) {
            fude_bake_read_links(_data, _size, &_files->links[_l]);
            fude_file_free(_data);
        }
        // Read only the sentences linked to: their ids first, then their texts.
        const u32         _n           = (u32)rde_arr_length(&_files->links[_l]);
        rde_arr TYPE(u32) _wanted_list = rde_arr_new(sizeof(u32), _heap);
        rde_arr_resize(&_wanted_list, _n);
        u32*              _wanted      = (u32*)_wanted_list.memory;
        for(u32 _i = 0; _i < _n; _i++) {
            _wanted[_i] = ((const fude_bake_link*)_files->links[_l].memory)[_i].to;
        }
        rde_sort(_wanted, _n, sizeof(u32), fude_bake_by_u32);
        rde_snprintf(_path, sizeof(_path), "%s/%s_sentences.tsv", _dir, FUDE_BAKE_SENT_CODES[_l]);
        u32 _size2 = 0;
        _files->tdata[_l] = rde_file_exists(_path) ? fude_file_read(_path, &_size2) : NULL;
        if(_files->tdata[_l] != NULL) {
            fude_bake_read_sentences(_files->tdata[_l], _size2, _wanted, _n, &_files->texts[_l]);
        }
        rde_arr_free(&_wanted_list);
    }
}

// Adds each kept word of _words (_all of them) to _table by its written form, and by its stem too when it conjugates.
RDE_INTERNAL void fude_bake_tatoeba_map_words(rde_hash_map* _table, const fude_bake_word* _words, u32 _all) {
    for(u32 _w = 0; _w < _all; _w++) {
        if(_words[_w].index == UINT32_MAX) {
            continue;
        }
        const u8* _s = (const u8*)_words[_w].written;
        const u32 _n = (u32)rde_strlen(_words[_w].written);
        // The written form; and, when it ends in a verb's or an i-adjective's last kana (う く ぐ す つ ぬ ぶ む る い),
        // its stem too.
        u32 _last_at = 0, _last = 0, _cps = 0;
        for(u32 _i = 0; _i < _n;) {
            u32       _len = 0;
            const u32 _cp  = fude_utf8_decode((const c8*)_s + _i, &_len);
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
            const fude_bake_word_key _key = { (const c8*)_s, _k == 0 ? _n : _last_at, (u8)_k };
            // Of words spelt alike, the first keeps the place; both are numbered, and the first is enough here.
            rde_hash_map_add(_table, (any)&_key, &_w);   // The first spelt alike keeps the place.
        }
    }
}

// Finds each kept word's best example sentence among the Japanese ones in _files, by the rules above, into _best (a
// sentence's index in japanese) and _best_score, which hold each word's best so far. A sentence the index has gives
// the words the index names (_sorted: the _sorted_count kept words' numbers, sorted by written form); any other gives
// the words of _words found in its text through _table. Returns how many sentences had a fitting length and English.
RDE_INTERNAL u32 fude_bake_tatoeba_best(const fude_bake_tatoeba_files* _files, const fude_bake_word* _words, const u32* _sorted, u32 _sorted_count,
                                        const rde_hash_map* _table, u32* _best, i32* _best_score) {
    const fude_bake_sentence* _js = (const fude_bake_sentence*)_files->japanese.memory;
    u32 _candidates = 0;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_files->japanese); _i++) {
        // The sentence as code points, with where each starts, if its length is within the lengths kept.
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
            const u32 _cp  = fude_utf8_decode((const c8*)_files->jdata + _js[_i].at + _p, &_len);
            _at[_n]    = _p;
            _cps[_n++] = _cp;
            _p += _len > 0 ? _len : 1u;
        }
        if(!_fits || _n < FUDE_BAKE_SENT_MIN || _n > FUDE_BAKE_SENT_MAX) {
            continue;
        }
        _at[_n] = _js[_i].len;
        // It must have an English translation; each other language it lacks adds 6 to its score. Each character
        // away from the ideal length adds 4.
        const u32 _en = fude_bake_link_of(&_files->links[0], _js[_i].id);
        if(_en == 0 || fude_bake_sentence_of(&_files->texts[0], _en) == NULL) {
            continue;
        }
        _candidates++;
        i32 _score = (i32)(_n > FUDE_BAKE_SENT_IDEAL ? _n - FUDE_BAKE_SENT_IDEAL : FUDE_BAKE_SENT_IDEAL - _n) * 4;
        for(u32 _l = 1; _l < FUDE_BAKE_SENT_LANGS; _l++) {
            const u32 _o = fude_bake_link_of(&_files->links[_l], _js[_i].id);
            _score += _o != 0 && fude_bake_sentence_of(&_files->texts[_l], _o) != NULL ? 0 : 6;
        }
        // An indexed sentence gives the words the index names.
        const fude_bake_sentence* _ix = fude_bake_sentence_of(&_files->index, _js[_i].id);
        if(_ix != NULL) {
            fude_bake_index_words((const c8*)_files->idata + _ix->at, _ix->len, _words, _sorted, _sorted_count, _i, _score, _best, _best_score);
            continue;
        }
        // Otherwise, every stretch of two to five characters is looked up, as a word as written or as a stem with kana
        // after it.
        _score += FUDE_BAKE_SENT_TEXT;
        for(u32 _a = 0; _a < _n; _a++) {
            for(u32 _len = 1; _len <= 5u && _a + _len <= _n; _len++) {
                const u8* _s     = _files->jdata + _js[_i].at + _at[_a];
                const u32 _bytes = _at[_a + _len] - _at[_a];
                for(u32 _k = 0; _k < 2u; _k++) {
                    if(_k == 1u && !(_a + _len < _n && _cps[_a + _len] >= 0x3041u && _cps[_a + _len] <= 0x309Fu)) {
                        continue;   // A stem only counts with kana after it.
                    }
                    const fude_bake_word_key _key   = { (const c8*)_s, _bytes, (u8)_k };
                    const u32*               _found = (const u32*)rde_hash_map_find(_table, (any)&_key);
                    if(_found == NULL || _len == 1u) {
                        continue;   // Not a word, or one kanji alone, whose word the text does not say.
                    }
                    const u32 _w = *_found;
                    if(_score < _best_score[_w]) {
                        _best[_w]       = _i;
                        _best_score[_w] = _score;
                    }
                }
            }
        }
    }
    return _candidates;
}

// Puts the sentences chosen into the bake: each word's in _best (an index in _files->japanese, or UINT32_MAX for
// none), once each, numbered in order, each followed by its translations; then gives each kept word of _words (_all
// of them) its sentence's number. _number has room for a number for each Japanese sentence. Returns how many words
// got a sentence.
RDE_INTERNAL u32 fude_bake_tatoeba_keep(fude_bake* _bake, const fude_bake_tatoeba_files* _files, const fude_bake_word* _words, u32 _all, const u32* _best,
                                        u32* _number) {
    const fude_bake_sentence* _js = (const fude_bake_sentence*)_files->japanese.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_files->japanese); _i++) {
        _number[_i] = UINT32_MAX;
    }
    rde_arr_resize(&_bake->sentence_words, _bake->word_count);
    u32* _sentence_of = (u32*)_bake->sentence_words.memory;
    for(u32 _i = 0; _i < _bake->word_count; _i++) {
        _sentence_of[_i] = UINT32_MAX;
    }
    u32 _with = 0;
    for(u32 _w = 0; _w < _all; _w++) {
        if(_words[_w].index == UINT32_MAX || _best[_w] == UINT32_MAX) {
            continue;
        }
        const u32 _i = _best[_w];
        if(_number[_i] == UINT32_MAX) {
            _number[_i] = _bake->sentence_count++;
            fude_put_data(&_bake->sentences, _files->jdata + _js[_i].at, _js[_i].len);
            fude_put_u8(&_bake->sentences, 0u);
            for(u32 _l = 0; _l < FUDE_BAKE_SENT_LANGS; _l++) {
                const u32                 _o = fude_bake_link_of(&_files->links[_l], _js[_i].id);
                const fude_bake_sentence* _t = _o != 0 ? fude_bake_sentence_of(&_files->texts[_l], _o) : NULL;
                if(_t != NULL) {
                    fude_put_data(&_bake->sentences, _files->tdata[_l] + _t->at, _t->len);
                }
                fude_put_u8(&_bake->sentences, 0u);
            }
        }
        _sentence_of[_words[_w].index] = _number[_i];
        _with++;
    }
    return _with;
}

// Reads Tatoeba's files in _dir (jpn_sentences.tsv, jpn_indices.csv, and each language's jpn-<code>_links.tsv and
// <code>_sentences.tsv) and gives each kept word its best example sentence, by the rules above. The sentences chosen
// are added to the bake once each, with their translations, and each word gets its sentence's number. A missing
// jpn_sentences.tsv is logged and gives no sentences; a missing index means sentences are found as text only. Returns
// false, with an error logged, when jpn_sentences.tsv is there but cannot be read: the bake then stops, rather than
// make data that would quietly ship without its sentences.
RDE_INTERNAL b8 fude_bake_tatoeba(fude_bake* _bake, const c8* _dir) {
    c8 _path[RDE_MAX_PATH];
    rde_snprintf(_path, sizeof(_path), "%s/jpn_sentences.tsv", _dir);
    if(!rde_file_exists(_path)) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "bake: %s not found; no example sentences", _path);
        return true;
    }
    const f64 _t0 = rde_engine_get_time_now();
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();

    // The Japanese sentences, and then each language's links from them.
    fude_bake_tatoeba_files _files;
    u32 _jsize = 0;
    _files.jdata = fude_file_read(_path, &_jsize);
    if(_files.jdata == NULL) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: %s is there but could not be read", _path);
        return false;
    }
    _files.japanese = rde_arr_new(sizeof(fude_bake_sentence), _heap);
    fude_bake_read_sentences(_files.jdata, _jsize, NULL, 0, &_files.japanese);
    // The word index, "id <tab> meaning id <tab> words", read as the sentences are, so its text is the words.
    fude_bake_tatoeba_read_index(&_files, _dir, _heap);
    fude_bake_tatoeba_read_translations(&_files, _dir, _heap);

    // The map of the words kept, by written form, and by stem where they conjugate.
    fude_bake_word* _words = (fude_bake_word*)_bake->words.memory;
    const u32       _all   = (u32)rde_arr_length(&_bake->words);
    rde_hash_map    _table = rde_hash_map_new(sizeof(fude_bake_word_key), sizeof(u32), fude_bake_word_key_hash, fude_bake_word_key_cmp, NULL, _heap);
    rde_hash_map_reserve(&_table, 2u * _bake->word_count);   // A form and, often, a stem for each.
    fude_bake_tatoeba_map_words(&_table, _words, _all);

    // ...and every kept word's number, sorted by written form, for the index.
    rde_arr TYPE(u32) _sorted_list  = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_sorted_list, _all);
    u32*              _sorted       = (u32*)_sorted_list.memory;
    u32               _sorted_count = 0;
    for(u32 _w = 0; _w < _all; _w++) {
        if(_words[_w].index != UINT32_MAX) {
            _sorted[_sorted_count++] = _w;
        }
    }
    fude_bake_sort_words = _words;
    rde_sort(_sorted, _sorted_count, sizeof(u32), fude_bake_by_written);

    // Each word's best sentence so far: its index in _files.japanese, and its score (lower is better).
    rde_arr TYPE(u32) _best_list  = rde_arr_new(sizeof(u32), _heap);
    rde_arr TYPE(i32) _score_list = rde_arr_new(sizeof(i32), _heap);
    rde_arr_resize(&_best_list, _all);
    rde_arr_resize(&_score_list, _all);
    u32* _best       = (u32*)_best_list.memory;
    i32* _best_score = (i32*)_score_list.memory;
    for(u32 _w = 0; _w < _all; _w++) {
        _best[_w]       = UINT32_MAX;
        _best_score[_w] = 0x7FFFFFFF;
    }
    const u32 _candidates = fude_bake_tatoeba_best(&_files, _words, _sorted, _sorted_count, &_table, _best, _best_score);

    // The sentences chosen go into the bake once each, numbered in order, each followed by its translations in the
    // FUDE_BAKE_SENT_CODES order (an empty string where there is none); then each word gets its sentence's number.
    rde_arr TYPE(u32) _number_list = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_number_list, rde_arr_length(&_files.japanese));
    u32* _number = (u32*)_number_list.memory;
    const u32 _with = fude_bake_tatoeba_keep(_bake, &_files, _words, _all, _best, _number);
    rde_log_level(RDE_LOG_LEVEL_INFO, "bake: Tatoeba: %u Japanese sentences, %u with English and a fitting length; %u words with an example sentence, %u sentences kept (%.2f MB), %.2f s",
                  (u32)rde_arr_length(&_files.japanese), _candidates, _with, _bake->sentence_count, (f64)fude_bytes_size(&_bake->sentences) / (1024.0 * 1024.0),
                  rde_engine_get_time_now() - _t0);

    rde_arr_free(&_number_list);
    rde_arr_free(&_sorted_list);
    rde_arr_free(&_files.index);
    fude_file_free(_files.idata);
    rde_arr_free(&_best_list);
    rde_arr_free(&_score_list);
    rde_hash_map_free(&_table);
    for(u32 _l = 0; _l < FUDE_BAKE_SENT_LANGS; _l++) {
        rde_arr_free(&_files.links[_l]);
        rde_arr_free(&_files.texts[_l]);
        fude_file_free(_files.tdata[_l]);
    }
    rde_arr_free(&_files.japanese);
    fude_file_free(_files.jdata);
    return true;
}

// --- look-alikes: Japanese's rules (bake.h's fude_bake_look) ------------------------------
// The characters worth look-alikes are those of a school grade, the rest of the Jouyou, or a JLPT level, and every
// plain kana. Kana are compared among themselves, and kanji among the kanji.

// The matcher's highest cost that counts, in a 100-unit box: 未 末 cost 4, 土 士 7, 待 持 10; 人 大 at 13 is too far.
#define FUDE_BAKE_LOOK_COST      11.0f   // For kanji: 未 末 4, 土 士 7, 待 持 10; 人 大 13 is too far.
#define FUDE_BAKE_LOOK_COST_KANA 21.0f   // Kana are simpler, so they cost more apart: わ れ 8, ツ ソ 15, シ ン 18.
#define FUDE_BAKE_LOOK_STROKES   3u      // How many strokes apart, at most.

// Returns whether _cp is a kana: hiragana U+3041..U+3096 or katakana U+30A1..U+30FA.
RDE_INTERNAL b8 fude_bake_look_kana(u32 _cp) {
    return (_cp >= 0x3041u && _cp <= 0x3096u) || (_cp >= 0x30A1u && _cp <= 0x30FAu);
}

// Returns whether _cp is a plain kana, with no small one and no dakuten: to a learner シ and ジ, or ツ and ッ, are the
// same character, not a mix-up.
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

// Returns whether a character is worth look-alikes: a plain kana, or a kanji with a school grade (1..8, the Jouyou)
// or a JLPT level.
RDE_INTERNAL b8 fude_bake_look_common(const fude_kanji_info* _info) {
    return fude_bake_look_kana(_info->codepoint) ? fude_bake_look_plain(_info->codepoint) : (_info->grade >= 1u && _info->grade <= 8u) || _info->level != 0u;
}

// The look-alike rules handed to the study's bake.
static const fude_bake_look FUDE_BAKE_LOOK = {
    .script      = fude_bake_look_kana,
    .common      = fude_bake_look_common,
    .cost        = FUDE_BAKE_LOOK_COST,
    .cost_script = FUDE_BAKE_LOOK_COST_KANA,
    .strokes     = FUDE_BAKE_LOOK_STROKES,
};

// --- the bake --------------------------------------------------------------------------

// Runs Japanese's bake with the app's command line (see the top): KanjiVG, then KANJIDIC2, the JLPT lists from N5 to
// N1, JMdict and Tatoeba; then writes the file and adds the look-alikes. Returns 0 on success, 1 on any failure (the
// log says why).
i32 fude_bake_run(const fude_bake_app* _app, i32 _argc, c8** _argv) {
    const c8* _kanjivg  = fude_bake_arg(_argc, _argv, "--kanjivg=",  "data/raw/kanjivg.xml");
    const c8* _kanjidic = fude_bake_arg(_argc, _argv, "--kanjidic=", "data/raw/kanjidic2.xml");
    const c8* _jlpt     = fude_bake_arg(_argc, _argv, "--jlpt=",     "data/raw/jlpt");
    // The full JMdict when it is there (English and the other languages), or else JMdict_e (English only).
    const c8* _jmdict   = fude_bake_arg(_argc, _argv, "--jmdict=",   rde_file_exists("data/raw/JMdict.xml") ? "data/raw/JMdict.xml" : "data/raw/JMdict_e.xml");
    const c8* _out      = fude_bake_arg(_argc, _argv, "--out=",      _app->out != NULL ? _app->out : FUDE_KANJI_FILE);
    const c8* _tatoeba  = fude_bake_arg(_argc, _argv, "--tatoeba=",  "data/raw/tatoeba");

    for(u32 _i = 0; _i < 2; _i++) {
        const c8* _src = _i == 0 ? _kanjivg : _kanjidic;
        if(!rde_file_exists(_src)) {
            rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: %s not found (see COMMANDS.txt for where to get it; run from the project root)", _src);
            return 1;
        }
    }

    // The meanings' languages are FUDE_BAKE_LANG_LIST's codes, taken from it so the two cannot disagree.
    const c8* _langs[FUDE_BAKE_JA_LANGS];
    for(u32 _l = 0; _l < FUDE_BAKE_JA_LANGS; _l++) {
        _langs[_l] = FUDE_BAKE_LANG_LIST[_l].code;
    }
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

        // JMdict comes after KANJIDIC2, because the words are ranked by their kanji's grades.
        if(!fude_bake_jmdict(&_bake, _jmdict)) {
            _rc = 1;
        }
        // Tatoeba comes after the words, because it finds their example sentences.
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

#endif
