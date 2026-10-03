#include "lang/wordsplit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See wordsplit.h. The index — every word's written form — is an
// open-addressed hash table built the first time it is needed (tens of
// thousands of words: a few hundred KB, a few ms). Where two words are spelt
// alike (本: ほん, もと), the one the data shows soonest as a character's example
// wins: the commoner.
//
// Conjugations are undone by rules, as pop-up dictionaries do: after a stretch
// that has a kanji, the hiragana that follow are tried against a list of
// endings, each with what the dictionary form ends in instead (ました → る, きたい →
// く, かった → い...); the reading that covers the most of the text wins — so
// 行きたい is 行く, not the noun 行き, and 面白かった is 面白い, not 面白.
// ===========================================================================

#define FUDE_WORDSPLIT_LOOK  10u    // characters tried from each position, at most
#define FUDE_WORDSPLIT_LINE  256u   // characters of a line looked at, at most
#define FUDE_WORDSPLIT_RULES 512u

typedef struct {
    u32 hash;
    u32 word;    // UINT32_MAX: empty
    u16 length;  // bytes of the written form
} fude_wordsplit_slot;

typedef struct {
    c8 in[24];   // the ending in the text (hiragana)...
    c8 out[8];   // ...and what the dictionary form ends in instead
    u8 in_cps;   // characters of in
} fude_wordsplit_rule;

static const fude_kanji_db* fude_wordsplit_for   = NULL;
static fude_wordsplit_slot* fude_wordsplit_table = NULL;
static u32                  fude_wordsplit_size  = 0;   // a power of two
static u32*                 fude_wordsplit_rank  = NULL; // per word: how common ('WFRQ', high half) then how soon an example (low): lower wins
static u32*                 fude_wordsplit_alike = NULL; // per word: the next spelt the same (UINT32_MAX: none) — from the commonest
static fude_wordsplit_rule  fude_wordsplit_rules[FUDE_WORDSPLIT_RULES];
static u32                  fude_wordsplit_rule_count = 0;

RDE_INTERNAL u32 fude_wordsplit_hash(const c8* _s, usize _n) {
    u32 _h = 2166136261u;
    for(usize _i = 0; _i < _n; _i++) {
        _h = (_h ^ (u8)_s[_i]) * 16777619u;
    }
    return _h != 0u ? _h : 1u;
}

RDE_INTERNAL b8 fude_wordsplit_is_kanji(u32 _cp) {
    return (_cp >= 0x4E00u && _cp <= 0x9FFFu) || (_cp >= 0x3400u && _cp <= 0x4DBFu) || (_cp >= 0xF900u && _cp <= 0xFAFFu);
}

RDE_INTERNAL b8 fude_wordsplit_is_hiragana(u32 _cp) {
    return _cp >= 0x3041u && _cp <= 0x309Fu;
}

// は が を に で と も の へ や: the particles a word is followed by.
RDE_INTERNAL b8 fude_wordsplit_is_particle(u32 _cp) {
    return _cp == 0x306Fu || _cp == 0x304Cu || _cp == 0x3092u || _cp == 0x306Bu || _cp == 0x3067u ||
           _cp == 0x3068u || _cp == 0x3082u || _cp == 0x306Eu || _cp == 0x3078u || _cp == 0x3084u;
}

RDE_INTERNAL u32 fude_wordsplit_cps(const c8* _s) {
    u32 _n = 0;
    while(fude_utf8_next(&_s) != 0) {
        _n++;
    }
    return _n;
}

RDE_INTERNAL void fude_wordsplit_add_rule(const c8* _in_a, const c8* _in_b, const c8* _out) {
    if(fude_wordsplit_rule_count == FUDE_WORDSPLIT_RULES) {
        return;
    }
    fude_wordsplit_rule* _r = &fude_wordsplit_rules[fude_wordsplit_rule_count++];
    snprintf(_r->in, sizeof(_r->in), "%s%s", _in_a, _in_b);
    snprintf(_r->out, sizeof(_r->out), "%s", _out);
    _r->in_cps = (u8)fude_wordsplit_cps(_r->in);
}

// The endings undone. Godan verbs: the i-row before ます / たい...; the te and ta
// forms; the a-row before ない. Ichidan verbs (their stem keeps its e / i kana):
// る after the ending. I-adjectives: かった, くない, くて, く, ければ, さ.
RDE_INTERNAL void fude_wordsplit_rules_make(void) {
    fude_wordsplit_rule_count = 0;
    static const c8* const _i_row[9][2] = { { "き", "く" }, { "ぎ", "ぐ" }, { "し", "す" }, { "ち", "つ" }, { "に", "ぬ" },
                                            { "び", "ぶ" }, { "み", "む" }, { "り", "る" }, { "い", "う" } };
    static const c8* const _a_row[9][2] = { { "か", "く" }, { "が", "ぐ" }, { "さ", "す" }, { "た", "つ" }, { "な", "ぬ" },
                                            { "ば", "ぶ" }, { "ま", "む" }, { "ら", "る" }, { "わ", "う" } };
    static const c8* const _polite[] = { "ます", "ました", "ません", "ませんでした", "たい", "たかった", "たくない", "ながら", "まして", "ましょう", "なさい" };
    static const c8* const _negative[] = { "ない", "なかった", "なくて", "ず", "せる", "れる" };
    for(u32 _k = 0; _k < 9u; _k++) {
        for(u32 _p = 0; _p < sizeof(_polite) / sizeof(_polite[0]); _p++) {
            fude_wordsplit_add_rule(_i_row[_k][0], _polite[_p], _i_row[_k][1]);
        }
        for(u32 _p = 0; _p < sizeof(_negative) / sizeof(_negative[0]); _p++) {
            fude_wordsplit_add_rule(_a_row[_k][0], _negative[_p], _a_row[_k][1]);
        }
    }
    static const c8* const _te[][2] = { { "いて", "く" }, { "いた", "く" }, { "いで", "ぐ" }, { "いだ", "ぐ" }, { "して", "す" }, { "した", "す" },
                                        { "って", "う" }, { "った", "う" }, { "って", "つ" }, { "った", "つ" }, { "って", "る" }, { "った", "る" },
                                        { "んで", "む" }, { "んだ", "む" }, { "んで", "ぶ" }, { "んだ", "ぶ" }, { "んで", "ぬ" }, { "んだ", "ぬ" },
                                        { "って", "く" }, { "った", "く" } };   // 行って, 行った
    for(u32 _k = 0; _k < sizeof(_te) / sizeof(_te[0]); _k++) {
        fude_wordsplit_add_rule(_te[_k][0], "", _te[_k][1]);
    }
    static const c8* const _ichidan[] = { "ます", "ました", "ません", "ませんでした", "た", "て", "ない", "なかった", "なくて", "たい", "たかった",
                                          "られる", "させる", "よう", "ろ", "れば", "ましょう", "ながら", "なさい" };
    for(u32 _k = 0; _k < sizeof(_ichidan) / sizeof(_ichidan[0]); _k++) {
        fude_wordsplit_add_rule(_ichidan[_k], "", "る");
    }
    static const c8* const _adjective[] = { "かった", "くない", "くなかった", "くて", "く", "ければ", "さ", "そう" };
    for(u32 _k = 0; _k < sizeof(_adjective) / sizeof(_adjective[0]); _k++) {
        fude_wordsplit_add_rule(_adjective[_k], "", "い");
    }
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
    if(fude_wordsplit_rule_count == 0) {
        fude_wordsplit_rules_make();
    }
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
    // Lines joined: Japanese has no spaces, and a line break (handwriting that
    // went on below) is never inside a word on purpose (読みま / した).
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
    c8  _form[128];
    for(u32 _i = 0; _i < _n && _found < _max;) {
        u32 _best      = UINT32_MAX;
        u32 _best_take = 0;   // characters of the text it covers
        const u32 _look = _n - _i < FUDE_WORDSPLIT_LOOK ? _n - _i : FUDE_WORDSPLIT_LOOK;
        for(u32 _len = _look; _len >= 1u; _len--) {
            b8 _kanji = false;
            for(u32 _k = _i; _k < _i + _len; _k++) {
                _kanji |= fude_wordsplit_is_kanji(_cps[_k]);
            }
            if(!_kanji) {
                continue;   // only words with a kanji
            }
            const c8*   _span = _text + _at[_i];
            const usize _span_bytes = _at[_i + _len] - _at[_i];
            // As written. A word that is a shorter one with a particle after it
            // (今日は, the greeting, in 今日は日本語を...) is taken as the shorter one:
            // the particle is the sentence's, far more often than not.
            const u32 _exact = fude_wordsplit_find(_db, _span, _span_bytes);
            if(_exact != UINT32_MAX && _len > _best_take) {
                const b8 _particle = _len >= 2u && fude_wordsplit_is_particle(_cps[_i + _len - 1u]) &&
                                     fude_wordsplit_find(_db, _span, _at[_i + _len - 1u] - _at[_i]) != UINT32_MAX;
                if(!_particle) {
                    _best      = _exact;
                    _best_take = _len;
                }
            }
            // Conjugated: an ending of the hiragana after it undone.
            u32 _run = 0;
            while(_i + _len + _run < _n && fude_wordsplit_is_hiragana(_cps[_i + _len + _run])) {
                _run++;
            }
            if(_run == 0 || _span_bytes + 8u >= sizeof(_form)) {
                continue;
            }
            const c8*   _after       = _text + _at[_i + _len];
            const usize _after_bytes = _at[_i + _len + _run] - _at[_i + _len];
            for(u32 _r = 0; _r < fude_wordsplit_rule_count; _r++) {
                const fude_wordsplit_rule* _rule = &fude_wordsplit_rules[_r];
                const usize                _in   = strlen(_rule->in);
                if(_rule->in_cps > _run || _in > _after_bytes || memcmp(_after, _rule->in, _in) != 0 || _len + _rule->in_cps <= _best_take) {
                    continue;
                }
                memcpy(_form, _span, _span_bytes);
                snprintf(_form + _span_bytes, sizeof(_form) - _span_bytes, "%s", _rule->out);
                const u32 _word = fude_wordsplit_find(_db, _form, strlen(_form));
                if(_word != UINT32_MAX) {
                    _best      = _word;
                    _best_take = _len + _rule->in_cps;
                }
            }
        }
        if(_best == UINT32_MAX) {
            _i++;
            continue;
        }
        // The word — and any spelt the same that are as common (本: ほん and もと;
        // the data cannot tell them apart: the learner can).
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
