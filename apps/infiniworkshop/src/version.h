// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef INFINIWORKSHOP_VERSION_H
#define INFINIWORKSHOP_VERSION_H

// InfiniWorkshop: the hobby-projects app made from the deep-zoom canvas (docs/product_split.md) — circuits that run,
// mechanisms that move, plans at true size, wood and sewing. What it is and offers is
// apps/infiniworkshop/infiniworkshop.c, which the canvas apps' shell (fude/zoom/shell.h) runs.
//
// Its name: the window's title, the side panel's version line, Your data's file, and in its words where the strings
// say {APP} (apps/infiniworkshop/tools/strings.py reads it here). Its id comes from it: the save folder on a device and
// the backup's extension (infiniworkshop/, .infiniworkshopbackup) — once it has users they must not change.
#define INFINIWORKSHOP_NAME "InfiniWorkshop"

// The app's version, shown at the bottom of the side panel and in Settings.
#define INFINIWORKSHOP_VERSION "0.2.0"

// The app's Apple ID on the App Store (digits only). Empty until the app is there.
#define INFINIWORKSHOP_APP_STORE_ID ""

#endif
