// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// Apple's headers come before RDE's, because rde.h defines `any`, a word their availability pragmas use.
#import <Foundation/Foundation.h>
#import <PDFKit/PDFKit.h>

#include "drawing/doc/pdf.h"

// ===========================================================================
// See pdf.h. A PDF's own text, read by PDFKit on iOS and macOS: a PDFDocument of its own beside the Core Graphics
// document pdf.c draws with, opened the first time the text is asked for.
//
// A search goes on a few milliseconds at a time, each time its matches are asked for (once a frame), because PDFKit's
// own background search answers through a run loop that the app's frame does not turn. It goes a page at a time, each
// page's text searched from the last match on, and the time is looked at after every page and every match: PDFKit's
// findString goes on through the whole document until it finds one, so a word that is rare, or missing, in a long book
// held one frame for as long as that took.
//
// Positions: PDFKit's are in the page's own space (points from its crop box's bottom-left, not turned); the ones
// given to pdf.c are on the page as read (turned as the PDF says, from its top-left). pdf.c then applies the
// learner's turns. The builder compiles Objective-C with ARC.
// ===========================================================================

// A PDF's text and its search: what fude_pdf_kit_open returns, as an opaque pointer pdf.c keeps.
@interface FudePdfText : NSObject
@property(nonatomic, strong) PDFDocument*   document;
@property(nonatomic, strong) NSMutableData* matches;   // The matches, as fude_pdf_match one after another.
@property(nonatomic) BOOL                   done;
@property(nonatomic, copy)   NSString*      query;
@property(nonatomic) NSUInteger             onPage;    // The page the search is on...
@property(nonatomic) NSUInteger             at;        // ...where in its text it goes on from (after the last match)...
@property(nonatomic, copy)   NSString*      text;      // ...and that text (nil: not read yet).
@end

// How far the PDF says _page is turned to be read, in degrees clockwise: 0, 90, 180 or 270.
static NSInteger fude_pdf_kit_turn(PDFPage* _page) {
    NSInteger _turn = _page.rotation % 360;
    return _turn < 0 ? _turn + 360 : _turn;
}

// Takes a point on _page as read (points from its top-left) to the page's own space (from its crop box's
// bottom-left, not turned).
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

// Takes a point in _page's own space back to the page as read: the opposite of fude_pdf_kit_to_page.
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

// The rectangle with opposite corners _a and _b, in either order.
static CGRect fude_pdf_kit_rect(CGPoint _a, CGPoint _b) {
    return CGRectMake(rde_math_min_f64(_a.x, _b.x), rde_math_min_f64(_a.y, _b.y), rde_math_abs_f64(_a.x - _b.x), rde_math_abs_f64(_a.y - _b.y));
}

@implementation FudePdfText

// Keeps a match: for each page it is on, its rectangle on the page as read, up to FUDE_PDF_MATCHES in all.
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

// Takes the search a few milliseconds (6 ms) further through the document: through each page's text in turn, every
// match on it kept, the time looked at after each page and each match. It is done at the document's end, or once
// FUDE_PDF_MATCHES matches are kept. Case, accents and full- or half-width forms all match alike. A page's text and
// what it makes go with a pool of their own, so a long book's pages are not all held until the frame's end.
- (void)work {
    const CFTimeInterval         _until = CFAbsoluteTimeGetCurrent() + 0.006;
    const NSStringCompareOptions _how   = NSCaseInsensitiveSearch | NSDiacriticInsensitiveSearch | NSWidthInsensitiveSearch;
    while(!self.done && CFAbsoluteTimeGetCurrent() < _until) {
        @autoreleasepool {
            if(self.onPage >= (NSUInteger)self.document.pageCount) {
                self.done = YES;
                break;
            }
            PDFPage* _p = [self.document pageAtIndex:self.onPage];
            if(self.text == nil) {
                self.text = _p.string != nil ? _p.string : @"";   // (a scan's page has none)
            }
            const NSUInteger _len   = self.text.length;
            const NSRange    _found = self.at < _len ? [self.text rangeOfString:self.query options:_how range:NSMakeRange(self.at, _len - self.at)]
                                                     : NSMakeRange(NSNotFound, 0);
            if(_found.location == NSNotFound || _found.length == 0) {
                self.onPage += 1;   // (this page is through: the next, from its start)
                self.at    = 0;
                self.text  = nil;
                continue;
            }
            PDFSelection* _match = [_p selectionForRange:_found];
            if(_match != nil) {
                [self keep:_match];
            }
            self.at = NSMaxRange(_found);
            if(self.matches.length / sizeof(fude_pdf_match) >= FUDE_PDF_MATCHES) {
                self.done = YES;
            }
        }
    }
}

@end

// See pdf.h. A locked PDF is tried with an empty password. The search starts out done, with nothing to find.
any fude_pdf_kit_open(const c8* _path) {
    if(_path == NULL || _path[0] == 0) {
        return NULL;
    }
    NSString* _file = [NSString stringWithUTF8String:_path];
    if(_file == nil) {
        return NULL;   // A path that is not UTF-8: no NSString, and NSURL would throw on nil.
    }
    if(![_file isAbsolutePath]) {
        _file = [[[NSFileManager defaultManager] currentDirectoryPath] stringByAppendingPathComponent:_file];   // On a device: the resources.
    }
    PDFDocument* _document = [[PDFDocument alloc] initWithURL:[NSURL fileURLWithPath:_file]];
    if(_document == nil) {
        return NULL;
    }
    if(_document.isLocked) {
        [_document unlockWithPassword:@""];
    }
    FudePdfText* _text = [[FudePdfText alloc] init];
    _text.document     = _document;
    _text.matches      = [NSMutableData data];
    _text.done         = YES;
    return (__bridge_retained any)_text;
}

// Whether page _page has any text besides white space.
b8 fude_pdf_kit_page_has_text(any _kit, u32 _page) {
    FudePdfText* _text = (__bridge FudePdfText*)_kit;
    if(_kit == NULL || _page >= (u32)_text.document.pageCount) {
        return false;
    }
    NSString* _s = [_text.document pageAtIndex:_page].string;
    return [_s stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]].length > 0;
}

// Frees what fude_pdf_kit_open made: ARC releases it once the bridge hands it back.
void fude_pdf_kit_close(any _kit) {
    if(_kit == NULL) {
        return;
    }
    // A pool of its own, since a worker closing an orphaned job's PDF (pdf.h: Threads) has none.
    @autoreleasepool {
        FudePdfText* _text = (__bridge_transfer FudePdfText*)_kit;
        _text.text = nil;
    }
}

// See pdf.h (fude_pdf_text_in), but with the box on the page as the PDF reads it. Line ends become \n, and white
// space at either end is trimmed.
usize fude_pdf_kit_text_in(any _kit, u32 _page, rde_vec_2F _from, rde_vec_2F _size, c8* _out, usize _out_size) {
    if(_kit == NULL || _out == NULL || _out_size == 0) {
        return 0u;
    }
    _out[0] = 0;
    FudePdfText* _text = (__bridge FudePdfText*)_kit;
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
    // Its line ends as \n, without the white space around the text.
    _s = [[_s stringByReplacingOccurrencesOfString:@"\r\n" withString:@"\n"] stringByReplacingOccurrencesOfString:@"\r" withString:@"\n"];
    _s = [_s stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    const c8*   _utf8 = _s.UTF8String;
    const usize _len  = _utf8 != NULL ? rde_strlen(_utf8) : 0u;
    usize       _n    = _len < _out_size - 1u ? _len : _out_size - 1u;
    while(_n > 0 && _n < _len && ((u8)_utf8[_n] & 0xC0u) == 0x80u) {
        _n--;   // Do not cut in the middle of a character.
    }
    rde_memcpy(_out, _utf8, _n);
    _out[_n] = 0;
    return _n;
}

// Starts a search for _query, trimmed of spaces at either end, dropping the last one's matches. An empty query
// leaves the search done, with no matches.
void fude_pdf_kit_find_start(any _kit, const c8* _query) {
    if(_kit == NULL) {
        return;
    }
    FudePdfText* _text = (__bridge FudePdfText*)_kit;
    _text.matches.length = 0;
    _text.onPage         = 0;
    _text.at             = 0;
    _text.text           = nil;
    _text.done           = YES;
    NSString* _q = _query != NULL ? [[NSString stringWithUTF8String:_query] stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] : @"";
    if(_q.length == 0) {
        return;
    }
    _text.query = _q;
    _text.done  = NO;
}

// Stops the search; the matches found so far are kept until the next search starts.
void fude_pdf_kit_find_stop(any _kit) {
    if(_kit == NULL) {
        return;
    }
    FudePdfText* _text = (__bridge FudePdfText*)_kit;
    _text.done = YES;
    _text.text = nil;
}

// Takes the search a little further, then copies out the matches so far (see pdf.h, fude_pdf_find_matches). _kit
// must not be NULL.
u32 fude_pdf_kit_find_matches(any _kit, fude_pdf_match* _out, u32 _max, b8* _done) {
    FudePdfText* _text = (__bridge FudePdfText*)_kit;
    [_text work];   // A little further each time it is asked.
    const u32 _n = (u32)(_text.matches.length / sizeof(fude_pdf_match));
    *_done       = _text.done;
    if(_out == NULL) {
        return _n;
    }
    const u32 _k = _n < _max ? _n : _max;
    rde_memcpy(_out, _text.matches.bytes, sizeof(fude_pdf_match) * _k);
    return _k;
}
