// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/app/documents.h"
#include "drawing/base/text.h"
#include "drawing/ink/notes.h"
#include "drawing/ink/pagefile.h"

// ===========================================================================
// See documents.h.
//
// A canvas's note stores which document lies under its ink as one byte (fude_note.document): 0 is none,
// FUDE_NOTE_DOCUMENT_OWN (1) is the canvas's own PDF kept beside it as notes/<id>.pdf, and 2 and up are the ids of the
// books in the app's library (fude_extension.library). The library is short, so it is searched linearly.
// ===========================================================================

// Writes into _out the path of the PDF under canvas _canvas: its own copy, or the library book's file. Returns false,
// with _out empty, when _document is 0 or names no book in the library.
b8 fude_documents_path(const fude_app* _app, u32 _canvas, u8 _document, c8* _out, usize _size) {
    _out[0] = 0;
    if(_document == 0u) {
        return false;
    }
    if(_document == FUDE_NOTE_DOCUMENT_OWN) {
        fude_notes_document_path(_canvas, _out, _size);
        return true;
    }
    const fude_extension* _ext = fude_app_ext(_app);
    for(u32 _i = 0; _i < _ext->library_count; _i++) {
        if(_ext->library[_i].id == _document) {
            rde_snprintf(_out, _size, "%s", _ext->library[_i].file);
            return true;
        }
    }
    return false;
}

// Adds the canvas note, copies the canvas's own PDF in when it has one, writes an empty page file for it and opens it.
// If the PDF cannot be copied the note is removed again, so a failure leaves nothing behind. Returns the new id, or 0.
u32 fude_documents_new_canvas(fude_app* _app, u8 _document, const c8* _own, const c8* _name, u32 _parent) {
    const u32 _id = fude_notes_add(_app->notes, FUDE_NOTE_CANVAS, _parent, _name);
    if(_id == 0u) {
        return 0u;
    }
    if(_document == FUDE_NOTE_DOCUMENT_OWN) {
        c8 _to[RDE_MAX_PATH];
        fude_notes_document_path(_id, _to, sizeof(_to));
        if(_own == NULL || !rde_file_copy(_own, _to) || !rde_file_exists(_to)) {
            fude_notes_remove(_app->notes, _id);
            return 0u;
        }
    }
    fude_notes_set_document(_app->notes, _id, _document);
    // Write its page file: no ink yet, a view showing the first page's width on screen, and no paper under it.
    fude_ink* _ink = (fude_ink*)rde_calloc(1, sizeof(fude_ink));
    fude_ink_init(_ink);
    c8 _path[RDE_MAX_PATH];
    fude_notes_canvas_path(_id, _path, sizeof(_path));
    const fude_page _page = { .paper = FUDE_PAPER_NONE };
    fude_save_document(_path, _ink, fude_doc_first_view(_app->window), _page, NULL);
    fude_ink_destroy(_ink);
    rde_free(_ink);
    fude_notes_open(_app->notes, _id);
    return _id;
}

// Opens the canvas over _book if there is one; otherwise makes it in the folder that holds the other books' canvases,
// so they all stay together. Returns the canvas's id, or 0 when a new one could not be made.
u32 fude_documents_open_book(fude_app* _app, const fude_doc_book* _book, const c8* _folder_name) {
    // Look for a canvas already over this book. On the way, remember the folder of the first canvas over any other
    // library book (document 2 and up), because a new canvas goes there too.
    b8  _found  = false;
    u32 _folder = 0u;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_app->notes->notes); _i++) {
        const fude_note* _n = &((const fude_note*)_app->notes->notes.memory)[_i];
        if(_n->kind != FUDE_NOTE_CANVAS || _n->document < 2u) {
            continue;
        }
        if(_n->document == _book->id) {
            fude_notes_open(_app->notes, _n->id);
            return _n->id;
        }
        if(!_found) {
            _found  = true;
            _folder = _n->parent;
        }
    }
    // No other book has a canvas. Reuse a top-level folder named _folder_name if there is one (made the first time and
    // emptied since); otherwise make it.
    for(u32 _i = 0; !_found && _folder_name != NULL && _i < (u32)rde_arr_length(&_app->notes->notes); _i++) {
        const fude_note* _n = &((const fude_note*)_app->notes->notes.memory)[_i];
        if(_n->kind == FUDE_NOTE_FOLDER && _n->parent == 0u && rde_strcmp(_n->name, _folder_name) == 0) {
            _found  = true;
            _folder = _n->id;
        }
    }
    if(!_found && _folder_name != NULL) {
        _folder = fude_notes_add(_app->notes, FUDE_NOTE_FOLDER, 0u, _folder_name);
    }
    return fude_documents_new_canvas(_app, _book->id, NULL, fude_text(_book->title), _folder);
}
