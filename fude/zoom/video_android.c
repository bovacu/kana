// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/video.h"

// ===========================================================================
// See video.h. Android's: com.rde.fude.FudeVideo (MediaCodec's H.264 encoder, MediaMuxer's MP4) does the work through
// three static methods, open, add and close, so only one video is made at a time. Each frame is converted to I420
// here (fude_zoom_video_i420) and handed over in one byte[] kept for the whole video (a global reference: the frames
// are big, and come one after another). Elsewhere this file is empty: video.c and video_apple.m are the others.
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

// One video being made: its size in pixels, the frame converted, and the Java array it is copied into.
struct fude_zoom_video {
    u32        width, height;
    rde_arr    yuv;     // TYPE(u8): a frame as I420, width * height * 3 / 2 bytes, on the std allocator.
    jbyteArray frame;   // The same size, a global reference, deleted by fude_zoom_video_close.
};

// See the header: a video can be made when the Java class was found.
b8 fude_zoom_video_available(void) {
    return fude_android_class(FUDE_JAVA_VIDEO) != NULL;
}

// See the header. Sizes under 16 or odd are refused here, since I420 halves both sides. Once Java has begun the file,
// a failure to make the frame's array closes it without keeping it, which deletes the file.
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
    fude_zoom_video* _v = (fude_zoom_video*)rde_calloc(1, sizeof(fude_zoom_video));
    const jsize _size   = (jsize)((usize)_width * _height * 3u / 2u);
    jbyteArray  _local  = (*_env)->NewByteArray(_env, _size);
    _v->width  = _width;
    _v->height = _height;
    _v->yuv    = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_v->yuv, (usize)_size);
    _v->frame  = _local != NULL ? (jbyteArray)(*_env)->NewGlobalRef(_env, _local) : NULL;
    if(_local != NULL) {
        (*_env)->DeleteLocalRef(_env, _local);
    }
    if(_v->frame == NULL) {
        fude_zoom_video_close(_v, false);
        return NULL;
    }
    return _v;
}

// See the header: converts the frame to I420, copies it into the Java array, and hands it to FudeVideo.add.
b8 fude_zoom_video_add(fude_zoom_video* _v, const u8* _rgba, u32 _stride) {
    JNIEnv*   _env = fude_android_env();
    jmethodID _add = fude_android_method(FUDE_JAVA_VIDEO, "add", "([B)Z");
    if(_v == NULL || _rgba == NULL || _env == NULL || _add == NULL) {
        return false;
    }
    fude_zoom_video_i420(_rgba, _v->width, _v->height, _stride, (u8*)_v->yuv.memory);
    (*_env)->SetByteArrayRegion(_env, _v->frame, 0, (jsize)((usize)_v->width * _v->height * 3u / 2u), (const jbyte*)_v->yuv.memory);
    const jboolean _ok = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_VIDEO), _add, _v->frame);
    return !fude_android_threw(_env, "FudeVideo.add") && _ok;
}

// See the header: FudeVideo.close finishes or drops the file; the global reference, the buffer and _v are freed here
// whatever it returns.
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
    if(rde_arr_is_inited(&_v->yuv)) {
        rde_arr_free(&_v->yuv);
    }
    rde_free(_v);
    return _ok && _keep;
}

#endif
