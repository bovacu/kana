// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_TEXTINK
#define FUDE_TEXTINK

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/catalog.h"
#include "drawing/ink/ink.h"
#include "drawing/ink/lasso.h"
#include "study/handwriting/segment.h"
#include "study/handwriting/recognize.h"

// ===========================================================================
// Text ink: text and handwriting, both ways, for the system clipboard on the canvas.
//
// TEXT TO INK (the context menu's Paste text): each character the data has is written in its own strokes (KanjiVG's,
// as the viewer draws them), with the brush's width and colour, a cell each, in lines that wrap at a width; a line
// break starts a new line, and a space leaves a cell empty. A right-to-left language (Arabic) is written leftwards,
// each letter in the form its neighbours give it (lang.h: fude_lang_form) and joined to the letter before; a sign
// written on a letter goes over that letter. What the data does not have (Latin letters, most punctuation) is left
// out and counted.
//
// INK TO TEXT (the selection's Copy as text, Translate and Save word): the selected strokes are read as Check reads
// them (recognize.h): ML Kit's best reading where it is, and the app's own (segment.h: each character's best match,
// with a line break between lines) where it is not, or when ML Kit reads nothing. ML Kit answers later, so a reading
// is started, then updated once a frame until it is done.
//
// Where it sits: the study layer's handwriting (study/handwriting), used by the page text widget (pagetext.h).
// Its main types and their lifecycle: fude_textink_write fills a fude_clip the caller owns. The caller owns a
// fude_textink_reader, made by fude_textink_reader_init and freed by fude_textink_reader_destroy; the character
// data and the catalog it reads with must outlive it.
// Threads: call it on the app's main thread only (it uses static scratch, and ML Kit's answer is polled there).
// Files: none.
// ===========================================================================

#define FUDE_TEXTINK_TEXT 1024u   // bytes of a reading, at most

RDE_STRUCT {
    u32 drawn;      // the characters written
    u32 skipped;    // the characters the data does not have
} fude_textink_result;

// Writes _text (UTF-8) as strokes into _clip (cleared first, and set up when it is not yet), placed around the
// written block's centre, Y up: _cell canvas units a character, lines no wider than _max_width, points of radius
// _radius, and strokes of _color. Returns how many characters were written and how many were left out.
fude_textink_result fude_textink_write(const fude_kanji_db* _db, const c8* _text, f32 _cell, f32 _max_width, f32 _radius, rde_color _color,
                                       fude_clip* _clip);

RDE_STRUCT {
    const fude_kanji_db* db;
    const fude_catalog*  catalog;
    fude_ink             drawing;       // the strokes being read, in writing order
    fude_segment         segment;
    fude_recognition     recognition;
    b8                   reading;       // a reading has started and is not done
    b8                   asked;         // ML Kit has the drawing
    c8                   text[FUDE_TEXTINK_TEXT];   // the reading once done ("" when nothing could be read)
} fude_textink_reader;

// Sets up a reader that reads with _db and _catalog; and frees it.
void fude_textink_reader_init(fude_textink_reader* _reader, const fude_kanji_db* _db, const fude_catalog* _catalog);
void fude_textink_reader_destroy(fude_textink_reader* _reader);

// Starts reading the strokes _ids of _ink (in any order; the marker's strokes are left out, as they are not
// writing). Returns false when there is nothing to read, or no data.
b8   fude_textink_read(fude_textink_reader* _reader, const fude_ink* _ink, const u32* _ids, u32 _count);
// Once a frame: returns true on the frame the reading is done, with its text in _reader->text.
b8   fude_textink_update(fude_textink_reader* _reader);

#endif
