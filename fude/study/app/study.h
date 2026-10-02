#ifndef FUDE_STUDY_APP
#define FUDE_STUDY_APP

#include "rde.h"
#include "drawing/app/app.h"
#include "study/widgets/pagetext.h"
#include "study/widgets/wordcard.h"
#include "study/models/sheet.h"
#include "study/models/select.h"
#include "study/chars/kanji.h"
#include "study/chars/catalog.h"
#include "study/screens/viewer.h"
#include "study/screens/browse.h"
#include "study/screens/practice.h"
#include "study/screens/album.h"
#include "study/screens/check.h"
#include "study/screens/exam.h"
#include "study/screens/stats.h"
#include "study/screens/scan.h"
#include "study/screens/vocabview.h"
#include "study/screens/wordexam.h"
#include "study/screens/translator.h"

// ===========================================================================
// The study layer's part of the app: what a study app (Kana; a Mandarin or a
// Korean one) adds to the drawing core through its extension (extension.h) —
// the camera on the toolbar, the page's reading (Copy as text, Translate, Save
// word, Check, Paste text, Text from a photo), the word card, Settings'
// Handwriting, the study files — and the verbs from one study screen to another.
//
// A study app's fude_app is the first member of its fude_study (and that the
// first of the app's own: Kana's kana_app), so every hook, handed the
// fude_app, finds the study's (FUDE_STUDY).
// ===========================================================================

struct fude_ui;

typedef struct fude_study {
    fude_app app;   // first

    fude_kanji_db*   db;             // the character data (NULL: missing — the app still draws)
    fude_catalog*    catalog;        // its parts and shapes (catalog.h)

    // The study's screens (in the app's table too, in its order), and the ticks
    // of Select mode (select.h) the screens that tick share.
    fude_viewer*     viewer;
    fude_browse*     browse;
    fude_practice*   practice;
    fude_album*      album;
    fude_check*      check;
    fude_exam*       exam;
    fude_stats*      stats;
    fude_scan*       scan;
    fude_vocabview*  vocab;
    fude_wordexam*   wordexam;
    fude_translator* translator;     // Into Japanese (the Vocabulary's Translate with Google)
    fude_selection*  selection;

    fude_pagetext  text;             // the page read as text (pagetext.h)
    fude_wordcard  word;             // a word into the vocabulary, over everything (wordcard.h)

    // Settings' Handwriting: Google ML Kit on or off (mlkit.h), and its model.
    rde_ui_label*  hand_label;
    rde_ui_label*  mlkit_label;
    rde_ui_button* mlkit_toggle;
    rde_ui_label*  mlkit_status;
    rde_ui_button* mlkit_download;   // Download / Retry, while there is no model
    i32            _mlkit_shown;     // what the row shows: the state, +100 when off; -1 none yet

    // A practice sheet asked for (fude_study_sheet): made next frame (fude_study_sheet_update).
    u32            sheet[FUDE_SHEET_MAX + 1u];   // one more: too many to fit is told
    u32            sheet_count;

    // The reviews due today, as the side panel counts them (fude_study_reviews_count).
    u32            _reviews_count;
    u32            _reviews_for;     // ...for this marks + reviews revision and day
    b8             _reviews_known;
} fude_study;

// The study a study app's fude_app is the start of.
#define FUDE_STUDY(_app) ((fude_study*)(_app))

// Its own, set up once its app is (the app's fields filled), and let go at the end.
void fude_study_init(fude_study* _study);
void fude_study_destroy(fude_study* _study);
// Once a frame, before the screens': Select mode ended when no screen that ticks
// is open; a better voice offered when the basic one spoke.
void fude_study_update(struct fude_app* _app);

// --- the verbs: from one study screen to another (verbs.c) ------------------------------

// Practice: one character alone, several as a set.
void fude_study_practice(struct fude_app* _app, const u32* _records, u32 _count);
// Practice as a set, even of one (a word's kanji: Next and Finish, the summary).
void fude_study_practice_set(struct fude_app* _app, const u32* _records, u32 _count);
// The viewer on _records[_at], walking them.
void fude_study_view(struct fude_app* _app, const u32* _records, u32 _count, u32 _at);
// An exam of _records, straight to its preview.
void fude_study_exam(struct fude_app* _app, const u32* _records, u32 _count);
// A practice sheet (sheet.h) of _records: shared or saved, next frame (files.c).
void fude_study_sheet(struct fude_app* _app, const u32* _records, u32 _count);
// _text written on the page at _canvas in the characters' own strokes, selected (Paste text's way).
void fude_study_write_text(struct fude_app* _app, const c8* _text, rde_vec_2F _canvas);
// The characters' reviews due today (review.h), of the Studying and Known ones:
// records into _out (at most _max); how many.
u32  fude_study_reviews_due(struct fude_app* _app, u32* _out, u32 _max);
// Them as an exam — or, with none due, a notice saying so (false).
b8   fude_study_review(struct fude_app* _app);

// Select mode's row (select.h), of every screen that ticks (Browse's, the
// chart's): All, None, Study, Exam, Sheet, Save list, Practice n, Done — on the
// ticked characters. Its faces from the ticks.
#define FUDE_STUDY_SELECT_COUNT 8u
extern const fude_row_button FUDE_STUDY_SELECT_BUTTONS[FUDE_STUDY_SELECT_COUNT];
void fude_study_select_faces(const fude_selection* _selection, fude_row_face* _faces);

// The page's menus' study buttons, and what they leave over the page
// (pagetext.h), as the extension's hooks.
void fude_study_menu_update(struct fude_app* _app);
void fude_study_selection_faces(struct fude_app* _app, const fude_row_def* _row, fude_row_face* _faces);
void fude_study_context_faces(struct fude_app* _app, const fude_row_def* _row, fude_row_face* _faces);
void fude_study_page_render(struct fude_app* _app, rde_window* _window);
b8   fude_study_page_press(struct fude_app* _app, rde_vec_2F _screen);
// Its widgets on the UI's canvas (the word card), as the extension's hooks.
void fude_study_ui_build(struct fude_ui* _ui, rde_ui_node* _root);
void fude_study_ui_update(struct fude_ui* _ui, b8 _full);
void fude_study_ui_restyle(struct fude_ui* _ui);
b8   fude_study_ui_hit(const struct fude_ui* _ui, rde_vec_2F _screen, rde_vec_2F _canvas);

// The reviews due today (review.h), counted for the side panel: counted again
// only when a mark, an answer or the day changes.
u32 fude_study_reviews_count(struct fude_app* _app);

// Its files (files.c), as the extension's hooks: the study files opened from
// the save folder _dir; ML Kit's setting into and out of the settings file.
void fude_study_session_open(struct fude_app* _app, const c8* _dir);
void fude_study_settings_gather(const struct fude_app* _app, fude_settings* _settings);
void fude_study_settings_apply(struct fude_app* _app, const fude_settings* _settings);
// A practice sheet of _records at _path (_cut: more were asked for than fit;
// _share: then to the share sheet).
void fude_study_sheet_write(struct fude_app* _app, const u32* _records, u32 _count, b8 _cut, const c8* _path, b8 _share);
// Once a frame: a sheet asked for, shared (a tablet) or saved where chosen (a computer).
void fude_study_sheet_update(struct fude_app* _app);

// Its launch flags, a developer's (look.c; look.h has the core's): read before
// anything is built, the screens they ask for opened before the saves load and
// after, and once a frame (after fude_look_frame) what they ask for at a frame
// of a shot's sequence.
void fude_study_look_args(i32 _argc, c8** _argv);
void fude_study_look_start(struct fude_app* _app);
void fude_study_look_loaded(struct fude_app* _app);
void fude_study_look_frame(struct fude_app* _app);
// --demo[=LANG]: six weeks of a learner's work written into the study files, for
// the store's screenshots (demo.c; a developer's build: nothing elsewhere).
void fude_study_demo(struct fude_app* _app, const c8* _lang);

// Settings' Handwriting (extension.h: a section).
extern const fude_extension_section FUDE_STUDY_HANDWRITING;
// The toolbar's camera: Text from a photo with the camera live (extension.h: a tool).
extern const fude_extension_tool FUDE_STUDY_CAMERA;

// The study's part of a study app's extension (extension.h), first in its
// initializer; the app adds its own (its side panel, its tutorial, its first
// launch):
//   const fude_extension KANA_EXTENSION = { FUDE_STUDY_EXTENSION, .nav = &KANA_NAV, ... };
#define FUDE_STUDY_EXTENSION                                                                                                      \
    .tools = &FUDE_STUDY_CAMERA, .tool_count = 1u,                                                                                \
    .selection_row = &FUDE_PAGETEXT_SELECTION_ROW, .context_row = &FUDE_PAGETEXT_CONTEXT_ROW,                                     \
    .menu_update = fude_study_menu_update, .selection_faces = fude_study_selection_faces, .context_faces = fude_study_context_faces, \
    .sections = &FUDE_STUDY_HANDWRITING, .section_count = 1u,                                                                     \
    .ui_build = fude_study_ui_build, .ui_update = fude_study_ui_update, .ui_restyle = fude_study_ui_restyle,                       \
    .ui_hit = fude_study_ui_hit, .page_render = fude_study_page_render, .page_press = fude_study_page_press,                       \
    .session_open = fude_study_session_open, .settings_gather = fude_study_settings_gather,                                         \
    .settings_apply = fude_study_settings_apply

#endif
