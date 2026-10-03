#ifndef FUDE_DOC_IMPORT
#define FUDE_DOC_IMPORT

#include "rde.h"

// ===========================================================================
// Documents brought in, each to a canvas of its own (doc.h): a PDF from Files;
// pictures from Files or the Photos library; pages scanned with the camera —
// pictures and scans made into a PDF first, a page each (pdf.h), so everything
// reads, zooms and is written on the same way.
//
// Files: the system's file dialog (RDE's; on a device, the Files picker). The
// Photos library and the camera: iOS's own (src: import_ios.m) — the photo
// picker needs no permission (only what is picked reaches the app); the
// document camera (VisionKit) finds the page's edges and straightens it, and
// asks for the camera the first time.
//
// What comes back comes on the main thread, and becomes a canvas on the next
// fude_import_update. Its name: the file's; or "Scan" / "Photos" and the date.
// ===========================================================================

struct fude_app;

// Where they can come from, here.
b8   fude_import_photos_available(void);
b8   fude_import_camera_available(void);

void fude_import_files(struct fude_app* _app);
void fude_import_photos(struct fude_app* _app);
void fude_import_camera(struct fude_app* _app);

// Once a frame: what came in made into a canvas (opened), or why not (a notice).
// True the frame one is made (the screens over the page close: it is there).
b8   fude_import_update(struct fude_app* _app);

// What the pickers hand back (import.c's, for import_ios.m): picture files
// (_kind: FUDE_IMPORT_PHOTOS or _CAMERA), in order; none (NULL, 0) when cancelled.
typedef enum { FUDE_IMPORT_FILES = 0, FUDE_IMPORT_PHOTOS, FUDE_IMPORT_CAMERA } FUDE_IMPORT_;
void fude_import_arrived(u8 _kind, const c8* const* _paths, u32 _count);

// The platform's (import_ios.m on iOS; nothing elsewhere).
b8   fude_import_platform_photos(void);
b8   fude_import_platform_camera(void);
void fude_import_platform_pick_photos(void);
void fude_import_platform_scan(void);
// Android's too (import_android.c): Files through the system's picker, and what
// came in, looked for (each fude_import_update).
void fude_import_platform_pick_files(void);
void fude_import_platform_poll(void);

#endif
