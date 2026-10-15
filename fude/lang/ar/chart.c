// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"
#include "drawing/base/text.h"

// ===========================================================================
// See lang.h. The Arabic chart (fude_lang_chart): each letter with the forms it takes in a word, written left to right
// as final, medial, initial and alone, so that, read from the right as Arabic is, the letter alone comes first and then
// where it joins; then hamza's letters, ta marbuta, alif maqsura and the lam-alif ligatures; the vowel marks; the
// digits.
//
// A column is always one form, so a form a letter does not have (ا has no initial) keeps its empty place. The letters'
// sections are marked mirrored: their rows are already laid out right to left, so a right-to-left screen keeps them as
// they are and they run from the right.
// ===========================================================================

// The chart's sections.
enum { AR_LETTERS = 0, AR_MORE, AR_MARKS, AR_DIGITS };

// Each section's title, jump button, the button's icon (one of its letters), and whether its rows are already laid out
// right to left.
static const fude_lang_chart_section AR_CHART_SECTIONS[] = {
    { FUDE_TEXT_AR_LETTERS, FUDE_TEXT_AR_LETTERS, "\xD8\xA8", true },   // ب
    { FUDE_TEXT_AR_MORE, FUDE_TEXT_AR_MORE, "\xD8\xA1", true },   // ء
    { FUDE_TEXT_AR_MARKS, FUDE_TEXT_AR_MARKS, "\xE2\x97\x8C\xD9\x8E", false },   // ◌َ
    { FUDE_TEXT_AR_DIGITS, FUDE_TEXT_AR_DIGITS, "\xD9\xA1", false },   // ١
};

// The rows, section by section (0 is an empty cell). In a letter's row the columns are its final, medial and initial
// presentation forms (U+FE70..U+FEFC), then the letter itself.
static const fude_lang_chart_row AR_CHART_ROWS[] = {
    { AR_LETTERS, 0u, 0u, { 0xFE8E, 0, 0, 0x0627 } },   // ﺎ · · ا
    { AR_LETTERS, 0u, 0u, { 0xFE90, 0xFE92, 0xFE91, 0x0628 } },   // ﺐ ﺒ ﺑ ب
    { AR_LETTERS, 0u, 0u, { 0xFE96, 0xFE98, 0xFE97, 0x062A } },   // ﺖ ﺘ ﺗ ت
    { AR_LETTERS, 0u, 0u, { 0xFE9A, 0xFE9C, 0xFE9B, 0x062B } },   // ﺚ ﺜ ﺛ ث
    { AR_LETTERS, 0u, 0u, { 0xFE9E, 0xFEA0, 0xFE9F, 0x062C } },   // ﺞ ﺠ ﺟ ج
    { AR_LETTERS, 0u, 0u, { 0xFEA2, 0xFEA4, 0xFEA3, 0x062D } },   // ﺢ ﺤ ﺣ ح
    { AR_LETTERS, 0u, 0u, { 0xFEA6, 0xFEA8, 0xFEA7, 0x062E } },   // ﺦ ﺨ ﺧ خ
    { AR_LETTERS, 0u, 0u, { 0xFEAA, 0, 0, 0x062F } },   // ﺪ · · د
    { AR_LETTERS, 0u, 0u, { 0xFEAC, 0, 0, 0x0630 } },   // ﺬ · · ذ
    { AR_LETTERS, 0u, 0u, { 0xFEAE, 0, 0, 0x0631 } },   // ﺮ · · ر
    { AR_LETTERS, 0u, 0u, { 0xFEB0, 0, 0, 0x0632 } },   // ﺰ · · ز
    { AR_LETTERS, 0u, 0u, { 0xFEB2, 0xFEB4, 0xFEB3, 0x0633 } },   // ﺲ ﺴ ﺳ س
    { AR_LETTERS, 0u, 0u, { 0xFEB6, 0xFEB8, 0xFEB7, 0x0634 } },   // ﺶ ﺸ ﺷ ش
    { AR_LETTERS, 0u, 0u, { 0xFEBA, 0xFEBC, 0xFEBB, 0x0635 } },   // ﺺ ﺼ ﺻ ص
    { AR_LETTERS, 0u, 0u, { 0xFEBE, 0xFEC0, 0xFEBF, 0x0636 } },   // ﺾ ﻀ ﺿ ض
    { AR_LETTERS, 0u, 0u, { 0xFEC2, 0xFEC4, 0xFEC3, 0x0637 } },   // ﻂ ﻄ ﻃ ط
    { AR_LETTERS, 0u, 0u, { 0xFEC6, 0xFEC8, 0xFEC7, 0x0638 } },   // ﻆ ﻈ ﻇ ظ
    { AR_LETTERS, 0u, 0u, { 0xFECA, 0xFECC, 0xFECB, 0x0639 } },   // ﻊ ﻌ ﻋ ع
    { AR_LETTERS, 0u, 0u, { 0xFECE, 0xFED0, 0xFECF, 0x063A } },   // ﻎ ﻐ ﻏ غ
    { AR_LETTERS, 0u, 0u, { 0xFED2, 0xFED4, 0xFED3, 0x0641 } },   // ﻒ ﻔ ﻓ ف
    { AR_LETTERS, 0u, 0u, { 0xFED6, 0xFED8, 0xFED7, 0x0642 } },   // ﻖ ﻘ ﻗ ق
    { AR_LETTERS, 0u, 0u, { 0xFEDA, 0xFEDC, 0xFEDB, 0x0643 } },   // ﻚ ﻜ ﻛ ك
    { AR_LETTERS, 0u, 0u, { 0xFEDE, 0xFEE0, 0xFEDF, 0x0644 } },   // ﻞ ﻠ ﻟ ل
    { AR_LETTERS, 0u, 0u, { 0xFEE2, 0xFEE4, 0xFEE3, 0x0645 } },   // ﻢ ﻤ ﻣ م
    { AR_LETTERS, 0u, 0u, { 0xFEE6, 0xFEE8, 0xFEE7, 0x0646 } },   // ﻦ ﻨ ﻧ ن
    { AR_LETTERS, 0u, 0u, { 0xFEEA, 0xFEEC, 0xFEEB, 0x0647 } },   // ﻪ ﻬ ﻫ ه
    { AR_LETTERS, 0u, 0u, { 0xFEEE, 0, 0, 0x0648 } },   // ﻮ · · و
    { AR_LETTERS, 0u, 0u, { 0xFEF2, 0xFEF4, 0xFEF3, 0x064A } },   // ﻲ ﻴ ﻳ ي
    { AR_MORE, 0u, 0u, { 0, 0, 0, 0x0621 } },   // · · · ء
    { AR_MORE, 0u, 0u, { 0xFE82, 0, 0, 0x0622 } },   // ﺂ · · آ
    { AR_MORE, 0u, 0u, { 0xFE84, 0, 0, 0x0623 } },   // ﺄ · · أ
    { AR_MORE, 0u, 0u, { 0xFE88, 0, 0, 0x0625 } },   // ﺈ · · إ
    { AR_MORE, 0u, 0u, { 0xFE86, 0, 0, 0x0624 } },   // ﺆ · · ؤ
    { AR_MORE, 0u, 0u, { 0xFE8A, 0xFE8C, 0xFE8B, 0x0626 } },   // ﺊ ﺌ ﺋ ئ
    { AR_MORE, 0u, 0u, { 0xFE94, 0, 0, 0x0629 } },   // ﺔ · · ة
    { AR_MORE, 0u, 0u, { 0xFEF0, 0, 0, 0x0649 } },   // ﻰ · · ى
    { AR_MORE, 0u, 0u, { 0xFEFC, 0, 0, 0xFEFB } },   // ﻼ · · ﻻ
    { AR_MORE, 0u, 0u, { 0xFEF6, 0, 0, 0xFEF5 } },   // ﻶ · · ﻵ
    { AR_MORE, 0u, 0u, { 0xFEF8, 0, 0, 0xFEF7 } },   // ﻸ · · ﻷ
    { AR_MORE, 0u, 0u, { 0xFEFA, 0, 0, 0xFEF9 } },   // ﻺ · · ﻹ
    { AR_MARKS, 0u, 0u, { 0x064E, 0x064F, 0x0650, 0x0652 } },   // ◌َ ◌ُ ◌ِ ◌ْ
    { AR_MARKS, 0u, 0u, { 0x0651, 0x064B, 0x064C, 0x064D } },   // ◌ّ ◌ً ◌ٌ ◌ٍ
    { AR_DIGITS, 0u, 0u, { 0x0660, 0x0661, 0x0662, 0x0663, 0x0664 } },   // ٠ ١ ٢ ٣ ٤
    { AR_DIGITS, 0u, 0u, { 0x0665, 0x0666, 0x0667, 0x0668, 0x0669 } },   // ٥ ٦ ٧ ٨ ٩
};

// The chart's definition: the widest row has 5 letters (the digits), empty cells keep their place, and rows wrap
// rather than make cells under 64 points.
static const fude_lang_chart_def AR_CHART = {
    .sections      = AR_CHART_SECTIONS,
    .section_count = (u32)(sizeof(AR_CHART_SECTIONS) / sizeof(AR_CHART_SECTIONS[0])),
    .rows          = AR_CHART_ROWS,
    .row_count     = (u32)(sizeof(AR_CHART_ROWS) / sizeof(AR_CHART_ROWS[0])),
    .columns       = 5u,
    .cell_min      = 64.0f,
    .flow          = false,
    .keep_gaps     = true,
    .section_gap   = 20.0f,
};

// Returns the Arabic chart, a static table.
const fude_lang_chart_def* fude_lang_chart(void) {
    return &AR_CHART;
}
