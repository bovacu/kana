// Crash reports (fude/drawing/base/crash.h): a report left by the last session found at start and forgotten; the
// session's marker; a real crash (a child process aborting) writing its report — who, what, where, what it was doing —
// and still dying of it; a death with no report found by the next start (a second run of this program); Android's
// reasons; the email it goes in (encoded, never cut inside a character or an escape).
#include "drawing/base/crash.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

const c8* rde_device_get_model(void) { return "Test Tablet"; }
const c8* rde_device_get_platform_name(void) { return "TestOS"; }
const c8* rde_device_get_os_version(void) { return "14"; }

static b8 exists(const c8* p) { struct stat st; return stat(p, &st) == 0; }

// The handler there before the crash handler (the engine's, in an app): handed the crash after the report is written.
// Here it ends the child at once, as SIGABRT would (128 + 6) — without the system's own crash report, which can hold a
// process for minutes.
static void before_handler(int sig) { _exit(128 + sig); }
static void put(const c8* p, const c8* text) { FILE* f = fopen(p, "wb"); if(f) { fputs(text, f); fclose(f); } }
static void get(const c8* p, c8* out, usize size) { out[0] = 0; FILE* f = fopen(p, "rb"); if(f) { const usize n = fread(out, 1, size - 1, f); out[n] = 0; fclose(f); } }

int main(int argc, char** argv) {
    mkdir("./saves", 0755);
    if(argc > 1 && strcmp(argv[1], "--second") == 0) {
        // (the next start: what the last session left, said on stdout)
        fude_crash_install("./saves/", "Second", "2.0");
        c8 text[4096];
        printf("%s", fude_crash_pending(text, sizeof(text)) ? text : "NONE");
        return 0;
    }
    signal(SIGABRT, before_handler);
    // A report the last session left: found, then forgotten.
    remove("./saves/crash_running");
    put("./saves/crash_report.txt", "App: Test 0.9\nWhat: SIGSEGV\n");
    fude_crash_install("./saves/", "Test", "1.0");
    c8 text[8192];
    CHECK(fude_crash_pending(text, sizeof(text)) && strstr(text, "App: Test 0.9") != NULL);
    CHECK(exists("./saves/crash_running"));
    fude_crash_forget();
    CHECK(!fude_crash_pending(text, sizeof(text)) && !exists("./saves/crash_report.txt"));
    // The session: off the screen, on again.
    fude_crash_session(false);
    CHECK(!exists("./saves/crash_running"));
    fude_crash_session(true);
    CHECK(exists("./saves/crash_running"));
    // A real crash, in a child: its report written, and the child still dies of it.
    for(int k = 0; k < 30; k++) {
        c8 note[32];
        snprintf(note, sizeof(note), "step %d", k);
        rde_crash_note(note);
    }
    rde_crash_note("drawing a stroke");
    const pid_t child = fork();
    if(child == 0) {
        raise(SIGABRT);
        _exit(0);   // (not reached)
    }
    int status = 0;
    waitpid(child, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 128 + SIGABRT);   // (the handler before it given the crash: it ended it)
    c8 report[8192];
    get("./saves/crash_report.txt", report, sizeof(report));
    printf("--- the report a crash left ---\n%s---\n", report);
    CHECK(strstr(report, "App: Test 1.0") != NULL && strstr(report, "Device: Test Tablet, TestOS 14") != NULL);
    CHECK(strstr(report, "What: SIGABRT") != NULL && strstr(report, "Where:\n  #0 ") != NULL);
    CHECK(strstr(report, "Notes:\n") != NULL && strstr(report, "drawing a stroke\n") != NULL && strstr(report, "step 29\n") != NULL);
    CHECK(strstr(report, "step 6\n") == NULL && strstr(report, "started\n") == NULL);   // (the last RDE_CRASH_NOTES only)
    remove("./saves/crash_report.txt");
    // A death on screen with no report: the next start says so (a second run of this program).
    fude_crash_session(true);
    CHECK(exists("./saves/crash_running"));
    c8 cmd[1200], out[4096] = { 0 };
    snprintf(cmd, sizeof(cmd), "%s --second", argv[0]);
    FILE* p = popen(cmd, "r");
    if(p) { const usize n = fread(out, 1, sizeof(out) - 1, p); out[n] = 0; pclose(p); }
    CHECK(strstr(out, "App: Second 2.0") != NULL && strstr(out, "ended while on screen") != NULL);
    // ...and nothing when it left the screen first.
    remove("./saves/crash_running");
    p = popen(cmd, "r");
    if(p) { const usize n = fread(out, 1, sizeof(out) - 1, p); out[n] = 0; pclose(p); }
    CHECK(strcmp(out, "NONE") == 0);
    // Android's reasons: a crash, native, an ANR, memory — not a person swiping it away or an update.
    CHECK(fude_crash_reason_reported(4) && fude_crash_reason_reported(5) && fude_crash_reason_reported(6) && fude_crash_reason_reported(3));
    CHECK(!fude_crash_reason_reported(10) && !fude_crash_reason_reported(16) && !fude_crash_reason_reported(1) && !fude_crash_reason_reported(-1));
    // The email: its address, subject and body encoded; a long body cut to fit, never inside a character or an escape.
    c8 url[300];
    fude_crash_mailto("What: a crash\n#0 libmain.so +0x1f", "dev@example.com", "App 1.0: report", url, sizeof(url));
    CHECK(strcmp(url, "mailto:dev@example.com?subject=App%201.0%3A%20report&body=What%3A%20a%20crash%0A%230%20libmain.so%20%2B0x1f") == 0);
    c8 lots[2000];
    lots[0] = 0;
    for(int k = 0; k < 150; k++) { strcat(lots, "\xC3\xA9t\xC3\xA9 "); }   // été
    c8 small[200];
    fude_crash_mailto(lots, "a@b.c", "S", small, sizeof(small));
    const usize n = strlen(small);
    CHECK(n < sizeof(small) && n > 150u);
    // (it ends on a whole escape of a whole character — …%A9, %20 or a letter — never %C, %C3 or half of é)
    const c8* last = strrchr(small, '%');
    CHECK(last != NULL && strlen(last) >= 3u);
    CHECK(!(n >= 3u && strcmp(small + n - 3u, "%C3") == 0));
    c8 tiny[10];
    fude_crash_mailto("body", "someone@somewhere.example", "S", tiny, sizeof(tiny));
    CHECK(strlen(tiny) < sizeof(tiny));
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
