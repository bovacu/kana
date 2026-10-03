#include "lang/lang.h"
#include "drawing/base/text.h"
#include "drawing/base/utf8.h"

#include <stdio.h>
#include <string.h>

#include "lang/ko/syllables.h"

// ===========================================================================
// See lang.h: Korean. Jamo (Hangul's letters), the syllables they make, and
// hanja; the levels of NIKL's Basic Korean Dictionary (beginner, intermediate,
// advanced); a hanja's sound (음) and meaning word (훈); the Revised
// Romanization of Korean (고시 2014-42), for the letters' Latin names and for
// typing Korean in Latin letters.
// ===========================================================================

const c8* fude_lang_code(void)      { return "ko"; }
const c8* fude_lang_ink_model(void) { return "ko"; }
const c8* fude_lang_voice(void)     { return "ko-KR"; }

RDE_LANGUAGE_ fude_lang_ui_language(const c8** _name, const c8** _flag) {
    *_name = "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4";   // 한국어
    *_flag = "assets/flags/kr.png";
    return RDE_LANGUAGE_KO_KR;
}

// --- the jamo, and the syllables they make ------------------------------------------------

#define FUDE_KO_SYLLABLE_FIRST 0xAC00u   // 가
#define FUDE_KO_SYLLABLE_LAST  0xD7A3u   // 힣
#define FUDE_KO_JAMO_FIRST     0x3131u   // ㄱ (compatibility jamo, as text writes a letter alone)
#define FUDE_KO_JAMO_LAST      0x3163u   // ㅣ
#define FUDE_KO_MEDIALS        21u
#define FUDE_KO_FINALS         28u       // the first: none

// A syllable's letters, as Unicode composes it: initial, medial, final.
static const c8* const FUDE_KO_INITIAL_LATIN[19] = {
    "g", "kk", "n", "d", "tt", "r", "m", "b", "pp", "s", "ss", "", "j", "jj", "ch", "k", "t", "p", "h",
};
static const c8* const FUDE_KO_MEDIAL_LATIN[FUDE_KO_MEDIALS] = {
    "a", "ae", "ya", "yae", "eo", "e", "yeo", "ye", "o", "wa", "wae", "oe", "yo", "u", "wo", "we", "wi", "yu", "eu", "ui", "i",
};
// A final as a syllable said alone ends: seven sounds (ㅅ ㅆ ㅈ ㅊ ㅌ ㅎ end in t).
static const c8* const FUDE_KO_FINAL_LATIN[FUDE_KO_FINALS] = {
    "", "k", "k", "k", "n", "n", "n", "t", "l", "k", "m", "l", "l", "l", "p", "l", "m", "p", "p", "t", "t", "ng", "t", "t", "k", "t", "p", "t",
};
// The compatibility jamo (ㄱ U+3131 ... ㅣ U+3163): each one's Latin name.
static const c8* const FUDE_KO_JAMO_LATIN[FUDE_KO_JAMO_LAST - FUDE_KO_JAMO_FIRST + 1u] = {
    "g/k", "kk", "gs", "n", "nj", "nh", "d/t", "tt", "r/l", "lg", "lm", "lb", "ls", "lt", "lp", "lh",   // ㄱ ㄲ ㄳ ㄴ ㄵ ㄶ ㄷ ㄸ ㄹ ㄺ ...
    "m", "b/p", "pp", "bs", "s", "ss", "ng", "j", "jj", "ch", "k", "t", "p", "h",                       // ㅁ ㅂ ㅃ ㅄ ㅅ ㅆ ㅇ ㅈ ㅉ ㅊ ㅋ ㅌ ㅍ ㅎ
    "a", "ae", "ya", "yae", "eo", "e", "yeo", "ye", "o", "wa", "wae", "oe", "yo", "u", "wo", "we", "wi", "yu", "eu", "ui", "i",
};
// The final clusters (ㄳ ㄵ ㄶ ㄺ ㄻ ㄼ ㄽ ㄾ ㄿ ㅀ ㅄ): letters, but not ones learnt alone.
static const u32 FUDE_KO_CLUSTERS[] = { 0x3133u, 0x3135u, 0x3136u, 0x313Au, 0x313Bu, 0x313Cu, 0x313Du, 0x313Eu, 0x313Fu, 0x3140u, 0x3144u };

// --- groups: jamo, syllables, hanja ------------------------------------------------------

// A syllable not in syllables.h (one no word has: 겱) is in no group: written, but
// listed nowhere; named a syllable still (fude_lang_group_name).
enum { FUDE_KO_JAMO = 0, FUDE_KO_SYLLABLES, FUDE_KO_HANJA, FUDE_KO_GROUPS };

u32 fude_lang_group_count(void) {
    return FUDE_KO_GROUPS;
}

u8 fude_lang_group(u32 _cp) {
    if(_cp >= FUDE_KO_JAMO_FIRST && _cp <= FUDE_KO_JAMO_LAST)         { return FUDE_KO_JAMO; }
    if(_cp >= FUDE_KO_SYLLABLE_FIRST && _cp <= FUDE_KO_SYLLABLE_LAST) {
        const u32 _n = _cp - FUDE_KO_SYLLABLE_FIRST;
        return (FUDE_KO_LISTED[_n >> 3u] >> (_n & 7u)) & 1u ? FUDE_KO_SYLLABLES : FUDE_LANG_NO_GROUP;
    }
    if((_cp >= 0x4E00u && _cp <= 0x9FFFu) || (_cp >= 0x3400u && _cp <= 0x4DBFu) ||
       (_cp >= 0xF900u && _cp <= 0xFAFFu) || (_cp >= 0x20000u && _cp <= 0x2FFFFu)) {
        return FUDE_KO_HANJA;
    }
    return FUDE_LANG_NO_GROUP;
}

u32 fude_lang_group_flags(u32 _group) {
    // The jamo are learnt whole (forty); a syllable is read by its letters, but
    // there are thousands: learnt by level.
    return _group == FUDE_KO_JAMO ? FUDE_LANG_GROUP_SCRIPT | FUDE_LANG_GROUP_SET :
           _group == FUDE_KO_SYLLABLES ? FUDE_LANG_GROUP_SCRIPT : 0u;
}

u32 fude_lang_group_name(u32 _group) {
    static const FUDE_TEXT_ _names[FUDE_KO_GROUPS] = { FUDE_TEXT_KO_JAMO, FUDE_TEXT_KO_SYLLABLES, FUDE_TEXT_KO_HANJA };
    return _group < FUDE_KO_GROUPS ? (u32)_names[_group] : _group == FUDE_LANG_NO_GROUP ? (u32)FUDE_TEXT_KO_SYLLABLES : (u32)FUDE_TEXT_COUNT;
}

u32 fude_lang_group_prompt(u32 _group) {
    static const FUDE_TEXT_ _prompts[FUDE_KO_GROUPS] = { FUDE_TEXT_EXAM_WRITE_JAMO, FUDE_TEXT_EXAM_WRITE_HANGUL, FUDE_TEXT_EXAM_WRITE_HANJA };
    return _group < FUDE_KO_GROUPS ? (u32)_prompts[_group] : (u32)FUDE_TEXT_COUNT;
}

b8 fude_lang_core(u32 _cp) {
    if(_cp < FUDE_KO_JAMO_FIRST || _cp > FUDE_KO_JAMO_LAST) {
        return false;
    }
    for(u32 _i = 0; _i < sizeof(FUDE_KO_CLUSTERS) / sizeof(FUDE_KO_CLUSTERS[0]); _i++) {
        if(FUDE_KO_CLUSTERS[_i] == _cp) {
            return false;
        }
    }
    return true;   // the 40: 19 consonants, 21 vowels
}

const c8* fude_lang_latin(u32 _cp) {
    if(_cp >= FUDE_KO_JAMO_FIRST && _cp <= FUDE_KO_JAMO_LAST) {
        return FUDE_KO_JAMO_LATIN[_cp - FUDE_KO_JAMO_FIRST];
    }
    if(_cp < FUDE_KO_SYLLABLE_FIRST || _cp > FUDE_KO_SYLLABLE_LAST) {
        return NULL;
    }
    // Every syllable's, made the first time one is asked for (11,172: ~90 KB).
    static c8 _names[FUDE_KO_SYLLABLE_LAST - FUDE_KO_SYLLABLE_FIRST + 1u][8];
    static b8 _made = false;
    if(!_made) {
        for(u32 _s = 0; _s <= FUDE_KO_SYLLABLE_LAST - FUDE_KO_SYLLABLE_FIRST; _s++) {
            const u32 _i = _s / (FUDE_KO_MEDIALS * FUDE_KO_FINALS);
            const u32 _m = (_s / FUDE_KO_FINALS) % FUDE_KO_MEDIALS;
            const u32 _f = _s % FUDE_KO_FINALS;
            snprintf(_names[_s], sizeof(_names[_s]), "%s%s%s", FUDE_KO_INITIAL_LATIN[_i], FUDE_KO_MEDIAL_LATIN[_m], FUDE_KO_FINAL_LATIN[_f]);
        }
        _made = true;
    }
    return _names[_cp - FUDE_KO_SYLLABLE_FIRST];
}

// --- levels: the dictionary's beginner, intermediate, advanced (1..3) ------------------------

u32 fude_lang_level_count(void) {
    return 3u;
}

u8 fude_lang_level_value(u32 _level) {
    return _level < 3u ? (u8)(_level + 1u) : 0u;
}

void fude_lang_level_name(u8 _value, b8 _long, c8* _out, usize _size) {
    static const c8* const _short[3] = { "\xEC\xB4\x88\xEA\xB8\x89", "\xEC\xA4\x91\xEA\xB8\x89", "\xEA\xB3\xA0\xEA\xB8\x89" };   // 초급 중급 고급
    static const FUDE_TEXT_ _names[3] = { FUDE_TEXT_KO_LEVEL_1, FUDE_TEXT_KO_LEVEL_2, FUDE_TEXT_KO_LEVEL_3 };
    if(_value == 0u || _value > 3u) {
        snprintf(_out, _size, "%s", "");
    } else if(_long) {
        snprintf(_out, _size, "%s %s", fude_text(_names[_value - 1u]), _short[_value - 1u]);   // Beginner 초급
    } else {
        snprintf(_out, _size, "%s", _short[_value - 1u]);
    }
}

// --- readings: a hanja's sound (음) and meaning word (훈), in Hangul ------------------------

u32 fude_lang_reading_kinds(void) {
    return 2u;
}

u32 fude_lang_reading_name(u32 _kind) {
    return _kind == 0u ? (u32)FUDE_TEXT_SORT_EUM : _kind == 1u ? (u32)FUDE_TEXT_SORT_HUN : (u32)FUDE_TEXT_COUNT;
}

u32 fude_lang_reading_fold(u32 _cp) {
    return _cp;   // Hangul is compared as it is
}

u32 fude_lang_word_reading_kind(u32 _order) {
    RDE_UNUSED(_order);
    return 0u;   // a hanja said alone: its sound
}

b8 fude_lang_word_reading_latin(void) {
    return true;   // words are read in romanization: 학교 hakgyo
}

// Romanization typed back into Hangul: syllable by syllable, each an initial
// (none: ㅇ), a vowel and a final. Where consonants stand between two vowels the
// last that can start a syllable does, and those before it end the one before
// (hakgyo: hak-gyo; annyeong: an-nyeong); a hyphen or an apostrophe parts two
// syllables (sarang-e). A final t is taken as ㅅ (맛, 옷, 것: the commonest).

typedef struct { const c8* latin; u8 index; } fude_ko_letter;

static const fude_ko_letter FUDE_KO_INITIALS_IN[] = {   // longest first
    { "kk", 1 }, { "tt", 4 }, { "pp", 8 }, { "ss", 10 }, { "jj", 13 }, { "ch", 14 }, { "sh", 9 },
    { "g", 0 }, { "k", 15 }, { "n", 2 }, { "d", 3 }, { "t", 16 }, { "r", 5 }, { "l", 5 }, { "m", 6 },
    { "b", 7 }, { "p", 17 }, { "s", 9 }, { "j", 12 }, { "h", 18 }, { "c", 14 }, { "f", 17 }, { "v", 7 }, { "z", 12 },
};
static const fude_ko_letter FUDE_KO_MEDIALS_IN[] = {    // longest first
    { "yae", 3 }, { "wae", 10 }, { "yeo", 6 }, { "weo", 14 },
    { "ae", 1 }, { "ya", 2 }, { "eo", 4 }, { "ye", 7 }, { "wa", 9 }, { "oe", 11 }, { "yo", 12 }, { "wo", 14 }, { "we", 15 },
    { "wi", 16 }, { "yu", 17 }, { "eu", 18 }, { "ui", 19 }, { "oi", 11 },
    { "a", 0 }, { "e", 5 }, { "o", 8 }, { "u", 13 }, { "i", 20 },
};
static const fude_ko_letter FUDE_KO_FINALS_IN[] = {     // longest first
    { "ng", 21 }, { "kk", 2 }, { "ss", 20 }, { "ch", 23 },
    { "k", 1 }, { "g", 1 }, { "n", 4 }, { "t", 19 }, { "d", 7 }, { "l", 8 }, { "r", 8 }, { "m", 16 }, { "p", 17 }, { "b", 17 },
    { "s", 19 }, { "j", 22 }, { "h", 27 },
};

// The letter of _table that is exactly _s[0.._n), or -1.
RDE_INTERNAL i32 fude_ko_letter_is(const fude_ko_letter* _table, u32 _count, const c8* _s, usize _n) {
    for(u32 _i = 0; _i < _count; _i++) {
        if(strlen(_table[_i].latin) == _n && strncmp(_table[_i].latin, _s, _n) == 0) {
            return (i32)_table[_i].index;
        }
    }
    return -1;
}

// The longest letter of _table _s starts with (its length into *_len), or -1.
RDE_INTERNAL i32 fude_ko_letter_at(const fude_ko_letter* _table, u32 _count, const c8* _s, usize* _len) {
    for(u32 _i = 0; _i < _count; _i++) {
        const usize _n = strlen(_table[_i].latin);
        if(strncmp(_table[_i].latin, _s, _n) == 0) {
            *_len = _n;
            return (i32)_table[_i].index;
        }
    }
    return -1;
}

RDE_INTERNAL b8 fude_ko_vowel_letter(c8 _c) {
    return _c == 'a' || _c == 'e' || _c == 'i' || _c == 'o' || _c == 'u' || _c == 'w' || _c == 'y';
}

#define FUDE_KO_COUNT(_t) ((u32)(sizeof(_t) / sizeof((_t)[0])))

b8 fude_lang_reading_from_latin(const c8* _latin, c8* _out, usize _size) {
    if(_out == NULL || _size == 0) {
        return false;
    }
    _out[0] = 0;
    // Lower case, letters and the marks that part syllables only.
    c8    _s[256];
    usize _n = 0;
    for(const c8* _p = _latin != NULL ? _latin : ""; *_p != 0 && _n + 1u < sizeof(_s); _p++) {
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

    usize _at  = 0;   // in _out
    usize _i   = 0;   // in _s
    b8    _any = false;
    while(_i < _n) {
        if(_s[_i] == '-' || _s[_i] == '\'') {
            _i++;
            continue;
        }
        if(_s[_i] == ' ') {
            if(_at + 1u < _size) {
                _out[_at++] = ' ';
                _out[_at]   = 0;
            }
            _i++;
            continue;
        }
        // The initial: none before a vowel, else the longest that is one.
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
        // The consonants up to the next vowel (or a mark, a space, the end).
        usize _run = 0;
        while(_i + _run < _n && !fude_ko_vowel_letter(_s[_i + _run]) && _s[_i + _run] != '-' && _s[_i + _run] != '\'' && _s[_i + _run] != ' ') {
            _run++;
        }
        const b8 _vowel_next = _i + _run < _n && fude_ko_vowel_letter(_s[_i + _run]);
        i32      _final      = 0;
        usize    _take       = 0;   // of the run, the final's letters
        if(_run > 0 && !_vowel_next) {
            _final = fude_ko_letter_is(FUDE_KO_FINALS_IN, FUDE_KO_COUNT(FUDE_KO_FINALS_IN), _s + _i, _run);
            if(_final < 0) {
                return false;
            }
            _take = _run;
        } else if(_run > 0) {
            // The run parts: the next syllable's initial its longest tail that is
            // one, the rest this one's final.
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
        c8 _one[5];
        fude_utf8_put(FUDE_KO_SYLLABLE_FIRST + ((u32)_initial * FUDE_KO_MEDIALS + (u32)_medial) * FUDE_KO_FINALS + (u32)_final, _one);
        const usize _l = strlen(_one);
        if(_at + _l < _size) {
            memcpy(_out + _at, _one, _l);
            _at += _l;
            _out[_at] = 0;
        }
        _any = true;
    }
    return _any;
}

// --- badges: Kana's, as Korean reads those hanja ------------------------------------------

u32 fude_lang_badge(FUDE_LANG_BADGE_ _badge) {
    static const u32 _badges[FUDE_LANG_BADGE_COUNT] = {
        0xC74Cu,   // 음 (音: its sound)
        0xD6C8u,   // 훈 (訓: its meaning word)
        0xBD80u,   // 부 (部)
        0xC2DCu,   // 시 (試)
        0xC5B4u,   // 어 (語)
        0xC0ACu,   // 사 (似)
        0xAE30u,   // 기 (記)
    };
    return _badge < FUDE_LANG_BADGE_COUNT ? _badges[_badge] : 0u;
}
