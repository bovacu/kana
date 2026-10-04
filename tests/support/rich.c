#include "rde.h"
#include <string.h>
// Text measuring is the engine's (a font, Slug): the tests have no font, so draw.c's
// estimates fall back to their defaults.
rde_vec_2F rde_rich_text_measure(const c8* m, rde_font* f, f32 s, f32 w, b8 wrap) { (void)m; (void)f; (void)s; (void)w; (void)wrap; return (rde_vec_2F){ 0.0f, 0.0f }; }
// The UI's shapes (draw.c's cards and chips, select.c's ticks): nothing to draw
// in a test. Weak, as some tests define their own.
__attribute__((weak)) void rde_rendering_2d_draw_rounded_rectangle(const rde_vec_2F c, const rde_vec_2F s, f32 r, u32 p, const rde_color col, rde_shader* sh) { (void)c; (void)s; (void)r; (void)p; (void)col; (void)sh; }
__attribute__((weak)) void rde_rendering_2d_draw_rounded_rectangle_with_border(const rde_vec_2F c, const rde_vec_2F s, f32 r, u32 p, const rde_color f, f32 t, const rde_color b, rde_shader* sh) { (void)c; (void)s; (void)r; (void)p; (void)f; (void)t; (void)b; (void)sh; }
__attribute__((weak)) void rde_rendering_2d_draw_circle_with_border(const rde_vec_2F c, f32 r, u32 n, const rde_color f, f32 t, const rde_color b, rde_shader* sh) { (void)c; (void)r; (void)n; (void)f; (void)t; (void)b; (void)sh; }
// The window (draw.c's --text-check measures against it): a tablet's, in a test.
__attribute__((weak)) rde_vec_2I rde_window_get_size(const rde_window* w) { (void)w; return (rde_vec_2I){ 744, 1133 }; }
// Shaped text (draw.c's labels, since right-to-left: the text engine's runs):
// nothing shaped, nothing drawn, in a test.
__attribute__((weak)) rde_text_engine_shaped_text rde_text_engine_shape_text(rde_font* f, const rde_text_engine_shaping_config* c, const c8* s) { (void)f; (void)c; (void)s; rde_text_engine_shaped_text t; memset(&t, 0, sizeof t); return t; }
__attribute__((weak)) void rde_text_engine_free_shaped_text(rde_text_engine_shaped_text* t) { (void)t; }
__attribute__((weak)) void rde_rendering_2d_draw_shaped_text_3(rde_font* f, const rde_text_engine_shaped_text* t, rde_vec_3F p, rde_vec_2F s, f32 r, rde_color c, rde_shader* sh, any x) { (void)f; (void)t; (void)p; (void)s; (void)r; (void)c; (void)sh; (void)x; }
// The locale's digits (draw.c's numbers): 0-9 in the tests' languages. Weak, as text.c has it too.
__attribute__((weak)) usize rde_localization_localize_digits(c8* _io, usize _capacity) { (void)_capacity; return strlen(_io); }
