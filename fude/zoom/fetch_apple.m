// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/fetch.h"

// ===========================================================================
// See fetch.h. Apple's (macOS, iOS): an NSURLSession download, its bytes read off
// the task as it goes, the file it leaves moved to <path>.part then into place.
// ===========================================================================

#import <Foundation/Foundation.h>

static NSURLSessionDownloadTask* fude_fetch_task  = nil;
static volatile FUDE_FETCH_      fude_fetch_status = FUDE_FETCH_IDLE;
static volatile u64              fude_fetch_got = 0, fude_fetch_total = 0;

b8 fude_fetch_start(const c8* _url, const c8* _path) {
    if(fude_fetch_status == FUDE_FETCH_RUNNING) {
        return false;
    }
    NSURL*    _from = [NSURL URLWithString:[NSString stringWithUTF8String:_url]];
    NSString* _to   = [NSString stringWithUTF8String:_path];
    if(_from == nil || _to == nil) {
        return false;
    }
    fude_fetch_status = FUDE_FETCH_RUNNING;
    fude_fetch_got    = 0;
    fude_fetch_total  = 0;
    fude_fetch_task = [[NSURLSession sharedSession] downloadTaskWithURL:_from completionHandler:^(NSURL* _file, NSURLResponse* _response, NSError* _error) {
        NSInteger _code = [_response isKindOfClass:[NSHTTPURLResponse class]] ? ((NSHTTPURLResponse*)_response).statusCode : 0;
        if(_error != nil || _file == nil || _code < 200 || _code >= 300) {
            fude_fetch_status = FUDE_FETCH_FAILED;
            return;
        }
        // (the part first, then into place: never a half file where the file goes)
        NSFileManager* _fm   = [NSFileManager defaultManager];
        NSString*      _part = [_to stringByAppendingString:@".part"];
        [_fm removeItemAtPath:_part error:nil];
        [_fm createDirectoryAtPath:[_to stringByDeletingLastPathComponent] withIntermediateDirectories:YES attributes:nil error:nil];
        if(![_fm moveItemAtURL:_file toURL:[NSURL fileURLWithPath:_part] error:nil]) {
            fude_fetch_status = FUDE_FETCH_FAILED;
            return;
        }
        [_fm removeItemAtPath:_to error:nil];
        fude_fetch_status = [_fm moveItemAtPath:_part toPath:_to error:nil] ? FUDE_FETCH_DONE : FUDE_FETCH_FAILED;
    }];
    [fude_fetch_task resume];
    return true;
}

FUDE_FETCH_ fude_fetch_state(u64* _got, u64* _total) {
    if(fude_fetch_task != nil && fude_fetch_status == FUDE_FETCH_RUNNING) {
        const int64_t _g = fude_fetch_task.countOfBytesReceived, _t = fude_fetch_task.countOfBytesExpectedToReceive;
        fude_fetch_got   = _g > 0 ? (u64)_g : 0u;
        fude_fetch_total = _t > 0 ? (u64)_t : 0u;
    }
    *_got   = fude_fetch_got;
    *_total = fude_fetch_total;
    return fude_fetch_status;
}

void fude_fetch_cancel(void) {
    if(fude_fetch_task != nil) {
        [fude_fetch_task cancel];
        fude_fetch_task = nil;
    }
    fude_fetch_status = FUDE_FETCH_IDLE;
}
