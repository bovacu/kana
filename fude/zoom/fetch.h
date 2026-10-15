// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_FETCH
#define FUDE_ZOOM_FETCH

#include "rde.h"

// ===========================================================================
// Fetch: downloads one file in the background, one at a time (0.1.95: the speech model, listen.h). The file appears at
// its path only once it is whole, so a download stopped half way never leaves half a file where the file goes.
//
// Where it sits: zoom, the canvas apps' layer. The page (page_speech.c) starts the speech model's download and polls it
// once a frame until it is done. Platforms: Android's (fetch_android.c) is the Java class com.rde.fude.FudeAndroid, an
// HttpURLConnection on a thread of its own that writes into <path>.part as the bytes come and renames it when whole.
// Apple's (fetch_apple.m, macOS and iOS) is an NSURLSession download task: the system downloads into a file of its own,
// and when it is done the file is moved to <path>.part and then into place. Elsewhere there are no downloads: starting
// one returns false.
// Threads: call everything on the main thread. The download itself runs on the platform's own thread (the Java thread
// "FudeFetch", or NSURLSession's delegate queue), which writes or moves the file into place and sets the state, a
// volatile value without a lock; fude_fetch_state reads it, with the byte counts.
// Files: writes the downloaded bytes to the path given, through <path>.part, creating the folder if it is missing.
// ===========================================================================

// The state of the download (fude_fetch_state).
typedef enum {
    // None started yet.
    FUDE_FETCH_IDLE = 0,
    FUDE_FETCH_RUNNING,
    // The file is whole and in place.
    FUDE_FETCH_DONE,
    // The download or the move into place failed.
    FUDE_FETCH_FAILED
} FUDE_FETCH_;

// Starts downloading _url into the file at _path, replacing it once the download is whole. Returns false when one is
// already running, the URL or path is not usable, or this platform has no downloads.
b8          fude_fetch_start(const c8* _url, const c8* _path);
// Returns the download's state, with the bytes received so far in *_got and the whole size in *_total (0 while it is
// not known yet). Both are 0 where there are no downloads.
FUDE_FETCH_ fude_fetch_state(u64* _got, u64* _total);

#endif
