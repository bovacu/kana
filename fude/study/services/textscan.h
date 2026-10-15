// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_TEXTSCAN
#define FUDE_TEXTSCAN

#include "rde.h"

// ===========================================================================
// Text scan: the platform's side of Text from a photo. It reads the lines of text in a photo picked from the
// library (the system's own picker, shown over the app), in frames of the live camera, and in a document's scanned
// pages, with Google ML Kit Text Recognition and the language's model (built into the app: nothing to download).
//
// Where it sits: the study layer's services (study/services). The Text from a photo screen (scan.h, which shows
// RDE's camera frames and is the same on every platform) reads photos and frames; the study's picture hooks
// (study.h: fude_study_picture_read) read a document's pages for the drawing core (doc.h).
// Its main types and their lifecycle: fude_textscan_result and fude_textscan_line are views of memory the platform
// file keeps (each channel's lines in an rde_arr on the std allocator, as many as were read, kept for the run); each
// result stays valid until the next read on its channel (see each call). The platform files are
// textscan_ios.m on an iOS device, textscan_android.c on Android (Java's com.rde.fude.FudeText), and stand-ins in
// textscan.c everywhere else, where nothing is available (the desktop, the iOS Simulator).
// Threads: call everything from the app's main thread, then poll once a frame: everything is asynchronous. On iOS the
// picked photo is handed back to the main thread before it is read, frames and pages are turned upright and handed
// to ML Kit on serial queues of their own, and ML Kit answers on the main queue. On Android, ML Kit answers FudeText
// on Android's own UI thread, and FudeText keeps the answers behind a lock (the photo's state in a volatile field),
// from where the polls take them through JNI.
// Files: none. The photo picked never leaves the device; on iOS the picker needs no permission, since only the photo
// picked reaches the app.
// Design: like everything ML Kit, it follows the Settings switch (mlkit.h): when it is off, it is never called, and
// picking and reading refuse. A photo, a camera frame and a document's page are read on separate channels, so one
// of each may be under way at once, and reading a page never keeps the camera's frames waiting. Whatever is read is
// first redrawn upright, one point a pixel, so that what ML Kit measures and what the screen shows are the same
// pixels, the same way up.
// ===========================================================================

#define FUDE_TEXTSCAN_TEXT  256u    // bytes of a line's text, at most

typedef enum {
    FUDE_TEXTSCAN_IDLE = 0,
    FUDE_TEXTSCAN_PICKING,      // the library is up
    FUDE_TEXTSCAN_READING,      // a photo was chosen: ML Kit is reading it
    FUDE_TEXTSCAN_DONE,         // read (fude_textscan_result), perhaps with no line at all
    FUDE_TEXTSCAN_CANCELLED,    // closed without a photo
    FUDE_TEXTSCAN_FAILED        // the photo could not be had, or read
} FUDE_TEXTSCAN_STATE_;

// A line of text found: what it says (UTF-8), and where. Its four corners are in the photo's pixels (top-left
// origin, Y down), clockwise from its top-left as the text runs, so a line of vertical text stands on end.
RDE_STRUCT {
    c8         text[FUDE_TEXTSCAN_TEXT];
    rde_vec_2F corners[4];
    u32        block;           // the lines of one block (a paragraph, a column) share it
} fude_textscan_line;

// The photo as read: its image encoded as JPEG for the screen to show (upright, the same pixels the lines are
// measured in), and its lines in reading order, every one read. For a camera frame or a page there is no image (NULL,
// 0), and the width and height are the UPRIGHT frame's.
RDE_STRUCT {
    const u8*                 image;
    usize                     image_size;
    u32                       width;
    u32                       height;
    const fude_textscan_line* lines;
    u32                       line_count;
} fude_textscan_result;

// Returns true when this platform can read text in pictures at all: ML Kit's recognizer is in the build and the
// language taught has a model (lang.h: fude_lang_text_readable; Thai and Arabic have none).
b8                   fude_textscan_available(void);
// Opens the photo library over _window. Returns false when it cannot: not available, ML Kit switched off, or a pick
// already under way.
b8                   fude_textscan_pick(rde_window* _window);
// Returns the state of the photo picked. DONE, CANCELLED and FAILED are reported once, and then it is IDLE again. At
// DONE, _out (which may be NULL) gets the result, which stays valid until the next pick.
FUDE_TEXTSCAN_STATE_ fude_textscan_poll(fude_textscan_result* _out);

// Starts reading a camera frame: _rgba is _width x _height pixels of 4 bytes, top row first, as the camera gave it,
// and _rotation is how many degrees clockwise stand it upright (rde_device_camera_get_rotation). The pixels are
// copied, so the caller's buffer is free at once. Returns false when it cannot now: not available, ML Kit off, or
// the last frame still being read (a newer one is then offered later).
b8                   fude_textscan_read_frame(const u8* _rgba, u32 _width, u32 _height, f32 _rotation);
// Returns true once a frame's reading is in: _out gets its lines, in the UPRIGHT frame's pixels, valid until the
// next frame is read.
b8                   fude_textscan_poll_frame(fude_textscan_result* _out);

// Starts reading a document's page (a scan: doc.h) the same way, on a channel of its own that the camera never
// waits for: _rgba is _width x _height pixels of 4 bytes, the top row first, already upright. The pixels are copied.
// Returns false when it cannot now: not available, ML Kit off, or a page still being read.
b8                   fude_textscan_read_page(const u8* _rgba, u32 _width, u32 _height);
// Returns true once the page's reading is in: _out gets its lines, in its pixels, valid until the next page is read.
b8                   fude_textscan_poll_page(fude_textscan_result* _out);

#endif
