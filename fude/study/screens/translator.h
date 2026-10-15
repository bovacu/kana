// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_TRANSLATOR
#define FUDE_TRANSLATOR

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/glyph.h"
#include "study/services/translate.h"
#include "drawing/widgets/scroll.h"
#include "study/chars/wordsplit.h"

// ===========================================================================
// Translator ("Into Japanese" in Kana: the Vocabulary's Translate with Google): a word or a sentence typed in the
// reader's language, in the field at the top right, and its translation into the language taught, by Google ML Kit's
// translator on the device (translate.h). Whether a translation is right is for the reader to judge; what the screen
// gives is what to do with it:
//
//   each translation is a card, newest first: what was typed, its translation (with a speaker that says it), and the
//   dictionary's words found in it (wordsplit.h), a row each. A tap on a word opens the word card, to save it (its
//   bookmark is filled once saved). A tap elsewhere on a card chooses it for the row: Save (the whole translation as
//   one word of the vocabulary, through the word card, its meaning what was typed), Write (on the page, in the
//   characters' own strokes) and Practice (its characters, as a set).
//
// Where it sits: a study screen (study/screens), opened by the Vocabulary screen's Translate with Google
// (vocabview.h). The study shell (study/app/shell.c) owns the one translator and places FUDE_TRANSLATOR_SCREEN in the
// app's screen table.
// Its main types and their lifecycle: fude_translator, made with fude_translator_init over the character data and
// freed with fude_translator_destroy when the app ends. Its cards live in it, so the translations of a visit are kept
// until the app closes.
// Threads: main thread only. Translations are asynchronous: asked for, then taken once a frame (translate.h).
// Files: none of its own. ML Kit downloads its language models the first time (about 30 MB a language).
// Design: Google's badge sits by the translations, as Google's terms ask (translate.h).
// ===========================================================================

#define FUDE_TRANSLATOR_CARDS 16u    // How many translations are kept, newest first.
#define FUDE_TRANSLATOR_INPUT 256u   // The most bytes that can be typed.

typedef enum {
    FUDE_TRANSLATOR_WAITING = 0,   // Its models are on their way (or asked for).
    FUDE_TRANSLATOR_ASKED,         // The translator has it.
    FUDE_TRANSLATOR_DONE,          // The translation is in (empty when it could not be translated).
    FUDE_TRANSLATOR_FAILED         // The models could not be downloaded.
} FUDE_TRANSLATOR_;

// One translation.
RDE_STRUCT {
    c8         from[FUDE_TRANSLATOR_INPUT];     // What was typed.
    c8         taught[FUDE_TRANSLATE_TEXT];   // Its translation, in the language taught.
    u8         state;                           // A FUDE_TRANSLATOR_.
    u32        ticket;
    u32        words[FUDE_WORDSPLIT_MAX];       // The dictionary's words found in it.
    u32        word_count;
    // As drawn last frame (screen space), for taps.
    rde_vec_2F min, max;
    rde_vec_2F speak_min, speak_max;
    f32        words_top;                       // The top of the first word's row.
} fude_translator_card;

RDE_STRUCT {
    const fude_kanji_db* db;
    fude_glyph           glyph;          // For the header's badge.
    b8                   open;
    fude_translator_card cards[FUDE_TRANSLATOR_CARDS];
    u32                  count;
    u32                  chosen;         // The card the row acts on (0 is the newest).
    b8                   prepared;       // The models were asked for on this visit.
    const c8*            from;           // The language typed in ("en"...), as the visit began.

    fude_scroller        scroller;
    rde_vec_2F           list_min;       // The cards' area, last frame.
    rde_vec_2F           list_max;
    f32                  content_h;
} fude_translator;

// Makes the translator over the character data (for the words found in a translation); destroy frees it.
void fude_translator_init(fude_translator* _tr, const fude_kanji_db* _db);
void fude_translator_destroy(fude_translator* _tr);
// Opens it, asking again for the models if the last visit's download failed; closes it. The cards stay.
void fude_translator_open(fude_translator* _tr);
void fude_translator_close(fude_translator* _tr);
// Translates _text (UTF-8, in the reader's language) into the language taught: a new card on top, the oldest let go
// when there is no room. An empty text does nothing; with ML Kit switched off, a notice says so and nothing is asked.
void fude_translator_ask(fude_translator* _tr, const c8* _text);
// The card chosen, once its translation is in; NULL otherwise. The pointer is into the translator and stays valid
// until the next fude_translator_ask.
const fude_translator_card* fude_translator_chosen(const fude_translator* _tr);

// One pointer, in screen space: a drag scrolls the cards; a tap is taken in fude_translator_update (a card's speaker
// says it, a word opens the word card, anywhere else chooses the card).
void fude_translator_pointer_down(fude_translator* _tr, rde_vec_2F _screen, f64 _time);
void fude_translator_pointer_moved(fude_translator* _tr, rde_vec_2F _screen, f64 _time);
void fude_translator_pointer_up(fude_translator* _tr, f64 _time);
// Once a frame while open: asks for the models, sends the translations waiting and takes their answers, scrolls, and
// takes taps. fude_translator_render draws the cards between _top and _bottom (screen y), in a 2D drawing block.
void fude_translator_update(fude_translator* _tr, f32 _dt);
void fude_translator_render(fude_translator* _tr, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The translator as a screen (screen.h). Its field takes the text to translate, asked when Return is pressed. Its
// row: Back, Save, Write and Practice, the last three acting on the card chosen once its translation is in.
extern const struct fude_screen FUDE_TRANSLATOR_SCREEN;

#endif
