// Apple's headers before RDE's: rde.h defines `any`, a word their availability
// pragmas use.
#import <TargetConditionals.h>
#if TARGET_OS_IOS
#import <UIKit/UIKit.h>
#import <PhotosUI/PhotosUI.h>
#import <VisionKit/VisionKit.h>
#endif

#include "drawing/doc/import.h"

// ===========================================================================
// See import.h. The iOS side: the Photos library's picker (PHPicker: many
// pictures at once, no permission asked) and the document camera (VisionKit:
// pages found, straightened, as many as are scanned). Each picture is written
// to the temporary folder and handed over in order on the main thread
// (fude_import_arrived); import.c deletes them once they are a PDF. The builder
// compiles Objective-C with ARC: the static below keeps the delegate up.
// ===========================================================================

#if defined(RDE_PLATFORM_IOS)

static id fude_import_delegate;   // the picker's or the camera's, while it is up

// A new file in the temporary folder, with extension _ext.
static NSString* fude_import_temp(NSString* _ext) {
    return [NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"import-%@.%@", [[NSUUID UUID] UUIDString], _ext]];
}

static void fude_import_hand(u8 _kind, NSArray<NSString*>* _paths) {
    const u32    _n = (u32)_paths.count;
    const c8**   _c = (const c8**)calloc(_n > 0u ? _n : 1u, sizeof(c8*));
    for(u32 _i = 0; _i < _n; _i++) {
        _c[_i] = [_paths[_i] fileSystemRepresentation];
    }
    fude_import_arrived(_kind, _c, _n);
    free(_c);
}

// The view controller on top, to present over.
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

@interface KanaImportPhotos : NSObject <PHPickerViewControllerDelegate>
@end

@implementation KanaImportPhotos

- (void)picker:(PHPickerViewController*)_picker didFinishPicking:(NSArray<PHPickerResult*>*)_results {
    [_picker dismissViewControllerAnimated:YES completion:nil];
    fude_import_delegate = nil;
    const NSUInteger _n = _results.count;
    if(_n == 0) {
        fude_import_hand(FUDE_IMPORT_PHOTOS, @[]);   // cancelled
        return;
    }
    // Each picture copied out as it comes (the file given goes once its block
    // returns), kept in the order picked; handed over when all are in.
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

@interface KanaImportScan : NSObject <VNDocumentCameraViewControllerDelegate>
@end

@implementation KanaImportScan

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

- (void)documentCameraViewControllerDidCancel:(VNDocumentCameraViewController*)_controller {
    [_controller dismissViewControllerAnimated:YES completion:nil];
    fude_import_delegate = nil;
    fude_import_hand(FUDE_IMPORT_CAMERA, @[]);
}

- (void)documentCameraViewController:(VNDocumentCameraViewController*)_controller didFailWithError:(NSError*)_error {
    rde_log_level(RDE_LOG_LEVEL_WARNING, "import: the document camera failed (%s)", _error.localizedDescription.UTF8String);
    [_controller dismissViewControllerAnimated:YES completion:nil];
    fude_import_delegate = nil;
    fude_import_hand(FUDE_IMPORT_CAMERA, @[]);
}

@end

// --- import.h's -----------------------------------------------------------------------------

b8 fude_import_platform_photos(void) {
    return true;
}

b8 fude_import_platform_camera(void) {
    return VNDocumentCameraViewController.isSupported;   // a camera, and not the Simulator
}

void fude_import_platform_pick_photos(void) {
    UIViewController* _top = fude_import_top();
    if(_top == nil || fude_import_delegate != nil) {
        return;
    }
    PHPickerConfiguration* _config = [[PHPickerConfiguration alloc] init];
    _config.filter                 = [PHPickerFilter imagesFilter];
    _config.selectionLimit         = 0;   // as many as wanted: a page each
    PHPickerViewController* _picker   = [[PHPickerViewController alloc] initWithConfiguration:_config];
    KanaImportPhotos*       _delegate = [[KanaImportPhotos alloc] init];
    _picker.delegate                  = _delegate;
    fude_import_delegate              = _delegate;
    [_top presentViewController:_picker animated:YES completion:nil];
}

void fude_import_platform_scan(void) {
    UIViewController* _top = fude_import_top();
    if(_top == nil || fude_import_delegate != nil || !VNDocumentCameraViewController.isSupported) {
        return;
    }
    VNDocumentCameraViewController* _camera   = [[VNDocumentCameraViewController alloc] init];
    KanaImportScan*                 _delegate = [[KanaImportScan alloc] init];
    _camera.delegate                          = _delegate;
    fude_import_delegate                      = _delegate;
    [_top presentViewController:_camera animated:YES completion:nil];
}

#endif
