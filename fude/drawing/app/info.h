// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_APP_INFO
#define FUDE_APP_INFO

#include "rde.h"

// ===========================================================================
// App info: what the app is, for the parts of the core that say so (fude_app.info): the window's title, the side
// panel's version line, Settings' About and Licences, Rate, the name of Your data's file, and the save folder.
//
// Where it sits: drawing/app. Each app fills one constant fude_app_info in its own source (apps/<app>/<app>.c),
// taking its name and version from its src/version.h. The app's strings tool reads the name from there too, for the
// texts that say it ({APP}: tools/strings/build.py). app.c, side.c, session.c and crashcard.c read it.
//
// Threads: it is constant data, read on the main thread.
// ===========================================================================

// The most documents Settings › Licences shows (it has a button for each, side.h); an app's table lists no more.
#define FUDE_APP_LICENCES 6u

// Every app's copyright, shown in Settings' About and in Licences. It is a name and a year, the same in every language.
#define FUDE_APP_COPYRIGHT "\xC2\xA9 2026 Borja Vazquez Cuesta"   // ©
// The side panel's FAQ & Contact: the address to write to (every app's), and the FAQ's page, used unless the app has
// its own (fude_app_info.faq_url). "" means there is no FAQ link yet.
#define FUDE_APP_CONTACT "rde.apps.support@gmail.com"
#define FUDE_APP_FAQ_URL ""

// A document in Licences: its name, and the files it is made of, shown one after the other.
typedef struct {
    u32       name;       // A FUDE_TEXT_ id.
    const c8* files[3];   // Paths in the app's assets (NULL after the last).
} fude_app_licence;

// Fills a fude_app_info's licences and licence_count from an app's table of documents, a constant array of
// fude_app_licence counted by its size, so that a document added to the table is counted with it:
//   static const fude_app_licence APP_LICENCES[] = { ... };
//   static const fude_app_info APP_INFO = { .name = APP_NAME, ..., FUDE_APP_LICENCES_OF(APP_LICENCES) };
// FUDE_APP_LICENCE_COUNT is that count. The build stops (an array of negative size) when the table holds none, or
// more than Licences shows (FUDE_APP_LICENCES), or when it is given a pointer, which has no size to count by.
#define FUDE_APP_LICENCE_N(_table)     (sizeof(_table) / sizeof((_table)[0]))
#define FUDE_APP_LICENCE_COUNT(_table) \
    ((u32)(FUDE_APP_LICENCE_N(_table) + 0u * sizeof(c8[FUDE_APP_LICENCE_N(_table) >= 1u && FUDE_APP_LICENCE_N(_table) <= FUDE_APP_LICENCES ? 1 : -1])))
#define FUDE_APP_LICENCES_OF(_table)   .licences = (_table), .licence_count = FUDE_APP_LICENCE_COUNT(_table)

// What the app is.
typedef struct {
    // Its name, as shown: the window's title, the version line, Your data's file ("<Name> backup").
    const c8*        name;
    // What must never change once the app has users: its save folder on a device and its backup's extension
    // (<id>backup). NULL means the name in lowercase, letters and digits only (Name: name/, .namebackup). An app
    // renamed later sets this to its old id, and an app whose name has no Latin letters must set one.
    const c8*        id;
    // Another app's id whose Your data files this one opens too (NULL: none): an app split from that one, taking what
    // was drawn in it (it opens that app's .<id>backup too). Every app's backup is the same kind of file
    // (backup.h).
    const c8*        imports;
    const c8*        version;      // Shown, and written into a backup.
    const c8*        store_id;     // The App Store's id, for Rate ("" shows the rating sheet until there is one).
    // The language's own script, as a fallback behind the UI font (Japanese's: Noto Sans JP; NULL: none).
    const c8*        script_font;
    // A font for the Latin letters Roboto lacks, behind it (Hindi's ṭ ḍ, Thai's ǎ ɔ: Noto Sans; NULL: none).
    const c8*        latin_font;
    u32              credits;      // About's credits, under the version (a FUDE_TEXT_ id; FUDE_TEXT_COUNT for none).
    // Settings › Licences' documents, in order: the app's constant table and how many it holds (up to
    // FUDE_APP_LICENCES), both set by FUDE_APP_LICENCES_OF. NULL and 0 for none.
    const fude_app_licence* licences;
    u32              licence_count;
    const c8*        faq_url;      // The FAQ & Contact page (NULL to use FUDE_APP_FAQ_URL).
    // The screen turns with the device, landscape too (the canvas apps). When false it stays portrait, either way up.
    b8               turns;
} fude_app_info;

#endif
