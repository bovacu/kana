#ifndef FUDE_WORDCARD
#define FUDE_WORDCARD

#include "rde.h"
#include "drawing/widgets/kit.h"
#include "study/chars/kanji.h"
#include "study/models/vocab.h"

// ===========================================================================
// The word card: where a word goes into the learner's vocabulary (vocab.h), and
// where a saved one is changed or taken out. The same card wherever a word is —
// a character's examples, an example sentence's words, a photo's or a
// translation's words, what the lasso read, or a word typed in:
//
//   the word, its reading and its meaning, in fields to change (a reading in
//   romaji becomes hiragana; one left empty is looked up in Kana's dictionary);
//   Translate with Google, where ML Kit's translator is (translate.h): the
//   meaning, typed in the reader's language, into Japanese for the word (the
//   dictionary's word, in its form, when it is one); the words found in what was
//   read, when it was more than one (tap: the fields take it); the sentence the
//   word was met in, and its translation, kept with it (vocab.h); the learner's
//   lists, ticked (a new one named on the spot); Remove (a saved word), Cancel,
//   Save.
//
// A new word comes with the lists the last one went in already ticked: saving a
// lesson's words one after another is a tap each.
//
// Its LIST form: a list's name, to make a new list or rename one; Delete list
// (its words stay in the vocabulary). Its NOTE form: the learner's note on a
// character (charnote.h), written freely; Delete note.
//
// Screens drawn by hand (the viewer, Text from a photo, the page's cards) ASK for
// it; the UI opens it on its next update (ui.h). Built with the kit
// (kit.h), over everything, the keyboard under it.
// ===========================================================================

struct fude_ui;

#define FUDE_WORDCARD_FOUND 8u   // words found in what was read, offered at most

RDE_STRUCT {
    c8 written[FUDE_USERWORD_WRITTEN];
    c8 reading[FUDE_USERWORD_READING];
    c8 meaning[FUDE_USERWORD_MEANING];
} fude_wordcard_word;

// A chip's own pointer for its callback: the UI and which chip.
RDE_STRUCT {
    struct fude_ui* ui;
    u32                  index;
} fude_wordcard_ref;

RDE_STRUCT {
    b8                  open;
    u32                 id;                        // the saved word shown (0: one not saved yet)
    u32                 kanji;                     // the page it comes from (0: none)
    u32                 lists[FUDE_VOCAB_LISTS];   // the lists as shown: ids, in the order made
    b8                  ticked[FUDE_VOCAB_LISTS];  // ...the word in it, on Save
    u32                 list_count;
    b8                  naming;                    // a new list's name being written
    b8                  list_mode;                 // the list form: list_id's name (0: a new list)
    u32                 list_id;
    b8                  note_mode;                 // the note form: note_cp's note
    u32                 note_cp;
    rde_ui_text_editor* note_field;
    fude_wordcard_word  found[FUDE_WORDCARD_FOUND];
    u32                 found_count;
    fude_wordcard_ref   refs[FUDE_VOCAB_LISTS + FUDE_WORDCARD_FOUND];
    c8                  sentence[FUDE_VOCAB_SENTENCE];      // the sentence it was met in ("": none), kept on Save
    c8                  sentence_to[FUDE_VOCAB_SENTENCE];   // ...its translation
    // The meaning into Japanese (Translate with Google): where it is, the
    // meaning sent (and its language), the answer's ticket.
    u8                  translating;               // FUDE_WORDCARD_TRANSLATE_ (wordcard.c)
    u32                 translate_says;            // the status's text (FUDE_TEXT_; FUDE_TEXT_COUNT none)
    b8                  translate_prepared;        // the models asked for, this time open
    c8                  translate_from[FUDE_USERWORD_MEANING];
    const c8*           translate_lang;
    u32                 ticket;

    fude_kit_modal      modal;                     // the card over its backdrop (a tap on it cancels)
    rde_ui_label*       title;
    rde_ui_text_editor* fields[3];                 // written, reading, meaning
    rde_ui_button*      translate;                 // Translate with Google
    rde_ui_label*       translate_status;
    rde_ui_image*       badge;                     // Google's, by a translation in
    rde_ui_label*       sentence_caption;
    rde_ui_label*       sentence_label;
    rde_ui_label*       sentence_to_label;
    rde_ui_label*       found_caption;
    rde_ui_button*      found_chips[FUDE_WORDCARD_FOUND];
    rde_ui_label*       lists_caption;
    rde_ui_button*      list_chips[FUDE_VOCAB_LISTS];
    rde_ui_button*      new_list;
    rde_ui_text_editor* list_field;
    rde_ui_button*      list_ok;
    rde_ui_label*       error;
    rde_ui_button*      remove;
    rde_ui_button*      cancel;
    rde_ui_button*      save;
    b8                  _layout;                   // to be laid out (opened, a list made, the screen turned)
    rde_vec_2F          _laid_out;                 // the screen size it was laid out for
} fude_wordcard;

// --- asking for it (from anywhere; it opens on the UI's next update) ------------------

// A word: saved already, the card shows it as saved (its own texts, its lists);
// otherwise these texts, to save. _kanji: the page it comes from (0: none).
void fude_wordcard_ask(const c8* _written, const c8* _reading, const c8* _meaning, u32 _kanji);
// A word met in a sentence (_sentence, Japanese) and its translation: as
// fude_wordcard_ask, the sentence kept with it when it is saved (a saved word
// keeps the one it has).
void fude_wordcard_ask_in(const c8* _written, const c8* _reading, const c8* _meaning, const c8* _sentence, const c8* _translation);
// A saved word, by id.
void fude_wordcard_ask_saved(u32 _id);
// Empty fields, to type a word in. _kanji: the page it is typed on (0: none).
void fude_wordcard_ask_typed(u32 _kanji);
// What the lasso read (_text): one word of Kana's dictionary, that word (as the
// dictionary writes it: 食べました is 食べる); more, the text, with them offered.
void fude_wordcard_ask_read(const fude_kanji_db* _db, const c8* _text);
// The list form: list _list renamed or deleted; 0, a new list named.
void fude_wordcard_ask_list(u32 _list);
// A list made in the list form since the last call (its id), once; 0: none.
u32  fude_wordcard_take_new_list(void);
// New words come with _list ticked (the Vocabulary screen's list shown); 0: none.
void fude_wordcard_prefer_list(u32 _list);
// The note form: character _codepoint's note.
void fude_wordcard_ask_note(u32 _codepoint);

// --- the UI's (ui.h) ---------------------------------------------------------------------

void fude_wordcard_create(struct fude_ui* _ui, rde_ui_node* _root);
// Once a frame: what was asked for opened; laid out when the screen changed.
void fude_wordcard_update(struct fude_ui* _ui);
void fude_wordcard_close(struct fude_ui* _ui);
// Translate with Google pressed: the meaning typed, into Japanese for the word.
void fude_wordcard_translate(struct fude_ui* _ui);
void fude_wordcard_apply_theme(struct fude_ui* _ui);

#endif
