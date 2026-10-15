// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/base/android.h"

#if defined(RDE_PLATFORM_ANDROID)

// ===========================================================================
// See android.h.
//
// The classes are looked up once, by fude_android_init on the engine's thread, and kept as global references in
// fude_android_classes, indexed by FUDE_JAVA_. Every other function only reads that table, which is why they may be
// called from other threads afterwards.
// ===========================================================================

// The JNI names of the Java classes, in the order of the FUDE_JAVA_ enum in android.h.
static const c8* const FUDE_JAVA_NAMES[FUDE_JAVA_COUNT] = {
    "com/rde/fude/FudeAndroid", "com/rde/fude/FudeSpeech", "com/rde/fude/FudeInk", "com/rde/fude/FudeTranslate",
    "com/rde/fude/FudeText", "com/rde/fude/FudePdf", "com/rde/fude/FudeImport", "com/rde/fude/FudeVideo",
};
// The classes found by fude_android_init, as global references; an entry stays NULL when its class was not found.
static jclass fude_android_classes[FUDE_JAVA_COUNT];
// True once fude_android_init has run, so a second call does nothing.
static b8     fude_android_ready = false;

// Returns RDE's JNI env for the calling thread (RDE attaches the thread to the VM when needed), or NULL.
JNIEnv* fude_android_env(void) {
    return (JNIEnv*)rde_android_get_jni_env();
}

// Finds every class once, keeps each as a global reference, and calls FudeAndroid.init with the activity. A class
// that cannot be found is logged (through fude_android_threw) and its entry stays NULL. The flag is set once there is
// an env to look them up with, so a call made with none (which leaves every class missing) is tried again by the next.
void fude_android_init(void) {
    if(fude_android_ready) {
        return;
    }
    JNIEnv* _env = fude_android_env();
    if(_env == NULL) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "android: no JNI env yet; the Java classes are not looked up");
        return;
    }
    fude_android_ready = true;
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

// Returns the global reference of a class, or NULL when it is missing or _class is out of range.
jclass fude_android_class(FUDE_JAVA_ _class) {
    return _class < FUDE_JAVA_COUNT ? fude_android_classes[_class] : NULL;
}

// Looks up a static method of a class. Returns NULL when there is no env, the class is missing, or the method is not
// found (the last case is logged).
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

// Makes a local byte[] holding _s's UTF-8 bytes without the NUL; a NULL _s gives an empty array. The caller deletes it.
jbyteArray fude_android_bytes(JNIEnv* _env, const c8* _s) {
    const jsize _n = _s != NULL ? (jsize)rde_strlen(_s) : 0;
    jbyteArray  _a = (*_env)->NewByteArray(_env, _n);
    if(_a != NULL && _n > 0) {
        (*_env)->SetByteArrayRegion(_env, _a, 0, _n, (const jbyte*)_s);
    }
    return _a;
}

// Copies a byte[] into _out, NUL-terminated and cut to fit _size, and deletes the local reference to the array.
// Returns how many bytes were copied.
usize fude_android_take_bytes(JNIEnv* _env, jbyteArray _bytes, c8* _out, usize _size) {
    if(_bytes == NULL) {
        if(_size > 0) {
            _out[0] = 0;
        }
        return 0;
    }
    if(_size == 0) {
        (*_env)->DeleteLocalRef(_env, _bytes);   // Still deleted: a native thread's local references are never freed.
        return 0;
    }
    _out[0] = 0;
    jsize _n = (*_env)->GetArrayLength(_env, _bytes);
    if((usize)_n + 1u > _size) {
        _n = (jsize)(_size - 1u);   // Cut to fit; the readers accept UTF-8 that is cut short.
    }
    (*_env)->GetByteArrayRegion(_env, _bytes, 0, _n, (jbyte*)_out);
    _out[_n] = 0;
    (*_env)->DeleteLocalRef(_env, _bytes);
    return (usize)_n;
}

// Copies a byte[] into new memory on the std allocator, NUL-terminated, and deletes the local reference. *_len (may be
// NULL) gets the length without the NUL. Returns NULL for a NULL array or when memory runs out; the local reference is
// deleted in both cases. The caller frees the memory with fude_android_free.
c8* fude_android_take_alloc(JNIEnv* _env, jbyteArray _bytes, usize* _len) {
    if(_len != NULL) {
        *_len = 0;
    }
    if(_bytes == NULL) {
        return NULL;
    }
    const jsize           _n    = (*_env)->GetArrayLength(_env, _bytes);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    c8*                   _s    = (c8*)_heap->malloc(_heap->allocator, (usize)_n + 1u);
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

// Frees memory from fude_android_take_alloc, through the same std allocator. NULL does nothing.
void fude_android_free(any _taken) {
    if(_taken != NULL) {
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        _heap->free(_heap->allocator, _taken);
    }
}

// Returns true when the last JNI call threw; the exception is then printed to the Android log, cleared (so later JNI
// calls work) and logged with _what naming the call. False when nothing was thrown or _env is NULL.
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
