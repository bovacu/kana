// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/app/import.h"
#include "drawing/doc/doc.h"
#include "drawing/app/documents.h"
#include "drawing/doc/pdf.h"
#include "drawing/app/app.h"
#include "drawing/base/save.h"
#include "drawing/base/text.h"
#include "drawing/ink/notes.h"
#include "drawing/widgets/notice.h"

// ===========================================================================
// See import.h.
//
// The pickers hand their files over through fude_import_arrived from their own callbacks, outside the app's update.
// The paths are copied into fude_import_in and wait there until the next fude_import_update, which makes them into a
// canvas during the frame, where the app's state can be changed safely.
// ===========================================================================

// What came in and has not been made into a canvas yet. Only one import waits at a time.
RDE_INTERNAL struct {
    // Something came in, or a picker was cancelled (then paths is empty).
    b8                    waiting;
    u8                    kind;    // FUDE_IMPORT_
    rde_arr TYPE(rde_str) paths;   // Every file that came in (a scan's pages, the pictures picked), copied.
} fude_import_in;

// Frees the waiting paths and marks nothing as waiting.
RDE_INTERNAL void fude_import_forget(void) {
    rde_str* _paths = (rde_str*)fude_import_in.paths.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&fude_import_in.paths); _i++) {
        rde_str_free(&_paths[_i]);
    }
    rde_arr_clear(&fude_import_in.paths);
    fude_import_in.waiting = false;
}

// Copies the paths a picker hands over, skipping empty ones, so the picker may free its own at once. A newer import
// replaces one still waiting. See the header.
void fude_import_arrived(u8 _kind, const c8* const* _paths, u32 _count) {
    if(!rde_arr_is_inited(&fude_import_in.paths)) {
        fude_import_in.paths = rde_arr_new(sizeof(rde_str), rde_memory_allocator_get_default_std());
    }
    fude_import_forget();
    fude_import_in.kind    = _kind;
    fude_import_in.waiting = true;
    for(u32 _i = 0; _paths != NULL && _i < _count; _i++) {
        if(_paths[_i] != NULL && _paths[_i][0] != 0) {
            const rde_str _copy = rde_str_new(_paths[_i], rde_memory_allocator_get_default_std());
            rde_arr_add(&fude_import_in.paths, (any)&_copy);
        }
    }
}

// --- where from -----------------------------------------------------------------------------

// Pictures are made into a PDF, so they need the PDF library as well as the platform's photo picker.
b8 fude_import_photos_available(void) {
    return fude_pdf_available() && fude_import_platform_photos();
}

// Scans are made into a PDF too, so they need the PDF library as well as the platform's document camera.
b8 fude_import_camera_available(void) {
    return fude_pdf_available() && fude_import_platform_camera();
}

// The file dialog's callback: _paths is NULL when the dialog failed (a notice says so), and empty when cancelled.
RDE_INTERNAL void fude_import_on_files(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter);
    RDE_UNUSED(_user_data);
    if(_paths == NULL) {
        fude_notice_show(fude_text(FUDE_TEXT_DOC_CANT_PICK));
        return;
    }
    fude_import_arrived(FUDE_IMPORT_FILES, _paths, _count);
}

// Opens the file picker for a PDF or pictures. Android has its own (see below); other platforms use RDE's dialog.
void fude_import_files(fude_app* _app) {
    if(!fude_pdf_available()) {
        fude_notice_show(fude_text(FUDE_TEXT_DOC_NOT_HERE));
        return;
    }
#if defined(RDE_PLATFORM_ANDROID)
    RDE_UNUSED(fude_import_on_files);
    fude_import_platform_pick_files();   // The system's document picker, which hands over copies, like the photos'.
    return;
#endif
    // A PDF, or pictures: many can be picked at once, and each becomes a page.
    // Kept in statics: the dialog reads them until it answers. "PDF" is the format's name in every language.
    static rde_dialog_filter _filters[2];
    _filters[0] = (rde_dialog_filter){ "PDF", "pdf" };
    _filters[1] = (rde_dialog_filter){ fude_text(FUDE_TEXT_DOC_PICTURES), "jpg;jpeg;png;heic;heif;tif;tiff" };
    rde_dialog_open_file(_app->window, _filters, 2u, NULL, true, fude_import_on_files, _app);
}

// Shows the Photos library's picker, when this platform has one.
void fude_import_photos(fude_app* _app) {
    RDE_UNUSED(_app);
    if(fude_import_photos_available()) {
        fude_import_platform_pick_photos();
    }
}

// Shows the document camera, when this platform has one.
void fude_import_camera(fude_app* _app) {
    RDE_UNUSED(_app);
    if(fude_import_camera_available()) {
        fude_import_platform_scan();
    }
}

// --- into a canvas --------------------------------------------------------------------------

// Whether _path ends in ".pdf", in any case.
b8 fude_import_is_pdf(const c8* _path) {
    const usize _n = rde_strlen(_path);
    return _n > 4u && rde_strcasecmp(&_path[_n - 4u], ".pdf") == 0;
}

// Writes into _out the file's name without its folder (after a slash or a backslash) and without its extension. A name
// that is only an extension, such as ".pdf", is kept whole.
void fude_import_name(const c8* _path, c8* _out, usize _size) {
    const c8* _slash = rde_strrchr(_path, '/');
    const c8* _back  = rde_strrchr(_path, '\\');
    const c8* _name  = _back != NULL && (_slash == NULL || _back > _slash) ? _back + 1 : _slash != NULL ? _slash + 1 : _path;
#if defined(RDE_PLATFORM_ANDROID)
    // Android's picker names its copies "<n>_<its name>" (FudeImport.java), so the number and the '_' are dropped.
    const c8* _digits = _name;
    while(*_digits >= '0' && *_digits <= '9') {
        _digits++;
    }
    _name = _digits > _name && *_digits == '_' && _digits[1] != 0 ? _digits + 1 : _name;
#endif
    rde_snprintf(_out, _size, "%s", _name);
    c8* _dot = rde_strrchr(_out, '.');
    if(_dot != NULL && _dot != _out) {
        *_dot = 0;
    }
}

// Makes one new canvas over a document: _paths is either one PDF, used as it is, or pictures, made into one PDF, a page
// each (fude_import_update calls it once for each PDF that came and once for the pictures together). Writes the
// canvas's name into _name (the file's name, or "Scan" or "Photos" and the date) and returns its id, or 0 when it
// could not be made.
RDE_INTERNAL u32 fude_import_into_canvas(fude_app* _app, const c8* const* _paths, u32 _count, c8* _name, usize _name_size) {
    const c8* _pdf      = NULL;
    rde_arr TYPE(const c8*) _pictures = rde_arr_new(sizeof(const c8*), rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < _count; _i++) {
        if(fude_import_is_pdf(_paths[_i])) {
            _pdf = _pdf != NULL ? _pdf : _paths[_i];
        } else {
            rde_arr_add(&_pictures, (any)&_paths[_i]);
        }
    }
    const c8* const* _picture = (const c8* const*)_pictures.memory;
    c8               _made[RDE_MAX_PATH] = "";
    u32              _id                 = 0u;
    if(_pdf != NULL) {
        fude_import_name(_pdf, _name, _name_size);
    } else {
        rde_snprintf(_made, sizeof(_made), "%simport.pdf", fude_save_dir());
        if(fude_pdf_from_images(_picture, (u32)rde_arr_length(&_pictures), _made)) {
            _pdf = _made;
            if(fude_import_in.kind == FUDE_IMPORT_FILES) {
                fude_import_name(_picture[0], _name, _name_size);
            } else {
                c8 _date[48];
                fude_text_date(_date, sizeof(_date), (u64)rde_date_now_unix_seconds());
                fude_text_format(_name, _name_size, fude_import_in.kind == FUDE_IMPORT_CAMERA ? FUDE_TEXT_DOC_SCAN_NAME : FUDE_TEXT_DOC_PHOTOS_NAME,
                                 (fude_text_arg[]){ FUDE_TS(_date) }, 1u);
            }
        }
    }
    // The PDF must open before a canvas is made over it: the file may be damaged, or a "picture" may not have been one.
    fude_pdf* _check = _pdf != NULL ? fude_pdf_open(_pdf) : NULL;
    if(_check != NULL) {
        fude_pdf_close(_check);
        _id = fude_documents_new_canvas(_app, FUDE_NOTE_DOCUMENT_OWN, _pdf, _name, 0u);
    }
    if(_made[0] != 0) {
        rde_file_delete(_made);
    }
    rde_arr_free(&_pictures);
    return _id;
}

// Takes what is waiting and hands it to the app's own page, or makes it into a canvas, then deletes the copies the
// pickers wrote and says in a notice how it went. See the header.
b8 fude_import_update(fude_app* _app) {
#if defined(RDE_PLATFORM_ANDROID)
    fude_import_platform_poll();
#endif
    if(!fude_import_in.waiting) {
        return false;
    }
    fude_import_in.waiting = false;
    const u32 _count = (u32)rde_arr_length(&fude_import_in.paths);
    if(_count == 0u) {
        return false;   // The picker was cancelled.
    }
    // A plain list of the paths, valid until fude_import_forget.
    rde_arr TYPE(const c8*) _list = rde_arr_new_with_capacity(sizeof(const c8*), _count, rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < _count; _i++) {
        const c8* _path = rde_str_to_char_ptr(&((rde_str*)fude_import_in.paths.memory)[_i]);
        rde_arr_add(&_list, (any)&_path);
    }
    const c8* const* _paths = (const c8* const*)_list.memory;
    // An app with its own page takes them itself (Sketching, for example, puts the pictures on its canvas); otherwise
    // they become a new canvas.
    const fude_page_kind* _kind = fude_app_ext(_app)->page_kind;
    const b8              _own  = _kind != NULL && _kind->imported != NULL;
    c8                    _name[FUDE_NOTE_NAME] = "";
    u32                   _id   = 0u;
    u32                   _made = 0u;
    if(_own) {
        _kind->imported(_app, fude_import_in.kind, _paths, _count);
    } else {
        // Each PDF a canvas of its own, then the pictures together as one: none of what was picked is left out.
        rde_arr TYPE(const c8*) _pictures = rde_arr_new(sizeof(const c8*), rde_memory_allocator_get_default_std());
        for(u32 _i = 0; _i < _count; _i++) {
            if(!fude_import_is_pdf(_paths[_i])) {
                rde_arr_add(&_pictures, (any)&_paths[_i]);
                continue;
            }
            const u32 _one = fude_import_into_canvas(_app, &_paths[_i], 1u, _name, sizeof(_name));
            _made += _one != 0u ? 1u : 0u;
            _id    = _one != 0u ? _one : _id;
        }
        if(rde_arr_length(&_pictures) > 0u) {
            const u32 _one = fude_import_into_canvas(_app, (const c8* const*)_pictures.memory, (u32)rde_arr_length(&_pictures), _name, sizeof(_name));
            _made += _one != 0u ? 1u : 0u;
            _id    = _one != 0u ? _one : _id;
        }
        rde_arr_free(&_pictures);
    }
    // The pictures the pickers wrote are copies, so the app deletes them; files picked from Files are the user's own
    // and are left alone.
    for(u32 _i = 0; fude_import_in.kind != FUDE_IMPORT_FILES && _i < _count; _i++) {
        rde_file_delete(_paths[_i]);
    }
    rde_arr_free(&_list);
    fude_import_forget();
    if(_own) {
        return false;
    }
    if(_id == 0u) {
        fude_notice_show(fude_text(FUDE_TEXT_DOC_CANT_OPEN));
        return false;
    }
    c8 _line[512];
    if(_made == 1u) {
        FUDE_TEXTF(_line, FUDE_TEXT_DOC_IMPORTED, FUDE_TS(_name));
    } else {
        FUDE_TEXTF(_line, FUDE_TEXT_DOC_IMPORTED_N, FUDE_TN(_made));
    }
    fude_notice_show(_line);
    return true;
}

// --- not here -------------------------------------------------------------------------------
// Every platform but iOS (import_ios.m) and Android (import_android.c) imports from Files only: these say there is no
// photo picker or camera, and do nothing.

#if !defined(RDE_PLATFORM_IOS) && !defined(RDE_PLATFORM_ANDROID)

// No Photos library here.
b8 fude_import_platform_photos(void) {
    return false;
}

// No document camera here.
b8 fude_import_platform_camera(void) {
    return false;
}

// Nothing to show.
void fude_import_platform_pick_photos(void) {
}

// Nothing to show.
void fude_import_platform_scan(void) {
}

#endif
