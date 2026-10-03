#include "lang/wordsplit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See wordsplit.h: Korean. The index — every word's written form — is an
// open-addressed hash table built the first time it is needed, as Chinese's.
// Korean writes a word with its particles and endings joined to it, and spaces
// between those chunks (어절): in each, the longest start that is a word (학교에서:
// 학교), or a verb's or an adjective's stem with its ending (먹었어요: 먹다;
// 했어요: 하다; 봐요: 보다; 추워요: 춥다; 몰라요: 모르다; 갑니다: 가다). One word a
// chunk; where two words are spelt alike, the commoner.
// ===========================================================================

#define FUDE_WORDSPLIT_CHUNK 16u    // syllables of a chunk looked at, at most

#define FUDE_KO_S0      0xAC00u
#define FUDE_KO_S1      0xD7A3u
#define FUDE_KO_I_NG    11u         // ㅇ: no initial
#define FUDE_KO_I_H     18u         // ㅎ
#define FUDE_KO_I_R     5u          // ㄹ
#define FUDE_KO_F_L     8u          // ㄹ
#define FUDE_KO_F_B     17u         // ㅂ
#define FUDE_KO_F_SS    20u         // ㅆ

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


// --- a chunk's syllables ----------------------------------------------------------------

RDE_INTERNAL b8 fude_ko_is_syllable(u32 _cp) {
    return _cp >= FUDE_KO_S0 && _cp <= FUDE_KO_S1;
}

RDE_INTERNAL u32 fude_ko_compose(u32 _i, u32 _m, u32 _f) {
    return FUDE_KO_S0 + (_i * 21u + _m) * 28u + _f;
}

// The word _syl[0.._n) spelt, then _ending (UTF-8, may be ""), or UINT32_MAX.
RDE_INTERNAL u32 fude_ko_find(const fude_kanji_db* _db, const u32* _syl, u32 _n, const c8* _ending) {
    c8    _key[4u * FUDE_WORDSPLIT_CHUNK + 8u];
    usize _k = 0;
    for(u32 _s = 0; _s < _n; _s++) {
        fude_utf8_put(_syl[_s], _key + _k);
        _k += strlen(_key + _k);
    }
    const usize _e = strlen(_ending);
    memcpy(_key + _k, _ending, _e + 1u);
    return _k + _e > 0 ? fude_wordsplit_find(_db, _key, _k + _e) : UINT32_MAX;
}

// The verb or adjective (its stem + 다) _syl[0.._n) is a form of: the stem as it
// is, or its last syllable's ending undone. UINT32_MAX when none.
RDE_INTERNAL u32 fude_ko_verb(const fude_kanji_db* _db, const u32* _syl, u32 _n) {
    static const c8* const _DA = "\xEB\x8B\xA4";   // 다
    if(_n == 0u) {
        return UINT32_MAX;
    }
    u32 _w = fude_ko_find(_db, _syl, _n, _DA);   // 먹(다), 가(다)
    if(_w != UINT32_MAX) {
        return _w;
    }
    u32       _stem[FUDE_WORDSPLIT_CHUNK];
    memcpy(_stem, _syl, sizeof(u32) * _n);
    const u32 _last = _syl[_n - 1u] - FUDE_KO_S0;
    const u32 _i    = _last / (21u * 28u);
    u32       _m    = (_last / 28u) % 21u;
    const u32 _f    = _last % 28u;
    // An ending that is a final: 갑(니다), 갈, 간, 감 — the stem without it.
    if(_f == 4u || _f == FUDE_KO_F_L || _f == 16u || _f == FUDE_KO_F_B) {
        _stem[_n - 1u] = fude_ko_compose(_i, _m, 0u);
        if((_w = fude_ko_find(_db, _stem, _n, _DA)) != UINT32_MAX) {
            return _w;
        }
    }
    // The past's ㅆ (먹었, 갔, 했) off; then the -아/-어 it was on.
    if(_f != 0u && _f != FUDE_KO_F_SS) {
        return UINT32_MAX;
    }
    if(_i == FUDE_KO_I_NG && (_m == 0u || _m == 4u || _m == 6u) && _n >= 2u) {   // 아 어 여: 먹어 → 먹
        if((_w = fude_ko_find(_db, _stem, _n - 1u, _DA)) != UINT32_MAX) {
            return _w;
        }
    }
    if(_i == FUDE_KO_I_NG && (_m == 9u || _m == 14u) && _n >= 2u) {              // 와 워 after a stem's ㅂ: 도와 → 돕, 추워 → 춥
        const u32 _prev = _stem[_n - 2u] - FUDE_KO_S0;
        if(_prev % 28u == 0u) {
            _stem[_n - 2u] = _stem[_n - 2u] + FUDE_KO_F_B;
            if((_w = fude_ko_find(_db, _stem, _n - 1u, _DA)) != UINT32_MAX) {
                return _w;
            }
            _stem[_n - 2u] = _syl[_n - 2u];
        }
    }
    if(_i == FUDE_KO_I_R && (_m == 0u || _m == 4u) && _n >= 2u) {                // 라 러 after ㄹ: 몰라 → 모르, 불러 → 부르
        const u32 _prev = _stem[_n - 2u] - FUDE_KO_S0;
        if(_prev % 28u == FUDE_KO_F_L) {
            _stem[_n - 2u] = _stem[_n - 2u] - FUDE_KO_F_L;
            _stem[_n - 1u] = fude_ko_compose(FUDE_KO_I_R, 18u, 0u);   // 르
            if((_w = fude_ko_find(_db, _stem, _n, _DA)) != UINT32_MAX) {
                return _w;
            }
            memcpy(_stem, _syl, sizeof(u32) * _n);
        }
    }
    // The vowel the ending made, back to the stem's: 해 하, 봐 보, 줘 주, 셔 시, 돼 되,
    // 써 쓰, 바빠 바쁘 — and the syllable as it is (가, 서, 보내).
    static const u8 _back[][2] = { { 1, 0 }, { 9, 8 }, { 14, 13 }, { 6, 20 }, { 10, 11 }, { 4, 18 }, { 0, 18 }, { 255, 255 } };
    for(u32 _b = 0; _back[_b][0] != 255u; _b++) {
        if(_m != _back[_b][0] || (_b == 0u && _i != FUDE_KO_I_H)) {   // ㅐ → ㅏ only in 해 (하다's)
            continue;
        }
        _stem[_n - 1u] = fude_ko_compose(_i, _back[_b][1], 0u);
        if((_w = fude_ko_find(_db, _stem, _n, _DA)) != UINT32_MAX) {
            return _w;
        }
    }
    _stem[_n - 1u] = fude_ko_compose(_i, _m, 0u);
    return fude_ko_find(_db, _stem, _n, _DA);
}

u32 fude_wordsplit(const fude_kanji_db* _db, const c8* _text, u32* _out, u32 _max) {
    if(_db == NULL || _text == NULL || _max == 0) {
        return 0;
    }
    if(_db != fude_wordsplit_for) {
        fude_wordsplit_build(_db);
    }
    static const u32 _YO = 0xC694u;   // 요: a polite ending, not the end of a word (가요: 가다, not 가요 "songs")
    u32       _found = 0;
    const c8* _p     = _text;
    u32       _cp    = fude_utf8_next(&_p);
    while(_cp != 0u && _found < _max) {
        if(!fude_ko_is_syllable(_cp)) {
            _cp = fude_utf8_next(&_p);
            continue;
        }
        // A chunk: the syllables up to anything else.
        u32 _syl[FUDE_WORDSPLIT_CHUNK];
        u32 _n = 0;
        while(fude_ko_is_syllable(_cp)) {
            if(_n < FUDE_WORDSPLIT_CHUNK) {
                _syl[_n++] = _cp;
            }
            _cp = fude_utf8_next(&_p);
        }
        u32 _best = UINT32_MAX;
        for(u32 _len = _n; _len >= 1u && _best == UINT32_MAX; _len--) {
            const b8 _polite = _len == _n && _len >= 2u && _syl[_len - 1u] == _YO;
            if(!_polite) {
                _best = fude_ko_find(_db, _syl, _len, "");
            }
            if(_best == UINT32_MAX) {
                _best = fude_ko_verb(_db, _syl, _len);
            }
        }
        if(_best == UINT32_MAX) {
            continue;
        }
        b8 _seen = false;
        for(u32 _k = 0; _k < _found; _k++) {
            _seen |= _out[_k] == _best;
        }
        if(!_seen) {
            _out[_found++] = _best;
        }
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
        if(fude_kanji_find(_db, _c, &_info)) {
            return _c;   // its first syllable (or hanja)
        }
    }
}
