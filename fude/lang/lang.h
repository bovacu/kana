// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_LANG_H
#define FUDE_LANG_H

#include "rde.h"

// ===========================================================================
// Language: everything the study layer asks of the one language a study app teaches. It gives the language's codes
// for translation, handwriting and speech, the kinds of character to learn, its levels and readings, its writing
// direction, its chart of letters, how its words are found in a line of text and how its free writing is read.
//
// Where it sits: this is the interface between the study layer (fude/study, what the six study apps share), which
// calls it, and one language, which implements it in its own folder: Japanese fude/lang/ja, Chinese fude/lang/zh,
// Korean fude/lang/ko, Thai fude/lang/th, Hindi fude/lang/hi, Arabic fude/lang/ar. An app compiles exactly one. The
// canvas apps (fude/zoom) teach no language and compile none.
// Besides lang.c, which answers most of this interface, a language's folder has:
//   - lang_ios.m, its iOS side: the text recognizer's options (fude_lang_text_options, below);
//   - wordsplit.c, how its words are found in a line (fude_lang_words, below), which the study's word splitter
//     follows (study/chars/wordsplit.h);
//   - strings.py, its UI strings: the study's string ids that name the language ("Into Japanese", "The Japanese
//     model...") in its own words, and the ids its lang.c returns (group, reading and prompt names). An app's strings
//     tool reads it after the study's;
//   - bake.c, the bake of its character data, in the study's format (study/chars/kanji.h);
//   - chart.c, its letters as a table (fude_lang_chart, below), which the study's chart screen draws
//     (study/screens/chart.h). Chinese has none.
//
// Its main types and their lifecycle: nothing here allocates memory for the caller, and nothing needs to be freed.
// Every string and table returned is constant or static, and stays valid for as long as the program runs. The study's
// word splitter fills a fude_lang_line on its own stack for each line it splits.
//
// Threads: everything here is called on the app's main thread. Some answers are built the first time they are asked
// for, without a lock (Korean's syllable names in fude_lang_latin, Japanese's conjugation rules in its word matcher),
// so none of it is safe to call from another thread.
//
// Files: none, except the bake (fude_bake_run), which reads the language's sources and writes the character data
// file whose format study/chars/kanji.h describes.
//
// Design: the language is chosen when the app is linked, not while it runs. An app teaches exactly one language, so
// the study layer can call these functions directly and holds no language's code itself (docs/architecture.md, "The
// language layer"). The character data's one-byte level and its two reading lists are the language's to fill: the
// study shows and sorts them by what this interface says about them.
// ===========================================================================

// --- codes -------------------------------------------------------------------------

// Returns the language's code as translation (ML Kit Translate) names it, such as "ja". The string is constant.
const c8* fude_lang_code(void);
// Returns the ML Kit Digital Ink Recognition model that reads the language's handwriting, such as "ja". The string is
// constant. The study shell hands it to the handwriting reader (drawing/ink/mlkit.h: fude_mlkit_set_model).
const c8* fude_lang_ink_model(void);
// Returns the voice that reads the language aloud (iOS AVSpeechSynthesizer, and Android's speech), such as "ja-JP".
// The string is constant.
const c8* fude_lang_voice(void);
// Whether ML Kit Text Recognition can read the language in pictures (Text from a photo, a scanned page's lines). It
// can read Japanese, Chinese, Korean and Hindi; it has no model for Thai or Arabic.
b8        fude_lang_text_readable(void);
// Returns the language as one of the app's UI languages: the fourth one, the language the app teaches (text.h). The
// result is its RDE id; *_name is set to its name in itself ("日本語") and *_flag to its flag's image (assets/flags/),
// both constant strings.
RDE_LANGUAGE_ fude_lang_ui_language(const c8** _name, const c8** _flag);

// --- groups: the kinds of character to learn ---------------------------------------

#define FUDE_LANG_GROUPS     4u      // The most groups a language may have.
#define FUDE_LANG_NO_GROUP   0xFFu   // The "group" of a character that is not one to learn (punctuation, Latin).

// A group's flags, which say what kind of group it is.
#define FUDE_LANG_GROUP_SCRIPT 0x01u   // The letters of a script: each is read as itself, has a Latin name
                                       // (fude_lang_latin) and has no meanings (kana; Korean's jamo).
#define FUDE_LANG_GROUP_SET    0x02u   // A set that is learnt whole: it is an exam source and a row of the
                                       // statistics, both made of its core characters (fude_lang_core).

// Returns how many groups the language has, at most FUDE_LANG_GROUPS.
u32       fude_lang_group_count(void);
// Returns the group a character is in, or FUDE_LANG_NO_GROUP when it is in none. Japanese: 0 is hiragana,
// 1 katakana and 2 kanji.
u8        fude_lang_group(u32 _codepoint);
// Returns a group's FUDE_LANG_GROUP_ flags; 0 means a group of characters learnt by level, such as Japanese's kanji.
u32       fude_lang_group_flags(u32 _group);    // FUDE_LANG_GROUP_ flags.
// Whether _codepoint is a letter of one of the language's scripts (its group is flagged FUDE_LANG_GROUP_SCRIPT): read
// as itself, with a Latin name and no meanings (kana; Korean's jamo and syllables; Thai's, Hindi's and Arabic's
// letters). It is worked out from the two above, so every language has it without writing it.
static inline b8 fude_lang_is_letter(u32 _codepoint) {
    const u8 _group = fude_lang_group(_codepoint);
    return _group != FUDE_LANG_NO_GROUP && (fude_lang_group_flags(_group) & FUDE_LANG_GROUP_SCRIPT) != 0u;
}
// Returns a group's name as a FUDE_TEXT_ id ("Hiragana"). For FUDE_LANG_NO_GROUP it returns the name a character
// outside every group is shown under (Japanese shows 々 as "Kanji"). FUDE_TEXT_COUNT means a group out of range.
u32       fude_lang_group_name(u32 _group);     // A FUDE_TEXT_ id.
// Returns what an exam asks the learner to do for a group, as a FUDE_TEXT_ id ("WRITE IN HIRAGANA"), or
// FUDE_TEXT_COUNT for a group out of range. Chinese asks the same for every group.
u32       fude_lang_group_prompt(u32 _group);   // A FUDE_TEXT_ id.
// Whether a character is one of its set's core characters, the ones learnt first. Japanese: the 71 kana of each
// script, not the small ones nor the old ones. Korean: the 40 jamo. Thai, Hindi and Arabic: the letters of level 1.
b8        fude_lang_core(u32 _codepoint);
// Returns a script letter's Latin name ("ka"; ー is "long"), or NULL for anything else. The string stays valid for as
// long as the program runs.
const c8* fude_lang_latin(u32 _codepoint);
// Whether the character is written on another letter (Thai's and Hindi's vowel signs and marks, such as Thai's tone
// marks; Arabic's harakat). Shown on its own, it gets a dotted circle where that letter would go (glyph.h).
b8        fude_lang_combining(u32 _codepoint);

// --- writing direction and joined forms ---------------------------------------------

// Whether the language is written right to left (Arabic). If it is, a word's boxes run from the right.
b8        fude_lang_rtl(void);
// Returns the character a letter is written as between the letters before and after it in a word. _before and
// _after are 0 when there is no letter there; marks do not count as neighbours. Arabic gives its joined forms: ب
// starting a word is ﺑ U+FE91, and ل before ا becomes the ligature ﻻ, in which case *_with_after is set to true to say
// that _after is written in it too (_with_after may be NULL). Every other language returns the letter itself.
u32       fude_lang_form(u32 _before, u32 _cp, u32 _after, b8* _with_after);
// Returns the letter a character is a form of (Arabic: ﺑ U+FE91 gives ب), or the character itself when it is not a
// form. Recognition reads letters, not their forms, so a form the learner writes is right when its letter is read.
u32       fude_lang_letter(u32 _cp);

// --- levels --------------------------------------------------------------------------

#define FUDE_LANG_LEVELS 9u   // The most levels a language may have.

// Returns how many levels the language has; they are listed easiest first (Japanese: JLPT N5 to N1). A character's
// level is one byte of its data (fude_kanji_info.level, where 0 means it has none).
u32       fude_lang_level_count(void);
// Returns the byte a level is stored as, given its place in the list (0 is the easiest), or 0 past the last level.
// Japanese stores N5 as 5, and so on down to N1 as 1.
u8        fude_lang_level_value(u32 _level);
// Writes a stored level's name into _out (_size bytes, NUL-terminated): short ("N5"), for a chip or under a
// character, or long when _long is true ("JLPT N5"), for a header. Writes "" for 0.
void      fude_lang_level_name(u8 _value, b8 _long, c8* _out, usize _size);

// --- readings ------------------------------------------------------------------------

// Returns how many kinds of reading a character's data has, at most two (Japanese: on and kun; Chinese: pinyin only;
// Thai, Hindi and Arabic: none). Each kind is a list joined by 、. A '.' in a reading marks where its stem ends
// (Japanese okurigana), and a '-' marks an affix.
u32       fude_lang_reading_kinds(void);
// Returns a kind of reading's name as a FUDE_TEXT_ id (a sort chip's "On"), or FUDE_TEXT_COUNT for a kind out of
// range.
u32       fude_lang_reading_name(u32 _kind);    // A FUDE_TEXT_ id.
// Turns a reading typed in Latin letters into the way the language writes readings (Japanese: romaji into hiragana),
// written into _out as UTF-8, NUL-terminated, in at most _size bytes. Returns false when the text is not a reading;
// _out then holds what could be read, which may be nothing. Thai, Hindi and Arabic always return false.
b8        fude_lang_reading_from_latin(const c8* _latin, c8* _out, usize _size);
// Returns a character of a reading typed all in capital Latin letters, as the language writes such a reading. Japanese
// writes it in katakana (CHEKKU: チェック, where chekku is ちぇっく); the other languages return _cp as it is.
u32       fude_lang_reading_capital(u32 _cp);
// Returns a reading's character as readings are compared, so that a search finds a reading however it was typed.
// Japanese compares katakana as hiragana. Chinese compares a pinyin vowel without its tone mark, ü as u. Thai, Hindi
// and Arabic compare a Latin letter without its accent (ǎ and ā as a). Returns 0 for a character that is left out,
// such as a tone's mark or a syllable's hyphen: paa-sǎa compares as paasaa.
u32       fude_lang_reading_fold(u32 _codepoint);
// Returns the kind of reading a one-character word is read by: _order 0 gives the kind tried first, and 1 the kind
// tried when the character has no reading of the first. Japanese tries kun, then on.
u32       fude_lang_word_reading_kind(u32 _order);
// Whether words are read in Latin letters (Korean: 학교 is read hakgyo). If they are, a reading typed with a word is
// kept as typed, and a word typed without one is read as its letters' Latin names (fude_lang_latin). If they are
// not, a reading typed in Latin letters is turned into the language's own (fude_lang_reading_from_latin).
b8        fude_lang_word_reading_latin(void);
// Whether a word written _written (UTF-8) is read as it is written, so that a word typed without a reading takes its
// written form as its reading. Japanese: a word all in kana (コーヒー, ありがとう). Every other language returns
// false: its words are read in Latin letters (fude_lang_word_reading_latin) or, Chinese, in pinyin.
b8        fude_lang_reads_as_written(const c8* _written);

// --- badges --------------------------------------------------------------------------

// The characters that headers and labels wear as badges, drawn from the language's own strokes (header.h). The
// comments give Japanese's.
typedef enum {
    FUDE_LANG_BADGE_READING_0 = 0,   // 音: the first kind of reading.
    FUDE_LANG_BADGE_READING_1,       // 訓: the second kind of reading.
    FUDE_LANG_BADGE_PARTS,           // 部: a character's parts.
    FUDE_LANG_BADGE_EXAM,            // 試: the exams.
    FUDE_LANG_BADGE_WORDS,           // 語: words (Vocabulary, the word exams).
    FUDE_LANG_BADGE_LOOKALIKES,      // 似: a character's look-alikes.
    FUDE_LANG_BADGE_NOTE,            // 記: the learner's note.
    FUDE_LANG_BADGE_TRANSLATE,       // 訳: the translator.
    FUDE_LANG_BADGE_COUNT
} FUDE_LANG_BADGE_;

// Returns the code point of the character a badge shows, or 0 for a badge out of range.
u32       fude_lang_badge(FUDE_LANG_BADGE_ _badge);

// --- the chart: the language's letters as a table ----------------------------------------
// A language written in letters shows them as a chart, which study/screens/chart.h draws. The chart has sections (its
// consonants, vowels, marks, digits and so on), each a title over rows of letters. The language says what is in the
// chart and how a narrow screen fits it; the drawing belongs to the study and is the same for every language.

#define FUDE_LANG_CHART_COLUMNS  12u   // The most letters a row may have.
#define FUDE_LANG_CHART_SECTIONS 6u    // The most sections a chart may have.

// A row's flags.
#define FUDE_LANG_CHART_ROW_SUBTITLE  0x01u   // A block starts at this row, under its own subtitle
                                              // (fude_lang_chart_row.subtitle).
#define FUDE_LANG_CHART_ROW_BY_COLUMN 0x02u   // The row is in a block whose letters are listed down its columns, not
                                              // along its rows, so the viewer walks them that way (the gojūon is read
                                              // a i u e o down each column).

// One row of the chart.
typedef struct {
    u32 section;                       // The section it is in. Rows come section by section.
    u32 flags;                         // Its FUDE_LANG_CHART_ROW_ flags.
    u32 subtitle;                      // The FUDE_TEXT_ id shown over it, when it has FUDE_LANG_CHART_ROW_SUBTITLE.
    u32 cp[FUDE_LANG_CHART_COLUMNS];   // Its letters, by column; 0 is an empty cell.
} fude_lang_chart_row;

// One section of the chart.
typedef struct {
    u32       title;      // The FUDE_TEXT_ id of the title over its letters ("Consonants").
    u32       button;     // The FUDE_TEXT_ id of its jump button, in the chart's row of buttons.
    const c8* glyph;      // That button's icon: one of its letters, in UTF-8.
    b8        mirrored;   // Whether its rows are already laid out right to left, so a right-to-left UI keeps them as
                          // they are instead of mirroring them: a row of an Arabic letter's forms keeps the letter
                          // alone at the right, to be read from the right. A row that wraps is not treated this way.
} fude_lang_chart_section;

// The language's chart.
typedef struct {
    // Its sections, in order, and how many there are.
    const fude_lang_chart_section* sections;
    u32                            section_count;
    // Its rows come from a table (rows, row_count), or from a function that fills in row _r and returns false past
    // the last row (Korean's syllable table is made, not listed). When there is a function, it is the one used.
    const fude_lang_chart_row*     rows;
    u32                            row_count;
    b8                           (*row)(u32 _r, fude_lang_chart_row* _out);
    u32                            columns;        // The number of letters in the widest row.
    // How a narrow screen fits the chart. When cell_min is 0, the cells shrink to fit the widest row, and a row never
    // wraps. Otherwise cell_min is the smallest a cell may get, in screen points, and a row with more letters than fit
    // wraps onto the next line. With flow, a section's letters instead run on line after line, whatever their rows
    // (Thai's rows are only groups of eight).
    f32                            cell_min;
    b8                             flow;
    b8                             keep_gaps;      // Whether an empty cell keeps its place while letters follow it in
                                                   // the row, instead of closing up (Arabic needs it, because each of
                                                   // its columns is always one form).
    f32                            section_gap;    // The space between two sections, in screen points.
} fude_lang_chart_def;

// Returns the language's chart, a static table, or NULL when it has none (Chinese has no alphabet to chart).
const fude_lang_chart_def* fude_lang_chart(void);

// --- words: how a line's words are found -------------------------------------------------
// The study finds the dictionary's words in a line of text (study/chars/wordsplit.h): from the left, it takes the
// longest stretch that is a word. The language says which letters a word is looked up for, what each character is
// compared as and where a word may end. A language whose words inflect (Japanese, Korean) finds the word at each
// place itself instead.

#define FUDE_LANG_LINE 256u   // The most characters of a line that are looked at.

// A line made ready to be split. Its lines are joined into one, because a line break is never inside a word on
// purpose: it is handwriting that went on below. Each character is stored as the language compares it
// (fude_lang_words_def.fold), with where it starts.
typedef struct {
    c8  text[4u * FUDE_LANG_LINE + 1u];   // The characters in UTF-8, NUL-terminated.
    u32 cps[FUDE_LANG_LINE];              // The characters, as code points.
    u32 at[FUDE_LANG_LINE + 1u];          // Where each character starts in text, in bytes; at[n] is the end.
    u32 letter_at[FUDE_LANG_LINE + 1u];   // For each character, the first letter (fude_lang_words_def.letter) at or
                                          // after it, or n when there is none. So cps[_i.._i + _len) holds a letter
                                          // when letter_at[_i] < _i + _len.
    u32 n;                                // The number of characters.
} fude_lang_line;

// How the language's words are found in a line. letter is required; the other functions may be NULL.
typedef struct {
    // Whether _cp is a letter a word is looked up for. A stretch without one is passed over (Japanese and Chinese: a
    // Han character, since the learner reads kana; Thai: a character of the Thai block).
    b8  (*letter)(u32 _cp);
    // Returns what _cp is compared as, or 0 when it is left out. Arabic leaves out its vowel marks and compares an
    // alif with a hamza as a bare one, because most text is written without them while the dictionary's words keep
    // theirs. NULL means each character is compared as it is.
    u32 (*fold)(u32 _cp);
    // Whether a word may end between the characters _last and _next (_next is 0 at the end of the line). Thai and
    // Hindi never end a word inside a syllable. NULL means a word may end anywhere.
    b8  (*ends)(u32 _last, u32 _next);
    // Finds the word at _line->cps[_at], for a language whose words inflect. Returns the word, or UINT32_MAX when
    // there is none, and sets *_take to how many characters it covers. *_take is at least 1, also when there is no
    // word, because it says where to look next. Japanese undoes a conjugation; Korean takes the longest start of a
    // chunk that is a word. It looks words up with fude_wordsplit_find (study/chars/wordsplit.h). NULL means the
    // study takes the longest stretch, up to look characters, that is a word as written.
    u32 (*match)(const fude_lang_line* _line, u32 _at, u32* _take);
    u32 look;    // The most characters tried from each place, as code points (Thai's and Hindi's signs count).
    // How many words are offered of those written the same as the one found and as common as it, the one found
    // included. Japanese 本 is ほん and もと, and Chinese 行 is xíng and háng: the data cannot tell them apart, but the
    // learner can. Korean offers 1: its writing is its reading, so the others would be the same row over and over,
    // differing only in meaning.
    u32 alike;
} fude_lang_words_def;

// Returns the language's rules for finding words, a static table.
const fude_lang_words_def* fude_lang_words(void);

// --- handwriting: how free writing is read ------------------------------------------------
// The study's own reader of free writing (study/handwriting/segment.h, used when ML Kit is off) splits a page's strokes
// into characters, and the matcher ranks each run of strokes by its shape (study/handwriting/match.h). The language
// says how its characters stand together. Every field may be NULL (or 0), which leaves its rule out: a language with
// none of them is read by shape, size and place alone. Japanese has them all, for its kana and kanji (lang/ja/lang.c);
// the other languages have none.

#define FUDE_LANG_SCRIPT_NONE 0xFFFFFFFFu   // What fude_lang_writing_def.script returns for a character that says
                                            // nothing of the script around it.

// How the language's free writing is read.
typedef struct {
    // A word is written in one script, so each character's candidate is chosen with its neighbours': switch_cost
    // returns what reading _b right after _a costs, for the scripts they are written in (Japanese: hiragana next to
    // katakana 12, katakana next to kanji 6). NULL: switching costs nothing, and each character keeps its best.
    f32 (*switch_cost)(u32 _a, u32 _b);
    // Look-alikes across scripts, one shape in two (Japanese: ロ and 口, ー and 一). twin returns _cp's, or 0 when it has
    // none. Which one is meant is told by the characters around it: script returns the script a character says they
    // are in (FUDE_LANG_SCRIPT_NONE when it says nothing, such as a mark), and choose returns which of _cp and its
    // twin to read, given the scripts of the nearest characters before and after whose script says something
    // (FUDE_LANG_SCRIPT_NONE when there is none). The three come together, or none of them.
    u32 (*twin)(u32 _cp);
    u32 (*script)(u32 _cp);
    u32 (*choose)(u32 _cp, u32 _before, u32 _after);
    // One letter at two sizes (Japanese: や and ゃ), which the matcher cannot tell apart: size_twin returns _cp at the
    // other size, or 0; small says whether _cp is the small one; small_fits whether the small letter _small can stand
    // between _before and _after (0: nothing there). A small letter where none can stand is read full size. The three
    // come together, or none of them.
    u32 (*size_twin)(u32 _cp);
    b8  (*small)(u32 _cp);
    b8  (*small_fits)(u32 _small, u32 _before, u32 _after);
    // Marks written last that make a letter another (Japanese's voicing marks: two ticks, か が; a ring, は ぱ): voiced
    // returns _base with _marks of them (2 for ticks, 1 for a ring), or 0 when it takes none. voice_size is how big
    // such a mark is at most, in the line's heights: strokes that small at the upper right of the rest are read as
    // marks. NULL: there are none.
    u32 (*voiced)(u32 _base, u32 _marks);
    f32 voice_size;
    // Whether _cp is a punctuation mark with a cell to itself and room after it (Japanese: 、。・). NULL: none.
    b8  (*mark)(u32 _cp);
    // Whether _cp is a letter all but out of use, read only when nothing likelier fits (Japanese: ゐ, ヺ). NULL: none.
    b8  (*rare)(u32 _cp);
    // Whether a character's height and place in its cell say where its line is, so the line's frame is fitted to it
    // (Japanese: not a small kana, nor ー, nor a mark below U+3040). NULL: every character from U+3040 on, the CJK
    // letters; the ones before it (marks, radicals, the alphabets) leave the frame as the strokes measure it.
    b8  (*placed)(u32 _cp);
} fude_lang_writing_def;

// Returns the language's rules for reading free writing, a static table.
const fude_lang_writing_def* fude_lang_writing(void);

// --- the data bake --------------------------------------------------------------------
// The bake is a desktop mode of the app itself, run from the project root with --bake (study/chars/bake.h:
// fude_bake_requested). It turns the language's sources into the character data file the app ships, in
// study/chars/kanji.h's format. Each language has its own, in its bake.c. A phone or tablet has none: the study's
// stand-in takes its place, and fails.

struct fude_bake_app;   // study/chars/bake.h

// Runs the bake for the app _app describes (study/chars/bake.h: fude_bake_app, its paths and its strokes' notice) with
// the app's command line (which can name other source and output paths), reporting on the log. Returns 0 on success;
// anything else is a failure, and the log says why.
i32       fude_bake_run(const struct fude_bake_app* _app, i32 _argc, c8** _argv);

// --- iOS ------------------------------------------------------------------------------

#if defined(__OBJC__)
@class MLKCommonTextRecognizerOptions;
// Returns new ML Kit Text Recognition options for the language's model (lang_ios.m), or nil for a language ML Kit
// has no model for (Thai and Arabic, for which fude_lang_text_readable is false).
MLKCommonTextRecognizerOptions* fude_lang_text_options(void);
#endif

#endif
