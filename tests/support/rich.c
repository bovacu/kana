#include "rde.h"
// Text measuring is the engine's (a font, Slug): the tests have no font, so draw.c's
// estimates fall back to their defaults.
rde_vec_2F rde_rich_text_measure(const c8* m, rde_font* f, f32 s, f32 w, b8 wrap) { (void)m; (void)f; (void)s; (void)w; (void)wrap; return (rde_vec_2F){ 0.0f, 0.0f }; }
// The UI's shapes (draw.c's cards and chips, select.c's ticks): nothing to draw
// in a test. Weak, as some tests define their own.
__attribute__((weak)) void rde_rendering_2d_draw_rounded_rectangle(const rde_vec_2F c, const rde_vec_2F s, f32 r, u32 p, const rde_color col, rde_shader* sh) { (void)c; (void)s; (void)r; (void)p; (void)col; (void)sh; }
__attribute__((weak)) void rde_rendering_2d_draw_rounded_rectangle_with_border(const rde_vec_2F c, const rde_vec_2F s, f32 r, u32 p, const rde_color f, f32 t, const rde_color b, rde_shader* sh) { (void)c; (void)s; (void)r; (void)p; (void)f; (void)t; (void)b; (void)sh; }
__attribute__((weak)) void rde_rendering_2d_draw_circle_with_border(const rde_vec_2F c, f32 r, u32 n, const rde_color f, f32 t, const rde_color b, rde_shader* sh) { (void)c; (void)r; (void)n; (void)f; (void)t; (void)b; (void)sh; }
