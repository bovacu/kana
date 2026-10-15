// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ANDROID
#define FUDE_ANDROID

#include "rde.h"

// ===========================================================================
// Android: the bridge from C to the apps' own Java code (fude/android/java, package com.rde.fude, which the builder
// adds with --android_java), reached through RDE's JNI env (rde_android_get_jni_env).
//
// Where it sits: drawing/base, compiled only on Android. The Android sides of the services and documents
// (study/services/*_android.c, doc/*_android.c, app/import_android.c, zoom/*_android.c), the app's start (app.c) and
// the crash reporter (crash.c) use it. They call the Java classes' static methods and poll for what those leave
// behind; nothing in Java ever calls back into C.
// Its main types and their lifecycle: the classes are global references found once by fude_android_init and kept for
// the life of the process. Byte arrays made here are local references that the caller deletes; memory handed out by
// fude_android_take_alloc is the caller's, freed with fude_android_free.
// Threads: fude_android_init runs once, at start, on the engine's thread (the app's main thread), because FindClass
// on any other thread (such as doc.c's worker) sees only the system's classes. After that, the other functions may
// be called from another thread too: doc.c's worker draws PDF pages through them (pdf_android.c), with RDE attaching
// the thread to the VM. A native thread's local references are never freed for it, so callers delete each one at once.
// Design: text crosses into and out of Java as UTF-8 bytes (byte[]), never as jstring. JNI's "modified UTF-8" has no
// 4-byte characters, and the CJK extension characters are exactly those.
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include <jni.h>

// The apps' Java classes, in the order of android.c's names: each FUDE_JAVA_<NAME> is com/rde/fude/Fude<Name>.
typedef enum {
    FUDE_JAVA_ANDROID = 0,   // com/rde/fude/FudeAndroid
    FUDE_JAVA_SPEECH,
    FUDE_JAVA_INK,
    FUDE_JAVA_TRANSLATE,
    FUDE_JAVA_TEXT,
    FUDE_JAVA_PDF,
    FUDE_JAVA_IMPORT,
    FUDE_JAVA_VIDEO,
    FUDE_JAVA_COUNT
} FUDE_JAVA_;

// Finds the classes and hands the Java side the activity (FudeAndroid.init). Called once, at start, on the engine's
// thread; any later call does nothing, once a call had an env to look them up with (one without is logged and
// changes nothing). A class that cannot be found is logged and stays NULL.
void    fude_android_init(void);
// Returns the calling thread's JNI env, attaching the thread to the VM when it was not attached yet, or NULL when
// there is none. Any thread.
JNIEnv* fude_android_env(void);
// Returns one of the classes as a global reference, or NULL when it is missing (or fude_android_init has not run).
// The reference lives as long as the process and must not be deleted. Any thread, once fude_android_init has run.
jclass  fude_android_class(FUDE_JAVA_ _class);
// Looks up a static method of one of the classes by name and JNI signature. Returns NULL when the class is missing,
// or when the method is not found (that is logged). Any thread with an env; callers usually keep the result.
jmethodID fude_android_method(FUDE_JAVA_ _class, const c8* _name, const c8* _signature);
// Makes a Java byte[] holding _s's UTF-8 bytes (without the NUL); a NULL _s gives an empty array. The array is a local
// reference that the caller deletes. Returns NULL when Java could not make it.
jbyteArray fude_android_bytes(JNIEnv* _env, const c8* _s);
// Copies a byte[]'s bytes into _out, NUL-terminated and cut to fit _size (the readers accept UTF-8 cut short), and
// deletes the local reference to the array. Returns how many bytes were copied: 0 for a NULL array or a _size of 0.
usize   fude_android_take_bytes(JNIEnv* _env, jbyteArray _bytes, c8* _out, usize _size);
// Copies a byte[]'s bytes into newly allocated memory, NUL-terminated, and deletes the local reference to the array.
// *_len (may be NULL) gets the length without the NUL. Returns NULL for a NULL array or when memory runs out.
// The caller owns the memory and frees it with fude_android_free. It is on the std allocator, not the engine's pool,
// because a photo or a long text can be megabytes, more than the pool is meant for.
c8*     fude_android_take_alloc(JNIEnv* _env, jbyteArray _bytes, usize* _len);
// Frees memory returned by fude_android_take_alloc. NULL is allowed and does nothing.
void    fude_android_free(any _taken);
// Returns true when the last JNI call on _env threw a Java exception; the exception is then described, cleared and
// logged, with _what naming the call. False when nothing was thrown or _env is NULL.
b8      fude_android_threw(JNIEnv* _env, const c8* _what);

#endif

#endif
