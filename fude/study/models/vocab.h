// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_VOCAB
#define FUDE_VOCAB

#include "rde.h"
#include "study/chars/kanji.h"

// ===========================================================================
// Vocabulary: the learner's own words, kept to study. A word is saved from a character's examples, a sentence, a
// photo, the lasso (what was written or pasted), or typed in, each with its reading and meaning, which the learner
// can change. Words go in the learner's named LISTS ("Lesson 3", "Food"...), as many or as few as they like: a word
// can be in several lists, or in none and still be in the vocabulary. Lists hold single characters too (Select mode's
// Save as list).
//
// A kanji's own words (shown by the viewer, and used in its exams' prompts) are the vocabulary's words written with
// it, plus those saved from its page (the word's kanji field below) even when they are written without it.
//
// Where it sits: a study model (study/models). The word card saves and edits words (study/widgets/wordcard.h); the
// viewer, the Vocabulary screen, word exams, Text from a photo, the translator, the exams' prompts and the statistics
// read them; Select mode's Save as list (study/app/verbs.c) makes lists of characters; the study's session opens the
// file (study/app/files.c).
// Its main types and their lifecycle: there is no object; the words, lists and sentences are one set held by this
// module. fude_vocab_open loads them when the session opens and fude_vocab_close frees them when the app ends.
// Without fude_vocab_open (or with a NULL path) they live in memory only, which is what the tests use. A word's id is
// never 0 and never reused, and neither is a list's.
// Threads: main thread only.
// Files: one small file, <save dir>/words.kana, in the KANA format (kfile.h), written whole and atomically on every
// change and read back with fude_save_load (save.h: the main file, else its .bak). Its layout:
//   header kind 'UWRD', then:
//   'VOCB'  u32 count, u32 the next word id, then per word: u32 id, u32 kanji (the code point of the page it was
//           saved from; 0 for none), u32 time low, u32 time high (when it was saved, in Unix seconds), then the
//           written form, the reading and the meaning, each as a u16 length in bytes and the UTF-8 bytes.
//   'LIST'  u32 count, u32 the next list id, then per list: u32 id, u32 time low, u32 time high (when it was made),
//           its name (u16 length, then the bytes), u32 n, and n word ids in the order they went in.
//   'SENT'  u32 count, then per sentence: u32 word id, then the sentence the word was met in and its translation
//           (each a u16 length, then the bytes). Only words met in a sentence have one. Builds older than this chunk
//           skip it, as readers skip chunk tags they do not know.
//   'WRDS'  only read, from files made before the vocabulary existed: the kanji's words, as u32 count, then per word
//           u32 kanji, u32 time low, u32 time high, and the written form, reading and meaning as above. They are read
//           into the vocabulary, each word once.
// Design: words are kept as text, not as the character data's word numbers, so a re-bake of the data cannot lose or
// change them. A few thousand words at most, so they are one array in the order saved, searched in full by id; by
// their written form and reading, which screens ask for each word they show every frame, they are found through a map.
// ===========================================================================

// The most characters a word's texts and a list's name can have, as the word card lets them be typed (wordcard.c):
// a word is short, and a field that size still shows it whole. Each text's bytes, its NUL included, are worked out
// from them at FUDE_USERWORD_CHAR_BYTES a character, so that what the card took is kept whole. A character, as the
// card counts it, is what a reader calls one (a grapheme): 3 bytes in Japanese, Chinese or Korean, and up to about 8
// in Hindi or Thai, a letter with its signs (कि, ที่). The card refuses a text longer than its bytes rather than cut it
// (a text that long is not a word of any language taught); one read from a file or the dictionary is cut, when longer,
// before a whole character (utf8.h).
#define FUDE_USERWORD_CHAR_BYTES    8u
#define FUDE_USERWORD_WRITTEN_CHARS 24u
#define FUDE_USERWORD_READING_CHARS 32u
#define FUDE_USERWORD_MEANING_CHARS 60u
#define FUDE_VOCAB_LIST_NAME_CHARS  30u
#define FUDE_USERWORD_WRITTEN (FUDE_USERWORD_CHAR_BYTES * FUDE_USERWORD_WRITTEN_CHARS + 1u)
#define FUDE_USERWORD_READING (FUDE_USERWORD_CHAR_BYTES * FUDE_USERWORD_READING_CHARS + 1u)
#define FUDE_USERWORD_MEANING (FUDE_USERWORD_CHAR_BYTES * FUDE_USERWORD_MEANING_CHARS + 1u)
#define FUDE_VOCAB_LIST_NAME  (FUDE_USERWORD_CHAR_BYTES * FUDE_VOCAB_LIST_NAME_CHARS + 1u)
#define FUDE_VOCAB_SENTENCE   384u    // The most bytes a word's sentence (or its translation) can have.

// A word's key among the reviews' schedules (review.h): above every code point, so words and characters never meet.
#define FUDE_VOCAB_KEY_BIT    0x40000000u
#define FUDE_VOCAB_KEY(_id)   (FUDE_VOCAB_KEY_BIT | (_id))

// One of the learner's words.
RDE_STRUCT {
    u32 id;        // Never 0, never reused.
    u32 kanji;     // The kanji whose page it was saved from (0 for none).
    u64 added;     // When it was saved, in Unix seconds.
    c8  written[FUDE_USERWORD_WRITTEN];
    c8  reading[FUDE_USERWORD_READING];
    c8  meaning[FUDE_USERWORD_MEANING];
} fude_vocab_word;

// Loads the vocabulary from _path, and saves there from now on. When nothing has been saved yet it starts empty. A
// NULL _path keeps it in memory only.
void fude_vocab_open(const c8* _path);
// Forgets every word, list and sentence, and the path, and frees their memory. Nothing is saved.
void fude_vocab_close(void);

// --- words ---------------------------------------------------------------------------

// The words, in the order saved: how many, and the _index-th (NULL past the end). A returned pointer is the
// module's and stays valid only until a word is next added or removed, or the vocabulary is opened or closed.
u32                    fude_vocab_count(void);
const fude_vocab_word* fude_vocab_at(u32 _index);
// The word with id _id; NULL when there is no such word. Valid as long as fude_vocab_at's pointers.
const fude_vocab_word* fude_vocab_get(u32 _id);
// The id of the saved word written _written and read _reading (a NULL reading is ""); 0 when it is not saved.
u32                    fude_vocab_find(const c8* _written, const c8* _reading);

// Saves a word, its texts cut to the sizes above, and returns its id. When a word with that written form and reading
// is saved already, it returns that one's id and changes nothing. _kanji is the page it comes from (0 for none).
// Returns 0 when nothing is written. A file that cannot be written is logged; the word is kept in memory.
u32  fude_vocab_add(const c8* _written, const c8* _reading, const c8* _meaning, u32 _kanji);
// Changes a saved word's texts; its lists and reviews stay. Returns false when there is no such word, the written
// form is empty, or another saved word already has that written form and reading.
b8   fude_vocab_update(u32 _id, const c8* _written, const c8* _reading, const c8* _meaning);
// Takes a word out of the vocabulary and off every list, and drops its sentence.
void fude_vocab_remove(u32 _id);

// Keeps with word _id the sentence it was met in and its translation; either may be empty. Setting it again
// replaces it, and setting both empty (or NULL) removes it. Nothing happens when there is no such word.
void      fude_vocab_set_sentence(u32 _id, const c8* _taught, const c8* _translation);
// Word _id's sentence ("" when it has none), and its translation into *_translation ("" when none; _translation may
// be NULL). Both strings are the module's and stay valid until the sentences next change.
const c8* fude_vocab_sentence(u32 _id, const c8** _translation);

// A kanji's own words (see the top): their ids in the order saved, at most _max of them into _out. With _out NULL it
// only counts. Returns how many it wrote, or, with _out NULL, how many there are.
u32  fude_vocab_of_kanji(u32 _kanji, u32* _out, u32 _max);

// --- lists ---------------------------------------------------------------------------

// How many lists there are.
u32       fude_vocab_list_count(void);
// The id of the _index-th list, in the order they were made (0 past the end).
u32       fude_vocab_list_at(u32 _index);
// A list's name ("" when there is no such list). The string is the module's and stays valid until the lists change.
const c8* fude_vocab_list_name(u32 _list);
// Makes a new, empty list and returns its id; 0 when the name is empty. There can be any number of lists.
u32       fude_vocab_list_add(const c8* _name);
// Renames a list. Returns false when there is no such list or the name is empty.
b8        fude_vocab_list_rename(u32 _list, const c8* _name);
// Removes a list; its words stay in the vocabulary.
void      fude_vocab_list_remove(u32 _list);
// A list's words: their ids in the order they went in, at most _max into _out. With _out NULL it only counts.
// Returns how many it wrote, or, with _out NULL, how many there are (0 when there is no such list).
u32       fude_vocab_list_words(u32 _list, u32* _out, u32 _max);
// Whether word _word is in list _list.
b8        fude_vocab_in_list(u32 _list, u32 _word);
// Puts word _word in list _list (_in), at its end, or takes it out. Nothing happens when there is no such list, or
// when the word to put in is not saved.
void      fude_vocab_set_in_list(u32 _list, u32 _word, b8 _in);
// Writes into _out a name for a new list that no list has yet ("List 3"), in the app's language.
void      fude_vocab_list_new_name(c8* _out, usize _size);
// Saves characters (records into _db) as a new list, named as fude_vocab_list_new_name names it (the name is written
// into _name). Each character becomes a word: written as itself, read by its first reading of the kind a one-character
// word is read by (lang.h; for Japanese its first kun reading, else its on reading), cut at the stem mark '.' and
// without '-' marks, with its meanings. A letter of the script (a kana) is read as itself, and its meaning is its
// romanisation (romaji). Returns the list's id, 0 when the list could not be made; *_saved gets how many characters
// went in.
u32       fude_vocab_add_characters(const fude_kanji_db* _db, const u32* _records, u32 _count, c8* _name, usize _name_size, u32* _saved);

// A number that goes up on every change to the words or the lists, so what shows them knows when to look again.
u32       fude_vocab_revision(void);

// --- a kanji's words, as the viewer and exams list them ---------------------------------

// How many words a kanji has (as fude_vocab_of_kanji counts them), and the _index-th of them into _out. _out's
// strings point into the saved word and stay valid until the vocabulary next changes. False past the last.
u32  fude_vocab_kanji_count(u32 _kanji);
b8   fude_vocab_kanji_at(u32 _kanji, u32 _index, fude_kanji_word* _out);

#endif
