// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_CRASH
#define FUDE_CRASH

#include "rde.h"

// ===========================================================================
// Crash reports (since 0.1.93): when the app dies, a short report is left in its save folder, and at the next launch
// the app asks whether to send it (crashcard.h sends it by email, and the person reads it before it goes).
//
// Where it sits: drawing/base, the app's side of it. The handler itself is RDE's (rde.h: rde_crash_install, which
// writes the report from inside the signal handler and walks the stack): this module says where the report goes, makes
// its header, keeps the running marker, finds what the last session left, and builds the email. fude_app_start
// installs it, the session marks when the app is on screen, and the crash card (app/crashcard.h) reads and sends the
// report. The notes a report ends with are RDE's too (rde_crash_note: "canvas opened", "screen home"); never pass a
// name or anything the person wrote.
// What gets reported:
//   a crash   a native signal (SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT, SIGTRAP) or, on Android, a Java exception
//             nothing caught. The report is written there and then: the app, its version, the device, what killed
//             it, where (each frame's library and offset, and its symbol where there is one; the rest is symbolized
//             later against the build), and the last things the app noted it was doing. Then the system's own
//             handling goes on (Android's tombstone, the process ended).
//   a death   the app gone while on screen with no report, because the system ended it (Android says why: a crash
//             in Java, an ANR, memory) or a watchdog did. The report says so.
// Nothing the person wrote, drew or named is ever in a report.
// Threads: every function is called on the main thread. RDE's handler runs on whichever thread crashed, on a stack of
// its own (so even a stack overflow is reported, on a worker too). On Android, a Java exception's report is written by
// the Java side, into the same file.
// Files, in the save folder:
//   crash_report.txt   the report, kept until it is sent or declined
//   crash_running      present while the app is on screen this session; finding it at launch with no report means
//                      the last session died
// Design: a death in the background is not a crash (the system ends background apps whenever it likes), so the
// running marker is removed when the app leaves the screen.
// ===========================================================================

// The two files' names in the save folder (above). Your data's backup leaves them out (backup.c): they are this
// device's, and a report restored on another device would be asked about there as its own crash. Android's Java side
// writes a Java exception's report to the same path, handed to it at install (FudeAndroid.java: crashInstall).
#define FUDE_CRASH_REPORT_FILE  "crash_report.txt"
#define FUDE_CRASH_RUNNING_FILE "crash_running"

// The most of a report the app keeps and shows, in bytes. A report is RDE's header (at most 1 KiB), a line on what
// killed the app, a stack walk and RDE_CRASH_NOTES short notes, well under this; a longer one (Android's report of a
// Java exception with a deep trace) loses its end. A fixed size is safe here: a report is diagnostics, never the
// person's data, and it holds more than an email link can carry anyway (FUDE_CRASH_MAILTO_MAX).
#define FUDE_CRASH_TEXT_MAX   8192u
// The longest mailto: link the crash card makes, in bytes. Not every mail app takes a much longer link, so the body is
// cut to fit it (fude_crash_mailto, at a whole character), and the person reads in the email exactly what is sent.
#define FUDE_CRASH_MAILTO_MAX 6000u

// Installs the crash report (RDE's handler, with this app's header: the app, its version, the device, the start time),
// marks this session as running, and keeps what the last session left (its report, or its death on screen without
// one) to be asked about. _dir is the save folder, ending in its separator; _app and _version head every report. Only
// the first call does anything.
void fude_crash_install(const c8* _dir, const c8* _app, const c8* _version);
// Says whether the app is on screen (true) or gone to the background (false), by creating or removing the running
// marker: a death in the background is not a crash. Does nothing before fude_crash_install.
void fude_crash_session(b8 _running);

// When the last session ended badly, copies its report into _out (NUL-terminated, cut to _size) and returns true.
// Returns false when there is nothing to ask about.
b8   fude_crash_pending(c8* _out, usize _size);
// Called once the report has been sent or declined: deletes it, and it is not offered again.
void fude_crash_forget(void);
// Builds a mailto: URL into _out for emailing _report to _to: the subject and the body percent-encoded, the body cut
// at a whole character to fit _size.
void fude_crash_mailto(const c8* _report, const c8* _to, const c8* _subject, c8* _out, usize _size);

// Returns whether an Android exit reason (ApplicationExitInfo.REASON_*: why the previous process ended) is one a
// report is made for: a signal, low memory, a crash in Java or native code, an ANR, a failure to start, or using too
// many resources.
b8   fude_crash_reason_reported(i32 _reason);

#endif
