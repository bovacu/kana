// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"
#include "drawing/base/text.h"
#include "drawing/base/utf8.h"

#include "lang/zh/traditional.h"

// ===========================================================================
// See lang.h. This is Mandarin Chinese: two groups, the simplified characters (with those both forms share) and the
// ones only Traditional Chinese writes (traditional.h, a sorted list); HSK 2025's levels (1 to 6, and 7-9 together);
// and pinyin as the one kind of reading, which can be typed with tone numbers or without. Chinese has no script of
// letters, so there are no Latin names, no core set and no chart.
// ===========================================================================

// Returns Chinese's code for translation (ML Kit Translate).
const c8* fude_lang_code(void)      { return "zh"; }
// Returns the ML Kit Digital Ink Recognition model that reads Chinese handwriting.
const c8* fude_lang_ink_model(void) { return "zh-Hani-CN"; }
// Returns the voice that reads Chinese aloud.
const c8* fude_lang_voice(void)     { return "zh-CN"; }
// Returns true: ML Kit Text Recognition can read Chinese in pictures.
b8        fude_lang_text_readable(void) { return true; }

// Returns Simplified Chinese as the app's fourth UI language, with its name in Chinese and China's flag.
RDE_LANGUAGE_ fude_lang_ui_language(const c8** _name, const c8** _flag) {
    *_name = "\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87";   // 简体中文
    *_flag = "assets/flags/cn.png";
    return RDE_LANGUAGE_ZH_CN;
}

// --- groups: simplified (and shared), traditional only -------------------------------

// Chinese's groups, and how many there are.
enum { FUDE_ZH_SIMPLIFIED = 0, FUDE_ZH_TRADITIONAL, FUDE_ZH_GROUPS };

// The order for rde_bsearch over the traditional-only list: two u32 code points, compared by value.
RDE_INTERNAL int fude_zh_compare(const any _a, const any _b) {
    const u32 _x = *(const u32*)_a, _y = *(const u32*)_b;
    return _x < _y ? -1 : (_x > _y ? 1 : 0);
}

// Returns how many groups there are: two.
u32 fude_lang_group_count(void) {
    return FUDE_ZH_GROUPS;
}

// Returns a Han character's group: traditional when traditional.h lists it, simplified otherwise. Anything else is in
// no group.
u8 fude_lang_group(u32 _cp) {
    if(fude_utf8_is_han(_cp)) {
        const b8 _traditional = rde_bsearch(&_cp, FUDE_ZH_TRADITIONAL_ONLY, sizeof(FUDE_ZH_TRADITIONAL_ONLY) / sizeof(FUDE_ZH_TRADITIONAL_ONLY[0]), sizeof(u32), fude_zh_compare) != NULL;
        return _traditional ? FUDE_ZH_TRADITIONAL : FUDE_ZH_SIMPLIFIED;
    }
    return FUDE_LANG_NO_GROUP;
}

// Returns no flags for either group.
u32 fude_lang_group_flags(u32 _group) {
    RDE_UNUSED(_group);
    return 0u;   // Characters with readings and meanings, learnt by level rather than as whole sets.
}

// Returns a group's name as a FUDE_TEXT_ id. A character in no group, such as a radical (⺀), is shown under the last
// group's name, traditional.
u32 fude_lang_group_name(u32 _group) {
    return _group == FUDE_ZH_SIMPLIFIED ? (u32)FUDE_TEXT_ZH_SIMPLIFIED : _group == FUDE_ZH_TRADITIONAL || _group == FUDE_LANG_NO_GROUP ? (u32)FUDE_TEXT_ZH_TRADITIONAL : (u32)FUDE_TEXT_COUNT;   // A radical (⺀) is shown as the last group.
}

// Returns what an exam asks for a group: the same for both, since both are written as hanzi.
u32 fude_lang_group_prompt(u32 _group) {
    return _group < FUDE_ZH_GROUPS ? (u32)FUDE_TEXT_EXAM_WRITE_HANZI : (u32)FUDE_TEXT_COUNT;   // Both kinds are hanzi.
}

// Returns false: no group is a set with core characters.
b8 fude_lang_core(u32 _cp) {
    RDE_UNUSED(_cp);
    return false;
}

// Returns false: no Chinese character is written on another.
b8 fude_lang_combining(u32 _cp) {
    RDE_UNUSED(_cp);
    return false;   // Every character stands alone.
}

// Returns NULL: Chinese has no script of letters to name.
const c8* fude_lang_latin(u32 _cp) {
    RDE_UNUSED(_cp);
    return NULL;   // Chinese has no script of letters.
}

// --- writing direction and joined forms: left to right, each letter as itself -------------

// Returns false: Chinese is written left to right here.
b8 fude_lang_rtl(void) {
    return false;
}

// Returns the character itself: Chinese has no joined forms.
u32 fude_lang_form(u32 _before, u32 _cp, u32 _after, b8* _with_after) {
    RDE_UNUSED(_before);
    RDE_UNUSED(_after);
    if(_with_after != NULL) {
        *_with_after = false;
    }
    return _cp;
}

// Returns the character itself, since no character is a form of another.
u32 fude_lang_letter(u32 _cp) {
    return _cp;
}

// --- levels: HSK 1 to 6, then 7-9 (stored as 7) ------------------------------------------

// Returns the seven levels: HSK 1 to 6, and 7-9 as one.
u32 fude_lang_level_count(void) {
    return 7u;
}

// Returns the byte a level is stored as: its place plus one (HSK 1 is 1, and 7-9 is 7); 0 past the last.
u8 fude_lang_level_value(u32 _level) {
    return _level < 7u ? (u8)(_level + 1u) : 0u;
}

// Writes a stored level's name, the same short or long: "HSK 3", or "HSK 7–9" for 7 and above; "" for no level.
void fude_lang_level_name(u8 _value, b8 _long, c8* _out, usize _size) {
    RDE_UNUSED(_long);
    if(_value == 0u) {
        rde_snprintf(_out, _size, "%s", "");
    } else if(_value >= 7u) {
        rde_snprintf(_out, _size, "HSK 7\xE2\x80\x93" "9");   // An en dash (–) between 7 and 9.
    } else {
        rde_snprintf(_out, _size, "HSK %u", (u32)_value);
    }
}

// --- readings: pinyin ------------------------------------------------------------------

// Returns the one kind of reading: pinyin.
u32 fude_lang_reading_kinds(void) {
    return 1u;
}

// Returns the name of the one kind of reading ("Pinyin"), as a FUDE_TEXT_ id.
u32 fude_lang_reading_name(u32 _kind) {
    return _kind == 0u ? (u32)FUDE_TEXT_SORT_PINYIN : (u32)FUDE_TEXT_COUNT;
}

// Each pinyin vowel (a e i o u ü) with its tone marks, for tones 1 to 4, and the vowels themselves in the same order.
static const u32 FUDE_ZH_TONED[6][4] = {
    { 0x0101u, 0x00E1u, 0x01CEu, 0x00E0u },   // ā á ǎ à
    { 0x0113u, 0x00E9u, 0x011Bu, 0x00E8u },   // ē é ě è
    { 0x012Bu, 0x00EDu, 0x01D0u, 0x00ECu },   // ī í ǐ ì
    { 0x014Du, 0x00F3u, 0x01D2u, 0x00F2u },   // ō ó ǒ ò
    { 0x016Bu, 0x00FAu, 0x01D4u, 0x00F9u },   // ū ú ǔ ù
    { 0x01D6u, 0x01D8u, 0x01DAu, 0x01DCu },   // ǖ ǘ ǚ ǜ
};
static const u32 FUDE_ZH_VOWELS[6] = { 'a', 'e', 'i', 'o', 'u', 0x00FCu };

// Returns a pinyin character as readings are compared: lower case, and a vowel without its tone mark, with ü as u.
u32 fude_lang_reading_fold(u32 _cp) {
    if(_cp >= 'A' && _cp <= 'Z') {
        return _cp + ('a' - 'A');
    }
    if(_cp == 0x00FCu || _cp == 0x00DCu) {
        return 'u';   // ü compares as u, so lü and lu are alike, as when typed without the dots.
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

// Appends one syllable to _out at *_at (_size bytes in all, kept NUL-terminated), with its tone marked. _letters are
// its _n letters (lower case, with v already turned into ü), and _tone is 1 to 5, or 0 for none; tones 5 and 0 are
// left unmarked. The mark goes on the vowel pinyin puts it on: an a or an e, else the o of ou, else the last vowel.
// Returns false when the syllable does not fit whole: the letters that do not fit are left out, so _out holds only
// what fitted.
RDE_INTERNAL b8 fude_zh_mark(const u32* _letters, u32 _n, u32 _tone, c8* _out, usize _size, usize* _at) {
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
        const usize _len = rde_strlen(_one);
        if(*_at + _len >= _size) {
            _out[*_at] = 0;
            return false;   // No room for this letter, so the syllable is not whole.
        }
        rde_memcpy(_out + *_at, _one, _len);
        *_at += _len;
    }
    _out[*_at] = 0;
    return true;
}

// Returns _cp as it is.
u32 fude_lang_reading_capital(u32 _cp) {
    return _cp;   // Chinese has no other way of writing a reading typed in capitals.
}

// Turns pinyin as typed ("ni3hao3", "ni3 hao3", "nihao", "lv4", "nǐ hǎo") into the way readings are written: the
// syllables run together, with their tones marked (a 5, or no number, leaves a syllable unmarked). A syllable ends at
// a tone digit, a space, an apostrophe, a hyphen or the end, and may be any length: toneless pinyin runs its syllables
// together into one ("zhonghuarenmingongheguo"), so its letters are kept in an array that grows as they come. Returns
// false when there is anything else, or a digit with no syllable before it, or nothing came out, or what came out did
// not fit in _out.
b8 fude_lang_reading_from_latin(const c8* _latin, c8* _out, usize _size) {
    if(_out == NULL || _size == 0) {
        return false;
    }
    _out[0] = 0;
    usize             _at      = 0;
    rde_arr TYPE(u32) _letters = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());   // The syllable's.
    b8                _ok      = true;
    b8                _fits    = true;   // Every syllable so far fitted in _out; once one does not, none more is written.
    const c8*         _p       = _latin != NULL ? _latin : "";
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
            rde_arr_add(&_letters, (any)&_c);
            continue;
        }
        // A syllable ends here: at a tone digit after it, a space, an apostrophe, a hyphen or the end.
        const u32 _tone = _c >= '1' && _c <= '5' ? _c - '0' : 0u;
        if(rde_arr_length(&_letters) > 0) {
            _fits = _fits && fude_zh_mark((const u32*)_letters.memory, (u32)rde_arr_length(&_letters), _tone, _out, _size, &_at);
            rde_arr_clear(&_letters);
        } else if(_tone != 0u) {
            _ok = false;   // A digit with no syllable before it.
        }
        if(_c == 0) {
            break;
        }
        if(_tone == 0u && _c != ' ' && _c != '\'' && _c != '-') {
            _ok = false;   // Anything else is not pinyin.
        }
    }
    rde_arr_free(&_letters);
    return _ok && _fits && _out[0] != 0;
}

// Returns pinyin, the only kind, whatever _order is.
u32 fude_lang_word_reading_kind(u32 _order) {
    RDE_UNUSED(_order);
    return 0u;
}

// Returns false, so that pinyin typed with tone numbers becomes pinyin with tone marks (fude_lang_reading_from_latin).
b8 fude_lang_word_reading_latin(void) {
    return false;   // Typed pinyin is turned into marked pinyin by fude_lang_reading_from_latin.
}

// Returns false: no word is read as it is written, since its words are read in pinyin, typed and turned into marked
// pinyin (fude_lang_reading_from_latin).
b8 fude_lang_reads_as_written(const c8* _written) {
    RDE_UNUSED(_written);
    return false;
}

// --- badges: simplified forms -----------------------------------------------------------

// Returns the character a badge shows, in FUDE_LANG_BADGE_ order, in its simplified form, or 0 out of range.
u32 fude_lang_badge(FUDE_LANG_BADGE_ _badge) {
    static const u32 _badges[FUDE_LANG_BADGE_COUNT] = {
        0x97F3u,   // 音: its sound (pinyin).
        0x97F3u,   // There is no second reading, so the same 音.
        0x90E8u,   // 部
        0x8BD5u,   // 试
        0x8BCDu,   // 词
        0x4F3Cu,   // 似
        0x8BB0u,   // 记
        0x8BD1u,   // 译
    };
    return _badge < FUDE_LANG_BADGE_COUNT ? _badges[_badge] : 0u;
}

// --- its chart: none (Chinese has no alphabet to chart) ------------------------------------------------------------

// Returns NULL: there is no chart.
const fude_lang_chart_def* fude_lang_chart(void) {
    return NULL;
}

// --- handwriting: how free writing is read ---------------------------------------------------

// Returns no rules (lang.h): Chinese's free writing is read by each character's shape, size and place alone, with no
// scripts to keep a word in.
const fude_lang_writing_def* fude_lang_writing(void) {
    static const fude_lang_writing_def _none = { .switch_cost = NULL };
    return &_none;
}
