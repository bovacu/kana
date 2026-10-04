// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/wordsplit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See wordsplit.h: Thai. The index — every word's written form — is an
// open-addressed hash table built the first time it is needed, as Chinese's.
// Thai is written without spaces between words: they are matched as written,
// the longest first from the left, never ending where a syllable goes on (a
// vowel sign or a tone mark after it is its last consonant's; a vowel written
// before its consonant, เ แ โ ใ ไ, starts the next). Where two words are spelt
// alike, the commoner.
// ===========================================================================

#define FUDE_WORDSPLIT_LOOK  24u    // characters (code points: signs and marks count) tried from each position, at most
#define FUDE_WORDSPLIT_LINE  256u   // characters of a line looked at, at most

typedef struct {
    u32 hash;
    u32 word;    // UINT32_MAX: empty
    u16 length;  // bytes of the written form
} fude_wordsplit_slot;

static const fude_kanji_db* fude_wordsplit_for   = NULL;
static fude_wordsplit_slot* fude_wordsplit_table = NULL;
static u32                  fude_wordsplit_size  = 0;   // a power of two
static u32*                 fude_wordsplit_rank  = NULL; // per word: how common ('WFRQ', high half) then how soon an example (low): lower wins
static u32*                 fude_wordsplit_alike = NULL; // per word: the next spelt the same (UINT32_MAX: none) — from the commonest

RDE_INTERNAL u32 fude_wordsplit_hash(const c8* _s, usize _n) {
    u32 _h = 2166136261u;
    for(usize _i = 0; _i < _n; _i++) {
        _h = (_h ^ (u8)_s[_i]) * 16777619u;
    }
    return _h != 0u ? _h : 1u;
}

RDE_INTERNAL b8 fude_wordsplit_is_kanji(u32 _cp) {
    return _cp >= 0x0E01u && _cp <= 0x0E5Bu;   // the Thai block's letters and signs
}

// Does _cp belong to the letter before it (a vowel sign above or below, a tone mark)?
RDE_INTERNAL b8 fude_wordsplit_joins_before(u32 _cp) {
    return _cp == 0x0E31u || (_cp >= 0x0E34u && _cp <= 0x0E3Au) || (_cp >= 0x0E47u && _cp <= 0x0E4Eu) || _cp == 0x0E33u || _cp == 0x0E45u;
}

// Does a word never end on _cp (a vowel written before the consonant it goes with)?
RDE_INTERNAL b8 fude_wordsplit_ends_not(u32 _cp) {
    return _cp >= 0x0E40u && _cp <= 0x0E44u;
}

RDE_INTERNAL void fude_wordsplit_put(u32 _word, const c8* _key, u16 _length) {
    const u32 _h    = fude_wordsplit_hash(_key, _length);
    const u32 _mask = fude_wordsplit_size - 1u;
    for(u32 _i = _h & _mask;; _i = (_i + 1u) & _mask) {
        fude_wordsplit_slot* _s = &fude_wordsplit_table[_i];
        if(_s->word == UINT32_MAX) {
            *_s = (fude_wordsplit_slot){ _h, _word, _length };
            return;
        }
        if(_s->hash == _h && _s->length == _length) {
            // Spelt alike: the commoner heads the chain, the others follow it.
            if(fude_wordsplit_rank[_word] < fude_wordsplit_rank[_s->word]) {
                fude_wordsplit_alike[_word] = _s->word;
                _s->word                    = _word;
            } else {
                fude_wordsplit_alike[_word]    = fude_wordsplit_alike[_s->word];
                fude_wordsplit_alike[_s->word] = _word;
            }
            return;
        }
    }
}

RDE_INTERNAL void fude_wordsplit_build(const fude_kanji_db* _db) {
    free(fude_wordsplit_table);
    free(fude_wordsplit_rank);
    free(fude_wordsplit_alike);
    fude_wordsplit_table = NULL;
    fude_wordsplit_rank  = NULL;
    fude_wordsplit_alike = NULL;
    fude_wordsplit_for   = _db;
    if(_db == NULL || _db->word_count == 0) {
        return;
    }
    // Each word's rank: how common it is (the data's 'WFRQ'), then how soon it
    // is shown as an example (its place in the lists, best first).
    fude_wordsplit_rank  = (u32*)malloc(sizeof(u32) * _db->word_count);
    fude_wordsplit_alike = (u32*)malloc(sizeof(u32) * _db->word_count);
    if(fude_wordsplit_rank == NULL || fude_wordsplit_alike == NULL) {
        free(fude_wordsplit_rank);
        free(fude_wordsplit_alike);
        fude_wordsplit_rank  = NULL;
        fude_wordsplit_alike = NULL;
        return;
    }
    for(u32 _w = 0; _w < _db->word_count; _w++) {
        fude_wordsplit_alike[_w] = UINT32_MAX;
    }
    for(u32 _w = 0; _w < _db->word_count; _w++) {
        fude_wordsplit_rank[_w] = ((u32)fude_kanji_word_freq(_db, _w) << 16) | 0xFFFFu;
    }
    static u32 _list[1024];
    for(u32 _r = 0; _r < _db->count; _r++) {
        const u32 _n = fude_kanji_words(_db, _r, _list, 1024u, NULL);
        for(u32 _p = 0; _p < _n; _p++) {
            if(_list[_p] < _db->word_count && _p < (fude_wordsplit_rank[_list[_p]] & 0xFFFFu)) {
                fude_wordsplit_rank[_list[_p]] = (fude_wordsplit_rank[_list[_p]] & 0xFFFF0000u) | _p;
            }
        }
    }
    fude_wordsplit_size = 1u;
    while(fude_wordsplit_size < _db->word_count * 2u) {
        fude_wordsplit_size <<= 1u;
    }
    fude_wordsplit_table = (fude_wordsplit_slot*)malloc(sizeof(fude_wordsplit_slot) * fude_wordsplit_size);
    if(fude_wordsplit_table == NULL) {
        return;
    }
    for(u32 _i = 0; _i < fude_wordsplit_size; _i++) {
        fude_wordsplit_table[_i].word = UINT32_MAX;
    }
    for(u32 _w = 0; _w < _db->word_count; _w++) {
        fude_kanji_word _word;
        if(!fude_kanji_word_at(_db, _w, &_word)) {
            continue;
        }
        const usize _n = strlen(_word.written);
        if(_n > 0 && _n <= 0xFFFFu) {
            fude_wordsplit_put(_w, _word.written, (u16)_n);
        }
    }
}

// The word written _key (_length bytes), or UINT32_MAX.
RDE_INTERNAL u32 fude_wordsplit_find(const fude_kanji_db* _db, const c8* _key, usize _length) {
    if(fude_wordsplit_table == NULL || _length == 0 || _length > 0xFFFFu) {
        return UINT32_MAX;
    }
    const u32 _h    = fude_wordsplit_hash(_key, _length);
    const u32 _mask = fude_wordsplit_size - 1u;
    for(u32 _i = _h & _mask;; _i = (_i + 1u) & _mask) {
        const fude_wordsplit_slot* _s = &fude_wordsplit_table[_i];
        if(_s->word == UINT32_MAX) {
            return UINT32_MAX;
        }
        if(_s->hash == _h && _s->length == _length) {
            fude_kanji_word _word;
            if(fude_kanji_word_at(_db, _s->word, &_word) && memcmp(_word.written, _key, _length) == 0) {
                return _s->word;
            }
        }
    }
}

u32 fude_wordsplit(const fude_kanji_db* _db, const c8* _text, u32* _out, u32 _max) {
    if(_db == NULL || _text == NULL || _max == 0) {
        return 0;
    }
    if(_db != fude_wordsplit_for) {
        fude_wordsplit_build(_db);
    }
    // Lines joined: a line break (handwriting that went on below) is never inside
    // a word on purpose.
    c8 _joined[4u * FUDE_WORDSPLIT_LINE + 1u];
    {
        usize     _j = 0;
        const c8* _s = _text;
        for(; *_s != 0 && _j + 1u < sizeof(_joined); _s++) {
            if(*_s != '\n' && *_s != '\r') {
                _joined[_j++] = *_s;
            }
        }
        if(*_s != 0) {
            // Cut short (a very long text): back to a character's start.
            while(_j > 0 && ((u8)_joined[_j - 1u] & 0xC0u) == 0x80u) {
                _j--;
            }
            if(_j > 0 && ((u8)_joined[_j - 1u] & 0x80u) != 0u) {
                _j--;
            }
        }
        _joined[_j] = 0;
        _text = _joined;
    }
    // The line as code points, each with where it starts.
    u32       _cps[FUDE_WORDSPLIT_LINE + 1u];
    usize     _at[FUDE_WORDSPLIT_LINE + 1u];
    u32       _n = 0;
    const c8* _p = _text;
    while(_n < FUDE_WORDSPLIT_LINE) {
        _at[_n] = (usize)(_p - _text);
        const u32 _c = fude_utf8_next(&_p);
        if(_c == 0) {
            break;
        }
        _cps[_n++] = _c;
    }
    _at[_n] = (usize)(_p - _text);

    u32 _found = 0;
    for(u32 _i = 0; _i < _n && _found < _max;) {
        u32 _best      = UINT32_MAX;
        u32 _best_take = 0;   // characters of the text it covers
        const u32 _look = _n - _i < FUDE_WORDSPLIT_LOOK ? _n - _i : FUDE_WORDSPLIT_LOOK;
        for(u32 _len = _look; _len >= 1u; _len--) {
            b8 _kanji = false;
            for(u32 _k = _i; _k < _i + _len; _k++) {
                _kanji |= fude_wordsplit_is_kanji(_cps[_k]);
            }
            if(!_kanji || fude_wordsplit_ends_not(_cps[_i + _len - 1u]) || (_i + _len < _n && fude_wordsplit_joins_before(_cps[_i + _len]))) {
                continue;   // only words of the script, and never half a syllable
            }
            const c8*   _span       = _text + _at[_i];
            const usize _span_bytes = _at[_i + _len] - _at[_i];
            const u32   _exact      = fude_wordsplit_find(_db, _span, _span_bytes);
            if(_exact != UINT32_MAX && _len > _best_take) {
                _best      = _exact;
                _best_take = _len;
                break;   // the longest first: none longer is left
            }
        }
        if(_best == UINT32_MAX) {
            _i++;
            continue;
        }
        // The word — and any spelt the same that are as common (the data cannot
        // tell them apart: the learner can).
        for(u32 _w = _best, _alike = 0; _w != UINT32_MAX && _found < _max && _alike < 3u; _w = fude_wordsplit_alike[_w]) {
            if(_w != _best && (fude_wordsplit_rank[_w] >> 16) != (fude_wordsplit_rank[_best] >> 16)) {
                continue;
            }
            b8 _seen = false;
            for(u32 _k = 0; _k < _found; _k++) {
                _seen |= _out[_k] == _w;
            }
            if(!_seen) {
                _out[_found++] = _w;
                _alike++;
            }
        }
        _i += _best_take;
    }
    return _found;
}

u32 fude_wordsplit_kanji(const fude_kanji_db* _db, const c8* _written) {
    if(_db == NULL || _written == NULL) {
        return 0;
    }
    const c8* _p = _written;
    for(;;) {
        const u32 _c = fude_utf8_next(&_p);
        if(_c == 0) {
            return 0;
        }
        fude_kanji_info _info;
        if(fude_wordsplit_is_kanji(_c) && fude_kanji_find(_db, _c, &_info)) {
            return _c;
        }
    }
}
