// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ANDROID
#define FUDE_ANDROID

#include "rde.h"

// ===========================================================================
// Android: the apps' own Java (fude/android/java, com.rde.fude — the builder's
// --android_java), reached through RDE's JNI env (rde_android_get_jni_env).
// The services' Android sides (study/services/*_android.c, doc/*_android.c)
// call its static methods on the engine's thread and poll what they leave;
// nothing calls back into C. Text crosses as UTF-8 bytes (byte[]), never as
// jstrings: JNI's modified UTF-8 has no 4-byte characters, and CJK's extension
// characters are exactly those.
//
// The classes are found once, at start, on the engine's thread (fude_android_init):
// FindClass on any other thread (doc.c's worker) sees the system's classes only.
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include <jni.h>

typedef enum {
    FUDE_JAVA_ANDROID = 0,   // com/rde/fude/FudeAndroid
    FUDE_JAVA_SPEECH,
    FUDE_JAVA_INK,
    FUDE_JAVA_TRANSLATE,
    FUDE_JAVA_TEXT,
    FUDE_JAVA_PDF,
    FUDE_JAVA_IMPORT,
    FUDE_JAVA_COUNT
} FUDE_JAVA_;

// Once, at start, on the engine's thread: the classes found, and the Java side
// handed the activity. Again: nothing.
void    fude_android_init(void);
// The calling thread's env (attached when it was not); NULL when none.
JNIEnv* fude_android_env(void);
// One of the classes (a global reference); NULL when it is missing.
jclass  fude_android_class(FUDE_JAVA_ _class);
// A static method of one of them; NULL (and logged) when missing.
jmethodID fude_android_method(FUDE_JAVA_ _class, const c8* _name, const c8* _signature);
// _s as a byte[] (UTF-8, a local reference; "" for NULL).
jbyteArray fude_android_bytes(JNIEnv* _env, const c8* _s);
// A byte[]'s bytes into _out, NUL-terminated, cut to _size: their length. Lets go
// of the local reference.
usize   fude_android_take_bytes(JNIEnv* _env, jbyteArray _bytes, c8* _out, usize _size);
// A byte[]'s bytes, newly allocated (free()), NUL-terminated; NULL for null. Lets
// go of the local reference.
c8*     fude_android_take_alloc(JNIEnv* _env, jbyteArray _bytes, usize* _len);
// Did the last call throw? Cleared and logged (with _what) when it did.
b8      fude_android_threw(JNIEnv* _env, const c8* _what);

#endif

#endif
