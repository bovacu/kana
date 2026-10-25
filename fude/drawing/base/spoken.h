// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SPOKEN
#define FUDE_SPOKEN

#include "rde.h"

// ===========================================================================
// Spoken: a voice the person asked to hear (a word read aloud, study/services/speech.h; a recording played back,
// zoom/voice.h) plays under RDE's spoken-audio category, and the app's own category comes back once it ends.
//
// Where it sits: drawing/base, under the study apps' speech and the canvas apps' recordings, which say when each voice
// starts and ends.
// Its main types and their lifecycle: none. A count of the voices playing: the first to start keeps the app's
// category and sets RDE_AUDIO_CATEGORY_SPOKEN, the last to end gives the app's back.
// Threads: the main thread.
// Files: none.
// Design: SPOKEN plays through the silent switch and lowers other apps' music instead of stopping it (rde.h), and the
// music stays lowered for as long as SPOKEN is set: so it is set only while a voice plays. Counted, so a recording
// moved while it plays (stopped and started again) holds it throughout, and the music is not let up between.
// ===========================================================================

// A voice starts: the first sets RDE's SPOKEN category, keeping the app's own. Each start has one fude_spoken_end.
void fude_spoken_start(void);
// A voice ended: once none plays, the app's own category is set again.
void fude_spoken_end(void);

#endif
