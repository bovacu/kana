#include "study/services/mlkit.h"
#include "lang/lang.h"

// ===========================================================================
// See mlkit.h. Android's side: ML Kit Digital Ink Recognition (com.rde.fude.
// FudeInk), the same model and the same strokes as iOS's (mlkit_ios.m).
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FUDE_MLKIT_STROKE_GAP_MS 300   // strokes carry no clock between them: this much apart

FUDE_MLKIT_ fude_mlkit_state(void) {
    JNIEnv*          _env   = fude_android_env();
    static jmethodID _state = NULL;
    if(_state == NULL) {
        _state = fude_android_method(FUDE_JAVA_INK, "state", "()I");
    }
    if(_env == NULL || _state == NULL) {
        return FUDE_MLKIT_UNAVAILABLE;
    }
    const jint _s = (*_env)->CallStaticIntMethod(_env, fude_android_class(FUDE_JAVA_INK), _state);
    return fude_android_threw(_env, "FudeInk.state") ? FUDE_MLKIT_FAILED : (FUDE_MLKIT_)_s;
}

void fude_mlkit_prepare(void) {
    JNIEnv*   _env     = fude_android_env();
    jmethodID _prepare = fude_android_method(FUDE_JAVA_INK, "prepare", "([B)V");
    if(!fude_mlkit_enabled() || _env == NULL || _prepare == NULL) {
        return;
    }
    jbyteArray _tag = fude_android_bytes(_env, fude_lang_ink_model());
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_INK), _prepare, _tag);
    fude_android_threw(_env, "FudeInk.prepare");
    (*_env)->DeleteLocalRef(_env, _tag);
}

b8 fude_mlkit_recognize(const fude_ink* _ink, const c8* _pre_context) {
    if(!fude_mlkit_enabled() || _ink == NULL || fude_mlkit_state() != FUDE_MLKIT_READY) {
        return false;
    }
    JNIEnv*   _env       = fude_android_env();
    jmethodID _recognize = fude_android_method(FUDE_JAVA_INK, "recognize", "([F[J[I[BFF)Z");
    if(_env == NULL || _recognize == NULL) {
        return false;
    }
    // The alive strokes' points, y down (ML Kit's, as a screen's), each with its clock.
    u32 _points = 0, _strokes = 0;
    for(u32 _s = 0; _s < fude_ink_stroke_count(_ink); _s++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
        if(_stroke->alive && _stroke->point_count > 0) {
            _points += _stroke->point_count;
            _strokes++;
        }
    }
    if(_points == 0) {
        return false;
    }
    f32*   _xy   = (f32*)malloc(sizeof(f32) * 2u * _points);
    jlong* _t    = (jlong*)malloc(sizeof(jlong) * _points);
    jint*  _ends = (jint*)malloc(sizeof(jint) * _strokes);
    if(_xy == NULL || _t == NULL || _ends == NULL) {
        free(_xy); free(_t); free(_ends);
        return false;
    }
    rde_vec_2F _min = { 1e30f, 1e30f }, _max = { -1e30f, -1e30f };
    u32  _p = 0, _e = 0;
    long _clock = 0;
    for(u32 _s = 0; _s < fude_ink_stroke_count(_ink); _s++) {
        const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
        if(!_stroke->alive || _stroke->point_count == 0) {
            continue;
        }
        const fude_ink_point* _pt = fude_ink_stroke_points(_ink, _stroke);
        long                  _last = _clock;
        for(u32 _k = 0; _k < _stroke->point_count; _k++) {
            _last          = _clock + (long)(_pt[_k].time * 1000.0f);
            _xy[2u * _p]      = _pt[_k].position.x;
            _xy[2u * _p + 1u] = -_pt[_k].position.y;
            _t[_p]            = (jlong)_last;
            _min = (rde_vec_2F){ fminf(_min.x, _pt[_k].position.x), fminf(_min.y, _pt[_k].position.y) };
            _max = (rde_vec_2F){ fmaxf(_max.x, _pt[_k].position.x), fmaxf(_max.y, _pt[_k].position.y) };
            _p++;
        }
        _clock     = _last + FUDE_MLKIT_STROKE_GAP_MS;
        _ends[_e++] = (jint)_p;
    }
    jfloatArray _jxy   = (*_env)->NewFloatArray(_env, (jsize)(2u * _points));
    jlongArray  _jt    = (*_env)->NewLongArray(_env, (jsize)_points);
    jintArray   _jends = (*_env)->NewIntArray(_env, (jsize)_strokes);
    (*_env)->SetFloatArrayRegion(_env, _jxy, 0, (jsize)(2u * _points), _xy);
    (*_env)->SetLongArrayRegion(_env, _jt, 0, (jsize)_points, _t);
    (*_env)->SetIntArrayRegion(_env, _jends, 0, (jsize)_strokes, _ends);
    jbyteArray _before = fude_android_bytes(_env, _pre_context != NULL ? _pre_context : "");
    const jboolean _ok = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_INK), _recognize, _jxy, _jt, _jends, _before,
                                                          fmaxf(_max.x - _min.x, 1.0f), fmaxf(_max.y - _min.y, 1.0f));
    const b8 _threw = fude_android_threw(_env, "FudeInk.recognize");
    (*_env)->DeleteLocalRef(_env, _jxy);
    (*_env)->DeleteLocalRef(_env, _jt);
    (*_env)->DeleteLocalRef(_env, _jends);
    (*_env)->DeleteLocalRef(_env, _before);
    free(_xy); free(_t); free(_ends);
    return !_threw && _ok == JNI_TRUE;
}

b8 fude_mlkit_poll(c8* _out, usize _size, f64* _ms) {
    JNIEnv*          _env  = fude_android_env();
    static jmethodID _take = NULL, _took = NULL;
    if(_take == NULL) {
        _take = fude_android_method(FUDE_JAVA_INK, "take", "()[B");
        _took = fude_android_method(FUDE_JAVA_INK, "took", "()D");
    }
    if(_env == NULL || _take == NULL || _took == NULL) {
        return false;
    }
    jbyteArray _answer = (jbyteArray)(*_env)->CallStaticObjectMethod(_env, fude_android_class(FUDE_JAVA_INK), _take);
    if(fude_android_threw(_env, "FudeInk.take") || _answer == NULL) {
        return false;
    }
    if(_out != NULL && _size > 0) {
        fude_android_take_bytes(_env, _answer, _out, _size);
    } else {
        (*_env)->DeleteLocalRef(_env, _answer);
    }
    if(_ms != NULL) {
        *_ms = (f64)(*_env)->CallStaticDoubleMethod(_env, fude_android_class(FUDE_JAVA_INK), _took);
        fude_android_threw(_env, "FudeInk.took");
    }
    return true;
}

#endif
