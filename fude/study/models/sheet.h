#ifndef FUDE_SHEET
#define FUDE_SHEET

#include "rde.h"
#include "study/chars/kanji.h"

// ===========================================================================
// Practice sheets: characters as a PDF to print and write on with a pencil —
// for each, its meaning and readings, its stroke order one stroke at a time,
// then rows of boxes: the first shows it whole (its strokes numbered), the next
// few faded to write over, the rest empty, with the dashed cross that centres a
// hand. One character fills a page; more share them (six a page at most).
//
// Everything Japanese is drawn from the character data's strokes (KanjiVG's
// curves, as PDF curves): no font goes in the file. Other text is Helvetica
// (Latin: the PDF's own); a character neither has is left out.
//
// A4, every page; the stroke order's credit (KanjiVG, CC BY-SA 3.0) at the foot
// of each.
// ===========================================================================

#define FUDE_SHEET_MAX 200u   // characters a sheet takes (the first ones)

RDE_STRUCT {
    u32 characters;   // went in
    u32 pages;
    u32 bytes;
} fude_sheet_info;

// The characters of _records (record indices, in order; the first FUDE_SHEET_MAX)
// as a sheet at _path: true when written whole. _info (may be NULL) gets what went
// in. Its texts (title, credit) in the app's language.
b8 fude_sheet_write(const fude_kanji_db* _db, const u32* _records, u32 _count, const c8* _path, fude_sheet_info* _info);

#endif
