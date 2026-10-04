// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/android.h"

#if defined(RDE_PLATFORM_ANDROID)

#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See android.h.
// ===========================================================================

static const c8* const FUDE_JAVA_NAMES[FUDE_JAVA_COUNT] = {
    "com/rde/fude/FudeAndroid", "com/rde/fude/FudeSpeech", "com/rde/fude/FudeInk", "com/rde/fude/FudeTranslate",
    "com/rde/fude/FudeText", "com/rde/fude/FudePdf", "com/rde/fude/FudeImport",
};
static jclass fude_android_classes[FUDE_JAVA_COUNT];
static b8     fude_android_ready = false;

JNIEnv* fude_android_env(void) {
    return (JNIEnv*)rde_android_get_jni_env();
}

void fude_android_init(void) {
    if(fude_android_ready) {
        return;
    }
    fude_android_ready = true;
    JNIEnv* _env = fude_android_env();
    if(_env == NULL) {
        return;
    }
    for(u32 _c = 0; _c < FUDE_JAVA_COUNT; _c++) {
        jclass _local = (*_env)->FindClass(_env, FUDE_JAVA_NAMES[_c]);
        if(fude_android_threw(_env, FUDE_JAVA_NAMES[_c]) || _local == NULL) {
            continue;
        }
        fude_android_classes[_c] = (jclass)(*_env)->NewGlobalRef(_env, _local);
        (*_env)->DeleteLocalRef(_env, _local);
    }
    jobject   _activity = (jobject)rde_android_get_activity();
    jmethodID _init     = fude_android_method(FUDE_JAVA_ANDROID, "init", "(Landroid/app/Activity;)V");
    if(_activity != NULL && _init != NULL) {
        (*_env)->CallStaticVoidMethod(_env, fude_android_classes[FUDE_JAVA_ANDROID], _init, _activity);
        fude_android_threw(_env, "FudeAndroid.init");
    }
    if(_activity != NULL) {
        (*_env)->DeleteLocalRef(_env, _activity);
    }
}

jclass fude_android_class(FUDE_JAVA_ _class) {
    return _class < FUDE_JAVA_COUNT ? fude_android_classes[_class] : NULL;
}

jmethodID fude_android_method(FUDE_JAVA_ _class, const c8* _name, const c8* _signature) {
    JNIEnv* _env = fude_android_env();
    jclass  _cls = fude_android_class(_class);
    if(_env == NULL || _cls == NULL) {
        return NULL;
    }
    jmethodID _m = (*_env)->GetStaticMethodID(_env, _cls, _name, _signature);
    if(fude_android_threw(_env, _name)) {
        return NULL;
    }
    return _m;
}

jbyteArray fude_android_bytes(JNIEnv* _env, const c8* _s) {
    const jsize _n = _s != NULL ? (jsize)strlen(_s) : 0;
    jbyteArray  _a = (*_env)->NewByteArray(_env, _n);
    if(_a != NULL && _n > 0) {
        (*_env)->SetByteArrayRegion(_env, _a, 0, _n, (const jbyte*)_s);
    }
    return _a;
}

usize fude_android_take_bytes(JNIEnv* _env, jbyteArray _bytes, c8* _out, usize _size) {
    if(_size == 0) {
        return 0;
    }
    _out[0] = 0;
    if(_bytes == NULL) {
        return 0;
    }
    jsize _n = (*_env)->GetArrayLength(_env, _bytes);
    if((usize)_n + 1u > _size) {
        _n = (jsize)(_size - 1u);   // cut to fit (the readers take UTF-8 cut short)
    }
    (*_env)->GetByteArrayRegion(_env, _bytes, 0, _n, (jbyte*)_out);
    _out[_n] = 0;
    (*_env)->DeleteLocalRef(_env, _bytes);
    return (usize)_n;
}

c8* fude_android_take_alloc(JNIEnv* _env, jbyteArray _bytes, usize* _len) {
    if(_len != NULL) {
        *_len = 0;
    }
    if(_bytes == NULL) {
        return NULL;
    }
    const jsize _n = (*_env)->GetArrayLength(_env, _bytes);
    c8*         _s = (c8*)malloc((usize)_n + 1u);
    if(_s != NULL) {
        (*_env)->GetByteArrayRegion(_env, _bytes, 0, _n, (jbyte*)_s);
        _s[_n] = 0;
        if(_len != NULL) {
            *_len = (usize)_n;
        }
    }
    (*_env)->DeleteLocalRef(_env, _bytes);
    return _s;
}

b8 fude_android_threw(JNIEnv* _env, const c8* _what) {
    if(_env == NULL || !(*_env)->ExceptionCheck(_env)) {
        return false;
    }
    (*_env)->ExceptionDescribe(_env);
    (*_env)->ExceptionClear(_env);
    rde_log_level(RDE_LOG_LEVEL_ERROR, "android: %s threw", _what != NULL ? _what : "a call");
    return true;
}

#endif
