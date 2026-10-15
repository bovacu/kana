// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// Apple's headers come before RDE's because rde.h defines `any`, a word that their availability pragmas use.
#import <TargetConditionals.h>
#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#import <PhotosUI/PhotosUI.h>
#import <VisionKit/VisionKit.h>
#endif

#include "drawing/app/import.h"

// ===========================================================================
// See import.h. This is the iOS side: the Photos library's picker (PHPicker, which picks many pictures at once and
// asks no permission) and the document camera (VisionKit, which finds each page's edges and straightens it, for as
// many pages as are scanned). Each picture is written to the temporary folder, and the files are handed over in order
// on the main thread (fude_import_arrived); import.c deletes them once they are a PDF.
//
// The builder compiles Objective-C with ARC, and a picker holds its delegate weakly, so the static below keeps the
// delegate alive while the picker or the camera is up.
// ===========================================================================

#if defined(RDE_PLATFORM_IOS)

static id fude_import_delegate;   // The picker's or the camera's delegate, while it is up; nil otherwise.

// Returns a new, unique path in the temporary folder ("import-<UUID>.<_ext>").
static NSString* fude_import_temp(NSString* _ext) {
    return [NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"import-%@.%@", [[NSUUID UUID] UUIDString], _ext]];
}

// Hands _paths to fude_import_arrived as C strings, from source _kind (FUDE_IMPORT_). An empty list means cancelled.
// The strings only live until it returns, which is enough because fude_import_arrived copies them.
static void fude_import_hand(u8 _kind, NSArray<NSString*>* _paths) {
    const u32    _n = (u32)_paths.count;
    rde_arr TYPE(const c8*) _list = rde_arr_new_with_capacity(sizeof(const c8*), _n > 0u ? _n : 1u, rde_memory_allocator_get_default_std());
    rde_arr_resize(&_list, _n);
    const c8** _c = (const c8**)_list.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        _c[_i] = [_paths[_i] fileSystemRepresentation];
    }
    fude_import_arrived(_kind, _c, _n);
    rde_arr_free(&_list);
}

// Returns the view controller on top of the key window (or of the first window there is), to present a picker over.
static UIViewController* fude_import_top(void) {
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

// --- the Photos library ---------------------------------------------------------------------

@interface FudeImportPhotos : NSObject <PHPickerViewControllerDelegate>
@end

@implementation FudeImportPhotos

// The picker closed: copies each picture out and hands them over once all are in, or hands over none when cancelled.
- (void)picker:(PHPickerViewController*)_picker didFinishPicking:(NSArray<PHPickerResult*>*)_results {
    [_picker dismissViewControllerAnimated:YES completion:nil];
    fude_import_delegate = nil;
    const NSUInteger _n = _results.count;
    if(_n == 0) {
        fude_import_hand(FUDE_IMPORT_PHOTOS, @[]);   // Cancelled.
        return;
    }
    // Copy each picture out as it arrives, because the file the provider gives is deleted once its block returns. The
    // copies keep the order picked, a slot each; a picture that fails leaves its slot empty and is skipped. They are
    // handed over on the main queue when all are in.
    NSMutableArray<NSString*>* _paths = [NSMutableArray arrayWithCapacity:_n];
    for(NSUInteger _i = 0; _i < _n; _i++) {
        [_paths addObject:@""];
    }
    dispatch_group_t _group = dispatch_group_create();
    for(NSUInteger _i = 0; _i < _n; _i++) {
        NSItemProvider* _provider = _results[_i].itemProvider;
        if(![_provider hasItemConformingToTypeIdentifier:@"public.image"]) {
            continue;
        }
        dispatch_group_enter(_group);
        [_provider loadFileRepresentationForTypeIdentifier:@"public.image" completionHandler:^(NSURL* _url, NSError* _error) {
            RDE_UNUSED(_error);
            if(_url != nil) {
                NSString* _to = fude_import_temp(_url.pathExtension.length > 0 ? _url.pathExtension : @"jpg");
                if([[NSFileManager defaultManager] copyItemAtPath:_url.path toPath:_to error:nil]) {
                    @synchronized(_paths) {
                        _paths[_i] = _to;
                    }
                }
            }
            dispatch_group_leave(_group);
        }];
    }
    dispatch_group_notify(_group, dispatch_get_main_queue(), ^{
        NSMutableArray<NSString*>* _got = [NSMutableArray array];
        for(NSString* _path in _paths) {
            if(_path.length > 0) {
                [_got addObject:_path];
            }
        }
        fude_import_hand(FUDE_IMPORT_PHOTOS, _got);
    });
}

@end

// --- the document camera --------------------------------------------------------------------

@interface FudeImportScan : NSObject <VNDocumentCameraViewControllerDelegate>
@end

@implementation FudeImportScan

// The scan is done: writes each page as a JPEG (quality 0.85) to the temporary folder and hands them over in order.
- (void)documentCameraViewController:(VNDocumentCameraViewController*)_controller didFinishWithScan:(VNDocumentCameraScan*)_scan {
    NSMutableArray<NSString*>* _paths = [NSMutableArray array];
    for(NSUInteger _i = 0; _i < _scan.pageCount; _i++) {
        NSData*   _jpeg = UIImageJPEGRepresentation([_scan imageOfPageAtIndex:_i], 0.85);
        NSString* _to   = fude_import_temp(@"jpg");
        if(_jpeg != nil && [_jpeg writeToFile:_to atomically:YES]) {
            [_paths addObject:_to];
        }
    }
    [_controller dismissViewControllerAnimated:YES completion:nil];
    fude_import_delegate = nil;
    fude_import_hand(FUDE_IMPORT_CAMERA, _paths);
}

// The scan was cancelled: hands over nothing.
- (void)documentCameraViewControllerDidCancel:(VNDocumentCameraViewController*)_controller {
    [_controller dismissViewControllerAnimated:YES completion:nil];
    fude_import_delegate = nil;
    fude_import_hand(FUDE_IMPORT_CAMERA, @[]);
}

// The camera failed: logs why and hands over nothing, as if cancelled.
- (void)documentCameraViewController:(VNDocumentCameraViewController*)_controller didFailWithError:(NSError*)_error {
    rde_log_level(RDE_LOG_LEVEL_WARNING, "import: the document camera failed (%s)", _error.localizedDescription.UTF8String);
    [_controller dismissViewControllerAnimated:YES completion:nil];
    fude_import_delegate = nil;
    fude_import_hand(FUDE_IMPORT_CAMERA, @[]);
}

@end

// --- import.h's -----------------------------------------------------------------------------

// The Photos library's picker is always there on iOS.
b8 fude_import_platform_photos(void) {
    return true;
}

// Whether the document camera can run: the device needs a camera.
b8 fude_import_platform_camera(void) {
    return VNDocumentCameraViewController.isSupported;   // False without a camera, and in the Simulator.
}

// Presents the Photos library's picker for any number of pictures. Does nothing while a picker or the camera is up.
void fude_import_platform_pick_photos(void) {
    UIViewController* _top = fude_import_top();
    if(_top == nil || fude_import_delegate != nil) {
        return;
    }
    PHPickerConfiguration* _config = [[PHPickerConfiguration alloc] init];
    _config.filter                 = [PHPickerFilter imagesFilter];
    _config.selectionLimit         = 0;   // No limit: as many as wanted, a page each.
    PHPickerViewController* _picker   = [[PHPickerViewController alloc] initWithConfiguration:_config];
    FudeImportPhotos*       _delegate = [[FudeImportPhotos alloc] init];
    _picker.delegate                  = _delegate;
    fude_import_delegate              = _delegate;
    [_top presentViewController:_picker animated:YES completion:nil];
}

// Presents the document camera. Does nothing while a picker or the camera is up, or where there is no camera.
void fude_import_platform_scan(void) {
    UIViewController* _top = fude_import_top();
    if(_top == nil || fude_import_delegate != nil || !VNDocumentCameraViewController.isSupported) {
        return;
    }
    VNDocumentCameraViewController* _camera   = [[VNDocumentCameraViewController alloc] init];
    FudeImportScan*                 _delegate = [[FudeImportScan alloc] init];
    _camera.delegate                          = _delegate;
    fude_import_delegate                      = _delegate;
    [_top presentViewController:_camera animated:YES completion:nil];
}

#endif
