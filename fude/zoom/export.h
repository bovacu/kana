// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_EXPORT
#define FUDE_ZOOM_EXPORT

#include "rde.h"
#include "drawing/base/kfile.h"
#include "zoom/scene.h"

// ===========================================================================
// What is on screen, out of the app (research §6, Phase 1's: the view):
//
//   PNG  the view drawn again off screen, larger (page.c: the renderer into a
//        render texture, read back the frame after and encoded by RDE).
//   SVG  the view as drawing, written here: strokes as paths (their width
//        followed in runs where the pen's pressure moves it), shapes as their
//        outlines, filled or not, fills as paths with what the eraser took
//        masked out, pictures as themselves (their own JPEG or PNG inside),
//        and the frames inside drawn in their places — the same frames the
//        renderer would draw, from three levels up down to half a point.
//
// Both are of the screen as the camera has it, _half each way (screen points),
// its paper's colour behind.
// ===========================================================================

// The SVG of the view into _out (appended). False: nothing could be written.
b8 fude_zoom_export_svg(const fude_zoom_scene* _s, fude_zoom_v2 _half, rde_color _paper, fude_bytes* _out);

#endif
