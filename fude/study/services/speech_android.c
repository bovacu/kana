#include "study/services/speech.h"
#include "lang/lang.h"

// ===========================================================================
// See speech.h. Android's side: the device's TextToSpeech in the language taught
// (com.rde.fude.FudeSpeech), started the first time a voice is asked about.
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

static b8 fude_speech_started = false;

b8 fude_speech_available(void) {
    JNIEnv* _env = fude_android_env();
    if(_env == NULL) {
        return false;
    }
    if(!fude_speech_started) {
        fude_speech_started = true;
        jmethodID _start = fude_android_method(FUDE_JAVA_SPEECH, "start", "([B)V");
        if(_start != NULL) {
            jbyteArray _voice = fude_android_bytes(_env, fude_lang_voice());
            (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_SPEECH), _start, _voice);
            fude_android_threw(_env, "FudeSpeech.start");
            (*_env)->DeleteLocalRef(_env, _voice);
        }
    }
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

#endif
