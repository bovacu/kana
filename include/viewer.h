#ifndef KANA_VIEWER
#define KANA_VIEWER

#include "rde.h"
#include "kanji.h"
#include "glyph.h"
#include "scroll.h"
#include "vocab.h"

// ===========================================================================
// The character viewer — milestone 2's thing to hold in the hand: one character
// at a time, writing itself stroke by stroke from the baked data (kanji.h).
//
// The big character and the kana of its readings are drawn from strokes
// (glyph.h). Other text is the UI font (Slug, with Noto Sans JP for Japanese):
// the words too — the learner's own (their vocabulary, vocab.h), then the examples from
// JMdict (kanji.h), each a row with its reading and meaning, scrolling when they
// do not all fit; tapping one practises its kanji as a set.
//
// Note (the header's, top right): the learner's own note on it (charnote.h),
// written in the word card's note form; when there is one, 記 shows it under the
// details (a tap: to change it).
//
// After the parts, 似: the characters it is easily mixed up with — first the
// learner's own mix-ups (an exam that read it as another, examlog.h), in the
// colour of a wrong answer; then those that look like it (kanji.h 'LOOK'). A tap
// visits one (it goes in the list right after this one: Prev comes back).
//
// Under the words, an example sentence (Tatoeba's, kanji.h) with its
// translation: one of the words', the learner's first; › shows the next word's,
// and a tap on the sentence reads it aloud. Under it, its words with their
// readings (wordsplit.h): a tap opens the word card (wordcard.h), to save one.
//
// ADD, by the words: the character's further JMdict words to pick from, each
// with a tick (tap: added to the learner's words, or taken off), and — the
// toolbar's form — a word typed in.
//
// It walks a LIST it is given — Browse's results — so Prev/Next stay inside
// whatever was filtered, sorted or searched. The buttons are the toolbar's.
//
// Portrait stacks the character over its details; a wide screen puts the
// details beside it, so the character stays big.
// ===========================================================================

#define KANA_VIEWER_WORD_KANJI  4u    // kanji a word can bring to practice
#define KANA_VIEWER_ROWS        64u   // words listed at most (the learner's and the character's)
#define KANA_VIEWER_SENTENCES   24u   // words whose example sentences can be gone through
#define KANA_VIEWER_SENTENCE_WORDS 8u  // a sentence's words offered (wordsplit.h), at most
#define KANA_VIEWER_SIMILAR     6u    // look-alikes shown, at most

typedef enum {
    KANA_VIEWER_ROW_EXAMPLE = 0,   // one of the character's examples
    KANA_VIEWER_ROW_YOURS,         // one of the learner's words
    KANA_VIEWER_ROW_TO_ADD,        // Add: a further JMdict word (ticked when it is the learner's)
    KANA_VIEWER_ROW_TYPED          // Add: a word the learner typed in (ticked; untick takes it off)
} KANA_VIEWER_ROW_;

// A word as listed (its text copied: the learner's words can change under it).
RDE_STRUCT {
    u8 kind;       // KANA_VIEWER_ROW_
    b8 ticked;
    c8 written[KANA_USERWORD_WRITTEN];
    c8 reading[KANA_USERWORD_READING];
    c8 meaning[KANA_USERWORD_MEANING];
} kana_viewer_row;

RDE_STRUCT {
    const kana_kanji_db* db;
    kana_glyph           glyph;
    b8                   open;
    rde_arr TYPE(u32)    list;        // record indices being walked
    u32                  position;    // into list
    f64                  started;     // when the current character began writing (engine clock)

    // The words: listed for the character on screen, scrolled in their area.
    b8                   adding;      // Add: the further words to pick from
    kana_viewer_row      rows[KANA_VIEWER_ROWS];
    u32                  row_count;
    u32                  rows_for;    // the record they were listed for (UINT32_MAX: none)
    b8                   rows_adding; // ...and in which mode
    u32                  rows_words;  // kana_vocab_revision they were listed at
    kana_scroller        scroll;      // the words: their scrolling and their taps
    b8                   in_list;     // the press began in the list
    i32                  pressed;     // the row under a press still a tap (-1: none)
    rde_vec_2F           list_min;    // the list's area, as laid out last frame (screen)
    rde_vec_2F           list_max;
    f32                  row_h;
    b8                   add_pressed; // the press began on Add
    rde_vec_2F           add_min;     // Add, as laid out last frame (none: min = max)
    rde_vec_2F           add_max;
    c8                   _tapped[KANA_USERWORD_WRITTEN];   // a word tapped, for kana_viewer_take_word ("": none)
    // Read aloud (speech.h, where there is a voice): a tap on a word's reading
    // says it; one on the 音 / 訓 lines, the character's readings.
    f32                  reading_x0;  // the readings' column in the rows (screen, last frame)
    f32                  reading_x1;
    f32                  save_x0;     // the rows' bookmark, from here to their right end: the word card (wordcard.h)
    rde_vec_2F           readings_min;   // the 音 / 訓 lines (none: min = max)
    rde_vec_2F           readings_max;
    b8                   readings_pressed;
    // The example sentence: the character's words that have one, the one shown.
    u32                  sentences[KANA_VIEWER_SENTENCES];   // word numbers (kanji.h)
    u32                  sentence_count;
    u32                  sentence_at;
    u32                  sentences_for;     // the record they were found for (UINT32_MAX: none)
    u32                  sentences_words;   // ...and kana_vocab_revision then
    u32                  sentence_words[KANA_VIEWER_SENTENCES][KANA_VIEWER_SENTENCE_WORDS];   // each one's words (word numbers)
    u8                   sentence_word_n[KANA_VIEWER_SENTENCES];
    rde_vec_2F           word_min[KANA_VIEWER_SENTENCE_WORDS];   // the shown one's words, as laid out last frame
    rde_vec_2F           word_max[KANA_VIEWER_SENTENCE_WORDS];
    u32                  word_shown;
    f32                  sentence_room;     // stacked: the tallest sentence card shown for this character (it only grows)
    // Look-alikes (似): code points, the learner's mix-ups first.
    u32                  similar[KANA_VIEWER_SIMILAR];
    u32                  similar_count;
    u32                  similar_mine;      // the first these many: the learner's own mix-ups
    u32                  similar_for;       // the record they were found for (UINT32_MAX: none)
    u32                  similar_log;       // ...and kana_examlog_revision then
    rde_vec_2F           similar_min[KANA_VIEWER_SIMILAR];   // as laid out last frame
    rde_vec_2F           similar_max[KANA_VIEWER_SIMILAR];
    u32                  similar_shown;
    i32                  similar_pressed;   // -1: none
    rde_vec_2F           note_min;          // Note, and the note's line (none: min = max)
    rde_vec_2F           note_max;
    rde_vec_2F           note_line_min;
    rde_vec_2F           note_line_max;
    b8                   note_pressed;
    rde_vec_2F           sentence_min;      // the Japanese: read aloud (none: min = max)
    rde_vec_2F           sentence_max;
    rde_vec_2F           next_min;          // ›: the next word's (none: min = max)
    rde_vec_2F           next_max;
    u8                   sentence_pressed;  // 0 none, 1 the sentence, 2 ›, 3 + k its word k
} kana_viewer;

void kana_viewer_init(kana_viewer* _viewer, const kana_kanji_db* _db);
void kana_viewer_destroy(kana_viewer* _viewer);

// There is character data to show.
b8   kana_viewer_available(const kana_viewer* _viewer);

// Opens on _records[_position], walking _records (copied).
void kana_viewer_show(kana_viewer* _viewer, const u32* _records, u32 _count, u32 _position);
// Opens on one character by code point, alone. False when the data has none.
b8   kana_viewer_show_codepoint(kana_viewer* _viewer, u32 _codepoint);
void kana_viewer_close(kana_viewer* _viewer);
void kana_viewer_next(kana_viewer* _viewer);
void kana_viewer_prev(kana_viewer* _viewer);
void kana_viewer_replay(kana_viewer* _viewer);
// Visits record _record: put in the list right after the character on screen, and
// shown (Prev comes back; Next goes on with the list).
void kana_viewer_visit(kana_viewer* _viewer, u32 _record);

// A pointer on the viewer (screen space, centre origin, Y up): the words scroll
// and are tapped, Add is tapped.
void kana_viewer_pointer_down(kana_viewer* _viewer, rde_vec_2F _screen, f64 _time);
void kana_viewer_pointer_moved(kana_viewer* _viewer, rde_vec_2F _screen, f64 _time);
void kana_viewer_pointer_up(kana_viewer* _viewer, f64 _time);
// Once a frame: the list scrolls; a tap on a word — listed: noted for
// kana_viewer_take_word; in Add: ticked or unticked (the learner's words change).
void kana_viewer_update(kana_viewer* _viewer, f32 _dt);
// A word was tapped: its kanji as records, each once, in the order written (at
// most _max); how many — 0 when no word was tapped. Once per tap.
u32  kana_viewer_take_word(kana_viewer* _viewer, u32* _out, u32 _max);
// Add, on or off (the toolbar's row follows).
void kana_viewer_set_adding(kana_viewer* _viewer, b8 _adding);
// The character on screen, its code point (0: none).
u32  kana_viewer_codepoint(const kana_viewer* _viewer);

// Draws the whole viewer. Inside a 2D drawing block; screen space is centre
// origin, Y up. _font is drawn at sizes given in screen units, scaled from the
// size it was loaded at, _font_px. _bottom_bar: the height kept free for the
// buttons (above the safe area).
void kana_viewer_render(kana_viewer* _viewer, rde_window* _window, rde_font* _font, f32 _font_px, f32 _bottom_bar);

#endif
