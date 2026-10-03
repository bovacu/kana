#include "study/services/textscan.h"
#include "study/services/mlkit.h"
#include "lang/lang.h"

// ===========================================================================
// See textscan.h. Android's side: ML Kit Text Recognition with the language's
// model, bundled (com.rde.fude.FudeText): a photo from the system's picker, a
// camera frame, a document's page — three channels, each its own result here.
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    fude_textscan_line lines[FUDE_TEXTSCAN_LINES];
    u32                count;
    u32                width, height;
    u8*                jpeg;        // the photo's (channel 0)
    usize              jpeg_size;
} fude_textscan_channel;

static fude_textscan_channel fude_textscan_channels[3];
static b8                    fude_textscan_started = false;

RDE_INTERNAL b8 fude_textscan_ready(void) {
    if(!fude_mlkit_enabled()) {
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

// A channel's lines, as FudeText writes them: "block \t text \t x0 \t y0 ... x3 \t y3" a line each.
RDE_INTERNAL void fude_textscan_parse(fude_textscan_channel* _c, c8* _s) {
    _c->count = 0;
    for(c8* _line = _s; _line != NULL && *_line != 0 && _c->count < FUDE_TEXTSCAN_LINES;) {
        c8* _next = strchr(_line, '\n');
        if(_next != NULL) {
            *_next++ = 0;
        }
        c8* _f[10];
        u32 _n = 0;
        for(c8* _p = _line; _n < 10u;) {
            _f[_n++] = _p;
            c8* _tab = strchr(_p, '\t');
            if(_tab == NULL) {
                break;
            }
            *_tab = 0;
            _p = _tab + 1;
        }
        if(_n == 10u) {
            fude_textscan_line* _l = &_c->lines[_c->count++];
            memset(_l, 0, sizeof(*_l));
            _l->block = (u32)atoi(_f[0]);
            strncpy(_l->text, _f[1], FUDE_TEXTSCAN_TEXT - 1u);
            for(u32 _k = 0; _k < 4u; _k++) {
                _l->corners[_k] = (rde_vec_2F){ (f32)atof(_f[2u + 2u * _k]), (f32)atof(_f[3u + 2u * _k]) };
            }
        }
        _line = _next;
    }
}

// A channel's lines, taken from Java when in: true then.
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
    free(_s);
    _c->width  = (u32)(*_env)->CallStaticIntMethod(_env, _cls, _w, (jint)_channel);
    _c->height = (u32)(*_env)->CallStaticIntMethod(_env, _cls, _h, (jint)_channel);
    fude_android_threw(_env, "FudeText.width");
    return true;
}

RDE_INTERNAL void fude_textscan_result_of(u32 _channel, fude_textscan_result* _out) {
    if(_out == NULL) {
        return;
    }
    const fude_textscan_channel* _c = &fude_textscan_channels[_channel];
    *_out = (fude_textscan_result){
        .image = _channel == 0u ? _c->jpeg : NULL, .image_size = _channel == 0u ? _c->jpeg_size : 0u,
        .width = _c->width, .height = _c->height, .lines = _c->lines, .line_count = _c->count,
    };
}

b8 fude_textscan_available(void) {
    return fude_android_class(FUDE_JAVA_TEXT) != NULL;
}

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
        free(_c->jpeg);
        jbyteArray _bytes = (jbyteArray)(*_env)->CallStaticObjectMethod(_env, fude_android_class(FUDE_JAVA_TEXT), _jpeg);
        _c->jpeg = fude_android_threw(_env, "FudeText.photoJpeg") ? NULL : (u8*)fude_android_take_alloc(_env, _bytes, &_c->jpeg_size);
        if(!fude_textscan_take(0u)) {
            _c->count = 0;
        }
        fude_textscan_result_of(0u, _out);
    }
    return (FUDE_TEXTSCAN_STATE_)_s;
}

// Pixels (a frame: channel 1; a page: channel 2) to Java, copied.
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

b8 fude_textscan_read_frame(const u8* _rgba, u32 _width, u32 _height, f32 _rotation) {
    return fude_textscan_read_pixels(1u, _rgba, _width, _height, _rotation);
}

b8 fude_textscan_poll_frame(fude_textscan_result* _out) {
    if(!fude_textscan_take(1u)) {
        return false;
    }
    fude_textscan_result_of(1u, _out);
    return true;
}

b8 fude_textscan_read_page(const u8* _rgba, u32 _width, u32 _height) {
    return fude_textscan_read_pixels(2u, _rgba, _width, _height, 0.0f);
}

b8 fude_textscan_poll_page(fude_textscan_result* _out) {
    if(!fude_textscan_take(2u)) {
        return false;
    }
    fude_textscan_result_of(2u, _out);
    return true;
}

#endif
