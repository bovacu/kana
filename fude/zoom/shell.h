// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SHELL_H
#define FUDE_ZOOM_SHELL_H

#include "rde.h"
#include "drawing/app/app.h"
#include "zoom/home.h"

// ===========================================================================
// Canvas shell: the program every canvas app runs. It answers what RDE calls (init, the events, each frame's update
// and render, the end) and owns everything the app is made of: the drawing core's page models and UI, the deep-zoom
// page (page.h) in place of the ink page, and the home screen (home.h), its one screen.
//
// Where it sits: the top of the zoom layer. It is the same for every canvas app (Sketching, InfiNote,
// InfiniWorkshop): an app is only a description of itself in a fude_zoom_shell_def (what it is, what it offers, its
// home screen's words and welcome, whether it opens on its home screen), and its main() hands that to
// fude_zoom_shell_run (apps/infinote/infinote.c is one).
// Its main types and their lifecycle: everything the app owns is a static in shell.c, made once when RDE calls its
// init and destroyed when RDE ends it, so there is one app per process. The fude_zoom_shell_def belongs to the app and
// must stay valid for the whole run (it is normally a static const), and so must its welcome.
// Threads: everything here runs on the app's main thread, inside rde_run.
// Files: it reads the app's config (./assets/config.rdef, beside the assets). The saves belong to the session
// (session.h), and a canvas's files to the page (zfile.h).
// Design: the core keeps its ink, its page and its canvas: the ink holds the brush's settings that the toolbar and
// Settings set, while the ink page and canvas sit idle. The extension's page kind (FUDE_ZOOM_PAGE_KIND) hands Undo,
// Redo, Clear and the opening and saving of a canvas to the deep-zoom page. The extension is the shell's, the same in
// every canvas app (the page's tools, menus, Settings sections and widgets, the papers); only the side panel's
// Tutorial depends on the description (an app with a welcome has one). The canvas asks the app's product what it
// offers (product.h), never which app it is. A developer's launch flags are drawing/app/look.h's and zoom/look.h's;
// a release takes no arguments, unless it is built with FUDE_ALLOW_ARGS to measure a release on a device.
// ===========================================================================

// The welcome (first-run guidance) an app shows over its home screen once, and again from the side panel's
// Tutorial. Its first step puts the canvas's examples on a new canvas and opens the first (Insert's Examples); the
// other way in is the home screen's New.
typedef struct {
    u32 title;                          // its title (a FUDE_TEXT_ id), with the app's name in it
    u32 lines[FUDE_ZOOM_HOME_LINES];    // its lines (FUDE_TEXT_; FUDE_TEXT_COUNT for an unused one)
    u32 first;                          // the label of its first step's button (FUDE_TEXT_)
} fude_zoom_shell_welcome;

// A canvas app: what makes it itself.
typedef struct {
    const fude_app_info*           info;           // what it is: its name, fonts, licences, credits (info.h)
    u8                             product;        // what it offers (product.h's FUDE_ZOOM_PRODUCT_): topics, tools,
                                                   // themes, sound
    u32                            home_title;     // the home screen's header (FUDE_TEXT_: canvases, notebooks...)
    u32                            home_new;       // the label of its New button (FUDE_TEXT_)
    b8                             new_notebook;   // New makes a notebook of pages, not the endless canvas
    u8                             new_pages;      // ...whose pages are this size (pages.h's FUDE_ZOOM_PAGES_)
    const fude_zoom_shell_welcome* welcome;        // its welcome (NULL: none, and no Tutorial in the side panel)
    // Launched with no arguments, it opens on its home screen, with the last canvas under it, and shows its welcome
    // over it until the welcome has been seen. Launched with arguments (a developer's look), it opens on the canvas.
    b8                             opens_home;
} fude_zoom_shell_def;

// Runs the app described by _def (through rde_run), from its main(), and returns rde_run's result. A release build
// ignores its launch arguments, unless it is built with FUDE_ALLOW_ARGS to measure a release on a device.
i32 fude_zoom_shell_run(const fude_zoom_shell_def* _def, i32 _argc, c8** _argv);

#endif
