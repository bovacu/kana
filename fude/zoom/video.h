// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_VIDEO
#define FUDE_ZOOM_VIDEO

#include "rde.h"

// ===========================================================================
// A video file made frame by frame (Sketching's zoom video: the camera flown
// from the whole canvas down to the view, each frame drawn off screen and
// handed in here): H.264 in an MP4, by the system's own encoder — AVFoundation's
// AVAssetWriter on Apple's (video_apple.m), MediaCodec and MediaMuxer on Android
// (video_android.c, com.rde.fude.FudeVideo). Elsewhere: none (video.c).
//
// Frames come as RGBA, the rows top first (as the PNG export reads them back),
// _width × _height as opened (both a multiple of 16: every encoder takes that);
// each lasts 1/_fps of a second. Made on the engine's thread, one at a time.
// ===========================================================================

typedef struct fude_zoom_video fude_zoom_video;

// Can one be made here?
b8   fude_zoom_video_available(void);
// A new one at _path (any file there replaced). NULL: it could not be begun.
fude_zoom_video* fude_zoom_video_open(const c8* _path, u32 _width, u32 _height, u32 _fps);
// The next frame (_stride bytes a row). False: it could not be taken (the video is
// still to be closed).
b8   fude_zoom_video_add(fude_zoom_video* _v, const u8* _rgba, u32 _stride);
// Finished and written (or, _keep false, let go of). False: no good file came of it.
b8   fude_zoom_video_close(fude_zoom_video* _v, b8 _keep);

// RGBA into YUV 4:2:0, planar (I420: Y, then U, then V, each plane's rows packed;
// BT.601, video range) — what Android's encoder is handed. _out: _width × _height
// × 3 / 2 bytes (_width and _height even).
void fude_zoom_video_i420(const u8* _rgba, u32 _width, u32 _height, u32 _stride, u8* _out);

#endif
