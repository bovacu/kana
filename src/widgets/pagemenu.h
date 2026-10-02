#ifndef KANA_PAGEMENU
#define KANA_PAGEMENU

#include "rde.h"
#include "widgets/row.h"
#include "handwriting/textink.h"
#include "services/translate.h"
#include "lang/ja/wordsplit.h"
#include "ink/lasso.h"

// ===========================================================================
// The page's menus, rows (row.h) floating over it:
//
//   over a lasso selection: Cut, Copy, Copy as text, Translate with Google,
//   Save word, Duplicate, Check, Delete — just above the selection's box (below
//   it with no room), hidden while it is dragged. Copy as text, Translate and
//   Save word read the strokes first (textink.h): Translate's answer comes in a
//   card by the selection, with the words found in it (+ to save one);
//
//   at a long press (right-click on a computer): Paste, Paste text, Text from a
//   photo, Select all — just over the finger.
//
// Its state (a reading under way, the card) lives on when the language changes
// and the widgets are built again (kana_pagemenu_build).
// ===========================================================================

struct kana_app;

// Translate with Google on a selection: where it is.
typedef enum {
    KANA_PAGEMENU_CARD_NONE = 0,   // no card
    KANA_PAGEMENU_CARD_READING,    // the selection being read
    KANA_PAGEMENU_CARD_GETTING,    // its models asked for / on their way
    KANA_PAGEMENU_CARD_ASKED,      // the translator has it
    KANA_PAGEMENU_CARD_DONE,       // translation: in (empty: it could not be)
    KANA_PAGEMENU_CARD_FAILED      // the models could not be downloaded
} KANA_PAGEMENU_CARD_;

// What a reading of the selection is for.
typedef enum {
    KANA_PAGEMENU_READ_NONE = 0,
    KANA_PAGEMENU_READ_COPY,        // Copy as text: the clipboard
    KANA_PAGEMENU_READ_TRANSLATE,   // Translate with Google: the card
    KANA_PAGEMENU_READ_VOCAB        // Save word: the word card (wordcard.h)
} KANA_PAGEMENU_READ_;

typedef struct kana_pagemenu {
    struct kana_app*    app;
    kana_row            selection;        // over the lasso's selection
    kana_row            context;          // the page's, at a long press
    kana_row_face       context_faces[KANA_ROW_BUTTONS];   // as it opened (what can be pasted then)
    rde_vec_2F          context_canvas;   // where it was opened, on the page: where Paste lands

    u32                 copied;           // the button saying "Copied" (KANA_ROW_NONE: none)...
    f64                 copied_until;     // ...until then (engine clock)
    kana_textink_reader reader;           // the selection being read (textink.h)
    b8                  _reader_ready;
    u8                  reading;          // KANA_PAGEMENU_READ_: what for (NONE: no reading under way)
    kana_clip           _text_clip;       // Paste text: the text written as strokes

    // The card: what was read, into the translator (translate.h), shown by the
    // selection until it goes.
    u8                  card;             // KANA_PAGEMENU_CARD_
    u32                 card_ticket;
    u32                 card_of;          // the selection it is for (its strokes, as one number)
    b8                  card_prepared;
    c8                  card_from[256];
    c8                  card_to[KANA_TRANSLATE_TEXT];
    rde_vec_2F          card_min;         // on screen, Kana's screen space (last frame)
    rde_vec_2F          card_max;
    rde_vec_2F          speak_min;        // its speaker (none: min = max)
    rde_vec_2F          speak_max;
    // What was read, as words (wordsplit.h): each a row on the card, tap to save it.
    u32                 words[KANA_WORDSPLIT_MAX];
    u32                 word_count;
    f32                 words_top;        // the rows on screen (last frame)
    f32                 words_left;
    f32                 words_right;
} kana_pagemenu;

void kana_pagemenu_init(kana_pagemenu* _menu, struct kana_app* _app);
void kana_pagemenu_destroy(kana_pagemenu* _menu);
// The rows, built (again) under _root.
void kana_pagemenu_build(kana_pagemenu* _menu, rde_ui_node* _root);
void kana_pagemenu_restyle(kana_pagemenu* _menu);
// Once a frame: the selection's menu placed (or hidden under a screen, _hidden),
// a reading done handed on, the card's next step.
void kana_pagemenu_update(kana_pagemenu* _menu, b8 _hidden);
// The card, once a frame over the page (Kana's screen space).
void kana_pagemenu_render(kana_pagemenu* _menu, rde_window* _window);
// Is _screen (Kana's screen space) on the card? _ui (UI canvas units) on a menu?
b8   kana_pagemenu_hit(const kana_pagemenu* _menu, rde_vec_2F _screen, rde_vec_2F _ui);
// A press at _screen: on the card's speaker, what was read is said aloud; on a
// word's row, the word card. True when it was one of them.
b8   kana_pagemenu_card_press(kana_pagemenu* _menu, rde_vec_2F _screen);

// The context menu for a long press at _screen (Kana's screen space); _canvas is
// the same point on the page, where Paste lands. It floats just above the finger.
// Closed by choosing an item, or by kana_pagemenu_close_context (any other press).
void kana_pagemenu_open_context(kana_pagemenu* _menu, rde_vec_2F _screen, rde_vec_2F _canvas);
void kana_pagemenu_close_context(kana_pagemenu* _menu);

// What its buttons do, for a look flag too: the selection translated, or read
// for the word card.
void kana_pagemenu_translate(kana_pagemenu* _menu);
void kana_pagemenu_save_word(kana_pagemenu* _menu);
// _text (UTF-8) written in the characters' own strokes with the brush
// (textink.h), centred at _canvas, selected (Paste text's work). What could not
// be written is said in a notice.
void kana_pagemenu_write_text(kana_pagemenu* _menu, const c8* _text, rde_vec_2F _canvas);

#endif
