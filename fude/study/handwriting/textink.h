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
// Text and handwriting, both ways — for the system clipboard on the canvas.
//
// TEXT → INK (the context menu's Paste text): each character the data has is
// written in its own strokes (KanjiVG's, as the viewer draws them), with the
// brush's width and colour, a cell each, in lines across that wrap at a width;
// a line break starts a new line, a space leaves a cell. What the data does not
// have (Latin letters, most punctuation) is left out and counted.
//
// INK → TEXT (the selection's Copy as text): the selected strokes are read as
// Check reads them (recognize.h) — ML Kit's best reading where it is, Kana's
// own (segment.h: each character's best match, lines on lines of their own)
// where it is not, or when ML Kit reads nothing. ML Kit answers later, so a
// reading is started, then updated once a frame until it is done.
// ===========================================================================

#define FUDE_TEXTINK_TEXT 1024u   // bytes of a reading, at most

RDE_STRUCT {
    u32 drawn;      // characters written
    u32 skipped;    // characters the data does not have
} fude_textink_result;

// _text (UTF-8) as strokes into _clip (cleared first), relative to the written
// block's centre, Y up: _cell canvas units a character, lines no wider than
// _max_width, points _radius wide and strokes _color.
fude_textink_result fude_textink_write(const fude_kanji_db* _db, const c8* _text, f32 _cell, f32 _max_width, f32 _radius, rde_color _color,
                                       fude_clip* _clip);

RDE_STRUCT {
    const fude_kanji_db* db;
    const fude_catalog*  catalog;
    fude_ink             drawing;       // the strokes being read, in writing order
    fude_segment         segment;
    fude_recognition     recognition;
    b8                   reading;       // started and not done
    b8                   asked;         // ML Kit has the drawing
    c8                   text[FUDE_TEXTINK_TEXT];   // the reading once done ("" when nothing could be read)
} fude_textink_reader;

void fude_textink_reader_init(fude_textink_reader* _reader, const fude_kanji_db* _db, const fude_catalog* _catalog);
void fude_textink_reader_destroy(fude_textink_reader* _reader);

// Starts reading strokes _ids of _ink (any order). False when there is nothing
// to read (or no data).
b8   fude_textink_read(fude_textink_reader* _reader, const fude_ink* _ink, const u32* _ids, u32 _count);
// Once a frame: true the frame the reading is done (the text in _reader->text).
b8   fude_textink_update(fude_textink_reader* _reader);

#endif
