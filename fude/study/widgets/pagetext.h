// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_PAGETEXT_H
#define FUDE_PAGETEXT_H

#include "rde.h"
#include "drawing/widgets/row.h"
#include "drawing/ink/lasso.h"
#include "study/handwriting/textink.h"
#include "study/services/translate.h"
#include "study/chars/wordsplit.h"

// ===========================================================================
// Page text: the page read as text. These are the study layer's buttons in the page's menus (pagemenu.h), and what
// they leave over the page.
//
//   Over a lasso selection: Copy as text, Translate with Google, Save word and Check. Copy as text, Translate and
//   Save word first read the strokes (textink.h). Translate's answer comes in a card by the selection, with the
//   words found in it (tap one to save it) and a speaker that reads it aloud.
//
//   At a long press: Paste text (the clipboard's text, written in the characters' own strokes) and Text from a
//   photo.
//
//   Over an area of a PDF's text: Copy as text, Translate and Save word, which take the PDF's own text (or a scanned
//   page's lines, once they are read) instead of reading handwriting.
//
// Where it sits: the study layer's widgets (study/widgets). A study app's extension (study.h: FUDE_STUDY_EXTENSION)
// gives the page's menus these rows (FUDE_PAGETEXT_SELECTION_ROW, FUDE_PAGETEXT_CONTEXT_ROW, FUDE_PAGETEXT_TEXT_ROW),
// which put these buttons between the core's own; the study's hooks (study.h) update and draw it each frame and
// hand it the presses on the page.
// Its main types and their lifecycle: one fude_pagetext lives in the fude_study (its text field), set up by
// fude_study_init and let go by fude_study_destroy. Its state (a reading under way, the card) is the study's, so it
// lives on when the UI is built again. Its handwriting reader is made the first time a selection is read.
// Threads: everything runs on the app's main thread. Readings and translations are asynchronous: a button starts
// one, and fude_pagetext_update takes its answer once a frame.
// Files: none. Copy as text writes to the system clipboard, and Paste text reads from it.
// Design: the card belongs to the selection it was read from, known by a key (its strokes hashed into one number):
// a new lasso, or strokes added or taken away, give another key, and the card goes.
// ===========================================================================

struct fude_app;

// How far Translate with Google has got on a selection.
typedef enum {
    FUDE_PAGETEXT_CARD_NONE = 0,   // no card
    FUDE_PAGETEXT_CARD_READING,    // the selection is being read
    FUDE_PAGETEXT_CARD_GETTING,    // its models have been asked for, or are on their way
    FUDE_PAGETEXT_CARD_ASKED,      // the translator has the text
    FUDE_PAGETEXT_CARD_DONE,       // the translation is in (empty when there could be none)
    FUDE_PAGETEXT_CARD_FAILED      // the models could not be downloaded
} FUDE_PAGETEXT_CARD_;

// What a reading of the selection is for.
typedef enum {
    FUDE_PAGETEXT_READ_NONE = 0,
    FUDE_PAGETEXT_READ_COPY,        // Copy as text: the text goes to the clipboard
    FUDE_PAGETEXT_READ_TRANSLATE,   // Translate with Google: the text goes to the card
    FUDE_PAGETEXT_READ_VOCAB        // Save word: the text goes to the word card (wordcard.h)
} FUDE_PAGETEXT_READ_;

typedef struct {
    struct fude_app*    app;
    fude_textink_reader reader;           // reads the selection's handwriting (textink.h)
    b8                  _reader_ready;
    u8                  reading;          // FUDE_PAGETEXT_READ_: what a reading is for (NONE: none under way)
    // An area of a PDF's text that the lasso took (lasso.h). Its text is read from the PDF at once, and handed on as
    // a finished reading is, at the next update.
    c8                  area_text[FUDE_TEXTINK_TEXT];
    b8                  area_ready;
    b8                  area_waiting;     // its page is still being read (a scan's), so it is asked again each frame
    rde_vec_2F          area_min, area_max;
    fude_clip           _text_clip;       // Paste text: the text written as strokes

    // The card: what was read, sent to the translator (translate.h), and shown by the selection until it goes.
    u8                  card;             // FUDE_PAGETEXT_CARD_
    u32                 card_ticket;
    u32                 card_of;          // the key of the selection it is for (its strokes, as one number)
    b8                  card_prepared;
    c8                  card_from[256];
    c8                  card_to[FUDE_TRANSLATE_TEXT];
    rde_vec_2F          card_min;         // where it was drawn last frame, in the app's screen space
    rde_vec_2F          card_max;
    rde_vec_2F          speak_min;        // its speaker (min = max when there is none)
    rde_vec_2F          speak_max;
    // What was read, as words (wordsplit.h): each is a row on the card, and a tap on it saves it.
    u32                 words[FUDE_WORDSPLIT_MAX];
    u32                 word_count;
    f32                 words_top;        // where the rows were drawn last frame
    f32                 words_left;
    f32                 words_right;
} fude_pagetext;

// The study app's rows for the page's menus (extension.h): the core's buttons and these, each in its place. Over a
// selection: Cut, Copy, Copy as text, Translate, Save word, Duplicate, Check, Delete.
extern const fude_row_def FUDE_PAGETEXT_SELECTION_ROW;   // over a lasso selection
extern const fude_row_def FUDE_PAGETEXT_CONTEXT_ROW;     // Paste, Paste text, Text from a photo, Select all
extern const fude_row_def FUDE_PAGETEXT_TEXT_ROW;        // over a PDF's text: Copy as text, Translate, Save word

// Sets _text up for _app, with nothing being read and no card; and lets go of what it holds (its reader, the
// strokes Paste text wrote).
void fude_pagetext_init(fude_pagetext* _text, struct fude_app* _app);
void fude_pagetext_destroy(fude_pagetext* _text);
// Once a frame, before the menus show (extension.h: menu_update): hands on a reading that is done (to the clipboard,
// the card or the word card), and takes the card one step further (asks for the models the first time, sends the
// text, takes the answer). The card goes when its selection changes.
void fude_pagetext_update(fude_pagetext* _text);
// Sets how its buttons show now. Copy as text and Save word read "Reading…" while the selection is being read for
// them. Paste text is greyed out when there is no text to paste, and Text from a photo where no photo can be read.
void fude_pagetext_selection_faces(const fude_pagetext* _text, const fude_row_def* _row, fude_row_face* _faces);
void fude_pagetext_context_faces(const fude_pagetext* _text, const fude_row_def* _row, fude_row_face* _faces);
// Draws the card over the page, once a frame (in the app's screen space), on the selection's other side from its
// menu. Nothing is drawn when there is no card.
void fude_pagetext_render(fude_pagetext* _text, rde_window* _window);
// Returns true when _screen (the app's screen space) is on the card as it was last drawn.
b8   fude_pagetext_hit(const fude_pagetext* _text, rde_vec_2F _screen);
// Handles a press at _screen: on a word's row it opens the word card for that word; on the card's speaker it reads
// aloud what was read. Returns true when the press was on one of them.
b8   fude_pagetext_press(fude_pagetext* _text, rde_vec_2F _screen);

// What its buttons do, also for a developer's look flag. fude_pagetext_translate reads the selection and shows its
// translation in the card (with ML Kit switched off, a notice says so instead); fude_pagetext_save_word reads the
// selection for the word card. Neither does anything when nothing is selected.
void fude_pagetext_translate(fude_pagetext* _text);
void fude_pagetext_save_word(fude_pagetext* _text);
// Writes _utf8 (UTF-8) in the characters' own strokes with the brush (textink.h), centred at _canvas, and leaves it
// selected with the Lasso tool chosen, ready to drag: Paste text's work. What could not be written is said in a
// notice.
void fude_pagetext_write(fude_pagetext* _text, const c8* _utf8, rde_vec_2F _canvas);

#endif
