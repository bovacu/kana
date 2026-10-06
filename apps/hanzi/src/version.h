// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef HANZI_VERSION_H
#define HANZI_VERSION_H

// The app's name: the window's title, the side panel's version line, Your
// data's file, and in its words where the strings say {APP}
// (apps/hanzi/tools/strings.py reads it here). The home screen's is the
// platform's (platform/ios: CFBundleDisplayName).
#define HANZI_NAME "Hanzi Learn!"

// Its id (info.h): the save folder on a device, the backup's extension. Never
// changes: it was the name's before the name was "Hanzi Learn!".
#define HANZI_ID "hanzi"

// The app's version, shown at the bottom of the side panel and in Settings.
#define HANZI_VERSION "0.1.4"

// The app's Apple ID on the App Store (App Store Connect › the app › App
// Information › Apple ID, digits only). Empty until the app is there: "Rate
// Hanzi" then shows the system's rating sheet instead of the review page.
#define HANZI_APP_STORE_ID ""

#endif
