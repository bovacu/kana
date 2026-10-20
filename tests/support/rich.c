#include "rde.h"
#include <string.h>
// Text measuring is the engine's (a font, Slug): the tests have no font, so draw.c's
// estimates fall back to their defaults. Weak: a suite measuring its own way (tests/draw/test.c) defines its own.
__attribute__((weak, visibility("hidden"))) rde_vec_2F rde_rich_text_measure(const c8* m, rde_font* f, f32 s, f32 w, b8 wrap) { (void)m; (void)f; (void)s; (void)w; (void)wrap; return (rde_vec_2F){ 0.0f, 0.0f }; }
// The UI's shapes (draw.c's cards and chips, select.c's ticks): nothing to draw
// in a test. Weak, as some tests define their own.
__attribute__((weak, visibility("hidden"))) void rde_rendering_2d_draw_rounded_rectangle(const rde_vec_2F c, const rde_vec_2F s, f32 r, u32 p, const rde_color col, rde_shader* sh) { (void)c; (void)s; (void)r; (void)p; (void)col; (void)sh; }
__attribute__((weak, visibility("hidden"))) void rde_rendering_2d_draw_rounded_rectangle_with_border(const rde_vec_2F c, const rde_vec_2F s, f32 r, u32 p, const rde_color f, f32 t, const rde_color b, rde_shader* sh) { (void)c; (void)s; (void)r; (void)p; (void)f; (void)t; (void)b; (void)sh; }
__attribute__((weak, visibility("hidden"))) void rde_rendering_2d_draw_circle_with_border(const rde_vec_2F c, f32 r, u32 n, const rde_color f, f32 t, const rde_color b, rde_shader* sh) { (void)c; (void)r; (void)n; (void)f; (void)t; (void)b; (void)sh; }
// The window (draw.c's --text-check measures against it): a tablet's, in a test.
__attribute__((weak, visibility("hidden"))) rde_vec_2I rde_window_get_size(const rde_window* w) { (void)w; return (rde_vec_2I){ 744, 1133 }; }
// Shaped text (draw.c draws every text from its kept shape): the tests have no font, so a text's shape stands in as
// one line for each of its lines and one glyph for each of its bytes, the byte as its glyph index, every box empty;
// on the std allocator. fude_test_shape makes one (a suite that counts the shaping calls it from its own
// rde_text_engine_shape_text), and fude_test_shaped_string reads back the text a shape was made from (for a suite's
// own rde_rendering_2d_draw_shaped_text_2, to see what is drawn).
rde_text_engine_shaped_text fude_test_shape(rde_font* f, const c8* s) {
    rde_text_engine_shaped_text t;
    memset(&t, 0, sizeof t);
    t.lines = rde_arr_new(sizeof(rde_text_engine_shaped_line), rde_memory_allocator_get_default_std());
    for(const c8* p = s;; p++) {
        rde_text_engine_shaped_line line;
        memset(&line, 0, sizeof line);
        line.chars = rde_arr_new(sizeof(rde_text_engine_shaped_char), rde_memory_allocator_get_default_std());
        for(; *p != 0 && *p != '\n'; p++) {
            rde_text_engine_shaped_char ch;
            memset(&ch, 0, sizeof ch);
            ch.glyph_index = (u8)*p;
            ch.font        = f;
            rde_arr_add(&line.chars, &ch);
        }
        rde_arr_add(&t.lines, &line);
        if(*p == 0) break;
    }
    return t;
}
// Writes the text shape t was made from into out (size bytes, cut to fit); returns its whole length.
usize fude_test_shaped_string(const rde_text_engine_shaped_text* t, c8* out, usize size) {
    usize n = 0;
    for(u32 l = 0; rde_arr_is_inited(&t->lines) && l < (u32)rde_arr_length(&t->lines); l++) {
        const rde_text_engine_shaped_line* line = (const rde_text_engine_shaped_line*)rde_arr_at(&t->lines, l);
        if(l > 0) { if(n + 1 < size) out[n] = '\n'; n++; }
        for(u32 k = 0; k < (u32)rde_arr_length(&line->chars); k++) {
            if(n + 1 < size) out[n] = (c8)((const rde_text_engine_shaped_char*)rde_arr_at(&line->chars, k))->glyph_index;
            n++;
        }
    }
    if(size > 0) out[n < size ? n : size - 1] = 0;
    return n;
}
__attribute__((weak, visibility("hidden"))) rde_text_engine_shaped_text rde_text_engine_shape_text(rde_font* f, const rde_text_engine_shaping_config* c, const c8* s) { (void)c; return fude_test_shape(f, s); }
__attribute__((weak, visibility("hidden"))) void rde_text_engine_free_shaped_text(rde_text_engine_shaped_text* t) {
    if(!rde_arr_is_inited(&t->lines)) return;
    for(u32 l = 0; l < (u32)rde_arr_length(&t->lines); l++) rde_arr_free(&((rde_text_engine_shaped_line*)rde_arr_at(&t->lines, l))->chars);
    rde_arr_free(&t->lines);
}
// The locale's digits (draw.c's numbers): 0-9 in the tests' languages. Weak, as text.c has it too.
__attribute__((weak, visibility("hidden"))) usize rde_localization_localize_digits(c8* _io, usize _capacity) { (void)_capacity; return strlen(_io); }
