// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_WORDCARD
#define FUDE_WORDCARD

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "study/chars/kanji.h"
#include "study/models/vocab.h"

// ===========================================================================
// Word card: where a word goes into the learner's vocabulary (vocab.h), and where a saved one is changed or taken
// out. It is the same card wherever the word comes from: a character's examples, an example sentence's words, a
// photo's or a translation's words, what the lasso read, or a word typed in. It holds:
//
//   the word, its reading and its meaning, in fields to change (a reading typed in romaji becomes hiragana; one left
//   empty is looked up in the app's dictionary); Translate with Google, where ML Kit's translator is (translate.h),
//   which turns the meaning, typed in the reader's language, into Japanese for the word (the dictionary's word, in
//   its dictionary form, when it is one); the words found in what was read, when it was more than one (a tap puts
//   one in the fields); the sentence the word was met in, and its translation, kept with it (vocab.h); the learner's
//   lists, ticked (a new one can be named on the spot); and Remove (for a saved word), Cancel and Save.
//
// A new word comes with the lists the last one went in already ticked, so saving a lesson's words one after
// another takes a tap each.
//
// Its LIST form edits a list's name, to make a new list or rename one, and has Delete list (its words stay in the
// vocabulary). Its NOTE form edits the learner's note on a character (charnote.h), written freely, and has Delete
// note.
//
// Where it sits: the study layer's widgets (study/widgets). It is built with the kit (kit.h) on the UI's canvas,
// over everything, with the keyboard under it, by the study's UI hooks (study.h: fude_study_ui_build and the rest).
// Screens drawn by hand (the viewer, Text from a photo, the page's cards) ASK for it, and the UI opens it on its
// next update (ui.h).
// Its main types and their lifecycle: the one fude_wordcard lives in the fude_study (its word field). It is built,
// closed, by fude_wordcard_create, and built again whenever the UI is (when the language changes), which closes an
// open card; its widgets belong to the UI's canvas. It has a chip for every list there is, made again when the number
// of lists changes; their array is the card's, let go by fude_wordcard_forget before the canvas goes. What was asked
// for, and the lists the last word went in, are statics of wordcard.c, so they survive a rebuild; the lists the last
// word went in are an array made on first use, which lasts as long as the app.
// Threads: everything runs on the app's main thread, including the fude_wordcard_ask calls. The translation is
// asynchronous: the card takes its answer once a frame.
// Files: none of its own; Save writes through the vocabulary and the character notes (vocab.h, charnote.h).
// ===========================================================================

struct fude_ui;

#define FUDE_WORDCARD_FOUND 8u   // the most words found in what was read that are offered

RDE_STRUCT {
    c8 written[FUDE_USERWORD_WRITTEN];
    c8 reading[FUDE_USERWORD_READING];
    c8 meaning[FUDE_USERWORD_MEANING];
} fude_wordcard_word;

// What a found word's chip's callback is handed: the UI, and which chip it is.
RDE_STRUCT {
    struct fude_ui* ui;
    u32                  index;
} fude_wordcard_ref;

// A list as the card shows it: its chip, and whether the word goes in it on Save.
RDE_STRUCT {
    u32            list;     // the list's id (vocab.h)
    b8             ticked;   // the word goes in it on Save
    rde_ui_button* chip;     // its chip, on the card (the canvas owns it)
} fude_wordcard_list;

RDE_STRUCT {
    b8                  open;
    u32                 id;                        // the saved word shown (0: one not saved yet)
    u32                 kanji;                     // the character page it comes from (0: none)
    // The lists as shown, in the order made, a chip each: as many as there are lists (wordcard.c:
    // fude_wordcard_list_chips makes the chips again when that changes).
    rde_arr TYPE(fude_wordcard_list) lists;
    b8                  naming;                    // a new list's name is being written
    b8                  list_mode;                 // the list form, on list_id's name (0: a new list)
    u32                 list_id;
    b8                  note_mode;                 // the note form, on note_cp's note
    u32                 note_cp;
    rde_ui_text_editor* note_field;
    fude_wordcard_word  found[FUDE_WORDCARD_FOUND];
    u32                 found_count;
    fude_wordcard_ref   refs[FUDE_WORDCARD_FOUND];   // the found words' chips' callbacks' data
    c8                  sentence[FUDE_VOCAB_SENTENCE];      // the sentence it was met in ("": none), kept on Save
    c8                  sentence_to[FUDE_VOCAB_SENTENCE];   // ...and its translation
    // The meaning into Japanese (Translate with Google): how far it has got, the meaning sent (and its language),
    // and the answer's ticket.
    u8                  translating;               // FUDE_WORDCARD_TRANSLATE_ (wordcard.c)
    u32                 translate_says;            // the status's text (FUDE_TEXT_; FUDE_TEXT_COUNT for none)
    b8                  translate_prepared;        // the models have been asked for since the card opened
    c8                  translate_from[FUDE_USERWORD_MEANING];
    const c8*           translate_lang;
    u32                 ticket;

    fude_kit_modal      modal;                     // the card over its backdrop (a tap on the backdrop cancels)
    rde_ui_label*       title;
    rde_ui_text_editor* fields[3];                 // written, reading, meaning
    rde_ui_button*      translate;                 // Translate with Google
    rde_ui_label*       translate_status;
    rde_ui_image*       badge;                     // Google's, shown by a translation once it is in
    rde_ui_label*       sentence_caption;
    rde_ui_label*       sentence_label;
    rde_ui_label*       sentence_to_label;
    rde_ui_label*       found_caption;
    rde_ui_button*      found_chips[FUDE_WORDCARD_FOUND];
    rde_ui_label*       lists_caption;
    rde_ui_button*      new_list;
    rde_ui_text_editor* list_field;
    rde_ui_button*      list_ok;
    rde_ui_label*       error;
    rde_ui_button*      remove;
    rde_ui_button*      cancel;
    rde_ui_button*      save;
    b8                  _layout;                   // it must be laid out again (opened, a list made, the screen turned)
    rde_vec_2F          _laid_out;                 // the screen size it was laid out for
} fude_wordcard;

// --- asking for it (from any screen; it opens on the UI's next update) ------------------
// Each ask replaces the one before it that has not opened yet. The strings are copied.

// Asks for a word. When it is saved already, the card shows it as saved (its own texts, its lists); otherwise it
// shows these texts, to save. _kanji is the character page it comes from (0: none).
void fude_wordcard_ask(const c8* _written, const c8* _reading, const c8* _meaning, u32 _kanji);
// Asks for a word met in a sentence (_sentence, in the language taught) with its translation: as fude_wordcard_ask,
// and the sentence is kept with the word when it is saved (a saved word keeps the one it already has).
void fude_wordcard_ask_in(const c8* _written, const c8* _reading, const c8* _meaning, const c8* _sentence, const c8* _translation);
// Asks for a saved word, by its id. Nothing opens if it has gone since.
void fude_wordcard_ask_saved(u32 _id);
// Asks for empty fields, to type a word in. _kanji is the character page it is typed on (0: none).
void fude_wordcard_ask_typed(u32 _kanji);
// Asks for what the lasso read (_text). When it holds one word of the app's dictionary, the card shows that word,
// as the dictionary writes it (食べました is 食べる); otherwise it shows the text, and offers the words found in it.
void fude_wordcard_ask_read(const fude_kanji_db* _db, const c8* _text);
// Asks for the list form: list _list, to rename or delete; with 0, a new list to name.
void fude_wordcard_ask_list(u32 _list);
// Returns the id of a list the list form has made since the last call, once; 0 when none was made.
u32  fude_wordcard_take_new_list(void);
// Makes new words come with list _list ticked (the list the Vocabulary screen shows); 0 for none.
void fude_wordcard_prefer_list(u32 _list);
// Asks for the note form, on character _codepoint's note.
void fude_wordcard_ask_note(u32 _codepoint);

// --- the UI's (ui.h) ---------------------------------------------------------------------

// Builds the card's widgets, closed, under _root (the UI's canvas), clearing whatever the card held.
void fude_wordcard_create(struct fude_ui* _ui, rde_ui_node* _root);
// Lets go of the card's own memory (its list chips' array) before the UI's canvas goes, which frees the widgets
// themselves. The card must be built again (fude_wordcard_create) before it is used. Safe on a card never built.
void fude_wordcard_forget(struct fude_ui* _ui);
// Once a frame: opens what was asked for, takes a translation's next step, and lays the card out again when the
// screen has changed.
void fude_wordcard_update(struct fude_ui* _ui);
// Closes the card without saving.
void fude_wordcard_close(struct fude_ui* _ui);
// Translate with Google, pressed: sends the meaning typed (in the reader's language) to be translated into the
// language taught for the written field. Nothing happens while the card is closed or a translation is on its way.
void fude_wordcard_translate(struct fude_ui* _ui);
// Restyles the card for the active theme.
void fude_wordcard_apply_theme(struct fude_ui* _ui);

#endif
