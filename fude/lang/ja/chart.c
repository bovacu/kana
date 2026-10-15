// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"
#include "lang/ja/romaji.h"
#include "drawing/base/text.h"

// ===========================================================================
// See lang.h. The kana chart (fude_lang_chart): all of hiragana, then all of katakana, each as the classic gojūon
// table, with the columns あ か さ た な は ま や ら わ ん, each read down a i u e o. Each is followed by the voiced columns (が ざ
// だ ば ぱ, with the ゛ and ゜ marks) and the small kana, under a subtitle of their own. Each cell is drawn from its
// strokes, with its romaji under it.
//
// The rows come from romaji.h's table, which has two blocks of hiragana: a block's five rows are its a, i, u, e and o
// lines across its columns, and the viewer walks a block down each column, as the table is read. Katakana is the same
// table moved by FUDE_ROMAJI_KATAKANA_SHIFT.
// ===========================================================================

// The chart's two sections, and how many there are.
enum { JA_HIRAGANA = 0, JA_KATAKANA, JA_SECTIONS };

// Each section's title, jump button and the button's icon (its first kana).
static const fude_lang_chart_section JA_CHART_SECTIONS[] = {
    { FUDE_TEXT_HIRAGANA, FUDE_TEXT_HIRAGANA, "\xE3\x81\x82", false },   // あ
    { FUDE_TEXT_KATAKANA, FUDE_TEXT_KATAKANA, "\xE3\x82\xA2", false },   // ア
};

// Fills in row _r of the chart. The rows go section by section (hiragana, then katakana), then block by block (the
// gojūon, then the voiced and small kana), then the block's five lines, a to o. The second block's first row carries
// its subtitle. Returns false past the last row.
RDE_INTERNAL b8 fude_ja_chart_row(u32 _r, fude_lang_chart_row* _out) {
    const u32 _section = _r / (2u * FUDE_ROMAJI_ROWS);
    const u32 _block   = (_r / FUDE_ROMAJI_ROWS) % 2u;
    const u32 _line    = _r % FUDE_ROMAJI_ROWS;
    if(_section >= JA_SECTIONS) {
        return false;
    }
    rde_memset(_out, 0, sizeof(*_out));
    _out->section = _section;
    _out->flags   = FUDE_LANG_CHART_ROW_BY_COLUMN;
    if(_block == 1u && _line == 0u) {
        _out->flags   |= FUDE_LANG_CHART_ROW_SUBTITLE;
        _out->subtitle = FUDE_TEXT_CHART_VOICED;
    }
    u32                       _columns;
    const fude_romaji_column* _table = fude_romaji_block(_block, &_columns);
    const u32                 _shift = _section == JA_KATAKANA ? FUDE_ROMAJI_KATAKANA_SHIFT : 0u;
    for(u32 _c = 0; _c < _columns && _c < FUDE_LANG_CHART_COLUMNS; _c++) {
        _out->cp[_c] = _table[_c].cp[_line] != 0u ? _table[_c].cp[_line] + _shift : 0u;
    }
    return true;
}

// Returns how many columns the widest block has (the gojūon's), which fixes the cells' size.
RDE_INTERNAL u32 fude_ja_chart_columns(void) {
    u32 _n;
    fude_romaji_block(0u, &_n);
    return _n;
}

// Returns the kana chart, a static table. Its number of columns is filled in from romaji.h's table on every call.
const fude_lang_chart_def* fude_lang_chart(void) {
    static fude_lang_chart_def _chart = {
        .sections      = JA_CHART_SECTIONS,
        .section_count = JA_SECTIONS,
        .row           = fude_ja_chart_row,
        .cell_min      = 0.0f,    // The cells shrink on a narrow screen, so a line of the table never wraps.
        .flow          = false,
        .keep_gaps     = true,    // An empty cell keeps its column, so the table keeps its shape.
        .section_gap   = 40.0f,   // Twice the other charts' gap, because hiragana and katakana are two tables.
    };
    _chart.columns = fude_ja_chart_columns();   // As romaji.h's table says.
    return &_chart;
}
