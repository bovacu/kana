#ifndef KANA_VERSION_H
#define KANA_VERSION_H

// The app's name: the window's title, the side panel's version line, Your
// data's file, and in its words where the strings say {APP}
// (apps/kana/tools/strings.py reads it here). The home screen's is the
// platform's (platform/ios: CFBundleDisplayName).
#define KANA_NAME "Kana Learn!"

// Its id (info.h): the save folder on a device, the backup's extension. Never
// changes: it was the name's before the name was "Kana Learn!".
#define KANA_ID "kana"

// The app's version, shown at the bottom of the side panel and in Settings.
// Minor: the milestone being worked on (docs/design.md); patch: builds handed
// out within it. The build date comes from the compiler.
#define KANA_VERSION "0.5.0"

// The app's Apple ID on the App Store (App Store Connect › the app › App
// Information › Apple ID, digits only). Empty until the app is there: "Rate
// Kana" then shows the system's rating sheet instead of the review page.
#define KANA_APP_STORE_ID ""

#endif
