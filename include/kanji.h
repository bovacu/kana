#ifndef KANA_KANJI
#define KANA_KANJI

#include "rde.h"
#include "kfile.h"

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
//             u8 JLPT N-level 5..1 (0: not on the lists), 1 reserved
//           (The N-level took a byte that was reserved and zero: files baked
//           before it simply read as "not on the lists".)
//   'GEOM'  the strokes, back to back. Per stroke:
//             u8 segment count, u8 type, u8 type variant, u8 alternative type,
//             i16 start x, y, then per segment i16 c1x c1y c2x c2y x y
//           Coordinates: KanjiVG's 109-unit box, Y DOWN (as in SVG), times
//           KANA_KANJI_FIXED. Types: the CJK Strokes block (U+31C0..U+31EF) as
//           code - 0x31C0 + 1, 0 when none; the variant is KanjiVG's letter
//           ('a', 'b', ...) or 0; the alternative is the type after a '/'.
//   'TEXT'  per character with text: three NUL-terminated UTF-8 strings — on
//           readings joined by "、", kun readings joined by "、", English
//           meanings joined by ", ".
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
//           character KANA_KANJI_LOOKALIKES u32 code points (0: none), closest
//           first — the common characters whose strokes the matcher (match.h)
//           finds closest to its own (未 末, 土 士, シ ツ). Baked after the rest
//           (the matcher reads the file).
// ===========================================================================

#define KANA_KANJI_VERSION      1u
#define KANA_KANJI_FILE         "assets/data/characters.kana"
#define KANA_KANJI_BOX          109.0f     // KanjiVG's coordinate box
#define KANA_KANJI_FIXED        64.0f      // stored coordinate = units * this
#define KANA_KANJI_RECORD_SIZE  20u
#define KANA_KANJI_STROKE_BASE  0x31C0u    // the CJK Strokes block

#define KANA_KANJI_KIND         KANA_TAG('C', 'H', 'A', 'R')
#define KANA_KANJI_CHUNK_CHARS  KANA_TAG('C', 'H', 'R', 'S')
#define KANA_KANJI_CHUNK_GEOM   KANA_TAG('G', 'E', 'O', 'M')
#define KANA_KANJI_CHUNK_TEXT   KANA_TAG('T', 'E', 'X', 'T')
#define KANA_KANJI_CHUNK_PARTS  KANA_TAG('P', 'A', 'R', 'T')
#define KANA_KANJI_CHUNK_WORDS  KANA_TAG('W', 'O', 'R', 'D')
#define KANA_KANJI_CHUNK_WORD_FREQ KANA_TAG('W', 'F', 'R', 'Q')
#define KANA_KANJI_CHUNK_SENTENCES KANA_TAG('S', 'E', 'N', 'T')
#define KANA_KANJI_CHUNK_LOOK      KANA_TAG('L', 'O', 'O', 'K')
#define KANA_KANJI_LOOKALIKES      4u    // look-alikes kept per character
#define KANA_KANJI_MAX_PARTS    32u
#define KANA_KANJI_MAX_WORDS    20u    // words a character can list (examples, then more)
#define KANA_KANJI_LANGUAGES    8u     // other languages' meanings read ('LNxx')

// One other language's meanings, in place in the file.
RDE_STRUCT {
    c8        code[3];      // "es"
    const u8* kanji;        // pairs (code point, offset), sorted
    u32       kanji_count;
    const u8* words;        // pairs (word number, offset), sorted
    u32       word_count;
    const c8* text;
    u32       text_size;
} kana_kanji_language;

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
    u8  jlpt_n;         // N5..N1 as 5..1 (Waller's community lists; since 2010 the
                        // JLPT publishes no official kanji lists); 0 = not listed
} kana_kanji_info;

// One stroke, as stored (units: KanjiVG's box, Y down).
RDE_STRUCT {
    rde_vec_2F start;
    u32        segments;       // cubic Bézier segments after start
    const u8*  _data;          // the segments, still fixed point
    u32        type;           // stroke type code point (U+31C0..), 0 = none
    c8         variant;        // 'a', 'b', ... or 0
    u32        alternative;    // an equally valid type, 0 = none
} kana_kanji_stroke;

// The loaded file. Kept whole in memory (a few MB) and read in place.
RDE_STRUCT {
    u8*       _file;
    u32       _file_size;
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
    const u8*  _look;          // 'LOOK': KANA_KANJI_LOOKALIKES u32 per character, or NULL
    kana_kanji_language _languages[KANA_KANJI_LANGUAGES];
    u32                 _language_count;
    i32                 _language;   // the meanings shown: one of _languages, -1 English
} kana_kanji_db;

// An example word (see 'WORD'), UTF-8.
RDE_STRUCT {
    const c8* written;   // 日本
    const c8* reading;   // にほん
    const c8* meaning;   // Japan
} kana_kanji_word;

b8   kana_kanji_load(kana_kanji_db* _db, const c8* _path);
void kana_kanji_unload(kana_kanji_db* _db);

// By position (codepoint order) or by codepoint.
b8   kana_kanji_at(const kana_kanji_db* _db, u32 _index, kana_kanji_info* _out);
b8   kana_kanji_find(const kana_kanji_db* _db, u32 _codepoint, kana_kanji_info* _out);
// The record index of a code point (for lists of records). False when absent.
b8   kana_kanji_find_index(const kana_kanji_db* _db, u32 _codepoint, u32* _index);

// Stroke _index (0-based, in writing order) of a character.
b8   kana_kanji_stroke_at(const kana_kanji_db* _db, const kana_kanji_info* _info, u32 _index, kana_kanji_stroke* _out);

// Segment _segment of a stroke: a cubic Bézier from where the one before ends
// (the first: from start) — its two control points, then its end (KanjiVG
// units, Y down). For drawing the curves as they are (a PDF's).
void kana_kanji_stroke_segment(const kana_kanji_stroke* _stroke, u32 _segment, rde_vec_2F _out[3]);

// A stroke as points (KanjiVG units, Y down), fine enough that no chord strays
// more than _tolerance units from the curve. Writes at most _max points and
// returns how many.
u32  kana_kanji_stroke_points(const kana_kanji_stroke* _stroke, f32 _tolerance, rde_vec_2F* _out, u32 _max);

// The parts of record _index (see 'PART'): up to _max code points into _out;
// how many. 0 when it has none, or the file has no parts.
u32  kana_kanji_parts(const kana_kanji_db* _db, u32 _index, u32* _out, u32 _max);
b8   kana_kanji_has_parts(const kana_kanji_db* _db);

// The words of record _index, best first: up to _max word numbers into _out;
// how many. 0 when it has none, or the file has no words. _examples (may be
// NULL) gets how many of the first are its examples — the rest are more to add.
u32  kana_kanji_words(const kana_kanji_db* _db, u32 _index, u32* _out, u32 _max, u32* _examples);
// Word number _word. False when there is no such word.
b8   kana_kanji_word_at(const kana_kanji_db* _db, u32 _word, kana_kanji_word* _out);

// The character's text, "" when there is none. Meanings in the language set
// (kana_kanji_set_language) when the file has them, in English otherwise.
const c8* kana_kanji_on(const kana_kanji_db* _db, const kana_kanji_info* _info);
const c8* kana_kanji_kun(const kana_kanji_db* _db, const kana_kanji_info* _info);
const c8* kana_kanji_meanings(const kana_kanji_db* _db, const kana_kanji_info* _info);
const c8* kana_kanji_meanings_english(const kana_kanji_db* _db, const kana_kanji_info* _info);

// The meanings (a kanji's, a word's) shown in language _code ("es", "pt", "fr";
// NULL or anything the file lacks: English). Words and kanji without one in
// it stay English.
void      kana_kanji_set_language(kana_kanji_db* _db, const c8* _code);
// An example sentence ('SENT'): the Japanese and its translation.
RDE_STRUCT {
    const c8* japanese;
    const c8* translation;   // in the language set (kana_kanji_set_language), else English
} kana_kanji_sentence;

// Word _word's example sentence. False when it has none.
b8        kana_kanji_word_sentence(const kana_kanji_db* _db, u32 _word, kana_kanji_sentence* _out);

// The characters that look like record _index's ('LOOK'): code points, closest
// first, at most _max into _out. How many (0: none, or the file has none).
u32       kana_kanji_lookalikes(const kana_kanji_db* _db, u32 _index, u32* _out, u32 _max);

// How common word _word is: lower the commoner ('WFRQ'); 0xFFFF when unknown.
u16       kana_kanji_word_freq(const kana_kanji_db* _db, u32 _word);
// Word _word's meaning in English, whatever the language (search).
const c8* kana_kanji_word_meaning_english(const kana_kanji_db* _db, u32 _word);

// UTF-8 of a code point into _out (at least 5 bytes), NUL-terminated.
void kana_kanji_utf8(u32 _codepoint, c8* _out);

// The next code point of a UTF-8 string, advancing *_s past it. 0 at the end (or
// on a malformed byte, which is skipped).
u32  kana_kanji_utf8_next(const c8** _s);

#endif
