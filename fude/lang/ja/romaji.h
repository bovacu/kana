// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ROMAJI
#define FUDE_ROMAJI

#include "rde.h"

// ===========================================================================
// Romaji: the kana and their romaji. It holds the gojūon table, in the order the kana chart lays it out, and a short
// name for each kana mark the table leaves out, and it turns typed romaji into hiragana.
//
// Where it sits: it is Japanese's own, in the language layer (fude/lang/ja), and only Japanese's files use it: lang.c
// for the kana's Latin names, the core kana and readings typed in romaji, chart.c for the kana chart, and bake.c to
// romanize readings. An app that teaches another language has that language's folder instead.
//
// Its main types and their lifecycle: fude_romaji_column, one column of the table. The tables are constant and last
// as long as the program; nothing is allocated or freed.
//
// Threads: the app calls it on its main thread. Every function only reads constant tables and writes to what it is
// given, so any thread could call it.
//
// Design: the table holds hiragana only. Unicode puts each katakana a fixed distance from its hiragana
// (FUDE_ROMAJI_KATAKANA_SHIFT), cell for cell, so the one table serves both scripts.
// ===========================================================================

#define FUDE_ROMAJI_ROWS            5u      // The rows of a column: a i u e o.
#define FUDE_ROMAJI_KATAKANA_SHIFT  0x60u   // A katakana is its hiragana plus this, cell for cell.

// One column of the table: five rows, a i u e o.
typedef struct {
    // The kana in each row, in hiragana; 0 when that cell has no kana.
    u32       cp[FUDE_ROMAJI_ROWS];
    // Each kana's romaji, as the chart labels it; NULL when that cell has no kana.
    const c8* romaji[FUDE_ROMAJI_ROWS];
} fude_romaji_column;

// Returns one of the table's two blocks, in hiragana, and sets *_columns to how many columns it has. Block 0 is the
// gojūon: the vowels first, then consonant by consonant. Block 1 (any _block but 0) is the voiced columns (the ゛ and
// ゜ marks), then the small kana. The table is constant.
const fude_romaji_column* fude_romaji_block(u32 _block, u32* _columns);

// Returns a kana's romaji as the chart labels it ("ka", or "(tsu)" for the small っ), for hiragana or katakana. For the
// marks the table leaves out it returns a short name (ー is "long", 々 is "repeat"). Returns NULL for anything else.
// The string is constant.
const c8* fude_romaji(u32 _codepoint);
// Whether a kana is one of those learnt first: the gojūon and its voiced forms, in either script (71 in each), but not
// the small ones (ぁ っ ゃ ゎ ゕ) nor the old ones (ゐ ゑ ゔ, ヷ..ヺ).
b8        fude_romaji_core(u32 _codepoint);

// Turns typed romaji into hiragana, written into _out as UTF-8, NUL-terminated, in at most _size bytes. It reads both
// Hepburn and Kunrei spellings, in either case, and ignores spaces. A doubled consonant becomes っ; n, nn and n' become
// ん; and "-" becomes ー. Returns false if some letters could not be read, or nothing could, or the hiragana do not
// fit _out, or the text is longer than any reading (255 bytes, its spaces dropped); _out then holds what could be
// read and fitted.
b8   fude_romaji_to_hiragana(const c8* _romaji, c8* _out, usize _size);

#endif
