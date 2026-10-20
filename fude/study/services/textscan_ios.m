// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// Apple's headers come before RDE's, because rde.h's `any` macro would otherwise reach the availability attributes
// in Photos' headers (apply_to = any(...)).
#import <TargetConditionals.h>
#if TARGET_OS_IOS && !TARGET_OS_SIMULATOR
#import <UIKit/UIKit.h>
#import <PhotosUI/PhotosUI.h>
#import <QuartzCore/QuartzCore.h>
#import <MLKitVision/MLKitVision.h>
#import <MLKitTextRecognitionCommon/MLKitTextRecognitionCommon.h>
#endif

#include "study/services/textscan.h"
#include "drawing/ink/mlkit.h"
#include "lang/lang.h"

// ===========================================================================
// See textscan.h. The iOS side: the system's photo library picker (PHPickerViewController, which needs no
// permission, since only the photo picked reaches the app), and ML Kit Text Recognition with the language's model
// (lang.h: fude_lang_text_options; Japanese's, for one), for photos, camera frames (RDE's camera hands the app the
// pixels) and a document's pages. The builder compiles Objective-C with ARC, so the statics below are what keeps
// each object alive.
//
// Whatever is read is redrawn upright first, one point a pixel (a photo at most FUDE_TEXTSCAN_SIDE pixels on its
// long side), so what ML Kit measures and what the screen shows are the same pixels, the same way up. The photo,
// the frames and the pages each have their own recognizer, and the frames and pages their own serial queue, so one of
// each can be read at once.
// ===========================================================================

// ML Kit has no Simulator build, so the Simulator uses the stand-ins in textscan.c.
#if defined(RDE_PLATFORM_IOS) && !defined(RDE_PLATFORM_IOS_SIMULATOR)   // A device only; the Simulator uses textscan.c.

#define FUDE_TEXTSCAN_SIDE    2048.0   // A photo's longer side, in pixels, at most.
#define FUDE_TEXTSCAN_QUALITY 0.85     // The JPEG quality of the copy shown on screen.

// The photo's recognizer, made the first time a photo is read.
static MLKTextRecognizer*   fude_textscan_recognizer;

// The photo picked and read: its state, and its result once it is DONE.
static FUDE_TEXTSCAN_STATE_ fude_textscan_now = FUDE_TEXTSCAN_IDLE;
static id                   fude_textscan_delegate;      // The picker's delegate, kept alive while the picker is up.
static NSData*              fude_textscan_jpeg;          // The result's image, kept until the next pick.
// Each channel's lines are an rde_arr of fude_textscan_line on the std allocator, made with the first reading and kept
// for the run (fude_textscan_collect), with their count beside them.
static rde_arr              fude_textscan_lines;
static u32                  fude_textscan_line_count;
static u32                  fude_textscan_width;
static u32                  fude_textscan_height;

// The last camera frame read. busy is set while a frame is being read and cleared when its result is taken; done is
// set on the main thread when the result is in.
static b8                   fude_textscan_frame_busy;
static b8                   fude_textscan_frame_done;
static rde_arr              fude_textscan_frame_lines;
static u32                  fude_textscan_frame_count;
static u32                  fude_textscan_frame_width;
static u32                  fude_textscan_frame_height;
static double               fude_textscan_frame_started;
static u32                  fude_textscan_frame_reads;   // How many frames were read, to log the time now and then.

// Copies _src (UTF-8) into _dst of _size bytes, cut at a whole character when it does not fit.
static void fude_textscan_copy(c8* _dst, usize _size, const c8* _src) {
    usize _n = _src != NULL ? rde_strlen(_src) : 0;
    if(_n >= _size) {
        _n = _size - 1;
        while(_n > 0 && ((u8)_src[_n] & 0xC0u) == 0x80u) {
            _n--;   // Do not cut inside a character.
        }
    }
    if(_n > 0) {
        rde_memcpy(_dst, _src, _n);
    }
    _dst[_n] = 0;
}

// _image redrawn upright (its orientation applied), one point a pixel, and at most _side pixels on its longer side
// (0 keeps its size).
static UIImage* fude_textscan_upright(UIImage* _image, double _side) {
    const double _w = _image.size.width * _image.scale;   // Its size is already upright.
    const double _h = _image.size.height * _image.scale;
    const double _k = _side > 0.0 ? rde_math_min_f64(1.0, _side / rde_math_max_f64(rde_math_max_f64(_w, _h), 1.0)) : 1.0;
    const CGSize _size = CGSizeMake(rde_math_max_f64(1.0, rde_math_round_f64(_w * _k)), rde_math_max_f64(1.0, rde_math_round_f64(_h * _k)));
    UIGraphicsImageRendererFormat* _format = [UIGraphicsImageRendererFormat preferredFormat];
    _format.scale  = 1.0;
    _format.opaque = YES;
    UIGraphicsImageRenderer* _renderer = [[UIGraphicsImageRenderer alloc] initWithSize:_size format:_format];
    return [_renderer imageWithActions:^(UIGraphicsImageRendererContext* _context) {
        RDE_UNUSED(_context);
        [_image drawInRect:CGRectMake(0.0, 0.0, _size.width, _size.height)];   // Drawing applies the orientation.
    }];
}

// Copies ML Kit's lines into _out (a channel's rde_arr of fude_textscan_line: made here the first time, on the std
// allocator, and emptied first), block by block in the order it gives them, and returns how many. Every line is kept
// but the empty ones. A line without four corner points takes its frame's corners. ML Kit answers on the main queue,
// so this runs on the main thread, as the polls that read the lines do.
static u32 fude_textscan_collect(MLKText* _text, rde_arr* _out) {
    if(!rde_arr_is_inited(_out)) {
        *_out = rde_arr_new(sizeof(fude_textscan_line), rde_memory_allocator_get_default_std());
    }
    rde_arr_clear(_out);
    u32 _block = 0;
    for(MLKTextBlock* _b in _text.blocks) {
        for(MLKTextLine* _l in _b.lines) {
            if(_l.text.length == 0) {
                continue;
            }
            fude_textscan_line* _line = (fude_textscan_line*)rde_arr_add_n(_out, 1u);
            rde_memset(_line, 0, sizeof(*_line));
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
        }
        _block++;
    }
    return (u32)rde_arr_length(_out);
}

// Returns a channel's first line, for a result to point at (NULL before the channel has read anything).
static const fude_textscan_line* fude_textscan_first(const rde_arr* _lines) {
    return rde_arr_is_inited(_lines) ? (const fude_textscan_line*)_lines->memory : NULL;
}

// The photo's recognizer, made the first time it is needed.
static MLKTextRecognizer* fude_textscan_reader(void) {
    if(fude_textscan_recognizer == nil) {
        fude_textscan_recognizer = [MLKTextRecognizer textRecognizerWithOptions:fude_lang_text_options()];
    }
    return fude_textscan_recognizer;
}

// Reads the photo picked: it is made upright and scaled, kept as JPEG for the screen, and given to ML Kit. Called on
// the main thread; ML Kit answers there too.
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
            fude_textscan_line_count = fude_textscan_collect(_text, &fude_textscan_lines);
            fude_textscan_now        = FUDE_TEXTSCAN_DONE;
            rde_log_level(RDE_LOG_LEVEL_INFO, "ML Kit: %u lines read in a %ux%u photo", fude_textscan_line_count, fude_textscan_width, fude_textscan_height);
        }];
    }
}

// The photo picker's delegate: it learns which photo was picked.
@interface FudeTextScanPicker : NSObject <PHPickerViewControllerDelegate>
@end

@implementation FudeTextScanPicker

// Closes the picker and loads the photo picked, if any. The photo arrives on another thread and is handed to the main
// thread to be read.
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
            fude_textscan_read(_photo);   // On the main thread, where the rest of the app is.
        });
    }];
}

@end

// The view controller on top of the key window, to present the picker over.
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

// Every read below asks this first: for a language with no model (Thai, Arabic) there is no recognizer to make
// (fude_lang_text_options is nil), and a read started anyway would never answer, leaving its page "reading" for good.
b8 fude_textscan_available(void) {
    return fude_lang_text_readable();   // A language ML Kit reads in pictures (not Thai or Arabic: no model).
}

// See the header. _window is not needed: the picker is presented over the view controller on top.
b8 fude_textscan_pick(rde_window* _window) {
    RDE_UNUSED(_window);
    if(!fude_textscan_available() || !fude_mlkit_enabled() || fude_textscan_now == FUDE_TEXTSCAN_PICKING || fude_textscan_now == FUDE_TEXTSCAN_READING) {
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
    FudeTextScanPicker*     _delegate = [[FudeTextScanPicker alloc] init];
    _library.delegate      = _delegate;
    fude_textscan_delegate = _delegate;
    fude_textscan_now      = FUDE_TEXTSCAN_PICKING;
    [_top presentViewController:_library animated:YES completion:nil];
    return true;
}

// See the header.
FUDE_TEXTSCAN_STATE_ fude_textscan_poll(fude_textscan_result* _out) {
    const FUDE_TEXTSCAN_STATE_ _state = fude_textscan_now;
    if(_state == FUDE_TEXTSCAN_DONE && _out != NULL) {
        _out->image      = (const u8*)fude_textscan_jpeg.bytes;
        _out->image_size = (usize)fude_textscan_jpeg.length;
        _out->width      = fude_textscan_width;
        _out->height     = fude_textscan_height;
        _out->lines      = fude_textscan_first(&fude_textscan_lines);
        _out->line_count = fude_textscan_line_count;
    }
    if(_state == FUDE_TEXTSCAN_DONE || _state == FUDE_TEXTSCAN_CANCELLED || _state == FUDE_TEXTSCAN_FAILED) {
        fude_textscan_now = FUDE_TEXTSCAN_IDLE;   // Each of these is reported once.
    }
    return _state;
}

// --- camera frames ---------------------------------------------------------------------

// Keeps a frame's reading, on the main thread, where fude_textscan_poll_frame looks for it. Every 30 frames it logs
// how long a frame took.
static void fude_textscan_frame_answer(u32 _count, u32 _width, u32 _height) {
    fude_textscan_frame_count  = _count;
    fude_textscan_frame_width  = _width;
    fude_textscan_frame_height = _height;
    fude_textscan_frame_done   = true;
    if(fude_textscan_frame_reads++ % 30u == 0u) {   // Now and then, log how long a frame takes.
        rde_log_level(RDE_LOG_LEVEL_INFO, "ML Kit: a %ux%u frame read in %.0f ms, %u lines", _width, _height,
                      (CACurrentMediaTime() - fude_textscan_frame_started) * 1000.0, _count);
    }
}

// See the header.
b8 fude_textscan_read_frame(const u8* _rgba, u32 _width, u32 _height, f32 _rotation) {
    if(!fude_textscan_available() || !fude_mlkit_enabled() || fude_textscan_frame_busy || _rgba == NULL || _width == 0 || _height == 0) {
        return false;
    }
    // Only the copy is made here, on the frame loop's thread. Turning it upright and handing it to ML Kit happen on a
    // queue of their own, so a read never holds a frame up. Frames have a recognizer of their own, because a photo may
    // be read meanwhile.
    static dispatch_queue_t   _queue;
    static MLKTextRecognizer* _frames_reader;
    if(_queue == nil) {
        _queue         = dispatch_queue_create("fude.textscan.frames", DISPATCH_QUEUE_SERIAL);
        _frames_reader = [MLKTextRecognizer textRecognizerWithOptions:fude_lang_text_options()];
    }
    NSData*             _data   = [NSData dataWithBytes:_rgba length:(NSUInteger)_width * _height * 4u];
    MLKTextRecognizer*  _reader = _frames_reader;
    fude_textscan_frame_busy    = true;
    fude_textscan_frame_started = CACurrentMediaTime();
    dispatch_async(_queue, ^{
        @autoreleasepool {
            // The frame as an image that says which way is up, then drawn upright.
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
            // The degrees clockwise that stand it upright, as UIKit's orientations name them.
            const int          _turn = ((int)rde_math_round_to_i64_f32(_rotation) % 360 + 360) % 360;
            UIImageOrientation _way  = _turn == 90 ? UIImageOrientationRight : _turn == 180 ? UIImageOrientationDown : _turn == 270 ? UIImageOrientationLeft
                                                                                                                       : UIImageOrientationUp;
            UIImage* _frame   = [UIImage imageWithCGImage:_cg scale:1.0 orientation:_way];
            CGImageRelease(_cg);
            UIImage*  _upright = fude_textscan_upright(_frame, 0.0);
            const u32 _uw      = (u32)_upright.size.width;
            const u32 _uh      = (u32)_upright.size.height;
            MLKVisionImage* _image = [[MLKVisionImage alloc] initWithImage:_upright];
            _image.orientation     = _upright.imageOrientation;
            [_reader processImage:_image completion:^(MLKText* _text, NSError* _error) {   // On the main queue.
                const u32 _n = (_error == nil && _text != nil) ? fude_textscan_collect(_text, &fude_textscan_frame_lines) : 0u;
                fude_textscan_frame_answer(_n, _uw, _uh);
            }];
        }
    });
    return true;
}

// --- a document's pages ------------------------------------------------------------------

// The last document page read, kept as the frame's are.
static b8                 fude_textscan_page_busy;
static b8                 fude_textscan_page_done;
static rde_arr            fude_textscan_page_lines;
static u32                fude_textscan_page_count;
static u32                fude_textscan_page_width;
static u32                fude_textscan_page_height;

// See the header. The page is already upright, so it is handed to ML Kit as it is.
b8 fude_textscan_read_page(const u8* _rgba, u32 _width, u32 _height) {
    if(!fude_textscan_available() || !fude_mlkit_enabled() || fude_textscan_page_busy || _rgba == NULL || _width == 0 || _height == 0) {
        return false;
    }
    // Pages have a recognizer and a queue of their own, so the camera's frames are never kept waiting.
    static dispatch_queue_t   _queue;
    static MLKTextRecognizer* _pages_reader;
    if(_queue == nil) {
        _queue        = dispatch_queue_create("fude.textscan.pages", DISPATCH_QUEUE_SERIAL);
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
            [_reader processImage:_image completion:^(MLKText* _text, NSError* _error) {   // On the main queue.
                fude_textscan_page_count  = (_error == nil && _text != nil) ? fude_textscan_collect(_text, &fude_textscan_page_lines) : 0u;
                fude_textscan_page_width  = _width;
                fude_textscan_page_height = _height;
                fude_textscan_page_done   = true;
            }];
        }
    });
    return true;
}

// See the header. Taking the result frees the channel for the next page.
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
        _out->lines      = fude_textscan_first(&fude_textscan_page_lines);
        _out->line_count = fude_textscan_page_count;
    }
    return true;
}

// See the header. Taking the result frees the channel for the next frame.
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
        _out->lines      = fude_textscan_first(&fude_textscan_frame_lines);
        _out->line_count = fude_textscan_frame_count;
    }
    return true;
}

#endif
