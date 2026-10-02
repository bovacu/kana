#include "study/app/study.h"
#include "drawing/base/text.h"
#include "drawing/widgets/notice.h"
#include "drawing/widgets/icons.h"
#include "study/models/marks.h"
#include "study/models/review.h"
#include "study/models/vocab.h"

#include <string.h>

// ===========================================================================
// The study's verbs (study.h): the ways from one study screen to another, and
// Select mode's row.
// ===========================================================================

// --- the verbs ----------------------------------------------------------------------------

void fude_study_practice(fude_app* _app, const u32* _records, u32 _count) {
    fude_study* _study = FUDE_STUDY(_app);
    if(_count == 1u) {
        fude_practice_open(_study->practice, _records[0]);
    } else if(_count > 1u) {
        fude_practice_open_set(_study->practice, _records, _count);
    }
}

void fude_study_practice_set(fude_app* _app, const u32* _records, u32 _count) {
    if(_count > 0u) {
        fude_practice_open_set(FUDE_STUDY(_app)->practice, _records, _count);
    }
}

void fude_study_view(fude_app* _app, const u32* _records, u32 _count, u32 _at) {
    if(_count > 0u) {
        fude_viewer_show(FUDE_STUDY(_app)->viewer, _records, _count, _at < _count ? _at : 0u);
    }
}

void fude_study_exam(fude_app* _app, const u32* _records, u32 _count) {
    if(_count > 0u) {
        fude_exam_open_with(FUDE_STUDY(_app)->exam, _records, _count);
    }
}

u32 fude_study_reviews_due(fude_app* _app, u32* _out, u32 _max) {
    fude_study* _study = FUDE_STUDY(_app);
    static u32 _marked[4096];
    u32        _n = fude_marks_list(FUDE_MARK_STUDYING, _marked, 4096u);
    _n += fude_marks_list(FUDE_MARK_KNOWN, _marked + _n, 4096u - _n);
    u32       _due[FUDE_REVIEW_SESSION];
    const u32 _d = fude_reviews_due(_marked, _n, _due, FUDE_REVIEW_SESSION);
    u32       _r = 0;
    for(u32 _i = 0; _i < _d && _r < _max && _study->db != NULL; _i++) {
        if(fude_kanji_find_index(_study->db, _due[_i], &_out[_r])) {
            _r++;
        }
    }
    return _r;
}

b8 fude_study_review(fude_app* _app) {
    u32       _records[FUDE_REVIEW_SESSION];
    const u32 _n = fude_study_reviews_due(_app, _records, FUDE_REVIEW_SESSION);
    if(_n == 0u) {
        fude_notice_show(fude_text(FUDE_TEXT_REVIEWS_NONE));
        return false;
    }
    fude_lasso_clear(_app->lasso, _app->ink);
    fude_exam_open_review(FUDE_STUDY(_app)->exam, _records, _n);
    return true;
}

// --- Select mode's row (Browse's and the chart's) ---------------------------------------
//
// What to do with the ticked characters (select.h). Its owner is whichever
// screen that ticks is on top; All takes what that one has in view.

RDE_INTERNAL void fude_study_select_all(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    const fude_screen_slot* _top = fude_app_top(_app);
    const u32*              _records;
    const u32               _n = _top != NULL && _top->vt->in_view != NULL ? _top->vt->in_view(_top->self, &_records) : 0u;
    if(_n > 0u) {
        fude_selection_add(FUDE_STUDY(_app)->selection, _records, _n);
    }
}

RDE_INTERNAL void fude_study_select_none(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_selection_clear(FUDE_STUDY(_app)->selection);
}

// Study: the ticked marked Studying — or, when every one of them already is, no
// longer marked.
RDE_INTERNAL void fude_study_select_study(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_study* _study = FUDE_STUDY(_app);
    const u32 _count = fude_selection_count(_study->selection);
    if(_count == 0u || _study->db == NULL) {
        return;
    }
    static u32 _cps[16384];
    const u32  _n   = _count < 16384u ? _count : 16384u;
    b8         _all = true;
    for(u32 _i = 0; _i < _n; _i++) {
        fude_kanji_info _info;
        fude_kanji_at(_study->db, fude_selection_records(_study->selection)[_i], &_info);
        _cps[_i] = _info.codepoint;
        _all     = _all && fude_marks_get(_info.codepoint) == FUDE_MARK_STUDYING;
    }
    fude_marks_set_many(_cps, _n, _all ? FUDE_MARK_NONE : FUDE_MARK_STUDYING);
}

// Exam (straight to its preview), Sheet, Practice: the ticked, in the order ticked.
RDE_INTERNAL void fude_study_select_exam(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_study* _study = FUDE_STUDY(_app);
    fude_study_exam(_app, fude_selection_records(_study->selection), fude_selection_count(_study->selection));
}

RDE_INTERNAL void fude_study_select_sheet(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_study* _study = FUDE_STUDY(_app);
    fude_study_sheet(_app, fude_selection_records(_study->selection), fude_selection_count(_study->selection));
}

RDE_INTERNAL void fude_study_select_practice(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_study* _study = FUDE_STUDY(_app);
    fude_study_practice_set(_app, fude_selection_records(_study->selection), fude_selection_count(_study->selection));
}

// Save list: the ticked, in the order ticked, as a new list of the vocabulary
// (vocab.h): each character a word (fude_vocab_add_characters).
RDE_INTERNAL void fude_study_select_list(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    fude_study* _study = FUDE_STUDY(_app);
    if(fude_selection_count(_study->selection) == 0u || _study->db == NULL) {
        return;
    }
    c8        _name[FUDE_VOCAB_LIST_NAME];
    u32       _saved = 0;
    const u32 _list  = fude_vocab_add_characters(_study->db, fude_selection_records(_study->selection), fude_selection_count(_study->selection), _name, sizeof(_name), &_saved);
    if(_list == 0u) {
        fude_notice_show(fude_text(FUDE_TEXT_VOCAB_LISTS_FULL));
        return;
    }
    c8 _line[192];
    FUDE_TEXTF(_line, FUDE_TEXT_VOCAB_LIST_SAVED, FUDE_TN(_saved), FUDE_TS(_name));
    fude_notice_show(_line);
}

// Done: taps open characters again; the ticks stay for next time.
RDE_INTERNAL void fude_study_select_done(fude_app* _app, void* _self, u32 _arg) {
    RDE_UNUSED(_self); RDE_UNUSED(_arg);
    FUDE_STUDY(_app)->selection->active = false;
}

enum { FUDE_STUDY_SELECT_ALL = 0, FUDE_STUDY_SELECT_NONE, FUDE_STUDY_SELECT_STUDY, FUDE_STUDY_SELECT_EXAM, FUDE_STUDY_SELECT_SHEET, FUDE_STUDY_SELECT_LIST,
       FUDE_STUDY_SELECT_PRACTICE, FUDE_STUDY_SELECT_DONE };
const fude_row_button FUDE_STUDY_SELECT_BUTTONS[FUDE_STUDY_SELECT_COUNT] = {
    { FUDE_TEXT_ALL,        FUDE_ICON_SELECT_ALL,  fude_study_select_all,      0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_NONE,       FUDE_ICON_SELECT_NONE, fude_study_select_none,     0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_STUDY,      FUDE_ICON_STAR,        fude_study_select_study,    0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_EXAM,       FUDE_ICON_EXAM,        fude_study_select_exam,     0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_SHEET,      FUDE_ICON_PRINT,       fude_study_select_sheet,    0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_SAVE_LIST,  FUDE_ICON_LISTS,       fude_study_select_list,     0, FUDE_ROW_QUIET,   false, NULL },
    { FUDE_TEXT_PRACTICE_N, FUDE_ICON_PEN,         fude_study_select_practice, 0, FUDE_ROW_PRIMARY, true,  NULL },
    { FUDE_TEXT_DONE,       FUDE_ICON_CHECK,       fude_study_select_done,     0, FUDE_ROW_QUIET,   false, NULL },
};

// "Practice n", the ticked; everything that acts on them greyed out without any.
void fude_study_select_faces(const fude_selection* _selection, fude_row_face* _faces) {
    const u32 _n = fude_selection_count(_selection);
    fude_row_face_count(&_faces[FUDE_STUDY_SELECT_PRACTICE], FUDE_TEXT_PRACTICE_N, _n);
    _faces[FUDE_STUDY_SELECT_PRACTICE].disabled = _n == 0u;
    _faces[FUDE_STUDY_SELECT_NONE].disabled     = _n == 0u;
    _faces[FUDE_STUDY_SELECT_STUDY].disabled    = _n == 0u;
    _faces[FUDE_STUDY_SELECT_EXAM].disabled     = _n == 0u;
    _faces[FUDE_STUDY_SELECT_SHEET].disabled    = _n == 0u;
    _faces[FUDE_STUDY_SELECT_LIST].disabled     = _n == 0u;
}
