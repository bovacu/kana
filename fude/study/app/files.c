// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/app/study.h"
#include "drawing/base/redraw.h"
#include "drawing/app/session.h"
#include "drawing/app/import.h"
#include "drawing/base/text.h"
#include "drawing/widgets/notice.h"
#include "study/models/marks.h"
#include "study/models/examlog.h"
#include "study/models/vocab.h"
#include "study/models/review.h"
#include "study/models/charnote.h"
#include "study/models/sheet.h"

// ===========================================================================
// See study.h. This file holds the study's files: what it keeps in the save folder, and the practice sheets that
// leave the app as PDF files.
//
// A practice sheet is asked for in one frame and made in the next (fude_study_sheet_update), so that a button's
// press returns at once. On a computer the save dialog answers later, through a callback, so the characters asked for
// are kept in statics here until it does.
// ===========================================================================

// --- the session's hooks (extension.h) ---------------------------------------------------

// Opens the study files in the save folder _dir: the marks, the exams, the words, the reviews and the characters'
// notes. Each model's header describes its file's format.
void fude_study_session_open(fude_app* _app, const c8* _dir) {
    RDE_UNUSED(_app);
    static const struct { const c8* name; void (*open)(const c8*); } _files[] = {
        { "marks.kana", fude_marks_open }, { "exams.kana", fude_examlog_open }, { "words.kana", fude_vocab_open },
        { "reviews.kana", fude_reviews_open }, { "charnotes.kana", fude_charnotes_open },
    };
    for(u32 _i = 0; _i < sizeof(_files) / sizeof(_files[0]); _i++) {
        c8 _path[RDE_MAX_PATH];
        rde_snprintf(_path, sizeof(_path), "%s%s", _dir, _files[_i].name);
        _files[_i].open(_path);
    }
}

// --- practice sheets -----------------------------------------------------------------------

// Keeps the sheet asked for until the next frame. One more record than fits is kept, so that
// fude_study_sheet_update can tell that too many were asked for.
void fude_study_sheet(fude_app* _app, const u32* _records, u32 _count) {
    fude_study* _study = FUDE_STUDY(_app);
    const u32   _keep  = _count < FUDE_SHEET_MAX + 1u ? _count : FUDE_SHEET_MAX + 1u;   // one more than fits
    rde_memcpy(_study->sheet, _records, sizeof(u32) * _keep);
    _study->sheet_count = _keep;
}

// Writes the sheet and says how it went (see the header). Saved on a computer, the notice names the file, without the
// folder the user has just chosen, so that the one line it has holds it; and when only the first characters went in,
// that same notice says so. Shared, the share sheet shows where it goes, and a notice says only that it was cut. The
// log line gives its size, for a developer.
void fude_study_sheet_write(fude_app* _app, const u32* _records, u32 _count, b8 _cut, const c8* _path, b8 _share) {
    fude_sheet_info _info;
    if(!fude_sheet_write(FUDE_STUDY(_app)->db, _records, _count, _path, _app->info->name, &_info) || (_share && !rde_mobile_share_file(_path, "application/pdf", fude_text(FUDE_TEXT_SHEET_TITLE)))) {
        fude_notice_show(fude_text(FUDE_TEXT_SHEET_FAILED));
        return;
    }
    c8 _line[512];
    if(!_share) {
        c8 _name[RDE_MAX_PATH];
        fude_import_name(_path, _name, sizeof(_name));   // The file's name, without its folder and its .pdf.
        if(_cut) {
            FUDE_TEXTF(_line, FUDE_TEXT_SHEET_SAVED_FIRST_N, FUDE_TN(FUDE_SHEET_MAX), FUDE_TS(_name));
        } else {
            FUDE_TEXTF(_line, FUDE_TEXT_SHEET_SAVED, FUDE_TS(_name));
        }
        fude_notice_show(_line);
    } else if(_cut) {
        FUDE_TEXTF(_line, FUDE_TEXT_SHEET_FIRST_N, FUDE_TN(FUDE_SHEET_MAX));
        fude_notice_show(_line);
    }
    rde_log_level(RDE_LOG_LEVEL_INFO, "sheet: practice sheet, %u characters on %u pages (%u bytes): %s", _info.characters, _info.pages, _info.bytes, _path);
}

#if !defined(RDE_PLATFORM_MOBILE)
// On a computer, the user chooses where to save the sheet first: the sheet asked for is kept here until the dialog
// answers.
RDE_INTERNAL u32 fude_study_sheet_records[FUDE_SHEET_MAX];
RDE_INTERNAL u32 fude_study_sheet_count;
RDE_INTERNAL b8  fude_study_sheet_cut;

// The save dialog's answer: NULL paths when it failed, none when it was cancelled. The file gets a .pdf extension
// when it has none.
RDE_INTERNAL void fude_study_on_sheet_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter);
    fude_app* _app = (fude_app*)_user_data;
    fude_redraw_request();   // (render on demand: a dialog's answer may come while the app sleeps, with no event)
    if(_paths == NULL) {
        fude_notice_show(fude_text(FUDE_TEXT_SHEET_FAILED));
        return;
    }
    if(_count == 0) {
        return;   // the user cancelled
    }
    c8 _path[RDE_MAX_PATH];
    fude_session_with_extension(_path, sizeof(_path), _paths[0], ".pdf", ".PDF");
    fude_study_sheet_write(_app, fude_study_sheet_records, fude_study_sheet_count, fude_study_sheet_cut, _path, false);
}
#endif

// Once a frame: makes the sheet asked for (fude_study_sheet), if any. A tablet writes it to the outbox and shares it;
// a computer asks where to save it first.
void fude_study_sheet_update(fude_app* _app) {
    fude_study* _study = FUDE_STUDY(_app);
    const u32   _count = _study->sheet_count;
    if(_count == 0u) {
        return;
    }
    _study->sheet_count = 0u;
    const b8  _cut  = _count > FUDE_SHEET_MAX;
    const u32 _keep = _cut ? FUDE_SHEET_MAX : _count;
#if defined(RDE_PLATFORM_MOBILE)
    c8 _path[RDE_MAX_PATH];
    fude_session_outbox_path(_path, sizeof(_path), fude_text(FUDE_TEXT_SHEET_TITLE), "pdf");
    fude_study_sheet_write(_app, _study->sheet, _keep, _cut, _path, true);
#else
    rde_memcpy(fude_study_sheet_records, _study->sheet, sizeof(u32) * _keep);
    fude_study_sheet_count = _keep;
    fude_study_sheet_cut   = _cut;
    static const rde_dialog_filter _filter = { "PDF", "pdf" };
    rde_dialog_save_file(_app->window, &_filter, 1, NULL, fude_study_on_sheet_path, _app);
#endif
}
