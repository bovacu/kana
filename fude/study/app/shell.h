// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_STUDY_SHELL_H
#define FUDE_STUDY_SHELL_H

#include "rde.h"
#include "drawing/app/app.h"
#include "study/chars/bake.h"

// ===========================================================================
// Study shell: the program every study app runs. It answers what RDE calls (init, the events, each frame's update
// and render, the end) and owns everything the app is made of: the page, the character data and every study screen.
//
// Where it sits: the top of the study layer (study/app). It is the same for every study app (one for each language
// taught): an app is only a description of itself in a fude_study_shell_def (what it is, its side panel, its lectures),
// and its main() hands that to fude_study_shell_run.
// Its main types and their lifecycle: everything the app owns is a static in shell.c, made once when RDE calls its
// init and destroyed when RDE ends it, so there is one app per process. The fude_study_shell_def belongs to the app
// and must stay valid for the whole run (it is normally a static const).
// Threads: everything here runs on the app's main thread, inside rde_run.
// Files: it reads the app's config (./assets/config.rdef, beside the assets, or bake.config when running the data
// bake) and the baked character data (kanji.h: FUDE_KANJI_FILE). The saves belong to the session (session.h).
// Design: app.h describes how the app is put together. The page lies at the bottom (page.h: writing on it, moving
// it). The screens stack over it in a fixed order (FUDE_STUDY_SCREEN_), and the first one open is on top: only it
// gets the pointer, the frame and Escape, and only its row of buttons shows (row.h). The retained UI (the toolbar,
// the page's menus, the screens' rows, the side panel, the word card) is one canvas (ui.h). What is saved, and what
// leaves the app as a file, belongs to the session (session.h). A developer's launch flags and measurements belong
// to look.h, and the study's own to study.h.
// ===========================================================================

// Every screen, in the order they stack: the first one open is on top, and gets the pointer, its row and the frame.
// The page lies under them all.
typedef enum {
    FUDE_STUDY_SCREEN_READCARD = 0,   // a must-read card (readcard.h): over everything, the welcome too
    FUDE_STUDY_SCREEN_WELCOME,        // over everything (the first time, and from the side panel's Tutorial)
    FUDE_STUDY_SCREEN_PRACTICE,       // over whatever opened it
    FUDE_STUDY_SCREEN_VIEWER,         // over the lists, an exam's results, Check and Statistics
    FUDE_STUDY_SCREEN_SCAN,
    FUDE_STUDY_SCREEN_WORDEXAM,       // over the Vocabulary
    FUDE_STUDY_SCREEN_EXAM,           // over the album (to show a kept exam)
    FUDE_STUDY_SCREEN_STATS,
    FUDE_STUDY_SCREEN_TRANSLATOR,     // over the Vocabulary (its Translate with Google, into the language taught)
    FUDE_STUDY_SCREEN_VOCAB,
    FUDE_STUDY_SCREEN_CHECK,
    FUDE_STUDY_SCREEN_LIBRARY,        // the lectures and the learner's documents (app/library.h)
    FUDE_STUDY_SCREEN_ALBUM,
    FUDE_STUDY_SCREEN_CHART,          // the language's letters (lang.h: fude_lang_chart; Chinese has none)
    FUDE_STUDY_SCREEN_BROWSE,
    FUDE_STUDY_SCREEN_COUNT
} FUDE_STUDY_SCREEN_;

// A study app: what makes it itself.
typedef struct {
    const fude_app_info*  info;          // what it is: its name, fonts, licences, credits (info.h)
    const fude_extension* ext;           // what it adds: FUDE_STUDY_EXTENSION, its side panel, its lectures
    fude_bake_app         bake;          // what its data bake needs, from the project root where it runs (bake.h):
                                         // its config, where the character data goes, its tools, its strokes' notice
} fude_study_shell_def;

// Runs the app described by _def (through rde_run), from its main(), and returns rde_run's result. With --bake it
// runs the offline data bake instead (bake.h). A release build ignores its launch arguments, unless it is built with
// FUDE_ALLOW_ARGS to measure a release on a device.
i32  fude_study_shell_run(const fude_study_shell_def* _def, i32 _argc, c8** _argv);

// Opens one of the side panel's screens over the page, first letting go of the lasso's selection. Browse and the
// chart are one or the other: opening one closes the other. Only Browse, the chart, the exams, the Vocabulary,
// Statistics, the album and the Library open this way; any other _screen does nothing.
void fude_study_open(fude_app* _app, FUDE_STUDY_SCREEN_ _screen);

// The side panel's Study entries, for an app's table (extension.h: fude_extension_nav_entry).
// fude_study_nav_open opens the screen _screen (a FUDE_STUDY_SCREEN_) and returns true, so the panel closes.
// fude_study_nav_review opens the reviews due today as an exam (_arg is unused); with none due it shows a notice
// instead and returns false, so the panel stays open. The _has_ hooks say whether the character data, the
// language's chart and the album are there; without them, the entries that need them are greyed out.
b8   fude_study_nav_open(fude_app* _app, u32 _screen);
b8   fude_study_nav_review(fude_app* _app, u32 _arg);
b8   fude_study_nav_has_data(const fude_app* _app);
b8   fude_study_nav_has_chart(const fude_app* _app);
b8   fude_study_nav_has_album(const fude_app* _app);

#endif
