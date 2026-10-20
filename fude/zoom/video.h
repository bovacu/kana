// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_VIDEO
#define FUDE_ZOOM_VIDEO

#include "rde.h"

// ===========================================================================
// Zoom video: writes a video file one frame at a time, as H.264 in an MP4, with the system's own encoder. It makes
// Export → Zoom video: the camera flown from the whole canvas down to the view, each frame drawn off screen
// and handed in here.
//
// Where it sits: zoom, the canvas apps' layer. The page (page_export.c) is its only user: it opens a video, adds one
// frame on each app frame, and closes it after the last one.
// Its main types and their lifecycle: fude_zoom_video_open makes a fude_zoom_video (opaque) and fude_zoom_video_close
// frees it, whether the file is kept or not.
// Platforms: on Apple's (video_apple.m) AVFoundation's AVAssetWriter encodes; on Android (video_android.c) MediaCodec
// and MediaMuxer do, through the Java class com.rde.fude.FudeVideo. Elsewhere (video.c) no video can be made.
// Threads: main thread only; nothing here starts a thread. Adding a frame and closing may block the calling thread
// while the system's encoder catches up: on Apple's, up to 2 seconds for each frame and up to 20 seconds to finish
// (past them the writer is cancelled, which waits for it to stop, so its file is deleted only once it is let go).
// Files: writes one MP4 (H.264 video, no sound) at the path it was opened with, replacing any file there. A video
// closed without keeping it, or that could not be finished, is deleted.
// Frames: RGBA, 4 bytes a pixel, the top row first (as the PNG export reads them back), as many pixels as the video
// was opened with. Each frame lasts 1/_fps of a second.
// Design: the video is not made in real time. A frame is handed in only when the encoder is ready for it, so a slow
// device makes the same video, only more slowly (docs/infinite_canvas_design.md §18).
// ===========================================================================

typedef struct fude_zoom_video fude_zoom_video;

// Returns whether a video can be made on this platform. False where there is no encoder (neither Apple's nor
// Android's), or on Android when the Java class is missing.
b8   fude_zoom_video_available(void);
// Starts a new video at _path, replacing any file there. _width and _height are in pixels; the page passes multiples
// of 16, which every encoder takes, and sizes under 16 or odd are refused. _fps is frames a second (0 is taken as 30
// on Apple's). Returns NULL when the video could not be begun.
fude_zoom_video* fude_zoom_video_open(const c8* _path, u32 _width, u32 _height, u32 _fps);
// Adds the next frame: _rgba holds the video's width × height pixels, _stride bytes from one row to the next. Returns
// false when the frame could not be taken; the video must still be closed.
b8   fude_zoom_video_add(fude_zoom_video* _v, const u8* _rgba, u32 _stride);
// Finishes the video and writes it out, or, with _keep false, drops it and deletes its file. Frees _v in every case.
// Returns false when no good file came of it (always false when _keep is false).
b8   fude_zoom_video_close(fude_zoom_video* _v, b8 _keep);

// Converts RGBA pixels to planar YUV 4:2:0 (I420: the Y plane, then U, then V, each plane's rows packed; BT.601 video
// range), which is what Android's encoder is handed. _width and _height must be even; _stride is the bytes from one
// RGBA row to the next. _out must hold _width × _height × 3 / 2 bytes. Safe on any thread: it only reads and writes
// the buffers it is given.
void fude_zoom_video_i420(const u8* _rgba, u32 _width, u32 _height, u32 _stride, u8* _out);

#endif
