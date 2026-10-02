#include "study/screens/scan.h"
#include "drawing/base/theme.h"
#include "drawing/base/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
static u32 g_polys = 0, g_borders = 0, g_textures = 0, g_uploads = 0;
static f32 g_last_rotation = 0.0f;
static rde_vec_2F g_last_scale = { 0, 0 };
static rde_texture* g_tex = (rde_texture*)0x1234;
void rde_rendering_2d_draw_stroke(const rde_vec_2F* p, const f32* r, u32 n, rde_color c) { (void)p; (void)r; (void)n; (void)c; }
void rde_rendering_2d_draw_circle(const rde_vec_2F p, f32 r, u32 s, const rde_color c, rde_shader* sh) { (void)p; (void)r; (void)s; (void)c; (void)sh; }
void rde_rendering_2d_draw_rectangle(const rde_vec_2F a, const rde_vec_2F b, const rde_color c) { (void)a; (void)b; (void)c; }
void rde_rendering_2d_draw_text_2(rde_font* f, const c8* t, rde_vec_3F p, rde_vec_2F s, f32 r, rde_color c) { (void)f; (void)t; (void)p; (void)s; (void)r; (void)c; }
void rde_rendering_2d_draw_polygon_with_border(const rde_vec_2F* p, u32 n, const rde_color f, f32 t, const rde_color b, rde_shader* sh) { (void)p; (void)n; (void)f; (void)t; (void)b; (void)sh; g_polys++; }
void rde_rendering_2d_draw_polygon_border(const rde_vec_2F* p, u32 n, f32 t, const rde_color c, rde_shader* sh) { (void)p; (void)n; (void)t; (void)c; (void)sh; g_borders++; }
void rde_rendering_2d_draw_texture_2(const rde_texture* t, rde_vec_3F p, rde_vec_2F s, f32 r, rde_color c) { (void)t; (void)p; (void)c; g_textures++; g_last_rotation = r; g_last_scale = s; }
rde_texture* rde_texture_load_from_memory(const u8* b, usize n, const c8* d, const rde_texture_parameters* p, rde_memory_allocator* a) { (void)b; (void)d; (void)p; (void)a; return n > 0 ? g_tex : NULL; }
void rde_texture_unload(rde_texture* t) { (void)t; }
RDE_MOBILE_PERMISSION_STATUS_ rde_mobile_get_permission_status(RDE_MOBILE_PERMISSION_ p) { (void)p; return RDE_MOBILE_PERMISSION_STATUS_GRANTED; }
void rde_mobile_request_permission(RDE_MOBILE_PERMISSION_ p, rde_mobile_permission_callback cb, any u) { (void)p; (void)cb; (void)u; }
rde_vec_2I rde_window_get_size(const rde_window* w) { (void)w; return (rde_vec_2I){ 744, 1133 }; }
rde_vec_4I rde_window_get_safe_area_insets(const rde_window* w) { (void)w; return (rde_vec_4I){ 0, 24, 0, 20 }; }
f64 rde_engine_get_time_now(void) { return 1000.0; }
void rde_memcpy(any d, const any s, usize n) { memcpy(d, s, n); }

// A memory texture is its size and pixels here; its "texture" is itself.
struct rde_memory_texture { u32 w, h; u8* pixels; };
rde_memory_texture* rde_memory_texture_create(u32 w, u32 h, u32 c, rde_memory_allocator* a) { (void)a; rde_memory_texture* m = calloc(1, sizeof *m); m->w = w; m->h = h; m->pixels = calloc((usize)w * h * c, 1); return m; }
void rde_memory_texture_destroy(rde_memory_texture* m) { free(m->pixels); free(m); }
u8* rde_memory_texture_get_pixels(const rde_memory_texture* m) { return m->pixels; }
const rde_texture* rde_memory_texture_get_texture(const rde_memory_texture* m) { return (const rde_texture*)m; }
void rde_memory_texture_gpu_upload(rde_memory_texture* m, const rde_texture_parameters* p) { (void)m; (void)p; g_uploads++; }
rde_vec_2UI rde_texture_get_size(const rde_texture* t) {
    if(t == g_tex) return (rde_vec_2UI){ 1000, 500 };
    if(t == (const rde_texture*)0x9abc) return (rde_vec_2UI){ 528, 48 };   // the badge, at 3x
    const rde_memory_texture* m = (const rde_memory_texture*)t; return (rde_vec_2UI){ m->w, m->h };
}
// A back camera giving 1920 x 1080 frames that need a quarter turn clockwise (an iPad held upright).
static b8  g_cam_open = false;
static u32 g_cam_frames = 0;
static rde_vec_2UI g_cam_size = { 0, 0 };
static b8 g_cam_subsystem = false;
b8 rde_engine_enable_subsystem(RDE_SUBSYSTEM_ s, b8 on) { CHECK(s == RDE_SUBSYSTEM_CAMERA); g_cam_subsystem = on; return true; }
b8 rde_engine_is_subsystem_enabled(RDE_SUBSYSTEM_ s) { (void)s; return g_cam_subsystem; }
u32 rde_device_camera_get_count(void) { CHECK(g_cam_subsystem); return 2; }
RDE_DEVICE_CAMERA_POSITION_ rde_device_camera_get_position_at(u32 i) { return i == 1 ? RDE_DEVICE_CAMERA_POSITION_BACK : RDE_DEVICE_CAMERA_POSITION_FRONT; }
rde_device_camera* rde_device_camera_open(u32 i, u32 w, u32 h, rde_memory_allocator* a) { (void)w; (void)h; (void)a; CHECK(i == 1); g_cam_open = true; return (rde_device_camera*)0x5678; }
void rde_device_camera_close(rde_device_camera* c) { (void)c; g_cam_open = false; g_cam_size = (rde_vec_2UI){ 0, 0 }; }
RDE_DEVICE_CAMERA_PERMISSION_ rde_device_camera_get_permission(rde_device_camera* c) { (void)c; return RDE_DEVICE_CAMERA_PERMISSION_APPROVED; }
rde_vec_2UI rde_device_camera_get_size(const rde_device_camera* c) { (void)c; return g_cam_size; }
f32 rde_device_camera_get_rotation(const rde_device_camera* c) { (void)c; return 90.0f; }
b8 rde_device_camera_update_texture(rde_device_camera* c, rde_memory_texture* m) {
    (void)c; g_cam_size = (rde_vec_2UI){ 1920, 1080 };
    if(m->w != 1920 || m->h != 1080) return false;   // the first frame only tells the size
    g_cam_frames++; return true;
}

static void tap(fude_scan* s, rde_vec_2F at) { fude_scan_pointer_down(s, at, 1.0); fude_scan_pointer_up(s, 1.05); }
// Translate with Google: Google's badge (a texture), the panel's clip and lines.
static rde_texture* g_badge = (rde_texture*)0x9abc;
static u32 g_badge_loads = 0, g_clips = 0, g_lines = 0;
rde_texture* rde_texture_load(const c8* f, rde_memory_allocator* a) { (void)a; CHECK(strstr(f, "powered-by-google") != NULL); g_badge_loads++; return g_badge; }
void rde_rendering_begin_clipping_rect(const rde_window* w, rde_vec_2I c, rde_vec_2UI s) { (void)w; (void)c; (void)s; g_clips++; }
void rde_rendering_end_clipping_rect(void) {}
void rde_rendering_2d_draw_line(const rde_vec_2F a, rde_vec_2F b, rde_color c) { (void)a; (void)b; (void)c; g_lines++; }

int main(void) {
    fude_scan s; fude_scan_init(&s);
    fude_scan_open(&s, NULL, (rde_vec_2F){ 10, 20 });
    CHECK(s.open && s.stage == FUDE_SCAN_EMPTY && fude_scan_kept(&s) == 0);
    c8 out[512];
    fude_scan_write(&s); CHECK(!fude_scan_take_text(&s, out, sizeof out));   // nothing to write

    // A 1000 x 500 photo, three lines (the middle one standing on end: vertical text).
    static fude_textscan_line lines[3];
    snprintf(lines[0].text, sizeof lines[0].text, "今日は");
    lines[0].corners[0] = (rde_vec_2F){ 100, 50 };  lines[0].corners[1] = (rde_vec_2F){ 500, 50 };
    lines[0].corners[2] = (rde_vec_2F){ 500, 120 }; lines[0].corners[3] = (rde_vec_2F){ 100, 120 };
    snprintf(lines[1].text, sizeof lines[1].text, "縦書き");
    lines[1].corners[0] = (rde_vec_2F){ 800, 50 };  lines[1].corners[1] = (rde_vec_2F){ 860, 50 };
    lines[1].corners[2] = (rde_vec_2F){ 860, 450 }; lines[1].corners[3] = (rde_vec_2F){ 800, 450 };
    snprintf(lines[2].text, sizeof lines[2].text, "日本語");
    lines[2].corners[0] = (rde_vec_2F){ 100, 300 }; lines[2].corners[1] = (rde_vec_2F){ 600, 300 };
    lines[2].corners[2] = (rde_vec_2F){ 600, 380 }; lines[2].corners[3] = (rde_vec_2F){ 100, 380 };
    lines[2].block = 1;
    const u8 jpeg[4] = { 1, 2, 3, 4 };
    const fude_textscan_result r = { jpeg, 4, 1000, 500, lines, 3 };
    fude_scan_show(&s, &r);
    CHECK(s.stage == FUDE_SCAN_RESULT && fude_scan_kept(&s) == 3);

    const f32 top = 566.5f - 24.0f - 8.0f, bottom = -566.5f + 20.0f + 76.0f + 24.0f;
    fude_scan_render(&s, NULL, NULL, 32.0f, top, bottom);
    CHECK(g_textures == 1 && g_polys == 3 && g_borders == 0 && g_last_rotation == 0.0f);
    printf("photo at (%.0f, %.0f), scale %.3f\n", (f64)s.picture_tl.x, (f64)s.picture_tl.y, (f64)s.picture_scale);
    CHECK(s.picture_scale > 0.6f && s.picture_scale < 0.75f);   // 696 points wide for 1000 pixels

    // Tap the vertical line: left out; the photo between lines: nothing.
    const rde_vec_2F mid_v = { s.picture_tl.x + 830.0f * s.picture_scale, s.picture_tl.y - 250.0f * s.picture_scale };
    tap(&s, mid_v);
    CHECK(fude_scan_kept(&s) == 2 && !((fude_scan_line*)s.lines.memory)[1].kept);
    tap(&s, (rde_vec_2F){ s.picture_tl.x + 700.0f * s.picture_scale, s.picture_tl.y - 200.0f * s.picture_scale });
    CHECK(fude_scan_kept(&s) == 2);
    g_polys = g_borders = 0; fude_scan_render(&s, NULL, NULL, 32.0f, top, bottom);
    CHECK(g_polys == 2 && g_borders == 1);

    // Write: the kept lines, in order, one a line; once.
    fude_scan_write(&s);
    CHECK(fude_scan_take_text(&s, out, sizeof out) && strcmp(out, "今日は\n日本語") == 0);
    CHECK(!fude_scan_take_text(&s, out, sizeof out));
    // Taken back in: all three.
    tap(&s, mid_v);
    fude_scan_write(&s);
    CHECK(fude_scan_take_text(&s, out, sizeof out) && strcmp(out, "今日は\n縦書き\n日本語") == 0);

    // A photo with no text, and one that cannot be shown.
    const fude_textscan_result none = { jpeg, 4, 1000, 500, lines, 0 };
    fude_scan_show(&s, &none); CHECK(s.stage == FUDE_SCAN_RESULT && fude_scan_kept(&s) == 0);
    fude_scan_write(&s); CHECK(!fude_scan_take_text(&s, out, sizeof out));
    const fude_textscan_result broken = { NULL, 0, 1000, 500, lines, 3 };
    fude_scan_show(&s, &broken); CHECK(s.stage == FUDE_SCAN_EMPTY && s.message == FUDE_TEXT_SCAN_FAILED);

    // The library is not here (the desktop): nothing waits.
    fude_scan_photos(&s); fude_scan_update(&s, 0.016f);
    CHECK(s.stage != FUDE_SCAN_WAITING);

    // Live: the back camera; the first frame tells the size, then frames come,
    // shown a quarter turn clockwise — upright, the picture is 1080 x 1920.
    fude_scan_camera(&s);
    CHECK(s.stage == FUDE_SCAN_LIVE && g_cam_open && s.camera != NULL);
    fude_scan_update(&s, 0.016f);   // the probe: the size
    CHECK(g_cam_frames == 0 && s.shown == NULL);
    fude_scan_update(&s, 0.016f);   // a frame texture that size
    fude_scan_update(&s, 0.016f);
    CHECK(g_cam_frames >= 1 && g_uploads >= 1 && s.shown != NULL && s.shown_rotation == 90.0f);
    CHECK(s.picture_w == 1080 && s.picture_h == 1920);
    g_textures = 0; fude_scan_render(&s, NULL, NULL, 32.0f, top, bottom);
    // RDE turns clockwise for a positive angle (checked on screen); the camera's
    // rows come top first, mirrored back by a negative height.
    CHECK(g_textures == 1 && g_last_rotation == 90.0f && g_last_scale.y < 0.0f && g_last_scale.x > 0.0f);
    CHECK(fude_scan_kept(&s) == 0);                        // live: nothing to write yet
    fude_scan_write(&s); CHECK(!fude_scan_take_text(&s, out, sizeof out));
    // Hold: the camera stops, the frame stays.
    CHECK(g_cam_subsystem);                                // on while live
    fude_scan_hold(&s);
    CHECK(s.stage == FUDE_SCAN_RESULT && !g_cam_open && s.camera == NULL && s.shown != NULL);
    CHECK(!g_cam_subsystem);                               // and off again
    // Live again, then the app goes to the background: held, camera off.
    fude_scan_camera(&s); CHECK(s.stage == FUDE_SCAN_LIVE && g_cam_open);
    fude_scan_pause(&s);  CHECK(!g_cam_open && s.stage != FUDE_SCAN_LIVE);
    // Back closes everything.
    fude_scan_camera(&s); CHECK(g_cam_open);
    fude_scan_close(&s); CHECK(!s.open && !g_cam_open && s.frame == NULL);
    fude_scan_destroy(&s);
    // Translate with Google (translate.h pretending, as the desktop's look does):
    // a photo of six lines; on, they are asked for four at a time, in order.
    fude_translate_demo(true);
    static fude_textscan_line six[6];
    for(u32 i = 0; i < 6u; i++) {
        snprintf(six[i].text, sizeof six[i].text, "行%u", i);
        const f32 y = 40.0f + 70.0f * (f32)i;
        six[i].corners[0] = (rde_vec_2F){ 100, y };      six[i].corners[1] = (rde_vec_2F){ 600, y };
        six[i].corners[2] = (rde_vec_2F){ 600, y + 50 }; six[i].corners[3] = (rde_vec_2F){ 100, y + 50 };
    }
    fude_scan_init(&s);
    fude_scan_open(&s, NULL, (rde_vec_2F){ 0, 0 });
    const fude_textscan_result r6 = { jpeg, 4, 1000, 500, six, 6 };
    fude_scan_show(&s, &r6);
    CHECK(!fude_scan_translating(&s));
    fude_scan_update(&s, 0.016f);
    CHECK(((fude_scan_line*)s.lines.memory)[0].translated == FUDE_SCAN_TRANSLATION_NONE);   // off: nothing asked
    g_textures = 0; fude_scan_render(&s, NULL, NULL, 32.0f, top, bottom);
    const f32 full_scale = s.picture_scale;
    fude_scan_translate(&s); CHECK(fude_scan_translating(&s));
    fude_scan_update(&s, 0.016f);
    fude_scan_line* L = (fude_scan_line*)s.lines.memory;
    CHECK(s.translate_asked == 4 && L[3].translated == FUDE_SCAN_TRANSLATION_ASKED && L[4].translated == FUDE_SCAN_TRANSLATION_NONE);
    fude_scan_update(&s, 0.016f);   // the four answers in, the last two asked
    CHECK(L[0].translated == FUDE_SCAN_TRANSLATION_DONE && strstr(L[0].translation, "行0") != NULL);
    CHECK(L[4].translated == FUDE_SCAN_TRANSLATION_ASKED && L[5].translated == FUDE_SCAN_TRANSLATION_ASKED);
    fude_scan_update(&s, 0.016f);
    u32 done = 0, empty = 0;
    for(u32 i = 0; i < 6u; i++) { done += L[i].translated == FUDE_SCAN_TRANSLATION_DONE; empty += L[i].translation[0] == 0; }
    CHECK(done == 6 && empty == 1 && s.translate_asked == 0);   // the demo fails every fifth: "could not be translated"
    printf("translation: \"%s\"\n", L[0].translation);

    // Drawn: the picture over the panel, Google's badge in it, the rows clipped.
    g_textures = 0; g_clips = 0; g_lines = 0;
    fude_scan_render(&s, NULL, NULL, 32.0f, top, bottom);
    CHECK(g_textures == 2 && g_badge_loads == 1 && g_clips == 1 && g_lines >= 1);
    CHECK(s.picture_tl.y - 500.0f * s.picture_scale > s.panel_rows_top);   // the picture above the panel, whole
    CHECK(s.panel_view > 100.0f && s.panel_content > 0.0f);
    CHECK(L[0].row_top > L[0].row_bottom && L[1].row_top <= L[0].row_bottom + 0.01f);
    printf("panel: view %.0f, content %.0f, picture scale %.3f (was %.3f)\n", (f64)s.panel_view, (f64)s.panel_content, (f64)s.picture_scale, (f64)full_scale);
    // A tap on a row leaves its line out (and back in).
    const rde_vec_2F row1 = { s.panel_left + 40.0f, (L[1].row_top + L[1].row_bottom) * 0.5f };
    tap(&s, row1); CHECK(!L[1].kept && fude_scan_kept(&s) == 5);
    tap(&s, row1); CHECK(L[1].kept && fude_scan_kept(&s) == 6);
    // Write still writes the Japanese.
    fude_scan_write(&s); CHECK(fude_scan_take_text(&s, out, sizeof out) && strncmp(out, "行0\n行1", strlen("行0\n行1")) == 0);
    // Off: the panel goes, the picture has the space again.
    fude_scan_translate(&s); CHECK(!fude_scan_translating(&s));
    g_textures = 0; fude_scan_render(&s, NULL, NULL, 32.0f, top, bottom);
    CHECK(g_textures == 1 && s.picture_scale == full_scale);   // (this photo is as wide as the screen: the same scale either way)
    fude_scan_destroy(&s);
    fude_translate_demo(false);

    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
