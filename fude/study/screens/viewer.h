#ifndef FUDE_VIEWER
#define FUDE_VIEWER

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/glyph.h"
#include "drawing/widgets/scroll.h"
#include "study/models/vocab.h"

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
// word card (wordcard.h) — a word typed in.
//
// It walks a LIST it is given — Browse's results — so Prev/Next stay inside
// whatever was filtered, sorted or searched. Its buttons are its rows (row.h),
// declared at the end of viewer.c.
//
// Portrait stacks the character over its details; a wide screen puts the
// details beside it, so the character stays big.
// ===========================================================================

#define FUDE_VIEWER_WORD_KANJI  4u    // kanji a word can bring to practice
#define FUDE_VIEWER_ROWS        64u   // words listed at most (the learner's and the character's)
#define FUDE_VIEWER_SENTENCES   24u   // words whose example sentences can be gone through
#define FUDE_VIEWER_SENTENCE_WORDS 8u  // a sentence's words offered (wordsplit.h), at most
#define FUDE_VIEWER_SIMILAR     6u    // look-alikes shown, at most

typedef enum {
    FUDE_VIEWER_ROW_EXAMPLE = 0,   // one of the character's examples
    FUDE_VIEWER_ROW_YOURS,         // one of the learner's words
    FUDE_VIEWER_ROW_TO_ADD,        // Add: a further JMdict word (ticked when it is the learner's)
    FUDE_VIEWER_ROW_TYPED          // Add: a word the learner typed in (ticked; untick takes it off)
} FUDE_VIEWER_ROW_;

// A word as listed (its text copied: the learner's words can change under it).
RDE_STRUCT {
    u8 kind;       // FUDE_VIEWER_ROW_
    b8 ticked;
    c8 written[FUDE_USERWORD_WRITTEN];
    c8 reading[FUDE_USERWORD_READING];
    c8 meaning[FUDE_USERWORD_MEANING];
} fude_viewer_row;

// What a press is, once it has moved: not yet known, a swipe, or anything else.
typedef enum { FUDE_VIEWER_SWIPE_UNKNOWN = 0, FUDE_VIEWER_SWIPE_YES, FUDE_VIEWER_SWIPE_NO } FUDE_VIEWER_SWIPE_;

RDE_STRUCT {
    const fude_kanji_db* db;
    fude_glyph           glyph;
    b8                   open;
    rde_arr TYPE(u32)    list;        // record indices being walked
    u32                  position;    // into list
    f64                  started;     // when the current character began writing (engine clock)

    // The words: listed for the character on screen, scrolled in their area.
    b8                   adding;      // Add: the further words to pick from
    fude_viewer_row      rows[FUDE_VIEWER_ROWS];
    u32                  row_count;
    u32                  rows_for;    // the record they were listed for (UINT32_MAX: none)
    b8                   rows_adding; // ...and in which mode
    u32                  rows_words;  // fude_vocab_revision they were listed at
    fude_scroller        scroll;      // the words: their scrolling and their taps
    // A swipe across the screen: to the next character (to the left) or the one
    // before. A press is told one when it first moves as far as a drag, mostly
    // sideways; from then on it presses and scrolls nothing.
    u8                   swipe;       // FUDE_VIEWER_SWIPE_
    rde_vec_2F           swipe_from;
    rde_vec_2F           swipe_at;
    f64                  swipe_time;
    b8                   in_list;     // the press began in the list
    i32                  pressed;     // the row under a press still a tap (-1: none)
    rde_vec_2F           list_min;    // the list's area, as laid out last frame (screen)
    rde_vec_2F           list_max;
    f32                  row_h;
    b8                   add_pressed; // the press began on Add
    rde_vec_2F           add_min;     // Add, as laid out last frame (none: min = max)
    rde_vec_2F           add_max;
    c8                   _tapped[FUDE_USERWORD_WRITTEN];   // a word tapped, for fude_viewer_take_word ("": none)
    // Read aloud (speech.h, where there is a voice): a tap on a word's reading
    // says it; one on the 音 / 訓 lines, the character's readings.
    f32                  reading_x0;  // the readings' column in the rows (screen, last frame)
    f32                  reading_x1;
    f32                  save_x0;     // the rows' bookmark, from here to their right end: the word card (wordcard.h)
    rde_vec_2F           readings_min;   // the 音 / 訓 lines (none: min = max)
    rde_vec_2F           readings_max;
    b8                   readings_pressed;
    // The example sentence: the character's words that have one, the one shown.
    u32                  sentences[FUDE_VIEWER_SENTENCES];   // word numbers (kanji.h)
    u32                  sentence_count;
    u32                  sentence_at;
    u32                  sentences_for;     // the record they were found for (UINT32_MAX: none)
    u32                  sentences_words;   // ...and fude_vocab_revision then
    u32                  sentence_words[FUDE_VIEWER_SENTENCES][FUDE_VIEWER_SENTENCE_WORDS];   // each one's words (word numbers)
    u8                   sentence_word_n[FUDE_VIEWER_SENTENCES];
    rde_vec_2F           word_min[FUDE_VIEWER_SENTENCE_WORDS];   // the shown one's words, as laid out last frame
    rde_vec_2F           word_max[FUDE_VIEWER_SENTENCE_WORDS];
    u32                  word_shown;
    f32                  sentence_room;     // stacked: the tallest sentence card shown for this character (it only grows)
    // Look-alikes (似): code points, the learner's mix-ups first.
    u32                  similar[FUDE_VIEWER_SIMILAR];
    u32                  similar_count;
    u32                  similar_mine;      // the first these many: the learner's own mix-ups
    u32                  similar_for;       // the record they were found for (UINT32_MAX: none)
    u32                  similar_log;       // ...and fude_examlog_revision then
    rde_vec_2F           similar_min[FUDE_VIEWER_SIMILAR];   // as laid out last frame
    rde_vec_2F           similar_max[FUDE_VIEWER_SIMILAR];
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
} fude_viewer;

void fude_viewer_init(fude_viewer* _viewer, const fude_kanji_db* _db);
void fude_viewer_destroy(fude_viewer* _viewer);

// There is character data to show.
b8   fude_viewer_available(const fude_viewer* _viewer);

// Opens on _records[_position], walking _records (copied).
void fude_viewer_show(fude_viewer* _viewer, const u32* _records, u32 _count, u32 _position);
// Opens on one character by code point, alone. False when the data has none.
b8   fude_viewer_show_codepoint(fude_viewer* _viewer, u32 _codepoint);
void fude_viewer_close(fude_viewer* _viewer);
void fude_viewer_next(fude_viewer* _viewer);
void fude_viewer_prev(fude_viewer* _viewer);
void fude_viewer_replay(fude_viewer* _viewer);
// Visits record _record: put in the list right after the character on screen, and
// shown (Prev comes back; Next goes on with the list).
void fude_viewer_visit(fude_viewer* _viewer, u32 _record);

// A pointer on the viewer (screen space, centre origin, Y up): the words scroll
// and are tapped, Add is tapped; a swipe across turns to the next character
// (swiped to the left) or the one before (to the right).
void fude_viewer_pointer_down(fude_viewer* _viewer, rde_vec_2F _screen, f64 _time);
void fude_viewer_pointer_moved(fude_viewer* _viewer, rde_vec_2F _screen, f64 _time);
void fude_viewer_pointer_up(fude_viewer* _viewer, f64 _time);
// Once a frame: the list scrolls; a tap on a word — listed: noted for
// fude_viewer_take_word; in Add: ticked or unticked (the learner's words change).
void fude_viewer_update(fude_viewer* _viewer, f32 _dt);
// A word was tapped: its kanji as records, each once, in the order written (at
// most _max); how many — 0 when no word was tapped. Once per tap.
u32  fude_viewer_take_word(fude_viewer* _viewer, u32* _out, u32 _max);
// Add, on or off (its row follows).
void fude_viewer_set_adding(fude_viewer* _viewer, b8 _adding);
// The character on screen, its code point (0: none).
u32  fude_viewer_codepoint(const fude_viewer* _viewer);

// Draws the whole viewer. Inside a 2D drawing block; screen space is centre
// origin, Y up. _font is drawn at sizes given in screen units, scaled from the
// size it was loaded at, _font_px. _bottom_bar: the height kept free for the
// buttons (above the safe area).
void fude_viewer_render(fude_viewer* _viewer, rde_window* _window, rde_font* _font, f32 _font_px, f32 _bottom_bar);

// The screen (screen.h): its rows of buttons, and what they do.
extern const struct fude_screen FUDE_VIEWER_SCREEN;

#endif
