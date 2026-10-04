#ifndef FUDE_LANG_H
#define FUDE_LANG_H

#include "rde.h"

// ===========================================================================
// The language a study app teaches: everything the study layer (fude/study)
// asks of it. One implementation per language, in fude/lang/<code>/lang.c, and
// an app compiles exactly one: Kana fude/lang/ja, Hanzi fude/lang/zh, Hangul
// fude/lang/ko, Thai fude/lang/th, Hindi fude/lang/hi, Arabic fude/lang/ar. Its
// iOS side (the text recognizer's options) is
// fude/lang/<code>/lang_ios.m.
//
// Beside this interface a language has:
//   - wordsplit.c, the word splitter (fude/lang/wordsplit.h);
//   - strings.py, its UI strings: the study's ids that name the language
//     ("Into Japanese", "The Japanese model...") in its own words, and the
//     ids its lang.c returns (group, reading and prompt names). An app's
//     strings tool reads it after the study's;
//   - bake.c, its character data (the study's format: study/chars/kanji.h);
//   - its own screens, if any (Japanese: the kana chart, chart.c), which its
//     app puts in its table.
//
// The character data's one-byte level and its two reading lists are the
// language's to fill: the study shows and sorts them by what this says.
// ===========================================================================

// --- codes -------------------------------------------------------------------------

// The language's code, as translation (ML Kit Translate) names it: "ja".
const c8* fude_lang_code(void);
// ML Kit Digital Ink Recognition's model for handwriting: "ja".
const c8* fude_lang_ink_model(void);
// The voice reading aloud (iOS AVSpeechSynthesizer): "ja-JP".
const c8* fude_lang_voice(void);
// Can ML Kit Text Recognition read the language in pictures (Text from a photo,
// a scanned page's lines)? Japanese, Chinese, Korean, Hindi: yes; Thai: no model.
b8        fude_lang_text_readable(void);
// The language as one of the app's UI languages (the fourth: text.h's taught
// one): its RDE id, its name in itself ("日本語") and its flag (assets/flags/).
RDE_LANGUAGE_ fude_lang_ui_language(const c8** _name, const c8** _flag);

// --- groups: the kinds of character to learn ---------------------------------------

#define FUDE_LANG_GROUPS     4u      // at most
#define FUDE_LANG_NO_GROUP   0xFFu   // not a character to learn (punctuation, Latin)

// A group's nature.
#define FUDE_LANG_GROUP_SCRIPT 0x01u   // letters of a script: read as themselves, each with a Latin name
                                       // (fude_lang_latin), no meanings (kana; jamo)
#define FUDE_LANG_GROUP_SET    0x02u   // learnt whole: an exam source and a statistics row, of
                                       // its core characters (fude_lang_core)

u32       fude_lang_group_count(void);
// The group a character is in, FUDE_LANG_NO_GROUP when none: Japanese 0 hiragana,
// 1 katakana, 2 kanji.
u8        fude_lang_group(u32 _codepoint);
u32       fude_lang_group_flags(u32 _group);    // FUDE_LANG_GROUP_
u32       fude_lang_group_name(u32 _group);     // FUDE_TEXT_: "Hiragana"; for FUDE_LANG_NO_GROUP, what one outside them all is shown as (々: "Kanji")
u32       fude_lang_group_prompt(u32 _group);   // FUDE_TEXT_: an exam's "WRITE IN HIRAGANA"
// A set's core: the characters learnt first (the 71 kana of each script, not
// the small ones nor the old).
b8        fude_lang_core(u32 _codepoint);
// A script letter's Latin name ("ka"; ー "long"); NULL for anything else.
const c8* fude_lang_latin(u32 _codepoint);
// Is it written on another letter (Thai's and Hindi's vowel signs and tone
// marks)? Shown alone, it gets a dotted circle where that letter goes (glyph.h).
b8        fude_lang_combining(u32 _codepoint);

// --- writing direction and joined forms ---------------------------------------------

// Is the language written right to left (Arabic)? A word's boxes then run from
// the right.
b8        fude_lang_rtl(void);
// The character a letter is written as between the letters before and after it
// in a word (0: none there; marks are not neighbours): Arabic's joined forms (ب
// starting a word: ﺑ U+FE91; ل before ا: the ligature ﻻ, and *_with_after then
// says _after is written in it). Every other language: the letter itself.
u32       fude_lang_form(u32 _before, u32 _cp, u32 _after, b8* _with_after);
// The letter a character is a form of (Arabic: ﺑ U+FE91 → ب); itself elsewhere.
// Recognition reads letters, not their forms: a form written is right when its
// letter is read.
u32       fude_lang_letter(u32 _cp);

// --- levels --------------------------------------------------------------------------

#define FUDE_LANG_LEVELS 9u   // at most

// The levels, easiest first (Japanese: JLPT N5 to N1). A character's level is
// one byte of its data (fude_kanji_info.level, 0: none).
u32       fude_lang_level_count(void);
// The byte a level is stored as: Japanese N5 → 5 ... N1 → 1.
u8        fude_lang_level_value(u32 _level);
// A stored level's name, short ("N5", for a chip or under a character) or long
// ("JLPT N5", in a header); "" for 0. Into _out.
void      fude_lang_level_name(u8 _value, b8 _long, c8* _out, usize _size);

// --- readings ------------------------------------------------------------------------

// A character's readings, as its data has them: up to two kinds (Japanese: on
// and kun), each a list joined by 、; a '.' in a reading marks where its stem
// ends (Japanese okurigana), and '-' an affix.
u32       fude_lang_reading_kinds(void);
u32       fude_lang_reading_name(u32 _kind);    // FUDE_TEXT_: a sort chip's "On"
// A reading typed in Latin letters, as the language writes readings (Japanese:
// romaji → hiragana). False when it is not one; _out then holds what could be read.
b8        fude_lang_reading_from_latin(const c8* _latin, c8* _out, usize _size);
// A reading's character as readings are compared (Japanese: katakana as hiragana;
// Thai, Hindi, Arabic: a Latin letter without its accent, ǎ ā as a). 0: left out
// (a tone's or a syllable's mark: paa-sǎa compares as paasaa).
u32       fude_lang_reading_fold(u32 _codepoint);
// Of a character's readings, the kind a one-character word is read by (Japanese:
// kun, then on).
u32       fude_lang_word_reading_kind(u32 _order);
// Are words read in Latin letters (Korean: 학교 hakgyo)? A word typed with a
// reading keeps it as typed then, and one typed without reads as its letters'
// Latin names (fude_lang_latin). Otherwise a reading typed in Latin letters is
// turned into the language's (fude_lang_reading_from_latin).
b8        fude_lang_word_reading_latin(void);

// --- badges --------------------------------------------------------------------------

// The characters headers and labels wear, written from the language's own
// strokes (header.h).
typedef enum {
    FUDE_LANG_BADGE_READING_0 = 0,   // 音
    FUDE_LANG_BADGE_READING_1,       // 訓
    FUDE_LANG_BADGE_PARTS,           // 部
    FUDE_LANG_BADGE_EXAM,            // 試
    FUDE_LANG_BADGE_WORDS,           // 語
    FUDE_LANG_BADGE_LOOKALIKES,      // 似
    FUDE_LANG_BADGE_NOTE,            // 記
    FUDE_LANG_BADGE_COUNT
} FUDE_LANG_BADGE_;

u32       fude_lang_badge(FUDE_LANG_BADGE_ _badge);

// --- iOS ------------------------------------------------------------------------------

#if defined(__OBJC__)
@class MLKCommonTextRecognizerOptions;
// ML Kit Text Recognition's options for the language's model (lang_ios.m).
MLKCommonTextRecognizerOptions* fude_lang_text_options(void);
#endif

#endif
