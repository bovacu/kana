#include "mlkit.h"

// ===========================================================================
// See mlkit.h. The iOS side: ML Kit's Objective-C API behind mlkit.h's C one
// (the builder compiles Objective-C with ARC: the statics below keep what is kept).
// ===========================================================================

#if defined(RDE_PLATFORM_IOS)

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <MLKitCommon/MLKitCommon.h>
#import <MLKitDigitalInkRecognition/MLKitDigitalInkRecognition.h>

#include <math.h>
#include <string.h>

#define KANA_MLKIT_STROKE_GAP_MS 300   // strokes carry no clock between them: this much apart
#define KANA_MLKIT_RESULT        4096

static KANA_MLKIT_                    kana_mlkit_now = KANA_MLKIT_IDLE;
static MLKDigitalInkRecognitionModel* kana_mlkit_model;
static MLKDigitalInkRecognizer*       kana_mlkit_recognizer;
static id                             kana_mlkit_on_success;
static id                             kana_mlkit_on_failure;
static b8                             kana_mlkit_busy;
static b8                             kana_mlkit_done;
static f64                            kana_mlkit_started;
static f64                            kana_mlkit_took;
static c8                             kana_mlkit_answer[KANA_MLKIT_RESULT];

KANA_MLKIT_ kana_mlkit_state(void) {
    return kana_mlkit_now;
}

static void kana_mlkit_make_recognizer(void) {
    MLKDigitalInkRecognizerOptions* _options = [[MLKDigitalInkRecognizerOptions alloc] initWithModel:kana_mlkit_model];
    kana_mlkit_recognizer = [MLKDigitalInkRecognizer digitalInkRecognizerWithOptions:_options];
    kana_mlkit_now = kana_mlkit_recognizer != nil ? KANA_MLKIT_READY : KANA_MLKIT_FAILED;
}

void kana_mlkit_prepare(void) {
    if(!kana_mlkit_enabled() || kana_mlkit_now == KANA_MLKIT_DOWNLOADING || kana_mlkit_now == KANA_MLKIT_READY) {
        return;
    }
    @autoreleasepool {
        if(kana_mlkit_model == nil) {
            MLKDigitalInkRecognitionModelIdentifier* _id = [MLKDigitalInkRecognitionModelIdentifier modelIdentifierForLanguageTag:@"ja"];
            if(_id == nil) {
                rde_log_level(RDE_LOG_LEVEL_ERROR, "ML Kit: no Japanese model");
                kana_mlkit_now = KANA_MLKIT_FAILED;
                return;
            }
            kana_mlkit_model = [[MLKDigitalInkRecognitionModel alloc] initWithModelIdentifier:_id];
        }

        MLKModelManager* _manager = [MLKModelManager modelManager];
        if([_manager isModelDownloaded:kana_mlkit_model]) {
            kana_mlkit_make_recognizer();
            return;
        }

        // Downloading: the answer comes as a notification, on the main queue.
        kana_mlkit_now = KANA_MLKIT_DOWNLOADING;
        if(kana_mlkit_on_success == nil) {
            NSNotificationCenter* _center = [NSNotificationCenter defaultCenter];
            kana_mlkit_on_success = [_center addObserverForName:MLKModelDownloadDidSucceedNotification object:nil queue:[NSOperationQueue mainQueue]
                                                      usingBlock:^(NSNotification* _note) {
                RDE_UNUSED(_note);
                rde_log_level(RDE_LOG_LEVEL_INFO, "ML Kit: the Japanese model is downloaded");
                kana_mlkit_make_recognizer();
            }];
            kana_mlkit_on_failure = [_center addObserverForName:MLKModelDownloadDidFailNotification object:nil queue:[NSOperationQueue mainQueue]
                                                      usingBlock:^(NSNotification* _note) {
                NSError* _error = _note.userInfo[MLKModelDownloadUserInfoKeyError];
                rde_log_level(RDE_LOG_LEVEL_ERROR, "ML Kit: the model download failed: %s", _error != nil ? _error.localizedDescription.UTF8String : "?");
                kana_mlkit_now = KANA_MLKIT_FAILED;
            }];
        }
        MLKModelDownloadConditions* _conditions = [[MLKModelDownloadConditions alloc] initWithAllowsCellularAccess:YES allowsBackgroundDownloading:YES];
        [_manager downloadModel:kana_mlkit_model conditions:_conditions];
    }
}

b8 kana_mlkit_recognize(const kana_ink* _ink, const c8* _pre_context) {
    if(!kana_mlkit_enabled() || kana_mlkit_now != KANA_MLKIT_READY || kana_mlkit_busy || _ink == NULL) {
        return false;
    }
    @autoreleasepool {
        NSMutableArray* _strokes = [NSMutableArray array];
        rde_vec_2F      _min     = { 1e30f, 1e30f };
        rde_vec_2F      _max     = { -1e30f, -1e30f };
        long            _clock   = 0;
        for(u32 _s = 0; _s < kana_ink_stroke_count(_ink); _s++) {
            const kana_ink_stroke* _stroke = kana_ink_stroke_at(_ink, _s);
            if(!_stroke->alive || _stroke->point_count == 0) {
                continue;
            }
            const kana_ink_point* _p      = kana_ink_stroke_points(_ink, _stroke);
            NSMutableArray*       _points = [NSMutableArray arrayWithCapacity:_stroke->point_count];
            long                  _t      = _clock;
            for(u32 _k = 0; _k < _stroke->point_count; _k++) {
                _t = _clock + (long)(_p[_k].time * 1000.0f);
                // ML Kit's y grows downwards, like a screen's.
                MLKStrokePoint* _point = [[MLKStrokePoint alloc] initWithX:_p[_k].position.x y:-_p[_k].position.y t:_t];
                [_points addObject:_point];
                _min = (rde_vec_2F){ fminf(_min.x, _p[_k].position.x), fminf(_min.y, _p[_k].position.y) };
                _max = (rde_vec_2F){ fmaxf(_max.x, _p[_k].position.x), fmaxf(_max.y, _p[_k].position.y) };
            }
            _clock = _t + KANA_MLKIT_STROKE_GAP_MS;
            MLKStroke* _mstroke = [[MLKStroke alloc] initWithPoints:_points];
            [_strokes addObject:_mstroke];
        }
        if(_strokes.count == 0) {
            return false;
        }

        MLKInk*                           _mink    = [[MLKInk alloc] initWithStrokes:_strokes];
        MLKWritingArea*                   _area    = [[MLKWritingArea alloc] initWithWidth:fmaxf(_max.x - _min.x, 1.0f) height:fmaxf(_max.y - _min.y, 1.0f)];
        NSString*                         _before  = _pre_context != NULL ? [NSString stringWithUTF8String:_pre_context] : @"";
        MLKDigitalInkRecognitionContext*  _context = [[MLKDigitalInkRecognitionContext alloc] initWithPreContext:_before writingArea:_area];

        kana_mlkit_busy    = true;
        kana_mlkit_done    = false;
        kana_mlkit_started = CACurrentMediaTime();
        [kana_mlkit_recognizer recognizeInk:_mink context:_context completion:^(MLKDigitalInkRecognitionResult* _result, NSError* _error) {
            // Best first, one per line.
            usize _len = 0;
            kana_mlkit_answer[0] = 0;
            if(_error != nil) {
                rde_log_level(RDE_LOG_LEVEL_ERROR, "ML Kit: recognition failed: %s", _error.localizedDescription.UTF8String);
            }
            for(MLKDigitalInkRecognitionCandidate* _candidate in _result.candidates) {
                const c8*   _text = _candidate.text.UTF8String;
                const usize _n    = _text != NULL ? strlen(_text) : 0;
                if(_len + _n + 2 > sizeof(kana_mlkit_answer)) {
                    break;
                }
                memcpy(kana_mlkit_answer + _len, _text, _n);
                _len += _n;
                kana_mlkit_answer[_len++] = '\n';
                kana_mlkit_answer[_len]   = 0;
            }
            kana_mlkit_took = (CACurrentMediaTime() - kana_mlkit_started) * 1000.0;
            kana_mlkit_done = true;
        }];
    }
    return true;
}

b8 kana_mlkit_poll(c8* _out, usize _size, f64* _ms) {
    if(!kana_mlkit_done) {
        return false;
    }
    kana_mlkit_done = false;
    kana_mlkit_busy = false;
    if(_out != NULL && _size > 0) {
        strncpy(_out, kana_mlkit_answer, _size - 1);
        _out[_size - 1] = 0;
    }
    if(_ms != NULL) {
        *_ms = kana_mlkit_took;
    }
    return true;
}

#endif
