// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_EXAMPLES_PARTS
#define FUDE_ZOOM_EXAMPLES_PARTS

#include "zoom/examples.h"
#include "zoom/placer.h"
#include "zoom/circuit.h"
#include "zoom/shape.h"
#include "zoom/props.h"

// ===========================================================================
// Examples parts: the examples' private interface. The examples (examples.h) are written as examples.c and the
// examples_*.c modules; this is what they share with each other: the drawing helpers, the examples themselves (which
// fude_zoom_example_build's table names), and the constants and types more than one needs. Included by examples.c and
// examples_*.c only, never anywhere else: everything outside the examples uses examples.h.
//
// Where it sits: fude/zoom, under examples.h (which it includes, with every header the examples use). The modules
// include this header and nothing else.
// Its main types and their lifecycle: an example being drawn (fzx), made on the stack by fude_zoom_example_build for
// the one example it draws.
// Threads: the main thread only, as the examples themselves.
// Files: none.
// Design: examples.c was one file of some 2,300 lines; it was split by the list's groups, its code moved unchanged.
// A function one module calls in another is declared here (not RDE_INTERNAL); what one module alone uses stays
// RDE_INTERNAL in it. Every name keeps examples.c's prefix, fzx_ and FZX_.
//
// Which module holds what:
//   examples.c              the list's groups and folders, the drawing helpers, the table that builds an example
//   examples_electronics.c  the electronics' examples
//   examples_mechanisms.c   the mechanisms' examples
//   examples_both.c         the examples with both: motors, dynamos, servos, solenoids, steppers, sensors on shafts
// ===========================================================================

// --- the constants and types the modules share (as examples.c had them) ----------------------------------------------

// A degree in radians.
#define FZX_DEG (RDE_PI / 180.0)

// An example being drawn: the placer (where and how it draws) and the caller's words.
typedef struct {
    fude_zoom_placer        p;
    fude_zoom_example_title title;
} fzx;

// FZX_VIA(corners...) hands fzx_route its corners (points from the middle) and how many. The corners are laid out by
// hand, so no wire passes over another's pin (it would join it) or along another's way.
#define FZX_VIA(...) (const fude_zoom_v2[]){ __VA_ARGS__ }, (u32)(sizeof((const fude_zoom_v2[]){ __VA_ARGS__ }) / sizeof(fude_zoom_v2))

// --- examples.c: the drawing helpers ---------------------------------------------------------------------------------

// Small helpers over placer.h, in points from the middle: a point in the frame's units; a part of the library at a
// size (0: its own) or at its own; a part's pin; a wire routed as the Wire tool does, or a straight one (fzx_line); a
// plain text; a pen line.
fude_zoom_v2 fzx_at(const fzx* _x, f64 _px, f64 _py);
u32 fzx_sized(fzx* _x, const c8* _id, f64 _px, f64 _py, f64 _turn, f64 _w, f64 _h, const c8* _text);
u32 fzx_part(fzx* _x, const c8* _id, f64 _px, f64 _py, f64 _turn, const c8* _text);
fude_zoom_v2 fzx_pin(const fzx* _x, u32 _o, u32 _pin);
void fzx_wire(fzx* _x, u32 _a, u32 _pa, u32 _b, u32 _pb);
void fzx_line(fzx* _x, u32 _a, u32 _pa, u32 _b, u32 _pb);
void fzx_text(fzx* _x, f64 _px, f64 _py, f64 _size, f64 _w, const c8* _text);
u32 fzx_path(fzx* _x, const fude_zoom_v2* _pts, u32 _n, b8 _closed, f64 _width);

// The one being drawn moved to _px, _py (points from where it was); the middle it had back.
fude_zoom_v2 fzx_move(fzx* _x, f64 _px, f64 _py);
// A wire from part _a's pin _pa to _b's _pb through the corners _via (_n of them; placer.h draws nothing when they do
// not fit in a wire).
void fzx_route(fzx* _x, u32 _a, u32 _pa, u32 _b, u32 _pb, const fude_zoom_v2* _via, u32 _n);
// A word by what is drawn (FUDE_ZOOM_EXAMPLE_WORD_'s, a material's: examples.h), _size points high, its top-left at
// _px, _py, wrapped at _w; nothing when the caller gives none.
void fzx_word(fzx* _x, f64 _px, f64 _py, f64 _size, f64 _w, u32 _word);
// Limits of its own on a part (its card's: props.h): _w, _a, _v, _r (0: none), the real part they are.
void fzx_limits(fzx* _x, u32 _part, f64 _w, f64 _a, f64 _v, f64 _r, i32 _preset);
// A drawing made a body of material _material (sim/body.h's; fixed: the ground): a pen line through _pts.
u32 fzx_drawn(fzx* _x, const fude_zoom_v2* _pts, u32 _n, b8 _closed, u32 _material, b8 _fixed);
// A drawn ball of _r points round at _px, _py, of material _material.
void fzx_ball(fzx* _x, f64 _px, f64 _py, f64 _r, u32 _material);
// Writes example _example's title (26 points high, wrapped at 520) with its top-left at _px, _py; nothing when the
// caller gives none.
void fzx_title(fzx* _x, f64 _px, f64 _py, u32 _example);

// --- examples_electronics.c: the electronics' examples ---------------------------------------------------------------

// Each draws its example (examples.h's FUDE_ZOOM_EXAMPLE_ of its name) into _x, its comment beside it:
// fude_zoom_example_build's table names them, and examples_both.c's canvases of many draw a few side by side. In the
// list's order, as the module has them.
void fzx_torch(fzx* _x);
void fzx_gates(fzx* _x);
void fzx_flasher(fzx* _x);
void fzx_full_adder(fzx* _x);
void fzx_adder_4(fzx* _x);
void fzx_counter(fzx* _x);
void fzx_adder_chip(fzx* _x);
void fzx_chaser(fzx* _x);
void fzx_led_resistor(fzx* _x);
void fzx_resistor_watts(fzx* _x);
void fzx_short_fuse(fzx* _x);
void fzx_cap_polarity(fzx* _x);
void fzx_transistor_size(fzx* _x);
void fzx_ringing(fzx* _x);
void fzx_digit_counter(fzx* _x);
void fzx_matrix_scan(fzx* _x);
void fzx_bar_meter(fzx* _x);
void fzx_panel_meters(fzx* _x);
void fzx_lcd_by_hand(fzx* _x);
void fzx_scope_rc(fzx* _x);
void fzx_light_temp(fzx* _x);
void fzx_decade(fzx* _x);
void fzx_flipflops(fzx* _x);
void fzx_comparator(fzx* _x);
void fzx_thyristors(fzx* _x);
void fzx_power_supply(fzx* _x);
void fzx_light_link(fzx* _x);
void fzx_speaker_tone(fzx* _x);
void fzx_traffic(fzx* _x);
void fzx_piano(fzx* _x);

// --- examples_mechanisms.c: the mechanisms' examples -----------------------------------------------------------------

// A link (a bar _thick thick) with its holes on _a and _b.
u32 fzx_bar(fzx* _x, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _thick);
// A belt (or a chain) whose ends' middles are on _a and _b (40 across).
u32 fzx_belt(fzx* _x, const c8* _id, fude_zoom_v2 _a, fude_zoom_v2 _b, const c8* _text);

// Each draws its example (examples.h's FUDE_ZOOM_EXAMPLE_ of its name) into _x, its comment beside it:
// fude_zoom_example_build's table names them, and examples_both.c's canvases of many draw a few side by side.
void fzx_strandbeest(fzx* _x);
void fzx_engine(fzx* _x);
void fzx_crank(fzx* _x);
void fzx_by_hand(fzx* _x);
void fzx_coupler_curve(fzx* _x);
void fzx_belts(fzx* _x);
void fzx_cam(fzx* _x);
void fzx_ratchet(fzx* _x);
void fzx_damper(fzx* _x);
void fzx_worm(fzx* _x);
void fzx_gear_train(fzx* _x);
void fzx_pendulum(fzx* _x);
void fzx_pulleys(fzx* _x);
void fzx_piston(fzx* _x);
void fzx_rack(fzx* _x);
void fzx_gears_rack(fzx* _x);
void fzx_bodies(fzx* _x);
void fzx_materials(fzx* _x);
void fzx_too_heavy(fzx* _x);
void fzx_motor_torque(fzx* _x);
void fzx_gearbox(fzx* _x);
void fzx_machine(fzx* _x);

// --- examples_both.c: the examples with both -------------------------------------------------------------------------

// Each draws its example (examples.h's FUDE_ZOOM_EXAMPLE_ of its name) into _x, its comment beside it:
// fude_zoom_example_build's table names them.
void fzx_solenoid_engine(fzx* _x);
void fzx_motor_gears(fzx* _x);
void fzx_dynamo(fzx* _x);
void fzx_forward_back(fzx* _x);
void fzx_turn_counter(fzx* _x);
void fzx_shuttle(fzx* _x);
void fzx_servo_tester(fzx* _x);
void fzx_servo_angles(fzx* _x);
void fzx_solenoid(fzx* _x);
void fzx_stepper(fzx* _x);
void fzx_shaft_sensors(fzx* _x);
void fzx_workbench(fzx* _x);
void fzx_everything(fzx* _x);

#endif
