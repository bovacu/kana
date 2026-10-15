// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_KANJI
#define FUDE_KANJI

#include "rde.h"
#include "drawing/base/utf8.h"
#include "drawing/base/kfile.h"

// ===========================================================================
// Character data: every character the app knows how to write (for Kana, every kana and kanji), with its strokes in
// order, and what the dictionaries say about it. It is baked offline from KanjiVG and KANJIDIC2 (`--bake`, see
// bake.h) into one small file that the app ships.
//
// Where it sits: the study layer's character data (study/chars). The study shell (shell.h) loads it once at start;
// nearly every study screen, the handwriting matcher (match.h), the glyph drawing (glyph.h) and the word splitter
// (wordsplit.h) read it.
// Its main types and their lifecycle: the shell owns the one fude_kanji_db (a static in shell.c), filled by
// fude_kanji_load and freed by fude_kanji_unload at the end. The file is kept whole in memory and read in place:
// every string and stroke handed out points into it, and stays valid until fude_kanji_unload. fude_kanji_info,
// fude_kanji_stroke, fude_kanji_word and fude_kanji_sentence are small values the caller keeps.
// Threads: it is loaded and read on the app's main thread. Once loaded the data does not change, except for the
// language of the meanings (fude_kanji_set_language), so reading it from another thread would be safe only while
// nothing sets the language or unloads it.
// Files: FUDE_KANJI_FILE (and FUDE_KANJI_STROKES_FILE beside it, when the strokes are kept apart), in the format
// below.
//
// LICENCE: the data is derived from KanjiVG (CC BY-SA 3.0, Ulrich Apel), from KANJIDIC2 and JMdict (CC BY-SA 4.0,
// EDRDG), and its JLPT N5-N1 levels from Jonathan Waller's JLPT Resources (CC BY). The baked file carries the same
// licences, and the app MUST show the attribution (assets/data/LICENSE-data.txt).
//
// STROKES ARE STORED AS CURVES, not points: KanjiVG's own cubic Béziers, in 16-bit fixed point. All ~6,700
// characters come to a few MB, several times smaller than points at the density scoring needs. Turning one
// character's curves into points costs microseconds, and only the character on screen is ever turned.
//
// THE FILE has a 'KANA' header of kind 'CHAR' (see kfile.h), then these chunks:
//   'CHRS'  u32 count, u32 record size, then a record per character, SORTED by code point: u32 code point, u32
//           geometry offset, u32 text offset (or UINT32_MAX), u16 frequency rank (0: none), u8 stroke count, u8
//           school grade (0: none), u8 old JLPT level 4..1 (0: none), u8 classical radical (0: none), u8 level (0:
//           none), and 1 reserved byte. The level is the language's (lang.h: fude_lang_level_value); for Japanese it
//           is the JLPT's N5..N1 as 5..1. The level took a byte that used to be reserved and zero, so files baked
//           before it simply read as having no level.
//   'GEOM'  the strokes, back to back. Each stroke is: u8 segment count, u8 type, u8 type variant, u8 alternative
//           type, i16 start x and y, then for each segment i16 c1x c1y c2x c2y x y. Coordinates are in KanjiVG's
//           109-unit box, Y DOWN (as in SVG), times FUDE_KANJI_FIXED. A type is a code point of the CJK Strokes block
//           (U+31C0..U+31EF) stored as code - 0x31C0 + 1, or 0 when there is none; the variant is KanjiVG's letter
//           ('a', 'b', ...) or 0; the alternative is the type written after a '/'.
//   'TEXT'  for each character with text, three NUL-terminated UTF-8 strings: the readings of the language's two
//           kinds (lang.h; for Japanese, on and then kun), each joined by "、", and the English meanings joined by
//           ", ".
//   'PART'  (optional: files baked before it have none) u32 count (the CHRS count), then count u32 offsets into the
//           lists that follow them (UINT32_MAX: none), then the lists, each a u8 n and n u32 code points. They are the
//           PARTS a character is built from: every element of KanjiVG's group tree (and a variant's original: 亻
//           brings 人), leaving out the character itself. 語 is 言 口 吾 五 二; 休 is 亻 人 木.
//   'WORD'  (optional) words from JMdict: u32 count (the CHRS count), then count u32 offsets into the lists
//           (UINT32_MAX: none), u32 word count, u32 the lists' size, then the lists. Each list is u8 n, u8 examples,
//           then n u32 word numbers, best first: the first `examples` of them are the character's examples (common
//           words, which are shown), and the rest are more words to add (the viewer's Add). Then come the words, to
//           the chunk's end, in number order, each three NUL-terminated UTF-8 strings: the written form, its reading
//           (kana) and its meaning (English glosses joined by "; "). 日 has 日本 にほん "Japan". A word is stored once,
//           however many of its kanji list it.
//   'LNxx'  (optional) meanings in another language, xx being its code ('LNes', 'LNpt', 'LNfr'): u32 n and n pairs
//           (u32 code point, u32 text offset) sorted by code point, for the kanji's meanings (KANJIDIC2's m_lang);
//           u32 m and m pairs (u32 word number, u32 text offset) sorted by number, for the words' meanings (JMdict's
//           glosses in that language, when the full JMdict was baked); then u32 the text's size, and the text
//           (NUL-terminated strings). Anything that is not there shows in English.
//   'WFRQ'  (optional) how common each word is: u32 count (the word count), then a u16 per word, lower for commoner
//           words (JMdict's newspaper band, or else about where its other lists sit; 1000 and above: not common). It
//           tells words spelt alike apart (本 ほん, 本 もと). The words are every kanji's, and every other COMMON word
//           made of the characters here (on no kanji's list: wordsplit.h reads text with them).
//   'SENT'  (optional) example sentences from Tatoeba (CC BY 2.0 FR): u32 n, u32 the text's size, then n sentences,
//           each five NUL-terminated strings: the Japanese, then English, Spanish, French and Portuguese ("" where
//           it has none). Then u32 the word count and a u32 per word: its sentence (UINT32_MAX: none), one with the
//           word in it, of a comfortable length.
//   'LOOK'  (optional) look-alikes: u32 count (the CHRS count), then for each character FUDE_KANJI_LOOKALIKES u32
//           code points (0: none), closest first. They are the common characters whose strokes the matcher (match.h)
//           finds closest to its own (未 末, 土 士, シ ツ). This chunk is baked after the rest, because the matcher
//           reads the file.
//
// THE STROKES IN A FILE OF THEIR OWN: a file without 'GEOM' has its strokes in FUDE_KANJI_STROKES_FILE beside it
// (of kind 'STRK': a 'NOTE' chunk saying how and when they were changed, as their licence asks, then 'GEOM').
// Hanzi's strokes are kept this way: they are Make Me a Hanzi's, under the Arphic Public License, which wants them
// kept apart.
//
// A chunk that does not match the records (a different count) is ignored rather than read wrongly: the app then
// has none of what it holds.
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

// One other language's meanings ('LNxx'), read in place in the file.
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
    u16 frequency;      // its rank among ~2,500 newspaper kanji; 0 = not ranked
    u8  strokes;
    u8  grade;          // 1..6 Kyouiku, 8 = the rest of Jouyou, 9 or 10 = Jinmeiyou; 0 = none
    u8  jlpt;           // the OLD JLPT's levels, 4..1; 0 = none
    u8  radical;        // classical (Kangxi) radical number; 0 = none
    u8  level;          // the language's level (lang.h); for Japanese N5..N1 as 5..1, from Waller's
                        // community lists, since the JLPT has published none since 2010; 0 = none
} fude_kanji_info;

// One stroke, as stored (in the units of KanjiVG's box, Y down). It points into the loaded file.
RDE_STRUCT {
    rde_vec_2F start;
    u32        segments;       // the cubic Bézier segments after start
    const u8*  _data;          // the segments, still fixed point
    u32        type;           // the stroke type's code point (U+31C0..), 0 = none
    c8         variant;        // 'a', 'b', ... or 0
    u32        alternative;    // an equally valid type, 0 = none
} fude_kanji_stroke;

// The loaded file, kept whole in memory (a few MB) and read in place.
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
    const c8** _word_text;     // word_count pointers to each word's written form (its reading and meaning follow)
    const u8*  _word_freq;     // 'WFRQ': a u16 per word, or NULL
    const c8** _sentences;     // 'SENT': each sentence's Japanese (its translations follow), or NULL
    u32        sentence_count;
    const u8*  _sentence_of;   // a u32 per word: its sentence, or NULL
    const u8*  _look;          // 'LOOK': FUDE_KANJI_LOOKALIKES u32 per character, or NULL
    fude_kanji_language _languages[FUDE_KANJI_LANGUAGES];
    u32                 _language_count;
    i32                 _language;   // the meanings shown: one of _languages, or -1 for English
} fude_kanji_db;

// An example word (see 'WORD'), in UTF-8, pointing into the loaded file.
RDE_STRUCT {
    const c8* written;   // 日本
    const c8* reading;   // にほん
    const c8* meaning;   // Japan
} fude_kanji_word;

// Loads the character data at _path (and its strokes' own file beside it, when they are apart). Returns false, with
// _db left empty and the reason logged, when the file is missing, is not one this build reads, or is damaged.
// fude_kanji_unload frees it all; every pointer into it is then invalid.
b8   fude_kanji_load(fude_kanji_db* _db, const c8* _path);
void fude_kanji_unload(fude_kanji_db* _db);

// Decodes a character's record into _out: by its position (_index, in code point order) or by its code point.
// Returns false when there is no such record, or no data at all (_db NULL: an app started without it).
b8   fude_kanji_at(const fude_kanji_db* _db, u32 _index, fude_kanji_info* _out);
b8   fude_kanji_find(const fude_kanji_db* _db, u32 _codepoint, fude_kanji_info* _out);
// Finds the record index of a code point (lists of characters hold record indices). Returns false when the data
// does not have it, or there is no data (_db NULL).
b8   fude_kanji_find_index(const fude_kanji_db* _db, u32 _codepoint, u32* _index);

// Gets stroke _index (0-based, in writing order) of a character. Returns false when there is no such stroke or its
// data is cut short.
b8   fude_kanji_stroke_at(const fude_kanji_db* _db, const fude_kanji_info* _info, u32 _index, fude_kanji_stroke* _out);

// Gets segment _segment of a stroke: a cubic Bézier from where the one before ends (the first starts at start). _out
// gets its two control points, then its end (KanjiVG units, Y down). It is for drawing the curves as they are (a
// PDF's); a segment out of range gives the stroke's start three times.
void fude_kanji_stroke_segment(const fude_kanji_stroke* _stroke, u32 _segment, rde_vec_2F _out[3]);

// Turns a stroke into points (KanjiVG units, Y down), fine enough that no chord strays more than _tolerance units
// from the curve. Writes at most _max points and returns how many.
u32  fude_kanji_stroke_points(const fude_kanji_stroke* _stroke, f32 _tolerance, rde_vec_2F* _out, u32 _max);

// Gets the parts of record _index (see 'PART'): up to _max code points into _out. Returns how many: 0 when it has
// none, or the file has no parts. fude_kanji_has_parts says whether the file has parts at all.
u32  fude_kanji_parts(const fude_kanji_db* _db, u32 _index, u32* _out, u32 _max);
b8   fude_kanji_has_parts(const fude_kanji_db* _db);

// Gets the words of record _index, best first: up to _max word numbers into _out. Returns how many: 0 when it has
// none, or the file has no words. _examples (may be NULL) gets how many of the first are its examples; the rest are
// more words to add.
u32  fude_kanji_words(const fude_kanji_db* _db, u32 _index, u32* _out, u32 _max, u32* _examples);
// Gets word number _word, its meaning in the language set (fude_kanji_set_language) when the file has it. Returns
// false when there is no such word.
b8   fude_kanji_word_at(const fude_kanji_db* _db, u32 _word, fude_kanji_word* _out);

// A character's text, or "" when it has none; the strings point into the loaded file. fude_kanji_reading gives its
// readings of kind _kind (0 or 1, the language's two kinds in lang.h: for Japanese, on and kun). fude_kanji_meanings
// gives its meanings in the language set (fude_kanji_set_language) when the file has them, and in English
// otherwise; fude_kanji_meanings_english always in English.
const c8* fude_kanji_reading(const fude_kanji_db* _db, const fude_kanji_info* _info, u32 _kind);
const c8* fude_kanji_meanings(const fude_kanji_db* _db, const fude_kanji_info* _info);
const c8* fude_kanji_meanings_english(const fude_kanji_db* _db, const fude_kanji_info* _info);

// Sets the language the meanings (a kanji's, a word's, a sentence's translation) are given in: _code is "es", "pt"
// or "fr", and NULL or anything the file lacks means English. Words and kanji without a meaning in it stay in
// English.
void      fude_kanji_set_language(fude_kanji_db* _db, const c8* _code);
// An example sentence ('SENT'): the sentence in the language taught, and its translation, pointing into the loaded
// file.
RDE_STRUCT {
    const c8* taught;
    const c8* translation;   // in the language set (fude_kanji_set_language), else English
} fude_kanji_sentence;

// Gets word _word's example sentence. Returns false when it has none, or the file has no sentences.
b8        fude_kanji_word_sentence(const fude_kanji_db* _db, u32 _word, fude_kanji_sentence* _out);

// Gets the characters that look like record _index's ('LOOK'): their code points, closest first, at most _max into
// _out. Returns how many: 0 when it has none, or the file has no look-alikes.
u32       fude_kanji_lookalikes(const fude_kanji_db* _db, u32 _index, u32* _out, u32 _max);

// Returns how common word _word is ('WFRQ'): lower for commoner words, 0xFFFF when it is not known.
u16       fude_kanji_word_freq(const fude_kanji_db* _db, u32 _word);
// Returns word _word's meaning in English, whatever the language set (for search); "" when there is no such word.
const c8* fude_kanji_word_meaning_english(const fude_kanji_db* _db, u32 _word);


#endif
