// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/doc/import.h"

// ===========================================================================
// See import.h. Android's: the system's photo picker, its document picker
// (Files: a PDF or pictures) and Google's document scanner (Play services), in
// com.rde.fude.FudeImport. Each leaves copies in the app's cache, polled here
// each update and handed to fude_import_arrived.
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

#include <stdlib.h>
#include <string.h>

RDE_INTERNAL void fude_import_call(const c8* _name) {
    JNIEnv*   _env = fude_android_env();
    jmethodID _m   = fude_android_method(FUDE_JAVA_IMPORT, _name, "()V");
    if(_env != NULL && _m != NULL) {
        (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_IMPORT), _m);
        fude_android_threw(_env, _name);
    }
}

b8 fude_import_platform_photos(void) {
    return fude_android_class(FUDE_JAVA_IMPORT) != NULL;
}

b8 fude_import_platform_camera(void) {
    static i32 _scanner = -1;   // asked once: Play services do not come and go
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

void fude_import_platform_pick_photos(void) {
    fude_import_call("pickPhotos");
}

void fude_import_platform_scan(void) {
    fude_import_call("scan");
}

void fude_import_platform_pick_files(void) {
    fude_import_call("pickFiles");
}

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
    const c8*  _list[64];
    u32        _count = 0;
    for(c8* _p = _joined; _p != NULL && *_p != 0 && _count < 64u;) {
        c8* _next = strchr(_p, '\n');
        if(_next != NULL) {
            *_next++ = 0;
        }
        if(*_p != 0) {
            _list[_count++] = _p;
        }
        _p = _next;
    }
    fude_import_arrived((u8)_k, _list, _count);   // none: cancelled
    free(_joined);
}

#endif
