#include "study/app/study.h"
#include "drawing/app/session.h"
#include "drawing/base/text.h"
#include "drawing/widgets/notice.h"
#include "study/services/mlkit.h"
#include "study/models/marks.h"
#include "study/models/examlog.h"
#include "study/models/vocab.h"
#include "study/models/review.h"
#include "study/models/charnote.h"
#include "study/models/sheet.h"

#include <stdio.h>
#include <string.h>

// ===========================================================================
// The study's files (study.h): what it keeps in the save folder, its setting in
// the settings file, and the practice sheets that leave the app as PDFs.
// ===========================================================================

// --- the session's (extension.h) ---------------------------------------------------------

// The study files: marks, exams, words, reviews, characters' notes.
void fude_study_session_open(fude_app* _app, const c8* _dir) {
    RDE_UNUSED(_app);
    static const struct { const c8* name; void (*open)(const c8*); } _files[] = {
        { "marks.kana", fude_marks_open }, { "exams.kana", fude_examlog_open }, { "words.kana", fude_vocab_open },
        { "reviews.kana", fude_reviews_open }, { "charnotes.kana", fude_charnotes_open },
    };
    for(u32 _i = 0; _i < sizeof(_files) / sizeof(_files[0]); _i++) {
        c8 _path[RDE_MAX_PATH];
        snprintf(_path, sizeof(_path), "%s%s", _dir, _files[_i].name);
        _files[_i].open(_path);
    }
}

// Google ML Kit on or off (Settings' Handwriting).
void fude_study_settings_gather(const fude_app* _app, fude_settings* _settings) {
    RDE_UNUSED(_app);
    _settings->mlkit = fude_mlkit_enabled();
}

void fude_study_settings_apply(fude_app* _app, const fude_settings* _settings) {
    RDE_UNUSED(_app);
    fude_mlkit_set_enabled(_settings->mlkit);
}

// --- practice sheets -----------------------------------------------------------------------

void fude_study_sheet(fude_app* _app, const u32* _records, u32 _count) {
    fude_study* _study = FUDE_STUDY(_app);
    const u32   _keep  = _count < FUDE_SHEET_MAX + 1u ? _count : FUDE_SHEET_MAX + 1u;   // one more than fits: too many is told
    memcpy(_study->sheet, _records, sizeof(u32) * _keep);
    _study->sheet_count = _keep;
}

void fude_study_sheet_write(fude_app* _app, const u32* _records, u32 _count, b8 _cut, const c8* _path, b8 _share) {
    fude_sheet_info _info;
    if(!fude_sheet_write(FUDE_STUDY(_app)->db, _records, _count, _path, &_info) || (_share && !rde_mobile_share_file(_path, "application/pdf", fude_text(FUDE_TEXT_SHEET_TITLE)))) {
        fude_notice_show(fude_text(FUDE_TEXT_SHEET_FAILED));
        return;
    }
    c8 _line[400];
    if(_cut) {
        FUDE_TEXTF(_line, FUDE_TEXT_SHEET_FIRST_N, FUDE_TN(FUDE_SHEET_MAX));
        fude_notice_show(_line);
    } else if(!_share) {
        FUDE_TEXTF(_line, FUDE_TEXT_SHEET_SAVED, FUDE_TS(_path));
        fude_notice_show(_line);
    }
    rde_log_level(RDE_LOG_LEVEL_INFO, "kana: practice sheet, %u characters on %u pages (%u bytes): %s", _info.characters, _info.pages, _info.bytes, _path);
}

#if !defined(RDE_PLATFORM_MOBILE)
// The desktop: where to save it chosen; the sheet asked for kept till then.
RDE_INTERNAL u32 fude_study_sheet_records[FUDE_SHEET_MAX];
RDE_INTERNAL u32 fude_study_sheet_count;
RDE_INTERNAL b8  fude_study_sheet_cut;

RDE_INTERNAL void fude_study_on_sheet_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter);
    fude_app* _app = (fude_app*)_user_data;
    if(_paths == NULL) {
        fude_notice_show(fude_text(FUDE_TEXT_SHEET_FAILED));
        return;
    }
    if(_count == 0) {
        return;   // cancelled
    }
    c8 _path[RDE_MAX_PATH];
    fude_session_with_extension(_path, sizeof(_path), _paths[0], ".pdf", ".PDF");
    fude_study_sheet_write(_app, fude_study_sheet_records, fude_study_sheet_count, fude_study_sheet_cut, _path, false);
}
#endif

// Once a frame: a sheet asked for (fude_study_sheet) — shared, or (the desktop)
// where to save it asked.
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
    memcpy(fude_study_sheet_records, _study->sheet, sizeof(u32) * _keep);
    fude_study_sheet_count = _keep;
    fude_study_sheet_cut   = _cut;
    static const rde_dialog_filter _filter = { "PDF", "pdf" };
    rde_dialog_save_file(_app->window, &_filter, 1, NULL, fude_study_on_sheet_path, _app);
#endif
}
