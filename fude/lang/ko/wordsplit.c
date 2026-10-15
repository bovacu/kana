// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"

#include "drawing/base/utf8.h"
#include "lang/ko/hangul.h"
#include "study/chars/wordsplit.h"

// ===========================================================================
// See lang.h. Korean's words (fude_lang_words), found by a matcher of its own. Korean writes a word with its particles
// and endings joined to it, and puts spaces between those chunks (어절). In each chunk the matcher finds one word: the
// longest start of the chunk that is a word as written (학교에서: 학교), or that is a verb's or an adjective's stem with
// its ending (먹었어요: 먹다; 했어요: 하다; 봐요: 보다; 추워요: 춥다; 몰라요: 모르다; 갑니다: 가다). The rest of the
// chunk is passed over.
// A stem is found by undoing the ending on the start's last syllable, letter by letter (hangul.h): a final that is
// an ending, the past's ㅆ, the -아/-어 ending, the ㅂ and 르 irregular verbs, and the vowels an ending merges with.
// ===========================================================================

#define FUDE_KO_CHUNK 16u   // The most syllables of a chunk that are looked at.

// The letters the matcher needs, by their number in Unicode's order (hangul.h). Initials:
#define FUDE_KO_I_R   5u    // ㄹ
#define FUDE_KO_I_NG  11u   // ㅇ, which has no sound before a vowel.
#define FUDE_KO_I_H   18u   // ㅎ
// Medials:
#define FUDE_KO_M_A   0u    // ㅏ
#define FUDE_KO_M_EO  4u    // ㅓ
#define FUDE_KO_M_YEO 6u    // ㅕ
#define FUDE_KO_M_WA  9u    // ㅘ
#define FUDE_KO_M_WO  14u   // ㅝ
#define FUDE_KO_M_EU  18u   // ㅡ
// Finals:
#define FUDE_KO_F_N   4u    // ㄴ
#define FUDE_KO_F_L   8u    // ㄹ
#define FUDE_KO_F_M   16u   // ㅁ
#define FUDE_KO_F_B   17u   // ㅂ
#define FUDE_KO_F_SS  20u   // ㅆ

// Looks up the word spelt by the syllables _syl[0.._n) followed by _ending (UTF-8, may be ""). Returns the word, or
// UINT32_MAX when the dictionary has no such word. _n is at most FUDE_KO_CHUNK and _ending a few bytes, which the key's
// buffer is sized for.
RDE_INTERNAL u32 fude_ko_find(const u32* _syl, u32 _n, const c8* _ending) {
    c8  _key[4u * FUDE_KO_CHUNK + 8u];
    u32 _bytes = 0;
    for(u32 _s = 0; _s < _n; _s++) {
        _bytes += fude_utf8_put(_syl[_s], _key + _bytes);
    }
    const usize _e = rde_strlen(_ending);
    rde_memcpy(_key + _bytes, _ending, _e);
    return fude_wordsplit_find(_key, _bytes + _e);
}

// Returns the verb or adjective (its dictionary form, the stem + 다) that the syllables _syl[0.._n) are a form of, or
// UINT32_MAX when they are none. It tries, in order, the syllables as a stem as they are, then each way of undoing an
// ending on the last syllable, and returns the first that is a word.
RDE_INTERNAL u32 fude_ko_verb(const u32* _syl, u32 _n) {
    static const c8* const _DA = "\xEB\x8B\xA4";   // 다
    if(_n == 0u) {
        return UINT32_MAX;
    }
    u32 _w = fude_ko_find(_syl, _n, _DA);   // The stem as it is: 먹(다), 가(다).
    if(_w != UINT32_MAX) {
        return _w;
    }
    u32 _stem[FUDE_KO_CHUNK];
    rde_memcpy(_stem, _syl, sizeof(u32) * _n);
    u32 _i, _m, _f;
    fude_ko_decompose(_syl[_n - 1u], &_i, &_m, &_f);
    // An ending that is only a final, as in 갑(니다), 갈, 간, 감: the stem is the syllable without it.
    if(_f == FUDE_KO_F_N || _f == FUDE_KO_F_L || _f == FUDE_KO_F_M || _f == FUDE_KO_F_B) {
        _stem[_n - 1u] = fude_ko_compose(_i, _m, 0u);
        if((_w = fude_ko_find(_stem, _n, _DA)) != UINT32_MAX) {
            return _w;
        }
    }
    // Any other final is not an ending, so there is no verb. The past's ㅆ (먹었, 갔, 했) is let through, and the
    // rules below undo the -아/-어 it was on, the ㅆ ignored.
    if(_f != 0u && _f != FUDE_KO_F_SS) {
        return UINT32_MAX;
    }
    // The last syllable is only the ending 아, 어 or 여, so the stem is the syllables before it.
    if(_i == FUDE_KO_I_NG && (_m == FUDE_KO_M_A || _m == FUDE_KO_M_EO || _m == FUDE_KO_M_YEO) && _n >= 2u) {   // 먹어 → 먹.
        if((_w = fude_ko_find(_stem, _n - 1u, _DA)) != UINT32_MAX) {
            return _w;
        }
    }
    // 와 or 워 after a syllable with no final: an ㅂ-irregular stem, whose ㅂ the ending took away.
    if(_i == FUDE_KO_I_NG && (_m == FUDE_KO_M_WA || _m == FUDE_KO_M_WO) && _n >= 2u) {   // 도와 → 돕, 추워 → 춥.
        if((_stem[_n - 2u] - FUDE_KO_SYLLABLE_FIRST) % FUDE_KO_FINALS == 0u) {
            _stem[_n - 2u] += FUDE_KO_F_B;
            if((_w = fude_ko_find(_stem, _n - 1u, _DA)) != UINT32_MAX) {
                return _w;
            }
            _stem[_n - 2u] = _syl[_n - 2u];
        }
    }
    // 라 or 러 after a syllable ending in ㄹ: a 르-irregular stem, whose 르 the ending split.
    if(_i == FUDE_KO_I_R && (_m == FUDE_KO_M_A || _m == FUDE_KO_M_EO) && _n >= 2u) {   // 몰라 → 모르, 불러 → 부르.
        if((_stem[_n - 2u] - FUDE_KO_SYLLABLE_FIRST) % FUDE_KO_FINALS == FUDE_KO_F_L) {
            _stem[_n - 2u] -= FUDE_KO_F_L;
            _stem[_n - 1u] = fude_ko_compose(FUDE_KO_I_R, FUDE_KO_M_EU, 0u);   // The syllable 르.
            if((_w = fude_ko_find(_stem, _n, _DA)) != UINT32_MAX) {
                return _w;
            }
            rde_memcpy(_stem, _syl, sizeof(u32) * _n);
        }
    }
    // An ending that merged with the stem's vowel: turn the vowel back into the stem's, as in 해 → 하, 봐 → 보, 줘 → 주,
    // 셔 → 시, 돼 → 되, 써 → 쓰, 바빠 → 바쁘. Each pair is {the merged medial, the stem's medial}. Last, the syllable as
    // it is without its final (가, 서, 보내).
    static const u8 _back[][2] = { { 1, 0 }, { 9, 8 }, { 14, 13 }, { 6, 20 }, { 10, 11 }, { 4, 18 }, { 0, 18 } };
    for(u32 _b = 0; _b < sizeof(_back) / sizeof(_back[0]); _b++) {
        if(_m != _back[_b][0] || (_b == 0u && _i != FUDE_KO_I_H)) {   // ㅐ → ㅏ only in 해, 하다's form.
            continue;
        }
        _stem[_n - 1u] = fude_ko_compose(_i, _back[_b][1], 0u);
        if((_w = fude_ko_find(_stem, _n, _DA)) != UINT32_MAX) {
            return _w;
        }
    }
    _stem[_n - 1u] = fude_ko_compose(_i, _m, 0u);
    return fude_ko_find(_stem, _n, _DA);
}

// The matcher (fude_lang_words_def.match): finds the word of the chunk that starts at _at, the run of syllables from
// there up to anything that is not one. Returns its longest start (of at most FUDE_KO_CHUNK syllables) that is a word,
// as written or as a verb's form, or UINT32_MAX. *_take is set to the whole chunk, a word found or not, so the chunk's
// particles and endings are passed over.
RDE_INTERNAL u32 fude_ko_match(const fude_lang_line* _line, u32 _at, u32* _take) {
    static const u32 _YO = 0xC694u;   // 요 ends a chunk as a polite ending, not as a word's end (가요: 가다, not "songs").
    u32 _n = 0;
    while(_at + _n < _line->n && fude_ko_is_syllable(_line->cps[_at + _n])) {
        _n++;
    }
    *_take = RDE_MAX(_n, 1u);
    _n     = RDE_MIN(_n, FUDE_KO_CHUNK);
    const u32* _syl  = _line->cps + _at;
    u32        _best = UINT32_MAX;
    for(u32 _len = _n; _len >= 1u && _best == UINT32_MAX; _len--) {
        const b8 _polite = _len == _n && _len >= 2u && _syl[_len - 1u] == _YO;
        if(!_polite) {
            _best = fude_ko_find(_syl, _len, "");
        }
        if(_best == UINT32_MAX) {
            _best = fude_ko_verb(_syl, _len);
        }
    }
    return _best;
}

// Returns Korean's rules for finding words, a static table. Only one word written alike is offered, because Korean's
// writing is its reading, so the others would differ only in meaning.
const fude_lang_words_def* fude_lang_words(void) {
    static const fude_lang_words_def _def = {
        .letter = fude_ko_is_syllable,
        .match  = fude_ko_match,
        .look   = FUDE_KO_CHUNK,
        .alike  = 1u,
    };
    return &_def;
}
