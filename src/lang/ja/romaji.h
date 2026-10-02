#ifndef KANA_ROMAJI
#define KANA_ROMAJI

#include "rde.h"

// ===========================================================================
// The kana and their romaji: the gojūon table — in the order the chart lays it
// out, katakana a fixed distance from hiragana — and a short name for each mark
// the table leaves out. Japanese's own (a language pack's, for another app).
// ===========================================================================

#define KANA_ROMAJI_ROWS            5u      // a column's rows: a i u e o
#define KANA_ROMAJI_KATAKANA_SHIFT  0x60u   // katakana = hiragana + this, cell for cell

// One column of the table: five rows, a i u e o. 0: no kana in that cell.
typedef struct {
    u32       cp[KANA_ROMAJI_ROWS];
    const c8* romaji[KANA_ROMAJI_ROWS];
} kana_romaji_column;

// The table's two blocks (hiragana): 0, the gojūon, vowels first then consonant
// by consonant; 1, the voiced columns (゛ ゜), then the small kana. Its columns.
const kana_romaji_column* kana_romaji_block(u32 _block, u32* _columns);

// A kana's romaji as the chart labels it ("ka", "(tsu)" for small っ), hiragana
// or katakana; a short name for the marks it leaves out (ー "long", 々 "repeat");
// NULL for anything else.
const c8* kana_romaji(u32 _codepoint);
// The kana learnt first: the gojūon and its voiced forms, in either script (71
// each) — not the small ones (ぁ っ ゃ ゎ ゕ), nor the old (ゐ ゑ ゔ, ヷ..ヺ).
b8        kana_romaji_core(u32 _codepoint);

// Romaji → hiragana (Hepburn and Kunrei spellings, doubled consonants → っ,
// n/nn/n' → ん, "-" → ー). False if some letters could not be read; _out then
// holds what could. _out is UTF-8, NUL-terminated.
b8   kana_romaji_to_hiragana(const c8* _romaji, c8* _out, usize _size);

#endif
