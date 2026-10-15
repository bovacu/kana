// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/fetch.h"

// ===========================================================================
// See fetch.h. Apple's (macOS, iOS): an NSURLSession download task on the shared session, its byte counts read off
// the task as it goes, the file it leaves moved to <path>.part and then into place by its completion handler, which
// runs on the session's own queue and sets the state.
// ===========================================================================

#import <Foundation/Foundation.h>

// The download running or last run; nil before the first.
static NSURLSessionDownloadTask* fude_fetch_task  = nil;
// The state, set to RUNNING here and to DONE or FAILED by the completion handler on the session's queue.
static volatile FUDE_FETCH_      fude_fetch_status = FUDE_FETCH_IDLE;
// The bytes received and expected (0 while not known), as last read off the task by fude_fetch_state.
static volatile u64              fude_fetch_got = 0, fude_fetch_total = 0;

// See the header. Returns false, starting nothing, while a download runs or when _url or _path cannot be made into
// an NSURL and an NSString; a response outside 2xx counts as a failure.
b8 fude_fetch_start(const c8* _url, const c8* _path) {
    if(fude_fetch_status == FUDE_FETCH_RUNNING || _url == NULL || _path == NULL) {
        return false;
    }
    NSString* _text = [NSString stringWithUTF8String:_url];
    NSURL*    _from = _text != nil ? [NSURL URLWithString:_text] : nil;   // (NSURL throws on a nil string: not UTF-8)
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

// See the header. While the download runs, the counts are read from the task (each 0 while the task does not know it);
// once it ends, the last counts read are kept.
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

