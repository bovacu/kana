// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_KANJI
#define FUDE_KANJI

#include "rde.h"
#include "drawing/base/utf8.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// The character data: every kana and kanji Kana knows how to write, with its
// strokes in order, and what KANJIDIC2 says about it. Baked offline from KanjiVG
// and KANJIDIC2 (`--bake`, see bake.h) into one small file the app ships.
//
// LICENCE: derived from KanjiVG (CC BY-SA 3.0, Ulrich Apel), and KANJIDIC2 and
// JMdict (CC BY-SA 4.0, EDRDG), with JLPT N5-N1 levels from Jonathan Waller's JLPT
// Resources (CC BY). The baked file carries the same licences, and the app MUST
// show the attribution (assets/data/LICENSE-data.txt).
//
// STROKES ARE STORED AS CURVES, not points: KanjiVG's own cubic Béziers, as
// 16-bit fixed point. All ~6,700 characters come to a few MB, several times
// smaller than points at scoring density; turning one character's curves into
// points costs microseconds, and only the character on screen is ever turned.
//
// FILE ('KANA' header, kind 'CHAR', see kfile.h):
//   'CHRS'  u32 count, u32 record size, then per character, SORTED by codepoint:
//             u32 codepoint, u32 geometry offset, u32 text offset (or UINT32_MAX),
//             u16 frequency rank (0: none), u8 stroke count, u8 school grade (0),
//             u8 old JLPT level 4..1 (0), u8 classical radical (0),
//             u8 level (0: none), 1 reserved — the language's levels (lang.h:
//             fude_lang_level_value; Japanese the JLPT's N5..N1 as 5..1)
//           (The level took a byte that was reserved and zero: files baked
//           before it simply read as "no level".)
//   'GEOM'  the strokes, back to back. Per stroke:
//             u8 segment count, u8 type, u8 type variant, u8 alternative type,
//             i16 start x, y, then per segment i16 c1x c1y c2x c2y x y
//           Coordinates: KanjiVG's 109-unit box, Y DOWN (as in SVG), times
//           FUDE_KANJI_FIXED. Types: the CJK Strokes block (U+31C0..U+31EF) as
//           code - 0x31C0 + 1, 0 when none; the variant is KanjiVG's letter
//           ('a', 'b', ...) or 0; the alternative is the type after a '/'.
//   'TEXT'  per character with text: three NUL-terminated UTF-8 strings — the
//           readings of the language's two kinds (lang.h; Japanese on, then
//           kun), each joined by "、", and English meanings joined by ", ".
//   'PART'  (optional: files baked before it have none) u32 count (the CHRS
//           count), then count u32 offsets into the lists after them (UINT32_MAX:
//           none), then the lists: u8 n, then n u32 code points — the PARTS a
//           character is built from, every element of KanjiVG's group tree
//           (and a variant's original: 亻 brings 人), the character itself left
//           out. 語 is 言 口 吾 五 二; 休 is 亻 人 木.
//   'WORD'  (optional) words from JMdict: u32 count (the CHRS count), then
//           count u32 offsets into the lists (UINT32_MAX: none), u32 word count,
//           u32 the lists' size, the lists — u8 n, u8 examples, then n u32 word
//           numbers, best first: the first `examples` are the character's
//           examples (common words, shown), the rest more to add (the viewer's
//           Add) — and then the words to the chunk's end, in number order: three
//           NUL-terminated UTF-8 strings each, the written form, its reading
//           (kana) and its meaning (English glosses joined by "; "). 日 has 日本
//           にほん "Japan"; a word is stored once, however many of its kanji list it.
//   'LNxx'  (optional) meanings in another language, xx its code ('LNes', 'LNpt',
//           'LNfr'): u32 n, n pairs (u32 code point, u32 text offset) sorted
//           by code point — the kanji's meanings (KANJIDIC2's m_lang); u32 m, m
//           pairs (u32 word number, u32 text offset) sorted by number — the
//           words' (JMdict's glosses in it, when full JMdict was baked); u32 the
//           text's size, the text (NUL-terminated strings). Anything not there
//           shows in English.
//   'WFRQ'  (optional) how common each word is: u32 count (the word count), then
//           a u16 per word, lower the commoner (JMdict's newspaper band, else
//           about where its other lists sit; 1000 on: not common). For telling
//           words spelt alike apart (本 ほん, 本 もと). The words: every kanji's, and
//           every other COMMON word of the characters here (on no kanji's list:
//           wordsplit.h reads text with them).
//   'SENT'  (optional) example sentences, Tatoeba's (CC BY 2.0 FR): u32 n, u32
//           the text's size, then n sentences, each five NUL-terminated strings —
//           the Japanese, then English, Spanish, French, Portuguese ("" where it
//           has none); then u32 the word count and a u32 per word: its sentence
//           (UINT32_MAX: none) — one with the word in it, a comfortable length.
//   'LOOK'  (optional) look-alikes: u32 count (the CHRS count), then per
//           character FUDE_KANJI_LOOKALIKES u32 code points (0: none), closest
//           first — the common characters whose strokes the matcher (match.h)
//           finds closest to its own (未 末, 土 士, シ ツ). Baked after the rest
//           (the matcher reads the file).
//
// THE STROKES IN A FILE OF THEIR OWN: a file without 'GEOM' has its strokes in
// FUDE_KANJI_STROKES_FILE beside it (kind 'STRK': a 'NOTE' chunk — how and when
// they were changed, as their licence asks — then 'GEOM'). Hanzi's are: Make Me a
// Hanzi's, under the Arphic Public License, which wants them kept apart.
// ===========================================================================

#define FUDE_KANJI_VERSION      1u
#define FUDE_KANJI_FILE         "assets/data/characters.kana"
#define FUDE_KANJI_BOX          109.0f     // KanjiVG's coordinate box
#define FUDE_KANJI_FIXED        64.0f      // stored coordinate = units * this
#define FUDE_KANJI_RECORD_SIZE  20u
#define FUDE_KANJI_STROKE_BASE  0x31C0u    // the CJK Strokes block

#define FUDE_KANJI_KIND         FUDE_TAG('C', 'H', 'A', 'R')
#define FUDE_KANJI_STROKES_KIND FUDE_TAG('S', 'T', 'R', 'K')
#define FUDE_KANJI_STROKES_FILE "strokes.kana"   // beside the character data, when its strokes are apart
#define FUDE_KANJI_CHUNK_NOTE   FUDE_TAG('N', 'O', 'T', 'E')
#define FUDE_KANJI_CHUNK_CHARS  FUDE_TAG('C', 'H', 'R', 'S')
#define FUDE_KANJI_CHUNK_GEOM   FUDE_TAG('G', 'E', 'O', 'M')
#define FUDE_KANJI_CHUNK_TEXT   FUDE_TAG('T', 'E', 'X', 'T')
#define FUDE_KANJI_CHUNK_PARTS  FUDE_TAG('P', 'A', 'R', 'T')
#define FUDE_KANJI_CHUNK_WORDS  FUDE_TAG('W', 'O', 'R', 'D')
#define FUDE_KANJI_CHUNK_WORD_FREQ FUDE_TAG('W', 'F', 'R', 'Q')
#define FUDE_KANJI_CHUNK_SENTENCES FUDE_TAG('S', 'E', 'N', 'T')
#define FUDE_KANJI_CHUNK_LOOK      FUDE_TAG('L', 'O', 'O', 'K')
#define FUDE_KANJI_LOOKALIKES      4u    // look-alikes kept per character
#define FUDE_KANJI_MAX_PARTS    32u
#define FUDE_KANJI_MAX_WORDS    20u    // words a character can list (examples, then more)
#define FUDE_KANJI_LANGUAGES    8u     // other languages' meanings read ('LNxx')

// One other language's meanings, in place in the file.
RDE_STRUCT {
    c8        code[3];      // "es"
    const u8* kanji;        // pairs (code point, offset), sorted
    u32       kanji_count;
    const u8* words;        // pairs (word number, offset), sorted
    u32       word_count;
    const c8* text;
    u32       text_size;
} fude_kanji_language;

// One character's record, decoded.
RDE_STRUCT {
    u32 codepoint;
    u32 geometry;       // offset into GEOM
    u32 text;           // offset into TEXT, or UINT32_MAX
    u16 frequency;      // rank among ~2,500 newspaper kanji; 0 = not ranked
    u8  strokes;
    u8  grade;          // 1..6 Kyouiku, 8 = rest of Jouyou, 9/10 = Jinmeiyou; 0 = none
    u8  jlpt;           // the OLD 4..1 levels; 0 = none
    u8  radical;        // classical (Kangxi) radical number; 0 = none
    u8  level;          // the language's level (lang.h); Japanese N5..N1 as 5..1 (Waller's
                        // community lists: since 2010 the JLPT publishes none); 0 = none
} fude_kanji_info;

// One stroke, as stored (units: KanjiVG's box, Y down).
RDE_STRUCT {
    rde_vec_2F start;
    u32        segments;       // cubic Bézier segments after start
    const u8*  _data;          // the segments, still fixed point
    u32        type;           // stroke type code point (U+31C0..), 0 = none
    c8         variant;        // 'a', 'b', ... or 0
    u32        alternative;    // an equally valid type, 0 = none
} fude_kanji_stroke;

// The loaded file. Kept whole in memory (a few MB) and read in place.
RDE_STRUCT {
    u8*       _file;
    u32       _file_size;
    u8*       _strokes_file;   // the strokes' own file, when they are apart (NULL: in _file)
    const u8* _records;
    u32       count;
    const u8* _geometry;
    u32       _geometry_size;
    const c8* _text;
    u32       _text_size;
    const u8* _parts_index;    // count u32 offsets into _parts, or NULL (no PART chunk)
    const u8* _parts;
    u32       _parts_size;
    const u8* _words_index;    // count u32 offsets into _word_lists, or NULL (no WORD chunk)
    const u8* _word_lists;
    u32       _word_lists_size;
    u32       word_count;
    const c8** _word_text;     // word_count pointers to each word's written form (then reading, meaning)
    const u8*  _word_freq;     // 'WFRQ': a u16 per word, or NULL
    const c8** _sentences;     // 'SENT': each sentence's Japanese (its translations follow), or NULL
    u32        sentence_count;
    const u8*  _sentence_of;   // a u32 per word: its sentence, or NULL
    const u8*  _look;          // 'LOOK': FUDE_KANJI_LOOKALIKES u32 per character, or NULL
    fude_kanji_language _languages[FUDE_KANJI_LANGUAGES];
    u32                 _language_count;
    i32                 _language;   // the meanings shown: one of _languages, -1 English
} fude_kanji_db;

// An example word (see 'WORD'), UTF-8.
RDE_STRUCT {
    const c8* written;   // 日本
    const c8* reading;   // にほん
    const c8* meaning;   // Japan
} fude_kanji_word;

b8   fude_kanji_load(fude_kanji_db* _db, const c8* _path);
void fude_kanji_unload(fude_kanji_db* _db);

// By position (codepoint order) or by codepoint.
b8   fude_kanji_at(const fude_kanji_db* _db, u32 _index, fude_kanji_info* _out);
b8   fude_kanji_find(const fude_kanji_db* _db, u32 _codepoint, fude_kanji_info* _out);
// The record index of a code point (for lists of records). False when absent.
b8   fude_kanji_find_index(const fude_kanji_db* _db, u32 _codepoint, u32* _index);

// Stroke _index (0-based, in writing order) of a character.
b8   fude_kanji_stroke_at(const fude_kanji_db* _db, const fude_kanji_info* _info, u32 _index, fude_kanji_stroke* _out);

// Segment _segment of a stroke: a cubic Bézier from where the one before ends
// (the first: from start) — its two control points, then its end (KanjiVG
// units, Y down). For drawing the curves as they are (a PDF's).
void fude_kanji_stroke_segment(const fude_kanji_stroke* _stroke, u32 _segment, rde_vec_2F _out[3]);

// A stroke as points (KanjiVG units, Y down), fine enough that no chord strays
// more than _tolerance units from the curve. Writes at most _max points and
// returns how many.
u32  fude_kanji_stroke_points(const fude_kanji_stroke* _stroke, f32 _tolerance, rde_vec_2F* _out, u32 _max);

// The parts of record _index (see 'PART'): up to _max code points into _out;
// how many. 0 when it has none, or the file has no parts.
u32  fude_kanji_parts(const fude_kanji_db* _db, u32 _index, u32* _out, u32 _max);
b8   fude_kanji_has_parts(const fude_kanji_db* _db);

// The words of record _index, best first: up to _max word numbers into _out;
// how many. 0 when it has none, or the file has no words. _examples (may be
// NULL) gets how many of the first are its examples — the rest are more to add.
u32  fude_kanji_words(const fude_kanji_db* _db, u32 _index, u32* _out, u32 _max, u32* _examples);
// Word number _word. False when there is no such word.
b8   fude_kanji_word_at(const fude_kanji_db* _db, u32 _word, fude_kanji_word* _out);

// The character's text, "" when there is none. Meanings in the language set
// (fude_kanji_set_language) when the file has them, in English otherwise.
// Its readings of kind _kind (0 or 1: the language's, lang.h; Japanese on, kun).
const c8* fude_kanji_reading(const fude_kanji_db* _db, const fude_kanji_info* _info, u32 _kind);
const c8* fude_kanji_meanings(const fude_kanji_db* _db, const fude_kanji_info* _info);
const c8* fude_kanji_meanings_english(const fude_kanji_db* _db, const fude_kanji_info* _info);

// The meanings (a kanji's, a word's) shown in language _code ("es", "pt", "fr";
// NULL or anything the file lacks: English). Words and kanji without one in
// it stay English.
void      fude_kanji_set_language(fude_kanji_db* _db, const c8* _code);
// An example sentence ('SENT'): the Japanese and its translation.
RDE_STRUCT {
    const c8* japanese;
    const c8* translation;   // in the language set (fude_kanji_set_language), else English
} fude_kanji_sentence;

// Word _word's example sentence. False when it has none.
b8        fude_kanji_word_sentence(const fude_kanji_db* _db, u32 _word, fude_kanji_sentence* _out);

// The characters that look like record _index's ('LOOK'): code points, closest
// first, at most _max into _out. How many (0: none, or the file has none).
u32       fude_kanji_lookalikes(const fude_kanji_db* _db, u32 _index, u32* _out, u32 _max);

// How common word _word is: lower the commoner ('WFRQ'); 0xFFFF when unknown.
u16       fude_kanji_word_freq(const fude_kanji_db* _db, u32 _word);
// Word _word's meaning in English, whatever the language (search).
const c8* fude_kanji_word_meaning_english(const fude_kanji_db* _db, u32 _word);


#endif
