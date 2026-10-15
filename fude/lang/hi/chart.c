// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"
#include "drawing/base/text.h"

// ===========================================================================
// See lang.h. The Hindi chart (fude_lang_chart): Devanagari's vowels; its consonants in their five groups by where in
// the mouth they are said (क ख ग घ ङ …), then the rest and the letters with a nukta; the vowel signs and the other
// marks; the digits. Each cell is a letter drawn from its strokes, with its Latin name under it.
//
// A row is one group. On a narrow screen a row wraps onto the next line (cells are at least 64 points), and the next
// group still starts a line of its own.
// ===========================================================================

// The chart's sections.
enum { HI_VOWELS = 0, HI_CONSONANTS, HI_MARKS, HI_DIGITS };

// Each section's title, jump button and the button's icon (one of its letters).
static const fude_lang_chart_section HI_CHART_SECTIONS[] = {
    { FUDE_TEXT_HI_VOWELS, FUDE_TEXT_HI_VOWELS, "\xE0\xA4\x85", false },   // अ
    { FUDE_TEXT_HI_CONSONANTS, FUDE_TEXT_HI_CONSONANTS, "\xE0\xA4\x95", false },   // क
    { FUDE_TEXT_HI_SIGNS, FUDE_TEXT_HI_SIGNS, "\xE0\xA4\xBE", false },   // ा
    { FUDE_TEXT_HI_DIGITS, FUDE_TEXT_HI_DIGITS, "\xE0\xA5\xA7", false },   // १
};

// The rows, section by section (0 is an empty cell).
static const fude_lang_chart_row HI_CHART_ROWS[] = {
    { HI_VOWELS, 0u, 0u, { 0x0905, 0x0906, 0x0907, 0x0908, 0x0909, 0x090A } },   // अ आ इ ई उ ऊ
    { HI_VOWELS, 0u, 0u, { 0x090B, 0x090F, 0x0910, 0x0913, 0x0914, 0x0911 } },   // ऋ ए ऐ ओ औ ऑ
    { HI_CONSONANTS, 0u, 0u, { 0x0915, 0x0916, 0x0917, 0x0918, 0x0919 } },   // क ख ग घ ङ
    { HI_CONSONANTS, 0u, 0u, { 0x091A, 0x091B, 0x091C, 0x091D, 0x091E } },   // च छ ज झ ञ
    { HI_CONSONANTS, 0u, 0u, { 0x091F, 0x0920, 0x0921, 0x0922, 0x0923 } },   // ट ठ ड ढ ण
    { HI_CONSONANTS, 0u, 0u, { 0x0924, 0x0925, 0x0926, 0x0927, 0x0928 } },   // त थ द ध न
    { HI_CONSONANTS, 0u, 0u, { 0x092A, 0x092B, 0x092C, 0x092D, 0x092E } },   // प फ ब भ म
    { HI_CONSONANTS, 0u, 0u, { 0x092F, 0x0930, 0x0932, 0x0935 } },   // य र ल व
    { HI_CONSONANTS, 0u, 0u, { 0x0936, 0x0937, 0x0938, 0x0939 } },   // श ष स ह
    { HI_CONSONANTS, 0u, 0u, { 0x0958, 0x0959, 0x095A, 0x095B, 0x095E } },   // क़ ख़ ग़ ज़ फ़
    { HI_CONSONANTS, 0u, 0u, { 0x095C, 0x095D } },   // ड़ ढ़
    { HI_MARKS, 0u, 0u, { 0x093E, 0x093F, 0x0940, 0x0941, 0x0942, 0x0943 } },   // ◌ा ◌ि ◌ी ◌ु ◌ू ◌ृ
    { HI_MARKS, 0u, 0u, { 0x0947, 0x0948, 0x094B, 0x094C, 0x0949 } },   // ◌े ◌ै ◌ो ◌ौ ◌ॉ
    { HI_MARKS, 0u, 0u, { 0x0902, 0x0901, 0x0903, 0x093C, 0x094D } },   // ◌ं ◌ँ ◌ः ◌़ ◌्
    { HI_MARKS, 0u, 0u, { 0x093D, 0x0950, 0x0964 } },   // ऽ ॐ ।
    { HI_DIGITS, 0u, 0u, { 0x0966, 0x0967, 0x0968, 0x0969, 0x096A } },   // ० १ २ ३ ४
    { HI_DIGITS, 0u, 0u, { 0x096B, 0x096C, 0x096D, 0x096E, 0x096F } },   // ५ ६ ७ ८ ९
};

// The chart's definition: the widest row has 6 letters, and rows wrap rather than make cells under 64 points.
static const fude_lang_chart_def HI_CHART = {
    .sections      = HI_CHART_SECTIONS,
    .section_count = (u32)(sizeof(HI_CHART_SECTIONS) / sizeof(HI_CHART_SECTIONS[0])),
    .rows          = HI_CHART_ROWS,
    .row_count     = (u32)(sizeof(HI_CHART_ROWS) / sizeof(HI_CHART_ROWS[0])),
    .columns       = 6u,
    .cell_min      = 64.0f,
    .flow          = false,
    .keep_gaps     = false,
    .section_gap   = 20.0f,
};

// Returns the Hindi chart, a static table.
const fude_lang_chart_def* fude_lang_chart(void) {
    return &HI_CHART;
}
