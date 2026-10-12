// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/fetch.h"

// ===========================================================================
// See fetch.h. Android's: com.rde.fude.FudeAndroid's fetch* (a thread of its own
// reading the URL into <path>.part, renamed when whole), its state polled.
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

b8 fude_fetch_start(const c8* _url, const c8* _path) {
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

void fude_fetch_cancel(void) {
    JNIEnv*   _env = fude_android_env();
    jmethodID _m   = _env != NULL ? fude_android_method(FUDE_JAVA_ANDROID, "fetchCancel", "()V") : NULL;
    if(_m != NULL) {
        (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_ANDROID), _m);
        fude_android_threw(_env, "FudeAndroid.fetchCancel");
    }
}

#elif !defined(__APPLE__)

// (no downloads here)
b8 fude_fetch_start(const c8* _url, const c8* _path) {
    RDE_UNUSED(_url); RDE_UNUSED(_path);
    return false;
}

FUDE_FETCH_ fude_fetch_state(u64* _got, u64* _total) {
    *_got = *_total = 0u;
    return FUDE_FETCH_IDLE;
}

void fude_fetch_cancel(void) {
}

#endif
