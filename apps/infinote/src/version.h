// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef INFINOTE_VERSION_H
#define INFINOTE_VERSION_H

// InfiNote: the note-taking app made from the deep-zoom canvas (docs/product_split.md) — writing, sketching, study, a
// diary, PDFs and maths. What it is and offers is apps/infinote/infinote.c, which the canvas apps' shell
// (fude/zoom/shell.h) runs.
//
// Its name: the window's title, the side panel's version line, Your data's file, and in its words where the strings
// say {APP} (apps/infinote/tools/strings.py reads it here). Its id comes from it: the save folder on a device and the
// backup's extension (infinote/, .infinotebackup) — once it has users they must not change.
#define INFINOTE_NAME "InfiNote"

// The app's version, shown at the bottom of the side panel and in Settings.
#define INFINOTE_VERSION "0.2.4"

// The app's Apple ID on the App Store (digits only). Empty until the app is there.
#define INFINOTE_APP_STORE_ID ""

#endif
