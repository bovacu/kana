// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_FETCH
#define FUDE_ZOOM_FETCH

#include "rde.h"

// ===========================================================================
// A file downloaded in the background (0.1.95: the speech model, listen.h), one at
// a time: into <path>.part as it comes, moved into place only once whole (a
// download stopped half way leaves nothing where the file goes). Android's
// (fetch_android.c: com.rde.fude.FudeAndroid, HttpURLConnection on a thread of its
// own), Apple's (fetch_apple.m: NSURLSession); elsewhere none.
// ===========================================================================

typedef enum {
    FUDE_FETCH_IDLE = 0,
    FUDE_FETCH_RUNNING,
    FUDE_FETCH_DONE,
    FUDE_FETCH_FAILED
} FUDE_FETCH_;

// Started (false: one already running, or none here).
b8          fude_fetch_start(const c8* _url, const c8* _path);
// How it goes: bytes come and of how many (0: not known yet).
FUDE_FETCH_ fude_fetch_state(u64* _got, u64* _total);
// Let go (the part dropped).
void        fude_fetch_cancel(void);

#endif
