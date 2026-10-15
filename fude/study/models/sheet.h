// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SHEET
#define FUDE_SHEET

#include "rde.h"
#include "study/chars/kanji.h"

// ===========================================================================
// Practice sheets: characters as a PDF to print and write on with a pencil. Each character gets its meaning and
// readings, its stroke order one stroke at a time, then rows of boxes: the first box shows it whole with its strokes
// numbered, the next few show it faded to write over, and the rest are empty, each with the dashed cross that helps a
// hand centre the character. One character fills a page; more characters share pages, six a page at most.
//
// Where it sits: a study model (study/models). The study's files (study/app/files.c: fude_study_sheet_write) call it
// when the viewer, Select mode or the Vocabulary screen asks for a sheet, then share or save the PDF.
// Its main types and their lifecycle: none to keep; fude_sheet_write builds the whole PDF in memory, writes it and
// frees it.
// Threads: main thread only (it reads the app's strings for the title and the credit).
// Files: writes one PDF 1.4 at the path it is given, A4 on every page, with the stroke order's credit at the foot of
// each page (the language's own text: KanjiVG, CC BY-SA 3.0, for Japanese). It is written in one go, not atomically;
// see sheet.c for how the PDF is laid out.
// Design: every character the character data has is drawn from its strokes (for Japanese, KanjiVG's curves) as PDF
// curves, so no font goes into the file. Other text is Helvetica, one of the PDF's standard fonts, which covers Latin
// only; a character neither of them can draw is left out.
// ===========================================================================

// The most characters a sheet takes; past that, only the first ones go in, and the notice that the sheet was made
// says so (study/app/files.c). A bound by design: a sheet is printed, six characters a page, so two hundred already
// make 34 pages, and the whole PDF is made in memory at once.
#define FUDE_SHEET_MAX 200u

// What went into a sheet.
RDE_STRUCT {
    u32 characters;   // How many characters went in.
    u32 pages;
    u32 bytes;
} fude_sheet_info;

// Writes the characters of _records (record indices into _db, in order) as a sheet at _path. Only the first
// FUDE_SHEET_MAX that have strokes go in. Its texts (the title, the credit) are in the app's language. Returns true
// when the file was written whole; false when there is no data, nothing to draw, no path, or the file could not be
// written (that last is logged). _creator is the app's name, written as the PDF's creator and producer (NULL: none).
// _info may be NULL; otherwise it is zeroed and, on success, says what went in.
b8 fude_sheet_write(const fude_kanji_db* _db, const u32* _records, u32 _count, const c8* _path, const c8* _creator, fude_sheet_info* _info);

#endif
