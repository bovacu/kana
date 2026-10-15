// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"
#include "lang/ja/romaji.h"
#include "drawing/base/text.h"
#include "drawing/base/utf8.h"

// ===========================================================================
// See lang.h. This is Japanese: three groups (hiragana, katakana and kanji), the JLPT's five levels, on and kun
// readings, and romaji as the kana's Latin names and as the way readings are typed (romaji.h).
// ===========================================================================

// Returns Japanese's code for translation (ML Kit Translate).
const c8* fude_lang_code(void)      { return "ja"; }
// Returns the ML Kit Digital Ink Recognition model that reads Japanese handwriting.
const c8* fude_lang_ink_model(void) { return "ja"; }
// Returns the voice that reads Japanese aloud.
const c8* fude_lang_voice(void)     { return "ja-JP"; }
// Returns true: ML Kit Text Recognition can read Japanese in pictures.
b8        fude_lang_text_readable(void) { return true; }

// Returns Japanese as the app's fourth UI language, with its name in Japanese and Japan's flag.
RDE_LANGUAGE_ fude_lang_ui_language(const c8** _name, const c8** _flag) {
    *_name = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";   // 日本語
    *_flag = "assets/flags/jp.png";
    return RDE_LANGUAGE_JA_JP;
}

// --- groups ------------------------------------------------------------------------------

// Japanese's groups, and how many there are.
enum { FUDE_JA_HIRAGANA = 0, FUDE_JA_KATAKANA, FUDE_JA_KANJI, FUDE_JA_GROUPS };

// Returns how many groups there are: three.
u32 fude_lang_group_count(void) {
    return FUDE_JA_GROUPS;
}

// Returns a character's group: hiragana ぁ..ゖ (U+3041..U+3096), katakana ァ..ヺ (U+30A1..U+30FA), or kanji for any
// Han character. Anything else, such as ー or 々, is in no group.
u8 fude_lang_group(u32 _cp) {
    if(_cp >= 0x3041u && _cp <= 0x3096u) { return FUDE_JA_HIRAGANA; }
    if(_cp >= 0x30A1u && _cp <= 0x30FAu) { return FUDE_JA_KATAKANA; }
    if(fude_utf8_is_han(_cp)) {
        return FUDE_JA_KANJI;
    }
    return FUDE_LANG_NO_GROUP;
}

// Returns a group's flags: the two kana scripts are scripts learnt as sets, and kanji are learnt by level.
u32 fude_lang_group_flags(u32 _group) {
    return _group == FUDE_JA_KANJI ? 0u : FUDE_LANG_GROUP_SCRIPT | FUDE_LANG_GROUP_SET;
}

// Returns a group's name as a FUDE_TEXT_ id. A character in no group (such as 々) is shown as a kanji.
u32 fude_lang_group_name(u32 _group) {
    static const FUDE_TEXT_ _names[FUDE_JA_GROUPS] = { FUDE_TEXT_HIRAGANA, FUDE_TEXT_KATAKANA, FUDE_TEXT_KANJI };
    return _group < FUDE_JA_GROUPS ? (u32)_names[_group] : _group == FUDE_LANG_NO_GROUP ? (u32)FUDE_TEXT_KANJI : (u32)FUDE_TEXT_COUNT;   // 々 is shown as a kanji.
}

// Returns what an exam asks for a group ("WRITE IN HIRAGANA"), as a FUDE_TEXT_ id.
u32 fude_lang_group_prompt(u32 _group) {
    static const FUDE_TEXT_ _prompts[FUDE_JA_GROUPS] = { FUDE_TEXT_EXAM_WRITE_HIRAGANA, FUDE_TEXT_EXAM_WRITE_KATAKANA, FUDE_TEXT_EXAM_WRITE_KANJI };
    return _group < FUDE_JA_GROUPS ? (u32)_prompts[_group] : (u32)FUDE_TEXT_COUNT;
}

// Returns whether a kana is one of the 71 core kana of its script (romaji.h).
b8 fude_lang_core(u32 _cp) {
    return fude_romaji_core(_cp);
}

// Returns false: no Japanese character is written on another.
b8 fude_lang_combining(u32 _cp) {
    RDE_UNUSED(_cp);
    return false;   // Every letter stands alone.
}

// Returns a kana's romaji, or a mark's short name (romaji.h).
const c8* fude_lang_latin(u32 _cp) {
    return fude_romaji(_cp);
}

// --- writing direction and joined forms: left to right, each letter as itself -------------

// Returns false: Japanese is written left to right here.
b8 fude_lang_rtl(void) {
    return false;
}

// Returns the letter itself: Japanese has no joined forms.
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

// --- levels: the JLPT's N5 (easiest) to N1, stored as 5..1 ------------------------------

// Returns the five JLPT levels.
u32 fude_lang_level_count(void) {
    return 5u;
}

// Returns the byte a level is stored as: the easiest, N5, is 5, down to N1 as 1; 0 past the last.
u8 fude_lang_level_value(u32 _level) {
    return _level < 5u ? (u8)(5u - _level) : 0u;
}

// Writes a stored level's name: "N5", or "JLPT N5" when _long is set; "" for no level.
void fude_lang_level_name(u8 _value, b8 _long, c8* _out, usize _size) {
    if(_value == 0u || _value > 5u) {   // The levels are JLPT N5 to N1; any other value is no level.
        rde_snprintf(_out, _size, "%s", "");
    } else {
        rde_snprintf(_out, _size, _long ? "JLPT N%u" : "N%u", (u32)_value);
    }
}

// --- readings: on, kun ---------------------------------------------------------------

// Returns the two kinds of reading: 0 is on, 1 is kun.
u32 fude_lang_reading_kinds(void) {
    return 2u;
}

// Returns a kind of reading's name for the sort chips ("On", "Kun"), as a FUDE_TEXT_ id.
u32 fude_lang_reading_name(u32 _kind) {
    return _kind == 0u ? (u32)FUDE_TEXT_SORT_ON : _kind == 1u ? (u32)FUDE_TEXT_SORT_KUN : (u32)FUDE_TEXT_COUNT;
}

// Turns a reading typed in romaji into hiragana (romaji.h).
b8 fude_lang_reading_from_latin(const c8* _latin, c8* _out, usize _size) {
    return fude_romaji_to_hiragana(_latin, _out, _size);
}

// Returns a character of a reading typed in capitals in katakana: CHEKKU is チェック.
u32 fude_lang_reading_capital(u32 _cp) {
    return (_cp >= 0x3041u && _cp <= 0x3096u) ? _cp + FUDE_ROMAJI_KATAKANA_SHIFT : _cp;   // Hiragana becomes katakana.
}

// Returns a reading's character as readings are compared: katakana as hiragana.
u32 fude_lang_reading_fold(u32 _cp) {
    return (_cp >= 0x30A1u && _cp <= 0x30F6u) ? _cp - FUDE_ROMAJI_KATAKANA_SHIFT : _cp;   // Katakana becomes hiragana.
}

// Returns the kind of reading a one-character word is read by: kun first, then on.
u32 fude_lang_word_reading_kind(u32 _order) {
    return _order == 0u ? 1u : 0u;   // Kun, then on.
}

// Returns false: words are read in kana, not in Latin letters.
b8 fude_lang_word_reading_latin(void) {
    return false;   // Words are read in kana.
}

// Returns whether _written is all kana, hiragana or katakana (ー and the iteration marks ゝ ゞ ヽ ヾ among them), and
// not empty: such a word reads as it is written.
b8 fude_lang_reads_as_written(const c8* _written) {
    const c8* _p   = _written != NULL ? _written : "";
    b8        _any = false;
    for(u32 _cp = fude_utf8_next(&_p); _cp != 0u; _cp = fude_utf8_next(&_p)) {
        if(!((_cp >= 0x3041u && _cp <= 0x309Fu) || (_cp >= 0x30A0u && _cp <= 0x30FFu))) {
            return false;
        }
        _any = true;
    }
    return _any;
}

// --- badges ----------------------------------------------------------------------------

// Returns the kanji a badge shows, in FUDE_LANG_BADGE_ order (音 訓 部 試 語 似 記 訳), or 0 out of range.
u32 fude_lang_badge(FUDE_LANG_BADGE_ _badge) {
    static const u32 _badges[FUDE_LANG_BADGE_COUNT] = {
        0x97F3u,   // 音
        0x8A13u,   // 訓
        0x90E8u,   // 部
        0x8A66u,   // 試
        0x8A9Eu,   // 語
        0x4F3Cu,   // 似
        0x8A18u,   // 記
        0x8A33u,   // 訳
    };
    return _badge < FUDE_LANG_BADGE_COUNT ? _badges[_badge] : 0u;
}

// --- handwriting: how free writing is read (lang.h: fude_lang_writing_def) ----------------------
// Kana and kanji written together: the shapes the scripts share (ロ 口), a word kept in one script, the small kana and
// where they can stand, the voicing marks, the marks 、。・ with their cells to themselves, and the kana out of use.
// The study's reader (study/handwriting/segment.c) applies them; Check and Paste text's reading are what they shape.

#ifndef FUDE_JA_SWITCH_KANA
#define FUDE_JA_SWITCH_KANA 12.0f    // hiragana next to katakana (a word is in one script)
#endif
#ifndef FUDE_JA_SWITCH_KANJI
// Katakana next to kanji pays this; hiragana next to kanji is free (okurigana, particles).
#define FUDE_JA_SWITCH_KANJI 6.0f    // katakana next to kanji
#endif
#define FUDE_JA_VOICE_SIZE  0.22f    // a dakuten tick or a handakuten ring: at most this × the line's height

// The scripts a character says the characters around it are in (fude_ja_script), as fude_ja_choose reads them.
enum { FUDE_JA_SCRIPT_KANJI = 0, FUDE_JA_SCRIPT_HIRAGANA, FUDE_JA_SCRIPT_KATAKANA };

// Look-alikes across scripts, katakana first: they have the same shape, so which one is meant is told by the
// characters around it.
static const u32 FUDE_JA_TWINS[][2] = {
    { 0x30D8, 0x3078 },   // ヘ へ
    { 0x30D9, 0x3079 },   // ベ べ
    { 0x30DA, 0x307A },   // ペ ぺ
    { 0x30FC, 0x4E00 },   // ー 一
    { 0x30AB, 0x529B },   // カ 力
    { 0x30A8, 0x5DE5 },   // エ 工
    { 0x30ED, 0x53E3 },   // ロ 口
    { 0x30CB, 0x4E8C },   // ニ 二
    { 0x30CF, 0x516B },   // ハ 八
    { 0x30BF, 0x5915 },   // タ 夕
    { 0x30C8, 0x535C },   // ト 卜
    { 0x30EA, 0x308A },   // リ り
    { 0x30CA, 0x5341 },   // ナ 十   (alike rather than the same)
    { 0x30C1, 0x5343 },   // チ 千
    { 0x30AA, 0x624D },   // オ 才
    { 0x30D2, 0x5315 },   // ヒ 匕
    { 0x30E0, 0x53B6 },   // ム 厶
    { 0x30CC, 0x53C8 },   // ヌ 又
    { 0x30CE, 0x4E3F },   // ノ 丿
    { 0x30EB, 0x513F },   // ル 儿
    { 0x30BB, 0x305B },   // セ せ
    { 0x30E2, 0x3082 },   // モ も
    { 0x30E4, 0x3084 },   // ヤ や
    { 0x30AD, 0x304D },   // キ き
};
#define FUDE_JA_TWIN_COUNT ((u32)(sizeof(FUDE_JA_TWINS) / sizeof(FUDE_JA_TWINS[0])))

// Returns the pair of FUDE_JA_TWINS that _cp is in, or NULL.
RDE_INTERNAL const u32* fude_ja_twin_pair(u32 _cp) {
    for(u32 _t = 0; _t < FUDE_JA_TWIN_COUNT; _t++) {
        if(_cp == FUDE_JA_TWINS[_t][0] || _cp == FUDE_JA_TWINS[_t][1]) {
            return FUDE_JA_TWINS[_t];
        }
    }
    return NULL;
}

// Returns _cp's look-alike in the other script (ロ for 口, 口 for ロ), or 0 when it has none.
RDE_INTERNAL u32 fude_ja_twin(u32 _cp) {
    const u32* _pair = fude_ja_twin_pair(_cp);
    return _pair == NULL ? 0u : _pair[_cp == _pair[0] ? 1 : 0];
}

// Returns the script _cp says the characters around it are in: hiragana, katakana or kanji (FUDE_JA_SCRIPT_); or
// FUDE_LANG_SCRIPT_NONE for anything else (a mark, ー).
RDE_INTERNAL u32 fude_ja_script(u32 _cp) {
    if(_cp >= 0x3041 && _cp <= 0x3096) { return FUDE_JA_SCRIPT_HIRAGANA; }
    if(_cp >= 0x30A1 && _cp <= 0x30FA) { return FUDE_JA_SCRIPT_KATAKANA; }
    if(_cp >= 0x4E00 && _cp <= 0x9FFF) { return FUDE_JA_SCRIPT_KANJI; }
    return FUDE_LANG_SCRIPT_NONE;
}

// Returns which of the look-alikes _cp and its twin is meant between characters in the scripts _before and _after: the
// katakana one next to katakana (ロボット) and the other otherwise (口); へ, which is a particle too, by what follows it
// (ヘン, but ロボットへ行く); ー only after katakana (コーヒー, but 一杯).
RDE_INTERNAL u32 fude_ja_choose(u32 _cp, u32 _before, u32 _after) {
    const u32* _pair = fude_ja_twin_pair(_cp);
    if(_pair == NULL) {
        return _cp;
    }
    const b8 _particle = _pair[1] == 0x3078;   // へ, the particle "to"
    b8       _want_kata;
    if(_pair[0] == 0x30FC) {
        _want_kata = _before == FUDE_JA_SCRIPT_KATAKANA;
    } else if(_particle && _after != FUDE_LANG_SCRIPT_NONE) {
        _want_kata = _after == FUDE_JA_SCRIPT_KATAKANA;
    } else {
        _want_kata = _before == FUDE_JA_SCRIPT_KATAKANA || _after == FUDE_JA_SCRIPT_KATAKANA;
    }
    return _pair[_want_kata ? 0 : 1];
}

// Returns a character's script for fude_ja_switch_cost: 0 hiragana, 1 katakana (ー too), 2 kanji, 3 anything else,
// 4 a hiragana particle (の と を は が に で へ も や か ね よ, which stand between katakana words all the time).
RDE_INTERNAL u32 fude_ja_script_of(u32 _cp) {
    switch(_cp) {
        case 0x306E: case 0x3068: case 0x3092: case 0x306F: case 0x304C: case 0x306B: case 0x3067:
        case 0x3078: case 0x3082: case 0x3084: case 0x304B: case 0x306D: case 0x3088:
            return 4u;
        default:
            break;
    }
    if(_cp >= 0x3041 && _cp <= 0x3096) { return 0u; }
    if(_cp >= 0x30A1 && _cp <= 0x30FC) { return 1u; }
    if((_cp >= 0x4E00 && _cp <= 0x9FFF) || (_cp >= 0x3400 && _cp <= 0x4DBF)) { return 2u; }
    return 3u;
}

// Returns what reading _b right after _a costs, for the scripts they are in (fude_ja_script_of).
RDE_INTERNAL f32 fude_ja_switch_cost(u32 _a_cp, u32 _b_cp) {
    const u32 _a = fude_ja_script_of(_a_cp);
    const u32 _b = fude_ja_script_of(_b_cp);
    if(_a == 4u || _b == 4u) {
        // A particle pays half the switch: it stands between katakana words (ツナのサンド), but is also a kana inside
        // hiragana ones (ありが).
        return (_a == 1u || _b == 1u) ? FUDE_JA_SWITCH_KANA * 0.5f : 0.0f;
    }
    if(_a == 3u || _b == 3u || _a == _b) {
        return 0.0f;
    }
    if(_a + _b == 1u) {                  // hiragana and katakana
        return FUDE_JA_SWITCH_KANA;
    }
    return (_a == 1u || _b == 1u) ? FUDE_JA_SWITCH_KANJI : 0.0f;
}

// Returns a small kana's full-size one, or the other way round (ゃ や, ッ ツ); 0 for anything else.
RDE_INTERNAL u32 fude_ja_size_twin(u32 _cp) {
    static const u32 _small[] = { 0x3041, 0x3043, 0x3045, 0x3047, 0x3049, 0x3063, 0x3083, 0x3085, 0x3087, 0x308E,
                                  0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30C3, 0x30E3, 0x30E5, 0x30E7, 0x30EE };
    for(u32 _i = 0; _i < sizeof(_small) / sizeof(_small[0]); _i++) {
        if(_cp == _small[_i])      { return _cp + 1u; }
        if(_cp == _small[_i] + 1u) { return _cp - 1u; }
    }
    switch(_cp) {
        case 0x3095: return 0x304B;   // ゕ か
        case 0x304B: return 0x3095;
        case 0x3096: return 0x3051;   // ゖ け
        case 0x3051: return 0x3096;
        case 0x30F5: return 0x30AB;   // ヵ カ
        case 0x30AB: return 0x30F5;
        case 0x30F6: return 0x30B1;   // ヶ ケ
        case 0x30B1: return 0x30F6;
        default:     return 0;
    }
}

// True when _cp is a small kana.
RDE_INTERNAL b8 fude_ja_small(u32 _cp) {
    const u32 _twin = fude_ja_size_twin(_cp);
    return _twin != 0 && (_twin == _cp + 1u || _cp == 0x3095 || _cp == 0x3096 || _cp == 0x30F5 || _cp == 0x30F6);
}

// True when _cp is one of the _count code points of _list.
RDE_INTERNAL b8 fude_ja_among(u32 _cp, const u32* _list, u32 _count) {
    for(u32 _k = 0; _k < _count; _k++) {
        if(_cp == _list[_k]) {
            return true;
        }
    }
    return false;
}

// True when the small kana _small can stand between _before and _after (0: nothing). ゃゅょ follow an i-row kana
// (きゃ, しょ; テュ, フュ in katakana). っ follows something and comes before a consonant: not a vowel, ん, a small kana
// or ー (あっい is あつい). ァィゥェォ follow the katakana they sound with (ティ, ファ, ウォ; not ドィ), and in hiragana
// hardly at all. ゎ, ゕ and ゖ are all but unused.
RDE_INTERNAL b8 fude_ja_small_fits(u32 _small, u32 _before, u32 _after) {
    const b8  _kata   = _small >= 0x30A1;
    const u32 _hira   = _kata ? _small - 0x60u : _small;
    const u32 _b      = _before >= 0x30A1 && _before <= 0x30F6 ? _before - 0x60u : _before;   // _before as hiragana
    const u32 _a      = _after >= 0x30A1 && _after <= 0x30F6 ? _after - 0x60u : _after;
    static const u32 _i_row[] = { 0x304D, 0x304E, 0x3057, 0x3058, 0x3061, 0x3062, 0x306B, 0x3072, 0x3073, 0x3074, 0x307F, 0x308A };
    // What っ cannot come before, as hiragana: a vowel, ん, or a small kana. ー is not in it, because it is not turned
    // into hiragana above (it is past ヶ, 0x30F6), so it is checked as itself.
    static const u32 _vowels[] = { 0x3042, 0x3044, 0x3046, 0x3048, 0x304A, 0x3093, 0x3041, 0x3043, 0x3045, 0x3047, 0x3049,
                                   0x3063, 0x3083, 0x3085, 0x3087 };
    switch(_hira) {
        case 0x3063:                               // っ
            return _before != 0 && !fude_ja_among(_a, _vowels, sizeof(_vowels) / sizeof(_vowels[0])) && _after != 0x30FC;   // nor ー
        case 0x3083: case 0x3085: case 0x3087:     // ゃ ゅ ょ
            return fude_ja_among(_b, _i_row, sizeof(_i_row) / sizeof(_i_row[0])) ||
                   (_kata && (_b == 0x3066 || _b == 0x3067 || _b == 0x3075 || _b == 0x3094));   // テュ デュ フュ ヴュ
        case 0x3041: case 0x3043: case 0x3045: case 0x3047: case 0x3049: {   // ァ ィ ゥ ェ ォ
            if(!_kata || !(_before >= 0x30A1 && _before <= 0x30FC)) {
                return false;
            }
            static const u32 _for_a[] = { 0x3075, 0x3094, 0x3064, 0x304F, 0x3050 };                                           // フ ヴ ツ ク グ
            static const u32 _for_i[] = { 0x3066, 0x3067, 0x3046, 0x3094, 0x3075, 0x3064, 0x304F, 0x3050, 0x3059, 0x305A };   // テ デ ウ ヴ フ ツ ク グ ス ズ
            static const u32 _for_u[] = { 0x3068, 0x3069, 0x3075 };                                                          // ト ド フ
            static const u32 _for_e[] = { 0x3057, 0x3058, 0x3061, 0x3046, 0x3094, 0x3075, 0x3064, 0x304F, 0x3050, 0x3044 };   // シ ジ チ ウ ヴ フ ツ ク グ イ
            static const u32 _for_o[] = { 0x3046, 0x3094, 0x3075, 0x3064, 0x304F, 0x3050 };                                   // ウ ヴ フ ツ ク グ
            switch(_hira) {
                case 0x3041: return fude_ja_among(_b, _for_a, sizeof(_for_a) / sizeof(_for_a[0]));
                case 0x3043: return fude_ja_among(_b, _for_i, sizeof(_for_i) / sizeof(_for_i[0]));
                case 0x3045: return fude_ja_among(_b, _for_u, sizeof(_for_u) / sizeof(_for_u[0]));
                case 0x3047: return fude_ja_among(_b, _for_e, sizeof(_for_e) / sizeof(_for_e[0]));
                default:     return fude_ja_among(_b, _for_o, sizeof(_for_o) / sizeof(_for_o[0]));
            }
        }
        default:
            return false;
    }
}

// Returns a base kana's voiced form: with _marks 2 its dakuten one (か が, ウ ヴ), with 1 its handakuten one (は ぱ);
// 0 when it has none.
RDE_INTERNAL u32 fude_ja_voiced(u32 _base, u32 _marks) {
    const b8  _kata = _base >= 0x30A1 && _base <= 0x30F6;
    const u32 _h    = _kata ? _base - 0x60u : _base;
    u32       _v    = 0;
    if(_marks == 2u) {
        if(_h >= 0x304B && _h <= 0x3061 && ((_h - 0x304B) % 2u) == 0u) { _v = _h + 1u; }   // か to ち
        else if(_h == 0x3064 || _h == 0x3066 || _h == 0x3068)           { _v = _h + 1u; }   // つ て と
        else if(_h >= 0x306F && _h <= 0x307B && ((_h - 0x306F) % 3u) == 0u) { _v = _h + 1u; } // は to ほ
        else if(_h == 0x3046)                                             { return _kata ? 0x30F4 : 0x3094; }   // う
    } else if(_marks == 1u) {
        if(_h >= 0x306F && _h <= 0x307B && ((_h - 0x306F) % 3u) == 0u) { _v = _h + 2u; }   // は to ほ
    }
    return _v != 0 && _kata ? _v + 0x60u : _v;
}

// True for the marks 、。・, which have a cell to themselves.
RDE_INTERNAL b8 fude_ja_mark(u32 _cp) {
    return _cp == 0x3001 || _cp == 0x3002 || _cp == 0x30FB;
}

// True for the kana out of use: ゐ ゑ ヰ ヱ ヷ ヸ ヹ ヺ ゔ ゎ ヮ ゕ ゖ ヵ ゟ ヿ.
RDE_INTERNAL b8 fude_ja_rare(u32 _cp) {
    switch(_cp) {
        case 0x3090: case 0x3091: case 0x30F0: case 0x30F1: case 0x30F7: case 0x30F8: case 0x30F9: case 0x30FA:
        case 0x3094: case 0x308E: case 0x30EE: case 0x3095: case 0x3096: case 0x30F5: case 0x309F: case 0x30FF:
            return true;
        default:
            return false;
    }
}

// Whether a character's place says where its line is. A small kana's and a mark's places are what is being judged
// (KanjiVG puts っ low and 、。 at the foot), so they are left out, with ー and the other signs before U+3040 (々).
RDE_INTERNAL b8 fude_ja_placed(u32 _cp) {
    return !fude_ja_small(_cp) && _cp >= 0x3040 && _cp != 0x30FC;
}

// Returns Japanese's rules for reading free writing (lang.h).
const fude_lang_writing_def* fude_lang_writing(void) {
    static const fude_lang_writing_def _writing = {
        .switch_cost = fude_ja_switch_cost,
        .twin        = fude_ja_twin,
        .script      = fude_ja_script,
        .choose      = fude_ja_choose,
        .size_twin   = fude_ja_size_twin,
        .small       = fude_ja_small,
        .small_fits  = fude_ja_small_fits,
        .voiced      = fude_ja_voiced,
        .voice_size  = FUDE_JA_VOICE_SIZE,
        .mark        = fude_ja_mark,
        .rare        = fude_ja_rare,
        .placed      = fude_ja_placed,
    };
    return &_writing;
}
