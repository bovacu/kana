// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/doc/pdf.h"

// ===========================================================================
// See pdf.h.
//
// Android's side, all through static methods of com.rde.fude.FudePdf. Reading and drawing are the system's
// PdfRenderer, and pictures are made into a PDF with PdfDocument. A PDF's own text, its search, and writing a PDF
// with the strokes over it are PDFBox (PdfBox-Android, in the apps' deps.lock), which FudePdf hands on to
// com.rde.fude.FudePdfBox: a document of its own for the same file, opened the first time its text is asked for.
//
// A search runs on a thread of FudePdfBox's ("FudePdfFind"), which also opens PDFBox's document for the text
// (fude_pdf_text_prepare), so the opening never holds a frame. Its matches are asked for once a frame, kept here, and
// copied out.
// Positions cross to Java on the page as the PDF reads it (turned as the PDF says); the learner's turns are applied
// here, as pdf.c does for PDFKit.
//
// Threads: drawing may run on a PDF worker's thread (worker.h: doc.c's or zoom/pdfview.c's), and so may closing an
// orphaned piece's PDF. RDE's env attaches that thread to Java, and every local reference made here is deleted at once,
// because a native thread's local references are never freed on their own.
// ===========================================================================

#if defined(RDE_PLATFORM_ANDROID)

#include "drawing/base/android.h"

// An open PDF on Android: FudePdf's handle for it, each page's size and turns, and the search's matches.
struct fude_pdf {
    jint                         handle;    // FudePdf's handle for the document (from 0).
    u32                          count;
    rde_arr TYPE(rde_vec_2F)     sizes;     // Each page's size in points, as the PDF reads it...
    rde_arr TYPE(u8)             turns;     // ...and the learner's quarter turns on top of that.
    rde_arr TYPE(fude_pdf_match) found;     // The search's matches as FudePdf last gave them, on the page as the PDF
                                            // reads it. Room for FUDE_PDF_MATCHES, made the first time there are any.
    u32                          found_count;
    b8                           found_done;
    jint                         found_version;   // FudePdf's change count when it last gave them (-1: never).
    b8                           text_ready;      // FudePdfBox's document for the text is open (or could not be).
};

// The page sizes, turns and matches as typed arrays, as in pdf.c.
RDE_INTERNAL rde_vec_2F*     fude_pdf_sizes(const fude_pdf* _pdf) { return (rde_vec_2F*)_pdf->sizes.memory; }
RDE_INTERNAL u8*             fude_pdf_turns(const fude_pdf* _pdf) { return (u8*)_pdf->turns.memory; }
RDE_INTERNAL fude_pdf_match* fude_pdf_found(const fude_pdf* _pdf) { return (fude_pdf_match*)_pdf->found.memory; }

// FudePdf's static method _name with _signature, looked up the first time and kept in *_cache.
RDE_INTERNAL jmethodID fude_pdf_method(jmethodID* _cache, const c8* _name, const c8* _signature) {
    if(*_cache == NULL) {
        *_cache = fude_android_method(FUDE_JAVA_PDF, _name, _signature);
    }
    return *_cache;
}

// Closes FudePdf's document _handle.
RDE_INTERNAL void fude_pdf_shut(jint _handle) {
    JNIEnv*   _env   = fude_android_env();
    jmethodID _close = fude_android_method(FUDE_JAVA_PDF, "close", "(I)V");
    if(_env != NULL && _close != NULL) {
        (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _close, _handle);
        fude_android_threw(_env, "FudePdf.close");
    }
}

// Android can show PDFs when the FudePdf class is there.
b8 fude_pdf_available(void) {
    return fude_android_class(FUDE_JAVA_PDF) != NULL;
}

// See the header. FudePdf opens the file and gives every page's size at once, so the worker that draws pages only
// ever draws (pdf.h: Threads). A page smaller than a point is taken as one point.
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
    if(fude_android_threw(_env, "FudePdf.open") || _handle < 0) {   // A handle from 0, or -1 when it failed.
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
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_pdf* _pdf = (fude_pdf*)rde_calloc(1, sizeof(fude_pdf));
    _pdf->handle   = _handle;
    _pdf->count    = (u32)_n;
    _pdf->sizes    = rde_arr_new_with_capacity(sizeof(rde_vec_2F), _n > 0 ? (usize)_n : 1u, _heap);
    _pdf->turns    = rde_arr_new_with_capacity(sizeof(u8), _n > 0 ? (usize)_n : 1u, _heap);
    rde_arr_resize(&_pdf->sizes, (usize)_n);
    rde_arr_resize(&_pdf->turns, (usize)_n);   // Zeroed: no turns yet.
    _pdf->found_version = -1;
    jfloat* _f     = (*_env)->GetFloatArrayElements(_env, _s, NULL);
    for(jsize _i = 0; _i < _n; _i++) {
        fude_pdf_sizes(_pdf)[_i] = (rde_vec_2F){ _f[2 * _i] > 1.0f ? _f[2 * _i] : 1.0f, _f[2 * _i + 1] > 1.0f ? _f[2 * _i + 1] : 1.0f };
    }
    (*_env)->ReleaseFloatArrayElements(_env, _s, _f, JNI_ABORT);
    (*_env)->DeleteLocalRef(_env, _s);
    return _pdf;
}

// See the header. The array of matches is freed only when a search made it.
void fude_pdf_close(fude_pdf* _pdf) {
    if(_pdf == NULL) {
        return;
    }
    fude_pdf_shut(_pdf->handle);
    rde_arr* _arrays[] = { &_pdf->found, &_pdf->turns, &_pdf->sizes };
    for(u32 _i = 0; _i < sizeof(_arrays) / sizeof(_arrays[0]); _i++) {
        if(rde_arr_is_inited(_arrays[_i])) {
            rde_arr_free(_arrays[_i]);
        }
    }
    rde_free(_pdf);
}

// The number of pages, 0 for NULL.
u32 fude_pdf_page_count(const fude_pdf* _pdf) {
    return _pdf != NULL ? _pdf->count : 0u;
}

// See the header. An odd number of the learner's quarter turns swaps the width and the height.
rde_vec_2F fude_pdf_page_size(const fude_pdf* _pdf, u32 _page) {
    if(_pdf == NULL || _page >= _pdf->count) {
        return (rde_vec_2F){ 595.0f, 842.0f };
    }
    const rde_vec_2F _s = fude_pdf_sizes(_pdf)[_page];
    return (fude_pdf_turns(_pdf)[_page] & 1u) != 0u ? (rde_vec_2F){ _s.y, _s.x } : _s;
}

// See the header. Only the turn modulo four is kept.
void fude_pdf_set_turn(fude_pdf* _pdf, u32 _page, u8 _quarters) {
    if(_pdf != NULL && _page < _pdf->count) {
        fude_pdf_turns(_pdf)[_page] = (u8)(_quarters & 3u);
    }
}

// See the header. PdfRenderer draws straight into _rgba through a direct ByteBuffer, with the learner's turn passed
// along. It may run on a worker thread.
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
                                                          (jint)fude_pdf_turns(_pdf)[_page], _out);
    const b8 _threw = fude_android_threw(_env, "FudePdf.render");
    (*_env)->DeleteLocalRef(_env, _out);
    return !_threw && _ok == JNI_TRUE;
}

// See the header. FudePdf.fromImages does the work, with PdfDocument.
b8 fude_pdf_from_images(const c8* const* _images, u32 _count, const c8* _out) {
    JNIEnv*   _env  = fude_android_env();
    jmethodID _from = fude_android_method(FUDE_JAVA_PDF, "fromImages", "([B[BI)Z");
    if(_images == NULL || _count == 0u || _out == NULL || _env == NULL || _from == NULL) {
        return false;
    }
    // The pictures' paths, one to a line, which is what FudePdf.fromImages reads.
    rde_str _joined = rde_str_new("", rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < _count; _i++) {
        rde_str_append_str(&_joined, _images[_i]);
        rde_str_append_str(&_joined, "\n");
    }
    jbyteArray _paths = fude_android_bytes(_env, rde_str_to_char_ptr(&_joined));
    jbyteArray _dest  = fude_android_bytes(_env, _out);
    rde_str_free(&_joined);
    const jboolean _ok = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_PDF), _from, _paths, _dest, (jint)FUDE_PDF_IMAGE_PX);
    const b8 _threw = fude_android_threw(_env, "FudePdf.fromImages");
    (*_env)->DeleteLocalRef(_env, _paths);
    (*_env)->DeleteLocalRef(_env, _dest);
    return !_threw && _ok == JNI_TRUE;
}

// See the header. FudePdf.pictureJpeg reads the picture with Android's ImageDecoder and gives back its bytes.
b8 fude_picture_bytes(const c8* _path, u32 _max_px, rde_arr* _out) {
    rde_arr_clear(_out);
    JNIEnv*   _env  = fude_android_env();
    jmethodID _read = fude_android_method(FUDE_JAVA_PDF, "pictureJpeg", "([BI)[B");
    if(_path == NULL || _env == NULL || _read == NULL) {
        return false;
    }
    jbyteArray _p    = fude_android_bytes(_env, _path);
    jbyteArray _jpeg = (jbyteArray)(*_env)->CallStaticObjectMethod(_env, fude_android_class(FUDE_JAVA_PDF), _read, _p, (jint)_max_px);
    (*_env)->DeleteLocalRef(_env, _p);
    if(fude_android_threw(_env, "FudePdf.pictureJpeg")) {
        return false;
    }
    usize _n = 0;
    u8* _taken = (u8*)fude_android_take_alloc(_env, _jpeg, &_n);   // NULL when it could not be read.
    const b8 _ok = _taken != NULL && _n > 0u;
    if(_ok) {
        rde_memcpy(rde_arr_add_n(_out, _n), _taken, _n);
    }
    fude_android_free(_taken);
    return _ok;
}

// --- the learner's turns ---------------------------------------------------------------------
// FudePdfBox's positions are on the page as the PDF reads it, and pdf.h's on the page as the learner turned it. These
// convert between the two, as pdf.c's do for PDFKit.

// Takes a point on page _page as the learner turned it to the same point on the page as the PDF reads it, both in
// points from the top-left corner. fude_pdf_return does the opposite.
RDE_INTERNAL rde_vec_2F fude_pdf_unturn(const fude_pdf* _pdf, u32 _page, rde_vec_2F _p) {
    const f32 _w = fude_pdf_sizes(_pdf)[_page].x, _h = fude_pdf_sizes(_pdf)[_page].y;
    switch(fude_pdf_turns(_pdf)[_page] & 3u) {
        case 1:  return (rde_vec_2F){ _p.y, _h - _p.x };
        case 2:  return (rde_vec_2F){ _w - _p.x, _h - _p.y };
        case 3:  return (rde_vec_2F){ _w - _p.y, _p.x };
        default: return _p;
    }
}

// Takes a point on page _page as the PDF reads it to the same point on the page as the learner turned it: the
// opposite of fude_pdf_unturn.
RDE_INTERNAL rde_vec_2F fude_pdf_return(const fude_pdf* _pdf, u32 _page, rde_vec_2F _p) {
    const f32 _w = fude_pdf_sizes(_pdf)[_page].x, _h = fude_pdf_sizes(_pdf)[_page].y;
    switch(fude_pdf_turns(_pdf)[_page] & 3u) {
        case 1:  return (rde_vec_2F){ _h - _p.y, _p.x };
        case 2:  return (rde_vec_2F){ _w - _p.x, _h - _p.y };
        case 3:  return (rde_vec_2F){ _p.y, _w - _p.x };
        default: return _p;
    }
}

// Turns the rectangle _from-_size in place, with fude_pdf_unturn when _to_pdf, else with fude_pdf_return. The result
// is the box round its turned corners.
RDE_INTERNAL void fude_pdf_box(const fude_pdf* _pdf, u32 _page, b8 _to_pdf, rde_vec_2F* _from, rde_vec_2F* _size) {
    const rde_vec_2F _a = { _from->x, _from->y };
    const rde_vec_2F _b = { _from->x + _size->x, _from->y + _size->y };
    const rde_vec_2F _p = _to_pdf ? fude_pdf_unturn(_pdf, _page, _a) : fude_pdf_return(_pdf, _page, _a);
    const rde_vec_2F _q = _to_pdf ? fude_pdf_unturn(_pdf, _page, _b) : fude_pdf_return(_pdf, _page, _b);
    *_from = (rde_vec_2F){ rde_math_min_f32(_p.x, _q.x), rde_math_min_f32(_p.y, _q.y) };
    *_size = (rde_vec_2F){ rde_math_abs_f32(_p.x - _q.x), rde_math_abs_f32(_p.y - _q.y) };
}

// --- its text: FudePdfBox's -------------------------------------------------------------------

// See the header. Asked of FudePdf once and remembered.
b8 fude_pdf_text_available(void) {
    static i8 _available = -1;   // Asked once: whether PDFBox is there (the app's deps.lock) and ready.
    if(_available < 0) {
        JNIEnv*   _env = fude_android_env();
        jmethodID _ask = fude_android_method(FUDE_JAVA_PDF, "textAvailable", "()Z");
        if(_env == NULL) {
            return false;   // No Java yet: it is asked again next time.
        }
        const jboolean _yes = _ask != NULL ? (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_PDF), _ask) : JNI_FALSE;
        _available          = !fude_android_threw(_env, "FudePdf.textAvailable") && _yes == JNI_TRUE ? 1 : 0;
    }
    return _available == 1;
}

// See the header. Asks FudePdfBox, through FudePdf, until it says ready, then remembers it: an open document stays
// open until the PDF closes.
b8 fude_pdf_text_prepare(fude_pdf* _pdf) {
    static jmethodID _prepare = NULL;
    JNIEnv*          _env     = fude_android_env();
    if(_pdf == NULL || _pdf->text_ready) {
        return true;
    }
    if(_env == NULL || fude_pdf_method(&_prepare, "textPrepare", "(I)Z") == NULL) {
        return true;   // (nothing to wait for: the questions answer at once, with nothing)
    }
    const jboolean _ready = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_PDF), _prepare, _pdf->handle);
    _pdf->text_ready      = fude_android_threw(_env, "FudePdf.textPrepare") || _ready == JNI_TRUE;
    return _pdf->text_ready;
}

// See the header. Asks FudePdfBox, through FudePdf.
b8 fude_pdf_page_has_text(fude_pdf* _pdf, u32 _page) {
    static jmethodID _has = NULL;
    JNIEnv*          _env = fude_android_env();
    if(_pdf == NULL || _page >= _pdf->count || _env == NULL || fude_pdf_method(&_has, "pageHasText", "(II)Z") == NULL) {
        return false;
    }
    const jboolean _yes = (*_env)->CallStaticBooleanMethod(_env, fude_android_class(FUDE_JAVA_PDF), _has, _pdf->handle, (jint)_page);
    return !fude_android_threw(_env, "FudePdf.pageHasText") && _yes == JNI_TRUE;
}

// See the header. The box is turned back to the page as the PDF reads it, and the text that comes back is cut to fit
// _out without cutting a character in half.
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
    fude_pdf_box(_pdf, _page, true, &_from, &_size);   // To the page as the PDF reads it, as FudePdfBox wants.
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
        _n--;   // Do not cut in the middle of a character.
    }
    rde_memcpy(_out, _s, _n);
    _out[_n] = 0;
    fude_android_free(_s);
    return _n;
}

// See the header. The search runs on FudePdfBox's own thread, keeping at most FUDE_PDF_MATCHES matches.
void fude_pdf_find_start(fude_pdf* _pdf, const c8* _query) {
    static jmethodID _start = NULL;
    JNIEnv*          _env   = fude_android_env();
    if(_pdf == NULL || _env == NULL || fude_pdf_method(&_start, "findStart", "(I[BI)V") == NULL) {
        return;
    }
    _pdf->found_count = 0u;   // The last search's matches are gone at once; the new ones come as they are asked for.
    _pdf->found_done  = false;
    jbyteArray _q = fude_android_bytes(_env, _query);
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _start, _pdf->handle, _q, (jint)FUDE_PDF_MATCHES);
    fude_android_threw(_env, "FudePdf.findStart");
    (*_env)->DeleteLocalRef(_env, _q);
}

// See the header. Forgets the matches kept here too.
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

// Takes the search's matches from FudePdf when they have changed since it last gave them (found_version says when).
// They come as one float array: the version, whether the search is done, the count, then page, x, y, w, h for each
// match.
RDE_INTERNAL void fude_pdf_found_take(fude_pdf* _pdf) {
    static jmethodID _matches = NULL;
    JNIEnv*          _env     = fude_android_env();
    if(_env == NULL || fude_pdf_method(&_matches, "findMatches", "(II)[F") == NULL) {
        return;
    }
    jfloatArray _a = (jfloatArray)(*_env)->CallStaticObjectMethod(_env, fude_android_class(FUDE_JAVA_PDF), _matches, _pdf->handle, _pdf->found_version);
    if(fude_android_threw(_env, "FudePdf.findMatches") || _a == NULL) {
        return;   // Nothing has changed.
    }
    const jsize _len = (*_env)->GetArrayLength(_env, _a);
    jfloat*     _f   = _len >= 3 ? (*_env)->GetFloatArrayElements(_env, _a, NULL) : NULL;
    if(_f != NULL) {
        // The version, done and count, then page, x, y, w, h for each match.
        u32 _n = _f[2] > 0.0f ? (u32)_f[2] : 0u;
        _n     = _n < (u32)((_len - 3) / 5) ? _n : (u32)((_len - 3) / 5);
        _n     = _n < FUDE_PDF_MATCHES ? _n : FUDE_PDF_MATCHES;
        if(_n > 0u && !rde_arr_is_inited(&_pdf->found)) {
            _pdf->found = rde_arr_new_with_capacity(sizeof(fude_pdf_match), FUDE_PDF_MATCHES, rde_memory_allocator_get_default_std());
            rde_arr_resize(&_pdf->found, FUDE_PDF_MATCHES);
        }
        for(u32 _i = 0; rde_arr_is_inited(&_pdf->found) && _i < _n; _i++) {
            const jfloat* _m = &_f[3u + 5u * _i];
            fude_pdf_found(_pdf)[_i] = (fude_pdf_match){ _m[0] > 0.0f ? (u32)_m[0] : 0u, { _m[1], _m[2] }, { _m[3], _m[4] } };
        }
        _pdf->found_count   = rde_arr_is_inited(&_pdf->found) ? _n : 0u;
        _pdf->found_done    = _f[1] != 0.0f;
        _pdf->found_version = (jint)_f[0];
        (*_env)->ReleaseFloatArrayElements(_env, _a, _f, JNI_ABORT);
    }
    (*_env)->DeleteLocalRef(_env, _a);
}

// See the header. Each match is turned to the page as the learner turned it as it is copied out.
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
        _out[_i] = fude_pdf_found(_pdf)[_i];
        if(_out[_i].page < _pdf->count) {
            fude_pdf_box(_pdf, _out[_i].page, false, &_out[_i].from, &_out[_i].size);   // As the learner turned it.
        }
    }
    return _k;
}

// --- writing one: FudePdfBox's ----------------------------------------------------------------

// A PDF being written by FudePdfBox.
struct fude_pdf_writer {
    jint handle;   // FudePdf's handle for the writer (from 0).
};

// Writing needs PDFBox, as the text does.
b8 fude_pdf_write_available(void) {
    return fude_pdf_text_available();   // The same library.
}

// See the header.
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
    fude_pdf_writer* _w = (fude_pdf_writer*)rde_calloc(1, sizeof(fude_pdf_writer));
    _w->handle          = _handle;
    return _w;
}

// See the header. FudePdfBox copies the page and turns it as the learner did.
void fude_pdf_write_page(fude_pdf_writer* _w, fude_pdf* _pdf, u32 _page) {
    static jmethodID _write = NULL;
    JNIEnv*          _env   = fude_android_env();
    if(_w == NULL || _pdf == NULL || _page >= _pdf->count || _env == NULL || fude_pdf_method(&_write, "writePage", "(IIII)V") == NULL) {
        return;
    }
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _write, _w->handle, _pdf->handle, (jint)_page, (jint)fude_pdf_turns(_pdf)[_page]);
    fude_android_threw(_env, "FudePdf.writePage");
}

// See the header.
void fude_pdf_write_stroke(fude_pdf_writer* _w, const rde_vec_2F* _points, const f32* _radii, u32 _n, rde_color _color, b8 _even) {
    static jmethodID _stroke = NULL;
    JNIEnv*          _env    = fude_android_env();
    if(_w == NULL || _n == 0u || _points == NULL || _radii == NULL || _env == NULL ||
       fude_pdf_method(&_stroke, "writeStroke", "(I[FIZ)V") == NULL) {
        return;
    }
    // The points go as x, y and half-width for each; the colour as RGBA packed in an int, red in the top byte.
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

// See the header. _t holds the triangles' points, three to a triangle.
void fude_pdf_write_fill(fude_pdf_writer* _w, const rde_vec_2F* _t, u32 _count, rde_color _color) {
    static jmethodID _fill = NULL;
    JNIEnv*          _env  = fude_android_env();
    if(_w == NULL || _count == 0u || _t == NULL || _env == NULL || fude_pdf_method(&_fill, "writeFill", "(I[FI)V") == NULL) {
        return;
    }
    // The corners go as x, y, three to a triangle, each triangle turned the same way round so under the nonzero rule
    // they fill as one shape. The colour goes as RGBA packed in an int, red in the top byte.
    jfloatArray _a = (*_env)->NewFloatArray(_env, (jsize)(6u * _count));
    jfloat*     _f = _a != NULL ? (*_env)->GetFloatArrayElements(_env, _a, NULL) : NULL;
    if(_f == NULL) {
        fude_android_threw(_env, "NewFloatArray");
        if(_a != NULL) {
            (*_env)->DeleteLocalRef(_env, _a);
        }
        return;
    }
    for(u32 _i = 0; _i < _count; _i++) {
        rde_vec_2F _p = _t[_i * 3u], _q = _t[_i * 3u + 1u], _r = _t[_i * 3u + 2u];
        if((_q.x - _p.x) * (_r.y - _p.y) - (_r.x - _p.x) * (_q.y - _p.y) < 0.0f) {
            const rde_vec_2F _s = _q;
            _q = _r;
            _r = _s;
        }
        jfloat* _o = &_f[6u * _i];
        _o[0] = _p.x; _o[1] = _p.y; _o[2] = _q.x; _o[3] = _q.y; _o[4] = _r.x; _o[5] = _r.y;
    }
    (*_env)->ReleaseFloatArrayElements(_env, _a, _f, 0);
    const u32 _rgba = ((u32)_color.r << 24) | ((u32)_color.g << 16) | ((u32)_color.b << 8) | (u32)_color.a;
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _fill, _w->handle, _a, (jint)_rgba);
    fude_android_threw(_env, "FudePdf.writeFill");
    (*_env)->DeleteLocalRef(_env, _a);
}

// See the header. The bytes and the three corners go to FudePdfBox as they are.
void fude_pdf_write_image(fude_pdf_writer* _w, const u8* _bytes, u32 _size, const rde_vec_2F _corners[3]) {
    static jmethodID _image = NULL;
    JNIEnv*          _env   = fude_android_env();
    if(_w == NULL || _bytes == NULL || _size == 0u || _env == NULL || fude_pdf_method(&_image, "writeImage", "(I[B[F)V") == NULL) {
        return;
    }
    jbyteArray  _b = (*_env)->NewByteArray(_env, (jsize)_size);
    jfloatArray _c = (*_env)->NewFloatArray(_env, 6);
    if(_b == NULL || _c == NULL) {
        fude_android_threw(_env, "NewByteArray");
        if(_b != NULL) { (*_env)->DeleteLocalRef(_env, _b); }
        if(_c != NULL) { (*_env)->DeleteLocalRef(_env, _c); }
        return;
    }
    (*_env)->SetByteArrayRegion(_env, _b, 0, (jsize)_size, (const jbyte*)_bytes);
    const jfloat _xy[6] = { _corners[0].x, _corners[0].y, _corners[1].x, _corners[1].y, _corners[2].x, _corners[2].y };
    (*_env)->SetFloatArrayRegion(_env, _c, 0, 6, _xy);
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _image, _w->handle, _b, _c);
    fude_android_threw(_env, "FudePdf.writeImage");
    (*_env)->DeleteLocalRef(_env, _b);
    (*_env)->DeleteLocalRef(_env, _c);
}

// See the header.
void fude_pdf_write_text(fude_pdf_writer* _w, const c8* _text, rde_vec_2F _at, f32 _size, rde_color _color) {
    static jmethodID _seen = NULL;
    JNIEnv*          _env  = fude_android_env();
    if(_w == NULL || _text == NULL || _text[0] == 0 || !(_size > 0.0f) || _env == NULL ||
       fude_pdf_method(&_seen, "writeText", "(I[BFFFI)V") == NULL) {
        return;
    }
    jbyteArray _t = fude_android_bytes(_env, _text);
    const u32 _rgba = ((u32)_color.r << 24) | ((u32)_color.g << 16) | ((u32)_color.b << 8) | (u32)_color.a;
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _seen, _w->handle, _t, _at.x, _at.y, _size, (jint)_rgba);
    fude_android_threw(_env, "FudePdf.writeText");
    (*_env)->DeleteLocalRef(_env, _t);
}

// See the header. FudePdfBox keeps the face with its document, and loads each face's Roboto the first time it is used.
void fude_pdf_write_face(fude_pdf_writer* _w, u8 _face) {
    static jmethodID _set = NULL;
    JNIEnv*          _env = fude_android_env();
    if(_w == NULL || _env == NULL || fude_pdf_method(&_set, "writeFace", "(II)V") == NULL) {
        return;
    }
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _set, _w->handle, (jint)(_face < FUDE_PDF_FACES ? _face : FUDE_PDF_FACE_REGULAR));
    fude_android_threw(_env, "FudePdf.writeFace");
}

// See the header.
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

// See the header.
void fude_pdf_write_page_end(fude_pdf_writer* _w) {
    static jmethodID _end = NULL;
    JNIEnv*          _env = fude_android_env();
    if(_w == NULL || _env == NULL || fude_pdf_method(&_end, "writePageEnd", "(I)V") == NULL) {
        return;
    }
    (*_env)->CallStaticVoidMethod(_env, fude_android_class(FUDE_JAVA_PDF), _end, _w->handle);
    fude_android_threw(_env, "FudePdf.writePageEnd");
}

// See the header. FudePdfBox ends a page still open and says whether the file was written.
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
    rde_free(_w);
    return _ok;
}

#endif
