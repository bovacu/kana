// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// ===========================================================================
// See video.h. Apple's: AVFoundation's AVAssetWriter (H.264 in an MP4), each
// frame a BGRA pixel buffer from its adaptor's pool. Not in real time: a frame
// waits for the writer to be ready for it.
// ===========================================================================

// (Apple's headers before RDE's: its `any` would read their `apply_to = any(...)`.)
#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>

#include <unistd.h>

#include "zoom/video.h"

@interface FudeZoomVideo : NSObject
@property(strong) AVAssetWriter*                        writer;
@property(strong) AVAssetWriterInput*                   input;
@property(strong) AVAssetWriterInputPixelBufferAdaptor* adaptor;
@property(assign) u32                                   width, height, fps;
@property(assign) i64                                   frames;
@end

@implementation FudeZoomVideo
@end

struct fude_zoom_video {
    void* writer;   // (a FudeZoomVideo, retained)
};

b8 fude_zoom_video_available(void) {
    return true;
}

fude_zoom_video* fude_zoom_video_open(const c8* _path, u32 _width, u32 _height, u32 _fps) {
    if(_path == NULL || _width < 16u || _height < 16u || (_width % 2u) != 0u || (_height % 2u) != 0u) {
        return NULL;
    }
    @autoreleasepool {
        NSString* _file = [NSString stringWithUTF8String:_path];
        [[NSFileManager defaultManager] removeItemAtPath:_file error:nil];
        NSError*       _error  = nil;
        AVAssetWriter* _writer = [AVAssetWriter assetWriterWithURL:[NSURL fileURLWithPath:_file] fileType:AVFileTypeMPEG4 error:&_error];
        if(_writer == nil) {
            return NULL;
        }
        NSDictionary* _settings = @{
            AVVideoCodecKey: AVVideoCodecTypeH264,
            AVVideoWidthKey: @(_width),
            AVVideoHeightKey: @(_height),
            AVVideoCompressionPropertiesKey: @{ AVVideoAverageBitRateKey: @(MAX(2000000u, _width * _height * _fps / 5u)) },   // (sharp lines: generous)
        };
        AVAssetWriterInput* _input = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo outputSettings:_settings];
        _input.expectsMediaDataInRealTime = NO;
        NSDictionary* _attributes = @{
            (id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
            (id)kCVPixelBufferWidthKey: @(_width),
            (id)kCVPixelBufferHeightKey: @(_height),
        };
        AVAssetWriterInputPixelBufferAdaptor* _adaptor = [AVAssetWriterInputPixelBufferAdaptor assetWriterInputPixelBufferAdaptorWithAssetWriterInput:_input
                                                                                                                       sourcePixelBufferAttributes:_attributes];
        if(![_writer canAddInput:_input]) {
            return NULL;
        }
        [_writer addInput:_input];
        if(![_writer startWriting]) {
            return NULL;
        }
        [_writer startSessionAtSourceTime:kCMTimeZero];
        FudeZoomVideo* _v = [FudeZoomVideo new];
        _v.writer  = _writer;
        _v.input   = _input;
        _v.adaptor = _adaptor;
        _v.width   = _width;
        _v.height  = _height;
        _v.fps     = _fps > 0u ? _fps : 30u;
        _v.frames  = 0;
        fude_zoom_video* _out = (fude_zoom_video*)calloc(1, sizeof(fude_zoom_video));
        _out->writer = (__bridge_retained void*)_v;
        return _out;
    }
}

b8 fude_zoom_video_add(fude_zoom_video* _video, const u8* _rgba, u32 _stride) {
    if(_video == NULL || _rgba == NULL) {
        return false;
    }
    @autoreleasepool {
        FudeZoomVideo* _v = (__bridge FudeZoomVideo*)_video->writer;
        for(u32 _wait = 0; !_v.input.readyForMoreMediaData && _wait < 2000u; _wait++) {
            usleep(1000);   // (not in real time: the writer catching up)
        }
        if(!_v.input.readyForMoreMediaData || _v.adaptor.pixelBufferPool == NULL) {
            return false;
        }
        CVPixelBufferRef _buffer = NULL;
        if(CVPixelBufferPoolCreatePixelBuffer(NULL, _v.adaptor.pixelBufferPool, &_buffer) != kCVReturnSuccess || _buffer == NULL) {
            return false;
        }
        CVPixelBufferLockBaseAddress(_buffer, 0);
        u8*          _base = (u8*)CVPixelBufferGetBaseAddress(_buffer);
        const usize  _row  = CVPixelBufferGetBytesPerRow(_buffer);
        for(u32 _y = 0; _y < _v.height; _y++) {
            const u8* _from = _rgba + (usize)_y * _stride;
            u8*       _to   = _base + (usize)_y * _row;
            for(u32 _x = 0; _x < _v.width; _x++) {
                _to[4u * _x]      = _from[4u * _x + 2u];   // (RGBA to BGRA)
                _to[4u * _x + 1u] = _from[4u * _x + 1u];
                _to[4u * _x + 2u] = _from[4u * _x];
                _to[4u * _x + 3u] = 255u;
            }
        }
        CVPixelBufferUnlockBaseAddress(_buffer, 0);
        const b8 _ok = [_v.adaptor appendPixelBuffer:_buffer withPresentationTime:CMTimeMake(_v.frames, (int32_t)_v.fps)];
        CVPixelBufferRelease(_buffer);
        if(_ok) {
            _v.frames++;
        }
        return _ok;
    }
}

b8 fude_zoom_video_close(fude_zoom_video* _video, b8 _keep) {
    if(_video == NULL) {
        return false;
    }
    b8 _ok = false;
    @autoreleasepool {
        FudeZoomVideo* _v = (__bridge_transfer FudeZoomVideo*)_video->writer;
        free(_video);
        if(_keep && _v.frames > 0) {
            [_v.input markAsFinished];
            dispatch_semaphore_t _done = dispatch_semaphore_create(0);
            [_v.writer finishWritingWithCompletionHandler:^{
                dispatch_semaphore_signal(_done);
            }];
            dispatch_semaphore_wait(_done, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(20.0 * NSEC_PER_SEC)));
            _ok = _v.writer.status == AVAssetWriterStatusCompleted;
        } else {
            [_v.writer cancelWriting];
        }
        if(!_ok) {
            [[NSFileManager defaultManager] removeItemAtURL:_v.writer.outputURL error:nil];
        }
    }
    return _ok;
}
