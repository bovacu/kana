// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_DOCUMENTS
#define FUDE_DOCUMENTS

#include "rde.h"
#include "drawing/app/app.h"
#include "drawing/doc/doc.h"

// ===========================================================================
// Documents: the app's documents. It finds which one is under each canvas's ink (doc.h shows one of them), makes new
// canvases over one, and opens the app's library of books. The canvases are the app's notes (notes.h); a book belongs
// to the app's extension (extension.h: library).
//
// Where it sits: drawing/app. The page (page.c) asks it for the open canvas's document; the Library (library.h) and
// the import (import.h) make canvases with it.
//
// Threads: main thread only.
//
// Files: a new canvas writes its page file (pagefile.h), and a canvas's own PDF is copied in as notes/<id>.pdf
// (notes.h).
// ===========================================================================

// Writes into _out where the PDF is for canvas _canvas, whose note says _document (fude_note.document): its own PDF,
// or a library book's file. Returns false (with _out empty) when _document is 0 (no document) or there is no such
// book.
b8   fude_documents_path(const fude_app* _app, u32 _canvas, u8 _document, c8* _out, usize _size);

// Makes a new canvas over a document and opens it: named _name, in folder _parent (0 for the top level), its view at
// the first page and no paper under it. _document is a library book's id, or FUDE_NOTE_DOCUMENT_OWN, in which case
// the PDF at _own is copied in as the canvas's own. Returns the canvas's id, or 0 when it could not be made (nothing
// is left behind then).
u32  fude_documents_new_canvas(fude_app* _app, u8 _document, const c8* _own, const c8* _name, u32 _parent);
// Opens the canvas over library book _book and returns its id: the one there is, or else a new one in the folder the
// other books' canvases are in. The first time, that folder is made at the top level, named _folder_name. Returns 0
// when a new canvas could not be made.
u32  fude_documents_open_book(fude_app* _app, const fude_doc_book* _book, const c8* _folder_name);

#endif
