// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "study/services/textscan.h"
#include "drawing/ink/mlkit.h"
#include "lang/lang.h"

// ===========================================================================
// See textscan.h. Android's side: ML Kit Text Recognition with the language's model, bundled with the app, through
// the static methods of Java's com.rde.fude.FudeText. There are three channels, each with its own result here:
// 0 is a photo from the system's picker, 1 a camera frame, and 2 a document's page.
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

// One channel's last result: its lines, the size of the upright picture they were measured in, and for the photo,
// the picture itself as JPEG.
typedef struct {
    rde_arr TYPE(fude_textscan_line) lines;   // Every line read (std allocator), made with the first; kept for the run.
    u32                width, height;
    u8*                jpeg;        // The photo's JPEG (channel 0 only), freed when the next photo comes in.
    usize              jpeg_size;
} fude_textscan_channel;

// The photo's, the camera frame's and the page's results, by channel number.
static fude_textscan_channel fude_textscan_channels[3];
// Whether FudeText.start has been called, which loads the language's model.
static b8                    fude_textscan_started = false;

// Whether a read may start now: ML Kit is switched on (mlkit.h), the language has a model, and Java is there. Starts
// FudeText with the language's code the first time.
RDE_INTERNAL b8 fude_textscan_ready(void) {
    // A language with no model (Thai, Arabic) has nothing to read with: a read started anyway would never answer, and
    // its page would stay "reading" for good.
    if(!fude_mlkit_enabled() || !fude_lang_text_readable()) {
        return false;
    }
    JNIEnv* _env = fude_android_env();
    if(_env == NULL) {
        return false;
    }
    if(!fude_textscan_started) {
        fude_textscan_started = true;
        jmethodID _start = fude_android_method(FUDE_JAVA_TEXT, "start", "([B)Z");
        if(_start != NULL) {
            jbyteArray _code = fude_android_bytes(_env, fude_lang_code());
            (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_TEXT), _start, _code);
            fude_android_threw(_env, "FudeText.start");
            (*_env)->DeleteLocalRef(_env, _code);
        }
    }
    return fude_android_class(FUDE_JAVA_TEXT) != NULL;
}

// Reads a channel's lines from the text FudeText writes, one line of text each: ten fields separated by tabs, the
// block, the text, then x0, y0 ... x3, y3, the four corners. _s is changed in place. A line without all ten fields
// is skipped; every other one is kept.
RDE_INTERNAL void fude_textscan_parse(fude_textscan_channel* _c, c8* _s) {
    if(!rde_arr_is_inited(&_c->lines)) {
        _c->lines = rde_arr_new(sizeof(fude_textscan_line), rde_memory_allocator_get_default_std());
    }
    rde_arr_clear(&_c->lines);
    for(c8* _line = _s; _line != NULL && *_line != 0;) {
        c8* _next = rde_strchr(_line, '\n');
        if(_next != NULL) {
            *_next++ = 0;
        }
        c8* _f[10];
        u32 _n = 0;
        for(c8* _p = _line; _n < 10u;) {
            _f[_n++] = _p;
            c8* _tab = rde_strchr(_p, '\t');
            if(_tab == NULL) {
                break;
            }
            *_tab = 0;
            _p = _tab + 1;
        }
        if(_n == 10u) {
            fude_textscan_line* _l = (fude_textscan_line*)rde_arr_add_n(&_c->lines, 1u);
            rde_memset(_l, 0, sizeof(*_l));
            _l->block = (u32)(i32)rde_strtol(_f[0], NULL, 10);
            rde_strcpy(_l->text, FUDE_TEXTSCAN_TEXT, _f[1]);
            for(u32 _k = 0; _k < 4u; _k++) {
                _l->corners[_k] = (rde_vec_2F){ (f32)rde_strtod(_f[2u + 2u * _k], NULL), (f32)rde_strtod(_f[3u + 2u * _k], NULL) };
            }
        }
        _line = _next;
    }
}

// Takes a channel's lines from Java once they are in, with the picture's size, and returns true; false while they
// are not in yet.
RDE_INTERNAL b8 fude_textscan_take(u32 _channel) {
    JNIEnv*          _env = fude_android_env();
    static jmethodID _take = NULL, _w = NULL, _h = NULL;
    if(_take == NULL) {
        _take = fude_android_method(FUDE_JAVA_TEXT, "takeLines", "(I)[B");
        _w    = fude_android_method(FUDE_JAVA_TEXT, "width", "(I)I");
        _h    = fude_android_method(FUDE_JAVA_TEXT, "height", "(I)I");
    }
    if(_env == NULL || _take == NULL || _w == NULL || _h == NULL) {
        return false;
    }
    jclass     _cls   = fude_android_class(FUDE_JAVA_TEXT);
    jbyteArray _lines = (jbyteArray)(*_env)->CallStaticObjectMethod(_env, _cls, _take, (jint)_channel);
    if(fude_android_threw(_env, "FudeText.takeLines") || _lines == NULL) {
        return false;
    }
    fude_textscan_channel* _c = &fude_textscan_channels[_channel];
    c8* _s = fude_android_take_alloc(_env, _lines, NULL);
    fude_textscan_parse(_c, _s);
    fude_android_free(_s);
    _c->width  = (u32)(*_env)->CallStaticIntMethod(_env, _cls, _w, (jint)_channel);
    _c->height = (u32)(*_env)->CallStaticIntMethod(_env, _cls, _h, (jint)_channel);
    fude_android_threw(_env, "FudeText.width");
    return true;
}

// Fills _out (when it is not NULL) with a channel's result. Only the photo (channel 0) has an image.
RDE_INTERNAL void fude_textscan_result_of(u32 _channel, fude_textscan_result* _out) {
    if(_out == NULL) {
        return;
    }
    const fude_textscan_channel* _c    = &fude_textscan_channels[_channel];
    const b8                     _some = rde_arr_is_inited(&_c->lines);
    *_out = (fude_textscan_result){
        .image = _channel == 0u ? _c->jpeg : NULL, .image_size = _channel == 0u ? _c->jpeg_size : 0u,
        .width = _c->width, .height = _c->height,
        .lines = _some ? (const fude_textscan_line*)_c->lines.memory : NULL, .line_count = _some ? (u32)rde_arr_length(&_c->lines) : 0u,
    };
}

// See the header. Only a language ML Kit reads in pictures counts; Thai and Arabic have no model.
b8 fude_textscan_available(void) {
    return fude_lang_text_readable() && fude_android_class(FUDE_JAVA_TEXT) != NULL;   // Not Thai or Arabic: no model.
}

// See the header. The system's picker is shown by FudeText; _window is not needed on Android.
b8 fude_textscan_pick(rde_window* _window) {
    RDE_UNUSED(_window);
    if(!fude_textscan_ready()) {
        return false;
    }
    JNIEnv*   _env  = fude_android_env();
    jmethodID _pick = fude_android_method(FUDE_JAVA_TEXT, "pick", "()Z");
    if(_pick == NULL) {
        return false;
    }
    const jboolean _ok = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_TEXT), _pick);
    return !fude_android_threw(_env, "FudeText.pick") && _ok == JNI_TRUE;
}

// See the header. FudeText's photo state is a FUDE_TEXTSCAN_STATE_ number, and FudeText reports DONE, CANCELLED and
// FAILED once. At DONE the photo's JPEG and lines are taken from Java.
FUDE_TEXTSCAN_STATE_ fude_textscan_poll(fude_textscan_result* _out) {
    JNIEnv*          _env   = fude_android_env();
    static jmethodID _state = NULL, _jpeg = NULL;
    if(_state == NULL) {
        _state = fude_android_method(FUDE_JAVA_TEXT, "photoState", "()I");
        _jpeg  = fude_android_method(FUDE_JAVA_TEXT, "photoJpeg", "()[B");
    }
    if(_env == NULL || _state == NULL || _jpeg == NULL) {
        return FUDE_TEXTSCAN_IDLE;
    }
    const jint _s = (*_env)->CallStaticIntMethod(_env, fude_android_class(FUDE_JAVA_TEXT), _state);
    if(fude_android_threw(_env, "FudeText.photoState")) {
        return FUDE_TEXTSCAN_FAILED;
    }
    if(_s == FUDE_TEXTSCAN_DONE) {
        fude_textscan_channel* _c = &fude_textscan_channels[0];
        fude_android_free(_c->jpeg);
        jbyteArray _bytes = (jbyteArray)(*_env)->CallStaticObjectMethod(_env, fude_android_class(FUDE_JAVA_TEXT), _jpeg);
        _c->jpeg = fude_android_threw(_env, "FudeText.photoJpeg") ? NULL : (u8*)fude_android_take_alloc(_env, _bytes, &_c->jpeg_size);
        if(!fude_textscan_take(0u) && rde_arr_is_inited(&_c->lines)) {
            rde_arr_clear(&_c->lines);   // The photo came without its lines: none.
        }
        fude_textscan_result_of(0u, _out);
    }
    return (FUDE_TEXTSCAN_STATE_)_s;
}

// Hands pixels to Java to be read on _channel (1 for a camera frame, 2 for a page), copied into a Java array, with
// the turn that stands them upright rounded to whole degrees from 0 to 359. Returns false when the read cannot start.
RDE_INTERNAL b8 fude_textscan_read_pixels(u32 _channel, const u8* _rgba, u32 _width, u32 _height, f32 _rotation) {
    if(_rgba == NULL || !fude_textscan_ready()) {
        return false;
    }
    JNIEnv*   _env  = fude_android_env();
    jmethodID _read = fude_android_method(FUDE_JAVA_TEXT, "readPixels", "(I[BIII)Z");
    if(_read == NULL) {
        return false;
    }
    const jsize _n = (jsize)(_width * _height * 4u);
    jbyteArray  _a = (*_env)->NewByteArray(_env, _n);
    if(_a == NULL) {
        fude_android_threw(_env, "NewByteArray");
        return false;
    }
    (*_env)->SetByteArrayRegion(_env, _a, 0, _n, (const jbyte*)_rgba);
    i32 _turn = ((i32)(_rotation + 0.5f) % 360 + 360) % 360;
    const jboolean _ok = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_TEXT), _read, (jint)_channel, _a,
                                                          (jint)_width, (jint)_height, (jint)_turn);
    const b8 _threw = fude_android_threw(_env, "FudeText.readPixels");
    (*_env)->DeleteLocalRef(_env, _a);
    return !_threw && _ok == JNI_TRUE;
}

// See the header: a frame is read on channel 1.
b8 fude_textscan_read_frame(const u8* _rgba, u32 _width, u32 _height, f32 _rotation) {
    return fude_textscan_read_pixels(1u, _rgba, _width, _height, _rotation);
}

// See the header.
b8 fude_textscan_poll_frame(fude_textscan_result* _out) {
    if(!fude_textscan_take(1u)) {
        return false;
    }
    fude_textscan_result_of(1u, _out);
    return true;
}

// See the header: a page is read on channel 2, already upright.
b8 fude_textscan_read_page(const u8* _rgba, u32 _width, u32 _height) {
    return fude_textscan_read_pixels(2u, _rgba, _width, _height, 0.0f);
}

// See the header.
b8 fude_textscan_poll_page(fude_textscan_result* _out) {
    if(!fude_textscan_take(2u)) {
        return false;
    }
    fude_textscan_result_of(2u, _out);
    return true;
}

#endif
