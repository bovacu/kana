// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/doc/import.h"
#include "drawing/doc/doc.h"
#include "drawing/doc/pdf.h"
#include "drawing/app/app.h"
#include "drawing/base/save.h"
#include "drawing/base/text.h"
#include "drawing/ink/notes.h"
#include "drawing/widgets/notice.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ===========================================================================
// See import.h. What came in waits here (copied) until the next update.
// ===========================================================================

#define FUDE_IMPORT_MAX 64u   // files at once, at most (a scan's pages, pictures picked)

RDE_INTERNAL struct {
    b8  waiting;
    u8  kind;     // FUDE_IMPORT_
    u32 count;
    c8* paths[FUDE_IMPORT_MAX];
} fude_import_in;

RDE_INTERNAL void fude_import_forget(void) {
    for(u32 _i = 0; _i < fude_import_in.count; _i++) {
        free(fude_import_in.paths[_i]);
        fude_import_in.paths[_i] = NULL;
    }
    fude_import_in.count   = 0;
    fude_import_in.waiting = false;
}

void fude_import_arrived(u8 _kind, const c8* const* _paths, u32 _count) {
    fude_import_forget();
    fude_import_in.kind    = _kind;
    fude_import_in.waiting = true;
    for(u32 _i = 0; _paths != NULL && _i < _count && fude_import_in.count < FUDE_IMPORT_MAX; _i++) {
        if(_paths[_i] != NULL && _paths[_i][0] != 0) {
            const usize _n = strlen(_paths[_i]) + 1u;
            fude_import_in.paths[fude_import_in.count] = (c8*)malloc(_n);
            memcpy(fude_import_in.paths[fude_import_in.count++], _paths[_i], _n);
        }
    }
}

// --- where from -----------------------------------------------------------------------------

b8 fude_import_photos_available(void) {
    return fude_pdf_available() && fude_import_platform_photos();
}

b8 fude_import_camera_available(void) {
    return fude_pdf_available() && fude_import_platform_camera();
}

RDE_INTERNAL void fude_import_on_files(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter);
    RDE_UNUSED(_user_data);
    if(_paths == NULL) {
        fude_notice_show(fude_text(FUDE_TEXT_DOC_CANT_PICK));
        return;
    }
    fude_import_arrived(FUDE_IMPORT_FILES, _paths, _count);
}

void fude_import_files(fude_app* _app) {
    if(!fude_pdf_available()) {
        fude_notice_show(fude_text(FUDE_TEXT_DOC_NOT_HERE));
        return;
    }
#if defined(RDE_PLATFORM_ANDROID)
    RDE_UNUSED(fude_import_on_files);
    fude_import_platform_pick_files();   // the system's document picker: copies, as the photos'
    return;
#endif
    // A PDF, or pictures (many at once: a page each).
    static const rde_dialog_filter _filters[] = { { "PDF", "pdf" }, { "Pictures", "jpg;jpeg;png;heic;heif;tif;tiff" } };
    rde_dialog_open_file(_app->window, _filters, 2u, NULL, true, fude_import_on_files, _app);
}

void fude_import_photos(fude_app* _app) {
    RDE_UNUSED(_app);
    if(fude_import_photos_available()) {
        fude_import_platform_pick_photos();
    }
}

void fude_import_camera(fude_app* _app) {
    RDE_UNUSED(_app);
    if(fude_import_camera_available()) {
        fude_import_platform_scan();
    }
}

// --- into a canvas --------------------------------------------------------------------------

RDE_INTERNAL b8 fude_import_is_pdf(const c8* _path) {
    const usize _n = strlen(_path);
    return _n > 4u && (strcmp(&_path[_n - 4u], ".pdf") == 0 || strcmp(&_path[_n - 4u], ".PDF") == 0);
}

// A file's name without its folder and extension, into _out.
RDE_INTERNAL void fude_import_stem(const c8* _path, c8* _out, usize _size) {
    const c8* _slash = strrchr(_path, '/');
    const c8* _back  = strrchr(_path, '\\');
    const c8* _name  = _back != NULL && (_slash == NULL || _back > _slash) ? _back + 1 : _slash != NULL ? _slash + 1 : _path;
#if defined(RDE_PLATFORM_ANDROID)
    // (The picker's copies are named "<n>_<its name>" (FudeImport.java): the name only.)
    const c8* _digits = _name;
    while(*_digits >= '0' && *_digits <= '9') {
        _digits++;
    }
    _name = _digits > _name && *_digits == '_' && _digits[1] != 0 ? _digits + 1 : _name;
#endif
    snprintf(_out, _size, "%s", _name);
    c8* _dot = strrchr(_out, '.');
    if(_dot != NULL && _dot != _out) {
        *_dot = 0;
    }
}

b8 fude_import_update(fude_app* _app) {
#if defined(RDE_PLATFORM_ANDROID)
    fude_import_platform_poll();
#endif
    if(!fude_import_in.waiting) {
        return false;
    }
    fude_import_in.waiting = false;
    if(fude_import_in.count == 0u) {
        return false;   // cancelled
    }
    // The app's own page takes them (Sketching: pictures on the canvas).
    const fude_page_kind* _kind = fude_app_ext(_app)->page_kind;
    if(_kind != NULL && _kind->imported != NULL) {
        _kind->imported(_app, fude_import_in.kind, (const c8* const*)fude_import_in.paths, fude_import_in.count);
        for(u32 _i = 0; fude_import_in.kind != FUDE_IMPORT_FILES && _i < fude_import_in.count; _i++) {
            rde_file_delete(fude_import_in.paths[_i]);
        }
        fude_import_forget();
        return false;
    }
    // A PDF as it is (the first, if pictures came with it); pictures made into one.
    const c8* _pdf = NULL;
    const c8* _pictures[FUDE_IMPORT_MAX];
    u32       _picture_count = 0;
    for(u32 _i = 0; _i < fude_import_in.count; _i++) {
        if(fude_import_is_pdf(fude_import_in.paths[_i])) {
            _pdf = _pdf != NULL ? _pdf : fude_import_in.paths[_i];
        } else {
            _pictures[_picture_count++] = fude_import_in.paths[_i];
        }
    }
    c8 _name[FUDE_NOTE_NAME];
    c8 _made[RDE_MAX_PATH] = "";
    if(_pdf != NULL) {
        fude_import_stem(_pdf, _name, sizeof(_name));
    } else {
        snprintf(_made, sizeof(_made), "%simport.pdf", fude_save_dir());
        if(!fude_pdf_from_images(_pictures, _picture_count, _made)) {
            fude_notice_show(fude_text(FUDE_TEXT_DOC_CANT_OPEN));
            fude_import_forget();
            return false;
        }
        _pdf = _made;
        if(fude_import_in.kind == FUDE_IMPORT_FILES) {
            fude_import_stem(_pictures[0], _name, sizeof(_name));
        } else {
            c8 _date[48];
            fude_text_date(_date, sizeof(_date), (u64)time(NULL));
            FUDE_TEXTF(_name, fude_import_in.kind == FUDE_IMPORT_CAMERA ? FUDE_TEXT_DOC_SCAN_NAME : FUDE_TEXT_DOC_PHOTOS_NAME, FUDE_TS(_date));
        }
    }
    // It has to open (a damaged file, a picture that was not one).
    fude_pdf* _check = fude_pdf_open(_pdf);
    u32       _id    = 0u;
    if(_check != NULL) {
        fude_pdf_close(_check);
        _id = fude_doc_new_canvas(_app, FUDE_NOTE_DOCUMENT_OWN, _pdf, _name, 0u);
    }
    if(_made[0] != 0) {
        rde_file_delete(_made);
    }
    // The pictures the pickers wrote for it are the app's to delete (Files' are the learner's).
    for(u32 _i = 0; fude_import_in.kind != FUDE_IMPORT_FILES && _i < fude_import_in.count; _i++) {
        rde_file_delete(fude_import_in.paths[_i]);
    }
    fude_import_forget();
    if(_id == 0u) {
        fude_notice_show(fude_text(FUDE_TEXT_DOC_CANT_OPEN));
        return false;
    }
    c8 _line[512];
    FUDE_TEXTF(_line, FUDE_TEXT_DOC_IMPORTED, FUDE_TS(_name));
    fude_notice_show(_line);
    return true;
}

// --- not here -------------------------------------------------------------------------------
// Every platform but iOS (import_ios.m) and Android (import_android.c): Files only.

#if !defined(RDE_PLATFORM_IOS) && !defined(RDE_PLATFORM_ANDROID)

b8 fude_import_platform_photos(void) {
    return false;
}

b8 fude_import_platform_camera(void) {
    return false;
}

void fude_import_platform_pick_photos(void) {
}

void fude_import_platform_scan(void) {
}

#endif
