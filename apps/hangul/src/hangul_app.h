#ifndef HANGUL_APP_H
#define HANGUL_APP_H

#include "rde.h"
#include "study/app/study.h"
#include "lang/ko/chart.h"
#include "study/app/welcome.h"
#include "drawing/doc/library.h"

// ===========================================================================
// Hangul's own part of the app: what it is (HANGUL_INFO), and what it adds to
// the drawing core — the study layer's extension (study.h) and its own: the side
// panel's Study section (Characters, Hangul, Album, Reviews, Vocabulary,
// Lectures, Exams, Statistics), the welcome as the tutorial and on the very
// first launch.
// ===========================================================================

// Every screen, in the order they stack: the first open one is on top (and gets
// the pointer, its row, the frame). The page is under them all.
typedef enum {
    HANGUL_SCREEN_READCARD = 0,  // a must-read card (readcard.h): over everything, the welcome too
    HANGUL_SCREEN_WELCOME,   // over everything (the first time, and from Settings)
    HANGUL_SCREEN_PRACTICE,      // over whatever opened it
    HANGUL_SCREEN_VIEWER,        // over the lists, an exam's results, Check, Statistics
    HANGUL_SCREEN_SCAN,
    HANGUL_SCREEN_WORDEXAM,      // over the Vocabulary
    HANGUL_SCREEN_EXAM,          // over the album (a kept exam)
    HANGUL_SCREEN_STATS,
    HANGUL_SCREEN_TRANSLATOR,    // over the Vocabulary (Translate with Google: into Japanese)
    HANGUL_SCREEN_VOCAB,
    HANGUL_SCREEN_CHECK,
    HANGUL_SCREEN_LIBRARY,       // the lectures and the learner's documents (doc/library.h)
    HANGUL_SCREEN_ALBUM,
    HANGUL_SCREEN_CHART,
    HANGUL_SCREEN_BROWSE,
    HANGUL_SCREEN_COUNT
} HANGUL_SCREEN_;

// Hangul: the study app (study.h: first, so a fude_app* is a hangul_app*), and
// its own screens: the Hangul chart (Korean's own) and the welcome.
typedef struct hangul_app {
    fude_study    study;
    fude_chart*   chart;
    fude_welcome* welcome;
    fude_library* library;
} hangul_app;

#define HANGUL_APP(_app) ((hangul_app*)(_app))

extern const fude_app_info  HANGUL_INFO;
extern const fude_extension HANGUL_EXTENSION;

// Its launch flags, a developer's, with the study's (study.h): --chart (the
// Hangul chart), --welcome[=PAGE]. Called where the core's are (look.h).
void hangul_look_args(i32 _argc, c8** _argv);
void hangul_look_start(fude_app* _app);
void hangul_look_loaded(fude_app* _app);
void hangul_look_frame(fude_app* _app);

// A screen of the side panel's opened over the page (the lasso's selection let
// go first; Browse and the chart are one or the other).
void hangul_app_open(fude_app* _app, HANGUL_SCREEN_ _screen);

#endif
