#ifndef FUDE_TRANSLATOR
#define FUDE_TRANSLATOR

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/glyph.h"
#include "study/services/translate.h"
#include "drawing/widgets/scroll.h"
#include "lang/wordsplit.h"

// ===========================================================================
// Into Japanese (the Vocabulary's Translate with Google): a word or a sentence
// typed in the reader's language — the field at the top right — and its
// Japanese, by Google ML Kit's translator, on the device (translate.h). Whether
// a translation is right is the reader's to judge; what the screen gives is
// what to do with it:
//
//   each translation a card, newest first: what was typed, its Japanese (a
//   speaker says it), and the words of the dictionary found in it (wordsplit.h),
//   a row each — a tap opens the word card, to save it (filled in: saved);
//   a tap on a card chooses it for the row: Save (the whole of it, as a word of
//   the vocabulary: the word card, its meaning what was typed), Write (on the
//   page, in the characters' own strokes), Practice (its characters, as a set).
//
// The models come the first time (about 30 MB a language); the translations of
// a visit are kept until the app closes. Google's badge sits by them.
// ===========================================================================

#define FUDE_TRANSLATOR_CARDS 16u    // translations kept, newest first
#define FUDE_TRANSLATOR_INPUT 256u   // bytes typed, at most

typedef enum {
    FUDE_TRANSLATOR_WAITING = 0,   // its models on their way (or asked for)
    FUDE_TRANSLATOR_ASKED,         // the translator has it
    FUDE_TRANSLATOR_DONE,          // in (empty: it could not be translated)
    FUDE_TRANSLATOR_FAILED         // the models could not be downloaded
} FUDE_TRANSLATOR_;

RDE_STRUCT {
    c8         from[FUDE_TRANSLATOR_INPUT];     // what was typed
    c8         japanese[FUDE_TRANSLATE_TEXT];   // its translation
    u8         state;                           // FUDE_TRANSLATOR_
    u32        ticket;
    u32        words[FUDE_WORDSPLIT_MAX];       // the dictionary's words in it
    u32        word_count;
    // As drawn last frame (screen space), for taps.
    rde_vec_2F min, max;
    rde_vec_2F speak_min, speak_max;
    f32        words_top;                       // the first word row's top
} fude_translator_card;

RDE_STRUCT {
    const fude_kanji_db* db;
    fude_glyph           glyph;          // the header's badge
    b8                   open;
    fude_translator_card cards[FUDE_TRANSLATOR_CARDS];
    u32                  count;
    u32                  chosen;         // the card the row acts on (0: the newest)
    b8                   prepared;       // the models asked for, this visit
    const c8*            from;           // the language typed in ("en"...), as the visit began

    fude_scroller        scroller;
    rde_vec_2F           list_min;       // the cards' area, last frame
    rde_vec_2F           list_max;
    f32                  content_h;
} fude_translator;

void fude_translator_init(fude_translator* _tr, const fude_kanji_db* _db);
void fude_translator_destroy(fude_translator* _tr);
void fude_translator_open(fude_translator* _tr);
void fude_translator_close(fude_translator* _tr);
// _text (UTF-8, the reader's language) translated into Japanese: a new card, on top.
void fude_translator_ask(fude_translator* _tr, const c8* _text);
// The card chosen, when its Japanese is in (NULL: none).
const fude_translator_card* fude_translator_chosen(const fude_translator* _tr);

void fude_translator_pointer_down(fude_translator* _tr, rde_vec_2F _screen, f64 _time);
void fude_translator_pointer_moved(fude_translator* _tr, rde_vec_2F _screen, f64 _time);
void fude_translator_pointer_up(fude_translator* _tr, f64 _time);
// Once a frame: the models asked for, translations sent and taken, scrolling, taps.
void fude_translator_update(fude_translator* _tr, f32 _dt);
void fude_translator_render(fude_translator* _tr, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The screen (screen.h).
extern const struct fude_screen FUDE_TRANSLATOR_SCREEN;

#endif
