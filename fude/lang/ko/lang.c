// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"
#include "drawing/base/text.h"
#include "drawing/base/utf8.h"

#include "lang/ko/hangul.h"
#include "lang/ko/syllables.h"

// ===========================================================================
// See lang.h. This is Korean: three groups, the jamo (Hangul's letters), the syllables they make, and the hanja; the
// levels of NIKL's Basic Korean Dictionary (beginner, intermediate, advanced); a hanja's sound (음) and meaning word
// (훈) as its two kinds of reading; and the Revised Romanization of Korean (고시 2014-42), for the letters' Latin names
// and for typing Korean in Latin letters. Syllables are put together and taken apart by hangul.h's arithmetic.
// ===========================================================================

// Returns Korean's code for translation (ML Kit Translate).
const c8* fude_lang_code(void)      { return "ko"; }
// Returns the ML Kit Digital Ink Recognition model that reads Korean handwriting.
const c8* fude_lang_ink_model(void) { return "ko"; }
// Returns the voice that reads Korean aloud.
const c8* fude_lang_voice(void)     { return "ko-KR"; }
// Returns true: ML Kit Text Recognition can read Korean in pictures.
b8        fude_lang_text_readable(void) { return true; }

// Returns Korean as the app's fourth UI language, with its name in Korean and South Korea's flag.
RDE_LANGUAGE_ fude_lang_ui_language(const c8** _name, const c8** _flag) {
    *_name = "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4";   // 한국어
    *_flag = "assets/flags/kr.png";
    return RDE_LANGUAGE_KO_KR;
}

// --- the jamo, and the syllables they make ------------------------------------------------

// The romanization of a syllable's letters, by their numbers in Unicode's order (hangul.h): initial, medial, final.
// The initial ㅇ (11) is silent, so it has none.
static const c8* const FUDE_KO_INITIAL_LATIN[19] = {
    "g", "kk", "n", "d", "tt", "r", "m", "b", "pp", "s", "ss", "", "j", "jj", "ch", "k", "t", "p", "h",
};
static const c8* const FUDE_KO_MEDIAL_LATIN[FUDE_KO_MEDIALS] = {
    "a", "ae", "ya", "yae", "eo", "e", "yeo", "ye", "o", "wa", "wae", "oe", "yo", "u", "wo", "we", "wi", "yu", "eu", "ui", "i",
};
// A final as a syllable said alone ends in it. There are only seven such sounds (k n t l m p ng): ㅅ ㅆ ㅈ ㅊ ㅌ ㅎ all
// end in t.
static const c8* const FUDE_KO_FINAL_LATIN[FUDE_KO_FINALS] = {
    "", "k", "k", "k", "n", "n", "n", "t", "l", "k", "m", "l", "l", "l", "p", "l", "m", "p", "p", "t", "t", "ng", "t", "t", "k", "t", "p", "t",
};
// Each compatibility jamo's Latin name, from ㄱ U+3131 to ㅣ U+3163. A consonant said differently at the start and the
// end of a syllable gets both ("g/k").
static const c8* const FUDE_KO_JAMO_LATIN[FUDE_KO_JAMO_LAST - FUDE_KO_JAMO_FIRST + 1u] = {
    "g/k", "kk", "gs", "n", "nj", "nh", "d/t", "tt", "r/l", "lg", "lm", "lb", "ls", "lt", "lp", "lh",   // ㄱ ㄲ ㄳ ㄴ ㄵ ㄶ ㄷ ㄸ ㄹ ㄺ ...
    "m", "b/p", "pp", "bs", "s", "ss", "ng", "j", "jj", "ch", "k", "t", "p", "h",                       // ㅁ ㅂ ㅃ ㅄ ㅅ ㅆ ㅇ ㅈ ㅉ ㅊ ㅋ ㅌ ㅍ ㅎ
    "a", "ae", "ya", "yae", "eo", "e", "yeo", "ye", "o", "wa", "wae", "oe", "yo", "u", "wo", "we", "wi", "yu", "eu", "ui", "i",
};
// The final clusters (ㄳ ㄵ ㄶ ㄺ ㄻ ㄼ ㄽ ㄾ ㄿ ㅀ ㅄ): jamo, but not letters learnt on their own, so not core.
static const u32 FUDE_KO_CLUSTERS[] = { 0x3133u, 0x3135u, 0x3136u, 0x313Au, 0x313Bu, 0x313Cu, 0x313Du, 0x313Eu, 0x313Fu, 0x3140u, 0x3144u };

// --- groups: jamo, syllables, hanja ------------------------------------------------------

// Korean's groups, and how many there are. A syllable that syllables.h does not list (outside KS X 1001's 2,350, in
// no word of the dictionary and not on the chart, such as 겱) is in no group: it can be written but is listed nowhere,
// and it is still named a syllable (fude_lang_group_name).
enum { FUDE_KO_JAMO = 0, FUDE_KO_SYLLABLES, FUDE_KO_HANJA, FUDE_KO_GROUPS };

// Returns how many groups there are: three.
u32 fude_lang_group_count(void) {
    return FUDE_KO_GROUPS;
}

// Returns a character's group: a compatibility jamo, a syllable that syllables.h lists (one bit for each syllable
// from U+AC00), or a hanja for any Han character. Anything else is in no group.
u8 fude_lang_group(u32 _cp) {
    if(_cp >= FUDE_KO_JAMO_FIRST && _cp <= FUDE_KO_JAMO_LAST)         { return FUDE_KO_JAMO; }
    if(fude_ko_is_syllable(_cp)) {
        const u32 _n = _cp - FUDE_KO_SYLLABLE_FIRST;
        return (FUDE_KO_LISTED[_n >> 3u] >> (_n & 7u)) & 1u ? FUDE_KO_SYLLABLES : FUDE_LANG_NO_GROUP;
    }
    if(fude_utf8_is_han(_cp)) {
        return FUDE_KO_HANJA;
    }
    return FUDE_LANG_NO_GROUP;
}

// Returns a group's flags: the jamo are a script learnt as a set, the syllables a script learnt by level, and the
// hanja are learnt by level.
u32 fude_lang_group_flags(u32 _group) {
    // The forty jamo are learnt as a whole set. A syllable is read by its letters, so it is a script's, but there are
    // thousands, so they are learnt by level, as the hanja are.
    return _group == FUDE_KO_JAMO ? FUDE_LANG_GROUP_SCRIPT | FUDE_LANG_GROUP_SET :
           _group == FUDE_KO_SYLLABLES ? FUDE_LANG_GROUP_SCRIPT : 0u;
}

// Returns a group's name as a FUDE_TEXT_ id. A character in no group is shown under the syllables' name.
u32 fude_lang_group_name(u32 _group) {
    static const FUDE_TEXT_ _names[FUDE_KO_GROUPS] = { FUDE_TEXT_KO_JAMO, FUDE_TEXT_KO_SYLLABLES, FUDE_TEXT_KO_HANJA };
    return _group < FUDE_KO_GROUPS ? (u32)_names[_group] : _group == FUDE_LANG_NO_GROUP ? (u32)FUDE_TEXT_KO_SYLLABLES : (u32)FUDE_TEXT_COUNT;
}

// Returns what an exam asks for a group ("write the jamo"...), as a FUDE_TEXT_ id.
u32 fude_lang_group_prompt(u32 _group) {
    static const FUDE_TEXT_ _prompts[FUDE_KO_GROUPS] = { FUDE_TEXT_EXAM_WRITE_JAMO, FUDE_TEXT_EXAM_WRITE_HANGUL, FUDE_TEXT_EXAM_WRITE_HANJA };
    return _group < FUDE_KO_GROUPS ? (u32)_prompts[_group] : (u32)FUDE_TEXT_COUNT;
}

// Returns whether a character is one of the 40 core jamo: any compatibility jamo but the final clusters.
b8 fude_lang_core(u32 _cp) {
    if(_cp < FUDE_KO_JAMO_FIRST || _cp > FUDE_KO_JAMO_LAST) {
        return false;
    }
    for(u32 _i = 0; _i < sizeof(FUDE_KO_CLUSTERS) / sizeof(FUDE_KO_CLUSTERS[0]); _i++) {
        if(FUDE_KO_CLUSTERS[_i] == _cp) {
            return false;
        }
    }
    return true;   // One of the 40: 19 consonants and 21 vowels.
}

// Returns false: no Korean character is written on another.
b8 fude_lang_combining(u32 _cp) {
    RDE_UNUSED(_cp);
    return false;   // Every letter stands alone.
}

// Returns a jamo's Latin name, or a syllable's romanization (initial, medial and final), or NULL for anything else.
const c8* fude_lang_latin(u32 _cp) {
    if(_cp >= FUDE_KO_JAMO_FIRST && _cp <= FUDE_KO_JAMO_LAST) {
        return FUDE_KO_JAMO_LATIN[_cp - FUDE_KO_JAMO_FIRST];
    }
    if(!fude_ko_is_syllable(_cp)) {
        return NULL;
    }
    // Every syllable's name is made the first time one is asked for: 11,172 names of 8 bytes, about 90 KB. The longest
    // name, such as "kkwaeng", has 7 letters, so 8 bytes fit it. It is made without a lock, on the main thread
    // (lang.h).
    static c8 _names[FUDE_KO_SYLLABLE_LAST - FUDE_KO_SYLLABLE_FIRST + 1u][8];
    static b8 _made = false;
    if(!_made) {
        for(u32 _s = 0; _s <= FUDE_KO_SYLLABLE_LAST - FUDE_KO_SYLLABLE_FIRST; _s++) {
            u32 _i, _m, _f;
            fude_ko_decompose(FUDE_KO_SYLLABLE_FIRST + _s, &_i, &_m, &_f);
            rde_snprintf(_names[_s], sizeof(_names[_s]), "%s%s%s", FUDE_KO_INITIAL_LATIN[_i], FUDE_KO_MEDIAL_LATIN[_m], FUDE_KO_FINAL_LATIN[_f]);
        }
        _made = true;
    }
    return _names[_cp - FUDE_KO_SYLLABLE_FIRST];
}

// --- writing direction and joined forms: left to right, each letter as itself -------------

// Returns false: Korean is written left to right.
b8 fude_lang_rtl(void) {
    return false;
}

// Returns the letter itself: Korean has no joined forms.
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

// --- levels: the dictionary's beginner, intermediate, advanced (1..3) ------------------------

// Returns the dictionary's three levels.
u32 fude_lang_level_count(void) {
    return 3u;
}

// Returns the byte a level is stored as: its place plus one (1 to 3); 0 past the last.
u8 fude_lang_level_value(u32 _level) {
    return _level < 3u ? (u8)(_level + 1u) : 0u;
}

// Writes a stored level's name: the Korean word for it (초급) when short, and the app's name for it followed by the
// Korean word ("Beginner 초급") when long; "" for no level.
void fude_lang_level_name(u8 _value, b8 _long, c8* _out, usize _size) {
    static const c8* const _short[3] = { "\xEC\xB4\x88\xEA\xB8\x89", "\xEC\xA4\x91\xEA\xB8\x89", "\xEA\xB3\xA0\xEA\xB8\x89" };   // 초급 중급 고급
    static const FUDE_TEXT_ _names[3] = { FUDE_TEXT_KO_LEVEL_1, FUDE_TEXT_KO_LEVEL_2, FUDE_TEXT_KO_LEVEL_3 };
    if(_value == 0u || _value > 3u) {
        rde_snprintf(_out, _size, "%s", "");
    } else if(_long) {
        rde_snprintf(_out, _size, "%s %s", fude_text(_names[_value - 1u]), _short[_value - 1u]);   // "Beginner 초급".
    } else {
        rde_snprintf(_out, _size, "%s", _short[_value - 1u]);
    }
}

// --- readings: a hanja's sound (음) and meaning word (훈), in Hangul ------------------------

// Returns the two kinds of reading: 0 is a hanja's sound (음), 1 its meaning word (훈).
u32 fude_lang_reading_kinds(void) {
    return 2u;
}

// Returns a kind of reading's name for the sort chips, as a FUDE_TEXT_ id.
u32 fude_lang_reading_name(u32 _kind) {
    return _kind == 0u ? (u32)FUDE_TEXT_SORT_EUM : _kind == 1u ? (u32)FUDE_TEXT_SORT_HUN : (u32)FUDE_TEXT_COUNT;
}

// Returns _cp as it is, since Hangul readings are compared as written.
u32 fude_lang_reading_fold(u32 _cp) {
    return _cp;   // Hangul is compared as it is.
}

// Returns the sound (음), the reading a hanja has when said alone, whatever _order is.
u32 fude_lang_word_reading_kind(u32 _order) {
    RDE_UNUSED(_order);
    return 0u;   // A hanja said alone is read by its sound.
}

// Returns true: words are read in romanization.
b8 fude_lang_word_reading_latin(void) {
    return true;   // Words are read in romanization: 학교 is hakgyo.
}

// Returns false: no word is read as it is written, since its words are read in Latin letters
// (fude_lang_word_reading_latin).
b8 fude_lang_reads_as_written(const c8* _written) {
    RDE_UNUSED(_written);
    return false;
}

// Romanization typed back into Hangul, syllable by syllable: each is an initial (ㅇ when there is none), a vowel and a
// final. Where consonants stand between two vowels, the longest run at their end that can start a syllable does, and
// those before it end the syllable before (hakgyo is hak-gyo; annyeong is an-nyeong). A hyphen or an apostrophe parts
// two syllables (sarang-e). A final t is taken as ㅅ, the commonest (맛, 옷, 것).

// One spelling of a letter: its Latin letters, and the letter's number in Unicode's order (hangul.h).
typedef struct { const c8* latin; u8 index; } fude_ko_letter;

// The spellings of the initials, medials and finals, each table with its longer spellings first, so that the first
// match is the longest. Some other spellings learners use are read too (sh, c, f, v, z; weo, oi).
static const fude_ko_letter FUDE_KO_INITIALS_IN[] = {   // Longest first.
    { "kk", 1 }, { "tt", 4 }, { "pp", 8 }, { "ss", 10 }, { "jj", 13 }, { "ch", 14 }, { "sh", 9 },
    { "g", 0 }, { "k", 15 }, { "n", 2 }, { "d", 3 }, { "t", 16 }, { "r", 5 }, { "l", 5 }, { "m", 6 },
    { "b", 7 }, { "p", 17 }, { "s", 9 }, { "j", 12 }, { "h", 18 }, { "c", 14 }, { "f", 17 }, { "v", 7 }, { "z", 12 },
};
static const fude_ko_letter FUDE_KO_MEDIALS_IN[] = {    // Longest first.
    { "yae", 3 }, { "wae", 10 }, { "yeo", 6 }, { "weo", 14 },
    { "ae", 1 }, { "ya", 2 }, { "eo", 4 }, { "ye", 7 }, { "wa", 9 }, { "oe", 11 }, { "yo", 12 }, { "wo", 14 }, { "we", 15 },
    { "wi", 16 }, { "yu", 17 }, { "eu", 18 }, { "ui", 19 }, { "oi", 11 },
    { "a", 0 }, { "e", 5 }, { "o", 8 }, { "u", 13 }, { "i", 20 },
};
static const fude_ko_letter FUDE_KO_FINALS_IN[] = {     // Longest first.
    { "ng", 21 }, { "kk", 2 }, { "ss", 20 }, { "ch", 23 },
    { "k", 1 }, { "g", 1 }, { "n", 4 }, { "t", 19 }, { "d", 7 }, { "l", 8 }, { "r", 8 }, { "m", 16 }, { "p", 17 }, { "b", 17 },
    { "s", 19 }, { "j", 22 }, { "h", 27 },
};

// Returns the number of _table's letter spelt exactly _s[0.._n), or -1 when there is none.
RDE_INTERNAL i32 fude_ko_letter_is(const fude_ko_letter* _table, u32 _count, const c8* _s, usize _n) {
    for(u32 _i = 0; _i < _count; _i++) {
        if(rde_strlen(_table[_i].latin) == _n && rde_strncmp(_table[_i].latin, _s, _n) == 0) {
            return (i32)_table[_i].index;
        }
    }
    return -1;
}

// Returns the number of the longest letter in _table that _s starts with, and sets *_len to its spelling's length; -1
// when there is none. The table must list longer spellings first.
RDE_INTERNAL i32 fude_ko_letter_at(const fude_ko_letter* _table, u32 _count, const c8* _s, usize* _len) {
    for(u32 _i = 0; _i < _count; _i++) {
        const usize _n = rde_strlen(_table[_i].latin);
        if(rde_strncmp(_table[_i].latin, _s, _n) == 0) {
            *_len = _n;
            return (i32)_table[_i].index;
        }
    }
    return -1;
}

// Returns whether _c is a letter a vowel's spelling can start with: a e i o u, and w and y.
RDE_INTERNAL b8 fude_ko_vowel_letter(c8 _c) {
    return _c == 'a' || _c == 'e' || _c == 'i' || _c == 'o' || _c == 'u' || _c == 'w' || _c == 'y';
}

// The number of items in the static array _t.
#define FUDE_KO_COUNT(_t) ((u32)(sizeof(_t) / sizeof((_t)[0])))

// The most bytes of a romanization fude_lang_reading_from_latin reads. What it is given comes from the app's fields
// and searches, none longer than 127 bytes (Check's text, Browse's search), so this is room to spare; a longer text is
// not a reading, and is refused rather than read in part.
#define FUDE_KO_LATIN_MAX 255u

// Returns _cp as it is.
u32 fude_lang_reading_capital(u32 _cp) {
    return _cp;   // Korean has no other way of writing a reading typed in capitals.
}

// Turns a romanization into Hangul by the rules above, keeping spaces. Returns false when the text has anything but
// letters, hyphens, apostrophes and spaces, or when some part of it is not a syllable; true when at least one syllable
// was made. The text is read whole or not at all: one longer than FUDE_KO_LATIN_MAX bytes returns false, as does one
// whose Hangul does not fit _out (which then holds the syllables that fitted).
b8 fude_lang_reading_from_latin(const c8* _latin, c8* _out, usize _size) {
    if(_out == NULL || _size == 0) {
        return false;
    }
    _out[0] = 0;
    // Lower the case, and accept only letters, spaces and the marks that part syllables.
    c8    _s[FUDE_KO_LATIN_MAX + 1u];
    usize _n = 0;
    for(const c8* _p = _latin != NULL ? _latin : ""; *_p != 0; _p++) {
        if(_n == FUDE_KO_LATIN_MAX) {
            return false;   // Longer than any reading, and it is not read in part.
        }
        c8 _c = *_p;
        if(_c >= 'A' && _c <= 'Z') {
            _c = (c8)(_c + ('a' - 'A'));
        }
        if((_c >= 'a' && _c <= 'z') || _c == '-' || _c == '\'' || _c == ' ') {
            _s[_n++] = _c;
        } else {
            return false;
        }
    }
    _s[_n] = 0;

    usize _at  = 0;   // Where the next syllable goes in _out.
    usize _i   = 0;   // Where the next syllable starts in _s.
    b8    _any = false;
    while(_i < _n) {
        if(_s[_i] == '-' || _s[_i] == '\'') {
            _i++;
            continue;
        }
        if(_s[_i] == ' ') {
            if(_at + 1u >= _size) {
                return false;   // No room for the space: the Hangul is not whole.
            }
            _out[_at++] = ' ';
            _out[_at]   = 0;
            _i++;
            continue;
        }
        // The initial: none (ㅇ) before a vowel, else the longest spelling of one. A w that no vowel follows is tried as
        // an initial, and fails.
        i32   _initial = 11;   // ㅇ
        usize _len     = 0;
        if(!fude_ko_vowel_letter(_s[_i]) || (_s[_i] == 'w' && _i + 1u < _n && !fude_ko_vowel_letter(_s[_i + 1u]))) {
            _initial = fude_ko_letter_at(FUDE_KO_INITIALS_IN, FUDE_KO_COUNT(FUDE_KO_INITIALS_IN), _s + _i, &_len);
            if(_initial < 0) {
                return false;
            }
            _i += _len;
        }
        const i32 _medial = fude_ko_letter_at(FUDE_KO_MEDIALS_IN, FUDE_KO_COUNT(FUDE_KO_MEDIALS_IN), _s + _i, &_len);
        if(_medial < 0) {
            return false;
        }
        _i += _len;
        // The run of consonants up to the next vowel, or up to a mark, a space or the end.
        usize _run = 0;
        while(_i + _run < _n && !fude_ko_vowel_letter(_s[_i + _run]) && _s[_i + _run] != '-' && _s[_i + _run] != '\'' && _s[_i + _run] != ' ') {
            _run++;
        }
        const b8 _vowel_next = _i + _run < _n && fude_ko_vowel_letter(_s[_i + _run]);
        i32      _final      = 0;
        usize    _take       = 0;   // How many letters of the run are the final's.
        if(_run > 0 && !_vowel_next) {
            _final = fude_ko_letter_is(FUDE_KO_FINALS_IN, FUDE_KO_COUNT(FUDE_KO_FINALS_IN), _s + _i, _run);
            if(_final < 0) {
                return false;
            }
            _take = _run;
        } else if(_run > 0) {
            // A vowel follows, so the run is parted: its longest tail that is an initial starts the next syllable, and
            // the rest, which must be a final or nothing, ends this one.
            b8 _parted = false;
            for(usize _tail = _run; _tail >= 1u && !_parted; _tail--) {
                if(fude_ko_letter_is(FUDE_KO_INITIALS_IN, FUDE_KO_COUNT(FUDE_KO_INITIALS_IN), _s + _i + _run - _tail, _tail) < 0) {
                    continue;
                }
                const usize _head = _run - _tail;
                const i32   _f    = _head == 0u ? 0 : fude_ko_letter_is(FUDE_KO_FINALS_IN, FUDE_KO_COUNT(FUDE_KO_FINALS_IN), _s + _i, _head);
                if(_f >= 0) {
                    _final  = _f;
                    _take   = _head;
                    _parted = true;
                }
            }
            if(!_parted) {
                return false;
            }
        }
        _i += _take;
        c8        _one[5];
        const u32 _l = fude_utf8_put(fude_ko_compose((u32)_initial, (u32)_medial, (u32)_final), _one);
        if(_at + _l >= _size) {
            return false;   // No room for the syllable: the Hangul is not whole.
        }
        rde_memcpy(_out + _at, _one, _l);
        _at += _l;
        _out[_at] = 0;
        _any = true;
    }
    return _any;
}

// --- badges: Kana's, as Korean reads those hanja ------------------------------------------

// Returns the syllable a badge shows, in FUDE_LANG_BADGE_ order, or 0 out of range. They are the Japanese app's badge
// kanji (音 訓 部 試 語 似 記) as Korean reads those hanja, and 번 from 번역 (translation).
u32 fude_lang_badge(FUDE_LANG_BADGE_ _badge) {
    static const u32 _badges[FUDE_LANG_BADGE_COUNT] = {
        0xC74Cu,   // 음 (音: its sound)
        0xD6C8u,   // 훈 (訓: its meaning word)
        0xBD80u,   // 부 (部)
        0xC2DCu,   // 시 (試)
        0xC5B4u,   // 어 (語)
        0xC0ACu,   // 사 (似)
        0xAE30u,   // 기 (記)
        0xBC88u,   // 번 (번역: translation)
    };
    return _badge < FUDE_LANG_BADGE_COUNT ? _badges[_badge] : 0u;
}

// --- handwriting: how free writing is read ---------------------------------------------------

// Returns no rules (lang.h): Korean's free writing is read by each character's shape, size and place alone, with no
// switching between Hangul and hanja costed.
const fude_lang_writing_def* fude_lang_writing(void) {
    static const fude_lang_writing_def _none = { .switch_cost = NULL };
    return &_none;
}
