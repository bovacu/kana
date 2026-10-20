// The canvas's drawing, for the suites that build the canvas's code without it (draw.c, render.c: nothing is drawn in
// them): the text's width for the instruments' numbers, the lasso's drawing — as nothing, or a width by its letters.

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/render.h"
#include <string.h>

f32 fude_draw_text_width(rde_font* _font, f32 _font_px, const c8* _text, f32 _px) { (void)_font; (void)_font_px; return (f32)strlen(_text) * _px * 0.5f; }
u32 fude_draw_text_wrap(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, f32 _width, f32 _line, rde_color _color) { (void)_font; (void)_font_px; (void)_text; (void)_x; (void)_y; (void)_px; (void)_width; (void)_line; (void)_color; return 1u; }

// (weak: a suite built with render.c, as the zoom suite is, has its own; the others draw nothing)
__attribute__((weak, visibility("hidden"))) void fude_zoom_render_object(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half) { (void)_r; (void)_s; (void)_object; (void)_to_screen; (void)_half; }
__attribute__((weak, visibility("hidden"))) void fude_zoom_render_glow(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, f32 _extra, rde_color _color) { (void)_r; (void)_s; (void)_object; (void)_to_screen; (void)_half; (void)_extra; (void)_color; }
__attribute__((weak, visibility("hidden"))) u32 fude_zoom_render_editable(const fude_zoom_renderer* _r, const fude_zoom_scene* _s, rde_arr* _out) { (void)_r; (void)_s; (void)_out; return 0; }
// A line of text drawn (render_text.c's plain texts, in the zoom suite): nothing.
__attribute__((weak, visibility("hidden"))) void fude_draw_text(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, rde_color _color) {
    (void)_font; (void)_font_px; (void)_text; (void)_x; (void)_y; (void)_px; (void)_color;
}
// A text the canvas places itself (draw.h: fude_draw_text_placed, which draws it from its kept shape): nothing.
__attribute__((weak, visibility("hidden"))) void fude_draw_text_placed(rde_font* _font, const c8* _text, rde_vec_3F _position, rde_vec_2F _scale, f32 _rotation_deg, rde_color _color) {
    (void)_font; (void)_text; (void)_position; (void)_scale; (void)_rotation_deg; (void)_color;
}
// Text in a box (render_text.c, in the zoom suite): one line, the whole text, as fude_draw_text_wrap's stand-in has it.
__attribute__((weak, visibility("hidden"))) u32 fude_draw_text_wrap_lines(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width) { (void)_font; (void)_font_px; (void)_text; (void)_px; (void)_width; return 1u; }
__attribute__((weak, visibility("hidden"))) u32 fude_draw_text_wrap_spans(rde_font* _font, f32 _font_px, const c8* _text, f32 _px, f32 _width, u32* _from, u32* _to, u32 _max) {
    (void)_font; (void)_font_px; (void)_px; (void)_width;
    if(_max == 0u) { return 0u; }
    _from[0] = 0u; _to[0] = (u32)strlen(_text);
    return 1u;
}
void rde_rendering_2d_draw_line_segmented(rde_vec_2F _init, rde_vec_2F _end, rde_color _color, f32 _thickness, f32 _segment_len, f32 _gap) { (void)_init; (void)_end; (void)_color; (void)_thickness; (void)_segment_len; (void)_gap; }
