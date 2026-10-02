#include "drawing/doc/pdf.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See pdf.h. Apple's: Core Graphics' CGPDFDocument (reading and drawing) and
// PDF context (writing), ImageIO for the images — all plain C. Elsewhere: not
// yet.
// ===========================================================================

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <unistd.h>

struct fude_pdf {
    CGPDFDocumentRef doc;
    u32              count;
    rde_vec_2F*      sizes;   // each page's, turned, in points
    c8               path[RDE_MAX_PATH];
    void*            kit;     // its text's document (pdf_kit.m), opened the first time it is asked
    b8               kit_tried;
};

// Its text's document, the first time.
RDE_INTERNAL void* fude_pdf_kit(fude_pdf* _pdf) {
    if(_pdf != NULL && !_pdf->kit_tried) {
        _pdf->kit_tried = true;
        _pdf->kit       = fude_pdf_kit_open(_pdf->path);
    }
    return _pdf != NULL ? _pdf->kit : NULL;
}

b8 fude_pdf_text_available(void) {
    return true;
}

usize fude_pdf_text_in(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, c8* _out, usize _out_size) {
    void* _kit = fude_pdf_kit(_pdf);
    if(_out_size > 0) {
        _out[0] = 0;
    }
    return _kit != NULL ? fude_pdf_kit_text_in(_kit, _page, _from, _size, _out, _out_size) : 0u;
}

void fude_pdf_find_start(fude_pdf* _pdf, const c8* _query) {
    void* _kit = fude_pdf_kit(_pdf);
    if(_kit != NULL) {
        fude_pdf_kit_find_start(_kit, _query);
    }
}

void fude_pdf_find_stop(fude_pdf* _pdf) {
    if(_pdf != NULL && _pdf->kit != NULL) {
        fude_pdf_kit_find_stop(_pdf->kit);
    }
}

u32 fude_pdf_find_matches(fude_pdf* _pdf, fude_pdf_match* _out, u32 _max, b8* _done) {
    *_done = true;
    return _pdf != NULL && _pdf->kit != NULL ? fude_pdf_kit_find_matches(_pdf->kit, _out, _max, _done) : 0u;
}

// _path as a file URL: a relative one from the working folder (a device's: the
// app's resources, where its assets are).
RDE_INTERNAL CFURLRef fude_pdf_url(const c8* _path) {
    c8 _full[RDE_MAX_PATH];
    if(_path[0] != '/') {
        c8 _cwd[RDE_MAX_PATH];
        if(getcwd(_cwd, sizeof(_cwd)) != NULL) {
            snprintf(_full, sizeof(_full), "%s/%s", _cwd, _path);
            _path = _full;
        }
    }
    return CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*)_path, (CFIndex)strlen(_path), false);
}

// How far page _page is turned to be read: 0, 90, 180 or 270 (clockwise).
RDE_INTERNAL i32 fude_pdf_turn(CGPDFPageRef _page) {
    i32 _turn = (i32)CGPDFPageGetRotationAngle(_page) % 360;
    return _turn < 0 ? _turn + 360 : _turn;
}

b8 fude_pdf_available(void) {
    return true;
}

fude_pdf* fude_pdf_open(const c8* _path) {
    if(_path == NULL || _path[0] == 0) {
        return NULL;
    }
    CFURLRef _url = fude_pdf_url(_path);
    if(_url == NULL) {
        return NULL;
    }
    CGPDFDocumentRef _doc = CGPDFDocumentCreateWithURL(_url);
    CFRelease(_url);
    if(_doc == NULL) {
        return NULL;
    }
    // Locked: an empty password opens many (printing and copying limits only).
    if(!CGPDFDocumentIsUnlocked(_doc) && !CGPDFDocumentUnlockWithPassword(_doc, "")) {
        CGPDFDocumentRelease(_doc);
        return NULL;
    }
    const size_t _count = CGPDFDocumentGetNumberOfPages(_doc);
    if(_count == 0) {
        CGPDFDocumentRelease(_doc);
        return NULL;
    }
    fude_pdf* _pdf = (fude_pdf*)calloc(1, sizeof(fude_pdf));
    snprintf(_pdf->path, sizeof(_pdf->path), "%s", _path);
    _pdf->doc      = _doc;
    _pdf->count    = (u32)_count;
    _pdf->sizes    = (rde_vec_2F*)calloc(_count, sizeof(rde_vec_2F));
    for(u32 _i = 0; _i < _pdf->count; _i++) {
        CGPDFPageRef _page = CGPDFDocumentGetPage(_doc, (size_t)_i + 1u);
        if(_page == NULL) {
            _pdf->sizes[_i] = (rde_vec_2F){ 595.0f, 842.0f };   // A4: a page that cannot be read still has a place
            continue;
        }
        const CGRect _box  = CGPDFPageGetBoxRect(_page, kCGPDFCropBox);
        const i32    _turn = fude_pdf_turn(_page);
        const f32    _w    = (f32)fmax(1.0, _box.size.width);
        const f32    _h    = (f32)fmax(1.0, _box.size.height);
        _pdf->sizes[_i]    = _turn == 90 || _turn == 270 ? (rde_vec_2F){ _h, _w } : (rde_vec_2F){ _w, _h };
    }
    return _pdf;
}

void fude_pdf_close(fude_pdf* _pdf) {
    if(_pdf == NULL) {
        return;
    }
    CGPDFDocumentRelease(_pdf->doc);
    if(_pdf->kit != NULL) {
        fude_pdf_kit_close(_pdf->kit);
    }
    free(_pdf->sizes);
    free(_pdf);
}

u32 fude_pdf_page_count(const fude_pdf* _pdf) {
    return _pdf != NULL ? _pdf->count : 0u;
}

rde_vec_2F fude_pdf_page_size(const fude_pdf* _pdf, u32 _page) {
    return _pdf != NULL && _page < _pdf->count ? _pdf->sizes[_page] : (rde_vec_2F){ 595.0f, 842.0f };
}

b8 fude_pdf_render(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, u32 _w, u32 _h, u8* _rgba) {
    if(_pdf == NULL || _page >= _pdf->count || _w == 0u || _h == 0u || _rgba == NULL || _size.x <= 0.0f || _size.y <= 0.0f) {
        return false;
    }
    CGPDFPageRef _p = CGPDFDocumentGetPage(_pdf->doc, (size_t)_page + 1u);
    if(_p == NULL) {
        return false;
    }
    CGColorSpaceRef _space = CGColorSpaceCreateDeviceRGB();
    CGContextRef    _ctx   = CGBitmapContextCreate(_rgba, _w, _h, 8u, (size_t)_w * 4u, _space, (CGBitmapInfo)kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(_space);
    if(_ctx == NULL) {
        return false;
    }
    CGContextSetRGBFillColor(_ctx, 1.0, 1.0, 1.0, 1.0);
    CGContextFillRect(_ctx, CGRectMake(0.0, 0.0, (CGFloat)_w, (CGFloat)_h));
    CGContextSetInterpolationQuality(_ctx, kCGInterpolationHigh);

    // The page as read (points, Y up, its bottom-left corner at 0), to the
    // pixels: the rectangle asked for fills them. (The bitmap's first row is
    // its top.)
    const rde_vec_2F _page_size = _pdf->sizes[_page];
    CGContextScaleCTM(_ctx, (CGFloat)_w / (CGFloat)_size.x, (CGFloat)_h / (CGFloat)_size.y);
    CGContextTranslateCTM(_ctx, -(CGFloat)_from.x, -(CGFloat)(_page_size.y - _from.y - _size.y));
    // The PDF's own space to the page as read: its crop box turned.
    const CGRect   _box  = CGPDFPageGetBoxRect(_p, kCGPDFCropBox);
    const CGFloat  _cx   = _box.origin.x, _cy = _box.origin.y, _cw = _box.size.width, _ch = _box.size.height;
    CGAffineTransform _t = CGAffineTransformMake(1.0, 0.0, 0.0, 1.0, -_cx, -_cy);
    switch(fude_pdf_turn(_p)) {
        case 90:  _t = CGAffineTransformMake(0.0, -1.0, 1.0, 0.0, -_cy, _cw + _cx); break;
        case 180: _t = CGAffineTransformMake(-1.0, 0.0, 0.0, -1.0, _cw + _cx, _ch + _cy); break;
        case 270: _t = CGAffineTransformMake(0.0, 1.0, -1.0, 0.0, _ch + _cy, -_cx); break;
        default:  break;
    }
    CGContextConcatCTM(_ctx, _t);
    CGContextClipToRect(_ctx, _box);
    CGContextDrawPDFPage(_ctx, _p);
    CGContextRelease(_ctx);
    return true;
}

b8 fude_pdf_from_images(const c8* const* _images, u32 _count, const c8* _out) {
    if(_images == NULL || _count == 0u || _out == NULL) {
        return false;
    }
    CFURLRef _url = fude_pdf_url(_out);
    if(_url == NULL) {
        return false;
    }
    CGContextRef _ctx = CGPDFContextCreateWithURL(_url, NULL, NULL);
    CFRelease(_url);
    if(_ctx == NULL) {
        return false;
    }
    // Read turned as taken, no larger than the PDF keeps; written back as JPEG
    // so the PDF keeps it as one (not a far larger lossless copy).
    const i32       _max       = (i32)FUDE_PDF_IMAGE_PX;
    const f64       _quality   = 0.82;
    CFNumberRef     _max_n     = CFNumberCreate(NULL, kCFNumberSInt32Type, &_max);
    CFNumberRef     _quality_n = CFNumberCreate(NULL, kCFNumberFloat64Type, &_quality);
    const void*     _keys[]    = { kCGImageSourceCreateThumbnailFromImageAlways, kCGImageSourceCreateThumbnailWithTransform, kCGImageSourceThumbnailMaxPixelSize };
    const void*     _values[]  = { kCFBooleanTrue, kCFBooleanTrue, _max_n };
    CFDictionaryRef _read      = CFDictionaryCreate(NULL, _keys, _values, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const void*     _qkeys[]   = { kCGImageDestinationLossyCompressionQuality };
    const void*     _qvalues[] = { _quality_n };
    CFDictionaryRef _write     = CFDictionaryCreate(NULL, _qkeys, _qvalues, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    u32             _pages     = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        if(_images[_i] == NULL) {
            continue;
        }
        CFURLRef _from = fude_pdf_url(_images[_i]);
        CGImageSourceRef _source = _from != NULL ? CGImageSourceCreateWithURL(_from, NULL) : NULL;
        if(_from != NULL) {
            CFRelease(_from);
        }
        CGImageRef _image = _source != NULL ? CGImageSourceCreateThumbnailAtIndex(_source, 0, _read) : NULL;
        if(_source != NULL) {
            CFRelease(_source);
        }
        if(_image == NULL) {
            continue;
        }
        CGImageRef            _jpeg_image = NULL;
        CFMutableDataRef      _jpeg       = CFDataCreateMutable(NULL, 0);
        CGImageDestinationRef _dest       = CGImageDestinationCreateWithData(_jpeg, CFSTR("public.jpeg"), 1, NULL);
        if(_dest != NULL) {
            CGImageDestinationAddImage(_dest, _image, _write);
            if(CGImageDestinationFinalize(_dest)) {
                CGDataProviderRef _provider = CGDataProviderCreateWithCFData(_jpeg);
                _jpeg_image                 = CGImageCreateWithJPEGDataProvider(_provider, NULL, true, kCGRenderingIntentDefault);
                CGDataProviderRelease(_provider);
            }
            CFRelease(_dest);
        }
        const CGRect _box = CGRectMake(0.0, 0.0, (CGFloat)CGImageGetWidth(_image) * 72.0 / 150.0, (CGFloat)CGImageGetHeight(_image) * 72.0 / 150.0);
        CGContextBeginPage(_ctx, &_box);
        CGContextDrawImage(_ctx, _box, _jpeg_image != NULL ? _jpeg_image : _image);
        CGContextEndPage(_ctx);
        _pages++;
        if(_jpeg_image != NULL) {
            CGImageRelease(_jpeg_image);
        }
        CFRelease(_jpeg);
        CGImageRelease(_image);
    }
    CFRelease(_read);
    CFRelease(_write);
    CFRelease(_max_n);
    CFRelease(_quality_n);
    CGPDFContextClose(_ctx);
    CGContextRelease(_ctx);
    if(_pages == 0u) {
        remove(_out);
        return false;
    }
    return true;
}

#else   // not yet: Android (PdfRenderer), Windows

b8 fude_pdf_text_available(void) {
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

b8 fude_pdf_available(void) {
    return false;
}

fude_pdf* fude_pdf_open(const c8* _path) {
    RDE_UNUSED(_path);
    return NULL;
}

void fude_pdf_close(fude_pdf* _pdf) {
    RDE_UNUSED(_pdf);
}

u32 fude_pdf_page_count(const fude_pdf* _pdf) {
    RDE_UNUSED(_pdf);
    return 0u;
}

rde_vec_2F fude_pdf_page_size(const fude_pdf* _pdf, u32 _page) {
    RDE_UNUSED(_pdf);
    RDE_UNUSED(_page);
    return (rde_vec_2F){ 595.0f, 842.0f };
}

b8 fude_pdf_render(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, u32 _w, u32 _h, u8* _rgba) {
    RDE_UNUSED(_pdf); RDE_UNUSED(_page); RDE_UNUSED(_from); RDE_UNUSED(_size); RDE_UNUSED(_w); RDE_UNUSED(_h); RDE_UNUSED(_rgba);
    return false;
}

b8 fude_pdf_from_images(const c8* const* _images, u32 _count, const c8* _out) {
    RDE_UNUSED(_images); RDE_UNUSED(_count); RDE_UNUSED(_out);
    return false;
}

#endif
