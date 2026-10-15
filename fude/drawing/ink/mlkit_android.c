// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/ink/mlkit.h"

// ===========================================================================
// See mlkit.h.
//
// Android's side: ML Kit Digital Ink Recognition through the Java class com.rde.fude.FudeInk, with the same model and
// the same strokes as iOS's (mlkit_ios.m). The ink crosses into Java as flat arrays: every point's x and y, every
// point's time in milliseconds, and the index where each stroke ends.
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

#define FUDE_MLKIT_STROKE_GAP_MS 300   // Strokes keep no clock between them; each starts this many ms after the last.

// Asks FudeInk where the model is; UNAVAILABLE when the Java side is missing, FAILED when the call threw. The method
// is looked up once and kept.
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

// Asks FudeInk to get the model for the app's tag ready, downloading it when missing. Nothing while the switch is off.
void fude_mlkit_prepare(void) {
    JNIEnv*   _env     = fude_android_env();
    jmethodID _prepare = fude_android_method(FUDE_JAVA_INK, "prepare", "([B)V");
    if(!fude_mlkit_enabled() || _env == NULL || _prepare == NULL) {
        return;
    }
    jbyteArray _tag = fude_android_bytes(_env, fude_mlkit_model());
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_INK), _prepare, _tag);
    fude_android_threw(_env, "FudeInk.prepare");
    (*_env)->DeleteLocalRef(_env, _tag);
}

// Hands the alive strokes to FudeInk.recognize, with the text before them and the ink's width and height (at least 1).
// Returns false when the switch is off, the model is not ready, there is no point, or Java refused or threw.
b8 fude_mlkit_recognize(const fude_ink* _ink, const c8* _pre_context) {
    if(!fude_mlkit_enabled() || _ink == NULL || fude_mlkit_state() != FUDE_MLKIT_READY) {
        return false;
    }
    JNIEnv*   _env       = fude_android_env();
    jmethodID _recognize = fude_android_method(FUDE_JAVA_INK, "recognize", "([F[J[I[BFF)Z");
    if(_env == NULL || _recognize == NULL) {
        return false;
    }
    // Count the alive strokes' points. They are sent with y pointing down (ML Kit's way, as on a screen), each with
    // its time.
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
    // Lay the ink out as FudeInk takes it: every point's x and y, its time, and each stroke's end. The arrays are on
    // the std allocator, because a page can hold any number of points.
    rde_memory_allocator* _heap     = rde_memory_allocator_get_default_std();
    rde_arr TYPE(f32)     _xy_list  = rde_arr_new_with_capacity(sizeof(f32), 2u * _points, _heap);
    rde_arr TYPE(jlong)   _t_list   = rde_arr_new_with_capacity(sizeof(jlong), _points, _heap);
    rde_arr TYPE(jint)    _end_list = rde_arr_new_with_capacity(sizeof(jint), _strokes > 0u ? _strokes : 1u, _heap);
    rde_arr_resize(&_xy_list, 2u * _points);
    rde_arr_resize(&_t_list, _points);
    rde_arr_resize(&_end_list, _strokes);
    f32*   _xy   = (f32*)_xy_list.memory;
    jlong* _t    = (jlong*)_t_list.memory;
    jint*  _ends = (jint*)_end_list.memory;
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
            _min = (rde_vec_2F){ rde_math_min_f32(_min.x, _pt[_k].position.x), rde_math_min_f32(_min.y, _pt[_k].position.y) };
            _max = (rde_vec_2F){ rde_math_max_f32(_max.x, _pt[_k].position.x), rde_math_max_f32(_max.y, _pt[_k].position.y) };
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
                                                          rde_math_max_f32(_max.x - _min.x, 1.0f), rde_math_max_f32(_max.y - _min.y, 1.0f));
    const b8 _threw = fude_android_threw(_env, "FudeInk.recognize");
    (*_env)->DeleteLocalRef(_env, _jxy);
    (*_env)->DeleteLocalRef(_env, _jt);
    (*_env)->DeleteLocalRef(_env, _jends);
    (*_env)->DeleteLocalRef(_env, _before);
    rde_arr_free(&_xy_list);
    rde_arr_free(&_t_list);
    rde_arr_free(&_end_list);
    return !_threw && _ok == JNI_TRUE;
}

// Takes FudeInk's answer when there is one: copies it into _out and reads how long it took into *_ms. Returns false
// while there is no answer, or when the call threw. The methods are looked up once and kept.
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
