// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/crash.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#if !defined(_WIN32)
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ucontext.h>
#if defined(RDE_PLATFORM_ANDROID)
#include <unwind.h>
#include "drawing/base/android.h"
#endif
#endif

#define FZC_HEADER 1024u
#define FZC_NOTE   64u
#define FZC_FRAMES 48u

static c8  fzc_report_path[1024];
static c8  fzc_running_path[1024];
static c8  fzc_header[FZC_HEADER];   // the app, its version, the device: made at start, written by the handler as it is
static c8  fzc_notes[FUDE_CRASH_NOTES][FZC_NOTE];
static u32 fzc_note_next = 0;
static b8  fzc_installed = false;
static b8  fzc_pending   = false;    // the last session left something to ask about
static c8  fzc_pending_text[8192];

// --- what a report says --------------------------------------------------------------------------

b8 fude_crash_reason_reported(i32 _reason) {
    // SIGNALED 2, LOW_MEMORY 3 (on screen: the app's own memory), CRASH 4, CRASH_NATIVE 5, ANR 6,
    // INITIALIZATION_FAILURE 7, EXCESSIVE_RESOURCE_USAGE 9
    return _reason == 2 || _reason == 3 || _reason == 4 || _reason == 5 || _reason == 6 || _reason == 7 || _reason == 9;
}

RDE_INTERNAL const c8* fzc_reason_name(i32 _reason) {
    switch(_reason) {
        case 2:  return "killed by a signal";
        case 3:  return "out of memory";
        case 4:  return "a crash in Java";
        case 5:  return "a native crash";
        case 6:  return "not responding (ANR)";
        case 7:  return "failed to start";
        case 9:  return "too many resources";
        default: return "ended by the system";
    }
}

void fude_crash_note(const c8* _what) {
    snprintf(fzc_notes[fzc_note_next % FUDE_CRASH_NOTES], FZC_NOTE, "%s", _what != NULL ? _what : "");
    fzc_note_next++;
}

// --- the handler -----------------------------------------------------------------------------------
#if !defined(_WIN32)

static struct sigaction fzc_previous[32];
static volatile sig_atomic_t fzc_crashing = 0;
static u8 fzc_stack[64 * 1024];

// (only what is safe in a handler: write, and numbers made by hand)
RDE_INTERNAL void fzc_put(int _fd, const c8* _s) {
    usize _n = 0;
    while(_s[_n] != 0) {
        _n++;
    }
    while(_n > 0u) {
        const ssize_t _w = write(_fd, _s, _n);
        if(_w <= 0) {
            return;
        }
        _s += _w;
        _n -= (usize)_w;
    }
}

RDE_INTERNAL void fzc_put_hex(int _fd, uintptr_t _v) {
    c8 _b[2 + sizeof(uintptr_t) * 2 + 1];
    u32 _n = (u32)(sizeof(uintptr_t) * 2);
    _b[0] = '0';
    _b[1] = 'x';
    for(u32 _i = 0; _i < _n; _i++) {
        const u32 _d = (u32)((_v >> ((_n - 1u - _i) * 4u)) & 0xFu);
        _b[2u + _i] = (c8)(_d < 10u ? '0' + _d : 'a' + _d - 10u);
    }
    _b[2u + _n] = 0;
    fzc_put(_fd, _b);
}

RDE_INTERNAL void fzc_put_dec(int _fd, i64 _v) {
    c8 _b[24];
    u32 _i = sizeof(_b) - 1u;
    _b[_i] = 0;
    const b8 _neg = _v < 0;
    u64 _u = _neg ? (u64)(-_v) : (u64)_v;
    do {
        _b[--_i] = (c8)('0' + _u % 10u);
        _u /= 10u;
    } while(_u > 0u && _i > 1u);
    if(_neg) {
        _b[--_i] = '-';
    }
    fzc_put(_fd, &_b[_i]);
}

RDE_INTERNAL const c8* fzc_signal_name(int _sig) {
    switch(_sig) {
        case SIGSEGV: return "SIGSEGV (a bad memory access)";
        case SIGBUS:  return "SIGBUS (a bad memory access)";
        case SIGFPE:  return "SIGFPE (an arithmetic error)";
        case SIGILL:  return "SIGILL (an illegal instruction)";
        case SIGABRT: return "SIGABRT (aborted)";
        case SIGTRAP: return "SIGTRAP (a trap)";
        default:      return "a signal";
    }
}

#if defined(RDE_PLATFORM_ANDROID)
typedef struct {
    uintptr_t* frames;
    u32        count, most;
} fzc_unwind;

RDE_INTERNAL _Unwind_Reason_Code fzc_unwind_one(struct _Unwind_Context* _ctx, void* _arg) {
    fzc_unwind* _u = (fzc_unwind*)_arg;
    const uintptr_t _pc = (uintptr_t)_Unwind_GetIP(_ctx);
    if(_pc != 0u && _u->count < _u->most) {
        _u->frames[_u->count++] = _pc;
    }
    return _u->count < _u->most ? _URC_NO_REASON : _URC_END_OF_STACK;
}
#endif

// Where it stopped and the frames under it, from the machine's state the signal handed over: its pc, then each frame's
// saved return address up the frame pointers' chain (every build keeps them) — the handler's own stack not walked, so a
// crash on a stack of its own (sigaltstack) loses nothing. Frames past the stack's known ends, or not going up, end it.
static uintptr_t fzc_stack_lo = 0, fzc_stack_hi = 0;   // the engine's thread's stack (found at start)

RDE_INTERNAL u32 fzc_walk(void* _context, uintptr_t* _frames, u32 _most) {
    if(_context == NULL || _most == 0u) {
        return 0u;
    }
    uintptr_t _pc = 0, _fp = 0;
#if defined(__APPLE__) && defined(__aarch64__)
    _pc = (uintptr_t)((ucontext_t*)_context)->uc_mcontext->__ss.__pc;
    _fp = (uintptr_t)((ucontext_t*)_context)->uc_mcontext->__ss.__fp;
#elif defined(__APPLE__) && defined(__x86_64__)
    _pc = (uintptr_t)((ucontext_t*)_context)->uc_mcontext->__ss.__rip;
    _fp = (uintptr_t)((ucontext_t*)_context)->uc_mcontext->__ss.__rbp;
#elif defined(__linux__) && defined(__aarch64__)
    _pc = (uintptr_t)((ucontext_t*)_context)->uc_mcontext.pc;
    _fp = (uintptr_t)((ucontext_t*)_context)->uc_mcontext.regs[29];
#elif defined(__linux__) && defined(__x86_64__) && defined(REG_RIP)
    _pc = (uintptr_t)((ucontext_t*)_context)->uc_mcontext.gregs[REG_RIP];
    _fp = (uintptr_t)((ucontext_t*)_context)->uc_mcontext.gregs[REG_RBP];
#elif defined(__linux__) && defined(__arm__)
    _pc = (uintptr_t)((ucontext_t*)_context)->uc_mcontext.arm_pc;
    _fp = (uintptr_t)((ucontext_t*)_context)->uc_mcontext.arm_fp;
#endif
    u32 _n = 0;
    if(_pc != 0u) {
        _frames[_n++] = _pc;
    }
    const b8 _known = fzc_stack_hi > fzc_stack_lo && _fp >= fzc_stack_lo && _fp < fzc_stack_hi;
    while(_n < _most && _fp != 0u && (_fp & (sizeof(uintptr_t) - 1u)) == 0u) {
        if(_known && (_fp < fzc_stack_lo || _fp + 2u * sizeof(uintptr_t) > fzc_stack_hi)) {
            break;
        }
        const uintptr_t _next = ((const uintptr_t*)_fp)[0];
        const uintptr_t _ret  = ((const uintptr_t*)_fp)[1];
        if(_ret == 0u || _next == 0u) {
            break;   // (the outermost frame: nothing called it)
        }
        _frames[_n++] = _ret;
        if(_next <= _fp || _next - _fp > (uintptr_t)(8u << 20)) {
            break;   // (not going up the stack: the chain is done, or not one)
        }
        _fp = _next;
    }
    return _n;
}

RDE_INTERNAL void fzc_handler(int _sig, siginfo_t* _info, void* _context) {
    if(fzc_crashing) {
        _exit(128 + _sig);   // (a crash while writing the report: no second one)
    }
    fzc_crashing = 1;
    const int _fd = open(fzc_report_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(_fd >= 0) {
        fzc_put(_fd, fzc_header);
        fzc_put(_fd, "What: ");
        fzc_put(_fd, fzc_signal_name(_sig));
        fzc_put(_fd, ", code ");
        fzc_put_dec(_fd, _info != NULL ? _info->si_code : 0);
        fzc_put(_fd, ", at ");
        fzc_put_hex(_fd, _info != NULL ? (uintptr_t)_info->si_addr : 0u);
        fzc_put(_fd, "\nWhere:\n");
        uintptr_t _frames[FZC_FRAMES];
        u32 _n = fzc_walk(_context, _frames, FZC_FRAMES);
#if defined(RDE_PLATFORM_ANDROID)
        if(_n < 3u) {
            // (code without frame pointers in the way: the unwinder's tables instead)
            fzc_unwind _u = { _frames, 0u, FZC_FRAMES };
            _Unwind_Backtrace(fzc_unwind_one, &_u);
            _n = _u.count;
        }
#endif
        for(u32 _i = 0; _i < _n; _i++) {
            Dl_info _dl;
            fzc_put(_fd, "  #");
            fzc_put_dec(_fd, (i64)_i);
            fzc_put(_fd, " ");
            if(dladdr((void*)_frames[_i], &_dl) != 0 && _dl.dli_fname != NULL) {
                const c8* _base = _dl.dli_fname;
                for(const c8* _p = _dl.dli_fname; *_p != 0; _p++) {
                    if(*_p == '/') {
                        _base = _p + 1;
                    }
                }
                fzc_put(_fd, _base);
                fzc_put(_fd, " +");
                fzc_put_hex(_fd, _frames[_i] - (uintptr_t)_dl.dli_fbase);
                if(_dl.dli_sname != NULL) {
                    fzc_put(_fd, " ");
                    fzc_put(_fd, _dl.dli_sname);
                }
            } else {
                fzc_put_hex(_fd, _frames[_i]);
            }
            fzc_put(_fd, "\n");
        }
        fzc_put(_fd, "Notes:\n");
        const u32 _first = fzc_note_next > FUDE_CRASH_NOTES ? fzc_note_next - FUDE_CRASH_NOTES : 0u;
        for(u32 _i = _first; _i < fzc_note_next; _i++) {
            fzc_put(_fd, "  ");
            fzc_put(_fd, fzc_notes[_i % FUDE_CRASH_NOTES]);
            fzc_put(_fd, "\n");
        }
        close(_fd);
    }
    // The system's own handling, as before: the handler there was (the engine's: Android's tombstone), else the default.
    if(_sig > 0 && _sig < 32) {
        sigaction(_sig, &fzc_previous[_sig], NULL);
        if((fzc_previous[_sig].sa_flags & SA_SIGINFO) && fzc_previous[_sig].sa_sigaction != NULL) {
            fzc_previous[_sig].sa_sigaction(_sig, _info, _context);
            return;
        }
        if(fzc_previous[_sig].sa_handler != SIG_DFL && fzc_previous[_sig].sa_handler != SIG_IGN && fzc_previous[_sig].sa_handler != NULL) {
            fzc_previous[_sig].sa_handler(_sig);
            return;
        }
    }
    signal(_sig, SIG_DFL);
    raise(_sig);
}

RDE_INTERNAL void fzc_set_handlers(void) {
    stack_t _alt;
    memset(&_alt, 0, sizeof(_alt));
    _alt.ss_sp   = fzc_stack;
    _alt.ss_size = sizeof(fzc_stack);
    sigaltstack(&_alt, NULL);   // (a stack overflow's crash has none left of its own)
    const int _sigs[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT, SIGTRAP };
    for(u32 _i = 0; _i < sizeof(_sigs) / sizeof(_sigs[0]); _i++) {
        struct sigaction _sa;
        memset(&_sa, 0, sizeof(_sa));
        _sa.sa_sigaction = fzc_handler;
        _sa.sa_flags     = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&_sa.sa_mask);
        sigaction(_sigs[_i], &_sa, &fzc_previous[_sigs[_i]]);
    }
}
#endif

// --- at start, and after ---------------------------------------------------------------------------

// A small file's text into _out (NUL-ended): false when there is none.
RDE_INTERNAL b8 fzc_read(const c8* _path, c8* _out, usize _size) {
    FILE* _f = fopen(_path, "rb");
    if(_f == NULL) {
        return false;
    }
    const usize _n = fread(_out, 1, _size - 1u, _f);
    fclose(_f);
    _out[_n] = 0;
    return true;
}

void fude_crash_install(const c8* _dir, const c8* _app, const c8* _version) {
    if(fzc_installed) {
        return;
    }
    fzc_installed = true;
    snprintf(fzc_report_path, sizeof(fzc_report_path), "%scrash_report.txt", _dir);
    snprintf(fzc_running_path, sizeof(fzc_running_path), "%scrash_running", _dir);
    // (made now: the handler only writes it)
    const time_t _now = time(NULL);
    struct tm _tm;
#if defined(_WIN32)
    gmtime_s(&_tm, &_now);
#else
    gmtime_r(&_now, &_tm);
#endif
    c8 _started[32];
    strftime(_started, sizeof(_started), "%Y-%m-%d %H:%M UTC", &_tm);
    snprintf(fzc_header, sizeof(fzc_header), "App: %s %s\nDevice: %s, %s %s\nStarted: %s\n", _app, _version, rde_device_get_model(),
             rde_device_get_platform_name(), rde_device_get_os_version(), _started);
    // What the last session left: a report (a crash), or its death on screen with none.
    fzc_pending = false;
    if(fzc_read(fzc_report_path, fzc_pending_text, sizeof(fzc_pending_text))) {
        fzc_pending = fzc_pending_text[0] != 0;
    } else if(rde_file_exists(fzc_running_path)) {
        i32 _reason = -1;
#if defined(RDE_PLATFORM_ANDROID)
        _reason = rde_android_get_last_exit_reason();
#endif
        if(_reason < 0 || fude_crash_reason_reported(_reason)) {
            snprintf(fzc_pending_text, sizeof(fzc_pending_text), "%sWhat: the app ended while on screen (%s), no crash report written\n", fzc_header,
                     _reason >= 0 ? fzc_reason_name(_reason) : "the system gave no reason");
            fzc_pending = true;
        }
    }
#if !defined(_WIN32)
    // (the engine's thread's stack, where a crash's frames are walked: its ends)
#if defined(__APPLE__)
    fzc_stack_hi = (uintptr_t)pthread_get_stackaddr_np(pthread_self());
    fzc_stack_lo = fzc_stack_hi - (uintptr_t)pthread_get_stacksize_np(pthread_self());
#elif defined(__linux__)
    pthread_attr_t _attr;
    if(pthread_getattr_np(pthread_self(), &_attr) == 0) {
        void*  _base = NULL;
        size_t _size = 0;
        if(pthread_attr_getstack(&_attr, &_base, &_size) == 0) {
            fzc_stack_lo = (uintptr_t)_base;
            fzc_stack_hi = (uintptr_t)_base + _size;
        }
        pthread_attr_destroy(&_attr);
    }
#endif
    fzc_set_handlers();
#endif
#if defined(RDE_PLATFORM_ANDROID)
    // (a Java exception nothing catches: written by the Java side, the same file)
    JNIEnv* _env = fude_android_env();
    jmethodID _m = _env != NULL ? fude_android_method(FUDE_JAVA_ANDROID, "crashInstall", "([B[B)V") : NULL;
    if(_m != NULL) {
        jbyteArray _p = fude_android_bytes(_env, fzc_report_path), _h = fude_android_bytes(_env, fzc_header);
        (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_ANDROID), _m, _p, _h);
        fude_android_threw(_env, "crashInstall");
        (*_env)->DeleteLocalRef(_env, _p);
        (*_env)->DeleteLocalRef(_env, _h);
    }
#endif
    fude_crash_session(true);
    fude_crash_note("started");
}

static b8 fzc_marked = false;

void fude_crash_session(b8 _running) {
    if(!fzc_installed || _running == fzc_marked) {
        return;
    }
    fzc_marked = _running;
    if(_running) {
        FILE* _f = fopen(fzc_running_path, "wb");
        if(_f != NULL) {
            fclose(_f);
        }
    } else {
        remove(fzc_running_path);
    }
}

b8 fude_crash_pending(c8* _out, usize _size) {
    if(!fzc_pending || _size == 0u) {
        return false;
    }
    snprintf(_out, _size, "%s", fzc_pending_text);
    return true;
}

void fude_crash_forget(void) {
    fzc_pending = false;
    remove(fzc_report_path);
}

void fude_crash_mailto(const c8* _report, const c8* _to, const c8* _subject, c8* _out, usize _size) {
    if(_size == 0u) {
        return;
    }
    usize _n = (usize)snprintf(_out, _size, "mailto:%s?subject=", _to);
    _n = _n < _size ? _n : _size - 1u;
    // (each byte as it is, or %XX; a character never cut in two: its whole UTF-8 or none of it)
    const c8* _parts[2] = { _subject, _report };
    b8 _full = false;
    for(u32 _k = 0; _k < 2u && !_full; _k++) {
        if(_k == 1u) {
            if(_n + 7u > _size) {
                break;
            }
            memcpy(_out + _n, "&body=", 6u);
            _n += 6u;
        }
        for(const u8* _p = (const u8*)_parts[_k]; *_p != 0;) {
            u32 _len = (*_p & 0x80u) == 0u ? 1u : (*_p & 0xE0u) == 0xC0u ? 2u : (*_p & 0xF0u) == 0xE0u ? 3u : 4u;
            for(u32 _i = 1; _i < _len; _i++) {
                if(_p[_i] == 0) {
                    _len = _i;
                    break;
                }
            }
            const b8 _plain = _len == 1u && ((*_p >= 'A' && *_p <= 'Z') || (*_p >= 'a' && *_p <= 'z') || (*_p >= '0' && *_p <= '9') || *_p == '.' || *_p == '-' || *_p == '_');
            const usize _need = _plain ? 1u : 3u * _len;
            if(_n + _need + 1u > _size) {
                _full = true;   // (no room for it: the rest left out)
                break;
            }
            if(_plain) {
                _out[_n++] = (c8)*_p;
            } else {
                static const c8 _hex[] = "0123456789ABCDEF";
                for(u32 _i = 0; _i < _len; _i++) {
                    _out[_n++] = '%';
                    _out[_n++] = _hex[_p[_i] >> 4];
                    _out[_n++] = _hex[_p[_i] & 0xFu];
                }
            }
            _p += _len;
        }
    }
    _out[_n] = 0;
}
