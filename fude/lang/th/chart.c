// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"
#include "drawing/base/text.h"

// ===========================================================================
// See lang.h. The Thai chart (fude_lang_chart): the 44 consonants, the two no longer used (ฃ ฅ) among them, in the
// order every Thai child learns them, ก ข ฃ ค ... ฮ; the vowels, written above, below, before or after the consonant;
// the tone marks and other signs; the digits. Each cell is a letter drawn from its strokes, with its name under it (ก
// is "ko kai").
//
// A row is only a group of eight, so on a narrow screen a section's letters flow on, line after line.
// ===========================================================================

// The chart's sections.
enum { TH_CONSONANTS = 0, TH_VOWELS, TH_MARKS, TH_DIGITS };

// Each section's title, jump button and the button's icon (one of its letters).
static const fude_lang_chart_section TH_CHART_SECTIONS[] = {
    { FUDE_TEXT_TH_CONSONANTS, FUDE_TEXT_TH_CONSONANTS, "\xE0\xB8\x81", false },   // ก
    { FUDE_TEXT_TH_VOWELS, FUDE_TEXT_TH_VOWELS, "\xE0\xB8\xB2", false },   // า
    { FUDE_TEXT_TH_MARKS, FUDE_TEXT_TH_MARKS, "\xE0\xB9\x88", false },   // ่
    { FUDE_TEXT_TH_DIGITS, FUDE_TEXT_TH_DIGITS, "\xE0\xB9\x91", false },   // ๑
};

// The rows, section by section (0 is an empty cell).
static const fude_lang_chart_row TH_CHART_ROWS[] = {
    { TH_CONSONANTS, 0u, 0u, { 0x0E01, 0x0E02, 0x0E03, 0x0E04, 0x0E05, 0x0E06, 0x0E07, 0x0E08 } },   // ก ข ฃ ค ฅ ฆ ง จ
    { TH_CONSONANTS, 0u, 0u, { 0x0E09, 0x0E0A, 0x0E0B, 0x0E0C, 0x0E0D, 0x0E0E, 0x0E0F, 0x0E10 } },   // ฉ ช ซ ฌ ญ ฎ ฏ ฐ
    { TH_CONSONANTS, 0u, 0u, { 0x0E11, 0x0E12, 0x0E13, 0x0E14, 0x0E15, 0x0E16, 0x0E17, 0x0E18 } },   // ฑ ฒ ณ ด ต ถ ท ธ
    { TH_CONSONANTS, 0u, 0u, { 0x0E19, 0x0E1A, 0x0E1B, 0x0E1C, 0x0E1D, 0x0E1E, 0x0E1F, 0x0E20 } },   // น บ ป ผ ฝ พ ฟ ภ
    { TH_CONSONANTS, 0u, 0u, { 0x0E21, 0x0E22, 0x0E23, 0x0E25, 0x0E27, 0x0E28, 0x0E29, 0x0E2A } },   // ม ย ร ล ว ศ ษ ส
    { TH_CONSONANTS, 0u, 0u, { 0x0E2B, 0x0E2C, 0x0E2D, 0x0E2E } },   // ห ฬ อ ฮ
    { TH_VOWELS, 0u, 0u, { 0x0E30, 0x0E31, 0x0E32, 0x0E33, 0x0E34, 0x0E35, 0x0E36, 0x0E37 } },   // ะ ◌ั ◌า ◌ำ ◌ิ ◌ี ◌ึ ◌ื
    { TH_VOWELS, 0u, 0u, { 0x0E38, 0x0E39, 0x0E40, 0x0E41, 0x0E42, 0x0E43, 0x0E44, 0x0E47 } },   // ◌ุ ◌ู เ แ โ ใ ไ ◌็
    { TH_VOWELS, 0u, 0u, { 0x0E24, 0x0E26, 0x0E45 } },   // ฤ ฦ ๅ
    { TH_MARKS, 0u, 0u, { 0x0E48, 0x0E49, 0x0E4A, 0x0E4B, 0x0E4C, 0x0E2F, 0x0E46, 0x0E3A } },   // ◌่ ◌้ ◌๊ ◌๋ ◌์ ฯ ◌ๆ ◌ฺ
    { TH_MARKS, 0u, 0u, { 0x0E4D, 0x0E4E } },   // ◌ํ ◌๎
    { TH_DIGITS, 0u, 0u, { 0x0E50, 0x0E51, 0x0E52, 0x0E53, 0x0E54 } },   // ๐ ๑ ๒ ๓ ๔
    { TH_DIGITS, 0u, 0u, { 0x0E55, 0x0E56, 0x0E57, 0x0E58, 0x0E59 } },   // ๕ ๖ ๗ ๘ ๙
};

// The chart's definition: rows of up to 8 letters whose letters flow on, in cells of at least 64 points.
static const fude_lang_chart_def TH_CHART = {
    .sections      = TH_CHART_SECTIONS,
    .section_count = (u32)(sizeof(TH_CHART_SECTIONS) / sizeof(TH_CHART_SECTIONS[0])),
    .rows          = TH_CHART_ROWS,
    .row_count     = (u32)(sizeof(TH_CHART_ROWS) / sizeof(TH_CHART_ROWS[0])),
    .columns       = 8u,
    .cell_min      = 64.0f,
    .flow          = true,
    .keep_gaps     = false,
    .section_gap   = 20.0f,
};

// Returns the Thai chart, a static table.
const fude_lang_chart_def* fude_lang_chart(void) {
    return &TH_CHART;
}
