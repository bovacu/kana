// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_INKTEST_H
#define FUDE_ZOOM_INKTEST_H

#include "rde.h"
#include "zoom/scene.h"
#include "drawing/ink/notes.h"

// ===========================================================================
// Test ink: a canvas filled with pen-like strokes over four depths, to see how smoothly a device draws a heavy canvas.
// It is the ink the canvas's speed is measured on (the developer's --gate, docs/infinite_canvas_design.md §10), here
// so that every build has it: Settings' Display makes a canvas of it (page_settings.c), so a person can feel on their
// own device what the measurements were taken on.
//
// Where it sits: fude/zoom, on the scene (scene.h) and the notes (drawing/ink/notes.h). The page's Settings row and
// the look harness's --gate (look.c) fill a canvas with it; the harness's other demos (--demo-deep, --stress) draw its
// rose curves in its colours.
// Its state: none. Everything is drawn into the scene it is handed, with the camera that scene has.
// Threads: main thread, as the scene and the notes.
// Files: none of its own; what it draws is saved as any drawing is (zfile.h), and the canvas it makes is a note
// (notes.h) like any other.
// Design: the ink is the same on every run and every device. Its numbers come from a fixed seed through a linear
// congruential generator, not RDE's random or the clock, because the gate's numbers and screenshots are compared
// across builds and must be measured on the same strokes every time (the zoom suite checks it against the gate as it
// was when it lived in look.c). Each stroke is a rose curve of FUDE_ZOOM_INKTEST_STROKE points with pressure and time,
// as a pen gives them, so it costs what a person's writing costs to keep, draw and save.
// ===========================================================================

// How many points each stroke of the test ink has: a long pen stroke's worth.
#define FUDE_ZOOM_INKTEST_STROKE 400u
// How many points Settings' test canvas holds: a million, as the developer's --gate=1000000 and the gate's target
// (docs/infinite_canvas_design.md §10), and as Settings' row says.
#define FUDE_ZOOM_INKTEST_POINTS 1000000u
// How many colours the ink and the look harness's demos draw in.
#define FUDE_ZOOM_INKTEST_COLOR_COUNT 6u

// The ink's colours: red, yellow, green, blue, purple, orange. Read-only, any thread.
extern const rde_color FUDE_ZOOM_INKTEST_COLORS[FUDE_ZOOM_INKTEST_COLOR_COUNT];

// Whether this build offers the test canvas in Settings' Display: a developer's build (RDE_DEBUG), or one built to
// be measured (FUDE_ALLOW_ARGS: the Mac's `measure`, the tablet's `android-measure`). A release that ships has none
// (the owner's call, 2026-10-15: the test is a developer's, not a person's). The ink itself is here in every build:
// the look harness's --gate and the zoom suite draw it.
#if defined(RDE_DEBUG) || defined(FUDE_ALLOW_ARGS)
#define FUDE_ZOOM_INKTEST_OFFERED 1
#else
#define FUDE_ZOOM_INKTEST_OFFERED 0
#endif

// Draws a rose curve of _petals petals in the camera's frame, round _c, its petals' tips _r from it (frame units) and
// turned by _phase radians, _width points wide on the screen at the camera's zoom, in _color: one stroke of
// FUDE_ZOOM_INKTEST_STROKE points with pressure and time, put in the undo history as a step of its own.
void fude_zoom_inktest_rose(fude_zoom_scene* _s, fude_zoom_v2 _c, f64 _r, f64 _phase, u32 _petals, rde_color _color, f64 _width);

// Fills the scene with _points points of test ink (each depth's share made up to whole strokes): strokes round the
// camera's point at its depth and three more, each 1024× inside the last, then the camera back out where it began.
// Each stroke is an undo step. It runs whole, in one call, as the camera goes down through the depths and back with
// it (nothing else may move the camera meanwhile): a million points take 66 ms on an M2 Mac in a release build. _half
// is half the screen's size, for the camera's steps (fude_zoom_camera_zoom_by).
void fude_zoom_inktest_fill(fude_zoom_scene* _s, u64 _points, fude_zoom_v2 _half);

// Makes a new canvas named _name at the top of the person's canvases and opens it (the session switches to it at the
// end of the frame). It never reuses a canvas, even one of the same name: another canvas, and everything drawn on it,
// is left as it was. Returns the new canvas's id (never 0). The notes' index is saved.
u32 fude_zoom_inktest_canvas(fude_notes* _notes, const c8* _name);

#endif
