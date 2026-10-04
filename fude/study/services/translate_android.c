// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/services/translate.h"

// ===========================================================================
// See translate.h. Android's side: ML Kit's on-device translator (com.rde.fude.
// FudeTranslate), the platform half translate.c wraps (the Settings switch,
// the demo).
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

static u32 fude_translate_android_ticket = 0;

b8 fude_translate_platform_available(void) {
    return fude_android_class(FUDE_JAVA_TRANSLATE) != NULL;
}

FUDE_TRANSLATE_STATE_ fude_translate_platform_state(const c8* _from, const c8* _to) {
    JNIEnv*          _env   = fude_android_env();
    static jmethodID _state = NULL;
    if(_state == NULL) {
        _state = fude_android_method(FUDE_JAVA_TRANSLATE, "state", "([B[B)I");
    }
    if(_env == NULL || _state == NULL) {
        return FUDE_TRANSLATE_UNAVAILABLE;
    }
    jbyteArray _f = fude_android_bytes(_env, _from), _t = fude_android_bytes(_env, _to);
    const jint _s = (*_env)->CallStaticIntMethod(_env, fude_android_class(FUDE_JAVA_TRANSLATE), _state, _f, _t);
    const b8   _threw = fude_android_threw(_env, "FudeTranslate.state");
    (*_env)->DeleteLocalRef(_env, _f);
    (*_env)->DeleteLocalRef(_env, _t);
    return _threw ? FUDE_TRANSLATE_FAILED : (FUDE_TRANSLATE_STATE_)_s;
}

void fude_translate_platform_prepare(const c8* _from, const c8* _to) {
    JNIEnv*   _env     = fude_android_env();
    jmethodID _prepare = fude_android_method(FUDE_JAVA_TRANSLATE, "prepare", "([B[B)V");
    if(_env == NULL || _prepare == NULL) {
        return;
    }
    jbyteArray _f = fude_android_bytes(_env, _from), _t = fude_android_bytes(_env, _to);
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_TRANSLATE), _prepare, _f, _t);
    fude_android_threw(_env, "FudeTranslate.prepare");
    (*_env)->DeleteLocalRef(_env, _f);
    (*_env)->DeleteLocalRef(_env, _t);
}

u32 fude_translate_platform_text(const c8* _text, const c8* _from, const c8* _to) {
    if(fude_translate_platform_state(_from, _to) != FUDE_TRANSLATE_READY) {
        return 0u;
    }
    JNIEnv*   _env       = fude_android_env();
    jmethodID _translate = fude_android_method(FUDE_JAVA_TRANSLATE, "translate", "(I[B[B[B)V");
    if(_env == NULL || _translate == NULL) {
        return 0u;
    }
    const u32 _ticket = ++fude_translate_android_ticket != 0u ? fude_translate_android_ticket : ++fude_translate_android_ticket;
    jbyteArray _s = fude_android_bytes(_env, _text), _f = fude_android_bytes(_env, _from), _t = fude_android_bytes(_env, _to);
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_TRANSLATE), _translate, (jint)_ticket, _s, _f, _t);
    const b8 _threw = fude_android_threw(_env, "FudeTranslate.translate");
    (*_env)->DeleteLocalRef(_env, _s);
    (*_env)->DeleteLocalRef(_env, _f);
    (*_env)->DeleteLocalRef(_env, _t);
    return _threw ? 0u : _ticket;
}

b8 fude_translate_platform_poll(u32* _ticket, c8* _out, usize _size) {
    JNIEnv*          _env    = fude_android_env();
    static jmethodID _ticket_m = NULL, _text_m = NULL;
    if(_ticket_m == NULL) {
        _ticket_m = fude_android_method(FUDE_JAVA_TRANSLATE, "takeTicket", "()I");
        _text_m   = fude_android_method(FUDE_JAVA_TRANSLATE, "takeText", "()[B");
    }
    if(_env == NULL || _ticket_m == NULL || _text_m == NULL) {
        return false;
    }
    const jint _t = (*_env)->CallStaticIntMethod(_env, fude_android_class(FUDE_JAVA_TRANSLATE), _ticket_m);
    if(fude_android_threw(_env, "FudeTranslate.takeTicket") || _t == 0) {
        return false;
    }
    jbyteArray _text = (jbyteArray)(*_env)->CallStaticObjectMethod(_env, fude_android_class(FUDE_JAVA_TRANSLATE), _text_m);
    if(fude_android_threw(_env, "FudeTranslate.takeText")) {
        return false;
    }
    *_ticket = (u32)_t;
    fude_android_take_bytes(_env, _text, _out, _size);
    return true;
}

#endif
