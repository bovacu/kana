// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/fetch.h"

// ===========================================================================
// See fetch.h. Android's: com.rde.fude.FudeAndroid's fetchStart and fetchState (a thread of its own reading the URL
// into <path>.part, renamed when whole), its state polled. The Java side keeps the state and refuses a second download
// while one runs; this file only passes the strings across and reads the state back.
//
// Elsewhere than Android and Apple's platforms (fetch_apple.m) the two functions are stubs: there are no downloads.
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

// See the header. Calls FudeAndroid.fetchStart(url, path) with both as UTF-8 bytes; false also when there is no JNI
// environment or the method is missing, or the Java threw.
b8 fude_fetch_start(const c8* _url, const c8* _path) {
    if(_url == NULL || _path == NULL) {
        return false;
    }
    JNIEnv*   _env = fude_android_env();
    jmethodID _m   = _env != NULL ? fude_android_method(FUDE_JAVA_ANDROID, "fetchStart", "([B[B)Z") : NULL;
    if(_m == NULL) {
        return false;
    }
    jbyteArray _u = fude_android_bytes(_env, _url), _p = fude_android_bytes(_env, _path);
    const jboolean _ok = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_ANDROID), _m, _u, _p);
    (*_env)->DeleteLocalRef(_env, _u);
    (*_env)->DeleteLocalRef(_env, _p);
    return !fude_android_threw(_env, "FudeAndroid.fetchStart") && _ok;
}

// See the header. FudeAndroid.fetchState returns { state, bytes got, bytes in all }; its state numbers (0 idle,
// 1 running, 2 done, 3 failed) are FUDE_FETCH_'s, so the first is cast across. Without Java, or when it threw, the
// state is IDLE.
FUDE_FETCH_ fude_fetch_state(u64* _got, u64* _total) {
    *_got = *_total = 0u;
    JNIEnv*   _env = fude_android_env();
    jmethodID _m   = _env != NULL ? fude_android_method(FUDE_JAVA_ANDROID, "fetchState", "()[J") : NULL;
    if(_m == NULL) {
        return FUDE_FETCH_IDLE;
    }
    jlongArray _a = (jlongArray)(*_env)->CallStaticObjectMethod(_env, fude_android_class(FUDE_JAVA_ANDROID), _m);
    if(fude_android_threw(_env, "FudeAndroid.fetchState") || _a == NULL) {
        return FUDE_FETCH_IDLE;
    }
    jlong _v[3] = { 0, 0, 0 };
    (*_env)->GetLongArrayRegion(_env, _a, 0, 3, _v);
    (*_env)->DeleteLocalRef(_env, _a);
    *_got   = (u64)(_v[1] > 0 ? _v[1] : 0);
    *_total = (u64)(_v[2] > 0 ? _v[2] : 0);
    return (FUDE_FETCH_)_v[0];
}

#elif !defined(__APPLE__)

// No downloads on this platform: never starts one.
b8 fude_fetch_start(const c8* _url, const c8* _path) {
    RDE_UNUSED(_url); RDE_UNUSED(_path);
    return false;
}

// No downloads on this platform: always IDLE, with no bytes.
FUDE_FETCH_ fude_fetch_state(u64* _got, u64* _total) {
    *_got = *_total = 0u;
    return FUDE_FETCH_IDLE;
}

#endif
