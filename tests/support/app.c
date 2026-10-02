// The study's verbs and the row's faces, for tests that link a screen (its row's
// buttons call them; nothing presses them here).
#include "study/app/study.h"
#include "drawing/base/text.h"
#include <stdio.h>
void fude_study_practice(fude_app* _app, const u32* _records, u32 _count) { (void)_app; (void)_records; (void)_count; }
void fude_study_practice_set(fude_app* _app, const u32* _records, u32 _count) { (void)_app; (void)_records; (void)_count; }
void fude_study_view(fude_app* _app, const u32* _records, u32 _count, u32 _at) { (void)_app; (void)_records; (void)_count; (void)_at; }
void fude_study_write_text(fude_app* _app, const c8* _text, rde_vec_2F _canvas) { (void)_app; (void)_text; (void)_canvas; }
const fude_row_button FUDE_STUDY_SELECT_BUTTONS[FUDE_STUDY_SELECT_COUNT];
void fude_study_select_faces(const fude_selection* _selection, fude_row_face* _faces) { (void)_selection; (void)_faces; }
void fude_row_face_count(fude_row_face* _face, u32 _text, u32 _n) { (void)_text; snprintf(_face->label, sizeof(_face->label), "%u", _n); }
void fude_row_face_next(fude_row_face* _face, b8 _last) { (void)_face; (void)_last; }
// A word asked for on the word card (wordcard.h): nobody to show it to.
__attribute__((weak)) void fude_wordcard_ask_saved(u32 _word) { (void)_word; }
