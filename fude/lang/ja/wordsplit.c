// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"

#include "drawing/base/utf8.h"
#include "study/chars/wordsplit.h"

// ===========================================================================
// See lang.h. Japanese's words (fude_lang_words), found by a matcher of its own. Like the study's splitter
// (study/chars/wordsplit.h) it takes the longest word first, from the left, but a word may be inflected. Only stretches
// with a kanji are looked up: particles and words in kana are passed over, because the learner reads those and the
// dictionary has none of them.
//
// Conjugations are undone by rules, as pop-up dictionaries do. After a stretch that has a kanji, the hiragana that
// follow are tried against a list of endings, each with what the dictionary form ends in instead (ました → る,
// きたい → く, かった → い...). The match that covers the most of the text wins, so 行きたい is 行く, not the noun 行き,
// and 面白かった is 面白い, not 面白.
//
// The rules are built the first time they are needed (fude_ja_rules_make), into a static table, without a lock: the
// matcher runs on the main thread only (lang.h).
// ===========================================================================

#define FUDE_JA_LOOK  10u    // The most characters tried from each place.
#define FUDE_JA_RULES 512u   // The most endings the table holds; fude_ja_rules_make makes about 200.

// One rule: an ending as it is in the text, and what the dictionary form ends in instead, both in UTF-8 hiragana.
typedef struct {
    c8  in[24];   // The ending in the text (hiragana)...
    c8  out[8];   // ...and what the dictionary form ends in instead.
    u32 in_cps;   // How many characters in has.
    u32 in_bytes;
    u32 out_bytes;
} fude_ja_rule;

// The rules, built on first use; a count of 0 means they are not built yet.
static fude_ja_rule fude_ja_rules[FUDE_JA_RULES];
static u32          fude_ja_rule_count = 0;

// Returns whether _cp is in the hiragana block (U+3041..U+309F).
RDE_INTERNAL b8 fude_ja_is_hiragana(u32 _cp) {
    return _cp >= 0x3041u && _cp <= 0x309Fu;
}

// Returns whether _cp is one of the particles that follow a word: は が を に で と も の へ や.
RDE_INTERNAL b8 fude_ja_is_particle(u32 _cp) {
    return _cp == 0x306Fu || _cp == 0x304Cu || _cp == 0x3092u || _cp == 0x306Bu || _cp == 0x3067u ||
           _cp == 0x3068u || _cp == 0x3082u || _cp == 0x306Eu || _cp == 0x3078u || _cp == 0x3084u;
}

// Adds the rule "_in_a followed by _in_b becomes _out" to the table, with its lengths worked out. A rule past
// FUDE_JA_RULES is dropped.
RDE_INTERNAL void fude_ja_add_rule(const c8* _in_a, const c8* _in_b, const c8* _out) {
    if(fude_ja_rule_count == FUDE_JA_RULES) {
        return;
    }
    fude_ja_rule* _r = &fude_ja_rules[fude_ja_rule_count++];
    rde_snprintf(_r->in, sizeof(_r->in), "%s%s", _in_a, _in_b);
    rde_snprintf(_r->out, sizeof(_r->out), "%s", _out);
    _r->in_cps    = fude_utf8_count(_r->in);
    _r->in_bytes  = (u32)rde_strlen(_r->in);
    _r->out_bytes = (u32)rde_strlen(_r->out);
}

// Builds the table of endings to undo. Godan verbs: the i-row kana before ます, たい and the like, and the a-row kana
// before ない and the like, each made for every row (き → く, か → く...); then the te and ta forms. Ichidan verbs keep
// their e or i kana in the stem, so their ending is simply replaced by る. I-adjectives: かった, くない, くて, く,
// ければ, さ and the like are replaced by い.
RDE_INTERNAL void fude_ja_rules_make(void) {
    fude_ja_rule_count = 0;
    static const c8* const _i_row[9][2] = { { "き", "く" }, { "ぎ", "ぐ" }, { "し", "す" }, { "ち", "つ" }, { "に", "ぬ" },
                                            { "び", "ぶ" }, { "み", "む" }, { "り", "る" }, { "い", "う" } };
    static const c8* const _a_row[9][2] = { { "か", "く" }, { "が", "ぐ" }, { "さ", "す" }, { "た", "つ" }, { "な", "ぬ" },
                                            { "ば", "ぶ" }, { "ま", "む" }, { "ら", "る" }, { "わ", "う" } };
    static const c8* const _polite[] = { "ます", "ました", "ません", "ませんでした", "たい", "たかった", "たくない", "ながら", "まして", "ましょう", "なさい" };
    static const c8* const _negative[] = { "ない", "なかった", "なくて", "ず", "せる", "れる" };
    for(u32 _k = 0; _k < 9u; _k++) {
        for(u32 _p = 0; _p < sizeof(_polite) / sizeof(_polite[0]); _p++) {
            fude_ja_add_rule(_i_row[_k][0], _polite[_p], _i_row[_k][1]);
        }
        for(u32 _p = 0; _p < sizeof(_negative) / sizeof(_negative[0]); _p++) {
            fude_ja_add_rule(_a_row[_k][0], _negative[_p], _a_row[_k][1]);
        }
    }
    static const c8* const _te[][2] = { { "いて", "く" }, { "いた", "く" }, { "いで", "ぐ" }, { "いだ", "ぐ" }, { "して", "す" }, { "した", "す" },
                                        { "って", "う" }, { "った", "う" }, { "って", "つ" }, { "った", "つ" }, { "って", "る" }, { "った", "る" },
                                        { "んで", "む" }, { "んだ", "む" }, { "んで", "ぶ" }, { "んだ", "ぶ" }, { "んで", "ぬ" }, { "んだ", "ぬ" },
                                        { "って", "く" }, { "った", "く" } };   // 行って and 行った, from 行く.
    for(u32 _k = 0; _k < sizeof(_te) / sizeof(_te[0]); _k++) {
        fude_ja_add_rule(_te[_k][0], "", _te[_k][1]);
    }
    static const c8* const _ichidan[] = { "ます", "ました", "ません", "ませんでした", "た", "て", "ない", "なかった", "なくて", "たい", "たかった",
                                          "られる", "させる", "よう", "ろ", "れば", "ましょう", "ながら", "なさい" };
    for(u32 _k = 0; _k < sizeof(_ichidan) / sizeof(_ichidan[0]); _k++) {
        fude_ja_add_rule(_ichidan[_k], "", "る");
    }
    static const c8* const _adjective[] = { "かった", "くない", "くなかった", "くて", "く", "ければ", "さ", "そう" };
    for(u32 _k = 0; _k < sizeof(_adjective) / sizeof(_adjective[0]); _k++) {
        fude_ja_add_rule(_adjective[_k], "", "い");
    }
}

// The matcher (fude_lang_words_def.match): finds the word at _line->cps[_at]. Of the stretches from _at that have a
// kanji, up to FUDE_JA_LOOK characters, it takes the match that covers the most of the text: a stretch as written, or
// a stretch with the hiragana after it read as a conjugation and undone. Returns the word, or UINT32_MAX, and sets
// *_take to the characters it covers (1 when there is no word).
RDE_INTERNAL u32 fude_ja_match(const fude_lang_line* _line, u32 _at, u32* _take) {
    if(fude_ja_rule_count == 0) {
        fude_ja_rules_make();
    }
    u32 _best      = UINT32_MAX;
    u32 _best_take = 0;   // How many characters of the text the best match covers.
    c8  _form[128];
    for(u32 _len = RDE_MIN(FUDE_JA_LOOK, _line->n - _at); _len >= 1u && _line->letter_at[_at] < _at + _len; _len--) {
        const c8*   _span       = _line->text + _line->at[_at];
        const usize _span_bytes = _line->at[_at + _len] - _line->at[_at];
        // The stretch as written. A word that is a shorter word with a particle after it (今日は, the greeting, in
        // 今日は日本語を...) is taken as the shorter word, because far more often than not the particle belongs to
        // the sentence.
        const u32 _exact = fude_wordsplit_find(_span, _span_bytes);
        if(_exact != UINT32_MAX && _len > _best_take) {
            const b8 _particle = _len >= 2u && fude_ja_is_particle(_line->cps[_at + _len - 1u]) &&
                                 fude_wordsplit_find(_span, _line->at[_at + _len - 1u] - _line->at[_at]) != UINT32_MAX;
            if(!_particle) {
                _best      = _exact;
                _best_take = _len;
            }
        }
        // Conjugated: try each rule whose ending starts the hiragana after the stretch, and look up the stretch with
        // the dictionary form's ending instead. A rule that cannot beat the best match is skipped.
        u32 _run = 0;
        while(_at + _len + _run < _line->n && fude_ja_is_hiragana(_line->cps[_at + _len + _run])) {
            _run++;
        }
        if(_run == 0 || _span_bytes + sizeof(fude_ja_rules[0].out) > sizeof(_form)) {
            continue;
        }
        const c8*   _after       = _line->text + _line->at[_at + _len];
        const usize _after_bytes = _line->at[_at + _len + _run] - _line->at[_at + _len];
        rde_memcpy(_form, _span, _span_bytes);
        for(u32 _r = 0; _r < fude_ja_rule_count; _r++) {
            const fude_ja_rule* _rule = &fude_ja_rules[_r];
            if(_rule->in_cps > _run || _rule->in_bytes > _after_bytes || _len + _rule->in_cps <= _best_take ||
               rde_memcmp(_after, _rule->in, _rule->in_bytes) != 0) {
                continue;
            }
            rde_memcpy(_form + _span_bytes, _rule->out, _rule->out_bytes);
            const u32 _word = fude_wordsplit_find(_form, _span_bytes + _rule->out_bytes);
            if(_word != UINT32_MAX) {
                _best      = _word;
                _best_take = _len + _rule->in_cps;
            }
        }
    }
    *_take = _best != UINT32_MAX ? _best_take : 1u;
    return _best;
}

// Returns Japanese's rules for finding words, a static table. Up to 3 words written alike are offered (本 is ほん and
// もと).
const fude_lang_words_def* fude_lang_words(void) {
    static const fude_lang_words_def _def = {
        .letter = fude_utf8_is_han,
        .match  = fude_ja_match,
        .look   = FUDE_JA_LOOK,
        .alike  = 3u,
    };
    return &_def;
}
