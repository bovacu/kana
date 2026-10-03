#ifndef KANA_APP_H
#define KANA_APP_H

#include "rde.h"
#include "study/app/study.h"
#include "lang/ja/chart.h"
#include "study/app/welcome.h"
#include "drawing/doc/library.h"

// ===========================================================================
// Kana's own part of the app: what it is (KANA_INFO), and what it adds to the
// drawing core — the study layer's extension (study.h) and its own: the side
// panel's Study section (Kanji, Kana, Album, Reviews, Vocabulary, Exams,
// Statistics), the welcome as the tutorial and on the very first launch.
// ===========================================================================

// Every screen, in the order they stack: the first open one is on top (and gets
// the pointer, its row, the frame). The page is under them all.
typedef enum {
    KANA_SCREEN_READCARD = 0,  // a must-read card (readcard.h): over everything, the welcome too
    KANA_SCREEN_WELCOME,   // over everything (the first time, and from Settings)
    KANA_SCREEN_PRACTICE,      // over whatever opened it
    KANA_SCREEN_VIEWER,        // over the lists, an exam's results, Check, Statistics
    KANA_SCREEN_SCAN,
    KANA_SCREEN_WORDEXAM,      // over the Vocabulary
    KANA_SCREEN_EXAM,          // over the album (a kept exam)
    KANA_SCREEN_STATS,
    KANA_SCREEN_TRANSLATOR,    // over the Vocabulary (Translate with Google: into Japanese)
    KANA_SCREEN_VOCAB,
    KANA_SCREEN_CHECK,
    KANA_SCREEN_LIBRARY,       // the lectures and the learner's documents (doc/library.h)
    KANA_SCREEN_ALBUM,
    KANA_SCREEN_CHART,
    KANA_SCREEN_BROWSE,
    KANA_SCREEN_COUNT
} KANA_SCREEN_;

// Kana: the study app (study.h: first, so a fude_app* is a kana_app*), and its
// own screens: the kana chart (Japanese's own) and the welcome.
typedef struct kana_app {
    fude_study    study;
    fude_chart*   chart;
    fude_welcome* welcome;
    fude_library* library;
} kana_app;

#define KANA_APP(_app) ((kana_app*)(_app))

extern const fude_app_info  KANA_INFO;
extern const fude_extension KANA_EXTENSION;

// Its launch flags, a developer's, with the study's (study.h): --kana (the
// chart), --welcome[=PAGE]. Called where the core's are (look.h).
void kana_look_args(i32 _argc, c8** _argv);
void kana_look_start(fude_app* _app);
void kana_look_loaded(fude_app* _app);
void kana_look_frame(fude_app* _app);

// A screen of the side panel's opened over the page (the lasso's selection let
// go first; Browse and the chart are one or the other).
void kana_app_open(fude_app* _app, KANA_SCREEN_ _screen);

#endif
