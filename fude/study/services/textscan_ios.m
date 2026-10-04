// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// Apple's headers before RDE's: rde.h's `any` macro would otherwise reach the
// availability attributes in Photos' headers (apply_to = any(...)).
#import <TargetConditionals.h>
#if TARGET_OS_IOS && !TARGET_OS_SIMULATOR
#import <UIKit/UIKit.h>
#import <PhotosUI/PhotosUI.h>
#import <QuartzCore/QuartzCore.h>
#import <MLKitVision/MLKitVision.h>
#import <MLKitTextRecognitionCommon/MLKitTextRecognitionCommon.h>
#endif

#include "study/services/textscan.h"
#include "study/services/mlkit.h"
#include "lang/lang.h"

// ===========================================================================
// See textscan.h. The iOS side: the system's photo library picker
// (PHPickerViewController — it needs no permission: only the photo picked
// reaches the app), and ML Kit Text Recognition's Japanese model, for photos
// and for camera frames (RDE's camera hands Kana the pixels). The builder
// compiles Objective-C with ARC: the statics below keep what is kept.
//
// Whatever is read is redrawn upright first, one point a pixel (a photo at most
// FUDE_TEXTSCAN_SIDE pixels on its long side): what ML Kit measures and what
// the screen shows are then the same pixels, the same way up.
// ===========================================================================

#if defined(RDE_PLATFORM_IOS) && !defined(RDE_PLATFORM_IOS_SIMULATOR)   // ML Kit has no Simulator build: the stand-ins there (.c)

#include <math.h>
#include <string.h>

#define FUDE_TEXTSCAN_SIDE    2048.0   // a photo's pixels on the long side, at most
#define FUDE_TEXTSCAN_QUALITY 0.85     // the shown copy's JPEG quality

static MLKTextRecognizer*   fude_textscan_recognizer;

// A photo, picked and read.
static FUDE_TEXTSCAN_STATE_ fude_textscan_now = FUDE_TEXTSCAN_IDLE;
static id                   fude_textscan_delegate;      // the picker's delegate, kept while it is up
static NSData*              fude_textscan_jpeg;          // the result's image, kept until the next pick
static fude_textscan_line   fude_textscan_lines[FUDE_TEXTSCAN_LINES];
static u32                  fude_textscan_line_count;
static u32                  fude_textscan_width;
static u32                  fude_textscan_height;

// A camera frame, read.
static b8                   fude_textscan_frame_busy;
static b8                   fude_textscan_frame_done;
static fude_textscan_line   fude_textscan_frame_lines[FUDE_TEXTSCAN_LINES];
static u32                  fude_textscan_frame_count;
static u32                  fude_textscan_frame_width;
static u32                  fude_textscan_frame_height;
static double               fude_textscan_frame_started;
static u32                  fude_textscan_frame_reads;   // how many, for the log now and then

// _src (UTF-8) into _dst, cut at a whole character when it does not fit.
static void fude_textscan_copy(c8* _dst, usize _size, const c8* _src) {
    usize _n = _src != NULL ? strlen(_src) : 0;
    if(_n >= _size) {
        _n = _size - 1;
        while(_n > 0 && ((u8)_src[_n] & 0xC0u) == 0x80u) {
            _n--;   // not inside a character
        }
    }
    if(_n > 0) {
        memcpy(_dst, _src, _n);
    }
    _dst[_n] = 0;
}

// _image redrawn upright (its orientation applied), at most _side pixels on its
// long side (0: as it is), one point a pixel.
static UIImage* fude_textscan_upright(UIImage* _image, double _side) {
    const double _w = _image.size.width * _image.scale;   // .size is already upright
    const double _h = _image.size.height * _image.scale;
    const double _k = _side > 0.0 ? fmin(1.0, _side / fmax(fmax(_w, _h), 1.0)) : 1.0;
    const CGSize _size = CGSizeMake(fmax(1.0, round(_w * _k)), fmax(1.0, round(_h * _k)));
    UIGraphicsImageRendererFormat* _format = [UIGraphicsImageRendererFormat preferredFormat];
    _format.scale  = 1.0;
    _format.opaque = YES;
    UIGraphicsImageRenderer* _renderer = [[UIGraphicsImageRenderer alloc] initWithSize:_size format:_format];
    return [_renderer imageWithActions:^(UIGraphicsImageRendererContext* _context) {
        RDE_UNUSED(_context);
        [_image drawInRect:CGRectMake(0.0, 0.0, _size.width, _size.height)];   // drawing applies the orientation
    }];
}

// ML Kit's lines, block by block as it orders them, into _out: how many.
static u32 fude_textscan_collect(MLKText* _text, fude_textscan_line* _out) {
    u32 _n     = 0;
    u32 _block = 0;
    for(MLKTextBlock* _b in _text.blocks) {
        for(MLKTextLine* _l in _b.lines) {
            if(_n >= FUDE_TEXTSCAN_LINES || _l.text.length == 0) {
                continue;
            }
            fude_textscan_line* _line = &_out[_n];
            fude_textscan_copy(_line->text, sizeof(_line->text), _l.text.UTF8String);
            _line->block = _block;
            if(_l.cornerPoints.count == 4) {
                for(u32 _c = 0; _c < 4u; _c++) {
                    const CGPoint _p = [_l.cornerPoints[_c] CGPointValue];
                    _line->corners[_c] = (rde_vec_2F){ (f32)_p.x, (f32)_p.y };
                }
            } else {
                const CGRect _r = _l.frame;
                _line->corners[0] = (rde_vec_2F){ (f32)CGRectGetMinX(_r), (f32)CGRectGetMinY(_r) };
                _line->corners[1] = (rde_vec_2F){ (f32)CGRectGetMaxX(_r), (f32)CGRectGetMinY(_r) };
                _line->corners[2] = (rde_vec_2F){ (f32)CGRectGetMaxX(_r), (f32)CGRectGetMaxY(_r) };
                _line->corners[3] = (rde_vec_2F){ (f32)CGRectGetMinX(_r), (f32)CGRectGetMaxY(_r) };
            }
            _n++;
        }
        _block++;
    }
    return _n;
}

static MLKTextRecognizer* fude_textscan_reader(void) {
    if(fude_textscan_recognizer == nil) {
        fude_textscan_recognizer = [MLKTextRecognizer textRecognizerWithOptions:fude_lang_text_options()];
    }
    return fude_textscan_recognizer;
}

// The photo read: upright and scaled, kept as JPEG for the screen, and given to ML Kit.
static void fude_textscan_read(UIImage* _photo) {
    if(_photo == nil) {
        fude_textscan_now = FUDE_TEXTSCAN_FAILED;
        return;
    }
    fude_textscan_now = FUDE_TEXTSCAN_READING;
    @autoreleasepool {
        UIImage* _upright    = fude_textscan_upright(_photo, FUDE_TEXTSCAN_SIDE);
        fude_textscan_jpeg   = UIImageJPEGRepresentation(_upright, FUDE_TEXTSCAN_QUALITY);
        fude_textscan_width  = (u32)_upright.size.width;
        fude_textscan_height = (u32)_upright.size.height;
        if(fude_textscan_jpeg == nil) {
            fude_textscan_now = FUDE_TEXTSCAN_FAILED;
            return;
        }
        MLKVisionImage* _image = [[MLKVisionImage alloc] initWithImage:_upright];
        _image.orientation     = _upright.imageOrientation;
        [fude_textscan_reader() processImage:_image completion:^(MLKText* _text, NSError* _error) {
            if(_error != nil || _text == nil) {
                rde_log_level(RDE_LOG_LEVEL_ERROR, "ML Kit: text recognition failed: %s", _error != nil ? _error.localizedDescription.UTF8String : "no result");
                fude_textscan_now = FUDE_TEXTSCAN_FAILED;
                return;
            }
            fude_textscan_line_count = fude_textscan_collect(_text, fude_textscan_lines);
            fude_textscan_now        = FUDE_TEXTSCAN_DONE;
            rde_log_level(RDE_LOG_LEVEL_INFO, "ML Kit: %u lines read in a %ux%u photo", fude_textscan_line_count, fude_textscan_width, fude_textscan_height);
        }];
    }
}

@interface KanaTextScanPicker : NSObject <PHPickerViewControllerDelegate>
@end

@implementation KanaTextScanPicker

- (void)picker:(PHPickerViewController*)_picker didFinishPicking:(NSArray<PHPickerResult*>*)_results {
    [_picker dismissViewControllerAnimated:YES completion:nil];
    fude_textscan_delegate = nil;
    if(_results.count == 0) {
        fude_textscan_now = FUDE_TEXTSCAN_CANCELLED;
        return;
    }
    NSItemProvider* _provider = _results[0].itemProvider;
    if(![_provider canLoadObjectOfClass:[UIImage class]]) {
        fude_textscan_now = FUDE_TEXTSCAN_FAILED;
        return;
    }
    fude_textscan_now = FUDE_TEXTSCAN_READING;
    [_provider loadObjectOfClass:[UIImage class] completionHandler:^(id<NSItemProviderReading> _object, NSError* _error) {
        RDE_UNUSED(_error);
        UIImage* _photo = [(NSObject*)_object isKindOfClass:[UIImage class]] ? (UIImage*)_object : nil;
        dispatch_async(dispatch_get_main_queue(), ^{
            fude_textscan_read(_photo);   // on the main thread, where the rest of the app is
        });
    }];
}

@end

// The view controller on top, to present over.
static UIViewController* fude_textscan_top(void) {
    UIWindow* _window = nil;
    for(UIScene* _scene in [UIApplication sharedApplication].connectedScenes) {
        if(![_scene isKindOfClass:[UIWindowScene class]]) {
            continue;
        }
        for(UIWindow* _w in ((UIWindowScene*)_scene).windows) {
            if(_w.isKeyWindow || _window == nil) {
                _window = _w;
            }
        }
    }
    UIViewController* _top = _window.rootViewController;
    while(_top.presentedViewController != nil) {
        _top = _top.presentedViewController;
    }
    return _top;
}

b8 fude_textscan_available(void) {
    return fude_lang_text_readable();   // a language ML Kit reads in pictures (Thai: none)
}

b8 fude_textscan_pick(rde_window* _window) {
    RDE_UNUSED(_window);
    if(!fude_mlkit_enabled() || fude_textscan_now == FUDE_TEXTSCAN_PICKING || fude_textscan_now == FUDE_TEXTSCAN_READING) {
        return false;
    }
    UIViewController* _top = fude_textscan_top();
    if(_top == nil) {
        return false;
    }
    PHPickerConfiguration* _config = [[PHPickerConfiguration alloc] init];
    _config.filter         = [PHPickerFilter imagesFilter];
    _config.selectionLimit = 1;
    PHPickerViewController* _library = [[PHPickerViewController alloc] initWithConfiguration:_config];
    KanaTextScanPicker*     _delegate = [[KanaTextScanPicker alloc] init];
    _library.delegate      = _delegate;
    fude_textscan_delegate = _delegate;
    fude_textscan_now      = FUDE_TEXTSCAN_PICKING;
    [_top presentViewController:_library animated:YES completion:nil];
    return true;
}

FUDE_TEXTSCAN_STATE_ fude_textscan_poll(fude_textscan_result* _out) {
    const FUDE_TEXTSCAN_STATE_ _state = fude_textscan_now;
    if(_state == FUDE_TEXTSCAN_DONE && _out != NULL) {
        _out->image      = (const u8*)fude_textscan_jpeg.bytes;
        _out->image_size = (usize)fude_textscan_jpeg.length;
        _out->width      = fude_textscan_width;
        _out->height     = fude_textscan_height;
        _out->lines      = fude_textscan_lines;
        _out->line_count = fude_textscan_line_count;
    }
    if(_state == FUDE_TEXTSCAN_DONE || _state == FUDE_TEXTSCAN_CANCELLED || _state == FUDE_TEXTSCAN_FAILED) {
        fude_textscan_now = FUDE_TEXTSCAN_IDLE;   // said once
    }
    return _state;
}

// --- camera frames ---------------------------------------------------------------------

// A frame's reading, handed back on the main thread (where fude_textscan_poll_frame looks).
static void fude_textscan_frame_answer(u32 _count, u32 _width, u32 _height) {
    fude_textscan_frame_count  = _count;
    fude_textscan_frame_width  = _width;
    fude_textscan_frame_height = _height;
    fude_textscan_frame_done   = true;
    if(fude_textscan_frame_reads++ % 30u == 0u) {   // now and then: how long a frame takes
        rde_log_level(RDE_LOG_LEVEL_INFO, "ML Kit: a %ux%u frame read in %.0f ms, %u lines", _width, _height,
                      (CACurrentMediaTime() - fude_textscan_frame_started) * 1000.0, _count);
    }
}

b8 fude_textscan_read_frame(const u8* _rgba, u32 _width, u32 _height, f32 _rotation) {
    if(!fude_mlkit_enabled() || fude_textscan_frame_busy || _rgba == NULL || _width == 0 || _height == 0) {
        return false;
    }
    // Only the copy here, on the frame loop's thread: turning it upright and
    // handing it to ML Kit happen on a queue of their own, so a read never holds
    // a frame up. Frames have a recognizer of their own (a photo may be read meanwhile).
    static dispatch_queue_t   _queue;
    static MLKTextRecognizer* _frames_reader;
    if(_queue == nil) {
        _queue         = dispatch_queue_create("kana.textscan.frames", DISPATCH_QUEUE_SERIAL);
        _frames_reader = [MLKTextRecognizer textRecognizerWithOptions:fude_lang_text_options()];
    }
    NSData*             _data   = [NSData dataWithBytes:_rgba length:(NSUInteger)_width * _height * 4u];
    MLKTextRecognizer*  _reader = _frames_reader;
    fude_textscan_frame_busy    = true;
    fude_textscan_frame_started = CACurrentMediaTime();
    dispatch_async(_queue, ^{
        @autoreleasepool {
            // The frame as an image that says which way is up; drawn upright.
            CGDataProviderRef _provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)_data);
            CGColorSpaceRef   _space    = CGColorSpaceCreateDeviceRGB();
            CGImageRef        _cg       = CGImageCreate(_width, _height, 8, 32, (size_t)_width * 4u, _space,
                                                        kCGBitmapByteOrderDefault | (CGBitmapInfo)kCGImageAlphaNoneSkipLast, _provider, NULL, false,
                                                        kCGRenderingIntentDefault);
            CGColorSpaceRelease(_space);
            CGDataProviderRelease(_provider);
            if(_cg == NULL) {
                dispatch_async(dispatch_get_main_queue(), ^{ fude_textscan_frame_answer(0u, 0u, 0u); });
                return;
            }
            // Degrees clockwise to stand upright, as UIKit names them.
            const int          _turn = ((int)lroundf(_rotation) % 360 + 360) % 360;
            UIImageOrientation _way  = _turn == 90 ? UIImageOrientationRight : _turn == 180 ? UIImageOrientationDown : _turn == 270 ? UIImageOrientationLeft
                                                                                                                       : UIImageOrientationUp;
            UIImage* _frame   = [UIImage imageWithCGImage:_cg scale:1.0 orientation:_way];
            CGImageRelease(_cg);
            UIImage*  _upright = fude_textscan_upright(_frame, 0.0);
            const u32 _uw      = (u32)_upright.size.width;
            const u32 _uh      = (u32)_upright.size.height;
            MLKVisionImage* _image = [[MLKVisionImage alloc] initWithImage:_upright];
            _image.orientation     = _upright.imageOrientation;
            [_reader processImage:_image completion:^(MLKText* _text, NSError* _error) {   // the main queue
                const u32 _n = (_error == nil && _text != nil) ? fude_textscan_collect(_text, fude_textscan_frame_lines) : 0u;
                fude_textscan_frame_answer(_n, _uw, _uh);
            }];
        }
    });
    return true;
}

// --- a document's pages ------------------------------------------------------------------

static b8                 fude_textscan_page_busy;
static b8                 fude_textscan_page_done;
static fude_textscan_line fude_textscan_page_lines[FUDE_TEXTSCAN_LINES];
static u32                fude_textscan_page_count;
static u32                fude_textscan_page_width;
static u32                fude_textscan_page_height;

b8 fude_textscan_read_page(const u8* _rgba, u32 _width, u32 _height) {
    if(!fude_mlkit_enabled() || fude_textscan_page_busy || _rgba == NULL || _width == 0 || _height == 0) {
        return false;
    }
    // A recognizer and a queue of its own: the camera's frames are never kept waiting.
    static dispatch_queue_t   _queue;
    static MLKTextRecognizer* _pages_reader;
    if(_queue == nil) {
        _queue        = dispatch_queue_create("kana.textscan.pages", DISPATCH_QUEUE_SERIAL);
        _pages_reader = [MLKTextRecognizer textRecognizerWithOptions:fude_lang_text_options()];
    }
    NSData*            _data   = [NSData dataWithBytes:_rgba length:(NSUInteger)_width * _height * 4u];
    MLKTextRecognizer* _reader = _pages_reader;
    fude_textscan_page_busy    = true;
    dispatch_async(_queue, ^{
        @autoreleasepool {
            CGDataProviderRef _provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)_data);
            CGColorSpaceRef   _space    = CGColorSpaceCreateDeviceRGB();
            CGImageRef        _cg       = CGImageCreate(_width, _height, 8, 32, (size_t)_width * 4u, _space,
                                                        kCGBitmapByteOrderDefault | (CGBitmapInfo)kCGImageAlphaNoneSkipLast, _provider, NULL, false,
                                                        kCGRenderingIntentDefault);
            CGColorSpaceRelease(_space);
            CGDataProviderRelease(_provider);
            if(_cg == NULL) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    fude_textscan_page_count = 0u;
                    fude_textscan_page_done  = true;
                });
                return;
            }
            UIImage* _page = [UIImage imageWithCGImage:_cg scale:1.0 orientation:UIImageOrientationUp];
            CGImageRelease(_cg);
            MLKVisionImage* _image = [[MLKVisionImage alloc] initWithImage:_page];
            _image.orientation     = UIImageOrientationUp;
            [_reader processImage:_image completion:^(MLKText* _text, NSError* _error) {   // the main queue
                fude_textscan_page_count  = (_error == nil && _text != nil) ? fude_textscan_collect(_text, fude_textscan_page_lines) : 0u;
                fude_textscan_page_width  = _width;
                fude_textscan_page_height = _height;
                fude_textscan_page_done   = true;
            }];
        }
    });
    return true;
}

b8 fude_textscan_poll_page(fude_textscan_result* _out) {
    if(!fude_textscan_page_done) {
        return false;
    }
    fude_textscan_page_done = false;
    fude_textscan_page_busy = false;
    if(_out != NULL) {
        _out->image      = NULL;
        _out->image_size = 0;
        _out->width      = fude_textscan_page_width;
        _out->height     = fude_textscan_page_height;
        _out->lines      = fude_textscan_page_lines;
        _out->line_count = fude_textscan_page_count;
    }
    return true;
}

b8 fude_textscan_poll_frame(fude_textscan_result* _out) {
    if(!fude_textscan_frame_done) {
        return false;
    }
    fude_textscan_frame_done = false;
    fude_textscan_frame_busy = false;
    if(_out != NULL) {
        _out->image      = NULL;
        _out->image_size = 0;
        _out->width      = fude_textscan_frame_width;
        _out->height     = fude_textscan_frame_height;
        _out->lines      = fude_textscan_frame_lines;
        _out->line_count = fude_textscan_frame_count;
    }
    return true;
}

#endif
