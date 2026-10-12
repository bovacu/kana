// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_CRASH
#define FUDE_CRASH

#include "rde.h"

// ===========================================================================
// CRASH REPORTS (0.1.93), every app's (installed by fude_app_start). When the app
// dies, a short report is left in its save folder; at the next launch the app
// asks whether to send it (crashcard.h: by email, read before it goes).
//
//   a crash        a native signal (SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT,
//                  SIGTRAP) or, on Android, a Java exception nothing caught: the
//                  report written there and then — the app, its version, the
//                  device, what killed it, where (each frame's library and offset,
//                  its symbol where there is one: symbolized later against the
//                  build), the last things the app noted it was doing — then the
//                  system's own handling goes on (Android's tombstone, the process
//                  ended).
//   a death        the app gone while on screen with no report (the system ended
//                  it: Android says why — a crash in Java, an ANR, memory — or a
//                  watchdog): a report saying so.
//
// Nothing the person wrote, drew or named is in it. The handler only writes
// (open/write/close), on a stack of its own, its text made at start.
//
//   <save>crash_report.txt   the report, until it is sent or declined
//   <save>crash_running      there while the app is on screen this session
// ===========================================================================

#define FUDE_CRASH_NOTES 24u   // the last things noted, kept for a report

// Installed: the handlers set, the session marked as running — and what the last session left (a report, or its death
// with none) kept to be asked about. _dir: the save folder (ending in its separator).
void fude_crash_install(const c8* _dir, const c8* _app, const c8* _version);
// The app on screen (true) or gone to the background (false): a death in the background is no crash.
void fude_crash_session(b8 _running);
// Noted (what the app is doing: "canvas opened", "screen home"): the last FUDE_CRASH_NOTES go in a report. No names.
void fude_crash_note(const c8* _what);

// The last session's report, if it ended badly: its text into _out (NUL-ended, cut to _size). False: none.
b8   fude_crash_pending(c8* _out, usize _size);
// The report sent or declined: gone.
void fude_crash_forget(void);
// An email of _report to _to: a mailto: URL into _out — its subject, its body percent-encoded, the body cut (at a
// whole character) to fit _size.
void fude_crash_mailto(const c8* _report, const c8* _to, const c8* _subject, c8* _out, usize _size);

// Android's reason the previous process ended (ApplicationExitInfo.REASON_*), and whether it is one a report is for.
b8   fude_crash_reason_reported(i32 _reason);

#endif
