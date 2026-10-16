// The test doubles for the parts of the engine a test cannot use for real: there is no GPU (drawing does nothing,
// a picture decodes to nothing) and the clock stands still (so a suite's timing is its own to set). Everything else —
// files, the log, errors, containers, strings, maths — is the engine's real library (tests/support/headless.c).
//
// Every double is weak: a suite that needs its own (a clock it moves, a drawing call it counts) defines it in its
// test.c, and that one wins.
#include "rde.h"

#define FUDE_STUB __attribute__((weak, visibility("hidden")))   // (hidden: a library's real one cannot take its place at load time)

static f64 fake_now = 1.0;   // the clock stands still
FUDE_STUB f64  rde_engine_get_time_now(void) { return fake_now; }
FUDE_STUB void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)p; (void)r; (void)n; (void)c; }
// (a stroke's mesh is made by the engine's real tessellation, rde_rendering_2d_tessellate_stroke;
// only drawing it is not)
FUDE_STUB void rde_rendering_2d_draw_stroke_mesh(const rde_stroke_mesh* m, rde_vec_2F t, rde_color c) { (void)m; (void)t; (void)c; }
FUDE_STUB void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)s; (void)c; (void)sh; }
FUDE_STUB void rde_rendering_2d_draw_text_2(rde_font* f, const c8* t, rde_vec_3F p, rde_vec_2F s, f32 r, rde_color c) { (void)f; (void)t; (void)p; (void)s; (void)r; (void)c; }
FUDE_STUB u8* rde_image_decode(const u8* b, usize n, u32* w, u32* h, rde_memory_allocator* a) { (void)b; (void)n; (void)a; if(w) *w = 0; if(h) *h = 0; return NULL; }
FUDE_STUB void rde_rendering_2d_draw_triangle(const rde_vec_2F a, const rde_vec_2F b, const rde_vec_2F c, const rde_color k, rde_shader* sh) { (void)a; (void)b; (void)c; (void)k; (void)sh; }
FUDE_STUB void rde_rendering_2d_draw_line_1(rde_vec_2F a, rde_vec_2F b, rde_color c, f32 t) { (void)a; (void)b; (void)c; (void)t; }
FUDE_STUB void rde_rendering_2d_draw_polygon(const rde_vec_2F* p, u32 n, const rde_color c, rde_shader* sh) { (void)p; (void)n; (void)c; (void)sh; }
FUDE_STUB void rde_rendering_2d_draw_circle_with_border(const rde_vec_2F p, f32 r, u32 s, const rde_color f, f32 t, const rde_color b, rde_shader* sh) { (void)p; (void)r; (void)s; (void)f; (void)t; (void)b; (void)sh; }
FUDE_STUB void rde_rendering_2d_draw_rounded_rectangle_with_border(const rde_vec_2F c, const rde_vec_2F s, f32 r, u32 p, const rde_color f, f32 t, const rde_color b, rde_shader* sh) { (void)c; (void)s; (void)r; (void)p; (void)f; (void)t; (void)b; (void)sh; }

// A right-to-left screen's mirror (draw.h): none in a test — ink.c draws through it, and a suite without draw.c still
// links. Weak: draw.c's own wins.
__attribute__((weak, visibility("hidden"))) rde_vec_2F fude_draw_at(rde_vec_2F _p) { return _p; }
