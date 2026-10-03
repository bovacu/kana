#include "lang/lang.h"
#include "lang/ja/romaji.h"
#include "drawing/base/text.h"

#include <stdio.h>

// ===========================================================================
// See lang.h: Japanese. Hiragana, katakana and kanji; the JLPT's five levels;
// on and kun readings; romaji.
// ===========================================================================

const c8* fude_lang_code(void)      { return "ja"; }
const c8* fude_lang_ink_model(void) { return "ja"; }
const c8* fude_lang_voice(void)     { return "ja-JP"; }

RDE_LANGUAGE_ fude_lang_ui_language(const c8** _name, const c8** _flag) {
    *_name = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";   // 日本語
    *_flag = "assets/flags/jp.png";
    return RDE_LANGUAGE_JA_JP;
}

// --- groups ------------------------------------------------------------------------------

enum { FUDE_JA_HIRAGANA = 0, FUDE_JA_KATAKANA, FUDE_JA_KANJI, FUDE_JA_GROUPS };

u32 fude_lang_group_count(void) {
    return FUDE_JA_GROUPS;
}

u8 fude_lang_group(u32 _cp) {
    if(_cp >= 0x3041u && _cp <= 0x3096u) { return FUDE_JA_HIRAGANA; }
    if(_cp >= 0x30A1u && _cp <= 0x30FAu) { return FUDE_JA_KATAKANA; }
    if((_cp >= 0x4E00u && _cp <= 0x9FFFu) || (_cp >= 0x3400u && _cp <= 0x4DBFu) ||
       (_cp >= 0xF900u && _cp <= 0xFAFFu) || (_cp >= 0x20000u && _cp <= 0x2FFFFu)) {
        return FUDE_JA_KANJI;
    }
    return FUDE_LANG_NO_GROUP;
}

u32 fude_lang_group_flags(u32 _group) {
    return _group == FUDE_JA_KANJI ? 0u : FUDE_LANG_GROUP_SCRIPT | FUDE_LANG_GROUP_SET;
}

u32 fude_lang_group_name(u32 _group) {
    static const FUDE_TEXT_ _names[FUDE_JA_GROUPS] = { FUDE_TEXT_HIRAGANA, FUDE_TEXT_KATAKANA, FUDE_TEXT_KANJI };
    return _group < FUDE_JA_GROUPS ? (u32)_names[_group] : _group == FUDE_LANG_NO_GROUP ? (u32)FUDE_TEXT_KANJI : (u32)FUDE_TEXT_COUNT;   // 々: as a kanji
}

u32 fude_lang_group_prompt(u32 _group) {
    static const FUDE_TEXT_ _prompts[FUDE_JA_GROUPS] = { FUDE_TEXT_EXAM_WRITE_HIRAGANA, FUDE_TEXT_EXAM_WRITE_KATAKANA, FUDE_TEXT_EXAM_WRITE_KANJI };
    return _group < FUDE_JA_GROUPS ? (u32)_prompts[_group] : (u32)FUDE_TEXT_COUNT;
}

b8 fude_lang_core(u32 _cp) {
    return fude_romaji_core(_cp);
}

const c8* fude_lang_latin(u32 _cp) {
    return fude_romaji(_cp);
}

// --- levels: the JLPT's N5 (easiest) to N1, stored as 5..1 ------------------------------

u32 fude_lang_level_count(void) {
    return 5u;
}

u8 fude_lang_level_value(u32 _level) {
    return _level < 5u ? (u8)(5u - _level) : 0u;
}

void fude_lang_level_name(u8 _value, b8 _long, c8* _out, usize _size) {
    if(_value == 0u) {
        snprintf(_out, _size, "%s", "");
    } else {
        snprintf(_out, _size, _long ? "JLPT N%u" : "N%u", (u32)_value);
    }
}

// --- readings: on, kun ---------------------------------------------------------------

u32 fude_lang_reading_kinds(void) {
    return 2u;
}

u32 fude_lang_reading_name(u32 _kind) {
    return _kind == 0u ? (u32)FUDE_TEXT_SORT_ON : _kind == 1u ? (u32)FUDE_TEXT_SORT_KUN : (u32)FUDE_TEXT_COUNT;
}

b8 fude_lang_reading_from_latin(const c8* _latin, c8* _out, usize _size) {
    return fude_romaji_to_hiragana(_latin, _out, _size);
}

u32 fude_lang_reading_fold(u32 _cp) {
    return (_cp >= 0x30A1u && _cp <= 0x30F6u) ? _cp - FUDE_ROMAJI_KATAKANA_SHIFT : _cp;   // katakana → hiragana
}

u32 fude_lang_word_reading_kind(u32 _order) {
    return _order == 0u ? 1u : 0u;   // kun, then on
}

b8 fude_lang_word_reading_latin(void) {
    return false;   // in kana
}

// --- badges ----------------------------------------------------------------------------

u32 fude_lang_badge(FUDE_LANG_BADGE_ _badge) {
    static const u32 _badges[FUDE_LANG_BADGE_COUNT] = {
        0x97F3u,   // 音
        0x8A13u,   // 訓
        0x90E8u,   // 部
        0x8A66u,   // 試
        0x8A9Eu,   // 語
        0x4F3Cu,   // 似
        0x8A18u,   // 記
    };
    return _badge < FUDE_LANG_BADGE_COUNT ? _badges[_badge] : 0u;
}
