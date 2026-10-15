// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/doc/pdf.h"
#include "drawing/base/kfile.h"
#include "drawing/base/draw.h"

// ===========================================================================
// See pdf.h.
//
// Apple's: Core Graphics' CGPDFDocument reads and draws, its PDF context writes, Core Text sets the text written, and
// ImageIO reads and encodes pictures, all from plain C. The PDF's text comes from PDFKit through pdf_kit.m, which
// places it on the page as the PDF reads it; this file turns positions to and from the page as the learner turned it.
// Android's is pdf_android.c. Elsewhere there is nothing yet (Windows is still to choose): see the stubs further down.
//
// Turns: a page has two. The PDF's own (its rotation, 0, 90, 180 or 270 degrees) is taken into its size as it opens.
// The learner's quarter turns (fude_pdf_set_turn) are kept beside it and applied whenever it is drawn, written, read
// or searched.
// ===========================================================================

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>

// The font a page's read text is set in, unseen (fude_pdf_write_hidden_text): Hiragino Sans in its regular weight (W3),
// which every Apple device has. It holds Latin, kana and kanji, which is what the first picture reader (Japanese text
// recognition) reads; Core Text finds another font for any character it lacks. It is named here rather than taken
// from the system, so a page shared as PDF is set the same from every device and system version.
#define FUDE_PDF_HIDDEN_FONT CFSTR("HiraginoSans-W3")

// An open PDF on Apple's: Core Graphics' document, each page's size and turns, and PDFKit's document for its text.
struct fude_pdf {
    CGPDFDocumentRef doc;
    u32              count;
    rde_arr TYPE(rde_vec_2F) sizes;   // Each page's size in points, turned as the PDF says...
    rde_arr TYPE(u8)         turns;   // ...and the learner's quarter turns on top of that.
    c8               path[RDE_MAX_PATH];
    any              kit;     // PDFKit's document for its text (pdf_kit.m), opened the first time it is needed.
    b8               kit_tried;
};

// The page sizes and turns as typed arrays (an rde_arr keeps its memory untyped).
RDE_INTERNAL rde_vec_2F* fude_pdf_sizes(const fude_pdf* _pdf) { return (rde_vec_2F*)_pdf->sizes.memory; }
RDE_INTERNAL u8*         fude_pdf_turns(const fude_pdf* _pdf) { return (u8*)_pdf->turns.memory; }

// Defined further down, with the other turning helpers; the text functions just below use it.
RDE_INTERNAL void fude_pdf_box(const fude_pdf* _pdf, u32 _page, b8 _to_pdf, rde_vec_2F* _from, rde_vec_2F* _size);

// PDFKit's document for the PDF's text, opened the first time it is asked for (NULL when PDFKit cannot open it). It is
// tried once only, so a PDF that PDFKit refuses is not opened again each frame.
RDE_INTERNAL any fude_pdf_kit(fude_pdf* _pdf) {
    if(_pdf != NULL && !_pdf->kit_tried) {
        _pdf->kit_tried = true;
        _pdf->kit       = fude_pdf_kit_open(_pdf->path);
    }
    return _pdf != NULL ? _pdf->kit : NULL;
}

// Apple's always has PDFKit.
b8 fude_pdf_text_available(void) {
    return true;
}

// See the header: PDFKit's document opens the first time the text is asked for, at once.
b8 fude_pdf_text_prepare(fude_pdf* _pdf) {
    RDE_UNUSED(_pdf);
    return true;
}

// See the header. False when PDFKit cannot open the PDF.
b8 fude_pdf_page_has_text(fude_pdf* _pdf, u32 _page) {
    any _kit = fude_pdf_kit(_pdf);
    return _kit != NULL && fude_pdf_kit_page_has_text(_kit, _page);
}

// See the header. The box is turned back to the page as the PDF reads it, which is what PDFKit takes.
usize fude_pdf_text_in(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, c8* _out, usize _out_size) {
    any _kit = fude_pdf_kit(_pdf);
    if(_out_size > 0) {
        _out[0] = 0;
    }
    if(_kit == NULL || _page >= _pdf->count) {
        return 0u;
    }
    fude_pdf_box(_pdf, _page, true, &_from, &_size);   // To the page as the PDF reads it, as PDFKit wants.
    return fude_pdf_kit_text_in(_kit, _page, _from, _size, _out, _out_size);
}

// See the header. PDFKit searches; nothing happens when it cannot open the PDF.
void fude_pdf_find_start(fude_pdf* _pdf, const c8* _query) {
    any _kit = fude_pdf_kit(_pdf);
    if(_kit != NULL) {
        fude_pdf_kit_find_start(_kit, _query);
    }
}

// Stops PDFKit's search, when PDFKit was ever opened.
void fude_pdf_find_stop(fude_pdf* _pdf) {
    if(_pdf != NULL && _pdf->kit != NULL) {
        fude_pdf_kit_find_stop(_pdf->kit);
    }
}

// See the header. PDFKit places its matches on the page as the PDF reads it, so each is turned to the page as the
// learner turned it.
u32 fude_pdf_find_matches(fude_pdf* _pdf, fude_pdf_match* _out, u32 _max, b8* _done) {
    *_done = true;
    if(_pdf == NULL || _pdf->kit == NULL) {
        return 0u;
    }
    const u32 _n = fude_pdf_kit_find_matches(_pdf->kit, _out, _max, _done);
    for(u32 _i = 0; _out != NULL && _i < _n; _i++) {
        if(_out[_i].page < _pdf->count) {
            fude_pdf_box(_pdf, _out[_i].page, false, &_out[_i].from, &_out[_i].size);   // As the learner turned it.
        }
    }
    return _n;
}

// _path as a file URL, which the caller releases. A relative path is taken from the working folder, which on a device
// is the app's resources, where its assets are.
RDE_INTERNAL CFURLRef fude_pdf_url(const c8* _path) {
    c8 _full[RDE_MAX_PATH];
    if(_path[0] != '/') {
        c8 _cwd[RDE_MAX_PATH];
        if(rde_file_get_working_dir(_cwd, sizeof(_cwd))) {
            rde_snprintf(_full, sizeof(_full), "%s%s", _cwd, _path);   // The working folder ends in its separator.
            _path = _full;
        }
    }
    return CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*)_path, (CFIndex)rde_strlen(_path), false);
}

// How far the PDF says page _page must be turned to be read, in degrees clockwise: 0, 90, 180 or 270.
RDE_INTERNAL i32 fude_pdf_turn(CGPDFPageRef _page) {
    i32 _turn = (i32)CGPDFPageGetRotationAngle(_page) % 360;
    return _turn < 0 ? _turn + 360 : _turn;
}

// Apple's can always show PDFs.
b8 fude_pdf_available(void) {
    return true;
}

// See the header. Every page's size is read here, on the main thread, so the worker that draws pages only ever draws
// (pdf.h: Threads).
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
    // A locked PDF: many open with an empty password, since they only limit printing and copying.
    if(!CGPDFDocumentIsUnlocked(_doc) && !CGPDFDocumentUnlockWithPassword(_doc, "")) {
        CGPDFDocumentRelease(_doc);
        return NULL;
    }
    const size_t _count = CGPDFDocumentGetNumberOfPages(_doc);
    if(_count == 0) {
        CGPDFDocumentRelease(_doc);
        return NULL;
    }
    fude_pdf* _pdf = (fude_pdf*)rde_calloc(1, sizeof(fude_pdf));
    rde_snprintf(_pdf->path, sizeof(_pdf->path), "%s", _path);
    _pdf->doc      = _doc;
    _pdf->count    = (u32)_count;
    _pdf->sizes    = rde_arr_new(sizeof(rde_vec_2F), rde_memory_allocator_get_default_std());
    _pdf->turns    = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_pdf->sizes, _count);
    rde_arr_resize(&_pdf->turns, _count);
    for(u32 _i = 0; _i < _pdf->count; _i++) {
        CGPDFPageRef _page = CGPDFDocumentGetPage(_doc, (size_t)_i + 1u);
        if(_page == NULL) {
            fude_pdf_sizes(_pdf)[_i] = (rde_vec_2F){ 595.0f, 842.0f };   // A4, so an unreadable page has a place.
            continue;
        }
        const CGRect _box  = CGPDFPageGetBoxRect(_page, kCGPDFCropBox);
        const i32    _turn = fude_pdf_turn(_page);
        const f32    _w    = (f32)rde_math_max_f64(1.0, _box.size.width);
        const f32    _h    = (f32)rde_math_max_f64(1.0, _box.size.height);
        fude_pdf_sizes(_pdf)[_i] = _turn == 90 || _turn == 270 ? (rde_vec_2F){ _h, _w } : (rde_vec_2F){ _w, _h };
    }
    return _pdf;
}

// See the header. Closes PDFKit's document too.
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

// The transform from page _page as the PDF reads it to the page as the learner turned it, both in points with Y up
// from the bottom-left corner. The page's size before the learner's turn comes from sizes.
RDE_INTERNAL CGAffineTransform fude_pdf_turned(const fude_pdf* _pdf, u32 _page) {
    const CGFloat _w = fude_pdf_sizes(_pdf)[_page].x, _h = fude_pdf_sizes(_pdf)[_page].y;
    switch(fude_pdf_turns(_pdf)[_page] & 3u) {
        case 1:  return CGAffineTransformMake(0.0, -1.0, 1.0, 0.0, 0.0, _w);
        case 2:  return CGAffineTransformMake(-1.0, 0.0, 0.0, -1.0, _w, _h);
        case 3:  return CGAffineTransformMake(0.0, 1.0, -1.0, 0.0, _h, 0.0);
        default: return CGAffineTransformIdentity;
    }
}

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

// The transform from the PDF's own space to page _p as read, in points with Y up from its bottom-left: its crop box
// moved to 0 and turned as the PDF says. Drawing a page (fude_pdf_render) and writing one (fude_pdf_write_page) both
// go through it, so the two always agree.
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

// See the header. Core Graphics draws straight into _rgba, on white.
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

    // From the page as read (points, Y up, its bottom-left corner at 0) to the pixels, so the rectangle asked for
    // fills them. The bitmap's first row is its top.
    const rde_vec_2F _page_size = fude_pdf_page_size(_pdf, _page);
    CGContextScaleCTM(_ctx, (CGFloat)_w / (CGFloat)_size.x, (CGFloat)_h / (CGFloat)_size.y);
    CGContextTranslateCTM(_ctx, -(CGFloat)_from.x, -(CGFloat)(_page_size.y - _from.y - _size.y));
    CGContextConcatCTM(_ctx, fude_pdf_turned(_pdf, _page));   // Then as the learner turned it.
    CGContextConcatCTM(_ctx, fude_pdf_reading(_p));            // Then from the PDF's own space to the page as read.
    CGContextClipToRect(_ctx, CGPDFPageGetBoxRect(_p, kCGPDFCropBox));
    CGContextDrawPDFPage(_ctx, _p);
    CGContextRelease(_ctx);
    return true;
}

// --- writing --------------------------------------------------------------------------------

// A PDF being written, through Core Graphics' PDF context.
struct fude_pdf_writer {
    CGContextRef ctx;
    CGFloat      height;   // The page's height, to turn points from the top-left into Core Graphics' Y up.
    b8           open;     // A page is begun and not ended yet.
    c8           path[RDE_MAX_PATH];   // Where it writes: checked at the end, since Core Graphics reports no error.
    // The visible text's face (FUDE_PDF_FACE_), and each face's Roboto, read from the assets the first time it is used
    // (NULL until then; a face whose file cannot be read stays NULL and its text is set in the system's font).
    u8           face;
    CGFontRef    fonts[FUDE_PDF_FACES];
    b8           fonts_tried[FUDE_PDF_FACES];
};

// Apple's can always write PDFs.
b8 fude_pdf_write_available(void) {
    return true;
}

// See the header.
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
    fude_pdf_writer* _w = (fude_pdf_writer*)rde_calloc(1, sizeof(fude_pdf_writer));
    _w->ctx = _ctx;
    rde_snprintf(_w->path, sizeof(_w->path), "%s", _out);
    return _w;
}

// See the header. The new page is the page as the learner turned it, and the PDF's page is drawn into it as it is,
// so its text stays text.
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

// See the header. Points are from the page's top-left, so each y is flipped for Core Graphics. A single point is a
// dot.
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
        // One path, painted once, so a see-through stroke is not darker where it crosses itself. It is as wide as its
        // first radius says.
        CGContextBeginPath(_c);
        CGContextMoveToPoint(_c, _points[0].x, _w->height - _points[0].y);
        for(u32 _i = 1; _i < _n; _i++) {
            CGContextAddLineToPoint(_c, _points[_i].x, _w->height - _points[_i].y);
        }
        CGContextSetLineWidth(_c, 2.0 * _radii[0]);
        CGContextStrokePath(_c);
    } else {
        // Its width follows the pen, so it is drawn a segment at a time, each as wide as the radii at its two ends
        // together. The ink is solid, so where the segments overlap does not show.
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

// See the header. _t holds the triangles' points, three to a triangle.
void fude_pdf_write_fill(fude_pdf_writer* _w, const rde_vec_2F* _t, u32 _count, rde_color _color) {
    if(_w == NULL || !_w->open || _t == NULL || _count == 0u) {
        return;
    }
    CGContextRef _c = _w->ctx;
    CGContextSaveGState(_c);
    CGContextSetRGBFillColor(_c, _color.r / 255.0, _color.g / 255.0, _color.b / 255.0, _color.a / 255.0);
    CGContextBeginPath(_c);
    for(u32 _i = 0; _i < _count; _i++) {
        // Each triangle is turned the same way round, so under the nonzero rule they fill as one shape, painted once.
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

// See the header. ImageIO reads the bytes, whether JPEG or PNG.
void fude_pdf_write_image(fude_pdf_writer* _w, const u8* _bytes, u32 _size, const rde_vec_2F _corners[3]) {
    if(_w == NULL || !_w->open || _bytes == NULL || _size == 0u) {
        return;
    }
    CFDataRef        _data = CFDataCreate(NULL, _bytes, (CFIndex)_size);
    CGImageSourceRef _src  = _data != NULL ? CGImageSourceCreateWithData(_data, NULL) : NULL;
    CGImageRef       _img  = _src != NULL ? CGImageSourceCreateImageAtIndex(_src, 0, NULL) : NULL;
    if(_img != NULL) {
        // Maps the unit square onto the corners. Core Graphics' Y is up, so each y is flipped.
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

// See the header. A face is kept for the writer's life and released by fude_pdf_write_end.
void fude_pdf_write_face(fude_pdf_writer* _w, u8 _face) {
    if(_w != NULL) {
        _w->face = _face < FUDE_PDF_FACES ? _face : FUDE_PDF_FACE_REGULAR;
    }
}

// Returns the writer's face's Roboto as a Core Graphics font, read from the app's assets the first time; NULL when its
// file cannot be read or is not a font (the caller then falls back on the system's font).
RDE_INTERNAL CGFontRef fude_pdf_write_font(fude_pdf_writer* _w) {
    static const c8* const _files[FUDE_PDF_FACES] = { FUDE_DRAW_FONT_REGULAR, FUDE_DRAW_FONT_BOLD, FUDE_DRAW_FONT_ITALIC, FUDE_DRAW_FONT_BOLD_ITALIC };
    const u8 _f = _w->face;
    if(!_w->fonts_tried[_f]) {
        _w->fonts_tried[_f] = true;
        rde_arr TYPE(u8) _bytes = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
        if(fude_file_read_into(_files[_f], &_bytes) && rde_arr_length(&_bytes) > 0u) {
            CFDataRef         _data     = CFDataCreate(NULL, _bytes.memory, (CFIndex)rde_arr_length(&_bytes));
            CGDataProviderRef _provider = _data != NULL ? CGDataProviderCreateWithCFData(_data) : NULL;
            _w->fonts[_f]               = _provider != NULL ? CGFontCreateWithDataProvider(_provider) : NULL;
            if(_provider != NULL) {
                CGDataProviderRelease(_provider);
            }
            if(_data != NULL) {
                CFRelease(_data);
            }
        }
        rde_arr_free(&_bytes);
        if(_w->fonts[_f] == NULL) {
            rde_log_level(RDE_LOG_LEVEL_WARNING, "pdf: %s could not be read; the text is set in the system's font", _files[_f]);
        }
    }
    return _w->fonts[_f];
}

// See the header.
void fude_pdf_write_text(fude_pdf_writer* _w, const c8* _text, rde_vec_2F _at, f32 _size, rde_color _color) {
    if(_w == NULL || !_w->open || _text == NULL || _text[0] == 0 || !(_size > 0.0f)) {
        return;
    }
    CFStringRef _s = CFStringCreateWithCString(NULL, _text, kCFStringEncodingUTF8);
    if(_s == NULL) {
        return;
    }
    // Roboto in the writer's face, as the screen draws it, else the system's font; in the context's fill colour. Core
    // Text finds another font for any character it lacks (its fallback, as on screen the app's script font is).
    CGFontRef         _roboto = fude_pdf_write_font(_w);
    CTFontRef         _font   = _roboto != NULL ? CTFontCreateWithGraphicsFont(_roboto, (CGFloat)_size, NULL, NULL)
                                                : CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, (CGFloat)_size, NULL);
    if(_font == NULL) {
        CFRelease(_s);
        return;
    }
    const any         _k[]   = { kCTFontAttributeName, kCTForegroundColorFromContextAttributeName };
    const any         _v[]   = { _font, kCFBooleanTrue };
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

// See the header.
void fude_pdf_write_hidden_text(fude_pdf_writer* _w, const c8* _text, rde_vec_2F _from, rde_vec_2F _size) {
    if(_w == NULL || !_w->open || _text == NULL || _text[0] == 0 || _size.x <= 0.0f || _size.y <= 0.0f) {
        return;
    }
    CFStringRef _s = CFStringCreateWithCString(NULL, _text, kCFStringEncodingUTF8);
    if(_s == NULL) {
        return;
    }
    // The text is set in FUDE_PDF_HIDDEN_FONT at 85% of the box's height, stretched across the box's width and drawn
    // invisibly, so a PDF reader finds it where the picture shows it.
    CTFontRef         _font  = CTFontCreateWithName(FUDE_PDF_HIDDEN_FONT, (CGFloat)_size.y * 0.85, NULL);
    const any         _k[]   = { kCTFontAttributeName };
    const any         _v[]   = { _font };
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

// See the header. Nothing happens when no page is begun.
void fude_pdf_write_page_end(fude_pdf_writer* _w) {
    if(_w != NULL && _w->open) {
        CGContextEndPage(_w->ctx);
        _w->open = false;
    }
}

// See the header. Core Graphics reports no error when it closes the file (a full disk, a folder that went away), so
// what it wrote is checked instead: a file that begins as a PDF does.
b8 fude_pdf_write_end(fude_pdf_writer* _w) {
    if(_w == NULL) {
        return false;
    }
    fude_pdf_write_page_end(_w);
    CGPDFContextClose(_w->ctx);
    CGContextRelease(_w->ctx);
    for(u32 _f = 0; _f < FUDE_PDF_FACES; _f++) {
        if(_w->fonts[_f] != NULL) {
            CGFontRelease(_w->fonts[_f]);
        }
    }
    c8 _head[8];
    const b8 _ok = fude_file_read_text(_w->path, _head, sizeof(_head)) >= 5u && rde_strncmp(_head, "%PDF-", 5) == 0;
    if(!_ok) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "pdf: %s was not written whole", _w->path);
    }
    rde_free(_w);
    return _ok;
}

// See the header.
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
    // Each picture is read turned as it was taken and no larger than FUDE_PDF_IMAGE_PX, then encoded as JPEG (quality
    // 0.82), so the PDF keeps it as a JPEG rather than as a far larger lossless copy.
    const i32       _max       = (i32)FUDE_PDF_IMAGE_PX;
    const f64       _quality   = 0.82;
    CFNumberRef     _max_n     = CFNumberCreate(NULL, kCFNumberSInt32Type, &_max);
    CFNumberRef     _quality_n = CFNumberCreate(NULL, kCFNumberFloat64Type, &_quality);
    const any       _keys[]    = { kCGImageSourceCreateThumbnailFromImageAlways, kCGImageSourceCreateThumbnailWithTransform, kCGImageSourceThumbnailMaxPixelSize };
    const any       _values[]  = { kCFBooleanTrue, kCFBooleanTrue, _max_n };
    CFDictionaryRef _read      = CFDictionaryCreate(NULL, _keys, _values, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const any       _qkeys[]   = { kCGImageDestinationLossyCompressionQuality };
    const any       _qvalues[] = { _quality_n };
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
        // The page is the picture's size at 150 dots an inch (72 points an inch). When the JPEG could not be made, the
        // picture is drawn as it was read.
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
        rde_file_delete(_out);
        return false;
    }
    return true;
}

#elif !defined(RDE_PLATFORM_ANDROID)   // Android's is pdf_android.c; elsewhere (Windows) there is nothing yet.

// The stubs for a platform with no PDFs yet: nothing opens, nothing is written, and every answer is no.

// No PDF text on this platform.
b8 fude_pdf_text_available(void) {
    return false;
}

// Nothing to ready: there is no text here to wait for.
b8 fude_pdf_text_prepare(fude_pdf* _pdf) {
    RDE_UNUSED(_pdf);
    return true;
}

// No PDF writing on this platform.
b8 fude_pdf_write_available(void) {
    return false;
}

// Nothing can be written here: always NULL.
fude_pdf_writer* fude_pdf_write_begin(const c8* _out) {
    RDE_UNUSED(_out);
    return NULL;
}

// The calls that write into a writer do nothing, since there is never one here.
void fude_pdf_write_page(fude_pdf_writer* _w, fude_pdf* _pdf, u32 _page) {
    RDE_UNUSED(_w); RDE_UNUSED(_pdf); RDE_UNUSED(_page);
}

// Nothing to write into.
void fude_pdf_write_stroke(fude_pdf_writer* _w, const rde_vec_2F* _points, const f32* _radii, u32 _n, rde_color _color, b8 _even) {
    RDE_UNUSED(_w); RDE_UNUSED(_points); RDE_UNUSED(_radii); RDE_UNUSED(_n); RDE_UNUSED(_color); RDE_UNUSED(_even);
}

// Nothing to write into.
void fude_pdf_write_fill(fude_pdf_writer* _w, const rde_vec_2F* _triangles, u32 _count, rde_color _color) {
    RDE_UNUSED(_w); RDE_UNUSED(_triangles); RDE_UNUSED(_count); RDE_UNUSED(_color);
}

// Nothing to write into.
void fude_pdf_write_text(fude_pdf_writer* _w, const c8* _text, rde_vec_2F _at, f32 _size, rde_color _color) {
    RDE_UNUSED(_w); RDE_UNUSED(_text); RDE_UNUSED(_at); RDE_UNUSED(_size); RDE_UNUSED(_color);
}

// Nothing to write into.
void fude_pdf_write_face(fude_pdf_writer* _w, u8 _face) {
    RDE_UNUSED(_w); RDE_UNUSED(_face);
}

// Nothing to write into.
void fude_pdf_write_image(fude_pdf_writer* _w, const u8* _bytes, u32 _size, const rde_vec_2F _corners[3]) {
    RDE_UNUSED(_w); RDE_UNUSED(_bytes); RDE_UNUSED(_size); RDE_UNUSED(_corners);
}

// Nothing to write into.
void fude_pdf_write_hidden_text(fude_pdf_writer* _w, const c8* _text, rde_vec_2F _from, rde_vec_2F _size) {
    RDE_UNUSED(_w); RDE_UNUSED(_text); RDE_UNUSED(_from); RDE_UNUSED(_size);
}

// Nothing to write into.
void fude_pdf_write_page_end(fude_pdf_writer* _w) {
    RDE_UNUSED(_w);
}

// Nothing was written: always false.
b8 fude_pdf_write_end(fude_pdf_writer* _w) {
    RDE_UNUSED(_w);
    return false;
}

// No text here.
b8 fude_pdf_page_has_text(fude_pdf* _pdf, u32 _page) {
    RDE_UNUSED(_pdf); RDE_UNUSED(_page);
    return false;
}

// Nothing is ever open to turn.
void fude_pdf_set_turn(fude_pdf* _pdf, u32 _page, u8 _quarters) {
    RDE_UNUSED(_pdf); RDE_UNUSED(_page); RDE_UNUSED(_quarters);
}

// No text here: _out is left empty.
usize fude_pdf_text_in(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, c8* _out, usize _out_size) {
    RDE_UNUSED(_pdf); RDE_UNUSED(_page); RDE_UNUSED(_from); RDE_UNUSED(_size);
    if(_out_size > 0) {
        _out[0] = 0;
    }
    return 0u;
}

// No search here.
void fude_pdf_find_start(fude_pdf* _pdf, const c8* _query) {
    RDE_UNUSED(_pdf); RDE_UNUSED(_query);
}

// No search here.
void fude_pdf_find_stop(fude_pdf* _pdf) {
    RDE_UNUSED(_pdf);
}

// No matches, and the search counts as done.
u32 fude_pdf_find_matches(fude_pdf* _pdf, fude_pdf_match* _out, u32 _max, b8* _done) {
    RDE_UNUSED(_pdf); RDE_UNUSED(_out); RDE_UNUSED(_max);
    *_done = true;
    return 0u;
}

// This platform shows no PDFs.
b8 fude_pdf_available(void) {
    return false;
}

// Nothing opens here: always NULL.
fude_pdf* fude_pdf_open(const c8* _path) {
    RDE_UNUSED(_path);
    return NULL;
}

// Nothing was opened, so there is nothing to free.
void fude_pdf_close(fude_pdf* _pdf) {
    RDE_UNUSED(_pdf);
}

// No pages.
u32 fude_pdf_page_count(const fude_pdf* _pdf) {
    RDE_UNUSED(_pdf);
    return 0u;
}

// A4, as for a page out of range.
rde_vec_2F fude_pdf_page_size(const fude_pdf* _pdf, u32 _page) {
    RDE_UNUSED(_pdf);
    RDE_UNUSED(_page);
    return (rde_vec_2F){ 595.0f, 842.0f };
}

// Nothing can be drawn here: always false.
b8 fude_pdf_render(fude_pdf* _pdf, u32 _page, rde_vec_2F _from, rde_vec_2F _size, u32 _w, u32 _h, u8* _rgba) {
    RDE_UNUSED(_pdf); RDE_UNUSED(_page); RDE_UNUSED(_from); RDE_UNUSED(_size); RDE_UNUSED(_w); RDE_UNUSED(_h); RDE_UNUSED(_rgba);
    return false;
}

// No PDF can be written here: always false.
b8 fude_pdf_from_images(const c8* const* _images, u32 _count, const c8* _out) {
    RDE_UNUSED(_images); RDE_UNUSED(_count); RDE_UNUSED(_out);
    return false;
}

#endif

// --- a picture as JPEG (or PNG) bytes ------------------------------------------------------

#if defined(__APPLE__)

// See the header. ImageIO does the reading and the encoding.
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
    // Read turned as it was taken and no larger than asked, then written back as JPEG at quality 0.86, or as PNG when
    // it has see-through parts, which a JPEG would fill in. (_jpeg below holds the PNG's bytes in that case.)
    const i32       _max     = (i32)_max_px;
    const f64       _quality = 0.86;
    CFNumberRef     _max_n   = CFNumberCreate(NULL, kCFNumberSInt32Type, &_max);
    CFNumberRef     _q_n     = CFNumberCreate(NULL, kCFNumberFloat64Type, &_quality);
    const any       _keys[]  = { kCGImageSourceCreateThumbnailFromImageAlways, kCGImageSourceCreateThumbnailWithTransform, kCGImageSourceThumbnailMaxPixelSize };
    const any       _vals[]  = { kCFBooleanTrue, kCFBooleanTrue, _max_n };
    CFDictionaryRef _read    = CFDictionaryCreate(NULL, _keys, _vals, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const any       _qkeys[] = { kCGImageDestinationLossyCompressionQuality };
    const any       _qvals[] = { _q_n };
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
                    rde_memcpy(rde_arr_add_n(_out, (usize)_n), CFDataGetBytePtr(_jpeg), (size_t)_n);
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

#elif !defined(RDE_PLATFORM_ANDROID)   // Android's is pdf_android.c.

// Elsewhere the picture is handed on as the file's own bytes, at any size: _max_px is not used.
b8 fude_picture_bytes(const c8* _path, u32 _max_px, rde_arr* _out) {
    RDE_UNUSED(_max_px);
    const b8 _ok = fude_file_read_into(_path, _out) && rde_arr_length(_out) > 0u;
    if(!_ok) {
        rde_arr_clear(_out);
    }
    return _ok;
}

#endif
