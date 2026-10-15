// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "drawing/ink/mlkit.h"

// ===========================================================================
// See mlkit.h.
//
// The iOS side: ML Kit's Objective-C API behind mlkit.h's C one. The builder compiles Objective-C with ARC, so the
// static variables below are what keeps the model, the recognizer and the notification observers alive. An answer is
// written by the recognizer's completion block (on the main queue) into fude_mlkit_answer and taken by
// fude_mlkit_poll.
// ===========================================================================

#if defined(RDE_PLATFORM_IOS) && !defined(RDE_PLATFORM_IOS_SIMULATOR)   // No Simulator build: mlkit.c stands in.

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <MLKitCommon/MLKitCommon.h>
#import <MLKitDigitalInkRecognition/MLKitDigitalInkRecognition.h>

#define FUDE_MLKIT_STROKE_GAP_MS 300   // Strokes keep no clock between them; each starts this many ms after the last.
// The size of the answer's buffer, in bytes; candidates that do not fit are left out.
#define FUDE_MLKIT_RESULT        4096
// Seconds a reading may take before it is given up as answered with nothing. ML Kit always calls back, but should it
// ever not, the app would otherwise never read handwriting with it again until it restarts.
#define FUDE_MLKIT_GIVE_UP       10.0

// Where the model is now.
static FUDE_MLKIT_                    fude_mlkit_now = FUDE_MLKIT_IDLE;
// The model for the app's tag, made by the first prepare, and the recognizer made once it is downloaded.
static MLKDigitalInkRecognitionModel* fude_mlkit_loaded;
static MLKDigitalInkRecognizer*       fude_mlkit_recognizer;
// The observers of the download's success and failure, registered once.
static id                             fude_mlkit_on_success;
static id                             fude_mlkit_on_failure;
// A reading was started and its answer not yet taken; its answer is in.
static b8                             fude_mlkit_busy;
static b8                             fude_mlkit_done;
// When the reading started (CACurrentMediaTime, seconds) and how long it took (milliseconds). Each reading also has a
// number, so that a late answer to a reading that was given up on is dropped.
static f64                            fude_mlkit_started;
static u32                            fude_mlkit_generation;   // The current reading's number.
static f64                            fude_mlkit_took;
// The answer: the candidates, best first, one per line.
static c8                             fude_mlkit_answer[FUDE_MLKIT_RESULT];

// Returns where the model is now.
FUDE_MLKIT_ fude_mlkit_state(void) {
    return fude_mlkit_now;
}

// Makes the recognizer for the downloaded model: READY when it could be made, FAILED otherwise.
static void fude_mlkit_make_recognizer(void) {
    MLKDigitalInkRecognizerOptions* _options = [[MLKDigitalInkRecognizerOptions alloc] initWithModel:fude_mlkit_loaded];
    fude_mlkit_recognizer = [MLKDigitalInkRecognizer digitalInkRecognizerWithOptions:_options];
    fude_mlkit_now = fude_mlkit_recognizer != nil ? FUDE_MLKIT_READY : FUDE_MLKIT_FAILED;
}

// Makes the model for the app's tag (FAILED when ML Kit has none for it), then makes the recognizer when the model is
// downloaded, or starts the download (cellular and background allowed) and waits for its notification. Nothing while
// the switch is off, while downloading, or once ready.
void fude_mlkit_prepare(void) {
    if(!fude_mlkit_enabled() || fude_mlkit_now == FUDE_MLKIT_DOWNLOADING || fude_mlkit_now == FUDE_MLKIT_READY) {
        return;
    }
    @autoreleasepool {
        if(fude_mlkit_loaded == nil) {
            MLKDigitalInkRecognitionModelIdentifier* _id = [MLKDigitalInkRecognitionModelIdentifier modelIdentifierForLanguageTag:[NSString stringWithUTF8String:fude_mlkit_model()]];
            if(_id == nil) {
                rde_log_level(RDE_LOG_LEVEL_ERROR, "ML Kit: no handwriting model for %s", fude_mlkit_model());
                fude_mlkit_now = FUDE_MLKIT_FAILED;
                return;
            }
            fude_mlkit_loaded = [[MLKDigitalInkRecognitionModel alloc] initWithModelIdentifier:_id];
        }

        MLKModelManager* _manager = [MLKModelManager modelManager];
        if([_manager isModelDownloaded:fude_mlkit_loaded]) {
            fude_mlkit_make_recognizer();
            return;
        }

        // Download it: the outcome comes as a notification, on the main queue. ML Kit sends one for every model it
        // downloads (a study app's translation models too), so each observer first checks that the notification is
        // about this model: another model's download must not mark handwriting ready, or failed.
        fude_mlkit_now = FUDE_MLKIT_DOWNLOADING;
        if(fude_mlkit_on_success == nil) {
            NSNotificationCenter* _center = [NSNotificationCenter defaultCenter];
            fude_mlkit_on_success = [_center addObserverForName:MLKModelDownloadDidSucceedNotification object:nil queue:[NSOperationQueue mainQueue]
                                                      usingBlock:^(NSNotification* _note) {
                if(![_note.userInfo[MLKModelDownloadUserInfoKeyRemoteModel] isEqual:fude_mlkit_loaded]) {
                    return;
                }
                rde_log_level(RDE_LOG_LEVEL_INFO, "ML Kit: the handwriting model is downloaded");
                fude_mlkit_make_recognizer();
            }];
            fude_mlkit_on_failure = [_center addObserverForName:MLKModelDownloadDidFailNotification object:nil queue:[NSOperationQueue mainQueue]
                                                      usingBlock:^(NSNotification* _note) {
                if(![_note.userInfo[MLKModelDownloadUserInfoKeyRemoteModel] isEqual:fude_mlkit_loaded]) {
                    return;
                }
                NSError* _error = _note.userInfo[MLKModelDownloadUserInfoKeyError];
                rde_log_level(RDE_LOG_LEVEL_ERROR, "ML Kit: the model download failed: %s", _error != nil ? _error.localizedDescription.UTF8String : "?");
                fude_mlkit_now = FUDE_MLKIT_FAILED;
            }];
        }
        MLKModelDownloadConditions* _conditions = [[MLKModelDownloadConditions alloc] initWithAllowsCellularAccess:YES allowsBackgroundDownloading:YES];
        [_manager downloadModel:fude_mlkit_loaded conditions:_conditions];
    }
}

// Builds an MLKInk from the alive strokes (y pointing down, times in ms) and starts reading it, with the text before
// it and the ink's size as its writing area. The completion block fills the answer. Returns false when the switch is
// off, the model is not ready, a reading is under way, or there is no stroke.
b8 fude_mlkit_recognize(const fude_ink* _ink, const c8* _pre_context) {
    if(!fude_mlkit_enabled() || fude_mlkit_now != FUDE_MLKIT_READY || fude_mlkit_busy || _ink == NULL) {
        return false;
    }
    @autoreleasepool {
        NSMutableArray* _strokes = [NSMutableArray array];
        rde_vec_2F      _min     = { 1e30f, 1e30f };
        rde_vec_2F      _max     = { -1e30f, -1e30f };
        long            _clock   = 0;
        for(u32 _s = 0; _s < fude_ink_stroke_count(_ink); _s++) {
            const fude_ink_stroke* _stroke = fude_ink_stroke_at(_ink, _s);
            if(!_stroke->alive || _stroke->point_count == 0) {
                continue;
            }
            const fude_ink_point* _p      = fude_ink_stroke_points(_ink, _stroke);
            NSMutableArray*       _points = [NSMutableArray arrayWithCapacity:_stroke->point_count];
            long                  _t      = _clock;
            for(u32 _k = 0; _k < _stroke->point_count; _k++) {
                _t = _clock + (long)(_p[_k].time * 1000.0f);
                // ML Kit's y grows downwards, like a screen's.
                MLKStrokePoint* _point = [[MLKStrokePoint alloc] initWithX:_p[_k].position.x y:-_p[_k].position.y t:_t];
                [_points addObject:_point];
                _min = (rde_vec_2F){ rde_math_min_f32(_min.x, _p[_k].position.x), rde_math_min_f32(_min.y, _p[_k].position.y) };
                _max = (rde_vec_2F){ rde_math_max_f32(_max.x, _p[_k].position.x), rde_math_max_f32(_max.y, _p[_k].position.y) };
            }
            _clock = _t + FUDE_MLKIT_STROKE_GAP_MS;
            MLKStroke* _mstroke = [[MLKStroke alloc] initWithPoints:_points];
            [_strokes addObject:_mstroke];
        }
        if(_strokes.count == 0) {
            return false;
        }

        MLKInk*                           _mink    = [[MLKInk alloc] initWithStrokes:_strokes];
        MLKWritingArea*                   _area    = [[MLKWritingArea alloc] initWithWidth:rde_math_max_f32(_max.x - _min.x, 1.0f) height:rde_math_max_f32(_max.y - _min.y, 1.0f)];
        NSString*                         _given   = _pre_context != NULL ? [NSString stringWithUTF8String:_pre_context] : nil;
        NSString*                         _before  = _given != nil ? _given : @"";   // (nil also when not UTF-8: read with no context)
        MLKDigitalInkRecognitionContext*  _context = [[MLKDigitalInkRecognitionContext alloc] initWithPreContext:_before writingArea:_area];

        fude_mlkit_busy    = true;
        fude_mlkit_done    = false;
        fude_mlkit_started = CACurrentMediaTime();
        const u32 _generation = ++fude_mlkit_generation;
        [fude_mlkit_recognizer recognizeInk:_mink context:_context completion:^(MLKDigitalInkRecognitionResult* _result, NSError* _error) {
            if(_generation != fude_mlkit_generation) {
                return;   // A late answer to a reading given up on (fude_mlkit_poll).
            }
            // The candidates, best first, one per line, as many as fit.
            usize _len = 0;
            fude_mlkit_answer[0] = 0;
            if(_error != nil) {
                rde_log_level(RDE_LOG_LEVEL_ERROR, "ML Kit: recognition failed: %s", _error.localizedDescription.UTF8String);
            }
            for(MLKDigitalInkRecognitionCandidate* _candidate in _result.candidates) {
                const c8*   _text = _candidate.text.UTF8String;
                const usize _n    = _text != NULL ? rde_strlen(_text) : 0;
                if(_len + _n + 2 > sizeof(fude_mlkit_answer)) {
                    break;
                }
                rde_memcpy(fude_mlkit_answer + _len, _text, _n);
                _len += _n;
                fude_mlkit_answer[_len++] = '\n';
                fude_mlkit_answer[_len]   = 0;
            }
            fude_mlkit_took = (CACurrentMediaTime() - fude_mlkit_started) * 1000.0;
            fude_mlkit_done = true;
        }];
    }
    return true;
}

// Takes the answer once it is in, and makes ML Kit free for the next reading. A reading with no answer after
// FUDE_MLKIT_GIVE_UP seconds is taken as answered with nothing. Returns false while there is no answer.
b8 fude_mlkit_poll(c8* _out, usize _size, f64* _ms) {
    if(!fude_mlkit_done) {
        if(!fude_mlkit_busy || CACurrentMediaTime() - fude_mlkit_started < FUDE_MLKIT_GIVE_UP) {
            return false;
        }
        // No answer for this long: take it as an empty one, so the reader falls back to its own and the next reading
        // can start. Should its answer still come, it is dropped, because its number is no longer the current one.
        rde_log_level(RDE_LOG_LEVEL_WARNING, "ML Kit: no answer after %.0f s; given up", FUDE_MLKIT_GIVE_UP);
        fude_mlkit_generation++;
        fude_mlkit_answer[0] = 0;
        fude_mlkit_took      = (CACurrentMediaTime() - fude_mlkit_started) * 1000.0;
        fude_mlkit_done      = true;
    }
    fude_mlkit_done = false;
    fude_mlkit_busy = false;
    if(_out != NULL && _size > 0) {
        rde_strcpy(_out, _size, fude_mlkit_answer);
    }
    if(_ms != NULL) {
        *_ms = fude_mlkit_took;
    }
    return true;
}

#endif
