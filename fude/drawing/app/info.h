#ifndef FUDE_APP_INFO
#define FUDE_APP_INFO

#include "rde.h"

// ===========================================================================
// What the app is, for the parts of the core that say so (fude_app.info): the
// window's title, the side panel's version line, Settings' About and Licences,
// Rate, Your data's file, the save folder. Each app fills one (Kana:
// kana_app.c), its name and version from its src/version.h — where its strings
// tool reads the name too, for the texts that say it ({APP}: tools/strings/build.py).
// ===========================================================================

#define FUDE_APP_LICENCES 6u   // Settings › Licences: its documents, at most

// A document of Licences: its name, and the files it is made of (one after the other).
typedef struct {
    u32       name;       // FUDE_TEXT_
    const c8* files[3];   // in the app's assets (NULL: no more)
} fude_app_licence;

typedef struct {
    const c8*        name;         // shown: the window's title, the version line, Your data's file ("Kana backup")
    // What must never change once the app has users: its save folder on a device
    // and its backup's extension (<id>backup). NULL: the name in lowercase, its
    // letters and digits (Kana: kana/, .kanabackup). An app renamed later sets
    // this to its old one; a name without Latin letters sets one.
    const c8*        id;
    const c8*        version;      // shown, and written into a backup
    const c8*        store_id;     // the App Store's id, for Rate ("": the rating sheet until there is one)
    const c8*        script_font;  // the language's own script, behind the UI font (Kana: Noto Sans JP; NULL: none)
    const c8*        latin_font;   // Latin letters Roboto has not, behind it (Hindi's ṭ ḍ, Thai's ǎ ɔ: Noto Sans; NULL: none)
    u32              credits;      // About's credits, under the version (FUDE_TEXT_; FUDE_TEXT_COUNT: none)
    fude_app_licence licences[FUDE_APP_LICENCES];
    u32              licence_count;
} fude_app_info;

#endif
