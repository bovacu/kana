// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef DRAW_VERSION_H
#define DRAW_VERSION_H

// The app's name: the window's title, the side panel's version line, Your
// data's file, and in its words where the strings say {APP}
// (apps/draw/tools/strings.py reads it here). Its id (info.h) comes from it:
// the save folder on a device, the backup's extension — once it has users they
// must not change, so a rename then sets the id to "draw".
#define DRAW_NAME "Draw"

// The app's version, shown at the bottom of the side panel and in Settings.
#define DRAW_VERSION "0.1.0"

// The app's Apple ID on the App Store (digits only). Empty until the app is
// there: Rate then shows the system's rating sheet instead of the review page.
#define DRAW_APP_STORE_ID ""

#endif
