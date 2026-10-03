#include "drawing/doc/pdf.h"

// ===========================================================================
// See pdf.h. Android's: the system's PdfRenderer (reading and drawing) and
// PdfDocument (pictures into one), in com.rde.fude.FudePdf. Not yet: writing a
// PDF with the strokes over it, and a PDF's own text (no PDFKit here).
//
// Drawing may run on doc.c's worker thread: RDE's env attaches it, and every
// local reference made here is let go at once (a native thread's never are).
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

#include <stdlib.h>
#include <string.h>

struct fude_pdf {
    jint        handle;   // FudePdf's (from 0)
    u32         count;
    rde_vec_2F* sizes;    // each page's, as the PDF reads it, in points
    u8*         turns;    // ...and the learner's quarter turns over that
};

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

// --- not yet on Android: writing, and the PDF's text ----------------------------------------

b8 fude_pdf_write_available(void) {
    return false;
}

fude_pdf_writer* fude_pdf_write_begin(const c8* _out) {
    RDE_UNUSED(_out);
    return NULL;
}

void fude_pdf_write_page(fude_pdf_writer* _w, fude_pdf* _pdf, u32 _page) {
    RDE_UNUSED(_w); RDE_UNUSED(_pdf); RDE_UNUSED(_page);
}

void fude_pdf_write_stroke(fude_pdf_writer* _w, const rde_vec_2F* _points, const f32* _radii, u32 _n, rde_color _color, b8 _even) {
    RDE_UNUSED(_w); RDE_UNUSED(_points); RDE_UNUSED(_radii); RDE_UNUSED(_n); RDE_UNUSED(_color); RDE_UNUSED(_even);
}

void fude_pdf_write_hidden_text(fude_pdf_writer* _w, const c8* _text, rde_vec_2F _from, rde_vec_2F _size) {
    RDE_UNUSED(_w); RDE_UNUSED(_text); RDE_UNUSED(_from); RDE_UNUSED(_size);
}

void fude_pdf_write_page_end(fude_pdf_writer* _w) {
    RDE_UNUSED(_w);
}

b8 fude_pdf_write_end(fude_pdf_writer* _w) {
    RDE_UNUSED(_w);
    return false;
}

b8 fude_pdf_text_available(void) {
    return false;
}

b8 fude_pdf_page_has_text(fude_pdf* _pdf, u32 _page) {
    RDE_UNUSED(_pdf); RDE_UNUSED(_page);
    return false;
}

usize fude_pdf_text_in(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, c8* _out, usize _out_size) {
    RDE_UNUSED(_pdf); RDE_UNUSED(_page); RDE_UNUSED(_from); RDE_UNUSED(_size);
    if(_out_size > 0) {
        _out[0] = 0;
    }
    return 0u;
}

void fude_pdf_find_start(fude_pdf* _pdf, const c8* _query) {
    RDE_UNUSED(_pdf); RDE_UNUSED(_query);
}

void fude_pdf_find_stop(fude_pdf* _pdf) {
    RDE_UNUSED(_pdf);
}

u32 fude_pdf_find_matches(fude_pdf* _pdf, fude_pdf_match* _out, u32 _max, b8* _done) {
    RDE_UNUSED(_pdf); RDE_UNUSED(_out); RDE_UNUSED(_max);
    *_done = true;
    return 0u;
}

#endif
