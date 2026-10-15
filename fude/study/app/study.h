// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_STUDY_APP
#define FUDE_STUDY_APP

#include "rde.h"
#include "drawing/app/app.h"
#include "study/widgets/pagetext.h"
#include "study/widgets/wordcard.h"
#include "study/models/sheet.h"
#include "study/models/select.h"
#include "study/app/welcome.h"
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
#include "study/screens/chart.h"
#include "drawing/app/library.h"

// ===========================================================================
// Study: the study layer's part of a study app. It is what a study app (Kana, Hanzi for Mandarin, Hangul for Korean,
// and the others) adds to the drawing core through its extension (extension.h): the camera on the toolbar, the
// page's reading (Copy as text, Translate, Save word, Check, Paste text, Text from a photo), the word card,
// Settings' Handwriting section and the study files. It also holds the verbs that lead from one study screen to
// another.
//
// Where it sits: the study layer (study/app). The study shell (shell.h) makes the fude_study and hands RDE's calls
// to it; the study screens and widgets reach the study through it, and each app's extension starts with
// FUDE_STUDY_EXTENSION.
// Its main types and their lifecycle: a study app's fude_app is the first member of its fude_study, so every hook,
// which is handed the fude_app, finds the study's part with FUDE_STUDY. The shell owns the one fude_study, and the
// screens it points to, as statics (shell.c): it fills it in, calls fude_study_init once the app is set up, and
// fude_study_destroy at the end.
// Threads: everything here runs on the app's main thread. Several calls use static buffers, so none may be called
// from another thread. The services it uses (ML Kit, text in photos, translation, speech) answer asynchronously and
// are polled once a frame.
// Files: in the save folder it opens the study files marks.kana, exams.kana, words.kana, reviews.kana and
// charnotes.kana (files.c; each model's header describes its format), and writes practice sheets as PDF files
// (sheet.h). ML Kit's switch is the core's setting (drawing/app/handwriting.h).
// Design: the hooks the extension needs are plain functions here, gathered by FUDE_STUDY_EXTENSION, so that each
// app's extension is the study's part plus its own side panel and lectures.
// ===========================================================================

struct fude_ui;

typedef struct fude_study {
    fude_app app;   // first, so that FUDE_STUDY can turn the app into its study

    // The character data (NULL when it is missing: the app still draws). Not const, because the meanings' language
    // is set through it (fude_kanji_set_language: the demo's); the screens take it const.
    fude_kanji_db*   db;
    fude_catalog*    catalog;        // its characters to filter, sort and search (catalog.h; Browse's)

    // The study's screens (they are in the app's table too, in its order), and the ticks of Select mode (select.h),
    // which the screens that tick share.
    fude_viewer*     viewer;
    fude_browse*     browse;
    fude_chart*      chart;          // the language's letters (lang.h: fude_lang_chart; empty without one)
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
    fude_welcome*    welcome;        // the welcome (with its readcard), the tutorial and the voice card (welcome.h)
    fude_library*    library;        // the lectures and the learner's documents (app/library.h)

    fude_pagetext  text;             // the page read as text (pagetext.h)
    fude_wordcard  word;             // a word going into the vocabulary, over everything (wordcard.h)

    // A practice sheet asked for (fude_study_sheet), made next frame (fude_study_sheet_update).
    u32            sheet[FUDE_SHEET_MAX + 1u];   // one more than fits, so that asking for too many can be told
    u32            sheet_count;

    // The reviews due today, as the side panel counts them (fude_study_reviews_count).
    u32            _reviews_count;
    u32            _reviews_for[3];  // ...counted for these revisions of the marks and reviews, and this day
    b8             _reviews_known;
} fude_study;

// The study whose first member is a study app's fude_app.
#define FUDE_STUDY(_app) ((fude_study*)(_app))

// Sets up the study's own parts (the page read as text), once its app is set up with its fields filled in; and lets
// go of them at the end.
void fude_study_init(fude_study* _study);
void fude_study_destroy(fude_study* _study);
// Once a frame, before the screens' update: ends Select mode when no screen that ticks is open, offers a better voice
// when the basic one has spoken, and shows the voice card when it is due (welcome.h).
void fude_study_update(struct fude_app* _app);

// --- the verbs: from one study screen to another (verbs.c) ------------------------------

// Opens Practice: on one character alone when _count is 1, on several as a set. Nothing happens for 0.
void fude_study_practice(struct fude_app* _app, const u32* _records, u32 _count);
// Opens Practice as a set, even of one character (a word's kanji: with Next and Finish, and the summary).
void fude_study_practice_set(struct fude_app* _app, const u32* _records, u32 _count);
// Opens the viewer on _records[_at] (the first when _at is out of range), able to walk through all of _records.
void fude_study_view(struct fude_app* _app, const u32* _records, u32 _count, u32 _at);
// Opens an exam of _records, straight to its preview.
void fude_study_exam(struct fude_app* _app, const u32* _records, u32 _count);
// Asks for a practice sheet (sheet.h) of _records: it is made next frame, then shared or saved (files.c). Only the
// first FUDE_SHEET_MAX fit; the rest are left out, and a notice says so.
void fude_study_sheet(struct fude_app* _app, const u32* _records, u32 _count);
// Writes _text on the page at _canvas, in the characters' own strokes, and leaves it selected (as Paste text does).
void fude_study_write_text(struct fude_app* _app, const c8* _text, rde_vec_2F _canvas);
// Finds the characters whose reviews are due today (review.h), among those marked Studying or Known. Their record
// numbers go into _out (at most _max of them); returns how many.
u32  fude_study_reviews_due(struct fude_app* _app, u32* _out, u32 _max);
// Opens the reviews due today as an exam and returns true; with none due, it shows a notice instead and returns false.
b8   fude_study_review(struct fude_app* _app);

// Select mode's row (select.h), shared by every screen that ticks (Browse, the chart): All, None, Study, Exam, Sheet,
// Save list, Practice n and Done, which act on the ticked characters.
#define FUDE_STUDY_SELECT_COUNT 8u
extern const fude_row_button FUDE_STUDY_SELECT_BUTTONS[FUDE_STUDY_SELECT_COUNT];
// Sets the row's faces from the ticks: Practice shows how many are ticked, and every button that acts on them is
// greyed out when none is.
void fude_study_select_faces(const fude_selection* _selection, fude_row_face* _faces);

// The study buttons in the page's menus, and what they leave over the page (pagetext.h), as the extension's hooks.
// fude_study_page_press acts on a press on the translation card (its speaker, or a word on it).
void fude_study_menu_update(struct fude_app* _app);
void fude_study_selection_faces(struct fude_app* _app, const fude_row_def* _row, fude_row_face* _faces);
void fude_study_context_faces(struct fude_app* _app, const fude_row_def* _row, fude_row_face* _faces);
void fude_study_page_render(struct fude_app* _app, rde_window* _window);
void fude_study_page_press(struct fude_app* _app, rde_vec_2F _screen);
// Its widgets on the UI's canvas (the word card), as the extension's hooks. fude_study_ui_forget lets go of the word
// card's own memory before the canvas goes. fude_study_ui_hit returns true when the point is on the translation card,
// or anywhere while the word card is open (it takes the whole screen).
void fude_study_ui_build(struct fude_ui* _ui, rde_ui_node* _root);
void fude_study_ui_forget(struct fude_ui* _ui);
void fude_study_ui_update(struct fude_ui* _ui, b8 _full);
void fude_study_ui_restyle(struct fude_ui* _ui);
b8   fude_study_ui_hit(const struct fude_ui* _ui, rde_vec_2F _screen, rde_vec_2F _canvas);

// Returns how many reviews are due today (review.h), for the side panel's count. It counts them again only when a
// mark, an answer or the day changes, because the panel asks every frame.
u32 fude_study_reviews_count(struct fude_app* _app);

// Its files (files.c), as the extension's hook: opens the study files in the save folder _dir (which ends in a
// separator).
void fude_study_session_open(struct fude_app* _app, const c8* _dir);
// Reading pictures (extension.h: a document's scanned pages) with ML Kit's text recognition (textscan.h), as the
// extension's hooks.
// fude_study_picture_read starts reading _rgba (_w x _h, 4 bytes a pixel, the top row first), and returns false when
// it cannot now. fude_study_picture_lines returns true once the lines are in, every one of them added to _out (an
// rde_arr of doc.h's fude_doc_line, the caller's): their boxes in the picture's pixels, and their text. A debug build
// can make lines up instead (fude_study_picture_demo, from --read-demo: four lines on every page), to look at it
// without a device; a release build ignores fude_study_picture_demo.
b8   fude_study_picture_read(const u8* _rgba, u32 _w, u32 _h);
b8   fude_study_picture_lines(rde_arr* _out);
void fude_study_picture_demo(b8 _on);
// Writes a practice sheet of _records as a PDF at _path. _cut says more were asked for than fit, and a notice then
// says only the first ones are on it. _share hands the file to the share sheet; without it, a notice says where it
// was saved (the file's name), and that only the first ones are on it when _cut. A notice also says when writing or
// sharing fails.
void fude_study_sheet_write(struct fude_app* _app, const u32* _records, u32 _count, b8 _cut, const c8* _path, b8 _share);
// Once a frame: makes the sheet asked for, if any. A tablet writes it to the outbox and shares it; a computer asks
// where to save it first.
void fude_study_sheet_update(struct fude_app* _app);

// Its launch flags, a developer's (look.c; look.h has the core's). fude_study_look_args reads them before anything is
// built; fude_study_look_start opens the screens they ask for before the saves load, and fude_study_look_loaded the
// ones that need the saves, after; fude_study_look_frame, once a frame after fude_look_frame, does what they ask for
// at a given frame of a shot's sequence. _argv must stay valid for the whole run.
void fude_study_look_args(i32 _argc, c8** _argv);
void fude_study_look_start(struct fude_app* _app);
void fude_study_look_loaded(struct fude_app* _app);
void fude_study_look_frame(struct fude_app* _app);
// --demo[=LANG]: writes six weeks of a learner's work into the study files, for the store's screenshots (demo.c).
// _lang is en, es, pt, ja or fr (NULL or "": the app's language). Only a developer's (debug) build does anything; it
// needs the character data, and adds to whatever the saves hold.
void fude_study_demo(struct fude_app* _app, const c8* _lang);

// Settings' sections (extension.h): Handwriting, the core's (drawing/app/handwriting.h) in the study's words.
#define FUDE_STUDY_SECTION_COUNT 1u
extern const fude_extension_section FUDE_STUDY_SECTIONS[FUDE_STUDY_SECTION_COUNT];
// The toolbar's camera: Text from a photo with the camera live straight away (extension.h: a tool). It shows only
// where text can be read in pictures (textscan.h).
extern const fude_extension_tool FUDE_STUDY_CAMERA;
// The side panel's Set up the voice button, shown while the device has no voice for the language (speech.h); it
// opens the voice card (welcome.h).
extern const fude_extension_side_button FUDE_STUDY_VOICE_BUTTON;

// Opens the welcome at its first page: what the app is, and its gestures. It is the side panel's Tutorial, and what
// the very first launch shows (extension.h: tutorial, first_launch).
void fude_study_tutorial(struct fude_app* _app);

// The study's part of a study app's extension (extension.h), first in its initializer; the app adds its own (its
// side panel, its lectures):
//   static const fude_extension KANA_EXTENSION = { FUDE_STUDY_EXTENSION, .nav = &KANA_NAV, ... };
#define FUDE_STUDY_EXTENSION                                                                                                      \
    .tools = &FUDE_STUDY_CAMERA, .tool_count = 1u,                                                                                \
    .selection_row = &FUDE_PAGETEXT_SELECTION_ROW, .context_row = &FUDE_PAGETEXT_CONTEXT_ROW, .text_row = &FUDE_PAGETEXT_TEXT_ROW, \
    .menu_update = fude_study_menu_update, .selection_faces = fude_study_selection_faces, .context_faces = fude_study_context_faces, \
    .sections = FUDE_STUDY_SECTIONS, .section_count = FUDE_STUDY_SECTION_COUNT, .side_button = &FUDE_STUDY_VOICE_BUTTON,          \
    .ui_build = fude_study_ui_build, .ui_forget = fude_study_ui_forget, .ui_update = fude_study_ui_update,                         \
    .ui_restyle = fude_study_ui_restyle,                                                                                          \
    .ui_hit = fude_study_ui_hit, .page_render = fude_study_page_render, .page_press = fude_study_page_press,                       \
    .session_open = fude_study_session_open,                                                                                      \
    .picture_read = fude_study_picture_read, .picture_lines = fude_study_picture_lines,                                        \
    .tutorial = fude_study_tutorial, .first_launch = fude_study_tutorial

#endif
