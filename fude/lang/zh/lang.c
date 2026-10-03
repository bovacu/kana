#include "lang/lang.h"
#include "drawing/base/text.h"
#include "drawing/base/utf8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lang/zh/traditional.h"

// ===========================================================================
// See lang.h: Mandarin Chinese. Simplified characters and the ones only
// Traditional Chinese writes; HSK 2025's levels (1 to 6, and 7-9 together);
// pinyin, typed with tone numbers or without.
// ===========================================================================

const c8* fude_lang_code(void)      { return "zh"; }
const c8* fude_lang_ink_model(void) { return "zh-Hani-CN"; }
const c8* fude_lang_voice(void)     { return "zh-CN"; }

// --- groups: simplified (and shared), traditional only -------------------------------

enum { FUDE_ZH_SIMPLIFIED = 0, FUDE_ZH_TRADITIONAL, FUDE_ZH_GROUPS };

RDE_INTERNAL int fude_zh_compare(const void* _a, const void* _b) {
    const u32 _x = *(const u32*)_a, _y = *(const u32*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

u32 fude_lang_group_count(void) {
    return FUDE_ZH_GROUPS;
}

u8 fude_lang_group(u32 _cp) {
    if((_cp >= 0x4E00u && _cp <= 0x9FFFu) || (_cp >= 0x3400u && _cp <= 0x4DBFu) ||
       (_cp >= 0xF900u && _cp <= 0xFAFFu) || (_cp >= 0x20000u && _cp <= 0x2FFFFu)) {
        const b8 _traditional = bsearch(&_cp, FUDE_ZH_TRADITIONAL_ONLY, sizeof(FUDE_ZH_TRADITIONAL_ONLY) / sizeof(FUDE_ZH_TRADITIONAL_ONLY[0]), sizeof(u32), fude_zh_compare) != NULL;
        return _traditional ? FUDE_ZH_TRADITIONAL : FUDE_ZH_SIMPLIFIED;
    }
    return FUDE_LANG_NO_GROUP;
}

u32 fude_lang_group_flags(u32 _group) {
    RDE_UNUSED(_group);
    return 0u;   // characters, with readings and meanings; learnt by level, not whole
}

u32 fude_lang_group_name(u32 _group) {
    return _group == FUDE_ZH_SIMPLIFIED ? (u32)FUDE_TEXT_ZH_SIMPLIFIED : _group == FUDE_ZH_TRADITIONAL || _group == FUDE_LANG_NO_GROUP ? (u32)FUDE_TEXT_ZH_TRADITIONAL : (u32)FUDE_TEXT_COUNT;   // a radical (⺀): as the last
}

u32 fude_lang_group_prompt(u32 _group) {
    RDE_UNUSED(_group);
    return (u32)FUDE_TEXT_EXAM_WRITE_HANZI;
}

b8 fude_lang_core(u32 _cp) {
    RDE_UNUSED(_cp);
    return false;
}

const c8* fude_lang_latin(u32 _cp) {
    RDE_UNUSED(_cp);
    return NULL;   // no script of letters
}

// --- levels: HSK 1 to 6, then 7-9 (stored as 7) ------------------------------------------

u32 fude_lang_level_count(void) {
    return 7u;
}

u8 fude_lang_level_value(u32 _level) {
    return _level < 7u ? (u8)(_level + 1u) : 0u;
}

void fude_lang_level_name(u8 _value, b8 _long, c8* _out, usize _size) {
    RDE_UNUSED(_long);
    if(_value == 0u) {
        snprintf(_out, _size, "%s", "");
    } else if(_value >= 7u) {
        snprintf(_out, _size, "HSK 7\xE2\x80\x93" "9");   // –
    } else {
        snprintf(_out, _size, "HSK %u", (u32)_value);
    }
}

// --- readings: pinyin ------------------------------------------------------------------

u32 fude_lang_reading_kinds(void) {
    return 1u;
}

u32 fude_lang_reading_name(u32 _kind) {
    return _kind == 0u ? (u32)FUDE_TEXT_SORT_PINYIN : (u32)FUDE_TEXT_COUNT;
}

// The vowels with their tones: a e i o u ü, each in tones 1-4.
static const u32 FUDE_ZH_TONED[6][4] = {
    { 0x0101u, 0x00E1u, 0x01CEu, 0x00E0u },   // ā á ǎ à
    { 0x0113u, 0x00E9u, 0x011Bu, 0x00E8u },   // ē é ě è
    { 0x012Bu, 0x00EDu, 0x01D0u, 0x00ECu },   // ī í ǐ ì
    { 0x014Du, 0x00F3u, 0x01D2u, 0x00F2u },   // ō ó ǒ ò
    { 0x016Bu, 0x00FAu, 0x01D4u, 0x00F9u },   // ū ú ǔ ù
    { 0x01D6u, 0x01D8u, 0x01DAu, 0x01DCu },   // ǖ ǘ ǚ ǜ
};
static const u32 FUDE_ZH_VOWELS[6] = { 'a', 'e', 'i', 'o', 'u', 0x00FCu };

u32 fude_lang_reading_fold(u32 _cp) {
    if(_cp >= 'A' && _cp <= 'Z') {
        return _cp + ('a' - 'A');
    }
    if(_cp == 0x00FCu || _cp == 0x00DCu) {
        return 'u';   // ü: lü and lu alike, as typed without it
    }
    for(u32 _v = 0; _v < 6u; _v++) {
        for(u32 _t = 0; _t < 4u; _t++) {
            if(FUDE_ZH_TONED[_v][_t] == _cp) {
                return _v == 5u ? 'u' : FUDE_ZH_VOWELS[_v];
            }
        }
    }
    return _cp;
}

// One syllable's letters (lower case, v as ü) with its tone (1-5, 0 none): its
// mark on the vowel pinyin puts it on — a or e, the o of ou, else the last vowel.
RDE_INTERNAL void fude_zh_mark(const u32* _letters, u32 _n, u32 _tone, c8* _out, usize _size, usize* _at) {
    i32 _on = -1;
    for(u32 _i = 0; _i < _n && _on < 0; _i++) {
        if(_letters[_i] == 'a' || _letters[_i] == 'e') {
            _on = (i32)_i;
        }
    }
    for(u32 _i = 0; _i + 1u < _n && _on < 0; _i++) {
        if(_letters[_i] == 'o' && _letters[_i + 1u] == 'u') {
            _on = (i32)_i;
        }
    }
    for(u32 _i = _n; _i-- > 0 && _on < 0;) {
        for(u32 _v = 0; _v < 6u; _v++) {
            if(_letters[_i] == FUDE_ZH_VOWELS[_v]) {
                _on = (i32)_i;
                break;
            }
        }
    }
    for(u32 _i = 0; _i < _n; _i++) {
        u32 _c = _letters[_i];
        if((i32)_i == _on && _tone >= 1u && _tone <= 4u) {
            for(u32 _v = 0; _v < 6u; _v++) {
                if(_c == FUDE_ZH_VOWELS[_v]) {
                    _c = FUDE_ZH_TONED[_v][_tone - 1u];
                    break;
                }
            }
        }
        c8 _one[5];
        fude_utf8_put(_c, _one);
        const usize _len = strlen(_one);
        if(*_at + _len < _size) {
            memcpy(_out + *_at, _one, _len);
            *_at += _len;
        }
    }
    _out[*_at] = 0;
}

// Pinyin as typed — "ni3hao3", "ni3 hao3", "nihao", "lv4", "nǐ hǎo" — written
// as readings are: syllables together, the tones marked (a 5, or none: unmarked).
b8 fude_lang_reading_from_latin(const c8* _latin, c8* _out, usize _size) {
    if(_out == NULL || _size == 0) {
        return false;
    }
    _out[0] = 0;
    usize     _at = 0;
    u32       _letters[16];
    u32       _n  = 0;
    b8        _ok = true;
    const c8* _p  = _latin != NULL ? _latin : "";
    for(;;) {
        u32 _c = fude_utf8_next(&_p);
        if(_c >= 'A' && _c <= 'Z') {
            _c += 'a' - 'A';
        }
        if(_c == 'v') {
            _c = 0x00FCu;
        }
        const b8 _letter = (_c >= 'a' && _c <= 'z') || _c == 0x00FCu || fude_lang_reading_fold(_c) != _c;
        if(_letter) {
            if(_n < 16u) {
                _letters[_n++] = _c;
            }
            continue;
        }
        // A syllable ends: a tone digit after it, a space, an apostrophe, or the end.
        const u32 _tone = _c >= '1' && _c <= '5' ? _c - '0' : 0u;
        if(_n > 0) {
            fude_zh_mark(_letters, _n, _tone, _out, _size, &_at);
            _n = 0;
        } else if(_tone != 0u) {
            _ok = false;   // a digit with no syllable before it
        }
        if(_c == 0) {
            break;
        }
        if(_tone == 0u && _c != ' ' && _c != '\'' && _c != '-') {
            _ok = false;   // not pinyin
        }
    }
    return _ok && _out[0] != 0;
}

u32 fude_lang_word_reading_kind(u32 _order) {
    RDE_UNUSED(_order);
    return 0u;
}

b8 fude_lang_word_reading_latin(void) {
    return false;   // pinyin typed with tone numbers becomes pinyin with tone marks (fude_lang_reading_from_latin)
}

// --- badges: simplified forms -----------------------------------------------------------

u32 fude_lang_badge(FUDE_LANG_BADGE_ _badge) {
    static const u32 _badges[FUDE_LANG_BADGE_COUNT] = {
        0x97F3u,   // 音 its sound (pinyin)
        0x97F3u,   // (no second reading)
        0x90E8u,   // 部
        0x8BD5u,   // 试
        0x8BCDu,   // 词
        0x4F3Cu,   // 似
        0x8BB0u,   // 记
    };
    return _badge < FUDE_LANG_BADGE_COUNT ? _badges[_badge] : 0u;
}
