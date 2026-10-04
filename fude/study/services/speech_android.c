// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/services/speech.h"
#include "lang/lang.h"

// ===========================================================================
// See speech.h. Android's side: the device's TextToSpeech in the language taught
// (com.rde.fude.FudeSpeech), started the first time a voice is asked about.
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

static b8 fude_speech_started = false;

// The engine started for the language's voice, the first time it is asked about.
RDE_INTERNAL void fude_speech_start(JNIEnv* _env) {
    if(fude_speech_started) {
        return;
    }
    fude_speech_started = true;
    jmethodID _start = fude_android_method(FUDE_JAVA_SPEECH, "start", "([B)V");
    if(_start != NULL) {
        jbyteArray _voice = fude_android_bytes(_env, fude_lang_voice());
        (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_SPEECH), _start, _voice);
        fude_android_threw(_env, "FudeSpeech.start");
        (*_env)->DeleteLocalRef(_env, _voice);
    }
}

b8 fude_speech_available(void) {
    JNIEnv* _env = fude_android_env();
    if(_env == NULL) {
        return false;
    }
    fude_speech_start(_env);
    static jmethodID _available = NULL;
    if(_available == NULL) {
        _available = fude_android_method(FUDE_JAVA_SPEECH, "available", "()Z");
    }
    if(_available == NULL) {
        return false;
    }
    const jboolean _yes = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_SPEECH), _available);
    return !fude_android_threw(_env, "FudeSpeech.available") && _yes == JNI_TRUE;
}

void fude_speak(const c8* _text) {
    JNIEnv*   _env   = fude_android_env();
    jmethodID _speak = fude_android_method(FUDE_JAVA_SPEECH, "speak", "([B)V");
    if(_env == NULL || _speak == NULL || _text == NULL) {
        return;
    }
    jbyteArray _bytes = fude_android_bytes(_env, _text);
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_SPEECH), _speak, _bytes);
    fude_android_threw(_env, "FudeSpeech.speak");
    (*_env)->DeleteLocalRef(_env, _bytes);
}

void fude_speech_stop(void) {
    JNIEnv*   _env  = fude_android_env();
    jmethodID _stop = fude_android_method(FUDE_JAVA_SPEECH, "stop", "()V");
    if(_env != NULL && _stop != NULL) {
        (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_SPEECH), _stop);
        fude_android_threw(_env, "FudeSpeech.stop");
    }
}

b8 fude_speech_take_hint(void) {
    return false;   // the device's voices are its own business on Android
}

FUDE_SPEECH_ fude_speech_state(void) {
    JNIEnv* _env = fude_android_env();
    if(_env == NULL) {
        return FUDE_SPEECH_NONE;
    }
    fude_speech_start(_env);
    // Back after a while away (the app's frames stop in the background: Settings,
    // where a voice may just have been installed) with none: the engine asked again.
    static f64 _last = 0.0;
    const f64  _now  = rde_engine_get_time_now();
    const b8   _back = _last > 0.0 && _now - _last > 1.5;
    _last = _now;
    static jmethodID _state = NULL, _recheck = NULL;
    if(_state == NULL)   { _state   = fude_android_method(FUDE_JAVA_SPEECH, "state", "()I"); }
    if(_recheck == NULL) { _recheck = fude_android_method(FUDE_JAVA_SPEECH, "recheck", "()V"); }
    if(_state == NULL) {
        return FUDE_SPEECH_NONE;
    }
    jint _s = (*_env)->CallStaticIntMethod(_env, fude_android_class(FUDE_JAVA_SPEECH), _state);
    if(fude_android_threw(_env, "FudeSpeech.state")) {
        return FUDE_SPEECH_NONE;
    }
    if(_back && _s == 3 && _recheck != NULL) {
        (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_SPEECH), _recheck);
        fude_android_threw(_env, "FudeSpeech.recheck");
        fude_speech_started = false;
        fude_speech_start(_env);
        return FUDE_SPEECH_CHECKING;
    }
    return _s == 2 ? FUDE_SPEECH_READY : _s == 3 ? FUDE_SPEECH_MISSING : FUDE_SPEECH_CHECKING;
}

b8 fude_speech_open_settings(void) {
    JNIEnv*   _env  = fude_android_env();
    jmethodID _open = fude_android_method(FUDE_JAVA_SPEECH, "openSettings", "()Z");
    if(_env == NULL || _open == NULL) {
        return false;
    }
    const jboolean _ok = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_SPEECH), _open);
    return !fude_android_threw(_env, "FudeSpeech.openSettings") && _ok == JNI_TRUE;
}

#endif
