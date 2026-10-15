// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/crash.h"
#include "drawing/base/kfile.h"
#include "drawing/base/utf8.h"
#if defined(RDE_PLATFORM_ANDROID)
#include "drawing/base/android.h"
#endif

// ===========================================================================
// See crash.h. The signal handler is RDE's (rde_crash_*); this file is what the app adds around it, through RDE.
// ===========================================================================

// The paths of crash_report.txt and crash_running in the save folder, made at install.
RDE_INTERNAL c8 fude_crash_report_path[RDE_MAX_PATH];
RDE_INTERNAL c8 fude_crash_running_path[RDE_MAX_PATH];
RDE_INTERNAL c8 fude_crash_header[1024];   // The app, its version, the device, the start time: handed to RDE's handler.
RDE_INTERNAL b8 fude_crash_installed = false;
RDE_INTERNAL b8 fude_crash_left = false;    // True when the last session left something to ask about.
// What the last session left, as found at start: its report, or a short report of its death on screen. Cut at
// FUDE_CRASH_TEXT_MAX (crash.h says why that is enough).
RDE_INTERNAL c8 fude_crash_left_text[FUDE_CRASH_TEXT_MAX];

// --- what a report says --------------------------------------------------------------------------

// Returns whether an Android exit reason is one a report is made for. See the header.
b8 fude_crash_reason_reported(i32 _reason) {
    // The ApplicationExitInfo values: SIGNALED 2, LOW_MEMORY 3 (while on screen, so it was the app's own memory),
    // CRASH 4, CRASH_NATIVE 5, ANR 6, INITIALIZATION_FAILURE 7, EXCESSIVE_RESOURCE_USAGE 9.
    return _reason == 2 || _reason == 3 || _reason == 4 || _reason == 5 || _reason == 6 || _reason == 7 || _reason == 9;
}

// Returns the words a report uses for an Android exit reason (the values listed in fude_crash_reason_reported).
RDE_INTERNAL const c8* fude_crash_reason_name(i32 _reason) {
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

// --- at start, and after ---------------------------------------------------------------------------

// Reads a small file's text into _out, NUL-terminated and cut before a whole character to fit _size. Returns false when
// the file cannot be read (usually because there is none).
RDE_INTERNAL b8 fude_crash_read(const c8* _path, c8* _out, usize _size) {
    if(!rde_file_exists(_path)) {
        return false;
    }
    u32 _bytes = 0;
    u8* _data  = fude_file_read(_path, &_bytes);   // (NULL for an empty file too)
    fude_utf8_copy(_out, _size, _data != NULL ? (const c8*)_data : "", _bytes);
    fude_file_free(_data);
    return true;
}

// Makes the paths and the report's header, keeps what the last session left, finds the engine's thread's stack, and
// installs the handlers (and, on Android, the Java side's). See the header.
void fude_crash_install(const c8* _dir, const c8* _app, const c8* _version) {
    if(fude_crash_installed) {
        return;
    }
    fude_crash_installed = true;
    rde_snprintf(fude_crash_report_path, sizeof(fude_crash_report_path), "%s%s", _dir, FUDE_CRASH_REPORT_FILE);
    rde_snprintf(fude_crash_running_path, sizeof(fude_crash_running_path), "%s%s", _dir, FUDE_CRASH_RUNNING_FILE);
    // The report's header is made now, because the handler can only write it out.
    c8 _started[32];
    rde_date_format(rde_date_now_utc(), "%Y-%m-%d %H:%M UTC", _started, sizeof(_started));
    rde_snprintf(fude_crash_header, sizeof(fude_crash_header), "App: %s %s\nDevice: %s, %s %s\nStarted: %s\n", _app, _version, rde_device_get_model(),
             rde_device_get_platform_name(), rde_device_get_os_version(), _started);
    // See what the last session left: a report (it crashed), or the running marker with no report (it died on
    // screen).
    fude_crash_left = false;
    if(fude_crash_read(fude_crash_report_path, fude_crash_left_text, sizeof(fude_crash_left_text))) {
        fude_crash_left = fude_crash_left_text[0] != 0;
    } else if(rde_file_exists(fude_crash_running_path)) {
        i32 _reason = -1;
#if defined(RDE_PLATFORM_ANDROID)
        _reason = rde_android_get_last_exit_reason();
#endif
        if(_reason < 0 || fude_crash_reason_reported(_reason)) {
            rde_snprintf(fude_crash_left_text, sizeof(fude_crash_left_text), "%sWhat: the app ended while on screen (%s), no crash report written\n", fude_crash_header,
                     _reason >= 0 ? fude_crash_reason_name(_reason) : "the system gave no reason");
            fude_crash_left = true;
        }
    }
    // RDE's handler writes the report (the header, what killed the app, where, the notes) from inside the signal.
    rde_crash_install(fude_crash_report_path, fude_crash_header);
#if defined(RDE_PLATFORM_ANDROID)
    // A Java exception that nothing catches is reported by the Java side, into the same file with the same header.
    JNIEnv* _env = fude_android_env();
    jmethodID _m = _env != NULL ? fude_android_method(FUDE_JAVA_ANDROID, "crashInstall", "([B[B)V") : NULL;
    if(_m != NULL) {
        jbyteArray _p = fude_android_bytes(_env, fude_crash_report_path), _h = fude_android_bytes(_env, fude_crash_header);
        (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_ANDROID), _m, _p, _h);
        fude_android_threw(_env, "crashInstall");
        (*_env)->DeleteLocalRef(_env, _p);
        (*_env)->DeleteLocalRef(_env, _h);
    }
#endif
    fude_crash_session(true);
    rde_crash_note("started");
}

// Whether the running marker is there now, as this session last set it.
RDE_INTERNAL b8 fude_crash_marked = false;

// Creates the running marker when the app comes on screen and removes it when the app leaves. See the header.
void fude_crash_session(b8 _running) {
    if(!fude_crash_installed || _running == fude_crash_marked) {
        return;
    }
    fude_crash_marked = _running;
    rde_crash_note(_running ? "on screen" : "to the background");
    if(_running) {
        rde_file* _f = rde_file_open(fude_crash_running_path, RDE_FILE_MODE_WRITE);   // (empty: its being there is the mark)
        if(_f != NULL) {
            rde_file_close(_f);
        }
    } else if(rde_file_exists(fude_crash_running_path)) {
        rde_file_delete(fude_crash_running_path);
    }
}

// Copies what the last session left into _out, when there is something. See the header.
b8 fude_crash_pending(c8* _out, usize _size) {
    if(!fude_crash_left || _size == 0u) {
        return false;
    }
    rde_snprintf(_out, _size, "%s", fude_crash_left_text);
    return true;
}

// Deletes the report and stops offering it. The running marker is not touched: this session's is still in use.
void fude_crash_forget(void) {
    fude_crash_left = false;
    if(rde_file_exists(fude_crash_report_path)) {
        rde_file_delete(fude_crash_report_path);
    }
}

// Builds "mailto:<to>?subject=<subject>&body=<report>" into _out, percent-encoding the subject and the body. Whatever
// does not fit _size is left out, cut between whole characters. See the header.
void fude_crash_mailto(const c8* _report, const c8* _to, const c8* _subject, c8* _out, usize _size) {
    if(_size == 0u) {
        return;
    }
    usize _n = (usize)rde_snprintf(_out, _size, "mailto:%s?subject=", _to);
    _n = _n < _size ? _n : _size - 1u;
    // Letters, digits, '.', '-' and '_' are written as they are, every other byte as %XX. A character is never cut in
    // two: either all of its UTF-8 bytes are written or none of them.
    const c8* _parts[2] = { _subject, _report };
    b8 _full = false;
    for(u32 _k = 0; _k < 2u && !_full; _k++) {
        if(_k == 1u) {
            if(_n + 7u > _size) {
                break;
            }
            rde_memcpy(_out + _n, "&body=", 6u);
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
                _full = true;   // There is no room for it, so the rest is left out.
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
