// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/video.h"

// ===========================================================================
// See video.h. Android's: com.rde.fude.FudeVideo (MediaCodec's H.264 encoder,
// MediaMuxer's MP4), handed each frame as I420 in one byte[] kept for the whole
// video (a global reference: the frames are big, and come one after another).
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

#include <stdlib.h>

struct fude_zoom_video {
    u32        width, height;
    u8*        yuv;
    jbyteArray frame;   // (global)
};

b8 fude_zoom_video_available(void) {
    return fude_android_class(FUDE_JAVA_VIDEO) != NULL;
}

fude_zoom_video* fude_zoom_video_open(const c8* _path, u32 _width, u32 _height, u32 _fps) {
    JNIEnv*   _env  = fude_android_env();
    jmethodID _open = fude_android_method(FUDE_JAVA_VIDEO, "open", "([BIII)Z");
    if(_env == NULL || _open == NULL || _path == NULL || _width < 16u || _height < 16u || (_width % 2u) != 0u || (_height % 2u) != 0u) {
        return NULL;
    }
    jbyteArray _bytes = fude_android_bytes(_env, _path);
    const jboolean _ok = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_VIDEO), _open, _bytes, (jint)_width, (jint)_height, (jint)_fps);
    (*_env)->DeleteLocalRef(_env, _bytes);
    if(fude_android_threw(_env, "FudeVideo.open") || !_ok) {
        return NULL;
    }
    fude_zoom_video* _v = (fude_zoom_video*)calloc(1, sizeof(fude_zoom_video));
    const jsize _size   = (jsize)((usize)_width * _height * 3u / 2u);
    jbyteArray  _local  = (*_env)->NewByteArray(_env, _size);
    _v->width  = _width;
    _v->height = _height;
    _v->yuv    = (u8*)malloc((usize)_size);
    _v->frame  = _local != NULL ? (jbyteArray)(*_env)->NewGlobalRef(_env, _local) : NULL;
    if(_local != NULL) {
        (*_env)->DeleteLocalRef(_env, _local);
    }
    if(_v->yuv == NULL || _v->frame == NULL) {
        fude_zoom_video_close(_v, false);
        return NULL;
    }
    return _v;
}

b8 fude_zoom_video_add(fude_zoom_video* _v, const u8* _rgba, u32 _stride) {
    JNIEnv*   _env = fude_android_env();
    jmethodID _add = fude_android_method(FUDE_JAVA_VIDEO, "add", "([B)Z");
    if(_v == NULL || _rgba == NULL || _env == NULL || _add == NULL) {
        return false;
    }
    fude_zoom_video_i420(_rgba, _v->width, _v->height, _stride, _v->yuv);
    (*_env)->SetByteArrayRegion(_env, _v->frame, 0, (jsize)((usize)_v->width * _v->height * 3u / 2u), (const jbyte*)_v->yuv);
    const jboolean _ok = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_VIDEO), _add, _v->frame);
    return !fude_android_threw(_env, "FudeVideo.add") && _ok;
}

b8 fude_zoom_video_close(fude_zoom_video* _v, b8 _keep) {
    if(_v == NULL) {
        return false;
    }
    JNIEnv*   _env   = fude_android_env();
    jmethodID _close = fude_android_method(FUDE_JAVA_VIDEO, "close", "(Z)Z");
    b8 _ok = false;
    if(_env != NULL && _close != NULL) {
        _ok = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_VIDEO), _close, (jboolean)_keep) != JNI_FALSE;
        _ok = !fude_android_threw(_env, "FudeVideo.close") && _ok;
    }
    if(_env != NULL && _v->frame != NULL) {
        (*_env)->DeleteGlobalRef(_env, _v->frame);
    }
    free(_v->yuv);
    free(_v);
    return _ok && _keep;
}

#endif
