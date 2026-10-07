// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/doc/pdf.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// See pdf.h. Apple's: Core Graphics' CGPDFDocument (reading and drawing) and
// PDF context (writing), ImageIO for the images — all plain C. Android's:
// pdf_android.c. Elsewhere: not yet.
// ===========================================================================

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>
#include <unistd.h>

struct fude_pdf {
    CGPDFDocumentRef doc;
    u32              count;
    rde_arr TYPE(rde_vec_2F) sizes;   // each page's, turned as the PDF says, in points
    rde_arr TYPE(u8)         turns;   // ...and the learner's quarter turns over that
    c8               path[RDE_MAX_PATH];
    void*            kit;     // its text's document (pdf_kit.m), opened the first time it is asked
    b8               kit_tried;
};

// Typed views of its arrays.
RDE_INTERNAL rde_vec_2F* fude_pdf_sizes(const fude_pdf* _pdf) { return (rde_vec_2F*)_pdf->sizes.memory; }
RDE_INTERNAL u8*         fude_pdf_turns(const fude_pdf* _pdf) { return (u8*)_pdf->turns.memory; }

RDE_INTERNAL void fude_pdf_box(const fude_pdf* _pdf, u32 _page, b8 _to_pdf, rde_vec_2F* _from, rde_vec_2F* _size);

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

b8 fude_pdf_page_has_text(fude_pdf* _pdf, u32 _page) {
    void* _kit = fude_pdf_kit(_pdf);
    return _kit != NULL && fude_pdf_kit_page_has_text(_kit, _page);
}

usize fude_pdf_text_in(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, c8* _out, usize _out_size) {
    void* _kit = fude_pdf_kit(_pdf);
    if(_out_size > 0) {
        _out[0] = 0;
    }
    if(_kit == NULL || _page >= _pdf->count) {
        return 0u;
    }
    fude_pdf_box(_pdf, _page, true, &_from, &_size);   // the page as the PDF reads it (PDFKit's)
    return fude_pdf_kit_text_in(_kit, _page, _from, _size, _out, _out_size);
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
    if(_pdf == NULL || _pdf->kit == NULL) {
        return 0u;
    }
    const u32 _n = fude_pdf_kit_find_matches(_pdf->kit, _out, _max, _done);
    for(u32 _i = 0; _out != NULL && _i < _n; _i++) {
        if(_out[_i].page < _pdf->count) {
            fude_pdf_box(_pdf, _out[_i].page, false, &_out[_i].from, &_out[_i].size);   // as the learner turned it
        }
    }
    return _n;
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
    _pdf->sizes    = rde_arr_new(sizeof(rde_vec_2F), rde_memory_allocator_get_default_std());
    _pdf->turns    = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_pdf->sizes, _count);
    rde_arr_resize(&_pdf->turns, _count);
    for(u32 _i = 0; _i < _pdf->count; _i++) {
        CGPDFPageRef _page = CGPDFDocumentGetPage(_doc, (size_t)_i + 1u);
        if(_page == NULL) {
            fude_pdf_sizes(_pdf)[_i] = (rde_vec_2F){ 595.0f, 842.0f };   // A4: a page that cannot be read still has a place
            continue;
        }
        const CGRect _box  = CGPDFPageGetBoxRect(_page, kCGPDFCropBox);
        const i32    _turn = fude_pdf_turn(_page);
        const f32    _w    = (f32)fmax(1.0, _box.size.width);
        const f32    _h    = (f32)fmax(1.0, _box.size.height);
        fude_pdf_sizes(_pdf)[_i] = _turn == 90 || _turn == 270 ? (rde_vec_2F){ _h, _w } : (rde_vec_2F){ _w, _h };
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
    rde_arr_free(&_pdf->turns);
    rde_arr_free(&_pdf->sizes);
    free(_pdf);
}

u32 fude_pdf_page_count(const fude_pdf* _pdf) {
    return _pdf != NULL ? _pdf->count : 0u;
}

rde_vec_2F fude_pdf_page_size(const fude_pdf* _pdf, u32 _page) {
    if(_pdf == NULL || _page >= _pdf->count) {
        return (rde_vec_2F){ 595.0f, 842.0f };
    }
    const rde_vec_2F _s = fude_pdf_sizes(_pdf)[_page];
    return (fude_pdf_turns(_pdf)[_page] & 1u) != 0u ? (rde_vec_2F){ _s.y, _s.x } : _s;
}

void fude_pdf_set_turn(fude_pdf* _pdf, u32 _page, u8 _quarters) {
    if(_pdf != NULL && _page < _pdf->count) {
        fude_pdf_turns(_pdf)[_page] = (u8)(_quarters & 3u);
    }
}

// The learner's turn of page _page over the page as the PDF reads (Y up from
// its bottom-left, _s its size): to the page as turned.
RDE_INTERNAL CGAffineTransform fude_pdf_turned(const fude_pdf* _pdf, u32 _page) {
    const CGFloat _w = fude_pdf_sizes(_pdf)[_page].x, _h = fude_pdf_sizes(_pdf)[_page].y;
    switch(fude_pdf_turns(_pdf)[_page] & 3u) {
        case 1:  return CGAffineTransformMake(0.0, -1.0, 1.0, 0.0, 0.0, _w);
        case 2:  return CGAffineTransformMake(-1.0, 0.0, 0.0, -1.0, _w, _h);
        case 3:  return CGAffineTransformMake(0.0, 1.0, -1.0, 0.0, _h, 0.0);
        default: return CGAffineTransformIdentity;
    }
}

// A point of the page as turned (from its top-left) on the page as the PDF reads
// it, and back.
RDE_INTERNAL rde_vec_2F fude_pdf_unturn(const fude_pdf* _pdf, u32 _page, rde_vec_2F _p) {
    const f32 _w = fude_pdf_sizes(_pdf)[_page].x, _h = fude_pdf_sizes(_pdf)[_page].y;
    switch(fude_pdf_turns(_pdf)[_page] & 3u) {
        case 1:  return (rde_vec_2F){ _p.y, _h - _p.x };
        case 2:  return (rde_vec_2F){ _w - _p.x, _h - _p.y };
        case 3:  return (rde_vec_2F){ _w - _p.y, _p.x };
        default: return _p;
    }
}

RDE_INTERNAL rde_vec_2F fude_pdf_return(const fude_pdf* _pdf, u32 _page, rde_vec_2F _p) {
    const f32 _w = fude_pdf_sizes(_pdf)[_page].x, _h = fude_pdf_sizes(_pdf)[_page].y;
    switch(fude_pdf_turns(_pdf)[_page] & 3u) {
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
    const rde_vec_2F _page_size = fude_pdf_page_size(_pdf, _page);
    CGContextScaleCTM(_ctx, (CGFloat)_w / (CGFloat)_size.x, (CGFloat)_h / (CGFloat)_size.y);
    CGContextTranslateCTM(_ctx, -(CGFloat)_from.x, -(CGFloat)(_page_size.y - _from.y - _size.y));
    CGContextConcatCTM(_ctx, fude_pdf_turned(_pdf, _page));   // as the learner turned it
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

// --- writing --------------------------------------------------------------------------------

struct fude_pdf_writer {
    CGContextRef ctx;
    CGFloat      height;   // the page's, for its top-left points
    b8           open;     // a page begun
    b8           ok;
};

// The PDF's own space to the page as read, Y up from its bottom-left: page _p's
// crop box, turned (as fude_pdf_render draws it).
RDE_INTERNAL CGAffineTransform fude_pdf_reading(CGPDFPageRef _p) {
    const CGRect  _box = CGPDFPageGetBoxRect(_p, kCGPDFCropBox);
    const CGFloat _cx = _box.origin.x, _cy = _box.origin.y, _cw = _box.size.width, _ch = _box.size.height;
    switch(fude_pdf_turn(_p)) {
        case 90:  return CGAffineTransformMake(0.0, -1.0, 1.0, 0.0, -_cy, _cw + _cx);
        case 180: return CGAffineTransformMake(-1.0, 0.0, 0.0, -1.0, _cw + _cx, _ch + _cy);
        case 270: return CGAffineTransformMake(0.0, 1.0, -1.0, 0.0, _ch + _cy, -_cx);
        default:  return CGAffineTransformMake(1.0, 0.0, 0.0, 1.0, -_cx, -_cy);
    }
}

b8 fude_pdf_write_available(void) {
    return true;
}

fude_pdf_writer* fude_pdf_write_begin(const c8* _out) {
    CFURLRef _url = _out != NULL ? fude_pdf_url(_out) : NULL;
    if(_url == NULL) {
        return NULL;
    }
    CGContextRef _ctx = CGPDFContextCreateWithURL(_url, NULL, NULL);
    CFRelease(_url);
    if(_ctx == NULL) {
        return NULL;
    }
    fude_pdf_writer* _w = (fude_pdf_writer*)calloc(1, sizeof(fude_pdf_writer));
    _w->ctx = _ctx;
    _w->ok  = true;
    return _w;
}

void fude_pdf_write_page(fude_pdf_writer* _w, fude_pdf* _pdf, u32 _page) {
    if(_w == NULL || _pdf == NULL || _page >= _pdf->count) {
        return;
    }
    const rde_vec_2F _size = fude_pdf_page_size(_pdf, _page);
    const CGRect     _box  = CGRectMake(0.0, 0.0, (CGFloat)_size.x, (CGFloat)_size.y);
    CGContextBeginPage(_w->ctx, &_box);
    _w->open   = true;
    _w->height = (CGFloat)_size.y;
    CGPDFPageRef _p = CGPDFDocumentGetPage(_pdf->doc, (size_t)_page + 1u);
    if(_p != NULL) {
        CGContextSaveGState(_w->ctx);
        CGContextConcatCTM(_w->ctx, fude_pdf_turned(_pdf, _page));
        CGContextConcatCTM(_w->ctx, fude_pdf_reading(_p));
        CGContextClipToRect(_w->ctx, CGPDFPageGetBoxRect(_p, kCGPDFCropBox));
        CGContextDrawPDFPage(_w->ctx, _p);
        CGContextRestoreGState(_w->ctx);
    }
}

void fude_pdf_write_stroke(fude_pdf_writer* _w, const rde_vec_2F* _points, const f32* _radii, u32 _n, rde_color _color, b8 _even) {
    if(_w == NULL || !_w->open || _n == 0u) {
        return;
    }
    CGContextRef _c = _w->ctx;
    CGContextSaveGState(_c);
    CGContextSetRGBStrokeColor(_c, _color.r / 255.0, _color.g / 255.0, _color.b / 255.0, _color.a / 255.0);
    CGContextSetRGBFillColor(_c, _color.r / 255.0, _color.g / 255.0, _color.b / 255.0, _color.a / 255.0);
    CGContextSetLineCap(_c, kCGLineCapRound);
    CGContextSetLineJoin(_c, kCGLineJoinRound);
    if(_n == 1u) {
        const CGFloat _r = _radii[0];
        CGContextFillEllipseInRect(_c, CGRectMake(_points[0].x - _r, _w->height - _points[0].y - _r, 2.0 * _r, 2.0 * _r));
    } else if(_even) {
        // One shape: a see-through stroke painted once.
        CGContextBeginPath(_c);
        CGContextMoveToPoint(_c, _points[0].x, _w->height - _points[0].y);
        for(u32 _i = 1; _i < _n; _i++) {
            CGContextAddLineToPoint(_c, _points[_i].x, _w->height - _points[_i].y);
        }
        CGContextSetLineWidth(_c, 2.0 * _radii[0]);
        CGContextStrokePath(_c);
    } else {
        // Its width follows the pen: a piece at a time (the ink is solid: where they meet does not show).
        for(u32 _i = 0; _i + 1u < _n; _i++) {
            CGContextBeginPath(_c);
            CGContextMoveToPoint(_c, _points[_i].x, _w->height - _points[_i].y);
            CGContextAddLineToPoint(_c, _points[_i + 1u].x, _w->height - _points[_i + 1u].y);
            CGContextSetLineWidth(_c, _radii[_i] + _radii[_i + 1u]);
            CGContextStrokePath(_c);
        }
    }
    CGContextRestoreGState(_c);
}

void fude_pdf_write_fill(fude_pdf_writer* _w, const rde_vec_2F* _t, u32 _count, rde_color _color) {
    if(_w == NULL || !_w->open || _t == NULL || _count == 0u) {
        return;
    }
    CGContextRef _c = _w->ctx;
    CGContextSaveGState(_c);
    CGContextSetRGBFillColor(_c, _color.r / 255.0, _color.g / 255.0, _color.b / 255.0, _color.a / 255.0);
    CGContextBeginPath(_c);
    for(u32 _i = 0; _i < _count; _i++) {
        // Each the same way round (nonzero: one shape, painted once).
        rde_vec_2F _a = _t[_i * 3u], _b = _t[_i * 3u + 1u], _d = _t[_i * 3u + 2u];
        if((_b.x - _a.x) * (_d.y - _a.y) - (_d.x - _a.x) * (_b.y - _a.y) < 0.0f) {
            const rde_vec_2F _s = _b;
            _b = _d;
            _d = _s;
        }
        CGContextMoveToPoint(_c, _a.x, _w->height - _a.y);
        CGContextAddLineToPoint(_c, _b.x, _w->height - _b.y);
        CGContextAddLineToPoint(_c, _d.x, _w->height - _d.y);
        CGContextClosePath(_c);
    }
    CGContextFillPath(_c);
    CGContextRestoreGState(_c);
}

void fude_pdf_write_image(fude_pdf_writer* _w, const u8* _bytes, u32 _size, const rde_vec_2F _corners[3]) {
    if(_w == NULL || !_w->open || _bytes == NULL || _size == 0u) {
        return;
    }
    CFDataRef        _data = CFDataCreate(NULL, _bytes, (CFIndex)_size);
    CGImageSourceRef _src  = _data != NULL ? CGImageSourceCreateWithData(_data, NULL) : NULL;
    CGImageRef       _img  = _src != NULL ? CGImageSourceCreateImageAtIndex(_src, 0, NULL) : NULL;
    if(_img != NULL) {
        // The unit square onto its corners (the page's Y up here).
        const CGFloat _x0 = _corners[0].x, _y0 = _w->height - _corners[0].y;
        const CGAffineTransform _t = { _corners[1].x - _x0, (_w->height - _corners[1].y) - _y0,
                                       _corners[2].x - _x0, (_w->height - _corners[2].y) - _y0, _x0, _y0 };
        CGContextSaveGState(_w->ctx);
        CGContextConcatCTM(_w->ctx, _t);
        CGContextDrawImage(_w->ctx, CGRectMake(0.0, 0.0, 1.0, 1.0), _img);
        CGContextRestoreGState(_w->ctx);
        CGImageRelease(_img);
    }
    if(_src != NULL) {
        CFRelease(_src);
    }
    if(_data != NULL) {
        CFRelease(_data);
    }
}

void fude_pdf_write_text(fude_pdf_writer* _w, const c8* _text, rde_vec_2F _at, f32 _size, rde_color _color) {
    if(_w == NULL || !_w->open || _text == NULL || _text[0] == 0 || !(_size > 0.0f)) {
        return;
    }
    CFStringRef _s = CFStringCreateWithCString(NULL, _text, kCFStringEncodingUTF8);
    if(_s == NULL) {
        return;
    }
    // The system's font (Core Text finds another for what it has no letter for), in the context's colour.
    CTFontRef         _font  = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, (CGFloat)_size, NULL);
    if(_font == NULL) {
        CFRelease(_s);
        return;
    }
    const void*       _k[]   = { kCTFontAttributeName, kCTForegroundColorFromContextAttributeName };
    const void*       _v[]   = { _font, kCFBooleanTrue };
    CFDictionaryRef   _attrs = CFDictionaryCreate(NULL, _k, _v, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef _as = CFAttributedStringCreate(NULL, _s, _attrs);
    CTLineRef         _line  = CTLineCreateWithAttributedString(_as);
    CGContextRef      _c     = _w->ctx;
    CGContextSaveGState(_c);
    CGContextSetRGBFillColor(_c, _color.r / 255.0, _color.g / 255.0, _color.b / 255.0, _color.a / 255.0);
    CGContextSetTextDrawingMode(_c, kCGTextFill);
    CGContextSetTextMatrix(_c, CGAffineTransformIdentity);
    CGContextSetTextPosition(_c, _at.x, _w->height - _at.y);
    CTLineDraw(_line, _c);
    CGContextRestoreGState(_c);
    CFRelease(_line);
    CFRelease(_as);
    CFRelease(_attrs);
    CFRelease(_font);
    CFRelease(_s);
}

void fude_pdf_write_hidden_text(fude_pdf_writer* _w, const c8* _text, rde_vec_2F _from, rde_vec_2F _size) {
    if(_w == NULL || !_w->open || _text == NULL || _text[0] == 0 || _size.x <= 0.0f || _size.y <= 0.0f) {
        return;
    }
    CFStringRef _s = CFStringCreateWithCString(NULL, _text, kCFStringEncodingUTF8);
    if(_s == NULL) {
        return;
    }
    // A Japanese font as tall as the box, stretched across it: unseen, but where the picture shows it.
    CTFontRef         _font  = CTFontCreateWithName(CFSTR("HiraginoSans-W3"), (CGFloat)_size.y * 0.85, NULL);
    const void*       _k[]   = { kCTFontAttributeName };
    const void*       _v[]   = { _font };
    CFDictionaryRef   _attrs = CFDictionaryCreate(NULL, _k, _v, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef _as = CFAttributedStringCreate(NULL, _s, _attrs);
    CTLineRef         _line  = CTLineCreateWithAttributedString(_as);
    CGFloat           _ascent = 0.0, _descent = 0.0;
    const double      _width = CTLineGetTypographicBounds(_line, &_ascent, &_descent, NULL);
    CGContextRef      _c     = _w->ctx;
    CGContextSaveGState(_c);
    CGContextSetTextDrawingMode(_c, kCGTextInvisible);
    CGContextSetTextMatrix(_c, CGAffineTransformIdentity);
    CGContextTranslateCTM(_c, _from.x, _w->height - _from.y - _size.y + _descent);
    CGContextScaleCTM(_c, _width > 0.0 ? (CGFloat)_size.x / (CGFloat)_width : 1.0, 1.0);
    CGContextSetTextPosition(_c, 0.0, 0.0);
    CTLineDraw(_line, _c);
    CGContextRestoreGState(_c);
    CFRelease(_line);
    CFRelease(_as);
    CFRelease(_attrs);
    CFRelease(_font);
    CFRelease(_s);
}

void fude_pdf_write_page_end(fude_pdf_writer* _w) {
    if(_w != NULL && _w->open) {
        CGContextEndPage(_w->ctx);
        _w->open = false;
    }
}

b8 fude_pdf_write_end(fude_pdf_writer* _w) {
    if(_w == NULL) {
        return false;
    }
    fude_pdf_write_page_end(_w);
    CGPDFContextClose(_w->ctx);
    CGContextRelease(_w->ctx);
    const b8 _ok = _w->ok;
    free(_w);
    return _ok;
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

#elif !defined(RDE_PLATFORM_ANDROID)   // Android: pdf_android.c; not yet: Windows

b8 fude_pdf_text_available(void) {
    return false;
}

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

void fude_pdf_write_fill(fude_pdf_writer* _w, const rde_vec_2F* _triangles, u32 _count, rde_color _color) {
    RDE_UNUSED(_w); RDE_UNUSED(_triangles); RDE_UNUSED(_count); RDE_UNUSED(_color);
}

void fude_pdf_write_text(fude_pdf_writer* _w, const c8* _text, rde_vec_2F _at, f32 _size, rde_color _color) {
    RDE_UNUSED(_w); RDE_UNUSED(_text); RDE_UNUSED(_at); RDE_UNUSED(_size); RDE_UNUSED(_color);
}

void fude_pdf_write_image(fude_pdf_writer* _w, const u8* _bytes, u32 _size, const rde_vec_2F _corners[3]) {
    RDE_UNUSED(_w); RDE_UNUSED(_bytes); RDE_UNUSED(_size); RDE_UNUSED(_corners);
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

b8 fude_pdf_page_has_text(fude_pdf* _pdf, u32 _page) {
    RDE_UNUSED(_pdf); RDE_UNUSED(_page);
    return false;
}

void fude_pdf_set_turn(fude_pdf* _pdf, u32 _page, u8 _quarters) {
    RDE_UNUSED(_pdf); RDE_UNUSED(_page); RDE_UNUSED(_quarters);
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

// --- a picture as JPEG (or PNG) bytes ------------------------------------------------------

#if defined(__APPLE__)

b8 fude_picture_bytes(const c8* _path, u32 _max_px, rde_arr* _out) {
    rde_arr_clear(_out);
    CFURLRef _from = fude_pdf_url(_path);
    CGImageSourceRef _source = _from != NULL ? CGImageSourceCreateWithURL(_from, NULL) : NULL;
    if(_from != NULL) {
        CFRelease(_from);
    }
    if(_source == NULL) {
        return false;
    }
    // Read turned as taken, no larger than asked; written back as JPEG (PNG when
    // it has see-through parts: a JPEG would fill them in).
    const i32       _max     = (i32)_max_px;
    const f64       _quality = 0.86;
    CFNumberRef     _max_n   = CFNumberCreate(NULL, kCFNumberSInt32Type, &_max);
    CFNumberRef     _q_n     = CFNumberCreate(NULL, kCFNumberFloat64Type, &_quality);
    const void*     _keys[]  = { kCGImageSourceCreateThumbnailFromImageAlways, kCGImageSourceCreateThumbnailWithTransform, kCGImageSourceThumbnailMaxPixelSize };
    const void*     _vals[]  = { kCFBooleanTrue, kCFBooleanTrue, _max_n };
    CFDictionaryRef _read    = CFDictionaryCreate(NULL, _keys, _vals, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const void*     _qkeys[] = { kCGImageDestinationLossyCompressionQuality };
    const void*     _qvals[] = { _q_n };
    CFDictionaryRef _write   = CFDictionaryCreate(NULL, _qkeys, _qvals, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CGImageRef      _image   = CGImageSourceCreateThumbnailAtIndex(_source, 0, _read);
    b8              _ok      = false;
    if(_image != NULL) {
        const CGImageAlphaInfo _alpha = CGImageGetAlphaInfo(_image);
        const b8               _clear = _alpha != kCGImageAlphaNone && _alpha != kCGImageAlphaNoneSkipLast && _alpha != kCGImageAlphaNoneSkipFirst;
        CFMutableDataRef       _jpeg  = CFDataCreateMutable(NULL, 0);
        CGImageDestinationRef  _dest  = CGImageDestinationCreateWithData(_jpeg, _clear ? CFSTR("public.png") : CFSTR("public.jpeg"), 1, NULL);
        if(_dest != NULL) {
            CGImageDestinationAddImage(_dest, _image, _write);
            if(CGImageDestinationFinalize(_dest)) {
                const CFIndex _n = CFDataGetLength(_jpeg);
                if(_n > 0) {
                    memcpy(rde_arr_add_n(_out, (usize)_n), CFDataGetBytePtr(_jpeg), (size_t)_n);
                    _ok = true;
                }
            }
            CFRelease(_dest);
        }
        CFRelease(_jpeg);
        CGImageRelease(_image);
    }
    CFRelease(_read);
    CFRelease(_write);
    CFRelease(_max_n);
    CFRelease(_q_n);
    CFRelease(_source);
    return _ok;
}

#elif !defined(RDE_PLATFORM_ANDROID)   // Android: pdf_android.c

b8 fude_picture_bytes(const c8* _path, u32 _max_px, rde_arr* _out) {
    RDE_UNUSED(_max_px);
    rde_arr_clear(_out);
    FILE* _f = fopen(_path, "rb");
    if(_f == NULL) {
        return false;
    }
    fseek(_f, 0, SEEK_END);
    const long _n = ftell(_f);
    fseek(_f, 0, SEEK_SET);
    if(_n <= 0) {
        fclose(_f);
        return false;
    }
    const b8 _ok = fread(rde_arr_add_n(_out, (usize)_n), 1, (size_t)_n, _f) == (size_t)_n;
    fclose(_f);
    if(!_ok) {
        rde_arr_clear(_out);
    }
    return _ok;
}

#endif
