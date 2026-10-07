// The canvas's drawing, for the suites that build the canvas's code without it (draw.c, render.c: nothing is drawn in
// them): the text's width for the instruments' numbers, the lasso's drawing — as nothing, or a width by its letters.

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/render.h"
#include <string.h>

f32 fude_draw_text_width(rde_font* _font, f32 _font_px, const c8* _text, f32 _px) { (void)_font; (void)_font_px; return (f32)strlen(_text) * _px * 0.5f; }
u32 fude_draw_text_wrap(rde_font* _font, f32 _font_px, const c8* _text, f32 _x, f32 _y, f32 _px, f32 _width, f32 _line, rde_color _color) { (void)_font; (void)_font_px; (void)_text; (void)_x; (void)_y; (void)_px; (void)_width; (void)_line; (void)_color; return 1u; }

void fude_zoom_render_object(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half) { (void)_r; (void)_s; (void)_object; (void)_to_screen; (void)_half; }
void fude_zoom_render_glow(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, f32 _extra, rde_color _color) { (void)_r; (void)_s; (void)_object; (void)_to_screen; (void)_half; (void)_extra; (void)_color; }
u32 fude_zoom_render_editable(const fude_zoom_renderer* _r, const fude_zoom_scene* _s, rde_arr* _out) { (void)_r; (void)_s; (void)_out; return 0; }
void rde_rendering_2d_draw_line_segmented(rde_vec_2F _init, rde_vec_2F _end, rde_color _color, f32 _thickness, f32 _segment_len, f32 _gap) { (void)_init; (void)_end; (void)_color; (void)_thickness; (void)_segment_len; (void)_gap; }
