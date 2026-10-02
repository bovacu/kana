// The app's verbs and the row's faces, for tests that link a screen (its row's
// buttons call them; nothing presses them here).
#include "app/app.h"
#include "base/text.h"
#include <stdio.h>
void kana_app_practice(kana_app* _app, const u32* _records, u32 _count) { (void)_app; (void)_records; (void)_count; }
void kana_app_practice_set(kana_app* _app, const u32* _records, u32 _count) { (void)_app; (void)_records; (void)_count; }
void kana_app_view(kana_app* _app, const u32* _records, u32 _count, u32 _at) { (void)_app; (void)_records; (void)_count; (void)_at; }
void kana_app_write_text(kana_app* _app, const c8* _text, rde_vec_2F _canvas) { (void)_app; (void)_text; (void)_canvas; }
const kana_row_button KANA_APP_SELECT_BUTTONS[KANA_APP_SELECT_COUNT];
void kana_app_select_faces(const kana_selection* _selection, kana_row_face* _faces) { (void)_selection; (void)_faces; }
void kana_row_face_count(kana_row_face* _face, u32 _text, u32 _n) { (void)_text; snprintf(_face->label, sizeof(_face->label), "%u", _n); }
void kana_row_face_next(kana_row_face* _face, b8 _last) { (void)_face; (void)_last; }
// A word asked for on the word card (wordcard.h): nobody to show it to.
__attribute__((weak)) void kana_wordcard_ask_saved(u32 _word) { (void)_word; }
