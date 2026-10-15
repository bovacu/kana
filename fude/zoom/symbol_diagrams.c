// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/symbol_parts.h"

// ===========================================================================
// See symbol.h. Symbols, the diagrams' draw functions, family by family: flowchart and basic shapes, UML structure, UML
// behaviour, data and ER, architecture (C4, servers and services, devices, network), BPMN, planning, and more (a
// sequence diagram's heads, a terminal, a key, an envelope, code, a team, a brace, a DFD's store, a topic).
//
// Each symbol is drawn by a function of its half sizes (hw, hh), fzs_<what>: its parts, in its own units round its
// middle (Y up). Most are drawn in the box's own proportions (u, v from -1 to 1, as the box is: fude_zoom_sym_uv); an
// icon's round bits are round whatever its box (a radius of the smaller half, FZS_R). The first part a function makes
// is the symbol's outline. Each is in the table FUDE_ZOOM_SYMBOLS (symbol.c), and drawn with symbol.c's pen.
//
// Part of the symbols: symbol_parts.h holds what the modules share and says which module holds what. Internal names:
// the pen's fude_zoom_sym_, the draw functions' fzs_ and FZS_. Main thread only, as symbol.h says.
// ===========================================================================

// Flowchart.
// Process: a rectangle.
void fzs_process(fude_zoom_sym* _s)    { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT); }
// Alternate process: a rectangle with rounded corners.
void fzs_rounded(fude_zoom_sym* _s)    { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.35, FZS_OUT); }
// Decision (and the diamond): a diamond touching the box's sides.
void fzs_decision(fude_zoom_sym* _s)   { const f64 _d[8] = { 0, -1, 1, 0, 0, 1, -1, 0 }; fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT); }
// Terminator: a stadium, its ends half circles.
void fzs_terminator(fude_zoom_sym* _s) { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, _s->hh, FZS_OUT); }
// Data: a parallelogram leaning right.
void fzs_data(fude_zoom_sym* _s)       { const f64 _d[8] = { -1, -1, 0.75, -1, 1, 1, -0.75, 1 }; fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT); }
// Predefined process: a rectangle with a line down each side, just in from its edge.
void fzs_predefined(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    fude_zoom_sym_line(_s, -0.82, -1, -0.82, 1, 0u);
    fude_zoom_sym_line(_s, 0.82, -1, 0.82, 1, 0u);
}
// Document: a rectangle whose bottom edge is a wave.
void fzs_document(fude_zoom_sym* _s) {
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_uv(_s, -1, 1);
    fude_zoom_sym_wave_pts(_s, -_s->hw, _s->hw, -_s->hh * 0.82, -_s->hh * 0.16, 2u);
    fude_zoom_sym_uv(_s, 1, 1);
    fude_zoom_sym_end(_s, FZS_OUT);
}
// Documents: a document with two more stacked behind it.
void fzs_documents(fude_zoom_sym* _s) {
    // The front one (its outline: what fills and connects), the two behind it showing above and right of it.
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_uv(_s, -1, 0.76);
    fude_zoom_sym_wave_pts(_s, -_s->hw, 0.76 * _s->hw, -_s->hh * 0.84, -_s->hh * 0.14, 2u);
    fude_zoom_sym_uv(_s, 0.76, 0.76);
    fude_zoom_sym_end(_s, FZS_OUT);
    for(u32 _k = 1; _k <= 2u; _k++) {
        const f64 _o = 0.12 * (f64)_k;
        const f64 _l[6] = { -1.0 + _o, 0.76 + _o, 0.76 + _o, 0.76 + _o, 0.76 + _o, -0.7 + _o };
        fude_zoom_sym_poly(_s, _l, 3u, 0u);
    }
}

// Manual input: a box whose top slopes down to the left.
void fzs_manual_input(fude_zoom_sym* _s) { const f64 _d[8] = { -1, -1, 1, -1, 1, 1, -1, 0.45 }; fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT); }
// Manual operation: a trapezoid, narrower at the bottom.
void fzs_manual_op(fude_zoom_sym* _s)    { const f64 _d[8] = { -0.75, -1, 0.75, -1, 1, 1, -1, 1 }; fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT); }
// Preparation: a long hexagon.
void fzs_preparation(fude_zoom_sym* _s)  { const f64 _d[12] = { -1, 0, -0.72, -1, 0.72, -1, 1, 0, 0.72, 1, -0.72, 1 }; fude_zoom_sym_poly(_s, _d, 6u, FZS_OUT); }
// Delay: a D, flat on the left and round on the right.
void fzs_delay(fude_zoom_sym* _s) {
    fude_zoom_sym_begin(_s);
    const f64 _r = _s->hh;
    fude_zoom_sym_uv(_s, -1, -1);
    fude_zoom_sym_arc_pts(_s, _s->hw - _r, 0.0, _r, _r, -RDE_PI * 0.5, RDE_PI * 0.5);
    fude_zoom_sym_uv(_s, -1, 1);
    fude_zoom_sym_end(_s, FZS_OUT);
}
// Stored data: its left side bowed out and its right side bowed in.
void fzs_stored_data(fude_zoom_sym* _s) {
    // Curved both sides: its left a half round out, its right a half round in.
    const f64 _rx = _s->hw * 0.18;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, -_s->hw + _rx, 0.0, _rx, _s->hh, RDE_PI * 0.5, RDE_PI * 1.5);
    fude_zoom_sym_arc_pts(_s, _s->hw, 0.0, _rx, _s->hh, RDE_PI * 1.5, RDE_PI * 0.5);
    fude_zoom_sym_end(_s, FZS_OUT);
}
// Internal storage: a rectangle with a line down near its left and one across near its top.
void fzs_internal_storage(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    fude_zoom_sym_line(_s, -0.78, -1, -0.78, 1, 0u);
    fude_zoom_sym_line(_s, -1, 0.7, 1, 0.7, 0u);
}
// Display: pointed on the left, round on the right.
void fzs_display(fude_zoom_sym* _s) {
    fude_zoom_sym_begin(_s);
    const f64 _r = _s->hh;
    fude_zoom_sym_uv(_s, -1, 0);
    fude_zoom_sym_uv(_s, -0.7, -1);
    fude_zoom_sym_arc_pts(_s, _s->hw - _r * 0.6, 0.0, _r * 0.6, _r, -RDE_PI * 0.5, RDE_PI * 0.5);
    fude_zoom_sym_uv(_s, -0.7, 1);
    fude_zoom_sym_end(_s, FZS_OUT);
}
// Off-page connector: a pentagon pointing down.
void fzs_offpage(fude_zoom_sym* _s)   { const f64 _d[10] = { -1, 1, 1, 1, 1, -0.3, 0, -1, -1, -0.3 }; fude_zoom_sym_poly(_s, _d, 5u, FZS_OUT); }
// Connector: an ellipse filling the box.
void fzs_connector(fude_zoom_sym* _s) { fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT); }
// Merge: a triangle pointing down.
void fzs_merge(fude_zoom_sym* _s)     { const f64 _d[6] = { -1, 1, 1, 1, 0, -1 }; fude_zoom_sym_poly(_s, _d, 3u, FZS_OUT); }
// Extract: a triangle pointing up.
void fzs_extract(fude_zoom_sym* _s)   { const f64 _d[6] = { -1, -1, 1, -1, 0, 1 }; fude_zoom_sym_poly(_s, _d, 3u, FZS_OUT); }
// Or: an ellipse with a + across it.
void fzs_or(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT);
    fude_zoom_sym_line(_s, -1, 0, 1, 0, 0u);
    fude_zoom_sym_line(_s, 0, -1, 0, 1, 0u);
}
// Summing junction: an ellipse with an × across it.
void fzs_summing(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT);
    fude_zoom_sym_line(_s, -0.7071, -0.7071, 0.7071, 0.7071, 0u);
    fude_zoom_sym_line(_s, -0.7071, 0.7071, 0.7071, -0.7071, 0u);
}
// Collate: two triangles point to point, an hourglass.
void fzs_collate(fude_zoom_sym* _s)   { const f64 _d[8] = { -1, 1, 1, 1, -1, -1, 1, -1 }; fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT); }
// Sort: a diamond with a line across its middle.
void fzs_sort(fude_zoom_sym* _s) {
    const f64 _d[8] = { 0, -1, 1, 0, 0, 1, -1, 0 };
    fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT);
    fude_zoom_sym_line(_s, -1, 0, 1, 0, 0u);
}
// Annotation: an open bracket down its left side.
void fzs_annotation(fude_zoom_sym* _s) { const f64 _d[8] = { -0.8, 1, -1, 1, -1, -1, -0.8, -1 }; fude_zoom_sym_poly(_s, _d, 4u, 0u); }
// Card: a rectangle with its top left corner cut off.
void fzs_card(fude_zoom_sym* _s)       { const f64 _d[10] = { -1, 0.55, -0.7, 1, 1, 1, 1, -1, -1, -1 }; fude_zoom_sym_poly(_s, _d, 5u, FZS_OUT); }
// Punched tape: a band whose top and bottom are waves.
void fzs_tape(fude_zoom_sym* _s) {
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_wave_pts(_s, -_s->hw, _s->hw, -_s->hh * 0.85, -_s->hh * 0.15, 2u);
    fude_zoom_sym_wave_pts(_s, _s->hw, -_s->hw, _s->hh * 0.85, -_s->hh * 0.15, 2u);
    fude_zoom_sym_end(_s, FZS_OUT);
}
// Loop limit: a rectangle with its top corners cut off.
void fzs_loop_limit(fude_zoom_sym* _s) { const f64 _d[12] = { -1, -1, 1, -1, 1, 0.5, 0.75, 1, -0.75, 1, -1, 0.5 }; fude_zoom_sym_poly(_s, _d, 6u, FZS_OUT); }
// Triangle (and the isosceles triangle): pointing up.
void fzs_triangle(fude_zoom_sym* _s)   { const f64 _d[6] = { -1, -1, 1, -1, 0, 1 }; fude_zoom_sym_poly(_s, _d, 3u, FZS_OUT); }
// Hexagon (and the regular hexagon): a corner at each side's middle.
void fzs_hexagon(fude_zoom_sym* _s)    { fude_zoom_sym_regular(_s, 6u, 0.0, FZS_OUT); }
// Ellipse: filling the box.
void fzs_ellipse(fude_zoom_sym* _s)    { fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT); }
// Callout: a box with a pointer down from its bottom edge.
void fzs_callout(fude_zoom_sym* _s) {
    const f64 _d[14] = { -1, -0.45, -1, 1, 1, 1, 1, -0.45, -0.2, -0.45, -0.55, -1, -0.5, -0.45 };
    fude_zoom_sym_poly(_s, _d, 7u, FZS_OUT);
}

// UML structure.
// Class (and the object and the enumeration): a rectangle.
void fzs_class(fude_zoom_sym* _s) { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT); }   // (its compartments' lines: from its text, page/render)
// Interface (provided): a circle on a stalk.
void fzs_interface(fude_zoom_sym* _s) {
    // A lollipop: a circle on a stalk (its name under it).
    const f64 _r = FZS_R(_s) * 0.5;
    fude_zoom_sym_ellipse(_s, 0, _s->hh - _r, _r, _r, FZS_OUT);
    fude_zoom_sym_line(_s, 0, (_s->hh - 2.0 * _r) / _s->hh, 0, -1, 0u);
}
// Package: a folder, its tab on its top left.
void fzs_package(fude_zoom_sym* _s) {
    const f64 _d[14] = { -1, -1, 1, -1, 1, 0.7, -0.35, 0.7, -0.35, 1, -1, 1, -1, 0.7 };
    fude_zoom_sym_poly(_s, _d, 6u, FZS_OUT);
    fude_zoom_sym_line(_s, -1, 0.7, -0.35, 0.7, 0u);
}
// Note: a sheet with its top right corner folded down.
void fzs_note(fude_zoom_sym* _s) {
    const f64 _f = FZS_R(_s) * 0.3;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw, -_s->hh);
    fude_zoom_sym_pt(_s, _s->hw, -_s->hh);
    fude_zoom_sym_pt(_s, _s->hw, _s->hh - _f);
    fude_zoom_sym_pt(_s, _s->hw - _f, _s->hh);
    fude_zoom_sym_pt(_s, -_s->hw, _s->hh);
    fude_zoom_sym_end(_s, FZS_OUT);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _s->hw - _f, _s->hh);
    fude_zoom_sym_pt(_s, _s->hw - _f, _s->hh - _f);
    fude_zoom_sym_pt(_s, _s->hw, _s->hh - _f);
    fude_zoom_sym_end(_s, 0u);
}
// Component: a rectangle with UML's component mark in its top right corner.
void fzs_component(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    // Its mark, top right: a box with two tabs on its left.
    const f64 _r = FZS_R(_s) * 0.18, _x = _s->hw - _r * 2.2, _y = _s->hh - _r * 2.0;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _x - _r, _y - _r * 1.2); fude_zoom_sym_pt(_s, _x + _r, _y - _r * 1.2);
    fude_zoom_sym_pt(_s, _x + _r, _y + _r * 1.2); fude_zoom_sym_pt(_s, _x - _r, _y + _r * 1.2);
    fude_zoom_sym_end(_s, FZS_LOOP);
    for(i32 _k = -1; _k <= 1; _k += 2) {
        fude_zoom_sym_begin(_s);
        const f64 _ty = _y + (f64)_k * _r * 0.55;
        fude_zoom_sym_pt(_s, _x - _r * 1.5, _ty - _r * 0.28); fude_zoom_sym_pt(_s, _x - _r * 0.5, _ty - _r * 0.28);
        fude_zoom_sym_pt(_s, _x - _r * 0.5, _ty + _r * 0.28); fude_zoom_sym_pt(_s, _x - _r * 1.5, _ty + _r * 0.28);
        fude_zoom_sym_end(_s, FZS_LOOP | FUDE_ZOOM_SYMBOL_FILLED);
    }
}
// Node: a box drawn in three dimensions, its top and right side showing.
void fzs_node(fude_zoom_sym* _s) {
    // A box seen from a corner: its front, its top, its side.
    const f64 _d = FZS_R(_s) * 0.25;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw, -_s->hh); fude_zoom_sym_pt(_s, _s->hw - _d, -_s->hh); fude_zoom_sym_pt(_s, _s->hw, -_s->hh + _d);
    fude_zoom_sym_pt(_s, _s->hw, _s->hh); fude_zoom_sym_pt(_s, -_s->hw + _d, _s->hh); fude_zoom_sym_pt(_s, -_s->hw, _s->hh - _d);
    fude_zoom_sym_end(_s, FZS_OUT);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw, _s->hh - _d); fude_zoom_sym_pt(_s, _s->hw - _d, _s->hh - _d); fude_zoom_sym_pt(_s, _s->hw, _s->hh);
    fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _s->hw - _d, _s->hh - _d); fude_zoom_sym_pt(_s, _s->hw - _d, -_s->hh);
    fude_zoom_sym_end(_s, 0u);
}
// Artifact: a rectangle with a small dog-eared sheet in its top right corner.
void fzs_artifact(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    const f64 _r = FZS_R(_s) * 0.16, _x = _s->hw - _r * 2.0, _y = _s->hh - _r * 2.4, _f = _r * 0.6;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _x - _r, _y - _r * 1.3); fude_zoom_sym_pt(_s, _x + _r, _y - _r * 1.3); fude_zoom_sym_pt(_s, _x + _r, _y + _r * 1.3 - _f);
    fude_zoom_sym_pt(_s, _x + _r - _f, _y + _r * 1.3); fude_zoom_sym_pt(_s, _x - _r, _y + _r * 1.3);
    fude_zoom_sym_end(_s, FZS_LOOP);
}
// Actor: a stick figure.
void fzs_actor(fude_zoom_sym* _s) {
    // A stick figure filling its box, its name under it.
    const f64 _h = _s->hh, _w = _s->hw, _head = rde_math_min_f64(_w * 0.42, _h * 0.2);
    fude_zoom_sym_ellipse(_s, 0, _h - _head, _head, _head, FZS_LOOP);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, 0, _h - 2.0 * _head); fude_zoom_sym_pt(_s, 0, -_h * 0.25); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_w, _h * 0.32); fude_zoom_sym_pt(_s, _w, _h * 0.32); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_w * 0.85, -_h); fude_zoom_sym_pt(_s, 0, -_h * 0.25); fude_zoom_sym_pt(_s, _w * 0.85, -_h); fude_zoom_sym_end(_s, 0u);
}
// Use case: an ellipse.
void fzs_use_case(fude_zoom_sym* _s) { fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT); }
// System boundary: a rectangle.
void fzs_boundary(fude_zoom_sym* _s) { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT); }
// Required interface: a socket, an arc over the top of a stalk, open below.
void fzs_socket(fude_zoom_sym* _s) {
    // A required interface: a half circle open to the right, on a stalk.
    const f64 _r = FZS_R(_s) * 0.5;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0, _s->hh - _r, _r, _r, RDE_PI * 0.15, RDE_PI * 0.85);
    fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_line(_s, 0, (_s->hh - _r) / _s->hh, 0, -1, 0u);
}

// UML behaviour.
// Lifeline: a head box at its top (at most 30 units tall) and a dashed line down from it.
void fzs_lifeline(fude_zoom_sym* _s) {
    const f64 _top = 1.0 - 2.0 * rde_math_min_f64(0.18, 30.0 / rde_math_max_f64(_s->hh, 1e-9));
    fude_zoom_sym_rect(_s, -1, _top, 1, 1, FZS_OUT);
    fude_zoom_sym_line(_s, 0, _top, 0, -1, FUDE_ZOOM_SYMBOL_DASHED);
}
// Activation: a narrow rectangle.
void fzs_activation(fude_zoom_sym* _s) { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT); }
// Destroy: an × filling the box.
void fzs_destroy(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -1, -1, 1, 1, 0u);
    fude_zoom_sym_line(_s, -1, 1, 1, -1, 0u);
}
// Fragment: a rectangle with its operator's tag in the top left corner.
void fzs_fragment(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    // Its tag, top left: a pentagon round its operator (alt, loop, opt — its text's first line).
    const f64 _tw = rde_math_min_f64(_s->hw * 0.5, 70.0), _th = rde_math_min_f64(_s->hh * 0.25, 24.0);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw, _s->hh - _th);
    fude_zoom_sym_pt(_s, -_s->hw + _tw - _th * 0.5, _s->hh - _th);
    fude_zoom_sym_pt(_s, -_s->hw + _tw, _s->hh - _th * 0.5);
    fude_zoom_sym_pt(_s, -_s->hw + _tw, _s->hh);
    fude_zoom_sym_end(_s, 0u);
}
// Initial node: a solid dot.
void fzs_initial(fude_zoom_sym* _s)   { fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s), FZS_R(_s), FZS_SOLID | FUDE_ZOOM_SYMBOL_FILLED); }
// Final node: a circle with a solid dot inside.
void fzs_final(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s), FZS_R(_s), FZS_OUT);
    fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s) * 0.6, FZS_R(_s) * 0.6, FZS_SOLID);
}
// Flow final: a circle with an × inside.
void fzs_flow_final(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s), FZS_R(_s), FZS_OUT);
    const f64 _k = 0.7071 * FZS_R(_s);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_k, -_k); fude_zoom_sym_pt(_s, _k, _k); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_k, _k); fude_zoom_sym_pt(_s, _k, -_k); fude_zoom_sym_end(_s, 0u);
}
// Fork or join: a solid bar.
void fzs_bar(fude_zoom_sym* _s)       { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_SOLID | FUDE_ZOOM_SYMBOL_FILLED); }
// Action: a rectangle with well rounded corners.
void fzs_action(fude_zoom_sym* _s)    { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.45, FZS_OUT); }
// State: a rectangle with rounded corners.
void fzs_state(fude_zoom_sym* _s)     { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.3, FZS_OUT); }
// Send signal: a box pointing right.
void fzs_send(fude_zoom_sym* _s)      { const f64 _d[10] = { -1, -1, 0.7, -1, 1, 0, 0.7, 1, -1, 1 }; fude_zoom_sym_poly(_s, _d, 5u, FZS_OUT); }
// Accept signal: a box with a notch in its left side.
void fzs_receive(fude_zoom_sym* _s)   { const f64 _d[10] = { -1, -1, 1, -1, 1, 1, -1, 1, -0.7, 0 }; fude_zoom_sym_poly(_s, _d, 5u, FZS_OUT); }
// Time event: an hourglass.
void fzs_hourglass(fude_zoom_sym* _s) { const f64 _d[8] = { -1, 1, 1, 1, -1, -1, 1, -1 }; fude_zoom_sym_poly(_s, _d, 4u, FZS_OUT); }
// History: a circle with an H inside.
void fzs_history(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s), FZS_R(_s), FZS_OUT);
    const f64 _r = FZS_R(_s);
    // An H.
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r * 0.32, -_r * 0.45); fude_zoom_sym_pt(_s, -_r * 0.32, _r * 0.45); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _r * 0.32, -_r * 0.45); fude_zoom_sym_pt(_s, _r * 0.32, _r * 0.45); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r * 0.32, 0); fude_zoom_sym_pt(_s, _r * 0.32, 0); fude_zoom_sym_end(_s, 0u);
}
// Choice: drawn as the decision.
void fzs_small_decision(fude_zoom_sym* _s) { fzs_decision(_s); }
// A header band's line: where its text box ends (fude_zoom_symbol_text_boxes' HEADER).
RDE_INTERNAL f64 fzs_header_v(const fude_zoom_sym* _s) {
    return 1.0 - rde_math_min_f64(_s->hh * 0.32, 44.0) / rde_math_max_f64(_s->hh, 1e-9);
}

// Swimlane: a rectangle with a header band across its top.
void fzs_vlane(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    fude_zoom_sym_line(_s, -1, fzs_header_v(_s), 1, fzs_header_v(_s), 0u);
}

// Data.
// Database (and the cylinder): a cylinder standing up, the near half of its top's rim drawn.
void fzs_cylinder(fude_zoom_sym* _s) {
    const f64 _ry = rde_math_min_f64(_s->hh * 0.18, _s->hw * 0.35);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0, _s->hh - _ry, _s->hw, _ry, 0.0, RDE_PI);
    fude_zoom_sym_arc_pts(_s, 0, -_s->hh + _ry, _s->hw, _ry, RDE_PI, 2.0 * RDE_PI);
    fude_zoom_sym_end(_s, FZS_OUT);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0, _s->hh - _ry, _s->hw, _ry, RDE_PI, 2.0 * RDE_PI);
    fude_zoom_sym_end(_s, 0u);
}
// Table: a rectangle.
void fzs_table(fude_zoom_sym* _s) { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT); }   // (its header's line: from its text)
// Entity: a rectangle.
void fzs_entity(fude_zoom_sym* _s) { fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT); }
// Weak entity: a rectangle with another just inside it.
void fzs_weak_entity(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    const f64 _g = FZS_R(_s) * 0.12;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw + _g, -_s->hh + _g); fude_zoom_sym_pt(_s, _s->hw - _g, -_s->hh + _g);
    fude_zoom_sym_pt(_s, _s->hw - _g, _s->hh - _g); fude_zoom_sym_pt(_s, -_s->hw + _g, _s->hh - _g);
    fude_zoom_sym_end(_s, FZS_LOOP);
}
// Relationship: drawn as the decision, a diamond.
void fzs_relationship(fude_zoom_sym* _s) { fzs_decision(_s); }
// Weak relationship: a diamond with another just inside it.
void fzs_weak_relationship(fude_zoom_sym* _s) {
    fzs_decision(_s);
    const f64 _k = 0.78;
    const f64 _d[8] = { 0, -_k, _k, 0, 0, _k, -_k, 0 };
    fude_zoom_sym_poly(_s, _d, 4u, FZS_LOOP);
}
// Attribute: an ellipse.
void fzs_attribute(fude_zoom_sym* _s) { fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT); }
// Multivalued attribute: an ellipse with another just inside it.
void fzs_multivalued(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT);
    const f64 _g = FZS_R(_s) * 0.14;
    fude_zoom_sym_ellipse(_s, 0, 0, _s->hw - _g, _s->hh - _g, FZS_LOOP);
}
// Derived attribute: a dashed ellipse.
void fzs_derived(fude_zoom_sym* _s) { fude_zoom_sym_ellipse(_s, 0, 0, _s->hw, _s->hh, FZS_OUT | FUDE_ZOOM_SYMBOL_DASHED); }
// File: a sheet with its top right corner folded down.
void fzs_file(fude_zoom_sym* _s) {
    const f64 _f = FZS_R(_s) * 0.45;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw, -_s->hh); fude_zoom_sym_pt(_s, _s->hw, -_s->hh); fude_zoom_sym_pt(_s, _s->hw, _s->hh - _f);
    fude_zoom_sym_pt(_s, _s->hw - _f, _s->hh); fude_zoom_sym_pt(_s, -_s->hw, _s->hh);
    fude_zoom_sym_end(_s, FZS_OUT);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _s->hw - _f, _s->hh); fude_zoom_sym_pt(_s, _s->hw - _f, _s->hh - _f); fude_zoom_sym_pt(_s, _s->hw, _s->hh - _f);
    fude_zoom_sym_end(_s, 0u);
}
// Storage: a cylinder with three rims across it.
void fzs_disks(fude_zoom_sym* _s) {
    // Storage: three discs stacked.
    const f64 _ry = rde_math_min_f64(_s->hh * 0.14, _s->hw * 0.3);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0, _s->hh - _ry, _s->hw, _ry, 0.0, RDE_PI);
    fude_zoom_sym_arc_pts(_s, 0, -_s->hh + _ry, _s->hw, _ry, RDE_PI, 2.0 * RDE_PI);
    fude_zoom_sym_end(_s, FZS_OUT);
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _y = _s->hh - _ry - (f64)_k * (2.0 * _s->hh - 2.0 * _ry) / 3.0;
        fude_zoom_sym_begin(_s);
        fude_zoom_sym_arc_pts(_s, 0, _y, _s->hw, _ry, RDE_PI, 2.0 * RDE_PI);
        fude_zoom_sym_end(_s, 0u);
    }
}

// Architecture (C4, servers and services, devices, network).
// Person (C4): a round head over a rounded body.
void fzs_person(fude_zoom_sym* _s) {
    // C4's person: a round head over a rounded body (its text in the body).
    const f64 _hr = rde_math_min_f64(_s->hw * 0.32, _s->hh * 0.24);
    const f64 _body_top = _s->hh - 2.0 * _hr + _hr * 0.25;
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _body_top, rde_math_min_f64(_s->hw, _s->hh) * 0.35, FZS_OUT);
    fude_zoom_sym_ellipse(_s, 0, _s->hh - _hr, _hr, _hr, FZS_LOOP | FUDE_ZOOM_SYMBOL_FILLED);
}
// Software system: a rectangle with slightly rounded corners.
void fzs_system(fude_zoom_sym* _s)   { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.12, FZS_OUT); }
// External system: a dashed rectangle with slightly rounded corners.
void fzs_external(fude_zoom_sym* _s) { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.12, FZS_OUT | FUDE_ZOOM_SYMBOL_DASHED); }
// Boundary (C4): a dashed rectangle, its corners barely rounded.
void fzs_c4_boundary(fude_zoom_sym* _s) { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.05, FZS_OUT | FUDE_ZOOM_SYMBOL_DASHED); }
// Server: a rack of three units, each with a light and a slot.
void fzs_server(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.08, FZS_OUT);
    for(u32 _k = 1; _k < 3u; _k++) {
        const f64 _v = 1.0 - 2.0 * (f64)_k / 3.0;
        fude_zoom_sym_line(_s, -1, _v, 1, _v, 0u);
    }
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _v = 1.0 - (2.0 * (f64)_k + 1.0) / 3.0;
        const f64 _r = FZS_R(_s) * 0.07;
        fude_zoom_sym_ellipse(_s, _s->hw * 0.62, _v * _s->hh, _r, _r, FZS_SOLID);
        fude_zoom_sym_line(_s, -0.7, _v, 0.2, _v, 0u);
    }
}
// Desktop: a monitor on a stand.
void fzs_desktop(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -0.45, 1, 1, FZS_OUT);
    fude_zoom_sym_rect(_s, -0.88, -0.3, 0.88, 0.88, FZS_LOOP);
    const f64 _d[8] = { -0.12, -0.45, 0.12, -0.45, 0.2, -0.85, -0.2, -0.85 };
    fude_zoom_sym_poly(_s, _d, 4u, FZS_LOOP);
    fude_zoom_sym_line(_s, -0.45, -1, 0.45, -1, 0u);
    fude_zoom_sym_line(_s, -0.45, -0.85, 0.45, -0.85, 0u);
}
// Laptop: its screen open over its keyboard's base.
void fzs_laptop(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw * 0.8, -_s->hh * 0.55, _s->hw * 0.8, _s->hh, FZS_R(_s) * 0.06, FZS_OUT);
    fude_zoom_sym_rect(_s, -0.7, -0.4, 0.7, 0.86, FZS_LOOP);
    const f64 _d[8] = { -0.8, -0.55, 0.8, -0.55, 1, -1, -1, -1 };
    fude_zoom_sym_poly(_s, _d, 4u, FZS_LOOP | FUDE_ZOOM_SYMBOL_FILLED);
}
// Phone: a rounded body, its screen and its button.
void fzs_phone(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.25, FZS_OUT);
    fude_zoom_sym_rect(_s, -0.82, -0.75, 0.82, 0.8, FZS_LOOP);
    const f64 _r = FZS_R(_s) * 0.09;
    fude_zoom_sym_ellipse(_s, 0, -_s->hh * 0.87, _r, _r, FZS_LOOP);
}
// Tablet: a rounded body and its screen.
void fzs_tablet(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.14, FZS_OUT);
    fude_zoom_sym_rect(_s, -0.86, -0.8, 0.86, 0.86, FZS_LOOP);
}
// Browser: a window with a bar across its top and three dots in it.
void fzs_browser(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.06, FZS_OUT);
    const f64 _bar = rde_math_min_f64(0.32, 22.0 / rde_math_max_f64(_s->hh, 1e-9));
    fude_zoom_sym_line(_s, -1, 1.0 - _bar, 1, 1.0 - _bar, 0u);
    const f64 _r = rde_math_min_f64(_bar * _s->hh * 0.22, _s->hw * 0.05);
    for(u32 _k = 0; _k < 3u; _k++) {
        fude_zoom_sym_ellipse(_s, -_s->hw + _r * (2.6 + 3.0 * (f64)_k), _s->hh * (1.0 - _bar * 0.5), _r, _r, FZS_LOOP);
    }
}
// Cloud (and the cloud shape): bumps along its top and ends, flat underneath.
void fzs_cloud(fude_zoom_sym* _s) {
    // Bumps round a base: arcs of circles along its top and ends, flat underneath.
    fude_zoom_sym_begin(_s);
    const f64 _w = _s->hw, _h = _s->hh;
    fude_zoom_sym_pt(_s, -_w * 0.7, -_h);
    fude_zoom_sym_pt(_s, _w * 0.7, -_h);
    fude_zoom_sym_arc_pts(_s, _w * 0.68, -_h * 0.3, _w * 0.32, _h * 0.7, -RDE_PI * 0.5, RDE_PI * 0.55);
    fude_zoom_sym_arc_pts(_s, _w * 0.18, _h * 0.25, _w * 0.42, _h * 0.75, RDE_PI * 0.1, RDE_PI * 0.95);
    fude_zoom_sym_arc_pts(_s, -_w * 0.42, _h * 0.05, _w * 0.3, _h * 0.55, RDE_PI * 0.35, RDE_PI * 1.05);
    fude_zoom_sym_arc_pts(_s, -_w * 0.7, -_h * 0.5, _w * 0.3, _h * 0.5, RDE_PI * 0.6, RDE_PI * 1.5);
    fude_zoom_sym_end(_s, FZS_OUT);
}
// Queue: a pipe on its side with three messages in it.
void fzs_queue(fude_zoom_sym* _s) {
    // A pipe on its side, slots along it (messages).
    const f64 _rx = rde_math_min_f64(_s->hw * 0.16, _s->hh * 0.45);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, _s->hw - _rx, 0, _rx, _s->hh, -RDE_PI * 0.5, RDE_PI * 0.5);
    fude_zoom_sym_arc_pts(_s, -_s->hw + _rx, 0, _rx, _s->hh, RDE_PI * 0.5, RDE_PI * 1.5);
    fude_zoom_sym_end(_s, FZS_OUT);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, _s->hw - _rx, 0, _rx, _s->hh, RDE_PI * 0.5, RDE_PI * 1.5);
    fude_zoom_sym_end(_s, 0u);
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _x = -_s->hw + _rx * 2.2 + (f64)_k * (_s->hw * 2.0 - _rx * 4.0) / 3.0;
        fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _x, -_s->hh * 0.55); fude_zoom_sym_pt(_s, _x, _s->hh * 0.55); fude_zoom_sym_end(_s, 0u);
    }
}
// Cache: a cylinder with a lightning bolt on it.
void fzs_cache(fude_zoom_sym* _s) {
    fzs_cylinder(_s);
    // A lightning bolt (fast).
    const f64 _r = FZS_R(_s) * 0.55;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _r * 0.15, _r * 0.9); fude_zoom_sym_pt(_s, -_r * 0.45, -_r * 0.05); fude_zoom_sym_pt(_s, -_r * 0.02, -_r * 0.05);
    fude_zoom_sym_pt(_s, -_r * 0.2, -_r * 0.9); fude_zoom_sym_pt(_s, _r * 0.45, _r * 0.15); fude_zoom_sym_pt(_s, _r * 0.02, _r * 0.15);
    fude_zoom_sym_end(_s, FZS_SOLID);
}
// Load balancer: a circle, one line in from the left and three arrows out to the right.
void fzs_balancer(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s);
    fude_zoom_sym_ellipse(_s, 0, 0, _r, _r, FZS_OUT);
    // One in from the left, three out to the right.
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r * 0.7, 0); fude_zoom_sym_pt(_s, -_r * 0.1, 0); fude_zoom_sym_end(_s, 0u);
    for(i32 _k = -1; _k <= 1; _k++) {
        fude_zoom_sym_arrow(_s, -_r * 0.1, 0, _r * 0.65, (f64)_k * _r * 0.45, _r * 0.18);
    }
}
// API gateway: a rounded box with "</>" on it.
void fzs_gateway(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.12, FZS_OUT);
    // "< >": code going through.
    const f64 _r = FZS_R(_s) * 0.42;
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r * 0.35, _r * 0.6); fude_zoom_sym_pt(_s, -_r, 0); fude_zoom_sym_pt(_s, -_r * 0.35, -_r * 0.6); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _r * 0.35, _r * 0.6); fude_zoom_sym_pt(_s, _r, 0); fude_zoom_sym_pt(_s, _r * 0.35, -_r * 0.6); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _r * 0.15, _r * 0.7); fude_zoom_sym_pt(_s, -_r * 0.15, -_r * 0.7); fude_zoom_sym_end(_s, 0u);
}
// Microservice: a hexagon.
void fzs_microservice(fude_zoom_sym* _s) { fude_zoom_sym_regular(_s, 6u, 0.0, FZS_OUT); }
// Function: a rounded box with a λ on it.
void fzs_function(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.2, FZS_OUT);
    // A λ.
    const f64 _r = FZS_R(_s) * 0.6;
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r * 0.55, _r * 0.85); fude_zoom_sym_pt(_s, -_r * 0.25, _r * 0.85); fude_zoom_sym_pt(_s, _r * 0.6, -_r * 0.85); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r * 0.6, -_r * 0.85); fude_zoom_sym_pt(_s, _r * 0.08, _r * 0.25); fude_zoom_sym_end(_s, 0u);
}
// Container: a shipping container, its ribs upright.
void fzs_container(fude_zoom_sym* _s) {
    // A shipping container: a box with its ribs.
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    for(u32 _k = 1; _k < 6u; _k++) {
        const f64 _u = -1.0 + 2.0 * (f64)_k / 6.0;
        fude_zoom_sym_line(_s, _u, -0.8, _u, 0.8, 0u);
    }
}
// Pod: a heptagon, a corner at its top.
void fzs_pod(fude_zoom_sym* _s) { fude_zoom_sym_regular(_s, 7u, RDE_PI * 0.5, FZS_OUT); }
// Firewall: a brick wall of four courses.
void fzs_firewall(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    for(u32 _row = 1; _row < 4u; _row++) {
        const f64 _v = -1.0 + 2.0 * (f64)_row / 4.0;
        fude_zoom_sym_line(_s, -1, _v, 1, _v, 0u);
    }
    for(u32 _row = 0; _row < 4u; _row++) {
        const f64 _v0 = -1.0 + 2.0 * (f64)_row / 4.0, _v1 = _v0 + 0.5;
        for(u32 _k = 0; _k < 3u; _k++) {
            const f64 _u = -1.0 + (2.0 * (f64)_k + (_row % 2u == 0u ? 1.0 : 2.0)) / 3.0 * (2.0 / 2.0);
            if(_u > -1.0 && _u < 1.0) {
                fude_zoom_sym_line(_s, _u, _v0, _u, _v1, 0u);
            }
        }
    }
}
// Bucket: a pail narrowing to its foot, the near half of its rim drawn.
void fzs_bucket(fude_zoom_sym* _s) {
    const f64 _ry = rde_math_min_f64(_s->hh * 0.16, _s->hw * 0.3);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0, _s->hh - _ry, _s->hw, _ry, 0.0, RDE_PI);
    fude_zoom_sym_pt(_s, -_s->hw * 0.75, -_s->hh);
    fude_zoom_sym_pt(_s, _s->hw * 0.75, -_s->hh);
    fude_zoom_sym_end(_s, FZS_OUT);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0, _s->hh - _ry, _s->hw, _ry, RDE_PI, 2.0 * RDE_PI);
    fude_zoom_sym_end(_s, 0u);
}
// Globe: a circle with a meridian, the equator and two parallels.
void fzs_globe(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s);
    fude_zoom_sym_ellipse(_s, 0, 0, _r, _r, FZS_OUT);
    fude_zoom_sym_ellipse(_s, 0, 0, _r * 0.45, _r, FZS_LOOP);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, 0, -_r); fude_zoom_sym_pt(_s, 0, _r); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r, 0); fude_zoom_sym_pt(_s, _r, 0); fude_zoom_sym_end(_s, 0u);
    for(i32 _k = -1; _k <= 1; _k += 2) {
        const f64 _y = (f64)_k * _r * 0.55, _x = rde_math_sqrt_f64(rde_math_max_f64(_r * _r - _y * _y, 0.0));
        fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_x, _y); fude_zoom_sym_pt(_s, _x, _y); fude_zoom_sym_end(_s, 0u);
    }
}
// Router: a circle with four arrows, two out and two in, along its diagonals.
void fzs_router(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s);
    fude_zoom_sym_ellipse(_s, 0, 0, _r, _r, FZS_OUT);
    // Four ways out.
    fude_zoom_sym_arrow(_s, _r * 0.1, _r * 0.1, _r * 0.7, _r * 0.7, _r * 0.2);
    fude_zoom_sym_arrow(_s, -_r * 0.1, -_r * 0.1, -_r * 0.7, -_r * 0.7, _r * 0.2);
    fude_zoom_sym_arrow(_s, _r * 0.7, -_r * 0.7, _r * 0.1, -_r * 0.1, _r * 0.2);
    fude_zoom_sym_arrow(_s, -_r * 0.7, _r * 0.7, -_r * 0.1, _r * 0.1, _r * 0.2);
}
// Switch: a rounded box with two arrows, one each way.
void fzs_switch(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.15, FZS_OUT);
    const f64 _l = _s->hw * 0.55, _h = _s->hh * 0.32, _a = FZS_R(_s) * 0.22;
    fude_zoom_sym_arrow(_s, -_l, _h, _l, _h, _a);
    fude_zoom_sym_arrow(_s, _l, -_h, -_l, -_h, _a);
}
// User: a head and shoulders.
void fzs_user(fude_zoom_sym* _s) {
    // A head and shoulders.
    const f64 _r = rde_math_min_f64(_s->hw * 0.45, _s->hh * 0.3);
    fude_zoom_sym_ellipse(_s, 0, _s->hh - _r, _r, _r, FZS_LOOP | FUDE_ZOOM_SYMBOL_FILLED);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_arc_pts(_s, 0, -_s->hh, _s->hw, _s->hh - 2.0 * _r - _s->hh * 0.08 + _s->hh, 0.0, RDE_PI);
    fude_zoom_sym_end(_s, FZS_OUT);
}
// Event bus: a stadium with an arrow along it.
void fzs_bus(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, _s->hh, FZS_OUT);
    fude_zoom_sym_arrow(_s, -_s->hw * 0.8, 0, _s->hw * 0.8, 0, FZS_R(_s) * 0.5);
}
// Security: a padlock.
void fzs_lock(fude_zoom_sym* _s) {
    // A padlock.
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh * 0.15, FZS_R(_s) * 0.12, FZS_OUT);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw * 0.6, _s->hh * 0.15);
    fude_zoom_sym_arc_pts(_s, 0, _s->hh * 0.4, _s->hw * 0.6, _s->hh * 0.6, RDE_PI, 0.0);
    fude_zoom_sym_pt(_s, _s->hw * 0.6, _s->hh * 0.15);
    fude_zoom_sym_end(_s, 0u);
    const f64 _r = FZS_R(_s) * 0.16;
    fude_zoom_sym_ellipse(_s, 0, -_s->hh * 0.4, _r, _r, FZS_SOLID);
}
// Scheduler: a clock face with its hands and twelve ticks.
void fzs_clock(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s);
    fude_zoom_sym_ellipse(_s, 0, 0, _r, _r, FZS_OUT);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, 0, _r * 0.62); fude_zoom_sym_pt(_s, 0, 0); fude_zoom_sym_pt(_s, _r * 0.45, -_r * 0.2); fude_zoom_sym_end(_s, 0u);
    for(u32 _k = 0; _k < 12u; _k++) {
        const f64 _a = 2.0 * RDE_PI * (f64)_k / 12.0;
        fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, rde_math_cos_f64(_a) * _r * 0.85, rde_math_sin_f64(_a) * _r * 0.85); fude_zoom_sym_pt(_s, rde_math_cos_f64(_a) * _r, rde_math_sin_f64(_a) * _r); fude_zoom_sym_end(_s, 0u);
    }
}
// Service: a gear of eight teeth with a hole.
void fzs_gear(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s);
    fude_zoom_sym_begin(_s);
    const u32 _teeth = 8u;
    for(u32 _k = 0; _k < _teeth; _k++) {
        const f64 _a = 2.0 * RDE_PI * (f64)_k / (f64)_teeth, _t = RDE_PI / (f64)_teeth;
        fude_zoom_sym_pt(_s, rde_math_cos_f64(_a - _t * 0.55) * _r * 0.78, rde_math_sin_f64(_a - _t * 0.55) * _r * 0.78);
        fude_zoom_sym_pt(_s, rde_math_cos_f64(_a - _t * 0.3) * _r, rde_math_sin_f64(_a - _t * 0.3) * _r);
        fude_zoom_sym_pt(_s, rde_math_cos_f64(_a + _t * 0.3) * _r, rde_math_sin_f64(_a + _t * 0.3) * _r);
        fude_zoom_sym_pt(_s, rde_math_cos_f64(_a + _t * 0.55) * _r * 0.78, rde_math_sin_f64(_a + _t * 0.55) * _r * 0.78);
    }
    fude_zoom_sym_end(_s, FZS_OUT);
    fude_zoom_sym_ellipse(_s, 0, 0, _r * 0.32, _r * 0.32, FZS_LOOP);
}
// Folder: a folder, its tab on its top left.
void fzs_folder(fude_zoom_sym* _s) {
    const f64 _d[14] = { -1, -1, 1, -1, 1, 0.7, -0.1, 0.7, -0.25, 1, -1, 1, -1, 0.7 };
    fude_zoom_sym_poly(_s, _d, 6u, FZS_OUT);
}
// Monitoring: a box with a line chart in it.
void fzs_chart(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    const f64 _d[10] = { -0.8, -0.6, -0.35, 0.1, 0.05, -0.25, 0.45, 0.45, 0.8, 0.2 };
    fude_zoom_sym_poly(_s, _d, 5u, 0u);
}
// Container database: drawn as the database.
void fzs_container_db(fude_zoom_sym* _s) { fzs_cylinder(_s); }
// Internet: drawn as the cloud.
void fzs_internet(fude_zoom_sym* _s) { fzs_cloud(_s); }

// BPMN.
// Start event: a circle.
void fzs_start_event(fude_zoom_sym* _s) { fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s), FZS_R(_s), FZS_OUT); }
// Intermediate event: a double circle.
void fzs_intermediate(fude_zoom_sym* _s) {
    fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s), FZS_R(_s), FZS_OUT);
    fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s) * 0.84, FZS_R(_s) * 0.84, FZS_LOOP);
}
// End event: a thick ring.
void fzs_end_event(fude_zoom_sym* _s) {
    // A thick ring: two close circles, the band between them solid.
    fude_zoom_sym_ellipse(_s, 0, 0, FZS_R(_s), FZS_R(_s), FZS_OUT);
    fude_zoom_sym_begin(_s);
    const u32 _k = 48u;
    for(u32 _i = 0; _i <= _k; _i++) {
        const f64 _a = 2.0 * RDE_PI * (f64)_i / (f64)_k;
        fude_zoom_sym_pt(_s, rde_math_cos_f64(_a) * FZS_R(_s), rde_math_sin_f64(_a) * FZS_R(_s));
    }
    for(u32 _i = 0; _i <= _k; _i++) {
        const f64 _a = -2.0 * RDE_PI * (f64)_i / (f64)_k;
        fude_zoom_sym_pt(_s, rde_math_cos_f64(_a) * FZS_R(_s) * 0.82, rde_math_sin_f64(_a) * FZS_R(_s) * 0.82);
    }
    fude_zoom_sym_end(_s, FZS_SOLID);
}
// Timer event: an intermediate event with a clock in it.
void fzs_timer_event(fude_zoom_sym* _s) {
    fzs_intermediate(_s);
    const f64 _r = FZS_R(_s) * 0.6;
    fude_zoom_sym_ellipse(_s, 0, 0, _r, _r, FZS_LOOP);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, 0, _r * 0.75); fude_zoom_sym_pt(_s, 0, 0); fude_zoom_sym_pt(_s, _r * 0.5, 0); fude_zoom_sym_end(_s, 0u);
}
// Message event: an intermediate event with an envelope in it.
void fzs_message_event(fude_zoom_sym* _s) {
    fzs_intermediate(_s);
    const f64 _w = FZS_R(_s) * 0.5, _h = FZS_R(_s) * 0.33;
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_w, -_h); fude_zoom_sym_pt(_s, _w, -_h); fude_zoom_sym_pt(_s, _w, _h); fude_zoom_sym_pt(_s, -_w, _h); fude_zoom_sym_end(_s, FZS_LOOP);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_w, _h); fude_zoom_sym_pt(_s, 0, 0); fude_zoom_sym_pt(_s, _w, _h); fude_zoom_sym_end(_s, 0u);
}
// Task: a rectangle with rounded corners.
void fzs_task(fude_zoom_sym* _s) { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.2, FZS_OUT); }
// Sub-process: a task with a boxed + at the middle of its foot.
void fzs_subprocess(fude_zoom_sym* _s) {
    fzs_task(_s);
    const f64 _b = FZS_R(_s) * 0.16;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_b, -_s->hh); fude_zoom_sym_pt(_s, _b, -_s->hh); fude_zoom_sym_pt(_s, _b, -_s->hh + 2.0 * _b); fude_zoom_sym_pt(_s, -_b, -_s->hh + 2.0 * _b);
    fude_zoom_sym_end(_s, FZS_LOOP);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_b * 0.6, -_s->hh + _b); fude_zoom_sym_pt(_s, _b * 0.6, -_s->hh + _b); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, 0, -_s->hh + _b * 0.4); fude_zoom_sym_pt(_s, 0, -_s->hh + _b * 1.6); fude_zoom_sym_end(_s, 0u);
}
// Exclusive gateway: a diamond with an × in it.
void fzs_xor_gateway(fude_zoom_sym* _s) {
    fzs_decision(_s);
    fude_zoom_sym_line(_s, -0.32, -0.32, 0.32, 0.32, 0u);
    fude_zoom_sym_line(_s, -0.32, 0.32, 0.32, -0.32, 0u);
}
// Parallel gateway: a diamond with a + in it.
void fzs_and_gateway(fude_zoom_sym* _s) {
    fzs_decision(_s);
    fude_zoom_sym_line(_s, -0.45, 0, 0.45, 0, 0u);
    fude_zoom_sym_line(_s, 0, -0.45, 0, 0.45, 0u);
}
// Inclusive gateway: a diamond with a circle in it.
void fzs_or_gateway(fude_zoom_sym* _s) {
    fzs_decision(_s);
    fude_zoom_sym_ellipse(_s, 0, 0, _s->hw * 0.4, _s->hh * 0.4, FZS_LOOP);
}
// Data object: drawn as the file.
void fzs_data_object(fude_zoom_sym* _s) { fzs_file(_s); }
// Data store: drawn as the database.
void fzs_data_store(fude_zoom_sym* _s)  { fzs_cylinder(_s); }
// Pool or lane: a rectangle with a band down its left for its name (at most 34 units wide).
void fzs_pool(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    const f64 _u = -1.0 + rde_math_min_f64(_s->hw * 0.16, 34.0) / rde_math_max_f64(_s->hw, 1e-9);   // (where its text box ends: LEFT)
    fude_zoom_sym_line(_s, _u, -1, _u, 1, 0u);
}
// Group: a dashed rectangle with rounded corners.
void fzs_group(fude_zoom_sym* _s) { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.08, FZS_OUT | FUDE_ZOOM_SYMBOL_DASHED); }

// Planning.
// Column (Kanban): a tall card with a header band across its top.
void fzs_column(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, rde_math_min_f64(_s->hw, _s->hh) * 0.04, FZS_OUT);
    fude_zoom_sym_line(_s, -1, fzs_header_v(_s), 1, fzs_header_v(_s), 0u);
}
// Task card: a rounded card with a solid tag down its left edge.
void fzs_task_card(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.1, FZS_OUT);
    // Its tag down its left edge.
    const f64 _t = rde_math_min_f64(_s->hw * 0.05, 6.0);
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, -_s->hw + _t * 0.5, -_s->hh * 0.75); fude_zoom_sym_pt(_s, -_s->hw + _t * 1.5, -_s->hh * 0.75);
    fude_zoom_sym_pt(_s, -_s->hw + _t * 1.5, _s->hh * 0.75); fude_zoom_sym_pt(_s, -_s->hw + _t * 0.5, _s->hh * 0.75);
    fude_zoom_sym_end(_s, FZS_SOLID);
}
// Lane: drawn as the pool.
void fzs_hlane(fude_zoom_sym* _s) { fzs_pool(_s); }
// An area (map.h): a region of the canvas, named — its outline only (its name in its corner).
void fzs_area(fude_zoom_sym* _s) { fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.025, FZS_OUT); }
// Milestone: a solid diamond.
void fzs_milestone(fude_zoom_sym* _s) {
    const f64 _d[8] = { 0, -1, 1, 0, 0, 1, -1, 0 };
    fude_zoom_sym_poly(_s, _d, 4u, FZS_SOLID | FUDE_ZOOM_SYMBOL_FILLED);
}
// 2 × 2 matrix: a rectangle cut into four quarters.
void fzs_matrix(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    fude_zoom_sym_line(_s, 0, -1, 0, 1, 0u);
    fude_zoom_sym_line(_s, -1, 0, 1, 0, 0u);
}
// Arrow: a wide block arrow pointing right.
void fzs_arrow_block(fude_zoom_sym* _s) {
    const f64 _d[14] = { -1, -0.4, 0.35, -0.4, 0.35, -1, 1, 0, 0.35, 1, 0.35, 0.4, -1, 0.4 };
    fude_zoom_sym_poly(_s, _d, 7u, FZS_OUT);
}
// Timeline: an arrow to the right with five ticks across it.
void fzs_timeline(fude_zoom_sym* _s) {
    fude_zoom_sym_arrow(_s, -_s->hw, 0, _s->hw, 0, rde_math_min_f64(_s->hh, 14.0));
    for(u32 _k = 0; _k < 5u; _k++) {
        const f64 _x = -_s->hw + (f64)_k * _s->hw * 2.0 / 5.0 + _s->hw * 0.15;
        fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _x, -_s->hh * 0.5); fude_zoom_sym_pt(_s, _x, _s->hh * 0.5); fude_zoom_sym_end(_s, 0u);
    }
}

// More (the research's: the sequence diagram's heads, a terminal, a key, an envelope, code, a team, a brace,
// a DFD's data store, an event stream's topic).
// Boundary object: a circle joined by a line to an upright bar on its left.
void fzs_seq_boundary(fude_zoom_sym* _s) {
    const f64 _r = rde_math_min_f64(_s->hw * 0.8, _s->hh);
    const f64 _cx = _s->hw - _r;
    fude_zoom_sym_ellipse(_s, _cx, 0, _r, _r, FZS_OUT);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_s->hw, -_r * 0.6); fude_zoom_sym_pt(_s, -_s->hw, _r * 0.6); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_s->hw, 0); fude_zoom_sym_pt(_s, _cx - _r, 0); fude_zoom_sym_end(_s, 0u);
}
// Control object: a circle with an arrowhead on its top.
void fzs_seq_control(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s) * 0.9;
    fude_zoom_sym_ellipse(_s, 0, -_s->hh * 0.08, _r, _r, FZS_OUT);
    // Its arrowhead on top, going round.
    const f64 _y = -_s->hh * 0.08 + _r;
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _r * 0.25, _y + _r * 0.28); fude_zoom_sym_pt(_s, -_r * 0.2, _y); fude_zoom_sym_pt(_s, _r * 0.25, _y - _r * 0.28); fude_zoom_sym_end(_s, 0u);
}
// Entity object: a circle on a line.
void fzs_seq_entity(fude_zoom_sym* _s) {
    const f64 _r = FZS_R(_s) * 0.9;
    fude_zoom_sym_ellipse(_s, 0, _s->hh * 0.08, _r, _r, FZS_OUT);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r, _s->hh * 0.08 - _r); fude_zoom_sym_pt(_s, _r, _s->hh * 0.08 - _r); fude_zoom_sym_end(_s, 0u);
}
// Terminal: a window with a title bar and a ">_" prompt.
void fzs_terminal(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.08, FZS_OUT);
    const f64 _bar = rde_math_min_f64(0.28, 18.0 / rde_math_max_f64(_s->hh, 1e-9));
    fude_zoom_sym_line(_s, -1, 1.0 - _bar, 1, 1.0 - _bar, 0u);
    const f64 _r = FZS_R(_s) * 0.3;
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_s->hw * 0.7, _r * 0.6); fude_zoom_sym_pt(_s, -_s->hw * 0.7 + _r, 0); fude_zoom_sym_pt(_s, -_s->hw * 0.7, -_r * 0.6); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_s->hw * 0.7 + _r * 1.4, -_r * 0.7); fude_zoom_sym_pt(_s, -_s->hw * 0.7 + _r * 2.6, -_r * 0.7); fude_zoom_sym_end(_s, 0u);
}
// Key: a ring on the left and a shaft with two teeth.
void fzs_key(fude_zoom_sym* _s) {
    const f64 _r = rde_math_min_f64(_s->hh * 0.55, _s->hw * 0.32);
    fude_zoom_sym_ellipse(_s, -_s->hw + _r, 0, _r, _r, FZS_OUT);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_s->hw + 2.0 * _r, 0); fude_zoom_sym_pt(_s, _s->hw, 0); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _s->hw * 0.55, 0); fude_zoom_sym_pt(_s, _s->hw * 0.55, -_r * 0.7); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _s->hw * 0.85, 0); fude_zoom_sym_pt(_s, _s->hw * 0.85, -_r * 0.7); fude_zoom_sym_end(_s, 0u);
}
// Envelope: a rectangle with its flap's V.
void fzs_envelope(fude_zoom_sym* _s) {
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    const f64 _d[6] = { -1, 1, 0, -0.15, 1, 1 };
    fude_zoom_sym_poly(_s, _d, 3u, 0u);
}
// Code: a rounded box with "</>" on it.
void fzs_code(fude_zoom_sym* _s) {
    fude_zoom_sym_rounded(_s, -_s->hw, -_s->hh, _s->hw, _s->hh, FZS_R(_s) * 0.15, FZS_OUT);
    const f64 _r = FZS_R(_s) * 0.6;
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, -_r * 0.3, _r * 0.55); fude_zoom_sym_pt(_s, -_r * 0.85, 0); fude_zoom_sym_pt(_s, -_r * 0.3, -_r * 0.55); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _r * 0.3, _r * 0.55); fude_zoom_sym_pt(_s, _r * 0.85, 0); fude_zoom_sym_pt(_s, _r * 0.3, -_r * 0.55); fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_begin(_s); fude_zoom_sym_pt(_s, _r * 0.15, _r * 0.7); fude_zoom_sym_pt(_s, -_r * 0.15, -_r * 0.7); fude_zoom_sym_end(_s, 0u);
}
// Users: two people, heads and shoulders, one behind the other.
void fzs_users(fude_zoom_sym* _s) {
    // Two heads and shoulders, one behind the other.
    const f64 _r = rde_math_min_f64(_s->hw * 0.26, _s->hh * 0.28);
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _x = _k == 0u ? _s->hw * 0.28 : -_s->hw * 0.18, _top = _k == 0u ? _s->hh : _s->hh * 0.82;
        const f64 _sh = _k == 0u ? _s->hw * 0.72 : _s->hw * 0.82;
        fude_zoom_sym_ellipse(_s, _x, _top - _r, _r, _r, FZS_LOOP | FUDE_ZOOM_SYMBOL_FILLED);
        fude_zoom_sym_begin(_s);
        fude_zoom_sym_arc_pts(_s, _x, -_s->hh, _sh, (_top - 2.0 * _r - _s->hh * 0.06) + _s->hh, 0.0, RDE_PI);
        fude_zoom_sym_end(_s, FZS_OUT);
    }
}
// Brace: a "{" down its left.
void fzs_brace(fude_zoom_sym* _s) {
    // A "{": its points along its left, its middle out.
    fude_zoom_sym_begin(_s);
    const f64 _w = _s->hw, _h = _s->hh;
    fude_zoom_sym_pt(_s, -_w * 0.6, _h);
    fude_zoom_sym_arc_pts(_s, -_w * 0.6, _h * 0.85, _w * 0.3, _h * 0.15, RDE_PI * 0.5, RDE_PI);
    fude_zoom_sym_pt(_s, -_w * 0.9, _h * 0.15);
    fude_zoom_sym_arc_pts(_s, -_w * 1.2, _h * 0.15, _w * 0.3, _h * 0.15, 0.0, -RDE_PI * 0.5);
    fude_zoom_sym_arc_pts(_s, -_w * 1.2, -_h * 0.15, _w * 0.3, _h * 0.15, RDE_PI * 0.5, 0.0);
    fude_zoom_sym_pt(_s, -_w * 0.9, -_h * 0.85);
    fude_zoom_sym_arc_pts(_s, -_w * 0.6, -_h * 0.85, _w * 0.3, _h * 0.15, RDE_PI, RDE_PI * 1.5);
    fude_zoom_sym_end(_s, 0u);
}
// Data store (DFD): two lines, along its top and its bottom.
void fzs_dfd_store(fude_zoom_sym* _s) {
    fude_zoom_sym_line(_s, -1, 1, 1, 1, 0u);
    fude_zoom_sym_line(_s, -1, -1, 1, -1, 0u);
}
// Event topic: an event stream of three partitions, each a row of cells.
void fzs_topic(fude_zoom_sym* _s) {
    // An event stream: three partitions, each a pipe, the newest end to the right.
    fude_zoom_sym_rect(_s, -1, -1, 1, 1, FZS_OUT);
    for(u32 _k = 1; _k < 3u; _k++) {
        const f64 _v = 1.0 - 2.0 * (f64)_k / 3.0;
        fude_zoom_sym_line(_s, -1, _v, 1, _v, 0u);
    }
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _v0 = 1.0 - 2.0 * (f64)_k / 3.0, _v1 = _v0 - 2.0 / 3.0;
        for(u32 _c = 1; _c < 5u; _c++) {
            const f64 _u = -1.0 + 2.0 * (f64)_c / 6.0;
            fude_zoom_sym_line(_s, _u, _v0, _u, _v1, 0u);
        }
    }
}
