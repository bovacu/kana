// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_VIEWER
#define FUDE_VIEWER

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/glyph.h"
#include "drawing/widgets/scroll.h"
#include "study/models/vocab.h"

// ===========================================================================
// Viewer: one character at a time, writing itself stroke by stroke from the baked data (kanji.h). It was the first
// thing to hold in the hand, milestone 2 of the Japanese study app's docs/design.md.
//
// The big character and the kana of its readings are drawn from strokes (glyph.h). Other text is in the UI font
// (drawn with Slug, with the app's script font, Noto Sans JP for Japanese, behind it). That includes the words: the
// learner's own first (their vocabulary, vocab.h), then the examples from JMdict (kanji.h), each a row with its
// reading and meaning, scrolling when they do not all fit. Tapping one practises its kanji as a set.
//
// Note (in the header, top right): the learner's own note on the character (charnote.h), written in the word card's
// note form. When there is one, 記 shows it under the details, and a tap on it changes it.
//
// After the parts, 似 lists the characters it is easily mixed up with: first the learner's own mix-ups (an exam that
// read it as another, examlog.h), in the colour of a wrong answer; then those that look like it (kanji.h, 'LOOK'). A
// tap visits one: it goes in the list right after this one, so Prev comes back.
//
// Under the words is an example sentence (from Tatoeba, kanji.h) with its translation: one of the words' sentences,
// the learner's words' first. › shows the next word's sentence, and a tap on the sentence reads it aloud. Under it
// are its words with their readings (wordsplit.h); a tap on one opens the word card (wordcard.h), to save it.
//
// ADD, by the words: the character's further JMdict words to pick from, each with a tick (a tap adds it to the
// learner's words or takes it off), and the word card (wordcard.h) for a word typed in.
//
// It walks a LIST it is given (Browse's results, the chart, an exam's characters), so Prev and Next stay inside
// whatever was filtered, sorted or searched. A portrait screen stacks the character over its details; a wide one
// puts the details beside it, so the character stays big.
//
// Where it sits: a study screen (study/screens). The study's verbs (fude_study_view) open it from Browse, the chart,
// Check, the exams and the statistics. The study shell (study/app/shell.c) owns the one viewer and places
// FUDE_VIEWER_SCREEN in the app's screen table.
// Its main types and their lifecycle: fude_viewer, made with fude_viewer_init over the character data and freed with
// fude_viewer_destroy when the app ends. It copies the list it walks.
// Threads: main thread only.
// Files: none of its own. Through the models it adds and removes the learner's words (vocab.h) and changes study
// marks (marks.h); it reads the notes (charnote.h) and the exam log (examlog.h).
// Design: its buttons are its rows (row.h), declared at the end of viewer.c.
// ===========================================================================

#define FUDE_VIEWER_WORD_KANJI  4u    // The most kanji a tapped word brings to practice.
#define FUDE_VIEWER_SENTENCES   24u   // The most words whose example sentences can be gone through.
#define FUDE_VIEWER_SENTENCE_WORDS 8u  // The most of a sentence's words offered (wordsplit.h).
#define FUDE_VIEWER_SIMILAR     6u    // The most look-alikes shown.

typedef enum {
    FUDE_VIEWER_ROW_EXAMPLE = 0,   // One of the character's examples.
    FUDE_VIEWER_ROW_YOURS,         // One of the learner's words.
    FUDE_VIEWER_ROW_TO_ADD,        // In Add: a further JMdict word (ticked when it is the learner's).
    FUDE_VIEWER_ROW_TYPED          // In Add: a word the learner typed in (ticked; unticking takes it off).
} FUDE_VIEWER_ROW_;

// A word as listed. Its text is copied, because the learner's words can change under it.
RDE_STRUCT {
    u8 kind;       // A FUDE_VIEWER_ROW_.
    b8 ticked;
    c8 written[FUDE_USERWORD_WRITTEN];
    c8 reading[FUDE_USERWORD_READING];
    c8 meaning[FUDE_USERWORD_MEANING];
} fude_viewer_row;

// What a press is, once it has moved: not known yet, a swipe, or anything else.
typedef enum { FUDE_VIEWER_SWIPE_UNKNOWN = 0, FUDE_VIEWER_SWIPE_YES, FUDE_VIEWER_SWIPE_NO } FUDE_VIEWER_SWIPE_;

RDE_STRUCT {
    const fude_kanji_db* db;
    fude_glyph           glyph;
    b8                   open;
    rde_arr TYPE(u32)    list;        // The record indices being walked.
    u32                  position;    // The one shown, an index into list.
    f64                  started;     // When the current character began writing (the engine's clock).

    // The words: listed for the character on screen, and scrolled in their area.
    b8                   adding;      // Add: the further words to pick from.
    rde_arr TYPE(fude_viewer_row) rows;   // Every word listed, the learner's however many, then the character's.
    u32                  rows_for;    // The record they were listed for (UINT32_MAX: none).
    b8                   rows_adding; // And in which mode.
    u32                  rows_words;  // The fude_vocab_revision they were listed at.
    u32                  rows_listed; // Goes up each time they are listed again.
    // The words' columns: the widths of the widest written form and reading, kept because measuring every row's text
    // every frame grows with the learner's words. Measured again when the words are listed again (rows_listed), the
    // language changes (fude_text_revision) or another font or size draws them.
    f32                  written_w;
    f32                  reading_w;
    u32                  widths_listed;   // The rows_listed they were measured at (0: never; it is 1 once words are listed).
    u32                  widths_text;     // The fude_text_revision then.
    const rde_font*      widths_font;     // The font and its pixel size then.
    f32                  widths_px;
    fude_scroller        scroll;      // The words' scrolling and their taps.
    // A swipe across the screen goes to the next character (a swipe to the left) or the one before. A press is taken
    // as a swipe when it first moves as far as a drag, mostly sideways; from then on it presses and scrolls nothing.
    u8                   swipe;       // A FUDE_VIEWER_SWIPE_.
    rde_vec_2F           swipe_from;
    rde_vec_2F           swipe_at;
    f64                  swipe_time;
    b8                   in_list;     // The press began in the list.
    i32                  pressed;     // The row under a press that is still a tap (-1: none).
    rde_vec_2F           list_min;    // The list's area, as laid out last frame (screen).
    rde_vec_2F           list_max;
    f32                  row_h;
    b8                   add_pressed; // The press began on Add.
    rde_vec_2F           add_min;     // Add, as laid out last frame (none: min = max).
    rde_vec_2F           add_max;
    c8                   _tapped[FUDE_USERWORD_WRITTEN];   // A word tapped, for fude_viewer_take_word ("": none).
    // Read aloud (speech.h, where there is a voice): a tap on a word's reading says it, and one on the 音 / 訓 lines
    // says the character's readings.
    f32                  reading_x0;  // The readings' column in the rows (screen, last frame).
    f32                  reading_x1;
    f32                  save_x0;     // The rows' bookmark, from here to their right end: opens the word card.
    rde_vec_2F           readings_min;   // The 音 / 訓 lines (none: min = max).
    rde_vec_2F           readings_max;
    b8                   readings_pressed;
    // The example sentence: the character's words that have one, and the one shown.
    u32                  sentences[FUDE_VIEWER_SENTENCES];   // Word numbers (kanji.h).
    u32                  sentence_count;
    u32                  sentence_at;
    u32                  sentences_for;     // The record they were found for (UINT32_MAX: none).
    u32                  sentences_words;   // And the fude_vocab_revision then.
    // Each sentence's words, as word numbers (kanji.h), and how many there are.
    u32                  sentence_words[FUDE_VIEWER_SENTENCES][FUDE_VIEWER_SENTENCE_WORDS];   // Each one's words.
    u8                   sentence_word_n[FUDE_VIEWER_SENTENCES];
    rde_vec_2F           word_min[FUDE_VIEWER_SENTENCE_WORDS];   // The shown sentence's words, as laid out last frame.
    rde_vec_2F           word_max[FUDE_VIEWER_SENTENCE_WORDS];
    u32                  word_shown;
    // Each chip's word in the sentence's list. One chip stands for a word read several ways (人 じん, にん, ひと), and
    // a tap takes the reading saved, else the first.
    u8                   word_of[FUDE_VIEWER_SENTENCE_WORDS];    // Each chip's word.
    // Stacked: the tallest sentence card shown for this character. It only grows, so › does not move the page.
    f32                  sentence_room;     // The room kept.
    // The look-alikes (似), as code points, the learner's mix-ups first.
    u32                  similar[FUDE_VIEWER_SIMILAR];
    u32                  similar_count;
    u32                  similar_mine;      // The first this many are the learner's own mix-ups.
    u32                  similar_for;       // The record they were found for (UINT32_MAX: none).
    u32                  similar_log;       // And the fude_examlog_revision then.
    rde_vec_2F           similar_min[FUDE_VIEWER_SIMILAR];   // As laid out last frame.
    rde_vec_2F           similar_max[FUDE_VIEWER_SIMILAR];
    u32                  similar_shown;
    i32                  similar_pressed;   // -1: none.
    rde_vec_2F           note_min;          // Note, and the note's line (none: min = max).
    rde_vec_2F           note_max;
    rde_vec_2F           note_line_min;
    rde_vec_2F           note_line_max;
    b8                   note_pressed;
    rde_vec_2F           sentence_min;      // The sentence itself: a tap reads it aloud (none: min = max).
    rde_vec_2F           sentence_max;
    rde_vec_2F           next_min;          // ›: the next word's sentence (none: min = max).
    rde_vec_2F           next_max;
    u8                   sentence_pressed;  // 0 none, 1 the sentence, 2 ›, 3 + k its word k.
} fude_viewer;

// Makes the viewer over the character data (_db may be NULL: it is then never available); destroy frees it.
void fude_viewer_init(fude_viewer* _viewer, const fude_kanji_db* _db);
void fude_viewer_destroy(fude_viewer* _viewer);

// Whether there is character data to show.
b8   fude_viewer_available(const fude_viewer* _viewer);

// Opens on _records[_position] (the first when _position is past the end), walking _records, which are copied.
// Nothing happens without data or records.
void fude_viewer_show(fude_viewer* _viewer, const u32* _records, u32 _count, u32 _position);
// Opens on one character, by code point, alone. Returns false when the data does not have it.
b8   fude_viewer_show_codepoint(fude_viewer* _viewer, u32 _codepoint);
// Closes it; goes to the next or the previous character of the list (wrapping round at its ends); and writes the
// character again from its first stroke.
void fude_viewer_close(fude_viewer* _viewer);
void fude_viewer_next(fude_viewer* _viewer);
void fude_viewer_prev(fude_viewer* _viewer);
void fude_viewer_replay(fude_viewer* _viewer);
// Visits record _record: puts it in the list right after the character on screen and shows it, so Prev comes back
// and Next goes on with the list.
void fude_viewer_visit(fude_viewer* _viewer, u32 _record);

// A pointer on the viewer (screen space, centre origin, Y up): the words scroll and are tapped, and Add is tapped. A
// swipe across turns to the next character (swiped to the left) or the one before (to the right).
void fude_viewer_pointer_down(fude_viewer* _viewer, rde_vec_2F _screen, f64 _time);
void fude_viewer_pointer_moved(fude_viewer* _viewer, rde_vec_2F _screen, f64 _time);
void fude_viewer_pointer_up(fude_viewer* _viewer, f64 _time);
// Once a frame while open: the list scrolls, and a tap on a word is acted on. A listed word's bookmark opens the word
// card, its reading is said aloud, and anywhere else it is kept for fude_viewer_take_word. In Add, a tap ticks or
// unticks the word, which adds it to the learner's words or takes it off.
void fude_viewer_update(fude_viewer* _viewer, f32 _dt);
// Whether a word was tapped: its kanji as records, each once, in the order written, at most _max into _out. Returns
// how many, 0 when no word was tapped. Each tap is handed out once.
u32  fude_viewer_take_word(fude_viewer* _viewer, u32* _out, u32 _max);
// Turns Add on or off; the row of buttons follows.
void fude_viewer_set_adding(fude_viewer* _viewer, b8 _adding);
// The code point of the character on screen (0 when none is).
u32  fude_viewer_codepoint(const fude_viewer* _viewer);

// Draws the whole viewer. Call it inside a 2D drawing block; screen space has its origin in the centre, Y up. _font is
// drawn at sizes given in screen units, scaled from the size it was loaded at, _font_px. _bottom_bar is the height
// kept free for the buttons, above the safe area.
void fude_viewer_render(fude_viewer* _viewer, rde_window* _window, rde_font* _font, f32 _font_px, f32 _bottom_bar);

// The viewer as a screen (screen.h). Its rows: Back, Prev, Replay, Next, Study (none, then Studying, then Known, then
// none again; it shows the mark), Sheet and Practice; in Add, Done and Type your own. A tapped word practises its
// kanji as a set. On a keyboard, the arrows go to the previous and next characters and Space replays. Coming back on
// top (from Practice), it writes the character again.
extern const struct fude_screen FUDE_VIEWER_SCREEN;

#endif
