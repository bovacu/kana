// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SYMBOL_PARTS
#define FUDE_ZOOM_SYMBOL_PARTS

#include "zoom/symbol.h"
#include "zoom/scene.h"
#include "zoom/circuit.h"
#include "zoom/mech.h"
#include "zoom/plan.h"

// ===========================================================================
// Symbol parts: the symbols' private interface. The symbols (symbol.h) are written as symbol.c and the symbol_*.c
// modules; this is what they share with each other: the pen every draw function draws with, the draw functions the
// table (FUDE_ZOOM_SYMBOLS, symbol.c) names, and the constants and types more than one needs. Included by symbol.c and
// symbol_*.c only, never anywhere else: everything outside the symbols uses symbol.h.
//
// Where it sits: fude/zoom, under symbol.h (which it includes, with every header the symbols use). The modules include
// this header and nothing else.
// Its main types and their lifecycle: a symbol being drawn (fude_zoom_sym), made on the stack by fude_zoom_symbol_parts
// and fude_zoom_symbol_head (symbol.c) round the caller's arrays; the table is a constant, with nothing to create or
// free.
// Threads: the main thread only, as symbol.h says.
// Files: none.
// Design: symbol.c was one file of some 3,200 lines; it was split by what each part does, its code moved unchanged. The
// table stays with the public functions that read it (FUDE_ZOOM_SYMBOL_N is its sizeof), and with the topics' parts,
// which are only ever named by it; the draw functions of the other families are declared here, family by family, each
// with its comment over its definition. A function one module calls in another is declared here (not RDE_INTERNAL);
// what one module alone uses stays RDE_INTERNAL in it. Every internal name keeps symbol.c's prefixes: fude_zoom_sym_,
// fzs_ and FZS_.
//
// Which module holds what:
//   symbol.c           the coordinator: the pen, the topics' parts (circuit.h's, mech.h's, plan.h's), the table
//                      FUDE_ZOOM_SYMBOLS, and the public functions (find, draw, outline, text boxes, heads, numbers)
//   symbol_diagrams.c  the diagrams' symbols: flowchart, UML, data and ER, architecture, BPMN, planning, and more
//   symbol_shapes.c    the other families': maths, sewing, plain shapes, Venn, geometry, science, stamps, brackets
// ===========================================================================

// --- the constants and types the modules share (as symbol.c had them) ------------------------------------------------

// A symbol being drawn: where its points and parts go, its half sizes, its curves' points to a whole turn, and the
// first point of the part being made.
typedef struct {
    rde_arr* points;
    rde_arr* parts;
    f64      hw, hh;
    u32      segments;
    u32      first;
    u32      kind;   // the kind being drawn (a topic's part is drawn by its id: symbol.c's fzs_circuit and the like)
} fude_zoom_sym;

#define FZS_OUT  (FUDE_ZOOM_SYMBOL_CLOSED | FUDE_ZOOM_SYMBOL_FILLED)   // an outline: closed, filled as the shape is
// A closed line that is not filled (a line inside an outline).
#define FZS_LOOP FUDE_ZOOM_SYMBOL_CLOSED
// A closed shape filled with the line's colour.
#define FZS_SOLID (FUDE_ZOOM_SYMBOL_CLOSED | FUDE_ZOOM_SYMBOL_SOLID)

// The smaller half (an icon's round bits stay round).
#define FZS_R(_s) rde_math_min_f64((_s)->hw, (_s)->hh)

// How far down a speech bubble's body goes (fzs_speech draws it so; symbol.c's text boxes keep within it).
#define FZS_BUBBLE_FOOT 0.45   // (a speech bubble's body's bottom, as v)

// How deep a cube is (fzs_cube draws it so; symbol.c's text boxes keep to its front).
#define FZS_CUBE_DEPTH 0.5   // (a cube's depth, of its box's half: as u and v)

// --- symbol.c: the pen -----------------------------------------------------------------------------------------------

// Adds a point in the symbol's own units to the part being made.
void fude_zoom_sym_pt(fude_zoom_sym* _s, f64 _x, f64 _y);
// In the box's proportions: u, v from -1 to 1.
void fude_zoom_sym_uv(fude_zoom_sym* _s, f64 _u, f64 _v);
// Starts a part at the next point.
void fude_zoom_sym_begin(fude_zoom_sym* _s);
// Ends the part begun by fude_zoom_sym_begin, flagged _flags: kept with 2 points or more (a solid one with 1: a dot),
// else dropped with its point.
void fude_zoom_sym_end(fude_zoom_sym* _s, u8 _flags);
// A part of _n points in u, v (pairs).
void fude_zoom_sym_poly(fude_zoom_sym* _s, const f64* _uv, u32 _n, u8 _flags);
// A straight line in u, v.
void fude_zoom_sym_line(fude_zoom_sym* _s, f64 _u0, f64 _v0, f64 _u1, f64 _v1, u8 _flags);
// A closed rectangle in u, v.
void fude_zoom_sym_rect(fude_zoom_sym* _s, f64 _u0, f64 _v0, f64 _u1, f64 _v1, u8 _flags);
// An arc of the ellipse round (cx, cy) (own units) _rx by _ry, from angle _a0 to _a1 (radians), its points
// added to the part being made (its ends included).
void fude_zoom_sym_arc_pts(fude_zoom_sym* _s, f64 _cx, f64 _cy, f64 _rx, f64 _ry, f64 _a0, f64 _a1);
// An ellipse (own units); a whole one closed.
void fude_zoom_sym_ellipse(fude_zoom_sym* _s, f64 _cx, f64 _cy, f64 _rx, f64 _ry, u8 _flags);
// A rounded rectangle (own units), its corners _r round.
void fude_zoom_sym_rounded(fude_zoom_sym* _s, f64 _x0, f64 _y0, f64 _x1, f64 _y1, f64 _r, u8 _flags);
// A wave from (x0, y) to (x1, y) (own units), _a high, _n half waves.
void fude_zoom_sym_wave_pts(fude_zoom_sym* _s, f64 _x0, f64 _x1, f64 _y, f64 _a, u32 _n);
// An arrowhead at (x, y) (own units) pointing along (dx, dy), _len long: filled.
void fude_zoom_sym_head(fude_zoom_sym* _s, f64 _x, f64 _y, f64 _dx, f64 _dy, f64 _len);
// A line with a head at its end (own units).
void fude_zoom_sym_arrow(fude_zoom_sym* _s, f64 _x0, f64 _y0, f64 _x1, f64 _y1, f64 _len);
// A regular polygon round the middle, _n sides, the first corner at angle _a.
void fude_zoom_sym_regular(fude_zoom_sym* _s, u32 _n, f64 _a, u8 _flags);

// --- symbol_diagrams.c: the diagrams' symbols ------------------------------------------------------------------------

// Flowchart and basic shapes: each draws its symbol (FUDE_ZOOM_SYMBOLS, symbol.c, names which; its comment is over
// it).
void fzs_process(fude_zoom_sym* _s);
void fzs_rounded(fude_zoom_sym* _s);
void fzs_decision(fude_zoom_sym* _s);
void fzs_terminator(fude_zoom_sym* _s);
void fzs_data(fude_zoom_sym* _s);
void fzs_predefined(fude_zoom_sym* _s);
void fzs_document(fude_zoom_sym* _s);
void fzs_documents(fude_zoom_sym* _s);
void fzs_manual_input(fude_zoom_sym* _s);
void fzs_manual_op(fude_zoom_sym* _s);
void fzs_preparation(fude_zoom_sym* _s);
void fzs_delay(fude_zoom_sym* _s);
void fzs_stored_data(fude_zoom_sym* _s);
void fzs_internal_storage(fude_zoom_sym* _s);
void fzs_display(fude_zoom_sym* _s);
void fzs_offpage(fude_zoom_sym* _s);
void fzs_connector(fude_zoom_sym* _s);
void fzs_merge(fude_zoom_sym* _s);
void fzs_extract(fude_zoom_sym* _s);
void fzs_or(fude_zoom_sym* _s);
void fzs_summing(fude_zoom_sym* _s);
void fzs_collate(fude_zoom_sym* _s);
void fzs_sort(fude_zoom_sym* _s);
void fzs_annotation(fude_zoom_sym* _s);
void fzs_card(fude_zoom_sym* _s);
void fzs_tape(fude_zoom_sym* _s);
void fzs_loop_limit(fude_zoom_sym* _s);
void fzs_triangle(fude_zoom_sym* _s);
void fzs_hexagon(fude_zoom_sym* _s);
void fzs_ellipse(fude_zoom_sym* _s);
void fzs_callout(fude_zoom_sym* _s);

// UML structure: each draws its symbol (FUDE_ZOOM_SYMBOLS, symbol.c, names which; its comment is over it).
void fzs_class(fude_zoom_sym* _s);
void fzs_interface(fude_zoom_sym* _s);
void fzs_package(fude_zoom_sym* _s);
void fzs_note(fude_zoom_sym* _s);
void fzs_component(fude_zoom_sym* _s);
void fzs_node(fude_zoom_sym* _s);
void fzs_artifact(fude_zoom_sym* _s);
void fzs_actor(fude_zoom_sym* _s);
void fzs_use_case(fude_zoom_sym* _s);
void fzs_boundary(fude_zoom_sym* _s);
void fzs_socket(fude_zoom_sym* _s);

// UML behaviour (sequence, activity, state): each draws its symbol (FUDE_ZOOM_SYMBOLS, symbol.c, names which; its
// comment is over it).
void fzs_lifeline(fude_zoom_sym* _s);
void fzs_activation(fude_zoom_sym* _s);
void fzs_destroy(fude_zoom_sym* _s);
void fzs_fragment(fude_zoom_sym* _s);
void fzs_initial(fude_zoom_sym* _s);
void fzs_final(fude_zoom_sym* _s);
void fzs_flow_final(fude_zoom_sym* _s);
void fzs_bar(fude_zoom_sym* _s);
void fzs_action(fude_zoom_sym* _s);
void fzs_state(fude_zoom_sym* _s);
void fzs_send(fude_zoom_sym* _s);
void fzs_receive(fude_zoom_sym* _s);
void fzs_hourglass(fude_zoom_sym* _s);
void fzs_history(fude_zoom_sym* _s);
void fzs_small_decision(fude_zoom_sym* _s);
void fzs_vlane(fude_zoom_sym* _s);

// Data and ER: each draws its symbol (FUDE_ZOOM_SYMBOLS, symbol.c, names which; its comment is over it).
void fzs_cylinder(fude_zoom_sym* _s);
void fzs_table(fude_zoom_sym* _s);
void fzs_entity(fude_zoom_sym* _s);
void fzs_weak_entity(fude_zoom_sym* _s);
void fzs_relationship(fude_zoom_sym* _s);
void fzs_weak_relationship(fude_zoom_sym* _s);
void fzs_attribute(fude_zoom_sym* _s);
void fzs_multivalued(fude_zoom_sym* _s);
void fzs_derived(fude_zoom_sym* _s);
void fzs_file(fude_zoom_sym* _s);
void fzs_disks(fude_zoom_sym* _s);

// Architecture (C4, servers and services, devices, network): each draws its symbol (FUDE_ZOOM_SYMBOLS, symbol.c,
// names which; its comment is over it).
void fzs_person(fude_zoom_sym* _s);
void fzs_system(fude_zoom_sym* _s);
void fzs_external(fude_zoom_sym* _s);
void fzs_c4_boundary(fude_zoom_sym* _s);
void fzs_server(fude_zoom_sym* _s);
void fzs_desktop(fude_zoom_sym* _s);
void fzs_laptop(fude_zoom_sym* _s);
void fzs_phone(fude_zoom_sym* _s);
void fzs_tablet(fude_zoom_sym* _s);
void fzs_browser(fude_zoom_sym* _s);
void fzs_cloud(fude_zoom_sym* _s);
void fzs_queue(fude_zoom_sym* _s);
void fzs_cache(fude_zoom_sym* _s);
void fzs_balancer(fude_zoom_sym* _s);
void fzs_gateway(fude_zoom_sym* _s);
void fzs_microservice(fude_zoom_sym* _s);
void fzs_function(fude_zoom_sym* _s);
void fzs_container(fude_zoom_sym* _s);
void fzs_pod(fude_zoom_sym* _s);
void fzs_firewall(fude_zoom_sym* _s);
void fzs_bucket(fude_zoom_sym* _s);
void fzs_globe(fude_zoom_sym* _s);
void fzs_router(fude_zoom_sym* _s);
void fzs_switch(fude_zoom_sym* _s);
void fzs_user(fude_zoom_sym* _s);
void fzs_bus(fude_zoom_sym* _s);
void fzs_lock(fude_zoom_sym* _s);
void fzs_clock(fude_zoom_sym* _s);
void fzs_gear(fude_zoom_sym* _s);
void fzs_folder(fude_zoom_sym* _s);
void fzs_chart(fude_zoom_sym* _s);
void fzs_container_db(fude_zoom_sym* _s);
void fzs_internet(fude_zoom_sym* _s);

// BPMN: each draws its symbol (FUDE_ZOOM_SYMBOLS, symbol.c, names which; its comment is over it).
void fzs_start_event(fude_zoom_sym* _s);
void fzs_intermediate(fude_zoom_sym* _s);
void fzs_end_event(fude_zoom_sym* _s);
void fzs_timer_event(fude_zoom_sym* _s);
void fzs_message_event(fude_zoom_sym* _s);
void fzs_task(fude_zoom_sym* _s);
void fzs_subprocess(fude_zoom_sym* _s);
void fzs_xor_gateway(fude_zoom_sym* _s);
void fzs_and_gateway(fude_zoom_sym* _s);
void fzs_or_gateway(fude_zoom_sym* _s);
void fzs_data_object(fude_zoom_sym* _s);
void fzs_data_store(fude_zoom_sym* _s);
void fzs_pool(fude_zoom_sym* _s);
void fzs_group(fude_zoom_sym* _s);

// Planning (Kanban, swimlanes, milestones): each draws its symbol (FUDE_ZOOM_SYMBOLS, symbol.c, names which; its
// comment is over it).
void fzs_column(fude_zoom_sym* _s);
void fzs_task_card(fude_zoom_sym* _s);
void fzs_hlane(fude_zoom_sym* _s);
void fzs_area(fude_zoom_sym* _s);
void fzs_milestone(fude_zoom_sym* _s);
void fzs_matrix(fude_zoom_sym* _s);
void fzs_arrow_block(fude_zoom_sym* _s);
void fzs_timeline(fude_zoom_sym* _s);

// More (the sequence diagram's heads, a terminal, a key, an envelope, code, a team, a brace, a DFD's store, a
// topic): each draws its symbol (FUDE_ZOOM_SYMBOLS, symbol.c, names which; its comment is over it).
void fzs_seq_boundary(fude_zoom_sym* _s);
void fzs_seq_control(fude_zoom_sym* _s);
void fzs_seq_entity(fude_zoom_sym* _s);
void fzs_terminal(fude_zoom_sym* _s);
void fzs_key(fude_zoom_sym* _s);
void fzs_envelope(fude_zoom_sym* _s);
void fzs_code(fude_zoom_sym* _s);
void fzs_users(fude_zoom_sym* _s);
void fzs_brace(fude_zoom_sym* _s);
void fzs_dfd_store(fude_zoom_sym* _s);
void fzs_topic(fude_zoom_sym* _s);

// --- symbol_shapes.c: the other families' symbols --------------------------------------------------------------------

// Maths (plot.h: what is in them is drawn from their text): each draws its symbol (FUDE_ZOOM_SYMBOLS, symbol.c,
// names which; its comment is over it).
void fzs_graph(fude_zoom_sym* _s);
void fzs_number_line(fude_zoom_sym* _s);

// Sewing: each draws its symbol (FUDE_ZOOM_SYMBOLS, symbol.c, names which; its comment is over it).
void fzs_grainline(fude_zoom_sym* _s);
void fzs_fold(fude_zoom_sym* _s);
void fzs_notch(fude_zoom_sym* _s);
void fzs_button(fude_zoom_sym* _s);
void fzs_buttonhole(fude_zoom_sym* _s);
void fzs_dart(fude_zoom_sym* _s);
void fzs_pattern_label(fude_zoom_sym* _s);

// Plain shapes: each draws its symbol (FUDE_ZOOM_SYMBOLS, symbol.c, names which; its comment is over it).
void fzs_rectangle(fude_zoom_sym* _s);
void fzs_round_rect(fude_zoom_sym* _s);
void fzs_circle(fude_zoom_sym* _s);
void fzs_parallelogram(fude_zoom_sym* _s);
void fzs_trapezoid(fude_zoom_sym* _s);
void fzs_right_triangle(fude_zoom_sym* _s);
void fzs_pentagon(fude_zoom_sym* _s);
void fzs_octagon(fude_zoom_sym* _s);
void fzs_star(fude_zoom_sym* _s);
void fzs_burst(fude_zoom_sym* _s);
void fzs_cross(fude_zoom_sym* _s);
void fzs_heart(fude_zoom_sym* _s);
void fzs_speech(fude_zoom_sym* _s);
void fzs_thought(fude_zoom_sym* _s);
void fzs_arc(fude_zoom_sym* _s);
void fzs_pie(fude_zoom_sym* _s);
void fzs_ring(fude_zoom_sym* _s);
void fzs_cube(fude_zoom_sym* _s);
void fzs_block_arrow(fude_zoom_sym* _s);
void fzs_double_arrow(fude_zoom_sym* _s);
void fzs_curved_arrow(fude_zoom_sym* _s);
void fzs_chevron(fude_zoom_sym* _s);

// Venn diagrams: their circles' radii (symbol.c's text boxes are placed by them), and each draws its symbol
// (FUDE_ZOOM_SYMBOLS, symbol.c, names which; its comment is over it).
f64 fzs_venn2_r(f64 _hw, f64 _hh);
void fzs_venn2(fude_zoom_sym* _s);
f64 fzs_venn3_r(f64 _hw, f64 _hh);
void fzs_venn3(fude_zoom_sym* _s);

// Geometry's marks: each draws its symbol (FUDE_ZOOM_SYMBOLS, symbol.c, names which; its comment is over it).
void fzs_angle_mark(fude_zoom_sym* _s);
void fzs_right_angle_mark(fude_zoom_sym* _s);
void fzs_tick1(fude_zoom_sym* _s);
void fzs_tick2(fude_zoom_sym* _s);
void fzs_tick3(fude_zoom_sym* _s);
void fzs_parallel1(fude_zoom_sym* _s);
void fzs_parallel2(fude_zoom_sym* _s);
void fzs_point(fude_zoom_sym* _s);
void fzs_segment(fude_zoom_sym* _s);
void fzs_ray(fude_zoom_sym* _s);
void fzs_line_both(fude_zoom_sym* _s);
void fzs_vector(fude_zoom_sym* _s);
void fzs_circle_radius(fude_zoom_sym* _s);
void fzs_protractor(fude_zoom_sym* _s);

// Science (glassware, optics, an atom, a cell, DNA): each draws its symbol (FUDE_ZOOM_SYMBOLS, symbol.c, names
// which; its comment is over it).
void fzs_beaker(fude_zoom_sym* _s);
void fzs_flask(fude_zoom_sym* _s);
void fzs_round_flask(fude_zoom_sym* _s);
void fzs_test_tube(fude_zoom_sym* _s);
void fzs_cylinder_measure(fude_zoom_sym* _s);
void fzs_funnel(fude_zoom_sym* _s);
void fzs_burner(fude_zoom_sym* _s);
void fzs_lens_convex(fude_zoom_sym* _s);
void fzs_lens_concave(fude_zoom_sym* _s);
void fzs_mirror_flat(fude_zoom_sym* _s);
void fzs_mirror_curved(fude_zoom_sym* _s);
void fzs_prism(fude_zoom_sym* _s);
void fzs_lamp(fude_zoom_sym* _s);
void fzs_atom(fude_zoom_sym* _s);
void fzs_benzene(fude_zoom_sym* _s);
void fzs_cell(fude_zoom_sym* _s);
void fzs_dna(fude_zoom_sym* _s);
void fzs_thermometer(fude_zoom_sym* _s);

// Stamps, in their own colours: each draws its symbol (FUDE_ZOOM_SYMBOLS, symbol.c, names which; its comment is over
// it).
void fzs_stamp_check(fude_zoom_sym* _s);
void fzs_stamp_cross(fude_zoom_sym* _s);
void fzs_stamp_warning(fude_zoom_sym* _s);
void fzs_stamp_question(fude_zoom_sym* _s);
void fzs_stamp_exclamation(fude_zoom_sym* _s);
void fzs_stamp_info(fude_zoom_sym* _s);
void fzs_stamp_star(fude_zoom_sym* _s);
void fzs_stamp_heart(fude_zoom_sym* _s);
void fzs_stamp_bulb(fude_zoom_sym* _s);
void fzs_stamp_flag(fude_zoom_sym* _s);
void fzs_stamp_pin(fude_zoom_sym* _s);
void fzs_stamp_clock(fude_zoom_sym* _s);

// Brackets: how far in a pair's are (symbol.c's text boxes go between them), and each draws its symbol
// (FUDE_ZOOM_SYMBOLS, symbol.c, names which; its comment is over it).
void fzs_paren_l(fude_zoom_sym* _s);
void fzs_paren_r(fude_zoom_sym* _s);
void fzs_square_l(fude_zoom_sym* _s);
void fzs_square_r(fude_zoom_sym* _s);
void fzs_curly_l(fude_zoom_sym* _s);
void fzs_curly_r(fude_zoom_sym* _s);
void fzs_angle_l(fude_zoom_sym* _s);
void fzs_angle_r(fude_zoom_sym* _s);
void fzs_vbar(fude_zoom_sym* _s);
void fzs_double_bar(fude_zoom_sym* _s);
void fzs_floor_l(fude_zoom_sym* _s);
void fzs_floor_r(fude_zoom_sym* _s);
void fzs_ceiling_l(fude_zoom_sym* _s);
void fzs_ceiling_r(fude_zoom_sym* _s);
void fzs_brace_under(fude_zoom_sym* _s);
void fzs_brace_over(fude_zoom_sym* _s);
f64 fzs_pair_width(f64 _hw, f64 _hh);
void fzs_pair_round(fude_zoom_sym* _s);
void fzs_pair_square(fude_zoom_sym* _s);
void fzs_pair_curly(fude_zoom_sym* _s);
void fzs_pair_angle(fude_zoom_sym* _s);
void fzs_pair_bar(fude_zoom_sym* _s);
void fzs_pair_double(fude_zoom_sym* _s);

#endif
