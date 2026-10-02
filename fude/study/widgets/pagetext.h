#ifndef FUDE_PAGETEXT
#define FUDE_PAGETEXT

#include "rde.h"
#include "drawing/widgets/row.h"
#include "drawing/ink/lasso.h"
#include "study/handwriting/textink.h"
#include "study/services/translate.h"
#include "lang/ja/wordsplit.h"

// ===========================================================================
// The page read as text — the study layer's buttons in the page's menus
// (pagemenu.h), and what they leave over the page:
//
//   over a lasso selection: Copy as text, Translate with Google, Save word,
//   Check. Copy as text, Translate and Save word read the strokes first
//   (textink.h): Translate's answer comes in a card by the selection, with the
//   words found in it (tap one to save it), and a speaker that says it;
//
//   at a long press: Paste text (the clipboard's, written in the characters' own
//   strokes), Text from a photo.
//
// A study app's rows (FUDE_PAGETEXT_SELECTION_ROW, FUDE_PAGETEXT_CONTEXT_ROW)
// put these between the core's own. Its state (a reading under way, the card)
// is the study's (study.h), and lives on when the UI is built again.
// ===========================================================================

struct fude_app;

// Translate with Google on a selection: where it is.
typedef enum {
    FUDE_PAGETEXT_CARD_NONE = 0,   // no card
    FUDE_PAGETEXT_CARD_READING,    // the selection being read
    FUDE_PAGETEXT_CARD_GETTING,    // its models asked for / on their way
    FUDE_PAGETEXT_CARD_ASKED,      // the translator has it
    FUDE_PAGETEXT_CARD_DONE,       // translation: in (empty: it could not be)
    FUDE_PAGETEXT_CARD_FAILED      // the models could not be downloaded
} FUDE_PAGETEXT_CARD_;

// What a reading of the selection is for.
typedef enum {
    FUDE_PAGETEXT_READ_NONE = 0,
    FUDE_PAGETEXT_READ_COPY,        // Copy as text: the clipboard
    FUDE_PAGETEXT_READ_TRANSLATE,   // Translate with Google: the card
    FUDE_PAGETEXT_READ_VOCAB        // Save word: the word card (wordcard.h)
} FUDE_PAGETEXT_READ_;

typedef struct {
    struct fude_app*    app;
    fude_textink_reader reader;           // the selection being read (textink.h)
    b8                  _reader_ready;
    u8                  reading;          // FUDE_PAGETEXT_READ_: what for (NONE: no reading under way)
    fude_clip           _text_clip;       // Paste text: the text written as strokes

    // The card: what was read, into the translator (translate.h), shown by the
    // selection until it goes.
    u8                  card;             // FUDE_PAGETEXT_CARD_
    u32                 card_ticket;
    u32                 card_of;          // the selection it is for (its strokes, as one number)
    b8                  card_prepared;
    c8                  card_from[256];
    c8                  card_to[FUDE_TRANSLATE_TEXT];
    rde_vec_2F          card_min;         // on screen, the app's screen space (last frame)
    rde_vec_2F          card_max;
    rde_vec_2F          speak_min;        // its speaker (none: min = max)
    rde_vec_2F          speak_max;
    // What was read, as words (wordsplit.h): each a row on the card, tap to save it.
    u32                 words[FUDE_WORDSPLIT_MAX];
    u32                 word_count;
    f32                 words_top;        // the rows on screen (last frame)
    f32                 words_left;
    f32                 words_right;
} fude_pagetext;

// The study app's rows of the page's menus (extension.h): the core's buttons
// and these, in their places.
extern const fude_row_def FUDE_PAGETEXT_SELECTION_ROW;   // Cut, Copy, Copy as text, Translate, Save word, Duplicate, Check, Delete
extern const fude_row_def FUDE_PAGETEXT_CONTEXT_ROW;     // Paste, Paste text, Text from a photo, Select all

void fude_pagetext_init(fude_pagetext* _text, struct fude_app* _app);
void fude_pagetext_destroy(fude_pagetext* _text);
// Once a frame, before the menus show (extension.h: menu_update): a reading done
// handed on, the card's next step.
void fude_pagetext_update(fude_pagetext* _text);
// How its buttons show now: "Reading…" while the selection is read for one; Paste
// text with no text to paste, Text from a photo where no photo can be read: greyed out.
void fude_pagetext_selection_faces(const fude_pagetext* _text, const fude_row_def* _row, fude_row_face* _faces);
void fude_pagetext_context_faces(const fude_pagetext* _text, const fude_row_def* _row, fude_row_face* _faces);
// The card, once a frame over the page (the app's screen space).
void fude_pagetext_render(fude_pagetext* _text, rde_window* _window);
// Is _screen (the app's screen space) on the card?
b8   fude_pagetext_hit(const fude_pagetext* _text, rde_vec_2F _screen);
// A press at _screen: on the card's speaker, what was read is said aloud; on a
// word's row, the word card. True when it was one of them.
b8   fude_pagetext_press(fude_pagetext* _text, rde_vec_2F _screen);

// What its buttons do, for a look flag too: the selection translated, or read
// for the word card.
void fude_pagetext_translate(fude_pagetext* _text);
void fude_pagetext_save_word(fude_pagetext* _text);
// _text (UTF-8) written in the characters' own strokes with the brush
// (textink.h), centred at _canvas, selected (Paste text's work). What could not
// be written is said in a notice.
void fude_pagetext_write(fude_pagetext* _text, const c8* _utf8, rde_vec_2F _canvas);

#endif
