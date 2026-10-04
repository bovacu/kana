// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/doc/pdf.h"

// ===========================================================================
// See pdf.h. Android's: the system's PdfRenderer (reading and drawing) and
// PdfDocument (pictures into one), in com.rde.fude.FudePdf; a PDF's own text and
// a PDF written with the strokes over it by PDFBox (PdfBox-Android, the apps'
// deps.lock), in com.rde.fude.FudePdfBox — a document of its own for the same
// file, opened the first time its text is asked for. A search goes on in a thread
// of FudePdfBox's; its matches are asked for once a frame, kept here, and copied
// out. Positions cross as the page as the PDF reads it (turned as it says): the
// learner's turns are put on here, as pdf.c does for PDFKit.
//
// Drawing may run on doc.c's worker thread: RDE's env attaches it, and every
// local reference made here is let go at once (a native thread's never are).
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct fude_pdf {
    jint            handle;    // FudePdf's (from 0)
    u32             count;
    rde_vec_2F*     sizes;     // each page's, as the PDF reads it, in points
    u8*             turns;     // ...and the learner's quarter turns over that
    fude_pdf_match* found;     // the search's matches as FudePdf last gave them (the page as the PDF reads it)
    u32             found_count;
    b8              found_done;
    jint            found_version;   // FudePdf's count of their changes, when they were given (-1: never)
};

// FudePdf's static method _name, found the first time (into *_cache).
RDE_INTERNAL jmethodID fude_pdf_method(jmethodID* _cache, const c8* _name, const c8* _signature) {
    if(*_cache == NULL) {
        *_cache = fude_android_method(FUDE_JAVA_PDF, _name, _signature);
    }
    return *_cache;
}

// FudePdf's document _handle let go.
RDE_INTERNAL void fude_pdf_shut(jint _handle) {
    JNIEnv*   _env   = fude_android_env();
    jmethodID _close = fude_android_method(FUDE_JAVA_PDF, "close", "(I)V");
    if(_env != NULL && _close != NULL) {
        (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _close, _handle);
        fude_android_threw(_env, "FudePdf.close");
    }
}

b8 fude_pdf_available(void) {
    return fude_android_class(FUDE_JAVA_PDF) != NULL;
}

fude_pdf* fude_pdf_open(const c8* _path) {
    JNIEnv*   _env   = fude_android_env();
    jmethodID _open  = fude_android_method(FUDE_JAVA_PDF, "open", "([B)I");
    jmethodID _sizes = fude_android_method(FUDE_JAVA_PDF, "pageSizes", "(I)[F");
    if(_path == NULL || _path[0] == 0 || _env == NULL || _open == NULL || _sizes == NULL) {
        return NULL;
    }
    jclass     _cls    = fude_android_class(FUDE_JAVA_PDF);
    jbyteArray _bytes  = fude_android_bytes(_env, _path);
    const jint _handle = (*_env)->CallStaticIntMethod(_env, _cls, _open, _bytes);
    (*_env)->DeleteLocalRef(_env, _bytes);
    if(fude_android_threw(_env, "FudePdf.open") || _handle < 0) {   // FudePdf: a handle from 0, -1 when it could not
        return NULL;
    }
    jfloatArray _s = (jfloatArray)(*_env)->CallStaticObjectMethod(_env, _cls, _sizes, _handle);
    const jsize _n = !fude_android_threw(_env, "FudePdf.pageSizes") && _s != NULL ? (*_env)->GetArrayLength(_env, _s) / 2 : 0;
    if(_n <= 0) {
        if(_s != NULL) {
            (*_env)->DeleteLocalRef(_env, _s);
        }
        fude_pdf_shut(_handle);
        return NULL;
    }
    fude_pdf* _pdf = (fude_pdf*)calloc(1, sizeof(fude_pdf));
    _pdf->handle   = _handle;
    _pdf->count    = (u32)_n;
    _pdf->sizes    = (rde_vec_2F*)calloc((usize)_n, sizeof(rde_vec_2F));
    _pdf->turns    = (u8*)calloc((usize)_n, sizeof(u8));
    _pdf->found_version = -1;
    jfloat* _f     = (*_env)->GetFloatArrayElements(_env, _s, NULL);
    for(jsize _i = 0; _i < _n; _i++) {
        _pdf->sizes[_i] = (rde_vec_2F){ _f[2 * _i] > 1.0f ? _f[2 * _i] : 1.0f, _f[2 * _i + 1] > 1.0f ? _f[2 * _i + 1] : 1.0f };
    }
    (*_env)->ReleaseFloatArrayElements(_env, _s, _f, JNI_ABORT);
    (*_env)->DeleteLocalRef(_env, _s);
    return _pdf;
}

void fude_pdf_close(fude_pdf* _pdf) {
    if(_pdf == NULL) {
        return;
    }
    fude_pdf_shut(_pdf->handle);
    free(_pdf->found);
    free(_pdf->turns);
    free(_pdf->sizes);
    free(_pdf);
}

u32 fude_pdf_page_count(const fude_pdf* _pdf) {
    return _pdf != NULL ? _pdf->count : 0u;
}

rde_vec_2F fude_pdf_page_size(const fude_pdf* _pdf, u32 _page) {
    if(_pdf == NULL || _page >= _pdf->count) {
        return (rde_vec_2F){ 595.0f, 842.0f };
    }
    const rde_vec_2F _s = _pdf->sizes[_page];
    return (_pdf->turns[_page] & 1u) != 0u ? (rde_vec_2F){ _s.y, _s.x } : _s;
}

void fude_pdf_set_turn(fude_pdf* _pdf, u32 _page, u8 _quarters) {
    if(_pdf != NULL && _page < _pdf->count) {
        _pdf->turns[_page] = (u8)(_quarters & 3u);
    }
}

b8 fude_pdf_render(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, u32 _w, u32 _h, u8* _rgba) {
    if(_pdf == NULL || _page >= _pdf->count || _w == 0u || _h == 0u || _rgba == NULL || _size.x <= 0.0f || _size.y <= 0.0f) {
        return false;
    }
    JNIEnv*          _env    = fude_android_env();
    static jmethodID _render = NULL;
    if(_render == NULL) {
        _render = fude_android_method(FUDE_JAVA_PDF, "render", "(IIFFFFIIILjava/nio/ByteBuffer;)Z");
    }
    if(_env == NULL || _render == NULL) {
        return false;
    }
    jobject _out = (*_env)->NewDirectByteBuffer(_env, _rgba, (jlong)_w * (jlong)_h * 4);
    if(_out == NULL) {
        fude_android_threw(_env, "NewDirectByteBuffer");
        return false;
    }
    const jboolean _ok = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_PDF), _render, _pdf->handle, (jint)_page,
                                                          _from.x, _from.y, _size.x, _size.y, (jint)_w, (jint)_h,
                                                          (jint)_pdf->turns[_page], _out);
    const b8 _threw = fude_android_threw(_env, "FudePdf.render");
    (*_env)->DeleteLocalRef(_env, _out);
    return !_threw && _ok == JNI_TRUE;
}

b8 fude_pdf_from_images(const c8* const* _images, u32 _count, const c8* _out) {
    JNIEnv*   _env  = fude_android_env();
    jmethodID _from = fude_android_method(FUDE_JAVA_PDF, "fromImages", "([B[BI)Z");
    if(_images == NULL || _count == 0u || _out == NULL || _env == NULL || _from == NULL) {
        return false;
    }
    usize _len = 1u;
    for(u32 _i = 0; _i < _count; _i++) {
        _len += strlen(_images[_i]) + 1u;
    }
    c8* _joined = (c8*)calloc(_len, 1u);
    for(u32 _i = 0; _i < _count; _i++) {
        strcat(_joined, _images[_i]);
        strcat(_joined, "\n");
    }
    jbyteArray _paths = fude_android_bytes(_env, _joined);
    jbyteArray _dest  = fude_android_bytes(_env, _out);
    free(_joined);
    const jboolean _ok = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_PDF), _from, _paths, _dest, (jint)FUDE_PDF_IMAGE_PX);
    const b8 _threw = fude_android_threw(_env, "FudePdf.fromImages");
    (*_env)->DeleteLocalRef(_env, _paths);
    (*_env)->DeleteLocalRef(_env, _dest);
    return !_threw && _ok == JNI_TRUE;
}

// --- the learner's turns ---------------------------------------------------------------------
// FudePdfBox's positions are the page as the PDF reads it; pdf.h's, as the learner
// turned it (as pdf.c's for PDFKit).

// A point of the page as turned (from its top-left) on the page as the PDF reads
// it, and back.
RDE_INTERNAL rde_vec_2F fude_pdf_unturn(const fude_pdf* _pdf, u32 _page, rde_vec_2F _p) {
    const f32 _w = _pdf->sizes[_page].x, _h = _pdf->sizes[_page].y;
    switch(_pdf->turns[_page] & 3u) {
        case 1:  return (rde_vec_2F){ _p.y, _h - _p.x };
        case 2:  return (rde_vec_2F){ _w - _p.x, _h - _p.y };
        case 3:  return (rde_vec_2F){ _w - _p.y, _p.x };
        default: return _p;
    }
}

RDE_INTERNAL rde_vec_2F fude_pdf_return(const fude_pdf* _pdf, u32 _page, rde_vec_2F _p) {
    const f32 _w = _pdf->sizes[_page].x, _h = _pdf->sizes[_page].y;
    switch(_pdf->turns[_page] & 3u) {
        case 1:  return (rde_vec_2F){ _h - _p.y, _p.x };
        case 2:  return (rde_vec_2F){ _w - _p.x, _h - _p.y };
        case 3:  return (rde_vec_2F){ _p.y, _w - _p.x };
        default: return _p;
    }
}

// A rectangle (_from, _size) through one of those: the box round its corners.
RDE_INTERNAL void fude_pdf_box(const fude_pdf* _pdf, u32 _page, b8 _to_pdf, rde_vec_2F* _from, rde_vec_2F* _size) {
    const rde_vec_2F _a = { _from->x, _from->y };
    const rde_vec_2F _b = { _from->x + _size->x, _from->y + _size->y };
    const rde_vec_2F _p = _to_pdf ? fude_pdf_unturn(_pdf, _page, _a) : fude_pdf_return(_pdf, _page, _a);
    const rde_vec_2F _q = _to_pdf ? fude_pdf_unturn(_pdf, _page, _b) : fude_pdf_return(_pdf, _page, _b);
    *_from = (rde_vec_2F){ fminf(_p.x, _q.x), fminf(_p.y, _q.y) };
    *_size = (rde_vec_2F){ fabsf(_p.x - _q.x), fabsf(_p.y - _q.y) };
}

// --- its text: FudePdfBox's -------------------------------------------------------------------

b8 fude_pdf_text_available(void) {
    static i8 _available = -1;   // asked once: PDFBox there (the app's deps.lock) and readied
    if(_available < 0) {
        JNIEnv*   _env = fude_android_env();
        jmethodID _ask = fude_android_method(FUDE_JAVA_PDF, "textAvailable", "()Z");
        if(_env == NULL) {
            return false;   // not yet: asked again
        }
        const jboolean _yes = _ask != NULL ? (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_PDF), _ask) : JNI_FALSE;
        _available          = !fude_android_threw(_env, "FudePdf.textAvailable") && _yes == JNI_TRUE ? 1 : 0;
    }
    return _available == 1;
}

b8 fude_pdf_page_has_text(fude_pdf* _pdf, u32 _page) {
    static jmethodID _has = NULL;
    JNIEnv*          _env = fude_android_env();
    if(_pdf == NULL || _page >= _pdf->count || _env == NULL || fude_pdf_method(&_has, "pageHasText", "(II)Z") == NULL) {
        return false;
    }
    const jboolean _yes = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_PDF), _has, _pdf->handle, (jint)_page);
    return !fude_android_threw(_env, "FudePdf.pageHasText") && _yes == JNI_TRUE;
}

usize fude_pdf_text_in(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, c8* _out, usize _out_size) {
    static jmethodID _in  = NULL;
    JNIEnv*          _env = fude_android_env();
    if(_out_size > 0) {
        _out[0] = 0;
    }
    if(_pdf == NULL || _page >= _pdf->count || _out == NULL || _out_size == 0 || _env == NULL ||
       fude_pdf_method(&_in, "textIn", "(IIFFFF)[B") == NULL) {
        return 0u;
    }
    fude_pdf_box(_pdf, _page, true, &_from, &_size);   // the page as the PDF reads it (FudePdfBox's)
    jbyteArray _bytes = (jbyteArray)(*_env)->CallStaticObjectMethod(_env, fude_android_class(FUDE_JAVA_PDF), _in, _pdf->handle, (jint)_page,
                                                                    _from.x, _from.y, _size.x, _size.y);
    if(fude_android_threw(_env, "FudePdf.textIn")) {
        return 0u;
    }
    usize _len = 0u;
    c8*   _s   = fude_android_take_alloc(_env, _bytes, &_len);
    if(_s == NULL) {
        return 0u;
    }
    usize _n = _len < _out_size - 1u ? _len : _out_size - 1u;
    while(_n > 0 && _n < _len && ((u8)_s[_n] & 0xC0u) == 0x80u) {
        _n--;   // not in the middle of a character
    }
    memcpy(_out, _s, _n);
    _out[_n] = 0;
    free(_s);
    return _n;
}

void fude_pdf_find_start(fude_pdf* _pdf, const c8* _query) {
    static jmethodID _start = NULL;
    JNIEnv*          _env   = fude_android_env();
    if(_pdf == NULL || _env == NULL || fude_pdf_method(&_start, "findStart", "(I[BI)V") == NULL) {
        return;
    }
    _pdf->found_count = 0u;   // the last search's gone at once (the new one's come as asked for)
    _pdf->found_done  = false;
    jbyteArray _q = fude_android_bytes(_env, _query);
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _start, _pdf->handle, _q, (jint)FUDE_PDF_MATCHES);
    fude_android_threw(_env, "FudePdf.findStart");
    (*_env)->DeleteLocalRef(_env, _q);
}

void fude_pdf_find_stop(fude_pdf* _pdf) {
    static jmethodID _stop = NULL;
    JNIEnv*          _env  = fude_android_env();
    if(_pdf == NULL) {
        return;
    }
    _pdf->found_count = 0u;
    _pdf->found_done  = true;
    if(_env != NULL && fude_pdf_method(&_stop, "findStop", "(I)V") != NULL) {
        (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _stop, _pdf->handle);
        fude_android_threw(_env, "FudePdf.findStop");
    }
}

// The search's matches from FudePdf, when they changed since they were last given.
RDE_INTERNAL void fude_pdf_found_take(fude_pdf* _pdf) {
    static jmethodID _matches = NULL;
    JNIEnv*          _env     = fude_android_env();
    if(_env == NULL || fude_pdf_method(&_matches, "findMatches", "(II)[F") == NULL) {
        return;
    }
    jfloatArray _a = (jfloatArray)(*_env)->CallStaticObjectMethod(_env, fude_android_class(FUDE_JAVA_PDF), _matches, _pdf->handle, _pdf->found_version);
    if(fude_android_threw(_env, "FudePdf.findMatches") || _a == NULL) {
        return;   // none changed
    }
    const jsize _len = (*_env)->GetArrayLength(_env, _a);
    jfloat*     _f   = _len >= 3 ? (*_env)->GetFloatArrayElements(_env, _a, NULL) : NULL;
    if(_f != NULL) {
        // version, done, count, then page, x, y, w, h each
        u32 _n = _f[2] > 0.0f ? (u32)_f[2] : 0u;
        _n     = _n < (u32)((_len - 3) / 5) ? _n : (u32)((_len - 3) / 5);
        _n     = _n < FUDE_PDF_MATCHES ? _n : FUDE_PDF_MATCHES;
        if(_n > 0u && _pdf->found == NULL) {
            _pdf->found = (fude_pdf_match*)calloc(FUDE_PDF_MATCHES, sizeof(fude_pdf_match));
        }
        for(u32 _i = 0; _pdf->found != NULL && _i < _n; _i++) {
            const jfloat* _m = &_f[3u + 5u * _i];
            _pdf->found[_i]  = (fude_pdf_match){ _m[0] > 0.0f ? (u32)_m[0] : 0u, { _m[1], _m[2] }, { _m[3], _m[4] } };
        }
        _pdf->found_count   = _pdf->found != NULL ? _n : 0u;
        _pdf->found_done    = _f[1] != 0.0f;
        _pdf->found_version = (jint)_f[0];
        (*_env)->ReleaseFloatArrayElements(_env, _a, _f, JNI_ABORT);
    }
    (*_env)->DeleteLocalRef(_env, _a);
}

u32 fude_pdf_find_matches(fude_pdf* _pdf, fude_pdf_match* _out, u32 _max, b8* _done) {
    *_done = true;
    if(_pdf == NULL) {
        return 0u;
    }
    fude_pdf_found_take(_pdf);
    *_done = _pdf->found_done;
    if(_out == NULL) {
        return _pdf->found_count;
    }
    const u32 _k = _pdf->found_count < _max ? _pdf->found_count : _max;
    for(u32 _i = 0; _i < _k; _i++) {
        _out[_i] = _pdf->found[_i];
        if(_out[_i].page < _pdf->count) {
            fude_pdf_box(_pdf, _out[_i].page, false, &_out[_i].from, &_out[_i].size);   // as the learner turned it
        }
    }
    return _k;
}

// --- writing one: FudePdfBox's ----------------------------------------------------------------

struct fude_pdf_writer {
    jint handle;   // FudePdf's writer (from 0)
};

b8 fude_pdf_write_available(void) {
    return fude_pdf_text_available();   // the same library's
}

fude_pdf_writer* fude_pdf_write_begin(const c8* _out) {
    JNIEnv*   _env   = fude_android_env();
    jmethodID _begin = fude_android_method(FUDE_JAVA_PDF, "writeBegin", "([B)I");
    if(_out == NULL || _out[0] == 0 || _env == NULL || _begin == NULL || !fude_pdf_write_available()) {
        return NULL;
    }
    jbyteArray _path   = fude_android_bytes(_env, _out);
    const jint _handle = (*_env)->CallStaticIntMethod(_env, fude_android_class(FUDE_JAVA_PDF), _begin, _path);
    const b8   _threw  = fude_android_threw(_env, "FudePdf.writeBegin");
    (*_env)->DeleteLocalRef(_env, _path);
    if(_threw || _handle < 0) {
        return NULL;
    }
    fude_pdf_writer* _w = (fude_pdf_writer*)calloc(1, sizeof(fude_pdf_writer));
    _w->handle          = _handle;
    return _w;
}

void fude_pdf_write_page(fude_pdf_writer* _w, fude_pdf* _pdf, u32 _page) {
    static jmethodID _write = NULL;
    JNIEnv*          _env   = fude_android_env();
    if(_w == NULL || _pdf == NULL || _page >= _pdf->count || _env == NULL || fude_pdf_method(&_write, "writePage", "(IIII)V") == NULL) {
        return;
    }
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _write, _w->handle, _pdf->handle, (jint)_page, (jint)_pdf->turns[_page]);
    fude_android_threw(_env, "FudePdf.writePage");
}

void fude_pdf_write_stroke(fude_pdf_writer* _w, const rde_vec_2F* _points, const f32* _radii, u32 _n, rde_color _color, b8 _even) {
    static jmethodID _stroke = NULL;
    JNIEnv*          _env    = fude_android_env();
    if(_w == NULL || _n == 0u || _points == NULL || _radii == NULL || _env == NULL ||
       fude_pdf_method(&_stroke, "writeStroke", "(I[FIZ)V") == NULL) {
        return;
    }
    // x, y and half-width each point; the colour RGBA, a byte each from the top.
    jfloatArray _a = (*_env)->NewFloatArray(_env, (jsize)(3u * _n));
    jfloat*     _f = _a != NULL ? (*_env)->GetFloatArrayElements(_env, _a, NULL) : NULL;
    if(_f == NULL) {
        fude_android_threw(_env, "NewFloatArray");
        if(_a != NULL) {
            (*_env)->DeleteLocalRef(_env, _a);
        }
        return;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        _f[3u * _i]      = _points[_i].x;
        _f[3u * _i + 1u] = _points[_i].y;
        _f[3u * _i + 2u] = _radii[_i];
    }
    (*_env)->ReleaseFloatArrayElements(_env, _a, _f, 0);
    const u32 _rgba = ((u32)_color.r << 24) | ((u32)_color.g << 16) | ((u32)_color.b << 8) | (u32)_color.a;
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _stroke, _w->handle, _a, (jint)_rgba, _even ? JNI_TRUE : JNI_FALSE);
    fude_android_threw(_env, "FudePdf.writeStroke");
    (*_env)->DeleteLocalRef(_env, _a);
}

void fude_pdf_write_hidden_text(fude_pdf_writer* _w, const c8* _text, rde_vec_2F _from, rde_vec_2F _size) {
    static jmethodID _hidden = NULL;
    JNIEnv*          _env    = fude_android_env();
    if(_w == NULL || _text == NULL || _text[0] == 0 || _size.x <= 0.0f || _size.y <= 0.0f || _env == NULL ||
       fude_pdf_method(&_hidden, "writeHiddenText", "(I[BFFFF)V") == NULL) {
        return;
    }
    jbyteArray _t = fude_android_bytes(_env, _text);
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _hidden, _w->handle, _t, _from.x, _from.y, _size.x, _size.y);
    fude_android_threw(_env, "FudePdf.writeHiddenText");
    (*_env)->DeleteLocalRef(_env, _t);
}

void fude_pdf_write_page_end(fude_pdf_writer* _w) {
    static jmethodID _end = NULL;
    JNIEnv*          _env = fude_android_env();
    if(_w == NULL || _env == NULL || fude_pdf_method(&_end, "writePageEnd", "(I)V") == NULL) {
        return;
    }
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _end, _w->handle);
    fude_android_threw(_env, "FudePdf.writePageEnd");
}

b8 fude_pdf_write_end(fude_pdf_writer* _w) {
    if(_w == NULL) {
        return false;
    }
    JNIEnv*   _env = fude_android_env();
    jmethodID _end = fude_android_method(FUDE_JAVA_PDF, "writeEnd", "(I)Z");
    b8        _ok  = false;
    if(_env != NULL && _end != NULL) {
        const jboolean _written = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_PDF), _end, _w->handle);
        _ok = !fude_android_threw(_env, "FudePdf.writeEnd") && _written == JNI_TRUE;
    }
    free(_w);
    return _ok;
}

#endif
