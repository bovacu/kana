#ifndef FUDE_TEXTSCAN
#define FUDE_TEXTSCAN

#include "rde.h"

// ===========================================================================
// Text from a photo — the platform's side: a photo picked from the library
// (the system's own picker, over the app), or frames of the live camera (RDE's
// camera: scan.h shows them), read by Google ML Kit Text Recognition with its
// Japanese model (in the app: no download). scan.h is the screen that uses it,
// the same on every platform.
//
// iOS: src/textscan_ios.m. Android: to come (ML Kit has the same recognizer
// there). Elsewhere (the desktop): not available.
//
// Like everything ML Kit, it follows the Settings switch (mlkit.h): off, it is
// never called (picking and reading refuse). Asynchronous: pick or read, then
// poll once a frame. A photo and a camera frame are read separately: one of
// each may be under way.
// ===========================================================================

#define FUDE_TEXTSCAN_LINES 256u    // lines kept from one photo, at most
#define FUDE_TEXTSCAN_TEXT  256u    // bytes of a line's text, at most

typedef enum {
    FUDE_TEXTSCAN_IDLE = 0,
    FUDE_TEXTSCAN_PICKING,      // the library is up
    FUDE_TEXTSCAN_READING,      // a photo was chosen: ML Kit is reading it
    FUDE_TEXTSCAN_DONE,         // read (fude_textscan_result): maybe no line at all
    FUDE_TEXTSCAN_CANCELLED,    // closed without a photo
    FUDE_TEXTSCAN_FAILED        // the photo could not be had, or read
} FUDE_TEXTSCAN_STATE_;

// A line of text found: what it says, and where — its four corners in the
// photo's pixels (top-left origin, y down), clockwise from its top-left, as
// the text runs (a line of vertical text stands on end).
RDE_STRUCT {
    c8         text[FUDE_TEXTSCAN_TEXT];
    rde_vec_2F corners[4];
    u32        block;           // lines of one block (a paragraph, a column) share it
} fude_textscan_line;

// The photo as read: encoded (JPEG) for the screen to show — upright, the same
// pixels the lines are measured in — and its lines in reading order. For a
// camera frame: no image; width and height the UPRIGHT frame's.
RDE_STRUCT {
    const u8*                 image;
    usize                     image_size;
    u32                       width;
    u32                       height;
    const fude_textscan_line* lines;
    u32                       line_count;
} fude_textscan_result;

// Can this platform read text in photos at all (ML Kit's recognizer is in it)?
b8                   fude_textscan_available(void);
// Opens the photo library over _window. False when it cannot (not available,
// ML Kit switched off, or a pick already under way).
b8                   fude_textscan_pick(rde_window* _window);
// The state now. DONE, CANCELLED and FAILED are said once, then it is IDLE again;
// at DONE, _out (may be NULL) gets the result — valid until the next pick.
FUDE_TEXTSCAN_STATE_ fude_textscan_poll(fude_textscan_result* _out);

// Starts reading a camera frame: _rgba, _width x _height pixels of 4 bytes, top
// row first, as the camera gave it — _rotation degrees clockwise to stand it
// upright (rde_device_camera_get_rotation). Copied: the caller's buffer is free
// at once. False when it cannot now: not available, ML Kit off, or the last
// frame still being read (then a newer one is offered later).
b8                   fude_textscan_read_frame(const u8* _rgba, u32 _width, u32 _height, f32 _rotation);
// True once a frame's reading is in: _out gets its lines, in the UPRIGHT
// frame's pixels — valid until the next frame read.
b8                   fude_textscan_poll_frame(fude_textscan_result* _out);

#endif
