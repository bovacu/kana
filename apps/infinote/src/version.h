// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef SKETCHING_VERSION_H
#define SKETCHING_VERSION_H

// InfiNote: the note-taking app made from the deep-zoom canvas (docs/product_split.md) — writing, sketching, study, a diary, PDFs and maths — Notes
// (zoom/product.h). Its shell is Sketching's (apps/sketching/sketching.c), which reads who it is here.
//
// Its name: the window's title, the side panel's version line, Your data's file, and in its words where the strings
// say {APP} (apps/infinote/tools/strings.py reads it here). Its id comes from it: the save folder on a device and the
// backup's extension (infinote/, .infinotebackup) — once it has users they must not change.
#define SKETCHING_NAME "InfiNote"

// The app's version, shown at the bottom of the side panel and in Settings.
#define SKETCHING_VERSION "0.1.80"

// The app's Apple ID on the App Store (digits only). Empty until the app is there.
#define SKETCHING_APP_STORE_ID ""

// What it offers (zoom/product.h).
#define SKETCHING_PRODUCT FUDE_ZOOM_PRODUCT_NOTES

// Sketching's Your data files opened too (Settings › Your data › Import): what was drawn there carried over.
#define SKETCHING_IMPORTS "sketching"

#endif
