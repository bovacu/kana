// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/ja/romaji.h"

// ===========================================================================
// See romaji.h.
//
// The chart's table is two constant arrays of columns, in hiragana. Typed romaji is turned into kana by a separate
// table of spellings, Hepburn and Kunrei alike, matched greedily from the left, longest spelling first, after the
// rules for ん, ー and the small っ have had their turn.
// ===========================================================================

// Block 0, the gojūon: the vowels first, then consonant by consonant. Each column is a i u e o, top to bottom; a cell
// with no kana is 0 and has no romaji.
static const fude_romaji_column FUDE_ROMAJI_BASIC[] = {
    { { 0x3042, 0x3044, 0x3046, 0x3048, 0x304A }, { "a",  "i",   "u",   "e",  "o"  } },
    { { 0x304B, 0x304D, 0x304F, 0x3051, 0x3053 }, { "ka", "ki",  "ku",  "ke", "ko" } },
    { { 0x3055, 0x3057, 0x3059, 0x305B, 0x305D }, { "sa", "shi", "su",  "se", "so" } },
    { { 0x305F, 0x3061, 0x3064, 0x3066, 0x3068 }, { "ta", "chi", "tsu", "te", "to" } },
    { { 0x306A, 0x306B, 0x306C, 0x306D, 0x306E }, { "na", "ni",  "nu",  "ne", "no" } },
    { { 0x306F, 0x3072, 0x3075, 0x3078, 0x307B }, { "ha", "hi",  "fu",  "he", "ho" } },
    { { 0x307E, 0x307F, 0x3080, 0x3081, 0x3082 }, { "ma", "mi",  "mu",  "me", "mo" } },
    { { 0x3084, 0,      0x3086, 0,      0x3088 }, { "ya", NULL,  "yu",  NULL, "yo" } },
    { { 0x3089, 0x308A, 0x308B, 0x308C, 0x308D }, { "ra", "ri",  "ru",  "re", "ro" } },
    { { 0x308F, 0x3090, 0,      0x3091, 0x3092 }, { "wa", "wi",  NULL,  "we", "wo" } },
    { { 0x3093, 0,      0,      0,      0      }, { "n",  NULL,  NULL,  NULL, NULL } },
};

// Block 1: the voiced columns (with the dakuten ゛ or the handakuten ゜), then the small kana, whose romaji are in
// parentheses. The last column holds the odd ones: the small っ and ゎ, ゔ, and the small ゕ and ゖ.
static const fude_romaji_column FUDE_ROMAJI_EXTRA[] = {
    { { 0x304C, 0x304E, 0x3050, 0x3052, 0x3054 }, { "ga", "gi",  "gu",  "ge", "go" } },
    { { 0x3056, 0x3058, 0x305A, 0x305C, 0x305E }, { "za", "ji",  "zu",  "ze", "zo" } },
    { { 0x3060, 0x3062, 0x3065, 0x3067, 0x3069 }, { "da", "ji",  "zu",  "de", "do" } },
    { { 0x3070, 0x3073, 0x3076, 0x3079, 0x307C }, { "ba", "bi",  "bu",  "be", "bo" } },
    { { 0x3071, 0x3074, 0x3077, 0x307A, 0x307D }, { "pa", "pi",  "pu",  "pe", "po" } },
    { { 0x3041, 0x3043, 0x3045, 0x3047, 0x3049 }, { "(a)", "(i)", "(u)", "(e)", "(o)" } },
    { { 0x3083, 0,      0x3085, 0,      0x3087 }, { "(ya)", NULL, "(yu)", NULL, "(yo)" } },
    { { 0x3063, 0x308E, 0x3094, 0x3095, 0x3096 }, { "(tsu)", "(wa)", "vu", "(ka)", "(ke)" } },
};

// How many columns each block has.
#define FUDE_ROMAJI_BASIC_COLUMNS (sizeof(FUDE_ROMAJI_BASIC) / sizeof(FUDE_ROMAJI_BASIC[0]))
#define FUDE_ROMAJI_EXTRA_COLUMNS (sizeof(FUDE_ROMAJI_EXTRA) / sizeof(FUDE_ROMAJI_EXTRA[0]))

// Returns block 0 (the gojūon) or, for any other _block, block 1, and sets *_columns to its number of columns.
const fude_romaji_column* fude_romaji_block(u32 _block, u32* _columns) {
    *_columns = _block == 0u ? (u32)FUDE_ROMAJI_BASIC_COLUMNS : (u32)FUDE_ROMAJI_EXTRA_COLUMNS;
    return _block == 0u ? FUDE_ROMAJI_BASIC : FUDE_ROMAJI_EXTRA;
}

// The characters of the kana and CJK symbol blocks that the table leaves out, each with a name short enough for a
// caption in Browse.
static const struct { u32 cp; const c8* name; } FUDE_ROMAJI_OTHERS[] = {
    { 0x309B, "tenten" }, { 0x309C, "maru" },          // ゛ and ゜ on their own.
    { 0x309D, "repeat" }, { 0x309E, "repeat+" },        // ゝ repeats the kana before it, and ゞ repeats it voiced.
    { 0x30FD, "repeat" }, { 0x30FE, "repeat+" },        // ヽ and ヾ, the same in katakana.
    { 0x30F7, "va" }, { 0x30F8, "vi" }, { 0x30F9, "ve" }, { 0x30FA, "vo" },
    { 0x30FB, "dot" }, { 0x30FC, "long" },              // ・ and ー.
    { 0x3005, "repeat" }, { 0x3006, "shime" },          // 々 repeats the kanji before it; 〆.
    { 0x3001, "comma" }, { 0x3002, "period" },          // 、 and 。.
};

// Returns whether _cp is a core kana: any hiragana from ぁ to ゖ (U+3041..U+3096), or the katakana in the same cells,
// except the small kana and the old ゐ ゑ ゔ listed in _left_out. That leaves 71 in each script. ヷ..ヺ have no
// hiragana cell, so they are left out too.
b8 fude_romaji_core(u32 _cp) {
    const u32 _h = (_cp >= 0x30A1u && _cp <= 0x30F6u) ? _cp - 0x60u : _cp;   // Katakana is looked at as hiragana.
    if(_h < 0x3041u || _h > 0x3096u) {
        return false;
    }
    static const u32 _left_out[] = { 0x3041, 0x3043, 0x3045, 0x3047, 0x3049, 0x3063, 0x3083, 0x3085, 0x3087, 0x308E, 0x3095, 0x3096,
                                     0x3090, 0x3091, 0x3094 };
    for(u32 _i = 0; _i < sizeof(_left_out) / sizeof(_left_out[0]); _i++) {
        if(_h == _left_out[_i]) {
            return false;
        }
    }
    return true;
}

// Returns a kana's or a mark's romaji: first the marks' short names, then the two blocks of the table, a katakana
// being looked up as its hiragana. Returns NULL for anything else.
const c8* fude_romaji(u32 _codepoint) {
    for(u32 _i = 0; _i < sizeof(FUDE_ROMAJI_OTHERS) / sizeof(FUDE_ROMAJI_OTHERS[0]); _i++) {
        if(FUDE_ROMAJI_OTHERS[_i].cp == _codepoint) {
            return FUDE_ROMAJI_OTHERS[_i].name;
        }
    }
    if(_codepoint >= 0x30A1u && _codepoint <= 0x30F6u) {
        _codepoint -= FUDE_ROMAJI_KATAKANA_SHIFT;
    }
    for(u32 _block = 0; _block < 2; _block++) {
        u32                       _n;
        const fude_romaji_column* _cols = fude_romaji_block(_block, &_n);
        for(u32 _c = 0; _c < _n; _c++) {
            for(u32 _r = 0; _r < FUDE_ROMAJI_ROWS; _r++) {
                if(_cols[_c].cp[_r] == _codepoint) {
                    return _cols[_c].romaji[_r];
                }
            }
        }
    }
    return NULL;
}

// --- typed romaji, as kana ----------------------------------------------------------

// One spelling: lower-case romaji, and the hiragana it is written as (UTF-8).
typedef struct {
    const c8* romaji;
    const c8* kana;
} fude_romaji_pair;

// Every spelling read, Hepburn and Kunrei alike (shi and si, tsu and tu), with x for the small kana (xa, xtsu). The
// table need not be in any order, because the matcher tries 4 letters first, then 3, 2 and 1, so a longer spelling
// always wins over a shorter one it starts with.
RDE_INTERNAL const fude_romaji_pair fude_romaji_table[] = {
    { "a", "あ" }, { "i", "い" }, { "u", "う" }, { "e", "え" }, { "o", "お" },
    { "ka", "か" }, { "ki", "き" }, { "ku", "く" }, { "ke", "け" }, { "ko", "こ" },
    { "kya", "きゃ" }, { "kyu", "きゅ" }, { "kyo", "きょ" },
    { "ga", "が" }, { "gi", "ぎ" }, { "gu", "ぐ" }, { "ge", "げ" }, { "go", "ご" },
    { "gya", "ぎゃ" }, { "gyu", "ぎゅ" }, { "gyo", "ぎょ" },
    { "sa", "さ" }, { "shi", "し" }, { "si", "し" }, { "su", "す" }, { "se", "せ" }, { "so", "そ" },
    { "sha", "しゃ" }, { "shu", "しゅ" }, { "sho", "しょ" }, { "she", "しぇ" },
    { "sya", "しゃ" }, { "syu", "しゅ" }, { "syo", "しょ" },
    { "za", "ざ" }, { "ji", "じ" }, { "zi", "じ" }, { "zu", "ず" }, { "ze", "ぜ" }, { "zo", "ぞ" },
    { "ja", "じゃ" }, { "ju", "じゅ" }, { "jo", "じょ" }, { "je", "じぇ" },
    { "jya", "じゃ" }, { "jyu", "じゅ" }, { "jyo", "じょ" },
    { "zya", "じゃ" }, { "zyu", "じゅ" }, { "zyo", "じょ" },
    { "ta", "た" }, { "chi", "ち" }, { "ti", "ち" }, { "tsu", "つ" }, { "tu", "つ" }, { "te", "て" }, { "to", "と" },
    { "cha", "ちゃ" }, { "chu", "ちゅ" }, { "cho", "ちょ" }, { "che", "ちぇ" },
    { "tya", "ちゃ" }, { "tyu", "ちゅ" }, { "tyo", "ちょ" },
    { "da", "だ" }, { "di", "ぢ" }, { "du", "づ" }, { "dzu", "づ" }, { "de", "で" }, { "do", "ど" },
    { "dya", "ぢゃ" }, { "dyu", "ぢゅ" }, { "dyo", "ぢょ" },
    { "na", "な" }, { "ni", "に" }, { "nu", "ぬ" }, { "ne", "ね" }, { "no", "の" },
    { "nya", "にゃ" }, { "nyu", "にゅ" }, { "nyo", "にょ" },
    { "ha", "は" }, { "hi", "ひ" }, { "fu", "ふ" }, { "hu", "ふ" }, { "he", "へ" }, { "ho", "ほ" },
    { "hya", "ひゃ" }, { "hyu", "ひゅ" }, { "hyo", "ひょ" },
    { "fa", "ふぁ" }, { "fi", "ふぃ" }, { "fe", "ふぇ" }, { "fo", "ふぉ" },
    { "ba", "ば" }, { "bi", "び" }, { "bu", "ぶ" }, { "be", "べ" }, { "bo", "ぼ" },
    { "bya", "びゃ" }, { "byu", "びゅ" }, { "byo", "びょ" },
    { "pa", "ぱ" }, { "pi", "ぴ" }, { "pu", "ぷ" }, { "pe", "ぺ" }, { "po", "ぽ" },
    { "pya", "ぴゃ" }, { "pyu", "ぴゅ" }, { "pyo", "ぴょ" },
    { "ma", "ま" }, { "mi", "み" }, { "mu", "む" }, { "me", "め" }, { "mo", "も" },
    { "mya", "みゃ" }, { "myu", "みゅ" }, { "myo", "みょ" },
    { "ya", "や" }, { "yu", "ゆ" }, { "yo", "よ" },
    { "ra", "ら" }, { "ri", "り" }, { "ru", "る" }, { "re", "れ" }, { "ro", "ろ" },
    { "rya", "りゃ" }, { "ryu", "りゅ" }, { "ryo", "りょ" },
    { "wa", "わ" }, { "wi", "ゐ" }, { "we", "ゑ" }, { "wo", "を" },
    { "vu", "ゔ" },
    { "xa", "ぁ" }, { "xi", "ぃ" }, { "xu", "ぅ" }, { "xe", "ぇ" }, { "xo", "ぉ" },
    { "xya", "ゃ" }, { "xyu", "ゅ" }, { "xyo", "ょ" }, { "xtsu", "っ" }, { "xtu", "っ" },
};

// Returns whether _c is a lower-case vowel letter.
RDE_INTERNAL b8 fude_romaji_vowel(c8 _c) {
    return _c == 'a' || _c == 'i' || _c == 'u' || _c == 'e' || _c == 'o';
}

// The most bytes of romaji, its spaces dropped, that fude_romaji_to_hiragana reads. What it is given comes from the
// app's fields and searches, none longer than 127 bytes (Check's text, Browse's search), so this is room to spare; a
// longer text is not a reading, and is refused rather than read in part.
#define FUDE_ROMAJI_MAX 255u

// Appends _kana to _out, whose text is *_len bytes long and which holds _size bytes, keeping it NUL-terminated.
// Returns false when the kana do not fit whole, and then leaves them out.
RDE_INTERNAL b8 fude_romaji_append(c8* _out, usize _size, usize* _len, const c8* _kana) {
    const usize _n = rde_strlen(_kana);
    if(*_len + _n + 1 > _size) {
        return false;
    }
    rde_memcpy(_out + *_len, _kana, _n);
    *_len += _n;
    _out[*_len] = 0;
    return true;
}

// Turns typed romaji into hiragana (see the header). The text is first lowered and its spaces dropped; one longer than
// FUDE_ROMAJI_MAX bytes is refused. Then, at each place: "-" is ー; an n that cannot start a syllable is ん; a doubled
// consonant (or the t of "tch") is a small っ; otherwise the longest spelling in the table that matches. A letter
// nothing matches is skipped and makes the result false. Kana that do not fit _out end the reading there, and make it
// false too.
b8 fude_romaji_to_hiragana(const c8* _romaji, c8* _out, usize _size) {
    usize _len  = 0;
    b8    _ok   = true;
    b8    _fits = true;   // Every kana so far fitted in _out.
    _out[0] = 0;

    // Lower the case and drop the spaces.
    c8    _s[FUDE_ROMAJI_MAX + 1u];
    usize _n = 0;
    for(const c8* _p = _romaji; *_p != 0; _p++) {
        if(*_p == ' ') {
            continue;
        }
        if(_n == FUDE_ROMAJI_MAX) {
            return false;   // Longer than any reading, and it is not read in part.
        }
        _s[_n++] = (c8)rde_char_to_lower((unsigned char)*_p);
    }
    _s[_n] = 0;

    for(usize _i = 0; _i < _n && _fits;) {
        const c8 _c = _s[_i];

        if(_c == '-') {
            _fits = fude_romaji_append(_out, _size, &_len, "ー");
            _i++;
            continue;
        }

        if(_c == 'n') {
            const c8 _next = _s[_i + 1];
            if(_next == '\'') {                                  // n' is ん, and keeps it from joining what follows.
                _fits = fude_romaji_append(_out, _size, &_len, "ん");
                _i += 2;
                continue;
            }
            if(_next == 'n' && !fude_romaji_vowel(_s[_i + 2]) && _s[_i + 2] != 'y') {
                _fits = fude_romaji_append(_out, _size, &_len, "ん");   // nn is ん, unless a vowel or y follows.
                _i += 2;
                continue;
            }
            if(_next != 0 && !fude_romaji_vowel(_next) && _next != 'y') {
                _fits = fude_romaji_append(_out, _size, &_len, "ん");   // An n before a consonant is ん.
                _i++;
                continue;
            }
            if(_next == 0) {
                _fits = fude_romaji_append(_out, _size, &_len, "ん");   // An n at the end is ん.
                _i++;
                continue;
            }
        }

        // A doubled consonant is a small っ ("kitte"), and so is the t of "tch" ("matcha").
        if(_c >= 'a' && _c <= 'z' && !fude_romaji_vowel(_c) && _c != 'n' &&
           (_s[_i + 1] == _c || (_c == 't' && _s[_i + 1] == 'c' && _s[_i + 2] == 'h'))) {
            _fits = fude_romaji_append(_out, _size, &_len, "っ");
            _i++;
            continue;
        }

        // The longest spelling that matches here, trying 4 letters down to 1.
        b8 _matched = false;
        for(usize _try = 4; _try >= 1 && !_matched; _try--) {
            if(_i + _try > _n) {
                continue;
            }
            for(u32 _t = 0; _t < sizeof(fude_romaji_table) / sizeof(fude_romaji_table[0]); _t++) {
                if(rde_strlen(fude_romaji_table[_t].romaji) == _try && rde_strncmp(&_s[_i], fude_romaji_table[_t].romaji, _try) == 0) {
                    _fits = fude_romaji_append(_out, _size, &_len, fude_romaji_table[_t].kana);
                    _i += _try;
                    _matched = true;
                    break;
                }
            }
        }

        if(!_matched) {
            _ok = false;
            _i++;
        }
    }

    return _ok && _fits && _len > 0;
}
