// Apple's headers before RDE's: rde.h defines `any`, a word their availability
// pragmas use.
#import <Foundation/Foundation.h>
#import <PDFKit/PDFKit.h>

#include "drawing/doc/pdf.h"

#include <string.h>

// ===========================================================================
// See pdf.h. A PDF's own text, by PDFKit (iOS and macOS): a PDFDocument of its
// own beside the Core Graphics one pdf.c draws with, opened the first time the
// text is asked for. A search goes a few milliseconds at a time, each time its
// matches are asked for (once a frame): PDFKit's own background search answers
// through a run loop the app's frame does not turn. Positions: PDFKit's are the page's own space (points from
// its crop box's bottom-left, not turned); pdf.h's are the page as read (turned,
// from its top-left). The builder compiles Objective-C with ARC.
// ===========================================================================

@interface KanaPdfText : NSObject
@property(nonatomic, strong) PDFDocument*   document;
@property(nonatomic, strong) NSMutableData* matches;   // fude_pdf_match, one after another
@property(nonatomic) BOOL                   done;
@property(nonatomic, copy)   NSString*      query;
@property(nonatomic, strong) PDFSelection*  last;      // the match the search goes on from (nil: the start)
@end

// A page's turn (0, 90, 180, 270) and its crop box.
static NSInteger fude_pdf_kit_turn(PDFPage* _page) {
    NSInteger _turn = _page.rotation % 360;
    return _turn < 0 ? _turn + 360 : _turn;
}

// A point of the page as read (from its top-left) in the page's own space.
static CGPoint fude_pdf_kit_to_page(PDFPage* _page, CGFloat _x, CGFloat _y) {
    const CGRect  _c = [_page boundsForBox:kPDFDisplayBoxCropBox];
    const CGFloat _cx = _c.origin.x, _cy = _c.origin.y, _cw = _c.size.width, _ch = _c.size.height;
    switch(fude_pdf_kit_turn(_page)) {
        case 90:  return CGPointMake(_cx + _y, _cy + _x);
        case 180: return CGPointMake(_cx + _cw - _x, _cy + _y);
        case 270: return CGPointMake(_cx + _cw - _y, _cy + _ch - _x);
        default:  return CGPointMake(_cx + _x, _cy + _ch - _y);
    }
}

// ...and back.
static CGPoint fude_pdf_kit_from_page(PDFPage* _page, CGFloat _x, CGFloat _y) {
    const CGRect  _c = [_page boundsForBox:kPDFDisplayBoxCropBox];
    const CGFloat _cx = _c.origin.x, _cy = _c.origin.y, _cw = _c.size.width, _ch = _c.size.height;
    switch(fude_pdf_kit_turn(_page)) {
        case 90:  return CGPointMake(_y - _cy, _x - _cx);
        case 180: return CGPointMake(_cx + _cw - _x, _y - _cy);
        case 270: return CGPointMake(_cy + _ch - _y, _cx + _cw - _x);
        default:  return CGPointMake(_x - _cx, _cy + _ch - _y);
    }
}

static CGRect fude_pdf_kit_rect(CGPoint _a, CGPoint _b) {
    return CGRectMake(fmin(_a.x, _b.x), fmin(_a.y, _b.y), fabs(_a.x - _b.x), fabs(_a.y - _b.y));
}

@implementation KanaPdfText

// A match: each page it is on, as a rectangle of the page as read.
- (void)keep:(PDFSelection*)_found {
    for(PDFPage* _page in _found.pages) {
        const CGRect  _r = [_found boundsForPage:_page];
        const CGPoint _a = fude_pdf_kit_from_page(_page, CGRectGetMinX(_r), CGRectGetMinY(_r));
        const CGPoint _b = fude_pdf_kit_from_page(_page, CGRectGetMaxX(_r), CGRectGetMaxY(_r));
        const CGRect  _q = fude_pdf_kit_rect(_a, _b);
        fude_pdf_match _m = { (u32)[self.document indexForPage:_page], { (f32)_q.origin.x, (f32)_q.origin.y }, { (f32)_q.size.width, (f32)_q.size.height } };
        if(self.matches.length / sizeof(fude_pdf_match) < FUDE_PDF_MATCHES) {
            [self.matches appendBytes:&_m length:sizeof(_m)];
        }
    }
}

// On through the document, a few milliseconds' worth: done at its end, or when
// as many matches as are kept are in.
- (void)work {
    const CFTimeInterval _until = CFAbsoluteTimeGetCurrent() + 0.006;
    const NSStringCompareOptions _how = NSCaseInsensitiveSearch | NSDiacriticInsensitiveSearch | NSWidthInsensitiveSearch;
    while(!self.done && CFAbsoluteTimeGetCurrent() < _until) {
        PDFSelection* _found = [self.document findString:self.query fromSelection:self.last withOptions:_how];
        // None more — or back round to the start: through.
        PDFPage* _was = self.last.pages.firstObject;
        PDFPage* _now = _found.pages.firstObject;
        if(_found == nil || _now == nil || (_was != nil && [self.document indexForPage:_now] < [self.document indexForPage:_was])) {
            self.done = YES;
            break;
        }
        [self keep:_found];
        self.last = _found;
        if(self.matches.length / sizeof(fude_pdf_match) >= FUDE_PDF_MATCHES) {
            self.done = YES;
        }
    }
}

@end

void* fude_pdf_kit_open(const c8* _path) {
    if(_path == NULL || _path[0] == 0) {
        return NULL;
    }
    NSString* _file = [NSString stringWithUTF8String:_path];
    if(![_file isAbsolutePath]) {
        _file = [[[NSFileManager defaultManager] currentDirectoryPath] stringByAppendingPathComponent:_file];   // a device's: the app's resources
    }
    PDFDocument* _document = [[PDFDocument alloc] initWithURL:[NSURL fileURLWithPath:_file]];
    if(_document == nil) {
        return NULL;
    }
    if(_document.isLocked) {
        [_document unlockWithPassword:@""];
    }
    KanaPdfText* _text = [[KanaPdfText alloc] init];
    _text.document     = _document;
    _text.matches      = [NSMutableData data];
    _text.done         = YES;
    return (__bridge_retained void*)_text;
}

void fude_pdf_kit_close(void* _kit) {
    if(_kit == NULL) {
        return;
    }
    KanaPdfText* _text = (__bridge_transfer KanaPdfText*)_kit;
    _text.last = nil;
}

usize fude_pdf_kit_text_in(void* _kit, u32 _page, rde_vec_2F _from, rde_vec_2F _size, c8* _out, usize _out_size) {
    if(_kit == NULL || _out == NULL || _out_size == 0) {
        return 0u;
    }
    _out[0] = 0;
    KanaPdfText* _text = (__bridge KanaPdfText*)_kit;
    if(_page >= (u32)_text.document.pageCount) {
        return 0u;
    }
    PDFPage*      _p = [_text.document pageAtIndex:_page];
    const CGPoint _a = fude_pdf_kit_to_page(_p, _from.x, _from.y);
    const CGPoint _b = fude_pdf_kit_to_page(_p, _from.x + _size.x, _from.y + _size.y);
    NSString*     _s = [_p selectionForRect:fude_pdf_kit_rect(_a, _b)].string;
    if(_s.length == 0) {
        return 0u;
    }
    // Its lines as \n, without the spaces around it.
    _s = [[_s stringByReplacingOccurrencesOfString:@"\r\n" withString:@"\n"] stringByReplacingOccurrencesOfString:@"\r" withString:@"\n"];
    _s = [_s stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    const c8*   _utf8 = _s.UTF8String;
    const usize _len  = _utf8 != NULL ? strlen(_utf8) : 0u;
    usize       _n    = _len < _out_size - 1u ? _len : _out_size - 1u;
    while(_n > 0 && _n < _len && ((u8)_utf8[_n] & 0xC0u) == 0x80u) {
        _n--;   // not in the middle of a character
    }
    memcpy(_out, _utf8, _n);
    _out[_n] = 0;
    return _n;
}

void fude_pdf_kit_find_start(void* _kit, const c8* _query) {
    if(_kit == NULL) {
        return;
    }
    KanaPdfText* _text = (__bridge KanaPdfText*)_kit;
    _text.matches.length = 0;
    _text.last           = nil;
    _text.done           = YES;
    NSString* _q = _query != NULL ? [[NSString stringWithUTF8String:_query] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] : @"";
    if(_q.length == 0) {
        return;
    }
    _text.query = _q;
    _text.done  = NO;
}

void fude_pdf_kit_find_stop(void* _kit) {
    if(_kit == NULL) {
        return;
    }
    KanaPdfText* _text = (__bridge KanaPdfText*)_kit;
    _text.done = YES;
    _text.last = nil;
}

u32 fude_pdf_kit_find_matches(void* _kit, fude_pdf_match* _out, u32 _max, b8* _done) {
    KanaPdfText* _text = (__bridge KanaPdfText*)_kit;
    [_text work];   // a little further, each time asked
    const u32 _n = (u32)(_text.matches.length / sizeof(fude_pdf_match));
    *_done       = _text.done;
    if(_out == NULL) {
        return _n;
    }
    const u32 _k = _n < _max ? _n : _max;
    memcpy(_out, _text.matches.bytes, sizeof(fude_pdf_match) * _k);
    return _k;
}
