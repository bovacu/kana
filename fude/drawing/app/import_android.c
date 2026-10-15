// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/app/import.h"

// ===========================================================================
// See import.h. This is Android's side: the system's photo picker, its document picker (Files: a PDF or pictures) and
// Google's document scanner (from Play services), all in com.rde.fude.FudeImport. Each leaves copies of what was
// picked in the app's cache. The Java side only stores them; fude_import_platform_poll fetches them each update and
// hands them to fude_import_arrived, so they reach the C side on the main thread.
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

// Calls FudeImport's static method _name, which takes nothing and returns nothing, and logs it if it threw.
RDE_INTERNAL void fude_import_call(const c8* _name) {
    JNIEnv*   _env = fude_android_env();
    jmethodID _m   = fude_android_method(FUDE_JAVA_IMPORT, _name, "()V");
    if(_env != NULL && _m != NULL) {
        (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_IMPORT), _m);
        fude_android_threw(_env, _name);
    }
}

// The photo picker is there whenever the FudeImport class is.
b8 fude_import_platform_photos(void) {
    return fude_android_class(FUDE_JAVA_IMPORT) != NULL;
}

// Whether Google's document scanner can run here, asked of Java once.
b8 fude_import_platform_camera(void) {
    static i32 _scanner = -1;   // -1 until asked. Asked once, because Play services do not come and go.
    if(_scanner < 0) {
        JNIEnv*   _env = fude_android_env();
        jmethodID _m   = fude_android_method(FUDE_JAVA_IMPORT, "scanAvailable", "()Z");
        _scanner       = 0;
        if(_env != NULL && _m != NULL) {
            const jboolean _ok = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_IMPORT), _m);
            _scanner           = !fude_android_threw(_env, "FudeImport.scanAvailable") && _ok == JNI_TRUE ? 1 : 0;
        }
    }
    return _scanner == 1;
}

// Shows the system's photo picker.
void fude_import_platform_pick_photos(void) {
    fude_import_call("pickPhotos");
}

// Shows Google's document scanner.
void fude_import_platform_scan(void) {
    fude_import_call("scan");
}

// Shows the system's document picker, for a PDF or pictures.
void fude_import_platform_pick_files(void) {
    fude_import_call("pickFiles");
}

// Takes what a picker left, if anything: its kind (FUDE_IMPORT_; below 0 when nothing came in) and its paths, which
// arrive as one UTF-8 byte array with a '\n' after each. Hands them to fude_import_arrived; none means cancelled.
void fude_import_platform_poll(void) {
    JNIEnv*          _env  = fude_android_env();
    static jmethodID _kind = NULL, _paths = NULL;
    if(_kind == NULL) {
        _kind  = fude_android_method(FUDE_JAVA_IMPORT, "takeKind", "()I");
        _paths = fude_android_method(FUDE_JAVA_IMPORT, "takePaths", "()[B");
    }
    if(_env == NULL || _kind == NULL || _paths == NULL) {
        return;
    }
    jclass     _cls = fude_android_class(FUDE_JAVA_IMPORT);
    const jint _k   = (*_env)->CallStaticIntMethod(_env, _cls, _kind);
    if(fude_android_threw(_env, "FudeImport.takeKind") || _k < 0) {
        return;
    }
    jbyteArray _bytes  = (jbyteArray)(*_env)->CallStaticObjectMethod(_env, _cls, _paths);
    c8*        _joined = fude_android_threw(_env, "FudeImport.takePaths") ? NULL : fude_android_take_alloc(_env, _bytes, NULL);
    rde_arr TYPE(const c8*) _list = rde_arr_new(sizeof(const c8*), rde_memory_allocator_get_default_std());   // Picked.
    for(c8* _p = _joined; _p != NULL && *_p != 0;) {
        c8* _next = rde_strchr(_p, '\n');
        if(_next != NULL) {
            *_next++ = 0;
        }
        if(*_p != 0) {
            rde_arr_add(&_list, (any)&_p);
        }
        _p = _next;
    }
    fude_import_arrived((u8)_k, (const c8**)_list.memory, (u32)rde_arr_length(&_list));   // None means cancelled.
    rde_arr_free(&_list);
    fude_android_free(_joined);
}

#endif
