#ifndef FUDE_VOCAB
#define FUDE_VOCAB

#include "rde.h"
#include "study/chars/kanji.h"

// ===========================================================================
// The learner's vocabulary: words kept to study — saved from a character's
// examples, a sentence, a photo, the lasso (what was written or pasted), or
// typed in — each with its reading and meaning, the learner's to change. Words
// go in the learner's named LISTS ("Lesson 3", "Food"...), as many or as few as
// they like: a word can be in several, or in none (it is still in the
// vocabulary). Lists hold single characters too (Select mode's Save as list).
//
// A kanji's own words (the viewer's, its exams' prompts) are the vocabulary's
// words written with it, and those saved from its page (KANJI below) even when
// written without it.
//
// Kept as text (not the character data's word numbers), so a re-bake cannot lose
// or change them. One small file (<save dir>/words.kana), written whole on every
// change (atomically, see kfile.h). Without fude_vocab_open it lives in memory
// only (the tests).
//
// FILE ('KANA' header, kind 'UWRD'):
//   'VOCB'  u32 count, u32 the next word id, then per word: u32 id, u32 kanji (its
//           page's code point, 0: none), u32 time low, u32 time high (when saved,
//           Unix seconds), then written form, reading and meaning, each as u16
//           byte length and the UTF-8 bytes.
//   'LIST'  u32 count, u32 the next list id, then per list: u32 id, u32 time low,
//           u32 time high (made), its name (u16 length, bytes), u32 n, n word ids
//           (in the order they went in).
//   'WRDS'  (read only: files from before the vocabulary) the kanji's words — u32
//           count, then per word u32 kanji, u32 time low, u32 time high, written,
//           reading, meaning (as above). Read into the vocabulary, each word once.
// ===========================================================================

#define FUDE_USERWORD_WRITTEN 64u     // bytes a written form can have (with its NUL)
#define FUDE_USERWORD_READING 96u
#define FUDE_USERWORD_MEANING 160u
#define FUDE_VOCAB_LIST_NAME  48u     // bytes a list's name can have (with its NUL)
#define FUDE_VOCAB_LISTS      64u     // lists at most

// A word's key among the reviews' (review.h): above every code point.
#define FUDE_VOCAB_KEY_BIT    0x40000000u
#define FUDE_VOCAB_KEY(_id)   (FUDE_VOCAB_KEY_BIT | (_id))

RDE_STRUCT {
    u32 id;        // never 0, never reused
    u32 kanji;     // the kanji whose page it was saved from (0: none)
    u64 added;     // Unix seconds
    c8  written[FUDE_USERWORD_WRITTEN];
    c8  reading[FUDE_USERWORD_READING];
    c8  meaning[FUDE_USERWORD_MEANING];
} fude_vocab_word;

void fude_vocab_open(const c8* _path);
void fude_vocab_close(void);

// --- words ---------------------------------------------------------------------------

// Every word, in the order saved.
u32                    fude_vocab_count(void);
const fude_vocab_word* fude_vocab_at(u32 _index);
// By id (NULL: no such word).
const fude_vocab_word* fude_vocab_get(u32 _id);
// The word written _written and read _reading: its id, 0 when it is not saved.
u32                    fude_vocab_find(const c8* _written, const c8* _reading);

// Saves a word (texts cut to the sizes above): its id — the one it had when it
// was saved already. _kanji: the page it comes from (0: none). 0 when it could
// not be (nothing written).
u32  fude_vocab_add(const c8* _written, const c8* _reading, const c8* _meaning, u32 _kanji);
// A saved word's texts changed (its lists and reviews stay). False when another
// saved word already is that written form and reading, or there is no such word.
b8   fude_vocab_update(u32 _id, const c8* _written, const c8* _reading, const c8* _meaning);
// Out of the vocabulary, and off every list.
void fude_vocab_remove(u32 _id);

// A kanji's own words (see the top): their ids, in the order saved, at most
// _max into _out (may be NULL to count). How many.
u32  fude_vocab_of_kanji(u32 _kanji, u32* _out, u32 _max);

// --- lists ---------------------------------------------------------------------------

u32       fude_vocab_list_count(void);
// The _index-th list (in the order made): its id.
u32       fude_vocab_list_at(u32 _index);
// A list's name ("" when there is no such list).
const c8* fude_vocab_list_name(u32 _list);
// A new list: its id; 0 when there are FUDE_VOCAB_LISTS already or the name is empty.
u32       fude_vocab_list_add(const c8* _name);
b8        fude_vocab_list_rename(u32 _list, const c8* _name);
// The list goes; its words stay in the vocabulary.
void      fude_vocab_list_remove(u32 _list);
// Its words: ids in the order they went in, at most _max into _out (NULL: just
// count). How many.
u32       fude_vocab_list_words(u32 _list, u32* _out, u32 _max);
b8        fude_vocab_in_list(u32 _list, u32 _word);
// _word in _list, or out of it.
void      fude_vocab_set_in_list(u32 _list, u32 _word, b8 _in);
// A name for a new list nobody has yet ("List 3"), in the app's language.
void      fude_vocab_list_new_name(c8* _out, usize _size);
// Characters (records into _db) as a new list, named as fude_vocab_list_new_name
// does (into _name): each one a word — written as itself, read as its first kun
// reading (else on; a kana, itself), with its meanings (a kana's, its romaji).
// The list's id (0: no room for another list); how many went in, into _saved.
u32       fude_vocab_add_characters(const fude_kanji_db* _db, const u32* _records, u32 _count, c8* _name, usize _name_size, u32* _saved);

// Goes up on every change (words, lists).
u32       fude_vocab_revision(void);

// --- a kanji's words, as the viewer and exams list them ---------------------------------

u32  fude_vocab_kanji_count(u32 _kanji);
b8   fude_vocab_kanji_at(u32 _kanji, u32 _index, fude_kanji_word* _out);

#endif
