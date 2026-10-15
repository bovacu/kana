// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"
#include "drawing/base/text.h"
#include "drawing/base/utf8.h"

// ===========================================================================
// See lang.h. This is Arabic (Modern Standard Arabic, in the Naskh hand). Its letters are in four groups: the letters
// as they stand alone (the 28, the hamza and its seats, ة ى, the lām-alif); the joined forms each takes inside a word
// (initial, medial and final: Unicode's presentation forms, each with strokes of its own, and the lām-alif ligatures
// لا لآ لأ لإ, alone and joined); the vowel marks (harakat); and the digits. Each has its Latin name: a letter's
// (ب "bāʾ"); a joined form's, with where it joins, as textbooks write ـبـ ("bāʾ-" starts a word, "-bāʾ-" is inside
// one, "-bāʾ" ends one); a mark's ("fatḥa"); a digit's ("wāḥid"). Words are read in romanization. There are two
// levels, the letters' own: the alphabet, then the rest (the hamza's seats, ة ى, لا, the tanwīn).
//
// Two tables hold it all: FUDE_AR_TABLE, every letter, form, mark and digit with its group, level and Latin name, and
// FUDE_AR_FORMS_OF, each letter's joined forms, which fude_lang_form picks from by the letters around it.
// ===========================================================================

// Returns Arabic's code for translation (ML Kit Translate).
const c8* fude_lang_code(void)      { return "ar"; }
// Returns the ML Kit Digital Ink Recognition model that reads Arabic handwriting.
const c8* fude_lang_ink_model(void) { return "ar"; }
// Returns the voice that reads Arabic aloud.
const c8* fude_lang_voice(void)     { return "ar-SA"; }
// Returns false: Text from a photo is not offered for Arabic.
b8        fude_lang_text_readable(void) { return false; }   // ML Kit Text Recognition has no Arabic model.

// Returns Arabic as the app's fourth UI language, with its name in Arabic and its flag.
RDE_LANGUAGE_ fude_lang_ui_language(const c8** _name, const c8** _flag) {
    *_name = "\xD8\xA7\xD9\x84\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A\xD8\xA9";   // العربية
    *_flag = "assets/flags/ar.png";
    return RDE_LANGUAGE_AR_SA;
}

// --- the letters ---------------------------------------------------------------------------

// Arabic's groups, and how many there are.
enum { FUDE_AR_LETTERS = 0, FUDE_AR_FORMS, FUDE_AR_MARKS, FUDE_AR_DIGITS, FUDE_AR_GROUPS };

// One character: its code point, its group, its level (1 the alphabet, 2 the rest) and its Latin name (UTF-8).
typedef struct { u16 codepoint; u8 group; u8 level; const c8* latin; } fude_ar_letter;

// Every character, sorted by code point, which fude_ar_letter_of's bisection needs: the letters (U+0621..U+064A),
// the marks, the digits, then the joined forms (U+FE82..U+FEFC). The lām-alif ligatures come last; their alone forms
// are in the letters' group.
static const fude_ar_letter FUDE_AR_TABLE[] = {
    { 0x0621, FUDE_AR_LETTERS, 2, "hamza" },                     // ء
    { 0x0622, FUDE_AR_LETTERS, 2, "alif madda" },                // آ
    { 0x0623, FUDE_AR_LETTERS, 2, "alif hamza" },                // أ
    { 0x0624, FUDE_AR_LETTERS, 2, "w\xC4\x81w hamza" },          // ؤ
    { 0x0625, FUDE_AR_LETTERS, 2, "alif hamza ta\xE1\xB8\xA5t" },                // إ
    { 0x0626, FUDE_AR_LETTERS, 2, "y\xC4\x81\xCA\xBE hamza" },   // ئ
    { 0x0627, FUDE_AR_LETTERS, 1, "alif" },                      // ا
    { 0x0628, FUDE_AR_LETTERS, 1, "b\xC4\x81\xCA\xBE" },         // ب
    { 0x0629, FUDE_AR_LETTERS, 2, "t\xC4\x81\xCA\xBE marb\xC5\xAB\xE1\xB9\xAD" "a" }, // ة
    { 0x062A, FUDE_AR_LETTERS, 1, "t\xC4\x81\xCA\xBE" },         // ت
    { 0x062B, FUDE_AR_LETTERS, 1, "th\xC4\x81\xCA\xBE" },        // ث
    { 0x062C, FUDE_AR_LETTERS, 1, "j\xC4\xABm" },                // ج
    { 0x062D, FUDE_AR_LETTERS, 1, "\xE1\xB8\xA5\xC4\x81\xCA\xBE" }, // ح
    { 0x062E, FUDE_AR_LETTERS, 1, "kh\xC4\x81\xCA\xBE" },        // خ
    { 0x062F, FUDE_AR_LETTERS, 1, "d\xC4\x81l" },                // د
    { 0x0630, FUDE_AR_LETTERS, 1, "dh\xC4\x81l" },               // ذ
    { 0x0631, FUDE_AR_LETTERS, 1, "r\xC4\x81\xCA\xBE" },         // ر
    { 0x0632, FUDE_AR_LETTERS, 1, "z\xC4\x81y" },                // ز
    { 0x0633, FUDE_AR_LETTERS, 1, "s\xC4\xABn" },                // س
    { 0x0634, FUDE_AR_LETTERS, 1, "sh\xC4\xABn" },               // ش
    { 0x0635, FUDE_AR_LETTERS, 1, "\xE1\xB9\xA3\xC4\x81" "d" },  // ص
    { 0x0636, FUDE_AR_LETTERS, 1, "\xE1\xB8\x8D\xC4\x81" "d" },  // ض
    { 0x0637, FUDE_AR_LETTERS, 1, "\xE1\xB9\xAD\xC4\x81\xCA\xBE" }, // ط
    { 0x0638, FUDE_AR_LETTERS, 1, "\xE1\xBA\x93\xC4\x81\xCA\xBE" }, // ظ
    { 0x0639, FUDE_AR_LETTERS, 1, "\xCA\xBF" "ayn" },            // ع
    { 0x063A, FUDE_AR_LETTERS, 1, "ghayn" },                     // غ
    { 0x0641, FUDE_AR_LETTERS, 1, "f\xC4\x81\xCA\xBE" },         // ف
    { 0x0642, FUDE_AR_LETTERS, 1, "q\xC4\x81" "f" },             // ق
    { 0x0643, FUDE_AR_LETTERS, 1, "k\xC4\x81" "f" },             // ك
    { 0x0644, FUDE_AR_LETTERS, 1, "l\xC4\x81m" },                // ل
    { 0x0645, FUDE_AR_LETTERS, 1, "m\xC4\xABm" },                // م
    { 0x0646, FUDE_AR_LETTERS, 1, "n\xC5\xABn" },                // ن
    { 0x0647, FUDE_AR_LETTERS, 1, "h\xC4\x81\xCA\xBE" },         // ه
    { 0x0648, FUDE_AR_LETTERS, 1, "w\xC4\x81w" },                // و
    { 0x0649, FUDE_AR_LETTERS, 2, "alif maq\xE1\xB9\xA3\xC5\xABra" }, // ى
    { 0x064A, FUDE_AR_LETTERS, 1, "y\xC4\x81\xCA\xBE" },         // ي
    { 0x064B, FUDE_AR_MARKS,   2, "fat\xE1\xB8\xA5" "at\xC4\x81n" }, // ◌ً
    { 0x064C, FUDE_AR_MARKS,   2, "\xE1\xB8\x8D" "ammat\xC4\x81n" }, // ◌ٌ
    { 0x064D, FUDE_AR_MARKS,   2, "kasrat\xC4\x81n" },           // ◌ٍ
    { 0x064E, FUDE_AR_MARKS,   1, "fat\xE1\xB8\xA5" "a" },       // ◌َ
    { 0x064F, FUDE_AR_MARKS,   1, "\xE1\xB8\x8D" "amma" },       // ◌ُ
    { 0x0650, FUDE_AR_MARKS,   1, "kasra" },                     // ◌ِ
    { 0x0651, FUDE_AR_MARKS,   1, "shadda" },                    // ◌ّ
    { 0x0652, FUDE_AR_MARKS,   1, "suk\xC5\xABn" },              // ◌ْ
    { 0x0660, FUDE_AR_DIGITS,  1, "\xE1\xB9\xA3ifr" },           // ٠
    { 0x0661, FUDE_AR_DIGITS,  1, "w\xC4\x81\xE1\xB8\xA5id" },   // ١
    { 0x0662, FUDE_AR_DIGITS,  1, "ithn\xC4\x81n" },             // ٢
    { 0x0663, FUDE_AR_DIGITS,  1, "thal\xC4\x81tha" },           // ٣
    { 0x0664, FUDE_AR_DIGITS,  1, "arba\xCA\xBF" "a" },          // ٤
    { 0x0665, FUDE_AR_DIGITS,  1, "khamsa" },                    // ٥
    { 0x0666, FUDE_AR_DIGITS,  1, "sitta" },                     // ٦
    { 0x0667, FUDE_AR_DIGITS,  1, "sab\xCA\xBF" "a" },           // ٧
    { 0x0668, FUDE_AR_DIGITS,  1, "tham\xC4\x81niya" },          // ٨
    { 0x0669, FUDE_AR_DIGITS,  1, "tis\xCA\xBF" "a" },           // ٩
    { 0xFE82, FUDE_AR_FORMS,   2, "-alif madda" },               // ﺂ
    { 0xFE84, FUDE_AR_FORMS,   2, "-alif hamza" },               // ﺄ
    { 0xFE86, FUDE_AR_FORMS,   2, "-w\xC4\x81w hamza" },         // ﺆ
    { 0xFE88, FUDE_AR_FORMS,   2, "-alif hamza ta\xE1\xB8\xA5t" },               // ﺈ
    { 0xFE8A, FUDE_AR_FORMS,   2, "-y\xC4\x81\xCA\xBE hamza" },  // ﺊ
    { 0xFE8B, FUDE_AR_FORMS,   2, "y\xC4\x81\xCA\xBE hamza-" },  // ﺋ
    { 0xFE8C, FUDE_AR_FORMS,   2, "-y\xC4\x81\xCA\xBE hamza-" }, // ﺌ
    { 0xFE8E, FUDE_AR_FORMS,   1, "-alif" },                     // ﺎ
    { 0xFE90, FUDE_AR_FORMS,   1, "-b\xC4\x81\xCA\xBE" },        // ﺐ
    { 0xFE91, FUDE_AR_FORMS,   1, "b\xC4\x81\xCA\xBE-" },        // ﺑ
    { 0xFE92, FUDE_AR_FORMS,   1, "-b\xC4\x81\xCA\xBE-" },       // ﺒ
    { 0xFE94, FUDE_AR_FORMS,   2, "-t\xC4\x81\xCA\xBE marb\xC5\xAB\xE1\xB9\xAD" "a" }, // ﺔ
    { 0xFE96, FUDE_AR_FORMS,   1, "-t\xC4\x81\xCA\xBE" },        // ﺖ
    { 0xFE97, FUDE_AR_FORMS,   1, "t\xC4\x81\xCA\xBE-" },        // ﺗ
    { 0xFE98, FUDE_AR_FORMS,   1, "-t\xC4\x81\xCA\xBE-" },       // ﺘ
    { 0xFE9A, FUDE_AR_FORMS,   1, "-th\xC4\x81\xCA\xBE" },       // ﺚ
    { 0xFE9B, FUDE_AR_FORMS,   1, "th\xC4\x81\xCA\xBE-" },       // ﺛ
    { 0xFE9C, FUDE_AR_FORMS,   1, "-th\xC4\x81\xCA\xBE-" },      // ﺜ
    { 0xFE9E, FUDE_AR_FORMS,   1, "-j\xC4\xABm" },               // ﺞ
    { 0xFE9F, FUDE_AR_FORMS,   1, "j\xC4\xABm-" },               // ﺟ
    { 0xFEA0, FUDE_AR_FORMS,   1, "-j\xC4\xABm-" },              // ﺠ
    { 0xFEA2, FUDE_AR_FORMS,   1, "-\xE1\xB8\xA5\xC4\x81\xCA\xBE" }, // ﺢ
    { 0xFEA3, FUDE_AR_FORMS,   1, "\xE1\xB8\xA5\xC4\x81\xCA\xBE-" }, // ﺣ
    { 0xFEA4, FUDE_AR_FORMS,   1, "-\xE1\xB8\xA5\xC4\x81\xCA\xBE-" }, // ﺤ
    { 0xFEA6, FUDE_AR_FORMS,   1, "-kh\xC4\x81\xCA\xBE" },       // ﺦ
    { 0xFEA7, FUDE_AR_FORMS,   1, "kh\xC4\x81\xCA\xBE-" },       // ﺧ
    { 0xFEA8, FUDE_AR_FORMS,   1, "-kh\xC4\x81\xCA\xBE-" },      // ﺨ
    { 0xFEAA, FUDE_AR_FORMS,   1, "-d\xC4\x81l" },               // ﺪ
    { 0xFEAC, FUDE_AR_FORMS,   1, "-dh\xC4\x81l" },              // ﺬ
    { 0xFEAE, FUDE_AR_FORMS,   1, "-r\xC4\x81\xCA\xBE" },        // ﺮ
    { 0xFEB0, FUDE_AR_FORMS,   1, "-z\xC4\x81y" },               // ﺰ
    { 0xFEB2, FUDE_AR_FORMS,   1, "-s\xC4\xABn" },               // ﺲ
    { 0xFEB3, FUDE_AR_FORMS,   1, "s\xC4\xABn-" },               // ﺳ
    { 0xFEB4, FUDE_AR_FORMS,   1, "-s\xC4\xABn-" },              // ﺴ
    { 0xFEB6, FUDE_AR_FORMS,   1, "-sh\xC4\xABn" },              // ﺶ
    { 0xFEB7, FUDE_AR_FORMS,   1, "sh\xC4\xABn-" },              // ﺷ
    { 0xFEB8, FUDE_AR_FORMS,   1, "-sh\xC4\xABn-" },             // ﺸ
    { 0xFEBA, FUDE_AR_FORMS,   1, "-\xE1\xB9\xA3\xC4\x81" "d" }, // ﺺ
    { 0xFEBB, FUDE_AR_FORMS,   1, "\xE1\xB9\xA3\xC4\x81" "d-" }, // ﺻ
    { 0xFEBC, FUDE_AR_FORMS,   1, "-\xE1\xB9\xA3\xC4\x81" "d-" }, // ﺼ
    { 0xFEBE, FUDE_AR_FORMS,   1, "-\xE1\xB8\x8D\xC4\x81" "d" }, // ﺾ
    { 0xFEBF, FUDE_AR_FORMS,   1, "\xE1\xB8\x8D\xC4\x81" "d-" }, // ﺿ
    { 0xFEC0, FUDE_AR_FORMS,   1, "-\xE1\xB8\x8D\xC4\x81" "d-" }, // ﻀ
    { 0xFEC2, FUDE_AR_FORMS,   1, "-\xE1\xB9\xAD\xC4\x81\xCA\xBE" }, // ﻂ
    { 0xFEC3, FUDE_AR_FORMS,   1, "\xE1\xB9\xAD\xC4\x81\xCA\xBE-" }, // ﻃ
    { 0xFEC4, FUDE_AR_FORMS,   1, "-\xE1\xB9\xAD\xC4\x81\xCA\xBE-" }, // ﻄ
    { 0xFEC6, FUDE_AR_FORMS,   1, "-\xE1\xBA\x93\xC4\x81\xCA\xBE" }, // ﻆ
    { 0xFEC7, FUDE_AR_FORMS,   1, "\xE1\xBA\x93\xC4\x81\xCA\xBE-" }, // ﻇ
    { 0xFEC8, FUDE_AR_FORMS,   1, "-\xE1\xBA\x93\xC4\x81\xCA\xBE-" }, // ﻈ
    { 0xFECA, FUDE_AR_FORMS,   1, "-\xCA\xBF" "ayn" },           // ﻊ
    { 0xFECB, FUDE_AR_FORMS,   1, "\xCA\xBF" "ayn-" },           // ﻋ
    { 0xFECC, FUDE_AR_FORMS,   1, "-\xCA\xBF" "ayn-" },          // ﻌ
    { 0xFECE, FUDE_AR_FORMS,   1, "-ghayn" },                    // ﻎ
    { 0xFECF, FUDE_AR_FORMS,   1, "ghayn-" },                    // ﻏ
    { 0xFED0, FUDE_AR_FORMS,   1, "-ghayn-" },                   // ﻐ
    { 0xFED2, FUDE_AR_FORMS,   1, "-f\xC4\x81\xCA\xBE" },        // ﻒ
    { 0xFED3, FUDE_AR_FORMS,   1, "f\xC4\x81\xCA\xBE-" },        // ﻓ
    { 0xFED4, FUDE_AR_FORMS,   1, "-f\xC4\x81\xCA\xBE-" },       // ﻔ
    { 0xFED6, FUDE_AR_FORMS,   1, "-q\xC4\x81" "f" },            // ﻖ
    { 0xFED7, FUDE_AR_FORMS,   1, "q\xC4\x81" "f-" },            // ﻗ
    { 0xFED8, FUDE_AR_FORMS,   1, "-q\xC4\x81" "f-" },           // ﻘ
    { 0xFEDA, FUDE_AR_FORMS,   1, "-k\xC4\x81" "f" },            // ﻚ
    { 0xFEDB, FUDE_AR_FORMS,   1, "k\xC4\x81" "f-" },            // ﻛ
    { 0xFEDC, FUDE_AR_FORMS,   1, "-k\xC4\x81" "f-" },           // ﻜ
    { 0xFEDE, FUDE_AR_FORMS,   1, "-l\xC4\x81m" },               // ﻞ
    { 0xFEDF, FUDE_AR_FORMS,   1, "l\xC4\x81m-" },               // ﻟ
    { 0xFEE0, FUDE_AR_FORMS,   1, "-l\xC4\x81m-" },              // ﻠ
    { 0xFEE2, FUDE_AR_FORMS,   1, "-m\xC4\xABm" },               // ﻢ
    { 0xFEE3, FUDE_AR_FORMS,   1, "m\xC4\xABm-" },               // ﻣ
    { 0xFEE4, FUDE_AR_FORMS,   1, "-m\xC4\xABm-" },              // ﻤ
    { 0xFEE6, FUDE_AR_FORMS,   1, "-n\xC5\xABn" },               // ﻦ
    { 0xFEE7, FUDE_AR_FORMS,   1, "n\xC5\xABn-" },               // ﻧ
    { 0xFEE8, FUDE_AR_FORMS,   1, "-n\xC5\xABn-" },              // ﻨ
    { 0xFEEA, FUDE_AR_FORMS,   1, "-h\xC4\x81\xCA\xBE" },        // ﻪ
    { 0xFEEB, FUDE_AR_FORMS,   1, "h\xC4\x81\xCA\xBE-" },        // ﻫ
    { 0xFEEC, FUDE_AR_FORMS,   1, "-h\xC4\x81\xCA\xBE-" },       // ﻬ
    { 0xFEEE, FUDE_AR_FORMS,   1, "-w\xC4\x81w" },               // ﻮ
    { 0xFEF0, FUDE_AR_FORMS,   2, "-alif maq\xE1\xB9\xA3\xC5\xABra" }, // ﻰ
    { 0xFEF2, FUDE_AR_FORMS,   1, "-y\xC4\x81\xCA\xBE" },        // ﻲ
    { 0xFEF3, FUDE_AR_FORMS,   1, "y\xC4\x81\xCA\xBE-" },        // ﻳ
    { 0xFEF4, FUDE_AR_FORMS,   1, "-y\xC4\x81\xCA\xBE-" },       // ﻴ
    { 0xFEF5, FUDE_AR_LETTERS, 2, "l\xC4\x81m alif madda" },     // ﻵ
    { 0xFEF6, FUDE_AR_FORMS,   2, "-l\xC4\x81m alif madda" },    // ﻶ
    { 0xFEF7, FUDE_AR_LETTERS, 2, "l\xC4\x81m alif hamza" },     // ﻷ
    { 0xFEF8, FUDE_AR_FORMS,   2, "-l\xC4\x81m alif hamza" },    // ﻸ
    { 0xFEF9, FUDE_AR_LETTERS, 2, "l\xC4\x81m alif hamza ta\xE1\xB8\xA5t" }, // ﻹ
    { 0xFEFA, FUDE_AR_FORMS,   2, "-l\xC4\x81m alif hamza ta\xE1\xB8\xA5t" }, // ﻺ
    { 0xFEFB, FUDE_AR_LETTERS, 2, "l\xC4\x81m alif" },           // ﻻ
    { 0xFEFC, FUDE_AR_FORMS,   2, "-l\xC4\x81m alif" },          // ﻼ
};

// How many characters the table has.
#define FUDE_AR_LETTER_COUNT ((u32)(sizeof(FUDE_AR_TABLE) / sizeof(FUDE_AR_TABLE[0])))

// Returns _cp's row of the table, found by bisection, or NULL when it is not in it.
RDE_INTERNAL const fude_ar_letter* fude_ar_letter_of(u32 _cp) {
    u32 _lo = 0, _hi = FUDE_AR_LETTER_COUNT;
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(FUDE_AR_TABLE[_mid].codepoint == _cp) {
            return &FUDE_AR_TABLE[_mid];
        }
        if(FUDE_AR_TABLE[_mid].codepoint < _cp) { _lo = _mid + 1u; } else { _hi = _mid; }
    }
    return NULL;
}

// --- groups ----------------------------------------------------------------------------------

// Returns how many groups there are: four.
u32 fude_lang_group_count(void) {
    return FUDE_AR_GROUPS;
}

// Returns a character's group, as the table says; FUDE_LANG_NO_GROUP for anything else.
u8 fude_lang_group(u32 _cp) {
    const fude_ar_letter* _l = fude_ar_letter_of(_cp);
    return _l != NULL ? _l->group : FUDE_LANG_NO_GROUP;
}

// Returns a group's flags: every group is a script's letters, learnt as a whole set.
u32 fude_lang_group_flags(u32 _group) {
    return _group < FUDE_AR_GROUPS ? FUDE_LANG_GROUP_SCRIPT | FUDE_LANG_GROUP_SET : 0u;   // Letters, each learnt whole.
}

// Returns a group's name as a FUDE_TEXT_ id. A character in no group is shown under the letters' name.
u32 fude_lang_group_name(u32 _group) {
    static const FUDE_TEXT_ _names[FUDE_AR_GROUPS] = { FUDE_TEXT_AR_LETTERS, FUDE_TEXT_AR_FORMS, FUDE_TEXT_AR_MARKS, FUDE_TEXT_AR_DIGITS };
    return _group < FUDE_AR_GROUPS ? (u32)_names[_group] : _group == FUDE_LANG_NO_GROUP ? (u32)FUDE_TEXT_AR_LETTERS : (u32)FUDE_TEXT_COUNT;
}

// Returns what an exam asks for a group ("write the letter"...), as a FUDE_TEXT_ id.
u32 fude_lang_group_prompt(u32 _group) {
    static const FUDE_TEXT_ _prompts[FUDE_AR_GROUPS] = { FUDE_TEXT_EXAM_WRITE_AR_LETTER, FUDE_TEXT_EXAM_WRITE_AR_FORM, FUDE_TEXT_EXAM_WRITE_AR_MARK, FUDE_TEXT_EXAM_WRITE_AR_DIGIT };
    return _group < FUDE_AR_GROUPS ? (u32)_prompts[_group] : (u32)FUDE_TEXT_COUNT;
}

// Returns whether a character is a core one: the alphabet, the characters of level 1.
b8 fude_lang_core(u32 _cp) {
    const fude_ar_letter* _l = fude_ar_letter_of(_cp);
    return _l != NULL && _l->level == 1u;
}

// Returns whether a character is written over or under a letter: the harakat (U+064B..U+0652) and the superscript
// alif (U+0670).
b8 fude_lang_combining(u32 _cp) {
    return (_cp >= 0x064Bu && _cp <= 0x0652u) || _cp == 0x0670u;
}

// Returns a character's Latin name from the table (ب is "bāʾ"), or NULL.
const c8* fude_lang_latin(u32 _cp) {
    const fude_ar_letter* _l = fude_ar_letter_of(_cp);
    return _l != NULL ? _l->latin : NULL;
}

// --- writing direction and joined forms -------------------------------------------------------

// Returns true: Arabic is written right to left.
b8 fude_lang_rtl(void) {
    return true;
}

// Each letter's forms in a word, as presentation forms (0 when it has none): final, initial, medial. A letter with an
// initial form joins the letter after it, and one with a final form joins the one before it. ء joins neither, and
// ا د ذ ر ز و ة ى join only the one before.
typedef struct { u16 letter; u16 final; u16 initial; u16 medial; } fude_ar_forms;
static const fude_ar_forms FUDE_AR_FORMS_OF[] = {
    { 0x0621, 0x0000, 0x0000, 0x0000 },   // ء
    { 0x0622, 0xFE82, 0x0000, 0x0000 },   // آ
    { 0x0623, 0xFE84, 0x0000, 0x0000 },   // أ
    { 0x0624, 0xFE86, 0x0000, 0x0000 },   // ؤ
    { 0x0625, 0xFE88, 0x0000, 0x0000 },   // إ
    { 0x0626, 0xFE8A, 0xFE8B, 0xFE8C },   // ئ
    { 0x0627, 0xFE8E, 0x0000, 0x0000 },   // ا
    { 0x0628, 0xFE90, 0xFE91, 0xFE92 },   // ب
    { 0x0629, 0xFE94, 0x0000, 0x0000 },   // ة
    { 0x062A, 0xFE96, 0xFE97, 0xFE98 },   // ت
    { 0x062B, 0xFE9A, 0xFE9B, 0xFE9C },   // ث
    { 0x062C, 0xFE9E, 0xFE9F, 0xFEA0 },   // ج
    { 0x062D, 0xFEA2, 0xFEA3, 0xFEA4 },   // ح
    { 0x062E, 0xFEA6, 0xFEA7, 0xFEA8 },   // خ
    { 0x062F, 0xFEAA, 0x0000, 0x0000 },   // د
    { 0x0630, 0xFEAC, 0x0000, 0x0000 },   // ذ
    { 0x0631, 0xFEAE, 0x0000, 0x0000 },   // ر
    { 0x0632, 0xFEB0, 0x0000, 0x0000 },   // ز
    { 0x0633, 0xFEB2, 0xFEB3, 0xFEB4 },   // س
    { 0x0634, 0xFEB6, 0xFEB7, 0xFEB8 },   // ش
    { 0x0635, 0xFEBA, 0xFEBB, 0xFEBC },   // ص
    { 0x0636, 0xFEBE, 0xFEBF, 0xFEC0 },   // ض
    { 0x0637, 0xFEC2, 0xFEC3, 0xFEC4 },   // ط
    { 0x0638, 0xFEC6, 0xFEC7, 0xFEC8 },   // ظ
    { 0x0639, 0xFECA, 0xFECB, 0xFECC },   // ع
    { 0x063A, 0xFECE, 0xFECF, 0xFED0 },   // غ
    { 0x0641, 0xFED2, 0xFED3, 0xFED4 },   // ف
    { 0x0642, 0xFED6, 0xFED7, 0xFED8 },   // ق
    { 0x0643, 0xFEDA, 0xFEDB, 0xFEDC },   // ك
    { 0x0644, 0xFEDE, 0xFEDF, 0xFEE0 },   // ل
    { 0x0645, 0xFEE2, 0xFEE3, 0xFEE4 },   // م
    { 0x0646, 0xFEE6, 0xFEE7, 0xFEE8 },   // ن
    { 0x0647, 0xFEEA, 0xFEEB, 0xFEEC },   // ه
    { 0x0648, 0xFEEE, 0x0000, 0x0000 },   // و
    { 0x0649, 0xFEF0, 0x0000, 0x0000 },   // ى
    { 0x064A, 0xFEF2, 0xFEF3, 0xFEF4 },   // ي
};

// Returns letter _cp's row of FUDE_AR_FORMS_OF, or NULL when it is not a letter there (a digit, a mark, a form).
RDE_INTERNAL const fude_ar_forms* fude_ar_forms_of(u32 _cp) {
    for(u32 _i = 0; _i < (u32)(sizeof(FUDE_AR_FORMS_OF) / sizeof(FUDE_AR_FORMS_OF[0])); _i++) {
        if(FUDE_AR_FORMS_OF[_i].letter == _cp) {
            return &FUDE_AR_FORMS_OF[_i];
        }
    }
    return NULL;
}

// Returns whether letter _cp joins the letter after it, which it does when it has an initial form.
RDE_INTERNAL b8 fude_ar_joins_after(u32 _cp) {
    const fude_ar_forms* _f = fude_ar_forms_of(_cp);
    return _f != NULL && _f->initial != 0u;
}

// Returns whether letter _cp joins the letter before it, which it does when it has a final form.
RDE_INTERNAL b8 fude_ar_joins_before(u32 _cp) {
    const fude_ar_forms* _f = fude_ar_forms_of(_cp);
    return _f != NULL && _f->final != 0u;
}

// Returns the form letter _cp takes between _before and _after: joined to the letter before when that one joins
// after it and _cp has a final form, and joined to the letter after when _cp has an initial form and that one joins
// before it. Medial when both, final or initial when one, alone when neither. ل before an alif becomes a lām-alif
// ligature, with *_with_after set; the ligature's final form is the code point after its alone form.
u32 fude_lang_form(u32 _before, u32 _cp, u32 _after, b8* _with_after) {
    if(_with_after != NULL) {
        *_with_after = false;
    }
    const fude_ar_forms* _f = fude_ar_forms_of(_cp);
    if(_f == NULL) {
        return _cp;   // A digit, a mark, or a form already.
    }
    const b8 _joined_before = _before != 0u && fude_ar_joins_after(_before) && _f->final != 0u;
    // ل followed by ا, آ, أ or إ is written together with it, as a ligature (ﻻ ﻵ ﻷ ﻹ).
    if(_cp == 0x0644u && (_after == 0x0627u || _after == 0x0622u || _after == 0x0623u || _after == 0x0625u)) {
        if(_with_after != NULL) {
            *_with_after = true;
        }
        const u32 _alone = _after == 0x0627u ? 0xFEFBu : _after == 0x0622u ? 0xFEF5u : _after == 0x0623u ? 0xFEF7u : 0xFEF9u;
        return _joined_before ? _alone + 1u : _alone;
    }
    const b8 _joined_after = _after != 0u && _f->initial != 0u && fude_ar_joins_before(_after);
    if(_joined_before && _joined_after) {
        return _f->medial;
    }
    if(_joined_before) {
        return _f->final;
    }
    if(_joined_after) {
        return _f->initial;
    }
    return _cp;
}

// Returns the letter a joined form is a form of, from FUDE_AR_FORMS_OF; a lām-alif ligature gives its ل. Anything
// below the presentation forms (U+FE70) is returned as it is.
u32 fude_lang_letter(u32 _cp) {
    if(_cp >= 0xFEF5u && _cp <= 0xFEFCu) {
        return 0x0644u;
    }
    if(_cp < 0xFE70u) {
        return _cp;
    }
    for(u32 _i = 0; _i < (u32)(sizeof(FUDE_AR_FORMS_OF) / sizeof(FUDE_AR_FORMS_OF[0])); _i++) {
        const fude_ar_forms* _f = &FUDE_AR_FORMS_OF[_i];
        if(_f->final == _cp || _f->initial == _cp || _f->medial == _cp) {
            return _f->letter;
        }
    }
    return _cp;
}

// --- levels: the alphabet, then the rest -----------------------------------------------------------

// Returns the two levels: the alphabet, then the rest.
u32 fude_lang_level_count(void) {
    return 2u;
}

// Returns the byte a level is stored as: its place plus one (1 or 2); 0 past the last.
u8 fude_lang_level_value(u32 _level) {
    return _level < 2u ? (u8)(_level + 1u) : 0u;
}

// Writes a stored level's name in the app's language, short or long; "" for no level.
void fude_lang_level_name(u8 _value, b8 _long, c8* _out, usize _size) {
    static const FUDE_TEXT_ _short[2] = { FUDE_TEXT_AR_LEVEL_1_SHORT, FUDE_TEXT_AR_LEVEL_2_SHORT };
    static const FUDE_TEXT_ _names[2] = { FUDE_TEXT_AR_LEVEL_1, FUDE_TEXT_AR_LEVEL_2 };
    if(_value == 0u || _value > 2u) {
        rde_snprintf(_out, _size, "%s", "");
    } else {
        rde_snprintf(_out, _size, "%s", fude_text(_long ? _names[_value - 1u] : _short[_value - 1u]));
    }
}

// --- readings: a letter has none of its own (its Latin name says it); words are read in romanization ---

// Returns 0: the character data has no readings.
u32 fude_lang_reading_kinds(void) {
    return 0u;
}

// Returns FUDE_TEXT_COUNT, since there are no kinds of reading to name.
u32 fude_lang_reading_name(u32 _kind) {
    RDE_UNUSED(_kind);
    return (u32)FUDE_TEXT_COUNT;
}

// Returns _cp as it is.
u32 fude_lang_reading_capital(u32 _cp) {
    return _cp;   // Arabic has no other way of writing a reading typed in capitals.
}

// Writes "" and returns false: a reading typed in Latin letters is not turned into Arabic.
b8 fude_lang_reading_from_latin(const c8* _latin, c8* _out, usize _size) {
    RDE_UNUSED(_latin);
    if(_out != NULL && _size > 0) {
        _out[0] = 0;
    }
    return false;   // A romanization is kept as typed, since words are read in it.
}

// Returns a romanization's character as readings are compared: without its accents and marks, and with hyphens left
// out (ʿilm compares as ilm; the same folding makes Thai's paa-sǎa paasaa and Hindi's pānī pani).
u32 fude_lang_reading_fold(u32 _cp) {
    return _cp == '-' ? 0u : fude_utf8_latin_base(_cp);
}

// Returns kind 0; there are no kinds of reading to choose from.
u32 fude_lang_word_reading_kind(u32 _order) {
    RDE_UNUSED(_order);
    return 0u;
}

// Returns true: words are read in romanization.
b8 fude_lang_word_reading_latin(void) {
    return true;   // Words are read in romanization: كتاب is kitāb.
}

// Returns false: no word is read as it is written, since its words are read in Latin letters
// (fude_lang_word_reading_latin).
b8 fude_lang_reads_as_written(const c8* _written) {
    RDE_UNUSED(_written);
    return false;
}

// --- badges: Arabic letters, each the start of the word it stands for ------------------------------

// Returns the letter a badge shows, in FUDE_LANG_BADGE_ order, or 0 out of range. Each is the first letter of the
// Arabic word for what the badge marks.
u32 fude_lang_badge(FUDE_LANG_BADGE_ _badge) {
    static const u32 _badges[FUDE_LANG_BADGE_COUNT] = {
        0x0635u,   // ص (صوت: sound)
        0x0645u,   // م (معنى: meaning)
        0x062Cu,   // ج (جزء: part)
        0x0641u,   // ف (فحص: test)
        0x0643u,   // ك (كلمة: word)
        0x0634u,   // ش (شبيه: alike)
        0x062Du,   // ح (حاشية: note)
        0x062Au,   // ت (ترجمة: translation)
    };
    return _badge < FUDE_LANG_BADGE_COUNT ? _badges[_badge] : 0u;
}

// --- handwriting: how free writing is read ---------------------------------------------------

// Returns no rules (lang.h): Arabic's free writing is read by each letter's shape, size and place alone.
const fude_lang_writing_def* fude_lang_writing(void) {
    static const fude_lang_writing_def _none = { .switch_cost = NULL };
    return &_none;
}
