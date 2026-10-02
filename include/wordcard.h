#ifndef KANA_WORDCARD
#define KANA_WORDCARD

#include "rde.h"
#include "kanji.h"
#include "vocab.h"

// ===========================================================================
// The word card: where a word goes into the learner's vocabulary (vocab.h), and
// where a saved one is changed or taken out. The same card wherever a word is —
// a character's examples, an example sentence's words, a photo's or a
// translation's words, what the lasso read, or a word typed in:
//
//   the word, its reading and its meaning, in fields to change (a reading in
//   romaji becomes hiragana; one left empty is looked up in Kana's dictionary);
//   the words found in what was read, when it was more than one (tap: the fields
//   take it); the learner's lists, ticked (a new one named on the spot); Remove
//   (a saved word), Cancel, Save.
//
// A new word comes with the lists the last one went in already ticked: saving a
// lesson's words one after another is a tap each.
//
// Its LIST form: a list's name, to make a new list or rename one; Delete list
// (its words stay in the vocabulary). Its NOTE form: the learner's note on a
// character (charnote.h), written freely; Delete note.
//
// Screens drawn by hand (the viewer, Text from a photo, the page's cards) ASK for
// it; the toolbar opens it on its next update. Built with the toolbar's kit
// (toolbar_kit.h), over everything, the keyboard under it.
// ===========================================================================

struct kana_toolbar;

#define KANA_WORDCARD_FOUND 8u   // words found in what was read, offered at most

RDE_STRUCT {
    c8 written[KANA_USERWORD_WRITTEN];
    c8 reading[KANA_USERWORD_READING];
    c8 meaning[KANA_USERWORD_MEANING];
} kana_wordcard_word;

// A chip's own pointer for its callback: the toolbar and which chip.
RDE_STRUCT {
    struct kana_toolbar* toolbar;
    u32                  index;
} kana_wordcard_ref;

RDE_STRUCT {
    b8                  open;
    u32                 id;                        // the saved word shown (0: one not saved yet)
    u32                 kanji;                     // the page it comes from (0: none)
    u32                 lists[KANA_VOCAB_LISTS];   // the lists as shown: ids, in the order made
    b8                  ticked[KANA_VOCAB_LISTS];  // ...the word in it, on Save
    u32                 list_count;
    b8                  naming;                    // a new list's name being written
    b8                  list_mode;                 // the list form: list_id's name (0: a new list)
    u32                 list_id;
    b8                  note_mode;                 // the note form: note_cp's note
    u32                 note_cp;
    rde_ui_text_editor* note_field;
    kana_wordcard_word  found[KANA_WORDCARD_FOUND];
    u32                 found_count;
    kana_wordcard_ref   refs[KANA_VOCAB_LISTS + KANA_WORDCARD_FOUND];

    rde_ui_button*      backdrop;
    rde_ui_image*       card;
    rde_ui_label*       title;
    rde_ui_text_editor* fields[3];                 // written, reading, meaning
    rde_ui_label*       found_caption;
    rde_ui_button*      found_chips[KANA_WORDCARD_FOUND];
    rde_ui_label*       lists_caption;
    rde_ui_button*      list_chips[KANA_VOCAB_LISTS];
    rde_ui_button*      new_list;
    rde_ui_text_editor* list_field;
    rde_ui_button*      list_ok;
    rde_ui_label*       error;
    rde_ui_button*      remove;
    rde_ui_button*      cancel;
    rde_ui_button*      save;
    b8                  _layout;                   // to be laid out (opened, a list made, the screen turned)
    rde_vec_2F          _laid_out;                 // the screen size it was laid out for
} kana_wordcard;

// --- asking for it (from anywhere; it opens on the toolbar's next update) -------------

// A word: saved already, the card shows it as saved (its own texts, its lists);
// otherwise these texts, to save. _kanji: the page it comes from (0: none).
void kana_wordcard_ask(const c8* _written, const c8* _reading, const c8* _meaning, u32 _kanji);
// A saved word, by id.
void kana_wordcard_ask_saved(u32 _id);
// Empty fields, to type a word in. _kanji: the page it is typed on (0: none).
void kana_wordcard_ask_typed(u32 _kanji);
// What the lasso read (_text): one word of Kana's dictionary, that word (as the
// dictionary writes it: 食べました is 食べる); more, the text, with them offered.
void kana_wordcard_ask_read(const kana_kanji_db* _db, const c8* _text);
// The list form: list _list renamed or deleted; 0, a new list named.
void kana_wordcard_ask_list(u32 _list);
// A list made in the list form since the last call (its id), once; 0: none.
u32  kana_wordcard_take_new_list(void);
// New words come with _list ticked (the Vocabulary screen's list shown); 0: none.
void kana_wordcard_prefer_list(u32 _list);
// The note form: character _codepoint's note.
void kana_wordcard_ask_note(u32 _codepoint);

// --- the toolbar's -----------------------------------------------------------------------

void kana_wordcard_create(struct kana_toolbar* _toolbar, rde_ui_node* _root);
// Once a frame: what was asked for opened; laid out when the screen changed.
void kana_wordcard_update(struct kana_toolbar* _toolbar);
void kana_wordcard_close(struct kana_toolbar* _toolbar);
void kana_wordcard_apply_theme(struct kana_toolbar* _toolbar);

#endif
