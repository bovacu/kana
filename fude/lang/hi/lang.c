#include "lang/lang.h"
#include "drawing/base/text.h"
#include "drawing/base/utf8.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// See lang.h: Hindi, in Devanagari. Its letters in four groups — the vowels
// (written alone, at a word's start), the consonants (each with its inherent
// a: क "ka"; and the ones with a dot under them for other languages' sounds,
// क़ "qa"), the vowel signs and marks (the matras a consonant wears, ा ि ी ...,
// and ं ँ ः ़ ्), the digits — each with its Latin name, as Hindi is taught to
// learners (Hunterian, with ISO 15919's dots for the retroflex sounds: ट ṭa).
// Words are read in romanization. Three levels, the letters' own: the ones
// learnt first, the rest, the rare.
// ===========================================================================

const c8* fude_lang_code(void)      { return "hi"; }
const c8* fude_lang_ink_model(void) { return "hi"; }
const c8* fude_lang_voice(void)     { return "hi-IN"; }
b8        fude_lang_text_readable(void) { return true; }    // ML Kit's Devanagari model

RDE_LANGUAGE_ fude_lang_ui_language(const c8** _name, const c8** _flag) {
    *_name = "\xE0\xA4\xB9\xE0\xA4\xBF\xE0\xA4\x82\xE0\xA4\xA6\xE0\xA5\x80";   // हिंदी
    *_flag = "assets/flags/in.png";
    return RDE_LANGUAGE_HI_IN;
}

// --- the letters ---------------------------------------------------------------------------

enum { FUDE_HI_VOWELS = 0, FUDE_HI_CONSONANTS, FUDE_HI_SIGNS, FUDE_HI_DIGITS, FUDE_HI_GROUPS };

typedef struct { u16 codepoint; u8 group; u8 level; const c8* latin; } fude_hi_letter;

// By code point (searched by halves).
static const fude_hi_letter FUDE_HI_LETTERS[] = {
    { 0x0901, FUDE_HI_SIGNS,      2, "candrabindu" },   // ँ
    { 0x0902, FUDE_HI_SIGNS,      1, "anusv\xC4\x81ra" },   // ं
    { 0x0903, FUDE_HI_SIGNS,      2, "visarga" },       // ः
    { 0x0905, FUDE_HI_VOWELS,     1, "a" },             // अ
    { 0x0906, FUDE_HI_VOWELS,     1, "\xC4\x81" },      // आ ā
    { 0x0907, FUDE_HI_VOWELS,     1, "i" },             // इ
    { 0x0908, FUDE_HI_VOWELS,     1, "\xC4\xAB" },      // ई ī
    { 0x0909, FUDE_HI_VOWELS,     1, "u" },             // उ
    { 0x090A, FUDE_HI_VOWELS,     1, "\xC5\xAB" },      // ऊ ū
    { 0x090B, FUDE_HI_VOWELS,     2, "\xE1\xB9\x9B" },  // ऋ ṛ
    { 0x090C, FUDE_HI_VOWELS,     3, "\xE1\xB8\xB7" },  // ऌ ḷ
    { 0x090D, FUDE_HI_VOWELS,     3, "\xC3\xAA" },      // ऍ ê
    { 0x090F, FUDE_HI_VOWELS,     1, "e" },             // ए
    { 0x0910, FUDE_HI_VOWELS,     1, "ai" },            // ऐ
    { 0x0911, FUDE_HI_VOWELS,     2, "\xC3\xB4" },      // ऑ ô
    { 0x0913, FUDE_HI_VOWELS,     1, "o" },             // ओ
    { 0x0914, FUDE_HI_VOWELS,     1, "au" },            // औ
    { 0x0915, FUDE_HI_CONSONANTS, 1, "ka" },            // क
    { 0x0916, FUDE_HI_CONSONANTS, 1, "kha" },           // ख
    { 0x0917, FUDE_HI_CONSONANTS, 1, "ga" },            // ग
    { 0x0918, FUDE_HI_CONSONANTS, 1, "gha" },           // घ
    { 0x0919, FUDE_HI_CONSONANTS, 2, "\xE1\xB9\x85" "a" },   // ङ ṅa
    { 0x091A, FUDE_HI_CONSONANTS, 1, "cha" },           // च
    { 0x091B, FUDE_HI_CONSONANTS, 1, "chha" },          // छ
    { 0x091C, FUDE_HI_CONSONANTS, 1, "ja" },            // ज
    { 0x091D, FUDE_HI_CONSONANTS, 1, "jha" },           // झ
    { 0x091E, FUDE_HI_CONSONANTS, 2, "\xC3\xB1" "a" },  // ञ ña
    { 0x091F, FUDE_HI_CONSONANTS, 1, "\xE1\xB9\xAD" "a" },   // ट ṭa
    { 0x0920, FUDE_HI_CONSONANTS, 1, "\xE1\xB9\xAD" "ha" },  // ठ ṭha
    { 0x0921, FUDE_HI_CONSONANTS, 1, "\xE1\xB8\x8D" "a" },   // ड ḍa
    { 0x0922, FUDE_HI_CONSONANTS, 1, "\xE1\xB8\x8D" "ha" },  // ढ ḍha
    { 0x0923, FUDE_HI_CONSONANTS, 1, "\xE1\xB9\x87" "a" },   // ण ṇa
    { 0x0924, FUDE_HI_CONSONANTS, 1, "ta" },            // त
    { 0x0925, FUDE_HI_CONSONANTS, 1, "tha" },           // थ
    { 0x0926, FUDE_HI_CONSONANTS, 1, "da" },            // द
    { 0x0927, FUDE_HI_CONSONANTS, 1, "dha" },           // ध
    { 0x0928, FUDE_HI_CONSONANTS, 1, "na" },            // न
    { 0x0929, FUDE_HI_CONSONANTS, 3, "\xE1\xB9\x89" "a" },   // ऩ ṉa
    { 0x092A, FUDE_HI_CONSONANTS, 1, "pa" },            // प
    { 0x092B, FUDE_HI_CONSONANTS, 1, "pha" },           // फ
    { 0x092C, FUDE_HI_CONSONANTS, 1, "ba" },            // ब
    { 0x092D, FUDE_HI_CONSONANTS, 1, "bha" },           // भ
    { 0x092E, FUDE_HI_CONSONANTS, 1, "ma" },            // म
    { 0x092F, FUDE_HI_CONSONANTS, 1, "ya" },            // य
    { 0x0930, FUDE_HI_CONSONANTS, 1, "ra" },            // र
    { 0x0931, FUDE_HI_CONSONANTS, 3, "\xE1\xB9\x9F" "a" },   // ऱ ṟa
    { 0x0932, FUDE_HI_CONSONANTS, 1, "la" },            // ल
    { 0x0933, FUDE_HI_CONSONANTS, 3, "\xE1\xB8\xB7" "a" },   // ळ ḷa
    { 0x0934, FUDE_HI_CONSONANTS, 3, "\xE1\xB8\xBB" "a" },   // ऴ ḻa
    { 0x0935, FUDE_HI_CONSONANTS, 1, "va" },            // व
    { 0x0936, FUDE_HI_CONSONANTS, 1, "sha" },           // श
    { 0x0937, FUDE_HI_CONSONANTS, 1, "\xE1\xB9\xA3" "ha" },  // ष ṣha
    { 0x0938, FUDE_HI_CONSONANTS, 1, "sa" },            // स
    { 0x0939, FUDE_HI_CONSONANTS, 1, "ha" },            // ह
    { 0x093C, FUDE_HI_SIGNS,      2, "nuqt\xC4\x81" },  // ़
    { 0x093D, FUDE_HI_SIGNS,      3, "avagraha" },      // ऽ
    { 0x093E, FUDE_HI_SIGNS,      1, "\xC4\x81" },      // ा ā
    { 0x093F, FUDE_HI_SIGNS,      1, "i" },             // ि
    { 0x0940, FUDE_HI_SIGNS,      1, "\xC4\xAB" },      // ी ī
    { 0x0941, FUDE_HI_SIGNS,      1, "u" },             // ु
    { 0x0942, FUDE_HI_SIGNS,      1, "\xC5\xAB" },      // ू ū
    { 0x0943, FUDE_HI_SIGNS,      2, "\xE1\xB9\x9B" },  // ृ ṛ
    { 0x0945, FUDE_HI_SIGNS,      3, "\xC3\xAA" },      // ॅ ê
    { 0x0947, FUDE_HI_SIGNS,      1, "e" },             // े
    { 0x0948, FUDE_HI_SIGNS,      1, "ai" },            // ै
    { 0x0949, FUDE_HI_SIGNS,      2, "\xC3\xB4" },      // ॉ ô
    { 0x094B, FUDE_HI_SIGNS,      1, "o" },             // ो
    { 0x094C, FUDE_HI_SIGNS,      1, "au" },            // ौ
    { 0x094D, FUDE_HI_SIGNS,      1, "vir\xC4\x81m" },  // ्
    { 0x0950, FUDE_HI_SIGNS,      3, "om" },            // ॐ
    { 0x0958, FUDE_HI_CONSONANTS, 2, "qa" },            // क़
    { 0x0959, FUDE_HI_CONSONANTS, 3, "\xE1\xB8\xAB" "a" },   // ख़ ḫa
    { 0x095A, FUDE_HI_CONSONANTS, 3, "\xC4\xA1" "a" },  // ग़ ġa
    { 0x095B, FUDE_HI_CONSONANTS, 2, "za" },            // ज़
    { 0x095C, FUDE_HI_CONSONANTS, 2, "\xE1\xB9\x9B" "a" },   // ड़ ṛa
    { 0x095D, FUDE_HI_CONSONANTS, 2, "\xE1\xB9\x9B" "ha" },  // ढ़ ṛha
    { 0x095E, FUDE_HI_CONSONANTS, 2, "fa" },            // फ़
    { 0x095F, FUDE_HI_CONSONANTS, 3, "\xE1\xBA\x8F" "a" },   // य़ ẏa
    { 0x0964, FUDE_HI_SIGNS,      2, "da\xE1\xB9\x87\xE1\xB8\x8D" "a" },   // । daṇḍa
    { 0x0965, FUDE_HI_SIGNS,      3, "dohr\xC4\x81 da\xE1\xB9\x87\xE1\xB8\x8D" "a" },   // ॥
    { 0x0966, FUDE_HI_DIGITS,     1, "\xC5\x9B\xC5\xABnya" },   // ० śūnya
    { 0x0967, FUDE_HI_DIGITS,     1, "ek" },            // १
    { 0x0968, FUDE_HI_DIGITS,     1, "do" },            // २
    { 0x0969, FUDE_HI_DIGITS,     1, "t\xC4\xABn" },    // ३ tīn
    { 0x096A, FUDE_HI_DIGITS,     1, "ch\xC4\x81r" },   // ४ chār
    { 0x096B, FUDE_HI_DIGITS,     1, "p\xC4\x81\xCC\x83" "ch" },   // ५ pā̃ch
    { 0x096C, FUDE_HI_DIGITS,     1, "chhah" },         // ६
    { 0x096D, FUDE_HI_DIGITS,     1, "s\xC4\x81t" },    // ७ sāt
    { 0x096E, FUDE_HI_DIGITS,     1, "\xC4\x81\xE1\xB9\xAD" "h" },   // ८ āṭh
    { 0x096F, FUDE_HI_DIGITS,     1, "nau" },           // ९
};

#define FUDE_HI_LETTER_COUNT ((u32)(sizeof(FUDE_HI_LETTERS) / sizeof(FUDE_HI_LETTERS[0])))

RDE_INTERNAL const fude_hi_letter* fude_hi_letter_of(u32 _cp) {
    u32 _lo = 0, _hi = FUDE_HI_LETTER_COUNT;
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(FUDE_HI_LETTERS[_mid].codepoint == _cp) {
            return &FUDE_HI_LETTERS[_mid];
        }
        if(FUDE_HI_LETTERS[_mid].codepoint < _cp) { _lo = _mid + 1u; } else { _hi = _mid; }
    }
    return NULL;
}

// --- groups ----------------------------------------------------------------------------------

u32 fude_lang_group_count(void) {
    return FUDE_HI_GROUPS;
}

u8 fude_lang_group(u32 _cp) {
    const fude_hi_letter* _l = fude_hi_letter_of(_cp);
    return _l != NULL ? _l->group : FUDE_LANG_NO_GROUP;
}

u32 fude_lang_group_flags(u32 _group) {
    return _group < FUDE_HI_GROUPS ? FUDE_LANG_GROUP_SCRIPT | FUDE_LANG_GROUP_SET : 0u;   // letters, each learnt whole
}

u32 fude_lang_group_name(u32 _group) {
    static const FUDE_TEXT_ _names[FUDE_HI_GROUPS] = { FUDE_TEXT_HI_VOWELS, FUDE_TEXT_HI_CONSONANTS, FUDE_TEXT_HI_SIGNS, FUDE_TEXT_HI_DIGITS };
    return _group < FUDE_HI_GROUPS ? (u32)_names[_group] : _group == FUDE_LANG_NO_GROUP ? (u32)FUDE_TEXT_HI_CONSONANTS : (u32)FUDE_TEXT_COUNT;
}

u32 fude_lang_group_prompt(u32 _group) {
    static const FUDE_TEXT_ _prompts[FUDE_HI_GROUPS] = { FUDE_TEXT_EXAM_WRITE_HI_VOWEL, FUDE_TEXT_EXAM_WRITE_HI_CONSONANT, FUDE_TEXT_EXAM_WRITE_HI_SIGN, FUDE_TEXT_EXAM_WRITE_HI_DIGIT };
    return _group < FUDE_HI_GROUPS ? (u32)_prompts[_group] : (u32)FUDE_TEXT_COUNT;
}

b8 fude_lang_core(u32 _cp) {
    const fude_hi_letter* _l = fude_hi_letter_of(_cp);
    return _l != NULL && _l->level == 1u;
}

// The vowel signs (matras), the nasal marks, the visarga, the nukta and the virama:
// each written on, beside or under a consonant.
b8 fude_lang_combining(u32 _cp) {
    return (_cp >= 0x0901u && _cp <= 0x0903u) || _cp == 0x093Cu || (_cp >= 0x093Eu && _cp <= 0x094Du) ||
           (_cp >= 0x0951u && _cp <= 0x0957u) || _cp == 0x0962u || _cp == 0x0963u;
}

const c8* fude_lang_latin(u32 _cp) {
    const fude_hi_letter* _l = fude_hi_letter_of(_cp);
    return _l != NULL ? _l->latin : NULL;
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
    static const FUDE_TEXT_ _short[3] = { FUDE_TEXT_HI_LEVEL_1_SHORT, FUDE_TEXT_HI_LEVEL_2_SHORT, FUDE_TEXT_HI_LEVEL_3_SHORT };
    static const FUDE_TEXT_ _names[3] = { FUDE_TEXT_HI_LEVEL_1, FUDE_TEXT_HI_LEVEL_2, FUDE_TEXT_HI_LEVEL_3 };
    if(_value == 0u || _value > 3u) {
        snprintf(_out, _size, "%s", "");
    } else {
        snprintf(_out, _size, "%s", fude_text(_long ? _names[_value - 1u] : _short[_value - 1u]));
    }
}

// --- readings: none of a letter's own (its Latin name says it); words are read in romanization ---

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
    return false;   // romanization is kept as typed (words are read in it)
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
    return true;   // words are read in romanization: हिंदी hindī
}

// --- badges: Devanagari letters, each the start of the word it stands for ------------------------

u32 fude_lang_badge(FUDE_LANG_BADGE_ _badge) {
    static const u32 _badges[FUDE_LANG_BADGE_COUNT] = {
        0x0927u,   // ध (ध्वनि: sound)
        0x0928u,   // न (नाम: name)
        0x092Du,   // भ (भाग: part)
        0x092Au,   // प (परीक्षा: exam)
        0x0936u,   // श (शब्द: word)
        0x0938u,   // स (समान: alike)
        0x091Fu,   // ट (टिप्पणी: note)
        0x0905u,   // अ (अनुवाद: translation)
    };
    return _badge < FUDE_LANG_BADGE_COUNT ? _badges[_badge] : 0u;
}
