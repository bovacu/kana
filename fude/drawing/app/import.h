// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_DOC_IMPORT
#define FUDE_DOC_IMPORT

#include "rde.h"

// ===========================================================================
// Import: documents brought in, each onto a canvas of its own (doc.h): a PDF from Files, pictures from Files or the
// Photos library, or pages scanned with the camera. Pictures and scans are made into a PDF first, a page each
// (pdf.h), so everything reads, zooms and is written on the same way. An app with its own page (extension.h:
// page_kind's imported, Sketching) takes what came in instead.
//
// Files come through the system's file dialog (RDE's; on a device, the Files picker). The Photos library and the
// camera are iOS's own (import_ios.m): the photo picker needs no permission, since only what is picked reaches the
// app; the document camera (VisionKit) finds the page's edges and straightens it, and asks for the camera the first
// time. Android's are the system's pickers and Google's document scanner (import_android.c).
//
// Where it sits: drawing/app. The Library's row starts an import, and the page (page.c) and the Library run
// fude_import_update each frame.
//
// Threads: what comes back arrives on the main thread (the pickers hand it over there, and Android's is polled each
// update), and becomes a canvas on the next fude_import_update.
//
// Files: the copies the pickers wrote (photos, scans) are deleted once used; Files' own are the learner's and are left.
// Pictures are made into a PDF in the save folder, which is copied in as the canvas's own and then deleted. The new
// canvas is named after the file, or "Scan" or "Photos" and the date.
// ===========================================================================

struct fude_app;

// Whether this platform can bring pictures in from the Photos library, or scan pages with the camera.
b8   fude_import_photos_available(void);
b8   fude_import_camera_available(void);

// Starts an import from Files, the Photos library or the camera. A platform that cannot show PDFs says so in a
// notice; one without photos or a camera does nothing.
void fude_import_files(struct fude_app* _app);
void fude_import_photos(struct fude_app* _app);
void fude_import_camera(struct fude_app* _app);

// For a picked file: whether it is a PDF (by its extension, in any case), and its name as the user knows it, written
// into _out without its folder, its extension, or the copy's number Android's picker puts before it ("3_notes.pdf"
// becomes "notes").
b8   fude_import_is_pdf(const c8* _path);
void fude_import_name(const c8* _path, c8* _out, usize _size);

// Called once a frame: makes what came in into a canvas and opens it, or says why not in a notice. Returns true the
// frame a canvas is made, so the caller closes the screens over the page; false otherwise, including when the app's
// own page took it.
b8   fude_import_update(struct fude_app* _app);

// Where an import came from.
typedef enum { FUDE_IMPORT_FILES = 0, FUDE_IMPORT_PHOTOS, FUDE_IMPORT_CAMERA } FUDE_IMPORT_;
// What the pickers hand back (import.c's, for the platform files): the files, in order, from _kind (FUDE_IMPORT_);
// none (NULL, 0) when cancelled. The paths are copied; call it on the main thread.
void fude_import_arrived(u8 _kind, const c8* const* _paths, u32 _count);

// The platform's side (import_ios.m on iOS, import_android.c on Android; elsewhere none, and the first two are false).
b8   fude_import_platform_photos(void);
b8   fude_import_platform_camera(void);
void fude_import_platform_pick_photos(void);
void fude_import_platform_scan(void);
// Android's only (import_android.c): Files through the system's picker, and a poll for what came in, run by each
// fude_import_update.
void fude_import_platform_pick_files(void);
void fude_import_platform_poll(void);

#endif
