// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"
#include "drawing/base/text.h"
#include "lang/ko/hangul.h"

// ===========================================================================
// See lang.h. The Hangul chart (fude_lang_chart): the consonants (the fourteen, then the five doubled ones), the vowels
// (the ten, then the eleven made from them), and the syllable table every Korean child learns (반절표), in which each
// consonant's row runs across the ten vowels: 가 갸 거 겨 고 교 구 규 그 기. Each cell is drawn from its strokes, with
// its romanization under it.
//
// The syllable table is made, not listed: each syllable is composed from its consonant and vowel, with no final
// (hangul.h).
// ===========================================================================

// The chart's sections.
enum { KO_CONSONANTS = 0, KO_VOWELS, KO_SYLLABLES };

// Each section's title, jump button and the button's icon (one of its letters).
static const fude_lang_chart_section KO_CHART_SECTIONS[] = {
    { FUDE_TEXT_CHART_CONSONANTS, FUDE_TEXT_CHART_CONSONANTS, "\xE3\x84\xB1", false },   // ㄱ
    { FUDE_TEXT_CHART_VOWELS,     FUDE_TEXT_CHART_VOWELS,     "\xE3\x85\x8F", false },   // ㅏ
    { FUDE_TEXT_CHART_SYLLABLES,  FUDE_TEXT_KO_SYLLABLES,     "\xEA\xB0\x80", false },   // 가
};

// The consonants' and the vowels' rows, as compatibility jamo (0 is an empty cell).
static const fude_lang_chart_row KO_CHART_ROWS[] = {
    // The consonants: the fourteen, then the five doubled.
    { KO_CONSONANTS, 0u, 0u, { 0x3131, 0x3134, 0x3137, 0x3139, 0x3141, 0x3142, 0x3145 } },   // ㄱ ㄴ ㄷ ㄹ ㅁ ㅂ ㅅ
    { KO_CONSONANTS, 0u, 0u, { 0x3147, 0x3148, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E } },   // ㅇ ㅈ ㅊ ㅋ ㅌ ㅍ ㅎ
    { KO_CONSONANTS, 0u, 0u, { 0x3132, 0x3138, 0x3143, 0x3146, 0x3149 } },                   // ㄲ ㄸ ㅃ ㅆ ㅉ
    // The vowels: the ten, then the eleven made from them.
    { KO_VOWELS, 0u, 0u, { 0x314F, 0x3151, 0x3153, 0x3155, 0x3157, 0x315B, 0x315C, 0x3160, 0x3161, 0x3163 } },   // ㅏ ㅑ ㅓ ㅕ ㅗ ㅛ ㅜ ㅠ ㅡ ㅣ
    { KO_VOWELS, 0u, 0u, { 0x3150, 0x3152, 0x3154, 0x3156, 0x3158, 0x3159, 0x315A, 0x315D, 0x315E, 0x315F } },   // ㅐ ㅒ ㅔ ㅖ ㅘ ㅙ ㅚ ㅝ ㅞ ㅟ
    { KO_VOWELS, 0u, 0u, { 0x3162 } },                                                                           // ㅢ
};

// The syllable table's consonants, as a syllable's initial numbers in Unicode's order, and its vowels, as its medial
// numbers (hangul.h). They are listed in the order the table is taught, not in Unicode's.
static const u8 KO_CHART_INITIALS[] = { 0, 2, 3, 5, 6, 7, 9, 11, 12, 14, 15, 16, 17, 18, 1, 4, 8, 10, 13 };   // ㄱ ㄴ ㄷ ... ㅎ, then ㄲ ㄸ ㅃ ㅆ ㅉ.
static const u8 KO_CHART_MEDIALS[]  = { 0, 2, 4, 6, 8, 12, 13, 17, 18, 20 };                                  // ㅏ ㅑ ㅓ ㅕ ㅗ ㅛ ㅜ ㅠ ㅡ ㅣ

// The number of items in the static array _t.
#define KO_CHART_COUNT(_t) ((u32)(sizeof(_t) / sizeof((_t)[0])))

// Fills in row _r of the chart: first the listed rows, then one row of the syllable table for each consonant, made by
// composing it with each vowel. Returns false past the last row.
RDE_INTERNAL b8 fude_ko_chart_row(u32 _r, fude_lang_chart_row* _out) {
    if(_r < KO_CHART_COUNT(KO_CHART_ROWS)) {
        *_out = KO_CHART_ROWS[_r];
        return true;
    }
    _r -= KO_CHART_COUNT(KO_CHART_ROWS);
    if(_r >= KO_CHART_COUNT(KO_CHART_INITIALS)) {
        return false;
    }
    rde_memset(_out, 0, sizeof(*_out));
    _out->section = KO_SYLLABLES;
    for(u32 _v = 0; _v < KO_CHART_COUNT(KO_CHART_MEDIALS); _v++) {
        _out->cp[_v] = fude_ko_compose(KO_CHART_INITIALS[_r], KO_CHART_MEDIALS[_v], 0u);
    }
    return true;
}

// The chart's definition.
static const fude_lang_chart_def KO_CHART = {
    .sections      = KO_CHART_SECTIONS,
    .section_count = KO_CHART_COUNT(KO_CHART_SECTIONS),
    .row           = fude_ko_chart_row,
    .columns       = 10u,        // The syllable table's ten vowels make the widest row.
    .cell_min      = 0.0f,       // The cells shrink on a narrow screen, so a row of the table never wraps.
    .flow          = false,
    .keep_gaps     = true,
    .section_gap   = 20.0f,
};

// Returns the Hangul chart, a static table.
const fude_lang_chart_def* fude_lang_chart(void) {
    return &KO_CHART;
}
