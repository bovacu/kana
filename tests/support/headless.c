// The engine, as the tests use it: its real library (linked by tests/run.sh), not started — no window, no GPU — with
// only its heap allocator set up (rde_cook_init, as a tool's process uses it), before main runs. So every container,
// string, file and maths function a suite reaches is RDE's own, not a copy.
//
// A suite that crashes says so and ends at once: its backtrace on stderr, then exit 128 + the signal. (Left to the
// system, macOS's crash reporter can hold a crashed process for many minutes, and the whole run with it.)
#include "rde.h"
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void fude_test_crashed(int _sig) {
    static const char _says[] = "\n*** the suite crashed (signal); its backtrace:\n";
    (void)!write(2, _says, sizeof(_says) - 1);
    any _frames[48];
    backtrace_symbols_fd(_frames, backtrace(_frames, 48), 2);
    _exit(128 + _sig);
}

__attribute__((constructor)) static void fude_test_engine_start(void) {   // (first: run.sh links this file first)
    if(!rde_cook_init()) {
        fprintf(stderr, "tests/support/headless.c: the engine's allocator could not be set up\n");
        exit(2);
    }
    const int _signals[] = { SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL, SIGTRAP };
    for(unsigned _i = 0; _i < sizeof(_signals) / sizeof(_signals[0]); _i++) {
        signal(_signals[_i], fude_test_crashed);
    }
}
