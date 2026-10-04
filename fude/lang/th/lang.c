#include "lang/lang.h"
#include "drawing/base/text.h"
#include "drawing/base/utf8.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See lang.h: Thai. The Thai script's letters in four groups — the consonants
// (44, and two obsolete), the vowels (their signs above, below, before and after
// the consonant, and the vowel letters ฤ ฦ), the tone marks and other signs,
// the digits — each with its Latin name: a consonant's (ก "ko kai"), a vowel's
// (า "sara a"), a mark's (่ "mai ek"), a digit's (๑ "nueng"). Words are read in
// romanization (the Royal Thai General System, with tones where the dictionary
// gives them). Three levels, the letters' own: the ones learnt first, the rest,
// and the rare and obsolete.
// ===========================================================================

const c8* fude_lang_code(void)      { return "th"; }
const c8* fude_lang_ink_model(void) { return "th"; }
const c8* fude_lang_voice(void)     { return "th-TH"; }
b8        fude_lang_text_readable(void) { return false; }   // ML Kit Text Recognition has no Thai model

RDE_LANGUAGE_ fude_lang_ui_language(const c8** _name, const c8** _flag) {
    *_name = "\xE0\xB9\x84\xE0\xB8\x97\xE0\xB8\xA2";   // ไทย
    *_flag = "assets/flags/th.png";
    return RDE_LANGUAGE_TH_TH;
}

// --- the letters ---------------------------------------------------------------------------

#define FUDE_TH_FIRST 0x0E01u   // ก
#define FUDE_TH_LAST  0x0E5Bu   // ๛

enum { FUDE_TH_CONSONANTS = 0, FUDE_TH_VOWELS, FUDE_TH_MARKS, FUDE_TH_DIGITS, FUDE_TH_GROUPS };

// Each code point of the block: its group (FUDE_LANG_NO_GROUP: none), its level
// (1 first, 2 the rest, 3 rare or obsolete) and its Latin name.
typedef struct { u8 group; u8 level; const c8* latin; } fude_th_letter;

#define C(_l, _n) { FUDE_TH_CONSONANTS, _l, _n }
#define V(_l, _n) { FUDE_TH_VOWELS, _l, _n }
#define M(_l, _n) { FUDE_TH_MARKS, _l, _n }
#define D(_n)     { FUDE_TH_DIGITS, 1, _n }
#define X         { FUDE_LANG_NO_GROUP, 0, NULL }

static const fude_th_letter FUDE_TH_LETTERS[FUDE_TH_LAST - FUDE_TH_FIRST + 1u] = {
    C(1, "ko kai"),      C(1, "kho khai"),     C(3, "kho khuat"),   C(1, "kho khwai"),   C(3, "kho khon"),     // ก ข ฃ ค ฅ
    C(2, "kho rakhang"), C(1, "ngo ngu"),      C(1, "cho chan"),    C(2, "cho ching"),   C(1, "cho chang"),    // ฆ ง จ ฉ ช
    C(1, "so so"),       C(2, "cho choe"),     C(2, "yo ying"),     C(2, "do chada"),    C(2, "to patak"),     // ซ ฌ ญ ฎ ฏ
    C(2, "tho than"),    C(2, "tho montho"),   C(2, "tho phuthao"), C(2, "no nen"),      C(1, "do dek"),       // ฐ ฑ ฒ ณ ด
    C(1, "to tao"),      C(1, "tho thung"),    C(1, "tho thahan"),  C(2, "tho thong"),   C(1, "no nu"),        // ต ถ ท ธ น
    C(1, "bo baimai"),   C(1, "po pla"),       C(1, "pho phueng"),  C(1, "fo fa"),       C(1, "pho phan"),     // บ ป ผ ฝ พ
    C(1, "fo fan"),      C(2, "pho samphao"),  C(1, "mo ma"),       C(1, "yo yak"),      C(1, "ro ruea"),      // ฟ ภ ม ย ร
    V(2, "rue"),         C(1, "lo ling"),      V(3, "lue"),         C(1, "wo waen"),     C(2, "so sala"),      // ฤ ล ฦ ว ศ
    C(2, "so ruesi"),    C(1, "so suea"),      C(1, "ho hip"),      C(2, "lo chula"),    C(1, "o ang"),        // ษ ส ห ฬ อ
    C(2, "ho nokhuk"),   M(2, "paiyannoi"),    V(1, "sara a"),      V(1, "mai han-akat"), V(1, "sara aa"),     // ฮ ฯ ะ ั า
    V(1, "sara am"),     V(1, "sara i"),       V(1, "sara ii"),     V(1, "sara ue"),     V(1, "sara uee"),     // ำ ิ ี ึ ื
    V(1, "sara u"),      V(1, "sara uu"),      M(3, "phinthu"),     X,                   X,                    // ุ ู ฺ (U+0E3B..3E: none)
    X,                   X,                    X,                   V(1, "sara e"),      V(1, "sara ae"),      // ฿ (a symbol) เ แ
    V(1, "sara o"),      V(1, "sara ai mai muan"), V(1, "sara ai mai malai"), V(3, "lakkhangyao"), M(2, "mai yamok"),   // โ ใ ไ ๅ ๆ
    V(2, "mai taikhu"),  M(1, "mai ek"),       M(1, "mai tho"),     M(1, "mai tri"),     M(1, "mai chattawa"), // ็ ่ ้ ๊ ๋
    M(2, "thanthakhat"), M(3, "nikhahit"),     M(3, "yamakkan"),    X,                                         // ์ ํ ๎ ๏ (a symbol)
    D("sun"), D("nueng"), D("song"), D("sam"), D("si"), D("ha"), D("hok"), D("chet"), D("paet"), D("kao"),     // ๐ ... ๙
    X,                   X,                                                                                    // ๚ ๛
};

#undef C
#undef V
#undef M
#undef D
#undef X

RDE_INTERNAL const fude_th_letter* fude_th_letter_of(u32 _cp) {
    return _cp >= FUDE_TH_FIRST && _cp <= FUDE_TH_LAST ? &FUDE_TH_LETTERS[_cp - FUDE_TH_FIRST] : NULL;
}

// --- groups ----------------------------------------------------------------------------------

u32 fude_lang_group_count(void) {
    return FUDE_TH_GROUPS;
}

u8 fude_lang_group(u32 _cp) {
    const fude_th_letter* _l = fude_th_letter_of(_cp);
    return _l != NULL ? _l->group : FUDE_LANG_NO_GROUP;
}

u32 fude_lang_group_flags(u32 _group) {
    return _group < FUDE_TH_GROUPS ? FUDE_LANG_GROUP_SCRIPT | FUDE_LANG_GROUP_SET : 0u;   // letters, each learnt whole
}

u32 fude_lang_group_name(u32 _group) {
    static const FUDE_TEXT_ _names[FUDE_TH_GROUPS] = { FUDE_TEXT_TH_CONSONANTS, FUDE_TEXT_TH_VOWELS, FUDE_TEXT_TH_MARKS, FUDE_TEXT_TH_DIGITS };
    return _group < FUDE_TH_GROUPS ? (u32)_names[_group] : _group == FUDE_LANG_NO_GROUP ? (u32)FUDE_TEXT_TH_CONSONANTS : (u32)FUDE_TEXT_COUNT;
}

u32 fude_lang_group_prompt(u32 _group) {
    static const FUDE_TEXT_ _prompts[FUDE_TH_GROUPS] = { FUDE_TEXT_EXAM_WRITE_TH_CONSONANT, FUDE_TEXT_EXAM_WRITE_TH_VOWEL, FUDE_TEXT_EXAM_WRITE_TH_MARK, FUDE_TEXT_EXAM_WRITE_TH_DIGIT };
    return _group < FUDE_TH_GROUPS ? (u32)_prompts[_group] : (u32)FUDE_TEXT_COUNT;
}

// The core: what a learner learns first (level 1).
b8 fude_lang_core(u32 _cp) {
    const fude_th_letter* _l = fude_th_letter_of(_cp);
    return _l != NULL && _l->group != FUDE_LANG_NO_GROUP && _l->level == 1u;
}

// The vowel signs above and below a consonant, and the tone marks and signs over it.
b8 fude_lang_combining(u32 _cp) {
    return _cp == 0x0E31u || (_cp >= 0x0E34u && _cp <= 0x0E3Au) || (_cp >= 0x0E47u && _cp <= 0x0E4Eu);
}

const c8* fude_lang_latin(u32 _cp) {
    const fude_th_letter* _l = fude_th_letter_of(_cp);
    return _l != NULL ? _l->latin : NULL;
}

// The level a letter is listed at (the bake's: prepare.py writes it into the data).
u8 fude_th_level(u32 _cp) {
    const fude_th_letter* _l = fude_th_letter_of(_cp);
    return _l != NULL ? _l->level : 0u;
}


// --- writing direction and joined forms: left to right, each letter as itself -------------

b8 fude_lang_rtl(void) {
    return false;
}

u32 fude_lang_form(u32 _before, u32 _cp, u32 _after, b8* _with_after) {
    RDE_UNUSED(_before);
    RDE_UNUSED(_after);
    if(_with_after != NULL) {
        *_with_after = false;
    }
    return _cp;
}

u32 fude_lang_letter(u32 _cp) {
    return _cp;
}

// --- levels: first, then, rare -------------------------------------------------------------------

u32 fude_lang_level_count(void) {
    return 3u;
}

u8 fude_lang_level_value(u32 _level) {
    return _level < 3u ? (u8)(_level + 1u) : 0u;
}

void fude_lang_level_name(u8 _value, b8 _long, c8* _out, usize _size) {
    static const FUDE_TEXT_ _short[3] = { FUDE_TEXT_TH_LEVEL_1_SHORT, FUDE_TEXT_TH_LEVEL_2_SHORT, FUDE_TEXT_TH_LEVEL_3_SHORT };
    static const FUDE_TEXT_ _names[3] = { FUDE_TEXT_TH_LEVEL_1, FUDE_TEXT_TH_LEVEL_2, FUDE_TEXT_TH_LEVEL_3 };
    if(_value == 0u || _value > 3u) {
        snprintf(_out, _size, "%s", "");
    } else {
        snprintf(_out, _size, "%s", fude_text(_long ? _names[_value - 1u] : _short[_value - 1u]));
    }
}

// --- readings: none of a letter's own (its name is its Latin name); words are read in romanization --

u32 fude_lang_reading_kinds(void) {
    return 0u;
}

u32 fude_lang_reading_name(u32 _kind) {
    RDE_UNUSED(_kind);
    return (u32)FUDE_TEXT_COUNT;
}

b8 fude_lang_reading_from_latin(const c8* _latin, c8* _out, usize _size) {
    RDE_UNUSED(_latin);
    if(_out != NULL && _size > 0) {
        _out[0] = 0;
    }
    return false;   // romanization is not turned back into Thai: it says too little (no tones, no vowel length)
}

// Romanization searched without its accents, tones or syllables' hyphens
// (paa-sǎa as paasaa, pānī as pani, ʿilm as ilm).
u32 fude_lang_reading_fold(u32 _cp) {
    return _cp == '-' ? 0u : fude_utf8_latin_base(_cp);
}

u32 fude_lang_word_reading_kind(u32 _order) {
    RDE_UNUSED(_order);
    return 0u;
}

b8 fude_lang_word_reading_latin(void) {
    return true;   // words are read in romanization: ภาษา phasa
}

// --- badges: Thai letters, each the start of the word it stands for ---------------------------

u32 fude_lang_badge(FUDE_LANG_BADGE_ _badge) {
    static const u32 _badges[FUDE_LANG_BADGE_COUNT] = {
        0x0E2Au,   // ส (เสียง: sound)
        0x0E0Au,   // ช (ชื่อ: name)
        0x0E1Bu,   // ป (ประกอบ: parts)
        0x0E17u,   // ท (ทดสอบ: test)
        0x0E04u,   // ค (คำ: word)
        0x0E21u,   // ม (เหมือน: alike)
        0x0E1Au,   // บ (บันทึก: note)
    };
    return _badge < FUDE_LANG_BADGE_COUNT ? _badges[_badge] : 0u;
}
