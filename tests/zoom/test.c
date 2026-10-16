// The deep-zoom canvas's models (fude/zoom): the stroke codec, the index, frames
// and the camera, the erasers, the undo history, shapes, pictures, and the file — every byte
// offset of a file cut off, and it still opens to the last state it held.
#include "zoom/codec.h"
#include "zoom/index.h"
#include "zoom/scene.h"
#include "zoom/erase.h"
#include "zoom/zfile.h"
#include "zoom/smooth.h"
#include "zoom/nav.h"
#include "zoom/fill.h"
#include "zoom/export.h"
#include "zoom/instrument.h"
#include "zoom/connect.h"
#include "zoom/bucket.h"
#include "zoom/shape.h"
#include "zoom/video.h"
#include "zoom/handfind.h"
#include "zoom/pdf.h"
#include "zoom/map.h"
#include "zoom/piece.h"
#include "zoom/sheet.h"
#include "zoom/stl.h"
#include "zoom/trim.h"
#include "zoom/examples.h"
#include "zoom/placer.h"
#include "zoom/circuit.h"
#include "zoom/mech.h"
#include "zoom/plan.h"
#include "zoom/props.h"
#include "zoom/product.h"
#include "zoom/pages.h"
#include "zoom/words.h"
#include "zoom/homelist.h"
#include "zoom/inktest.h"
#include "zoom/adpcm.h"
#include "zoom/audio.h"
#include "zoom/voice.h"
#include "zoom/transcript.h"
#include "zoom/tex.h"
#include "zoom/formula.h"
#include "zoom/texcalc.h"
#include "zoom/handmath.h"
#include "zoom/mathread.h"
#include "sim/body.h"
#include "sim/sparse.h"
#include "zoom/calc.h"
#include "zoom/plot.h"
#include "zoom/symbol.h"
#include "zoom/rich.h"
#include "zoom/paper.h"
#include "zoom/chart.h"
#include "zoom/plot.h"
#include "drawing/ink/canvas.h"
#include "zoom/display.h"
#include "zoom/select.h"
#include "zoom/render.h"
#include "zoom/render_parts.h"   // (the renderer's own interface: its caches looked into, a shape drawn alone)
#include "zoom/cut.h"
#include "zoom/snap.h"
#include "zoom/nest.h"
#include "zoom/logic.h"
#include "zoom/custom.h"
#include "zoom/limits.h"
#include "zoom/props.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static int fails = 0;

static f64 fude_zoom_select_wrap_test(f64 a) {
    while(a > 3.14159265358979323846)  { a -= 2.0 * 3.14159265358979323846; }
    while(a < -3.14159265358979323846) { a += 2.0 * 3.14159265358979323846; }
    return a;
}
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

static u32 rng = 12345u;
static u32 rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static f64 rndf(void) { return (f64)(rnd() & 0xFFFFFF) / (f64)0xFFFFFF; }

// --- codec ------------------------------------------------------------------------------------

// A pen-like stroke: a smooth curve sampled at a steady rate, a little jitter.
static void pen_stroke(fude_zoom_qpoint* q, u32 n, f64 scale) {
    f64 x = 0, y = 0, a = rndf() * 6.28, v = 3.0 + rndf() * 4.0;
    for(u32 i = 0; i < n; i++) {
        q[i].x = (i32)llround(x * scale); q[i].y = (i32)llround(y * scale);
        q[i].pressure = (u16)(400 + 300 * sin(i * 0.05)); q[i].time = i * 4u + (rnd() & 1u);
        a += (rndf() - 0.5) * 0.15; x += cos(a) * v; y += sin(a) * v;
    }
    q[0].x = 0; q[0].y = 0;
}

static void test_codec(void) {
    fude_zoom_qpoint in[1200], out[1200];
    const u32 counts[] = { 1, 2, 3, 31, 32, 33, 64, 511, 512, 513, 1200 };
    for(u32 c = 0; c < sizeof(counts) / sizeof(counts[0]); c++) {
        for(u8 ch = 0; ch < 4; ch++) {
            const u32 n = counts[c];
            pen_stroke(in, n, 16.0);
            if(n > 10) { in[n / 2].x += 1 << 20; in[n / 3].y -= 1 << 22; }   // wild values: the escape
            fude_bytes b = fude_bytes_new(64);
            fude_zoom_codec_encode(&b, in, n, ch);
            CHECK(fude_zoom_codec_decode(b.memory, fude_bytes_size(&b), out, n, ch));
            b8 same = true;
            for(u32 i = 0; i < n; i++) {
                if(out[i].x != in[i].x || out[i].y != in[i].y) same = false;
                if((ch & FUDE_ZOOM_CHANNEL_PRESSURE) && out[i].pressure != in[i].pressure) same = false;
                if((ch & FUDE_ZOOM_CHANNEL_TIME) && out[i].time != in[i].time) same = false;
                if(!(ch & FUDE_ZOOM_CHANNEL_PRESSURE) && out[i].pressure != 0) same = false;
            }
            CHECK(same);
            // Cut short: decoding says so instead of reading past the end.
            if(fude_bytes_size(&b) > 2) CHECK(!fude_zoom_codec_decode(b.memory, fude_bytes_size(&b) / 2, out, n, ch));
            rde_arr_free(&b);
        }
    }
    // The size on pen-like strokes, drawn at 1/16 pt quanta: x, y and pressure.
    u64 bytes = 0, points = 0, bytes_t = 0;
    for(u32 k = 0; k < 200; k++) {
        const u32 n = 100 + rnd() % 412;
        pen_stroke(in, n, 16.0);
        fude_bytes b = fude_bytes_new(64);
        fude_zoom_codec_encode(&b, in, n, FUDE_ZOOM_CHANNEL_PRESSURE);
        bytes += fude_bytes_size(&b); points += n;
        rde_arr_clear(&b);
        fude_zoom_codec_encode(&b, in, n, FUDE_ZOOM_CHANNEL_PRESSURE | FUDE_ZOOM_CHANNEL_TIME);
        bytes_t += fude_bytes_size(&b);
        rde_arr_free(&b);
    }
    printf("  codec: %.2f bytes a point (x, y, pressure), %.2f with time\n", (f64)bytes / (f64)points, (f64)bytes_t / (f64)points);
    CHECK((f64)bytes / (f64)points < 3.0);
}

// --- transforms and the index -------------------------------------------------------------------

static void test_sim_and_index(void) {
    const fude_zoom_sim a = fude_zoom_sim_from_xform((fude_zoom_xform){ 12.5, -3.0, 0.25, 0.7 });
    const fude_zoom_sim b = fude_zoom_sim_from_xform((fude_zoom_xform){ -100.0, 40.0, 1.0 / 1024.0, -0.2 });
    const fude_zoom_v2 p = { 3.25, -8.5 };
    const fude_zoom_v2 q = fude_zoom_sim_apply(fude_zoom_sim_compose(a, b), p);
    const fude_zoom_v2 r = fude_zoom_sim_apply(a, fude_zoom_sim_apply(b, p));
    CHECK(fabs(q.x - r.x) < 1e-9 && fabs(q.y - r.y) < 1e-9);
    const fude_zoom_v2 back = fude_zoom_sim_apply(fude_zoom_sim_inverse(a), fude_zoom_sim_apply(a, p));
    CHECK(fabs(back.x - p.x) < 1e-12 && fabs(back.y - p.y) < 1e-12);

    // Inserted one by one, and built packed: both find exactly what brute force does.
    enum { N = 3000 };
    static fude_zoom_box boxes[N]; static u32 values[N];
    for(u32 i = 0; i < N; i++) {
        const f64 x = rndf() * 1e4, y = rndf() * 1e4, s = pow(2.0, rndf() * 12.0) * 0.01;   // sizes over 12 octaves
        boxes[i] = (fude_zoom_box){ x, y, x + s, y + s }; values[i] = i;
    }
    fude_zoom_index inc, packed; fude_zoom_index_init(&inc); fude_zoom_index_init(&packed);
    for(u32 i = 0; i < N; i++) fude_zoom_index_insert(&inc, i, boxes[i]);
    fude_zoom_index_build(&packed, values, boxes, N);
    rde_arr found = rde_arr_new(sizeof(u32), NULL);
    b8 all_same = true;
    for(u32 k = 0; k < 300; k++) {
        const f64 x = rndf() * 1e4, y = rndf() * 1e4, s = rndf() * 800;
        const fude_zoom_box qb = { x, y, x + s, y + s * 0.5 };
        u32 brute = 0;
        for(u32 i = 0; i < N; i++) brute += fude_zoom_box_overlaps(boxes[i], qb) ? 1u : 0u;
        for(u32 t = 0; t < 2; t++) {
            rde_arr_clear(&found);
            fude_zoom_index_query(t == 0 ? &inc : &packed, qb, &found);
            if(rde_arr_length(&found) != brute) all_same = false;
            for(u32 i = 0; i < rde_arr_length(&found); i++) if(!fude_zoom_box_overlaps(boxes[((u32*)found.memory)[i]], qb)) all_same = false;
        }
    }
    CHECK(all_same);
    rde_arr_free(&found);
    fude_zoom_index_destroy(&inc); fude_zoom_index_destroy(&packed);
}

// --- strokes ------------------------------------------------------------------------------------

// A scratch table of n zeroed items, size bytes each (an rde_arr: freed by the caller), its first.
static any table(rde_arr* a, usize size, u32 n) { *a = rde_arr_new(size, NULL); rde_arr_resize(a, n); return a->memory; }

// A straight stroke from a to b in the camera frame, points every `step`, as the page draws one.
static u32 line(fude_zoom_scene* s, f64 ax, f64 ay, f64 bx, f64 by, f64 step, f32 radius) {
    const u32 frame = s->camera.frame;
    const i8  q     = fude_zoom_quantum_for(s->camera.z);
    const f64 g     = ldexp(1.0, q);
    const f64 len   = hypot(bx - ax, by - ay);
    const u32 n     = (u32)(len / step) + 1u;
    rde_arr pa; fude_zoom_qpoint* pts = table(&pa, sizeof(*pts), n + 1);
    for(u32 i = 0; i < n; i++) {
        const f64 t = n > 1 ? (f64)i / (f64)(n - 1) : 0.0;
        pts[i].x = (i32)llround((bx - ax) * t / g); pts[i].y = (i32)llround((by - ay) * t / g);
        pts[i].pressure = 600; pts[i].time = i * 4u;
    }
    const u32 o = fude_zoom_scene_add_stroke(s, frame, (fude_zoom_v2){ ax, ay }, q, pts, n, FUDE_ZOOM_CHANNEL_PRESSURE | FUDE_ZOOM_CHANNEL_TIME,
                                             (rde_color){ 10, 20, 30, 255 }, radius, FUDE_ZOOM_FLAG_FROM_PEN, 0, 0);
    rde_arr_free(&pa);
    fude_zoom_history_push(s, frame, fude_zoom_scene_object(s, o)->box, NULL, 0, &o, 1);
    return o;
}

static b8 alive(const fude_zoom_scene* s, u32 o) { return (fude_zoom_scene_object(s, o)->flags & FUDE_ZOOM_FLAG_ALIVE) != 0; }

// The alive strokes' ends, frame units.
static void ends(const fude_zoom_scene* s, u32 o, fude_zoom_v2* a, fude_zoom_v2* b) {
    const fude_zoom_object* ob = fude_zoom_scene_object(s, o);
    rde_arr qa; fude_zoom_qpoint* q = table(&qa, sizeof(*q), ob->count);
    fude_zoom_scene_points(s, o, q);
    *a = fude_zoom_scene_point_at(ob, &q[0]); *b = fude_zoom_scene_point_at(ob, &q[ob->count - 1]);
    rde_arr_free(&qa);
}

static u32 alive_count(const fude_zoom_scene* s) { return fude_zoom_scene_alive_strokes(s, NULL); }
// Strokes and fills (what a partial erase leaves).
static u32 alive_drawn(const fude_zoom_scene* s) {
    u32 n = 0;
    for(u32 i = 0; i < fude_zoom_scene_object_count(s); i++) {
        const fude_zoom_object* o = fude_zoom_scene_object(s, i);
        n += (o->kind == FUDE_ZOOM_KIND_STROKE || o->kind == FUDE_ZOOM_KIND_FILL) && alive(s, i) ? 1u : 0u;
    }
    return n;
}

// --- frames and the camera ----------------------------------------------------------------------

static void test_frames(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const fude_zoom_v2 half = { 500, 400 };

    // In past 64: a child, the camera at 1/16 in it; out again: the root, the
    // child gone (nothing was drawn in it).
    fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 100.0);
    fude_zoom_camera_settle(&s, half);
    CHECK(s.camera.frame != s.root);
    CHECK(fabs(s.camera.z - 100.0 / 1024.0) < 1e-12);
    const u32 child = s.camera.frame;
    fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 1.0 / 100.0);
    fude_zoom_camera_settle(&s, half);
    CHECK(s.camera.frame == s.root);
    CHECK(fabs(s.camera.z - 1.0) < 1e-12);
    CHECK(fude_zoom_scene_frame(&s, child)->removed);
    // The child's origin sat on the paper's lattice (6.25 parent units for a 1/1024 child).
    CHECK(fmod(fabs(fude_zoom_scene_frame(&s, child)->xf.ox), FUDE_ZOOM_PAPER_ALIGN * FUDE_ZOOM_CHILD_SCALE) < 1e-9);

    // An empty canvas does not zoom out past the bottom (nothing out there).
    fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 1.0 / 1000.0);
    fude_zoom_camera_settle(&s, half);
    CHECK(s.camera.frame == s.root && s.camera.z >= FUDE_ZOOM_Z_MIN);
    fude_zoom_camera_look_at(&s, s.root, (fude_zoom_v2){ 0, 0 }, 1.0);

    // Twenty levels down (10^60), a stroke at each, aimed at the same spot of the
    // screen: each is where it was drawn, seen from the deepest frame and back.
    const u32 levels = 20;
    u32 strokes[21], frames[21];
    fude_zoom_camera_look_at(&s, s.root, (fude_zoom_v2){ 1234.5, -987.25 }, 1.0);
    for(u32 l = 0; l <= levels; l++) {
        const fude_zoom_v2 c = s.camera.at;
        const f64 d = 100.0 / s.camera.z;   // 100 pt across on screen
        strokes[l] = line(&s, c.x - d, c.y + d * 0.3, c.x + d, c.y - d * 0.3, d / 50.0, (f32)(2.0 / s.camera.z));
        frames[l]  = s.camera.frame;
        if(l < levels) {
            // Off to one side a little, then 1024× in.
            fude_zoom_camera_pan(&s, (fude_zoom_v2){ -37.0, 21.0 });
            for(u32 k = 0; k < 10; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&s, half); }
        }
    }
    CHECK(fude_zoom_scene_depth(&s, s.camera.frame) >= levels);
    // Seen from the camera: the deepest stroke is exactly where it was drawn on
    // screen; each level up is 1024× bigger round the same screen spot it had.
    b8 exact = true;
    for(u32 l = 0; l <= levels; l++) {
        fude_zoom_v2 a, b; ends(&s, strokes[l], &a, &b);
        const fude_zoom_sim to_cam = fude_zoom_scene_sim(&s, frames[l], s.camera.frame);
        const fude_zoom_sim back   = fude_zoom_scene_sim(&s, s.camera.frame, frames[l]);
        const fude_zoom_v2  a2     = fude_zoom_sim_apply(back, fude_zoom_sim_apply(to_cam, a));
        const f64 tol = 1e-6 * hypot(b.x - a.x, b.y - a.y);
        if(fabs(a2.x - a.x) > tol || fabs(a2.y - a.y) > tol) exact = false;
    }
    CHECK(exact);
    {
        fude_zoom_v2 a, b; ends(&s, strokes[levels], &a, &b);
        const fude_zoom_v2 sa = fude_zoom_camera_to_screen(&s.camera, a);
        CHECK(fabs(sa.x - (-100.0)) < 0.05 && fabs(sa.y - 30.0) < 0.05);
    }

    // Back out to the top: the camera ends in the root at the zoom it began
    // (the frames it passed through are kept: they have strokes).
    for(u32 k = 0; k < levels * 10; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 0.5); fude_zoom_camera_settle(&s, half); }
    CHECK(s.camera.frame == s.root);
    CHECK(fabs(s.camera.z - 1.0) < 1e-6);
    for(u32 l = 0; l <= levels; l++) CHECK(alive(&s, strokes[l]));

    // Out past the root: a new root above, the old one inside it.
    const u32 old_root = s.root;
    for(u32 k = 0; k < 8; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 0.5); fude_zoom_camera_settle(&s, half); }
    CHECK(s.root != old_root);
    CHECK(fude_zoom_scene_frame(&s, old_root)->parent == s.root);
    CHECK(s.camera.frame == s.root);

    // Later is on top: a stroke inside a child, a big stroke over it from the
    // parent, then into the child again — new strokes go to a new frame on top.
    fude_zoom_camera_look_at(&s, old_root, (fude_zoom_v2){ 5000, 5000 }, 1.0);
    for(u32 k = 0; k < 7; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&s, half); }
    const u32 detail = s.camera.frame;
    CHECK(detail != old_root);
    line(&s, s.camera.at.x - 10, s.camera.at.y, s.camera.at.x + 10, s.camera.at.y, 0.5, 1.0f);
    CHECK(fude_zoom_camera_drawing_frame(&s, (fude_zoom_box){ s.camera.at.x - 1, s.camera.at.y - 1, s.camera.at.x + 1, s.camera.at.y + 1 }) == detail);
    for(u32 k = 0; k < 7; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 0.5); fude_zoom_camera_settle(&s, half); }
    CHECK(s.camera.frame == old_root);
    line(&s, s.camera.at.x - 50, s.camera.at.y, s.camera.at.x + 50, s.camera.at.y, 1.0, 20.0f);   // paints over the detail
    for(u32 k = 0; k < 7; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&s, half); }
    CHECK(s.camera.frame == detail);
    const fude_zoom_box near = { s.camera.at.x - 1, s.camera.at.y - 1, s.camera.at.x + 1, s.camera.at.y + 1 };
    const u32 fresh = fude_zoom_camera_drawing_frame(&s, near);
    CHECK(fresh != detail);
    CHECK(s.camera.frame == fresh);
    CHECK(fude_zoom_scene_object(&s, fude_zoom_scene_frame(&s, fresh)->object)->z > fude_zoom_scene_object(&s, fude_zoom_scene_frame(&s, detail)->object)->z);

    fude_zoom_scene_destroy(&s);
}

// --- erasers and undo ---------------------------------------------------------------------------

static u32 born_alive(const fude_zoom_scene* s, u32 from, fude_zoom_v2* a0, fude_zoom_v2* b0, fude_zoom_v2* a1, fude_zoom_v2* b1) {
    u32 n = 0;
    for(u32 i = from; i < fude_zoom_scene_object_count(s); i++) {
        if(fude_zoom_scene_object(s, i)->kind != FUDE_ZOOM_KIND_STROKE || !alive(s, i)) continue;
        if(n == 0) ends(s, i, a0, b0); else if(n == 1) ends(s, i, a1, b1);
        n++;
    }
    return n;
}

// Is (x, y) inside an alive FILL (its rings and all)?
static b8 filled_at(const fude_zoom_scene* s, f64 x, f64 y) {
    b8 in = false;
    for(u32 i = 0; i < fude_zoom_scene_object_count(s) && !in; i++) {
        const fude_zoom_object* o = fude_zoom_scene_object(s, i);
        if(o->kind != FUDE_ZOOM_KIND_FILL || !alive(s, i)) continue;
        rde_arr qa, pa, ra; fude_zoom_qpoint* q = table(&qa, sizeof(*q), o->count); fude_zoom_v2* p = table(&pa, sizeof(*p), o->count); u32* r = table(&ra, sizeof(u32), o->count);
        fude_zoom_scene_points(s, i, q);
        for(u32 k = 0; k < o->count; k++) { p[k] = fude_zoom_scene_point_at(o, &q[k]); r[k] = q[k].time; }
        in = fude_zoom_fill_inside_rings(p, r, o->count, (fude_zoom_v2){ x, y });
        rde_arr_free(&qa); rde_arr_free(&pa); rde_arr_free(&ra);
    }
    return in;
}

static void test_erasers(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_eraser e; fude_zoom_eraser_init(&e);

    // PARTIAL: a line from x 0 to 100 rubbed at x 50 (radius 2, the line's 1):
    // its outline, a fill, with exactly the disc's sweep taken out.
    const u32 l = line(&s, 0, 0, 100, 0, 1.0, 1.0f);
    const u32 mark = fude_zoom_scene_object_count(&s);
    fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_PARTIAL);
    CHECK(fude_zoom_erase_step(&s, &e, s.camera.frame, (fude_zoom_v2){ 50, -10 }, (fude_zoom_v2){ 50, 10 }, 2.0, 0.5) >= 1);
    fude_zoom_erase_end(&s, &e, s.camera.frame, (fude_zoom_box){ 0, -10, 100, 10 });
    fude_zoom_v2 a0, b0, a1, b1;
    CHECK(!alive(&s, l));
    CHECK(born_alive(&s, mark, &a0, &b0, &a1, &b1) == 0);   // no strokes: its outline
    CHECK(filled_at(&s, 25, 0) && filled_at(&s, 75, 0) && filled_at(&s, 47.5, 0.5) && filled_at(&s, 52.5, -0.5));
    CHECK(!filled_at(&s, 50, 0) && !filled_at(&s, 48.6, 0) && !filled_at(&s, 51.4, 0.9));   // the disc's sweep, x 48 to 52
    CHECK(!filled_at(&s, 25, 1.4) && filled_at(&s, -0.8, 0));   // its width; its round end
    CHECK(alive_drawn(&s) == 1);
    CHECK(fude_zoom_history_undo(&s));
    CHECK(alive(&s, l) && alive_drawn(&s) == 1 && !filled_at(&s, 25, 0));
    CHECK(fude_zoom_history_redo(&s));
    CHECK(!alive(&s, l) && alive_drawn(&s) == 1 && filled_at(&s, 25, 0));

    // A sweep that cuts it twice: one undo brings back the original only.
    const u32 l2 = line(&s, 0, 50, 100, 50, 1.0, 1.0f);
    fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_PARTIAL);
    fude_zoom_erase_step(&s, &e, s.camera.frame, (fude_zoom_v2){ 30, 40 }, (fude_zoom_v2){ 30, 60 }, 2.0, 0.5);
    fude_zoom_erase_step(&s, &e, s.camera.frame, (fude_zoom_v2){ 30, 60 }, (fude_zoom_v2){ 70, 60 }, 2.0, 0.5);
    fude_zoom_erase_step(&s, &e, s.camera.frame, (fude_zoom_v2){ 70, 60 }, (fude_zoom_v2){ 70, 40 }, 2.0, 0.5);
    fude_zoom_erase_end(&s, &e, s.camera.frame, (fude_zoom_box){ 0, 40, 100, 60 });
    CHECK(alive_drawn(&s) == 2);
    CHECK(filled_at(&s, 10, 50) && filled_at(&s, 50, 50) && filled_at(&s, 90, 50) && !filled_at(&s, 30, 50) && !filled_at(&s, 70, 50));
    CHECK(fude_zoom_history_undo(&s));
    CHECK(alive(&s, l2) && alive_drawn(&s) == 2);
    CHECK(fude_zoom_history_redo(&s));
    CHECK(alive_drawn(&s) == 2 && !alive(&s, l2));

    // An "o": its outline keeps the middle open (a hole, not filled in).
    {
        fude_zoom_scene os; fude_zoom_scene_init(&os, 7);
        fude_zoom_qpoint oq[65];
        for(u32 k = 0; k <= 64; k++) {
            const f64 an = 6.283185307179586 * (f64)k / 64.0;
            oq[k] = (fude_zoom_qpoint){ (i32)llround(cos(an) * 30.0 * 16.0), (i32)llround(sin(an) * 30.0 * 16.0), 1023u, 0u };
        }
        const u32 ol = fude_zoom_scene_add_stroke(&os, os.root, (fude_zoom_v2){ 0, 0 }, -4, oq, 65, 0u, (rde_color){ 1, 2, 3, 255 }, 2.0f, 0u, 0, 0);
        fude_zoom_eraser oe; fude_zoom_eraser_init(&oe);
        fude_zoom_erase_begin(&oe, FUDE_ZOOM_ERASE_PARTIAL);
        fude_zoom_erase_step(&os, &oe, os.root, (fude_zoom_v2){ 30, -10 }, (fude_zoom_v2){ 30, 10 }, 3.0, 0.5);   // through the ring's right side
        fude_zoom_erase_end(&os, &oe, os.root, (fude_zoom_box){ 0, 0, 1, 1 });
        CHECK(!alive(&os, ol));
        CHECK(!filled_at(&os, 0, 0) && filled_at(&os, -30, 0) && filled_at(&os, 0, 30) && !filled_at(&os, 30, 0) && !filled_at(&os, -20, 0));
        fude_zoom_eraser_destroy(&oe);
        fude_zoom_scene_destroy(&os);
    }

    // STROKE: a crossing line goes whole.
    const u32 v = line(&s, 20, -50, 20, 100, 1.0, 1.0f);
    const u32 before = alive_count(&s);
    fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_STROKE);
    fude_zoom_erase_step(&s, &e, s.camera.frame, (fude_zoom_v2){ 19, 80 }, (fude_zoom_v2){ 21, 80 }, 1.0, 0.5);
    fude_zoom_erase_end(&s, &e, s.camera.frame, (fude_zoom_box){ 0, 0, 1, 1 });
    CHECK(!alive(&s, v) && alive_count(&s) == before - 1);

    // TRIM: a line crossed at 30 and 70, trimmed at 50: what is left stops at the crossings.
    fude_zoom_scene_destroy(&s); fude_zoom_scene_init(&s, 7);
    const u32 h = line(&s, 0, 0, 100, 0, 1.0, 1.0f);
    line(&s, 30, -20, 30, 20, 1.0, 1.0f);
    line(&s, 70, -20, 70, 20, 1.0, 1.0f);
    const u32 mark2 = fude_zoom_scene_object_count(&s);
    fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_TRIM);
    fude_zoom_erase_step(&s, &e, s.camera.frame, (fude_zoom_v2){ 50, -3 }, (fude_zoom_v2){ 50, 3 }, 1.0, 0.5);
    fude_zoom_erase_end(&s, &e, s.camera.frame, (fude_zoom_box){ 0, 0, 1, 1 });
    CHECK(!alive(&s, h));
    CHECK(born_alive(&s, mark2, &a0, &b0, &a1, &b1) == 2);
    CHECK(fabs(b0.x - 30.0) < 1e-3 && fabs(a1.x - 70.0) < 1e-3);
    // ...and a line with no crossing goes whole.
    const u32 lone = line(&s, 0, 200, 100, 200, 1.0, 1.0f);
    const u32 n_before = alive_count(&s);
    fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_TRIM);
    fude_zoom_erase_step(&s, &e, s.camera.frame, (fude_zoom_v2){ 50, 197 }, (fude_zoom_v2){ 50, 203 }, 1.0, 0.5);
    fude_zoom_erase_end(&s, &e, s.camera.frame, (fude_zoom_box){ 0, 0, 1, 1 });
    CHECK(!alive(&s, lone) && alive_count(&s) == n_before - 1);
    // ...and a crossing on a hidden layer is no crossing (not drawn: a trim stopping there stopped at nothing seen):
    // crossed at 30, and at 70 by a hidden line, trimmed at 50, only 0 to 30 is left.
    {
        const u32 hl = line(&s, 0, 400, 100, 400, 1.0, 1.0f);
        line(&s, 30, 380, 30, 420, 1.0, 1.0f);
        fude_zoom_scene_add_layer(&s, s.home, 2, FUDE_ZOOM_LAYER_HIDDEN, "Hidden");
        s.layer = 2;
        line(&s, 70, 380, 70, 420, 1.0, 1.0f);
        s.layer = 0;
        fude_zoom_scene_layers_refresh(&s);
        const u32 mark3 = fude_zoom_scene_object_count(&s);
        fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_TRIM);
        fude_zoom_erase_step(&s, &e, s.camera.frame, (fude_zoom_v2){ 50, 397 }, (fude_zoom_v2){ 50, 403 }, 1.0, 0.5);
        fude_zoom_erase_end(&s, &e, s.camera.frame, (fude_zoom_box){ 0, 0, 1, 1 });
        CHECK(!alive(&s, hl));
        CHECK(born_alive(&s, mark3, &a0, &b0, &a1, &b1) == 1 && fabs(a0.x) < 1e-3 && fabs(b0.x - 30.0) < 1e-3);
    }

    fude_zoom_eraser_destroy(&e);
    fude_zoom_scene_destroy(&s);
}

// --- the instruments ------------------------------------------------------------------------

// (Its drawing — text, the lasso's glow — tests/support/zoomdraw.c's: nothing drawn in this suite.)

static void test_instruments(void) {
    fude_zoom_instruments ins; fude_zoom_instruments_init(&ins);
    const fude_zoom_v2 half = { 500, 400 };
    CHECK(!fude_zoom_instruments_any(&ins));
    CHECK(fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half) == 0);
    CHECK(fude_zoom_instruments_any(&ins) && ins.tools[0].shown && ins.tools[0].kind == FUDE_ZOOM_INSTRUMENT_RULER);
    ins.tools[0].at = (fude_zoom_v2){ 0, -100 };
    fude_zoom_instrument_edge e[FUDE_ZOOM_INSTRUMENT_EDGES];
    CHECK(fude_zoom_instrument_edges(&ins, 0, e) == 4u);
    CHECK(fabs(e[0].a.x + 280) < 1e-9 && fabs(e[0].a.y + 132) < 1e-9 && fabs(e[0].b.x - 280) < 1e-9);   // its bottom edge

    // The pen near an edge draws along it, never past its end.
    fude_zoom_v2 on;
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 10, -140 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED);
    CHECK(fabs(on.x - 10) < 1e-9 && fabs(on.y + 132) < 1e-9 && fude_zoom_instruments_ruled_straight(&ins));
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 200, -160 });
    CHECK(fabs(on.x - 200) < 1e-9 && fabs(on.y + 132) < 1e-9);
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 400, -140 });
    CHECK(fabs(on.x - 280) < 1e-9);
    f64 along; b8 deg; fude_zoom_v2 lab;
    CHECK(fude_zoom_instruments_ruled(&ins, &along, &deg, &lab) && !deg && fabs(along - 270) < 1e-9);
    fude_zoom_instruments_pen_up(&ins);
    CHECK(!fude_zoom_instruments_ruled(&ins, &along, &deg, &lab));
    // Far from both edges, on its body: the pen moves it.
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ -100, -100 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_HELD);
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ -50, -90 });
    CHECK(fabs(ins.tools[0].at.x - 50) < 1e-9 && fabs(ins.tools[0].at.y + 90) < 1e-9);
    fude_zoom_instruments_pen_up(&ins);
    CHECK(ins.held < 0);
    // Off it: nothing.
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 0, 300 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_NONE);
    fude_zoom_instruments_pen_up(&ins);
    // On its body near an edge it is held (a pencil draws from outside it); a hair inside the edge still draws.
    ins.tools[0].at = (fude_zoom_v2){ 0, -100 };
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ -150, -122 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_HELD);
    fude_zoom_instruments_pen_up(&ins);
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ -150, -130 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED && fabs(on.y + 132) < 1e-9);
    fude_zoom_instruments_pen_up(&ins);
    // A thin one (a true-size ruler zoomed out: its band 12 points): held on it and a little round it,
    // drawn along past that.
    ins.tools[0].at    = (fude_zoom_v2){ 0, 0 };
    ins.tools[0].thick = 12.0 / FUDE_ZOOM_INSTRUMENT_RULER_WIDTH;
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ -150, 0 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_HELD);
    fude_zoom_instruments_pen_up(&ins);
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ -150, -12 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_HELD);
    fude_zoom_instruments_pen_up(&ins);
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ -150, -18 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED && fabs(on.y + 6) < 1e-9);
    fude_zoom_instruments_pen_up(&ins);
    ins.tools[0].thick = 0.0;
    ins.tools[0].at    = (fude_zoom_v2){ 50, -90 };
    // Its knob turns it round its middle, resting at 30°.
    ins.tools[0].at = (fude_zoom_v2){ 0, 0 };
    const fude_zoom_v2 knob = fude_zoom_instrument_knob(&ins, 0);
    CHECK(fude_zoom_instruments_pen_down(&ins, knob, &on) == FUDE_ZOOM_INSTRUMENT_PEN_HELD && ins.turning);
    const f64 r = hypot(knob.x, knob.y), a30 = 30.6 * 3.14159265358979 / 180.0;
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ cos(a30) * r, sin(a30) * r });
    CHECK(fabs(ins.tools[0].angle - 3.14159265358979 / 6.0) < 1e-9);   // within 1.5° of 30°: 30° exactly
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ cos(0.7) * r, sin(0.7) * r });
    CHECK(fabs(ins.tools[0].angle - 0.7) < 1e-9);   // further: as the pen has it
    fude_zoom_instruments_pen_up(&ins);
    // Two fingers turn it a quarter round the middle between them (never scaling it).
    ins.tools[0].angle = 0.0;
    CHECK(fude_zoom_instruments_finger_down(&ins, 1, (fude_zoom_v2){ -100, 0 }));
    CHECK(fude_zoom_instruments_finger_down(&ins, 2, (fude_zoom_v2){ 100, 0 }));
    fude_zoom_instruments_finger_moved(&ins, 1, (fude_zoom_v2){ 0, -100 });
    fude_zoom_instruments_finger_moved(&ins, 2, (fude_zoom_v2){ 0, 100 });
    CHECK(fabs(ins.tools[0].angle - 3.14159265358979 / 2.0) < 1e-9 && hypot(ins.tools[0].at.x, ins.tools[0].at.y) < 1e-9);
    CHECK(fude_zoom_instruments_finger_up(&ins, 1) && ins.held == 0 && ins.hands == 1u);
    CHECK(fude_zoom_instruments_finger_up(&ins, 2) && ins.held < 0);
    CHECK(!fude_zoom_instruments_finger_down(&ins, 3, (fude_zoom_v2){ 300, 300 }));   // not on one: the page's

    // Docking: the 45° set square dragged onto the ruler's top edge lies on it,
    // then slides along it, and lets go when pulled well away.
    ins.tools[0].angle = 0.0; ins.tools[0].at = (fude_zoom_v2){ 0, -150 };   // its top edge at y -118
    CHECK(fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_SQUARE_45, half) == 1);
    ins.tools[1].at = (fude_zoom_v2){ 0, 50 }; ins.tools[1].angle = 0.03;   // its bottom leg a little turned
    CHECK(fude_zoom_instruments_finger_down(&ins, 5, (fude_zoom_v2){ 0, 50 }));
    fude_zoom_instruments_finger_moved(&ins, 5, (fude_zoom_v2){ 0, -25 });   // the leg near the edge
    CHECK(ins.docked == 0 && fabs(ins.tools[1].angle) < 1e-9);
    fude_zoom_instrument_edges(&ins, 1, e);
    CHECK(fabs(e[0].a.y + 118) < 1e-6 && fabs(e[0].b.y + 118) < 1e-6);   // on the ruler's edge
    fude_zoom_instruments_finger_moved(&ins, 5, (fude_zoom_v2){ 60, -5 });   // sideways, and a little up
    fude_zoom_instrument_edges(&ins, 1, e);
    CHECK(ins.docked == 0 && fabs(e[0].a.y + 118) < 1e-6);
    fude_zoom_instruments_finger_moved(&ins, 5, (fude_zoom_v2){ 60, 80 });   // pulled away
    CHECK(ins.docked < 0);
    fude_zoom_instruments_finger_up(&ins, 5);

    // The protractor's round edge: an arc, its angle told in degrees.
    fude_zoom_instrument_remove(&ins, 1);
    fude_zoom_instrument_remove(&ins, 0);
    CHECK(!fude_zoom_instruments_any(&ins));
    const i32 pro = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_PROTRACTOR, half);
    CHECK(pro == 0 && ins.tools[0].serial == 3u);   // the slot again, another one in it
    ins.tools[pro].at = (fude_zoom_v2){ 0, 0 }; ins.tools[pro].angle = 0.0;
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 165, 4 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED);
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 0, 300 });
    CHECK(fabs(on.x) < 1e-6 && fabs(on.y - 160) < 1e-6);
    CHECK(fude_zoom_instruments_ruled(&ins, &along, &deg, &lab) && deg && fabs(along - (90.0 - atan2(4, 165) * 180 / 3.14159265358979)) < 0.01);
    fude_zoom_instruments_pen_up(&ins);
    // Drawn, with the stand-ins: nothing to see, but it runs through.
    const fude_zoom_instruments_look look = { NULL, 32.0f, 0.37, { 1, 1, 1, 200 }, { 0, 0, 0, 255 }, { 0, 0, 0, 255 }, { 0, 0, 0, 255 }, { 0, 0, 255, 255 }, NULL };
    fude_zoom_instruments_render(&ins, &look, 0.0);

    // Several of a kind, each a little further on; at most a dozen in all.
    fude_zoom_instruments_init(&ins);
    const i32 r1 = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half);
    const i32 r2 = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half);
    const i32 c1 = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_COMPASS, half);
    const i32 c2 = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_COMPASS, half);
    CHECK(r1 == 0 && r2 == 1 && c1 == 2 && c2 == 3);
    CHECK(fude_zoom_instruments_count(&ins, FUDE_ZOOM_INSTRUMENT_RULER) == 2u && fude_zoom_instruments_count(&ins, FUDE_ZOOM_INSTRUMENT_COMPASS) == 2u);
    CHECK(fabs(ins.tools[r2].at.x - ins.tools[r1].at.x - 36.0) < 1e-9 && fabs(ins.tools[r2].at.y - ins.tools[r1].at.y + 36.0) < 1e-9);
    // Each compass its own radius.
    ins.tools[c2].at = (fude_zoom_v2){ 300, 200 };
    const fude_zoom_v2 hinge = fude_zoom_instrument_knob(&ins, (u32)c2);
    CHECK(fude_zoom_instruments_pen_down(&ins, hinge, &on) == FUDE_ZOOM_INSTRUMENT_PEN_HELD && ins.held == c2 && ins.turning);
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ hinge.x + 40, hinge.y });
    fude_zoom_instruments_pen_up(&ins);
    CHECK(ins.tools[c2].radius > FUDE_ZOOM_COMPASS_R + 1.0 && fabs(ins.tools[c1].radius - FUDE_ZOOM_COMPASS_R) < 1e-9);
    for(u32 i = 4; i < FUDE_ZOOM_INSTRUMENT_MOST; i++) {
        CHECK(fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_SQUARE_45, half) == (i32)i);
    }
    CHECK(fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half) < 0);   // full
    // Its ×: a finger down and up on it puts it away; one sliding off it does not.
    ins.tools[r1].at = (fude_zoom_v2){ -2000, 0 };   // (out from under the others)
    const fude_zoom_v2 x1 = fude_zoom_instrument_close_at(&ins, (u32)r1);
    CHECK(fude_zoom_instruments_finger_down(&ins, 9, x1) && ins.closing == r1 && ins.held < 0);
    CHECK(fude_zoom_instruments_finger_moved(&ins, 9, (fude_zoom_v2){ x1.x + 60, x1.y }) && ins.closing < 0);
    CHECK(fude_zoom_instruments_finger_up(&ins, 9) && ins.tools[r1].shown);
    CHECK(fude_zoom_instruments_finger_down(&ins, 10, x1));
    CHECK(fude_zoom_instruments_finger_up(&ins, 10) && !ins.tools[r1].shown && ins.closing_hand == 0u);
    CHECK(fude_zoom_instruments_count(&ins, FUDE_ZOOM_INSTRUMENT_RULER) == 1u);
    // ...and the pen's: the compass put away, the other left.
    ins.tools[c1].at = (fude_zoom_v2){ 2000, 0 };
    const fude_zoom_v2 xc = fude_zoom_instrument_close_at(&ins, (u32)c1);
    CHECK(fude_zoom_instruments_pen_down(&ins, xc, &on) == FUDE_ZOOM_INSTRUMENT_PEN_HELD && ins.closing == c1);
    fude_zoom_instruments_pen_up(&ins);
    CHECK(!ins.tools[c1].shown && ins.tools[c2].shown);
    CHECK(fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half) == r1);   // room again: the first free slot
    // A docked one whose partner is put away is free again.
    ins.docked = c2; ins.held = r2;
    fude_zoom_instrument_remove(&ins, (u32)c2);
    CHECK(ins.docked < 0);
    ins.held = -1;
    // A ruler's size tab: dragged out, the ruler longer, its zero end where it was; a compass has none.
    fude_zoom_instruments_init(&ins);
    const i32 gr = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half);
    ins.tools[gr].at = (fude_zoom_v2){ 0, 0 };
    fude_zoom_v2 tab;
    CHECK(fude_zoom_instrument_grow_at(&ins, (u32)gr, &tab) && fabs(tab.x - 310.0) < 1e-9 && fabs(tab.y) < 1e-9);
    CHECK(fude_zoom_instruments_pen_down(&ins, tab, &on) == FUDE_ZOOM_INSTRUMENT_PEN_HELD && ins.growing && ins.held == gr);
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 610, 40 });   // (sideways: only along it counts)
    CHECK(fabs(fude_zoom_instrument_extent(&ins, (u32)gr) - 860.0) < 1e-6);
    CHECK(fabs(ins.tools[gr].at.x - (-280.0 + 430.0)) < 1e-6 && fabs(ins.tools[gr].at.y) < 1e-9);
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ -400, 0 });   // past its other end: as short as it goes
    CHECK(fabs(ins.tools[gr].size - 0.3) < 1e-9);
    fude_zoom_instruments_pen_up(&ins);
    CHECK(!ins.growing && ins.held < 0);
    const i32 gc = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_COMPASS, half);
    CHECK(!fude_zoom_instrument_grow_at(&ins, (u32)gc, &tab));

    // A circle template: near its hole's edge the pen draws round it (beside it: inside the
    // hole); on its plate it moves it; in the middle of the hole it draws as ever.
    fude_zoom_instruments_init(&ins);
    const i32 ct = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_CIRCLE, half);
    ins.tools[ct].at = (fude_zoom_v2){ 0, 0 }; ins.tools[ct].radius = 100.0; ins.tools[ct].angle = 0.0;
    CHECK(fude_zoom_instrument_round(FUDE_ZOOM_INSTRUMENT_CIRCLE) && !fude_zoom_instrument_round(FUDE_ZOOM_INSTRUMENT_RULER));
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 0, 95 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED && fabs(on.y - 100.0) < 1e-6);
    const fude_zoom_v2 inward = fude_zoom_instruments_ruled_outward(&ins);
    CHECK(fabs(inward.x) < 1e-6 && fabs(inward.y + 1.0) < 1e-6);
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ -150, 0 });   // round to the left: a quarter
    CHECK(fabs(on.x + 100.0) < 1e-6 && fabs(on.y) < 1e-6);
    CHECK(fude_zoom_instruments_ruled(&ins, &along, &deg, &lab) && deg && fabs(along - 90.0) < 1e-6);
    fude_zoom_instruments_pen_up(&ins);
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 0, 0 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_NONE);
    fude_zoom_instruments_pen_up(&ins);
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 0, -125 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_HELD && ins.held == ct);
    fude_zoom_instruments_pen_up(&ins);
    fude_zoom_v2 ck[FUDE_ZOOM_INSTRUMENT_KEYS];
    CHECK(fude_zoom_instrument_keys(&ins, (u32)ct, ck) == 1u && fabs(ck[0].x) < 1e-9);   // snapped by its centre

    // Chained: two rulers crossing square — along the first's lower edge, past where the second
    // crosses it and toward the second's edge, the line turns onto it at the crossing (the corner
    // exactly there); kept near the first, it waits at the crossing a little, then goes on.
    fude_zoom_instruments_init(&ins);
    const i32 ra = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half);
    const i32 rb = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half);
    ins.tools[ra].at = (fude_zoom_v2){ 0, 0 };   ins.tools[ra].angle = 0.0;                    // lower edge y -32, x -280..280
    ins.tools[rb].at = (fude_zoom_v2){ 200, 0 }; ins.tools[rb].angle = 3.14159265358979 / 2.0;   // edges x 168 and 232
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 0, -40 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED && ins.ruled == ra);
    CHECK(ins.cross_n >= 2u);
    fude_zoom_v2 corner, n1;
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 150, -40 });
    CHECK(fabs(on.y + 32.0) < 1e-6 && !fude_zoom_instruments_ruled_turn(&ins, &corner, &n1));
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 170, -80 });   // past x 168, near the second's edge
    CHECK(ins.ruled == rb && fude_zoom_instruments_ruled_turn(&ins, &corner, &n1));
    CHECK(fabs(corner.x - 168.0) < 1e-6 && fabs(corner.y + 32.0) < 1e-6 && fabs(n1.y + 1.0) < 1e-9);   // left: out of the first was down
    CHECK(fabs(on.x - 168.0) < 1e-6 && fabs(on.y + 80.0) < 1e-6);
    // Turned, it cannot go back past the corner along the second into the first (from the
    // tablet: it went on under the first's body): held at the corner, then on the right way.
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 165, 10 });
    CHECK(ins.ruled == rb && fabs(on.x - 168.0) < 1e-6 && fabs(on.y + 32.0) < 1e-6);
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 165, -120 });
    CHECK(ins.ruled == rb && fabs(on.x - 168.0) < 1e-6 && fabs(on.y + 120.0) < 1e-6);
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 172, -200 });
    CHECK(fabs(on.x - 168.0) < 1e-6 && fabs(on.y + 200.0) < 1e-6);   // along the second
    fude_zoom_instruments_pen_up(&ins);
    // Kept near the first: it cannot go on into the second (as real ones): it stays at the crossing;
    // back before it, it goes back along the first.
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 0, -40 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED && ins.ruled == ra);
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 150, -33 });
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 178, -33 });
    CHECK(ins.ruled == ra && fabs(on.x - 168.0) < 1e-6 && fabs(on.y + 32.0) < 1e-6);   // at the crossing
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 260, -33 });
    CHECK(ins.ruled == ra && fabs(on.x - 168.0) < 1e-6 && !fude_zoom_instruments_ruled_turn(&ins, &corner, &n1));   // still there
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 120, -33 });
    CHECK(ins.ruled == ra && fabs(on.x - 120.0) < 1e-6);   // back along the first
    fude_zoom_instruments_pen_up(&ins);
    // Begun under the second (inside its body): leaving it, nothing in the way — on along the first.
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 200, -36 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED && ins.ruled == ra);
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 230, -34 });
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 260, -34 });
    CHECK(ins.ruled == ra && fabs(on.x - 260.0) < 1e-6);
    fude_zoom_instruments_pen_up(&ins);

    // Three crossing at (nearly) one point: the line turns onto the one the pen goes along, not the first met
    // (found on the tablet: round the outside of three set squares, it turned in along one under the others).
    fude_zoom_instruments_init(&ins);
    const i32 r3a = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half);
    const i32 r3b = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half);
    const i32 r3c = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half);
    ins.tools[r3a].at = (fude_zoom_v2){ 0, 0 };   ins.tools[r3a].angle = 0.0;                     // its lower edge along y -32
    ins.tools[r3b].at = (fude_zoom_v2){ 100, 0 }; ins.tools[r3b].angle = 3.14159265358979 / 2.0;  // an edge up x 68
    ins.tools[r3c].at = (fude_zoom_v2){ 70.0 + 32.0 * 0.70710678, -32.0 + 32.0 * 0.70710678 };   // (its body up and right of that edge)
    ins.tools[r3c].angle = -3.14159265358979 / 4.0;                                               // an edge down through (70, -32)
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ -100, -40 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED && ins.ruled == r3a);
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 0, -40 });
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 60, -40 });
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 90, -60 });
    CHECK(ins.ruled == r3c && ins.ruled != r3b);   // down along the slanting one: the pen is by it
    fude_zoom_instruments_pen_up(&ins);

    // Resting against two at once (Borja: "rulers need to be able to snap to more than one"): a short ruler
    // lying on a long one, slid along it up to an upright one, stops flush against that too.
    fude_zoom_instruments_init(&ins);
    const i32 ra2 = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half);
    const i32 rb2 = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half);
    const i32 rc2 = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half);
    ins.tools[ra2].at = (fude_zoom_v2){ 0, 0 };     ins.tools[ra2].angle = 0.0;                     // its top edge along y 32
    ins.tools[rb2].at = (fude_zoom_v2){ 150, 300 }; ins.tools[rb2].angle = 3.14159265358979 / 2.0;  // an edge up x 118, from y 20
    ins.tools[rc2].at = (fude_zoom_v2){ -50, 66 };  ins.tools[rc2].angle = 0.0; ins.tools[rc2].size = 0.3;   // 168 long, a hair over the first
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ -50, 66 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_HELD && ins.held == rc2);
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ -50, 67 });
    CHECK(ins.docked == ra2 && fabs(ins.tools[rc2].at.y - 64.0) < 1e-6);   // lying on the first
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 30, 67 });
    CHECK(ins.docked == ra2 && fabs(ins.tools[rc2].at.y - 64.0) < 1e-6 && fabs(ins.tools[rc2].at.x - 34.0) < 1e-6);   // and flush against the upright one
    b8 touching = false;
    for(u32 c = 0; c < ins.contacts; c++) {
        touching = touching || ins.contact_slot[c] == rb2;
    }
    CHECK(touching);
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ -60, 67 });
    CHECK(fabs(ins.tools[rc2].at.x + 60.0) < 1e-6 && ins.contacts == 0u);   // slid back away from it: free of it
    fude_zoom_instruments_pen_up(&ins);

    // Round the outside of three set squares lying flush (Borja, from the tablet: "look at the suggested direction,
    // and look at 3, there is no even direction at all"): a 30° one along a 45°'s long side and on past its corner,
    // another 30° along the first one's short leg and on past its end. Along the 45°'s foot, past its corner, the
    // line goes on down the 30° (whose edge runs through that very corner: the same crossing as the 45°'s own side,
    // once dropped as found twice); up that one's long side, past its top, on along the other 30° (it stopped there).
    fude_zoom_instruments_init(&ins);
    const i32 s45 = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_SQUARE_45, half);
    const i32 s30 = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_SQUARE_30, half);
    const i32 s3b = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_SQUARE_30, half);
    const f64 sq = 0.70710678118654752;
    ins.tools[s45].at = (fude_zoom_v2){ 0, 0 }; ins.tools[s45].angle = 0.0;   // right angle (-93.3, -93.3), foot to (186.7, -93.3), up to (-93.3, 186.7)
    fude_zoom_instrument_edge se[FUDE_ZOOM_INSTRUMENT_EDGES];
    fude_zoom_instrument_edges(&ins, (u32)s45, se);
    const fude_zoom_v2 k1 = se[0].b, k2 = se[1].b;
    const fude_zoom_v2 p0 = { k2.x + (k1.x - k2.x) * 0.45, k2.y + (k1.y - k2.y) * 0.45 };
    ins.tools[s30].at = (fude_zoom_v2){ 0, 0 }; ins.tools[s30].angle = -3.14159265358979 / 4.0;   // its long leg down along the 45°'s, on past it
    fude_zoom_instrument_edges(&ins, (u32)s30, se);
    ins.tools[s30].at = (fude_zoom_v2){ p0.x - se[0].a.x, p0.y - se[0].a.y };                    // its right angle on the 45°'s long side
    fude_zoom_instrument_edges(&ins, (u32)s30, se);
    const fude_zoom_v2 d1 = se[1].a, d2 = se[1].b;   // its long side, up to its top
    const fude_zoom_v2 mid = { (se[2].a.x + se[2].b.x) * 0.5, (se[2].a.y + se[2].b.y) * 0.5 };
    ins.tools[s3b].at = (fude_zoom_v2){ 0, 0 }; ins.tools[s3b].angle = 3.14159265358979 / 4.0;   // its long leg up along the first 30°'s short one
    fude_zoom_instrument_edges(&ins, (u32)s3b, se);
    ins.tools[s3b].at = (fude_zoom_v2){ mid.x - se[0].a.x, mid.y - se[0].a.y };
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 0, k1.y - 6.0 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED && ins.ruled == s45 && ins.ruled_edge == 0u);
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 150, k1.y - 6.0 });
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ k1.x + 15.0 * sq - 4.0 * sq, k1.y - 15.0 * sq - 4.0 * sq });   // past its corner, down by the 30°
    CHECK(ins.ruled == s30 && ins.ruled_edge == 0u && fabs(on.x - (k1.x + 15.0 * sq)) < 1e-6 && fabs(on.y - (k1.y - 15.0 * sq)) < 1e-6);
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ k1.x + 60.0 * sq - 4.0 * sq, k1.y - 60.0 * sq - 4.0 * sq });
    CHECK(ins.ruled == s30 && fabs(on.x - (k1.x + 60.0 * sq)) < 1e-6 && fabs(on.y - (k1.y - 60.0 * sq)) < 1e-6);   // on down it
    fude_zoom_instruments_pen_up(&ins);
    const f64          hl = hypot(d2.x - d1.x, d2.y - d1.y);
    fude_zoom_v2       hn = { -(d2.y - d1.y) / hl, (d2.x - d1.x) / hl };   // out of its long side
    if(hn.x * ((d1.x + d2.x) * 0.5 - ins.tools[s30].at.x) + hn.y * ((d1.y + d2.y) * 0.5 - ins.tools[s30].at.y) < 0.0) {
        hn = (fude_zoom_v2){ -hn.x, -hn.y };
    }
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ d1.x + (d2.x - d1.x) * 0.3 + hn.x * 5.0, d1.y + (d2.y - d1.y) * 0.3 + hn.y * 5.0 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED &&
          ins.ruled == s30 && ins.ruled_edge == 1u);
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ d1.x + (d2.x - d1.x) * 0.95 + hn.x * 5.0, d1.y + (d2.y - d1.y) * 0.95 + hn.y * 5.0 });
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ d2.x + 12.0 * sq + 4.0 * sq, d2.y + 12.0 * sq - 4.0 * sq });   // past its top, by the other 30°
    CHECK(ins.ruled == s3b && ins.ruled_edge == 0u && fabs(on.x - (d2.x + 12.0 * sq)) < 1e-6 && fabs(on.y - (d2.y + 12.0 * sq)) < 1e-6);
    fude_zoom_instruments_pen_up(&ins);
    // Round the outside of one's corner: up its side, past the corner (where both its sides are as near: it waits
    // there), then round by its other side — on along that (it stayed at the corner for good).
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ k2.x - 6.0, 0 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED && ins.ruled == s45 && ins.ruled_edge == 2u);
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ k2.x - 6.0, k2.y - 20.0 });
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ k2.x - 2.0, k2.y + 6.0 });
    CHECK(fabs(on.x - k2.x) < 1e-6 && fabs(on.y - k2.y) < 1e-6);   // at the corner
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ k2.x + 8.0, k2.y + 2.0 });
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ k2.x + 40.0 * sq + 4.0 * sq, k2.y - 40.0 * sq + 4.0 * sq });
    CHECK(ins.ruled == s45 && ins.ruled_edge == 1u && fabs(on.x - (k2.x + 40.0 * sq)) < 1e-6 && fabs(on.y - (k2.y - 40.0 * sq)) < 1e-6);   // round it, down its long side
    fude_zoom_instruments_pen_up(&ins);

    // A stencil (a piece laid as an instrument): an open line across its middle and a closed square — the pen near
    // the line draws ON it (traced), held at its ends (a line with two, not a loop); on the plate away from its
    // lines, the plate is held.
    fude_zoom_instruments_init(&ins);
    const i32 sn = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_STENCIL, half);
    fude_zoom_stencil* st = &ins.stencils[sn];
    memset(st, 0, sizeof(*st));
    st->points[0] = (fude_zoom_v2){ -100, 0 }; st->points[1] = (fude_zoom_v2){ 0, 0 }; st->points[2] = (fude_zoom_v2){ 100, 0 };
    st->first[0] = 0; st->count[0] = 3; st->closed[0] = false;
    const fude_zoom_v2 sqr[4] = { { -140, -100 }, { 140, -100 }, { 140, 100 }, { -140, 100 } };
    for(u32 i = 0; i < 4u; i++) st->points[3 + i] = sqr[i];
    st->first[1] = 3; st->count[1] = 4; st->closed[1] = true;
    st->lines = 2; st->hw = 160; st->hh = 120;
    ins.tools[sn].at = (fude_zoom_v2){ 0, 0 }; ins.tools[sn].angle = 0.0; ins.tools[sn].radius = 160; ins.tools[sn].radius2 = 120;
    fude_zoom_instrument_edge sne[FUDE_ZOOM_INSTRUMENT_EDGES];
    CHECK(fude_zoom_instrument_edges(&ins, (u32)sn, sne) == 2u && sne[0].curve && sne[0].open && sne[0].trace && sne[1].curve && !sne[1].open);
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ -50, 6 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED && ins.ruled == sn && ins.ruled_edge == 0u);
    CHECK(fabs(on.x + 50.0) < 1e-6 && fabs(on.y) < 1e-6 && fude_zoom_instruments_ruled_traced(&ins));
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 0, 5 });   // (as a pen goes: a little at a time)
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 60, 5 });
    CHECK(fabs(on.x - 60.0) < 1e-6 && fabs(on.y) < 1e-6);
    fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 100, 4 });
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 130, 4 });
    CHECK(fabs(on.x - 100.0) < 1e-6 && fabs(on.y) < 1e-6);   // held at its end (no way round)
    fude_zoom_instruments_pen_up(&ins);
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 141, 50 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED && ins.ruled_edge == 1u && fabs(on.x - 140.0) < 1e-6);   // its square's side
    fude_zoom_instruments_pen_up(&ins);
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 60, 60 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_HELD && ins.held == sn);   // on its plate, away from its lines
    fude_zoom_instruments_pen_up(&ins);
    fude_zoom_instrument_remove(&ins, (u32)sn);
    // A ruler dragged up to a stencil's curved line (an arch from (-100, 0) over (0, 80) to (100, 0)) docks to nothing:
    // the line's ends are not an edge to lie along (it docked to the chord between them, along y 0).
    {
        fude_zoom_instruments_init(&ins);
        const i32 arch = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_STENCIL, half);
        fude_zoom_stencil* ast = &ins.stencils[arch];
        memset(ast, 0, sizeof(*ast));
        ast->points[0] = (fude_zoom_v2){ -100, 0 }; ast->points[1] = (fude_zoom_v2){ 0, 80 }; ast->points[2] = (fude_zoom_v2){ 100, 0 };
        ast->first[0] = 0; ast->count[0] = 3; ast->closed[0] = false;
        ast->lines = 1; ast->hw = 160; ast->hh = 120;
        ins.tools[arch].at = (fude_zoom_v2){ 0, 0 }; ins.tools[arch].angle = 0.0; ins.tools[arch].radius = 160; ins.tools[arch].radius2 = 120;
        const i32 below = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_RULER, half);
        ins.tools[below].at = (fude_zoom_v2){ 0, -300 }; ins.tools[below].angle = 0.0;   // (its top edge along y -268, going the chord's other way)
        CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 0, -300 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_HELD && ins.held == below);
        fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 0, -34 });   // (its top edge 2 under the chord)
        CHECK(ins.docked < 0 && fabs(ins.tools[below].at.y + 34.0) < 1e-9 && fabs(ins.tools[below].angle) < 1e-12);
        fude_zoom_instruments_pen_up(&ins);
    }

    // A corner radius gauge: its round a quarter turn at its lower left, the pen along it staying
    // on it, and past its end on along the side it meets; snapped by the corner its round rounds.
    fude_zoom_instruments_init(&ins);
    const i32 cg = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_CORNER, half);
    ins.tools[cg].at = (fude_zoom_v2){ 0, 0 }; ins.tools[cg].angle = 0.0; ins.tools[cg].radius = 60.0;   // half side 54: round about (6, 6)
    fude_zoom_instrument_edge ce[FUDE_ZOOM_INSTRUMENT_EDGES];
    CHECK(fude_zoom_instrument_edges(&ins, (u32)cg, ce) == 5u && ce[4].arc && fabs(ce[4].span - 3.14159265358979 / 2.0) < 1e-9);
    fude_zoom_v2 cks[FUDE_ZOOM_INSTRUMENT_KEYS];
    CHECK(fude_zoom_instrument_keys(&ins, (u32)cg, cks) >= 1u && fabs(cks[0].x + 54.0) < 1e-9 && fabs(cks[0].y + 54.0) < 1e-9);
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ -38, -38 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED && ins.ruled == cg && ins.ruled_edge == 4u);
    CHECK(fabs(hypot(on.x - 6.0, on.y - 6.0) - 60.0) < 1e-6);
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ -60, -10 });
    CHECK(fabs(hypot(on.x - 6.0, on.y - 6.0) - 60.0) < 1e-6 && on.x < -50.0);   // round it, towards its left side
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ -70, 40 });
    CHECK(ins.ruled_edge == 3u && fabs(on.x + 54.0) < 1e-6 && fabs(on.y - 40.0) < 1e-6);   // past its end: on along its side (chained), as a corner rounded goes on
    const fude_zoom_v2 cout = fude_zoom_instruments_ruled_outward(&ins);
    CHECK(cout.x < -0.99);   // the line outside it (away from the round's centre)
    fude_zoom_instruments_pen_up(&ins);
    CHECK(fude_zoom_instruments_at(&ins, (fude_zoom_v2){ 20, 20 }) == cg && fude_zoom_instruments_at(&ins, (fude_zoom_v2){ -50, -50 }) < 0);   // its plate, not past its round
    // Down its side and round its round (found on the tablet: once round, the line jumped back to where the
    // side meets the round — its own corner taken for another's — the pen in over the plate made it worse).
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ -62, 40 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED && ins.ruled_edge == 3u);
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ -62, 0 });
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ -50, -30 });
    CHECK(ins.ruled_edge == 4u && fabs(hypot(on.x - 6.0, on.y - 6.0) - 60.0) < 1e-6 && on.y < -20.0);   // turned onto its round
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ -10, -40 });
    const fude_zoom_v2 was = on;
    CHECK(ins.ruled_edge == 4u && fabs(hypot(on.x - 6.0, on.y - 6.0) - 60.0) < 1e-6 && on.y < -40.0);   // on round it, not back at the side
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ -20, -10 });
    CHECK(ins.ruled_edge == 4u && fabs(on.x - was.x) < 1e-6 && fabs(on.y - was.y) < 1e-6);   // the pen in over the plate: it stays
    fude_zoom_instruments_pen_up(&ins);
    fude_zoom_instrument_remove(&ins, (u32)cg);

    // An ellipse template: its hole's edge drawn along from inside; a French curve's, from outside,
    // the pen going on along it (never jumping across), its body held by the pen.
    fude_zoom_instruments_init(&ins);
    const i32 el = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_ELLIPSE, half);
    ins.tools[el].at = (fude_zoom_v2){ 0, 0 }; ins.tools[el].radius = 120.0; ins.tools[el].radius2 = 60.0; ins.tools[el].angle = 0.0;
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 0, 52 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED && fabs(on.y - 60.0) < 0.5);
    const fude_zoom_v2 ein = fude_zoom_instruments_ruled_outward(&ins);
    CHECK(ein.y < -0.95);   // into the hole
    on = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 100, 30 });
    CHECK(fabs((on.x * on.x) / (120.0 * 120.0) + (on.y * on.y) / (60.0 * 60.0) - 1.0) < 0.02);   // on the ellipse
    fude_zoom_instruments_pen_up(&ins);
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ 0, 0 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_NONE);
    fude_zoom_instruments_pen_up(&ins);
    fude_zoom_instrument_remove(&ins, (u32)el);
    const i32 fc = fude_zoom_instrument_add(&ins, FUDE_ZOOM_INSTRUMENT_CURVE, half);
    ins.tools[fc].at = (fude_zoom_v2){ 0, 0 }; ins.tools[fc].angle = 0.0;
    fude_zoom_v2 cv[FUDE_ZOOM_INSTRUMENT_CURVE_POINTS];
    const u32 cn = fude_zoom_instrument_curve(&ins, (u32)fc, cv, FUDE_ZOOM_INSTRUMENT_CURVE_POINTS);
    CHECK(cn >= 150u && fude_zoom_fill_inside(cv, cn, (fude_zoom_v2){ -70, -30 }));
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ -26, -90 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_RULED);
    CHECK(!fude_zoom_instruments_ruled_straight(&ins));   // a curve: its line keeps every point (not its two ends)
    const fude_zoom_v2 fout = fude_zoom_instruments_ruled_outward(&ins);
    CHECK(!fude_zoom_fill_inside(cv, cn, (fude_zoom_v2){ on.x + fout.x * 6.0, on.y + fout.y * 6.0 }) &&
          fude_zoom_fill_inside(cv, cn, (fude_zoom_v2){ on.x - fout.x * 6.0, on.y - fout.y * 6.0 }));
    const fude_zoom_v2 step = fude_zoom_instruments_pen_moved(&ins, (fude_zoom_v2){ 30, -75 });
    CHECK(hypot(step.x - 30.0, step.y + 66.0) < 15.0);   // along its lower side, not over to the upper
    fude_zoom_instruments_pen_up(&ins);
    CHECK(fude_zoom_instruments_pen_down(&ins, (fude_zoom_v2){ -70, -20 }, &on) == FUDE_ZOOM_INSTRUMENT_PEN_HELD && ins.held == fc);
    fude_zoom_instruments_pen_up(&ins);
}

// Random strokes and sweeps; the alive set after every step, then every undo
// and redo matched against it.
// Order-free (a reopened canvas lists its objects frame by frame).
static u64 signature(const fude_zoom_scene* s) {
    u64 h = 0;
    for(u32 i = 0; i < fude_zoom_scene_object_count(s); i++) {
        const fude_zoom_object* o = fude_zoom_scene_object(s, i);
        if((o->kind == FUDE_ZOOM_KIND_STROKE || o->kind == FUDE_ZOOM_KIND_FILL) && (o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
            u64 m = o->id * 0x9E3779B97F4A7C15ull ^ (u64)o->count * 0xC2B2AE3D27D4EB4Full;
            m ^= m >> 29; m *= 0xBF58476D1CE4E5B9ull; m ^= m >> 32;
            h += m;
        }
    }
    return h;
}

static void random_step(fude_zoom_scene* s, fude_zoom_eraser* e) {
    if(rnd() % 3 != 0 || alive_count(s) == 0) {
        const f64 x = rndf() * 200, y = rndf() * 200, a = rndf() * 6.28, len = 10 + rndf() * 80;
        line(s, x, y, x + cos(a) * len, y + sin(a) * len, 0.7, (f32)(0.5 + rndf()));
    } else {
        fude_zoom_erase_begin(e, (FUDE_ZOOM_ERASE_)(rnd() % 3));
        for(u32 k = 0; k < 1 + rnd() % 4; k++) {
            const f64 x = rndf() * 200, y = rndf() * 200;
            fude_zoom_erase_step(s, e, s->camera.frame, (fude_zoom_v2){ x, y }, (fude_zoom_v2){ x + rndf() * 40 - 20, y + rndf() * 40 - 20 }, 1 + rndf() * 4, 0.3);
        }
        fude_zoom_erase_end(s, e, s->camera.frame, (fude_zoom_box){ 0, 0, 200, 200 });
    }
}

static void test_history(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_eraser e; fude_zoom_eraser_init(&e);
    enum { STEPS = 300 };
    static u64 sig[STEPS + 1];
    sig[0] = signature(&s);
    u32 steps = 0;
    for(u32 i = 0; i < STEPS; i++) {
        const u32 before = s.action_count;
        random_step(&s, &e);
        if(s.action_count != before) sig[++steps] = signature(&s);
    }
    b8 ok = true;
    for(u32 i = steps; i > 0; i--) { if(!fude_zoom_history_undo(&s) || signature(&s) != sig[i - 1]) ok = false; }
    CHECK(ok);
    CHECK(!fude_zoom_history_can_undo(&s));
    for(u32 i = 0; i < steps; i++) { if(!fude_zoom_history_redo(&s) || signature(&s) != sig[i + 1]) ok = false; }
    CHECK(ok);
    // Half undone, then a new stroke: the undone half is gone for good (garbage).
    for(u32 i = 0; i < steps / 2; i++) fude_zoom_history_undo(&s);
    line(&s, 0, 0, 10, 10, 1.0, 1.0f);
    CHECK(!fude_zoom_history_can_redo(&s));

    // The budget: past it the oldest steps go, and what only they kept is let go.
    s.history_budget = 20000;
    for(u32 i = 0; i < 200; i++) line(&s, rndf() * 100, rndf() * 100, rndf() * 100, rndf() * 100, 0.5, 1.0f);
    CHECK(s.history_bytes <= 20000 + 20000);
    CHECK(s.action_count < 200);
    fude_zoom_eraser_destroy(&e);
    fude_zoom_scene_destroy(&s);
}

// --- moving things -------------------------------------------------------------------------------

static void test_moves(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const fude_zoom_v2 half = { 500, 400 };
    const u32 a = line(&s, 0, 0, 100, 0, 1.0, 1.0f);
    // A detail one level in, then back out.
    fude_zoom_camera_look_at(&s, s.root, (fude_zoom_v2){ 50, 50 }, 1.0);
    for(u32 k = 0; k < 7; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&s, half); }
    const u32 detail = s.camera.frame;
    const u32 d = line(&s, s.camera.at.x - 20, s.camera.at.y, s.camera.at.x + 20, s.camera.at.y, 0.5, 1.0f);
    for(u32 k = 0; k < 7; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 0.5); fude_zoom_camera_settle(&s, half); }
    CHECK(s.camera.frame == s.root);
    const u32 fo = fude_zoom_scene_frame(&s, detail)->object;

    // Where the detail's stroke is, seen from the root, before and after.
    fude_zoom_v2 da, db; ends(&s, d, &da, &db);
    const fude_zoom_v2 da_root = fude_zoom_sim_apply(fude_zoom_scene_sim(&s, detail, s.root), da);

    // The line turned a quarter round its start and doubled; the frame moved 1000 across.
    const u32 ids[2] = { a, fo };
    const fude_zoom_place before[2] = { fude_zoom_scene_place_of(&s, a), fude_zoom_scene_place_of(&s, fo) };
    const fude_zoom_sim turn  = { 0.0, 2.0, 0.0, 0.0 };   // 90°, ×2, round the origin (the line's start)
    const fude_zoom_sim shift = { 1.0, 0.0, 1000.0, 0.0 };
    const fude_zoom_place after[2] = { fude_zoom_place_moved(before[0], turn), fude_zoom_place_moved(before[1], shift) };
    fude_zoom_scene_set_place(&s, a, after[0]);
    fude_zoom_scene_set_place(&s, fo, after[1]);
    fude_zoom_history_push_moved(&s, s.root, (fude_zoom_box){ 0, 0, 1, 1 }, ids, before, after, 2);
    fude_zoom_v2 aa, ab; ends(&s, a, &aa, &ab);
    CHECK(fabs(aa.x) < 1e-9 && fabs(aa.y) < 1e-9 && fabs(ab.x) < 1e-9 && fabs(ab.y - 200.0) < 1e-9);
    CHECK(fabs(fude_zoom_scene_object(&s, a)->box.max_y - 202.0) < 0.01);   // its box went with it (half-width 1, ×2)
    // The detail went with its frame.
    const fude_zoom_v2 da_now = fude_zoom_sim_apply(fude_zoom_scene_sim(&s, detail, s.root), da);
    CHECK(fabs(da_now.x - (da_root.x + 1000.0)) < 1e-6 && fabs(da_now.y - da_root.y) < 1e-6);
    // Found where it is now, not where it was.
    rde_arr found = rde_arr_new(sizeof(u32), NULL);
    fude_zoom_scene_query(&s, s.root, (fude_zoom_box){ -5, 150, 5, 210 }, &found);
    CHECK(rde_arr_length(&found) == 1);
    rde_arr_clear(&found);
    fude_zoom_scene_query(&s, s.root, (fude_zoom_box){ 40, -5, 60, 5 }, &found);
    CHECK(rde_arr_length(&found) == 0);
    rde_arr_free(&found);

    // Undo puts both back; redo again.
    CHECK(fude_zoom_history_undo(&s));
    ends(&s, a, &aa, &ab);
    CHECK(fabs(ab.x - 100.0) < 1e-9 && fabs(ab.y) < 1e-9);
    CHECK(fabs(fude_zoom_scene_frame(&s, detail)->xf.ox - after[1].t.x + 1000.0) < 1e-9);
    CHECK(fude_zoom_history_redo(&s));

    // A frame deleted, its undo brings it back; and the whole thing through the file.
    fude_zoom_scene_set_alive(&s, fo, false);
    fude_zoom_history_push(&s, s.root, (fude_zoom_box){ 0, 0, 1, 1 }, &fo, 1, NULL, 0);
    CHECK(fude_zoom_scene_frame(&s, detail)->removed);
    mkdir("./saves", 0755);
    remove("./saves/moves.zoom"); remove("./saves/moves.zoom.bak");
    fude_zoom_file f; memset(&f, 0, sizeof f); snprintf(f.path, sizeof f.path, "%s", "./saves/moves.zoom");
    CHECK(fude_zoom_file_flush(&f, &s));
    fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
    CHECK(fude_zoom_file_open(&g, "./saves/moves.zoom", &t) == FUDE_LOAD_OK);
    const u32 ta = fude_zoom_scene_find_object(&t, fude_zoom_scene_object(&s, a)->id);
    CHECK(ta != FUDE_ZOOM_NONE);
    if(ta != FUDE_ZOOM_NONE) {
        fude_zoom_v2 ta0, ta1; ends(&t, ta, &ta0, &ta1);
        CHECK(fabs(ta1.x) < 1e-6 && fabs(ta1.y - 200.0) < 1e-6);
    }
    const u32 tdetail = fude_zoom_scene_find_frame(&t, fude_zoom_scene_frame(&s, detail)->id);
    CHECK(tdetail != FUDE_ZOOM_NONE && fude_zoom_scene_frame(&t, tdetail)->removed);
    // Undo the delete after reopening: the frame is back, still where it was moved.
    CHECK(fude_zoom_history_undo(&t));
    if(tdetail != FUDE_ZOOM_NONE) {
        CHECK(!fude_zoom_scene_frame(&t, tdetail)->removed);
        CHECK(fabs(fude_zoom_scene_frame(&t, tdetail)->xf.ox - after[1].t.x) < 1e-9);
    }
    // And the move before it.
    CHECK(fude_zoom_history_undo(&t));
    if(tdetail != FUDE_ZOOM_NONE) CHECK(fabs(fude_zoom_scene_frame(&t, tdetail)->xf.ox - before[1].t.x) < 1e-9);
    fude_zoom_scene_destroy(&t);

    // An erase on a turned, scaled stroke: its outline keeps its turn and scale, and stays where it was.
    fude_zoom_eraser e; fude_zoom_eraser_init(&e);
    fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_PARTIAL);
    fude_zoom_erase_step(&s, &e, s.root, (fude_zoom_v2){ -10, 100 }, (fude_zoom_v2){ 10, 100 }, 2.0, 0.5);
    fude_zoom_erase_end(&s, &e, s.root, (fude_zoom_box){ 0, 0, 1, 1 });
    CHECK(!alive(&s, a));
    CHECK(filled_at(&s, 0, 50) && filled_at(&s, 0, 150) && filled_at(&s, 1.5, 96.5) && !filled_at(&s, 0, 100) && !filled_at(&s, 0, 98.6));   // reach 2; half-width 2
    CHECK(!filled_at(&s, 2.6, 50));
    CHECK(fabs(fude_zoom_scene_object(&s, s.objects.count - 1)->scale - 2.0) < 1e-12);
    fude_zoom_eraser_destroy(&e);
    fude_zoom_scene_destroy(&s);
}

// --- shapes ----------------------------------------------------------------------------------------

// A hand's version of a path: points every ~3 units along it, a little wobble.
static u32 hand(fude_zoom_v2* out, u32 max, const fude_zoom_v2* corners, u32 n, b8 closed, f64 wobble) {
    u32 k = 0;
    const u32 sides = closed ? n : n - 1;
    for(u32 i = 0; i < sides; i++) {
        const fude_zoom_v2 a = corners[i], b = corners[(i + 1) % n];
        const u32 steps = (u32)(hypot(b.x - a.x, b.y - a.y) / 3.0) + 1;
        for(u32 j = 0; j < steps && k < max; j++) {
            const f64 t = (f64)j / steps;
            out[k++] = (fude_zoom_v2){ a.x + (b.x - a.x) * t + (rndf() - 0.5) * wobble, a.y + (b.y - a.y) * t + (rndf() - 0.5) * wobble };
        }
    }
    if(k < max) out[k++] = closed ? corners[0] : corners[n - 1];
    return k;
}

static void test_shapes(void) {
    static fude_zoom_v2 p[4096];
    fude_zoom_shape_fit f;
    // A line, a little wobbly.
    { const fude_zoom_v2 c[2] = { { 0, 0 }, { 300, 40 } }; const u32 n = hand(p, 4096, c, 2, false, 3.0);
      CHECK(fude_zoom_shape_recognize(p, n, &f) && f.type == FUDE_ZOOM_SHAPE_LINE && fabs(f.n[0] - 300) < 6 && fabs(f.n[1] - 40) < 6); }
    // A rectangle, nearly level: level.
    { const fude_zoom_v2 c[4] = { { 0, 0 }, { 300, 6 }, { 297, 206 }, { -3, 200 } }; const u32 n = hand(p, 4096, c, 4, true, 3.0);
      CHECK(fude_zoom_shape_recognize(p, n, &f) && f.type == FUDE_ZOOM_SHAPE_RECT);
      CHECK(fabs(f.rotation) < 1e-9 && fabs(f.n[0] - 150) < 10 && fabs(f.n[1] - 100) < 10); }
    // ...whichever corner it is drawn from, either way round, however the hand wobbles: level, the same rectangle (its
    // angle within a quarter turn either way: turned half round it would be upside down).
    {
        const fude_zoom_v2 base[4] = { { 0, 0 }, { 300, 6 }, { 297, 206 }, { -3, 200 } };
        const u32 rng_was = rng;
        u32 tries = 0, level = 0;
        for(u32 k = 0; k < 400u; k++) {
            rng = 7919u * (k + 1u);
            fude_zoom_v2 c[4];
            const u32 from = k % 4u;
            for(u32 i = 0; i < 4u; i++) c[i] = base[(k & 4u) ? (from + 4u - i) % 4u : (from + i) % 4u];
            const u32 n = hand(p, 4096, c, 4, true, 3.0);
            tries++;
            level += fude_zoom_shape_recognize(p, n, &f) && f.type == FUDE_ZOOM_SHAPE_RECT && fabs(f.rotation) < 1e-9 &&
                     fabs(f.n[0] - 150) < 10 && fabs(f.n[1] - 100) < 10 && fabs(f.at.x - 148.5) < 8 && fabs(f.at.y - 103) < 8;
        }
        CHECK(level == tries);
        // Tilted on purpose (20°): kept tilted, its angle within a quarter turn.
        u32 tilted = 0;
        for(u32 k = 0; k < 200u; k++) {
            rng = 104729u * (k + 1u);
            const f64 a = 0.349 + (k & 1u ? 3.14159265358979 : 0.0), ca = cos(a), sa = sin(a);
            fude_zoom_v2 c[4];
            const fude_zoom_v2 r[4] = { { 0, 0 }, { 300, 0 }, { 300, 200 }, { 0, 200 } };
            for(u32 i = 0; i < 4u; i++) c[i] = (fude_zoom_v2){ r[(i + k) % 4u].x * ca - r[(i + k) % 4u].y * sa, r[(i + k) % 4u].x * sa + r[(i + k) % 4u].y * ca };
            const u32 n = hand(p, 4096, c, 4, true, 2.0);
            tilted += fude_zoom_shape_recognize(p, n, &f) && f.type == FUDE_ZOOM_SHAPE_RECT && fabs(f.rotation - 0.349) < 0.05 ? 1u : 0u;
        }
        CHECK(tilted == 200u);
        rng = rng_was;
    }
    // A triangle.
    { const fude_zoom_v2 c[3] = { { 0, 0 }, { 260, 0 }, { 120, 220 } }; const u32 n = hand(p, 4096, c, 3, true, 3.0);
      CHECK(fude_zoom_shape_recognize(p, n, &f) && f.type == FUDE_ZOOM_SHAPE_POLYGON && f.count == 6); }
    // A big rectangle drawn slowly (over 3,000 points): every point looked at for corners, so it is the rectangle (the
    // first 2,048 alone, two sides and part of a third, once made it a triangle).
    { const fude_zoom_v2 c[4] = { { 0, 0 }, { 3000, 0 }, { 3000, 2000 }, { 0, 2000 } }; const u32 n = hand(p, 4096, c, 4, true, 3.0);
      CHECK(n > 3000u && fude_zoom_shape_recognize(p, n, &f) && f.type == FUDE_ZOOM_SHAPE_RECT);
      CHECK(fabs(f.n[0] - 1500) < 10 && fabs(f.n[1] - 1000) < 10 && fabs(f.at.x - 1500) < 10 && fabs(f.at.y - 1000) < 10); }
    // A circle, and an ellipse.
    for(u32 e = 0; e < 2; e++) {
        const f64 rx = 120, ry = e == 0 ? 116 : 60;
        u32 n = 0;
        for(u32 i = 0; i <= 200; i++) { const f64 a = 6.2831853 * i / 200.0; p[n++] = (fude_zoom_v2){ 400 + cos(a) * rx + (rndf() - 0.5) * 4, 300 + sin(a) * ry + (rndf() - 0.5) * 4 }; }
        CHECK(fude_zoom_shape_recognize(p, n, &f) && f.type == FUDE_ZOOM_SHAPE_ELLIPSE);
        if(e == 0) CHECK(fabs(f.n[0] - f.n[1]) < 1e-9 && fabs(f.n[0] - 118) < 6 && fabs(f.at.x - 400) < 5 && fabs(f.at.y - 300) < 5);
        else CHECK(fabs(fmax(f.n[0], f.n[1]) - 120) < 8 && fabs(fmin(f.n[0], f.n[1]) - 60) < 8);
    }
    // A scribble, and an open curve: no shape.
    { u32 n = 0; for(u32 i = 0; i < 300; i++) p[n++] = (fude_zoom_v2){ rndf() * 200, rndf() * 200 };
      CHECK(!fude_zoom_shape_recognize(p, n, &f)); }
    { u32 n = 0; for(u32 i = 0; i <= 100; i++) { const f64 a = 3.1415926 * i / 100.0; p[n++] = (fude_zoom_v2){ cos(a) * 150, sin(a) * 150 }; }
      CHECK(!fude_zoom_shape_recognize(p, n, &f)); }

    // Shapes in a canvas: drawn, erased whole, moved, through the file.
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const f64 rect[3] = { 50, 30, 0 };
    const u32 r = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 100, 100 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, rect, 3, (rde_color){ 1, 2, 3, 255 }, 2.0f, FUDE_ZOOM_FLAG_FILLED, 0);
    fude_zoom_history_push(&s, s.root, fude_zoom_scene_object(&s, r)->box, NULL, 0, &r, 1);
    CHECK(fabs(fude_zoom_scene_object(&s, r)->box.min_x - 48) < 1e-9 && fabs(fude_zoom_scene_object(&s, r)->box.max_y - 132) < 1e-9);
    const f64 circle[2] = { 40, 40 };
    const u32 c = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 400, 100 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, circle, 2, (rde_color){ 1, 2, 3, 255 }, 2.0f, 0, 0);
    fude_zoom_history_push(&s, s.root, fude_zoom_scene_object(&s, c)->box, NULL, 0, &c, 1);
    // The eraser inside the filled rectangle rubs it out there: it becomes its
    // line (a stroke, untouched) and its fill (cut); inside the outline-only
    // circle, nothing. Whole strokes' eraser takes a shape whole.
    fude_zoom_eraser e; fude_zoom_eraser_init(&e);
    fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_PARTIAL);
    CHECK(fude_zoom_erase_step(&s, &e, s.root, (fude_zoom_v2){ 100, 100 }, (fude_zoom_v2){ 101, 100 }, 2.0, 0.5) >= 1);
    CHECK(fude_zoom_erase_step(&s, &e, s.root, (fude_zoom_v2){ 400, 100 }, (fude_zoom_v2){ 401, 100 }, 2.0, 0.5) == 0);
    fude_zoom_erase_end(&s, &e, s.root, (fude_zoom_box){ 0, 0, 1, 1 });
    CHECK(!alive(&s, r) && alive(&s, c));
    {
        u32 lines = 0, fills = 0;
        for(u32 i = 0; i < fude_zoom_scene_object_count(&s); i++) {
            const fude_zoom_object* o = fude_zoom_scene_object(&s, i);
            if(!(o->flags & FUDE_ZOOM_FLAG_ALIVE)) continue;
            lines += o->kind == FUDE_ZOOM_KIND_STROKE && o->count == 5 ? 1u : 0u;   // the rectangle's corners, closed
            fills += o->kind == FUDE_ZOOM_KIND_FILL && (o->channels & FUDE_ZOOM_CHANNEL_TIME) ? 1u : 0u;
        }
        CHECK(lines == 1 && fills == 1);
    }
    CHECK(fude_zoom_history_undo(&s) && alive(&s, r));
    fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_STROKE);
    CHECK(fude_zoom_erase_step(&s, &e, s.root, (fude_zoom_v2){ 100, 100 }, (fude_zoom_v2){ 101, 100 }, 2.0, 0.5) == 1);
    fude_zoom_erase_end(&s, &e, s.root, (fude_zoom_box){ 0, 0, 1, 1 });
    CHECK(!alive(&s, r) && fude_zoom_history_undo(&s) && alive(&s, r));
    // The circle moved and doubled.
    const fude_zoom_place b0 = fude_zoom_scene_place_of(&s, c);
    const fude_zoom_place a0 = fude_zoom_place_moved(b0, (fude_zoom_sim){ 2.0, 0.0, -400.0, 0.0 });
    fude_zoom_scene_set_place(&s, c, a0);
    fude_zoom_history_push_moved(&s, s.root, (fude_zoom_box){ 0, 0, 1, 1 }, &c, &b0, &a0, 1);
    CHECK(fabs(fude_zoom_scene_object(&s, c)->box.max_x - (400 + 80 + 4)) < 0.5);   // radius 40·2 and half-width 2·2
    remove("./saves/shapes.zoom"); remove("./saves/shapes.zoom.bak");
    fude_zoom_file fz; memset(&fz, 0, sizeof fz); snprintf(fz.path, sizeof fz.path, "%s", "./saves/shapes.zoom");
    CHECK(fude_zoom_file_flush(&fz, &s));
    fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
    CHECK(fude_zoom_file_open(&g, "./saves/shapes.zoom", &t) == FUDE_LOAD_OK);
    const u32 tr = fude_zoom_scene_find_object(&t, fude_zoom_scene_object(&s, r)->id);
    const u32 tc = fude_zoom_scene_find_object(&t, fude_zoom_scene_object(&s, c)->id);
    CHECK(tr != FUDE_ZOOM_NONE && tc != FUDE_ZOOM_NONE);
    if(tr != FUDE_ZOOM_NONE && tc != FUDE_ZOOM_NONE) {
        f64 nn[16];
        CHECK(fude_zoom_scene_shape_numbers(&t, tr, nn, 16) == 3 && nn[0] == 50 && nn[1] == 30);
        CHECK(fude_zoom_scene_object(&t, tr)->kind == FUDE_ZOOM_KIND_SHAPE && fude_zoom_scene_object(&t, tr)->channels == FUDE_ZOOM_SHAPE_RECT);
        CHECK(fabs(fude_zoom_scene_object(&t, tc)->scale - 2.0) < 1e-12 && alive(&t, tr));
        CHECK(fude_zoom_history_undo(&t) && fabs(fude_zoom_scene_object(&t, tc)->scale - 1.0) < 1e-12);
    }
    fude_zoom_scene_destroy(&t);
    fude_zoom_eraser_destroy(&e);
    fude_zoom_scene_destroy(&s);
}

// --- smoothing -------------------------------------------------------------------------------------

static void test_smoothing(void) {
    enum { N = 400 };
    static fude_zoom_v2 raw[N], out[N], live[N];
    // A straight line, a point every unit: it stays as it is, ends and all.
    for(u32 i = 0; i < N; i++) raw[i] = (fude_zoom_v2){ (f64)i, 0.0 };
    fude_zoom_smooth_points(raw, N, 3.0, out, 0);
    b8 straight = true;
    for(u32 i = 0; i < N; i++) straight = straight && fabs(out[i].y) < 1e-12 && fabs(out[i].x - raw[i].x) < 1e-9;
    CHECK(straight);
    // The same line, shaky (±2): the shake mostly goes; the ends stay where the pen was.
    for(u32 i = 0; i < N; i++) raw[i] = (fude_zoom_v2){ (f64)i * 1.5, (rndf() - 0.5) * 4.0 };
    fude_zoom_smooth_points(raw, N, 3.0, out, 0);
    f64 before = 0, after = 0;
    for(u32 i = 20; i < N - 20; i++) { before += raw[i].y * raw[i].y; after += out[i].y * out[i].y; }
    CHECK(after < before * 0.25);   // white noise through ~7 points of the window: ~1/7 of it left
    // The ends: where the pen was, less its shake (within it), and no hook — the
    // last stretch runs on level like the rest of the line.
    CHECK(hypot(out[0].x - raw[0].x, out[0].y) < 2.5 && hypot(out[N - 1].x - raw[N - 1].x, out[N - 1].y) < 2.5);
    CHECK(fabs(out[N - 1].y - out[N - 6].y) < 1.5 && fabs(out[5].y - out[0].y) < 1.5);
    // A circle keeps its size (the window is small beside it: it barely shrinks).
    for(u32 i = 0; i < N; i++) { const f64 a = 6.2831853 * i / (N - 1); raw[i] = (fude_zoom_v2){ 100 * cos(a), 100 * sin(a) }; }
    fude_zoom_smooth_points(raw, N, 3.0, out, 0);
    CHECK(fabs(hypot(out[N / 2].x, out[N / 2].y) - 100.0) < 0.1);
    // As drawn: a point at a time, only the unsettled end smoothed again each
    // time — the same line as smoothing it whole at the end.
    for(u32 i = 0; i < N; i++) raw[i] = (fude_zoom_v2){ (f64)i * 1.2 + (rndf() - 0.5) * 3.0, sin(i * 0.05) * 30.0 + (rndf() - 0.5) * 3.0 };
    fude_zoom_smooth_points(raw, N, 4.0, out, 0);
    u32 from = 0;
    for(u32 n = 1; n <= N; n++) {
        if(n >= 3) from = fude_zoom_smooth_points(raw, n, 4.0, live, from);
        else for(u32 i = 0; i < n; i++) live[i] = raw[i];
        CHECK(from <= n);
    }
    f64 worst = 0;
    for(u32 i = 0; i < N; i++) worst = fmax(worst, hypot(live[i].x - out[i].x, live[i].y - out[i].y));
    CHECK(worst < 1e-9);
    // Settled means settled: only the last three widths (and a sample) are still open.
    { f64 len = 0; for(u32 i = from + 1; i < N; i++) len += hypot(raw[i].x - raw[i - 1].x, raw[i].y - raw[i - 1].y);
      CHECK(from < N && len < 12.0 + 3.0); }
    // No width: as drawn.
    fude_zoom_smooth_points(raw, N, 0.0, out, 0);
    CHECK(memcmp(raw, out, sizeof raw) == 0);
    // The rope: slack within its length; taut, the tip follows at its length.
    fude_zoom_v2 tip = { 0, 0 };
    CHECK(!fude_zoom_smooth_rope(&tip, (fude_zoom_v2){ 15, 0 }, 20.0) && tip.x == 0 && tip.y == 0);
    CHECK(fude_zoom_smooth_rope(&tip, (fude_zoom_v2){ 50, 0 }, 20.0) && fabs(tip.x - 30.0) < 1e-12 && tip.y == 0);
    CHECK(fude_zoom_smooth_rope(&tip, (fude_zoom_v2){ 30, 40 }, 20.0) && fabs(tip.x - 30.0) < 1e-9 && fabs(tip.y - 20.0) < 1e-9);
    // Levels: off draws as is, high smooths most.
    CHECK(fude_zoom_smooth_sigma(FUDE_ZOOM_SMOOTH_OFF) == 0.0 && fude_zoom_smooth_sigma(FUDE_ZOOM_SMOOTH_HIGH) > fude_zoom_smooth_sigma(FUDE_ZOOM_SMOOTH_MEDIUM));
}

// --- getting around ----------------------------------------------------------------------------------

typedef struct { f64 last; u32 n; u32 order_ok; u32 want; } near_seen;
static b8 near_visit(any user, u32 value, fude_zoom_box box, fude_zoom_box leaf, f64 d2) {
    near_seen* q = (near_seen*)user; (void)value; (void)box; (void)leaf;
    q->order_ok += d2 >= q->last ? 1u : 0u;
    q->last = d2; q->n++;
    return q->n < q->want;
}

// Flown to _to: every step on the way a settled camera, nothing made, and it lands exactly there.
static b8 fly_and_check(fude_zoom_scene* s, fude_zoom_camera to, fude_zoom_v2 half) {
    fude_zoom_flight f;
    const u32 frames = fude_zoom_scene_frame_count(s);
    b8 ok = true;
    if(fude_zoom_fly_begin(&f, s, to, half, 0.0)) {
        for(u32 i = 1; i <= 200 && f.active; i++) {
            fude_zoom_fly_step(&f, s, half, f.length * (f64)i / 199.0);
            const f64 z = s->camera.z;
            ok = ok && isfinite(s->camera.at.x) && isfinite(s->camera.at.y) && isfinite(z);
            if(f.active) ok = ok && z >= FUDE_ZOOM_Z_MIN * 0.999 && z <= FUDE_ZOOM_Z_MAX * 1.001;
        }
    }
    ok = ok && !f.active && fude_zoom_scene_frame_count(s) == frames;
    ok = ok && s->camera.frame == to.frame && s->camera.at.x == to.at.x && s->camera.at.y == to.at.y && s->camera.z == to.z;
    return ok;
}

// Flown to _to and settled: the same depth, the same point in the middle (a view deeper in its frame
// than a frame shows lands in the frame below, as zooming in does).
static b8 fly_near(fude_zoom_scene* s, fude_zoom_camera to, fude_zoom_v2 half) {
    fude_zoom_flight f;
    if(fude_zoom_fly_begin(&f, s, to, half, 0.0)) {
        for(u32 i = 1; i <= 200 && f.active; i++) fude_zoom_fly_step(&f, s, half, f.length * (f64)i / 199.0);
    }
    const fude_zoom_v2 at = fude_zoom_sim_apply(fude_zoom_scene_sim(s, to.frame, s->camera.frame), to.at);
    return !f.active && fabs(fude_zoom_nav_depth(s) - fude_zoom_nav_depth_of(s, to)) < 1e-9 && hypot(at.x - s->camera.at.x, at.y - s->camera.at.y) * s->camera.z < 1e-3;
}

static void test_navigation(void) {
    // Nearest first: in order, and the first is brute force's nearest.
    {
        enum { N = 2000 };
        static fude_zoom_box boxes[N]; static u32 values[N];
        for(u32 i = 0; i < N; i++) { const f64 x = rndf() * 1e4, y = rndf() * 1e4, w = rndf() * 30; boxes[i] = (fude_zoom_box){ x, y, x + w, y + w }; values[i] = i; }
        fude_zoom_index ix; fude_zoom_index_init(&ix);
        for(u32 i = 0; i < N; i++) fude_zoom_index_insert(&ix, values[i], boxes[i]);
        const fude_zoom_v2 at = { 5000, 5000 };
        near_seen q = { -1.0, 0, 0, 300 };
        fude_zoom_index_nearest(&ix, at, near_visit, &q, 100000u);
        CHECK(q.n == 300 && q.order_ok == 300);
        f64 best = 1e300;
        for(u32 i = 0; i < N; i++) {
            const f64 dx = fmax(fmax(boxes[i].min_x - at.x, at.x - boxes[i].max_x), 0.0), dy = fmax(fmax(boxes[i].min_y - at.y, at.y - boxes[i].max_y), 0.0);
            best = fmin(best, dx * dx + dy * dy);
        }
        near_seen one = { -1.0, 0, 0, 1 };
        fude_zoom_index_nearest(&ix, at, near_visit, &one, 100000u);
        CHECK(one.n == 1 && fabs(one.last - best) < 1e-9);
        fude_zoom_index_destroy(&ix);
    }

    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const fude_zoom_v2 half = { 500, 400 };
    // A stroke at the top, then eight levels down, one at each: the deep view.
    line(&s, -50, 0, 50, 0, 2.0, 2.0f);
    for(u32 l = 0; l < 8; l++) {
        for(u32 k = 0; k < 10; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&s, half); }
        const f64 d = 100.0 / s.camera.z;
        line(&s, s.camera.at.x - d, s.camera.at.y, s.camera.at.x + d, s.camera.at.y, d / 40.0, (f32)(2.0 / s.camera.z));
    }
    const fude_zoom_camera deep = s.camera;
    CHECK(fude_zoom_scene_depth(&s, deep.frame) >= 8);
    // The depth, said: 1024^8 in is 10^24.
    CHECK(fabs(fude_zoom_nav_depth(&s) - log10(deep.z) - 8.0 * log10(1024.0)) < 1e-9);
    // Out to the top in a flight, and back down to the deep view.
    const fude_zoom_camera top = { s.root, { 30, 0 }, 1.0 };
    CHECK(fly_and_check(&s, top, half));
    CHECK(fly_and_check(&s, deep, half));
    // A second branch: a stroke far to the side at level 3; from the deep view across to it.
    fude_zoom_camera_look_at(&s, s.root, (fude_zoom_v2){ 4000, 3000 }, 1.0);
    for(u32 k = 0; k < 30; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&s, half); }
    { const f64 d = 100.0 / s.camera.z; line(&s, s.camera.at.x - d, s.camera.at.y, s.camera.at.x + d, s.camera.at.y, d / 40.0, (f32)(2.0 / s.camera.z)); }
    const fude_zoom_camera side = s.camera;
    CHECK(side.frame != deep.frame);
    fude_zoom_camera_look_at(&s, deep.frame, deep.at, deep.z);
    CHECK(fly_and_check(&s, side, half));
    CHECK(fly_and_check(&s, deep, half));
    // Already there: nothing to fly.
    { fude_zoom_flight f; CHECK(!fude_zoom_fly_begin(&f, &s, deep, half, 0.0) && s.camera.frame == deep.frame); }
    fude_zoom_scene_destroy(&s);

    // Marks: strokes far left and far right of an empty screen — an arrow each way.
    fude_zoom_scene m; fude_zoom_scene_init(&m, 7);
    line(&m, -3000, 0, -2900, 0, 2.0, 2.0f);
    const u32 far_right = line(&m, 2900, 10, 3000, 10, 2.0, 2.0f);
    fude_zoom_nav_mark marks[FUDE_ZOOM_NAV_MARKS];
    u32 n = fude_zoom_nav_marks(&m, half, 30.0f, marks);
    CHECK(n == 2);
    b8 left = false, right = false;
    for(u32 i = 0; i < n; i++) {
        left  = left  || (!marks[i].ring && fabsf(fabsf(marks[i].angle) - 3.14159f) < 0.05f && fabsf(marks[i].at.x + 470.0f) < 1.0f);
        right = right || (!marks[i].ring && fabsf(marks[i].angle) < 0.05f && fabsf(marks[i].at.x - 470.0f) < 1.0f);
    }
    CHECK(left && right);
    // Moved, a thing is in the index twice until it is built again (its old entry stale, where it was): an arrow
    // pointed both ways, one of them to nothing. It points only where the thing is now. Moved back, it has two entries
    // with its own box, and is still one arrow (and, far out, one ring: below).
    {
        fude_zoom_place p = fude_zoom_scene_place_of(&m, far_right);
        p.t.y += 3000.0;
        fude_zoom_scene_set_place(&m, far_right, p);
        CHECK(fude_zoom_scene_frame(&m, m.root)->stale == 1u);
        n = fude_zoom_nav_marks(&m, half, 30.0f, marks);
        b8 up = false;
        right = false;
        for(u32 i = 0; i < n; i++) {
            up    = up    || (!marks[i].ring && fabsf(marks[i].angle - 0.785f) < 0.1f);
            right = right || (!marks[i].ring && fabsf(marks[i].angle) < 0.05f);
        }
        CHECK(n == 2 && up && !right);
        p.t.y -= 3000.0;
        fude_zoom_scene_set_place(&m, far_right, p);
        n = fude_zoom_nav_marks(&m, half, 30.0f, marks);
        up = right = false;
        for(u32 i = 0; i < n; i++) {
            up    = up    || (!marks[i].ring && fabsf(marks[i].angle - 0.785f) < 0.1f);
            right = right || (!marks[i].ring && fabsf(marks[i].angle) < 0.05f && fabsf(marks[i].at.x - 470.0f) < 1.0f);
        }
        CHECK(n == 2 && right && !up && fude_zoom_scene_frame(&m, m.root)->stale == 2u);
    }
    // A layer's record (a point at the origin, never drawn) and what a hidden layer has: no arrow to
    // either (an arrow to the record flew there zoomed in without end).
    {
        fude_zoom_camera_look_at(&m, m.root, (fude_zoom_v2){ 0, 4000 }, 1.0);
        const u32 rec = fude_zoom_scene_add_layer(&m, m.root, 2, FUDE_ZOOM_LAYER_HIDDEN, "Hidden");
        m.layer = 2;
        const u32 hid = line(&m, 0, 2800, 100, 2800, 2.0, 2.0f);   // straight below the camera, on the hidden layer
        m.layer = 0;
        fude_zoom_scene_layers_refresh(&m);
        n = fude_zoom_nav_marks(&m, half, 30.0f, marks);
        b8 below = false;
        for(u32 i = 0; i < n; i++) {
            below = below || (!marks[i].ring && fabsf(marks[i].angle + 1.5708f) < 0.3f);
        }
        CHECK(!below && n == 2);   // only the two lines still drawn (down-left and down-right)
        // Nor to a guide (it reaches far past what it helps draw) or a constraint (an attribute of what it names),
        // which what is drawn there (fude_zoom_nav_drawn) leaves out too: an arrow to one led to an empty view.
        {
            const rde_color ink = { 0, 0, 0, 255 };
            const u32 guide = fude_zoom_scene_add_shape(&m, m.root, (fude_zoom_place){ { 0, 2800 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_GUIDE,
                                                        (const f64[3]){ 1, 0, 100 }, 3u, ink, 2.0f, 0u, 0);
            const u32 tie = fude_zoom_scene_add_shape(&m, m.root, (fude_zoom_place){ { 50, 2700 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_CONSTRAINT,
                                                      (const f64[3]){ FUDE_ZOOM_CONSTRAINT_PARALLEL, 0, 0 }, 3u, ink, 2.0f, 0u, 0);
            n = fude_zoom_nav_marks(&m, half, 30.0f, marks);
            CHECK(n == 2);
            fude_zoom_scene_set_alive(&m, guide, false);
            fude_zoom_scene_set_alive(&m, tie, false);
        }
        fude_zoom_scene_set_alive(&m, rec, false);
        fude_zoom_scene_layers_refresh(&m);
        n = fude_zoom_nav_marks(&m, half, 30.0f, marks);
        CHECK(n == 3);   // shown again: an arrow to it
        // A box with nothing in it is framed sanely (never a zoom without end).
        const fude_zoom_camera p = fude_zoom_nav_framing(half, m.root, (fude_zoom_box){ 0, 0, 0, 0 }, 0.6);
        CHECK(isfinite(p.z) && p.z < 1e9);
        fude_zoom_scene_set_alive(&m, hid, false);
        fude_zoom_camera_look_at(&m, m.root, (fude_zoom_v2){ 0, 0 }, 1.0);
    }
    // Far out, both on screen but specks (a point across): a ring round each, no arrows.
    fude_zoom_camera_look_at(&m, m.root, (fude_zoom_v2){ 0, 0 }, 0.01);
    n = fude_zoom_nav_marks(&m, half, 30.0f, marks);
    CHECK(n == 2 && marks[0].ring && marks[1].ring);
    // A tap's framing shows the thing in the middle, most of the screen.
    const fude_zoom_camera c = fude_zoom_nav_framing(half, m.root, (fude_zoom_box){ -3000, -10, -2900, 10 }, 0.6);
    CHECK(c.frame == m.root && fabs(c.at.x + 2950) < 1e-9 && fabs(c.z - 6.0) < 1e-9);
    fude_zoom_scene_destroy(&m);

    // Home stays home: out past the top (a new root above), the depth is under
    // ×1 — and so it is once the file is opened again.
    {
        fude_zoom_scene h; fude_zoom_scene_init(&h, 7);
        line(&h, -50, 0, 50, 0, 2.0, 2.0f);
        const fude_zoom_id home = fude_zoom_scene_frame(&h, h.home)->id;
        for(u32 k = 0; k < 14; k++) { fude_zoom_camera_zoom_at(&h, (fude_zoom_v2){ 0, 0 }, 0.5); fude_zoom_camera_settle(&h, half); }
        CHECK(h.root != h.home && fude_zoom_scene_frame(&h, h.home)->id == home);
        CHECK(fabs(fude_zoom_nav_depth(&h) - log10(pow(0.5, 14))) < 1e-9);
        mkdir("./saves", 0755);
        remove("./saves/home.zoom"); remove("./saves/home.zoom.bak");
        fude_zoom_file fh; memset(&fh, 0, sizeof fh); snprintf(fh.path, sizeof fh.path, "%s", "./saves/home.zoom");
        CHECK(fude_zoom_file_flush(&fh, &h) && fude_zoom_file_checkpoint(&fh, &h));
        fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
        CHECK(fude_zoom_file_open(&g, "./saves/home.zoom", &t) == FUDE_LOAD_OK);
        CHECK(fude_zoom_scene_frame(&t, t.home)->id == home && t.home != t.root);
        fude_zoom_scene_destroy(&t);
        fude_zoom_scene_destroy(&h);
    }

    // The depth's levels. Out twelve levels past a drawing (a frame made at each on the way, nothing in
    // it but the one below): one level, the drawing framed — said as a fair zoom (it was ×8·10⁻³⁷, the
    // top at its zoom 1, with seven more like it on the way down), and landed on exactly.
    {
        fude_zoom_scene h; fude_zoom_scene_init(&h, 7);
        line(&h, -50, 0, 50, 0, 2.0, 2.0f);
        for(u32 k = 0; k < 120; k++) { fude_zoom_camera_zoom_at(&h, (fude_zoom_v2){ 0, 0 }, 0.5); fude_zoom_camera_settle(&h, half); }
        CHECK(fude_zoom_scene_depth(&h, h.home) >= 11u);
        fude_zoom_camera v[8];
        u32 n = fude_zoom_nav_levels(&h, half, v, 8u);
        const f64 framed = log10(2.0 * 500.0 * 0.8 / 104.0);   // the line and its ends' round caps, 104 across
        CHECK(n == 1u && v[0].frame == h.home && fabs(fude_zoom_nav_depth_of(&h, v[0]) - framed) < 0.02);
        CHECK(fly_and_check(&h, v[0], half));
        // There: the one level is where the camera is — still offered (the list is never empty).
        n = fude_zoom_nav_levels(&h, half, v, 8u);
        CHECK(n == 1u && v[0].frame == h.home);
        // In closer, and a second drawing two levels up, off to the side: two levels, the first
        // drawing and then both — the levels made on the way out past them, none.
        fude_zoom_camera_look_at(&h, h.home, (fude_zoom_v2){ 0, 0 }, 40.0);
        u32 up = h.home;
        for(u32 k = 0; k < 2u; k++) up = fude_zoom_scene_frame(&h, up)->parent;
        fude_zoom_camera_look_at(&h, up, (fude_zoom_v2){ 0.5, 0 }, 1000.0);
        line(&h, 0.4, 0.0, 0.6, 0.0, 0.004, 0.002f);
        fude_zoom_camera_look_at(&h, h.home, (fude_zoom_v2){ 0, 0 }, 40.0);
        n = fude_zoom_nav_levels(&h, half, v, 8u);
        CHECK(n == 2u && v[0].frame == h.home && v[1].frame == up);
        CHECK(fude_zoom_nav_depth_of(&h, v[1]) < fude_zoom_nav_depth_of(&h, v[0]) - 3.0);
        CHECK(fly_near(&h, v[1], half) && fly_and_check(&h, v[0], half));
        // At most _max: the nearest, and the outermost last.
        CHECK(fude_zoom_nav_levels(&h, half, v, 1u) == 1u && v[0].frame == up);
        fude_zoom_scene_destroy(&h);
        // Nothing drawn: the top at its zoom 1.
        fude_zoom_scene e; fude_zoom_scene_init(&e, 7);
        CHECK(fude_zoom_nav_levels(&e, half, v, 8u) == 1u && v[0].frame == e.root && v[0].z == 1.0);
        fude_zoom_scene_destroy(&e);
    }

    // An arrow to what is drawn in a frame made by zooming in: its drawing, from that frame — not the
    // view it was made for, many times wider (its preview was a speck in the middle of nothing).
    {
        fude_zoom_scene a; fude_zoom_scene_init(&a, 7);
        line(&a, -50, 0, 50, 0, 2.0, 2.0f);
        fude_zoom_camera_look_at(&a, a.root, (fude_zoom_v2){ 3000, 0 }, 1.0);
        for(u32 k = 0; k < 12; k++) { fude_zoom_camera_zoom_at(&a, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&a, half); }
        const u32 deep = a.camera.frame;
        CHECK(deep != a.root && fude_zoom_scene_frame(&a, deep)->parent == a.root);
        { const f64 d = 2.0 / a.camera.z; line(&a, a.camera.at.x - d, a.camera.at.y, a.camera.at.x + d, a.camera.at.y, d / 40.0, (f32)(1.0 / a.camera.z)); }
        const fude_zoom_box made = fude_zoom_scene_object(&a, fude_zoom_scene_frame(&a, deep)->object)->box;
        CHECK(made.max_x - made.min_x > 1.0);   // the view it was made for: units across
        fude_zoom_camera_look_at(&a, a.root, (fude_zoom_v2){ 1500, 0 }, 1.0);
        fude_zoom_nav_mark marks[FUDE_ZOOM_NAV_MARKS];
        const u32 n = fude_zoom_nav_marks(&a, half, 30.0f, marks);
        i32 right = -1;
        for(u32 i = 0; i < n; i++) if(!marks[i].ring && fabsf(marks[i].angle) < 0.1f) right = (i32)i;
        CHECK(right >= 0 && marks[right].frame == deep);
        const fude_zoom_box got = marks[right].box;
        const fude_zoom_box drawn = fude_zoom_nav_drawn(&a, deep, FUDE_ZOOM_NONE);
        CHECK(memcmp(&got, &drawn, sizeof got) == 0 && got.max_x - got.min_x < 10.0);
        // ...and the map's whole is the drawing too, not the view either was made for.
        const fude_zoom_box all = fude_zoom_map_contents(&a);
        CHECK(all.max_x < 3000.0 + 0.01 && all.max_x > 3000.0 - 0.01 && all.min_x < -49.0);
        fude_zoom_scene_destroy(&a);
    }

    // Bookmarks (MARK objects): undone and redone as any change, kept through
    // the journal, a checkpoint, a reopen (undone again there) and a compaction.
    {
        fude_zoom_scene h; fude_zoom_scene_init(&h, 7);
        line(&h, -50, 0, 50, 0, 2.0, 2.0f);
        mkdir("./saves", 0755);
        remove("./saves/marks.zoom"); remove("./saves/marks.zoom.bak");
        fude_zoom_file fh; memset(&fh, 0, sizeof fh); snprintf(fh.path, sizeof fh.path, "%s", "./saves/marks.zoom");
        const fude_zoom_box vb = { -10, -10, 10, 10 };
        const u32 m1 = fude_zoom_scene_add_mark(&h, h.root, (fude_zoom_v2){ 10, 20 }, 2.0, 1);
        fude_zoom_history_push(&h, h.root, vb, NULL, 0, &m1, 1);
        CHECK(fude_zoom_file_flush(&fh, &h) && fude_zoom_file_checkpoint(&fh, &h));
        const u32 m2 = fude_zoom_scene_add_mark(&h, h.root, (fude_zoom_v2){ -5, 7 }, 0.5, 2);
        fude_zoom_history_push(&h, h.root, vb, NULL, 0, &m2, 1);
        fude_zoom_scene_set_alive(&h, m1, false);
        fude_zoom_history_push(&h, h.root, vb, &m1, 1, NULL, 0);
        CHECK(fude_zoom_history_undo(&h) && alive(&h, m1));
        CHECK(fude_zoom_history_redo(&h) && !alive(&h, m1));
        f64 z = 0; u32 number = 0;
        CHECK(fude_zoom_scene_mark_of(&h, m2, &z, &number) && z == 0.5 && number == 2 && !fude_zoom_scene_mark_of(&h, 0, NULL, NULL));
        CHECK(fude_zoom_file_flush(&fh, &h));   // the last two in the journal only
        fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
        CHECK(fude_zoom_file_open(&g, "./saves/marks.zoom", &t) == FUDE_LOAD_OK);
        const u32 t1 = fude_zoom_scene_find_object(&t, fude_zoom_scene_object(&h, m1)->id);
        const u32 t2 = fude_zoom_scene_find_object(&t, fude_zoom_scene_object(&h, m2)->id);
        CHECK(t1 != FUDE_ZOOM_NONE && t2 != FUDE_ZOOM_NONE);
        if(t1 != FUDE_ZOOM_NONE && t2 != FUDE_ZOOM_NONE) {
            CHECK(!alive(&t, t1) && alive(&t, t2) && fude_zoom_scene_object(&t, t2)->kind == FUDE_ZOOM_KIND_MARK);
            CHECK(fude_zoom_scene_mark_of(&t, t2, &z, &number) && z == 0.5 && number == 2 && fude_zoom_scene_object(&t, t2)->t.x == -5);
            CHECK(fude_zoom_history_undo(&t) && alive(&t, t1));   // letting the first go, undone after reopening
        }
        CHECK(fude_zoom_file_compact(&g, &t));
        fude_zoom_scene u; fude_zoom_scene_init(&u, 7); fude_zoom_file gu;
        CHECK(fude_zoom_file_open(&gu, "./saves/marks.zoom", &u) == FUDE_LOAD_OK);
        u32 marks = 0;
        for(u32 i = 0; i < fude_zoom_scene_object_count(&u); i++) marks += fude_zoom_scene_object(&u, i)->kind == FUDE_ZOOM_KIND_MARK && alive(&u, i) ? 1u : 0u;
        CHECK(marks == 2);
        fude_zoom_scene_destroy(&u);
        fude_zoom_scene_destroy(&t);
        fude_zoom_scene_destroy(&h);
    }

    // Said briefly.
    c8 b[32];
    fude_zoom_nav_say(0.0, b, sizeof b);            CHECK(strcmp(b, "\xC3\x97" "1") == 0);
    fude_zoom_nav_say(log10(12.0), b, sizeof b);    CHECK(strcmp(b, "\xC3\x97" "12") == 0);
    fude_zoom_nav_say(log10(0.5), b, sizeof b);     CHECK(strcmp(b, "\xC3\x97" "0.5") == 0);
    fude_zoom_nav_say(log10(0.004), b, sizeof b);   CHECK(strcmp(b, "\xC3\x97" "0.004") == 0);
    fude_zoom_nav_say(6.0, b, sizeof b);            CHECK(strcmp(b, "\xC3\x97" "10\xE2\x81\xB6") == 0);
    fude_zoom_nav_say(log10(1024.0), b, sizeof b);  CHECK(strcmp(b, "\xC3\x97" "1024") != 0 && strcmp(b, "\xC3\x97" "10\xC2\xB3") == 0);
    fude_zoom_nav_say(log10(3162.0), b, sizeof b);  CHECK(strcmp(b, "\xC3\x97" "3\xC2\xB7" "10\xC2\xB3") == 0);
    fude_zoom_nav_say(log10(4e-6), b, sizeof b);    CHECK(strcmp(b, "\xC3\x97" "4\xC2\xB7" "10\xE2\x81\xBB\xE2\x81\xB6") == 0);
    fude_zoom_nav_say(24.0, b, sizeof b);           CHECK(strcmp(b, "\xC3\x97" "10\xC2\xB2\xE2\x81\xB4") == 0);
}

// --- filling ---------------------------------------------------------------------------------------

static f64 tri_area(const rde_arr* t) {
    const fude_zoom_v2* v = (const fude_zoom_v2*)t->memory; f64 a = 0;
    for(u32 i = 0; i + 2 < (u32)rde_arr_length(t); i += 3)
        a += fabs((v[i + 1].x - v[i].x) * (v[i + 2].y - v[i].y) - (v[i + 2].x - v[i].x) * (v[i + 1].y - v[i].y)) * 0.5;
    return a;
}

// A cut fill's edge as it shows (fill.h's edges_rings): how long, how many
// lines, and none of it inside a cut or outside the outline.
static f64 edges_length(const fude_zoom_v2* p, const u32* rings, u32 n, u32* lines, b8 (*bad)(fude_zoom_v2)) {
    rde_arr e = rde_arr_new(sizeof(fude_zoom_v2), NULL), l = rde_arr_new(sizeof(u32), NULL);
    const u32 k = fude_zoom_fill_edges_rings(p, rings, n, &e, &l);
    const fude_zoom_v2* v = (const fude_zoom_v2*)e.memory;
    const u32*          w = (const u32*)l.memory;
    f64 len = 0;
    *lines = k > 0 ? 1u : 0u;
    for(u32 i = 0; i + 1 < k; i++) {
        if(w[i + 1] != w[i]) { (*lines)++; continue; }
        len += hypot(v[i + 1].x - v[i].x, v[i + 1].y - v[i].y);
        if(bad((fude_zoom_v2){ (v[i].x + v[i + 1].x) * 0.5, (v[i].y + v[i + 1].y) * 0.5 })) len += 1000.0;
    }
    rde_arr_free(&e); rde_arr_free(&l);
    return len;
}
static b8 edges_in_band(fude_zoom_v2 q) { return (q.x > 4.0 + 1e-9 && q.x < 6.0 - 1e-9) || q.y < -1e-9 || q.y > 10.0 + 1e-9 || q.x < -1e-9 || q.x > 10.0 + 1e-9; }
static b8 edges_never(fude_zoom_v2 q) { (void)q; return false; }
static b8 edges_in_cap(fude_zoom_v2 q) { return q.x > 4.0 + 1e-9 && q.x < 6.0 - 1e-9 && q.y > -1e-9 && q.y < 3.0 - 1e-9; }

static void test_fill_edges(void) {
    // A square, a band cut right through it (its level ends outside): two
    // rectangles' edges, all round each (the square's stretches and the band's
    // sides: four lines) — not the square's across the band, nor the band's
    // beyond the square.
    const fude_zoom_v2 sq[8] = { {0,0},{10,0},{10,10},{0,10}, {4,-5},{6,-5},{6,15},{4,15} };
    const u32          sr[8] = { 0,0,0,0, 1,1,1,1 };
    u32 lines = 0;
    f64 len = edges_length(sq, sr, 8, &lines, edges_in_band);
    CHECK(fabs(len - 56.0) < 1e-9 && lines == 4);
    // The band only part way in from the bottom (a bite): the square's line
    // round from one side of it to the other, and the bite's.
    const fude_zoom_v2 bi[8] = { {0,0},{10,0},{10,10},{0,10}, {4,-5},{6,-5},{6,3},{4,3} };
    len = edges_length(bi, sr, 8, &lines, edges_in_cap);
    CHECK(fabs(len - (40.0 - 2.0 + 3.0 + 3.0 + 2.0)) < 1e-9 && lines == 2);
    // Uncut: the outline as it is.
    len = edges_length(sq, sr, 4, &lines, edges_never);
    CHECK(fabs(len - 40.0) < 1e-9 && lines == 1);
    // A level stroke rubbed through the middle (the eraser's way slanted, its
    // own ends round): no hairline left bridging the gap.
    const fude_zoom_v2 ln[10] = { {-50,-2},{50,-2},{50,2},{-50,2}, {-3,-9},{1,-9},{3,9},{-1,9},{-2,0},{2,0} };
    const u32          lr[10] = { 0,0,0,0, 1,1,1,1,1,1 };
    rde_arr e = rde_arr_new(sizeof(fude_zoom_v2), NULL), l = rde_arr_new(sizeof(u32), NULL);
    const u32 k = fude_zoom_fill_edges_rings(ln, lr, 8, &e, &l);
    const fude_zoom_v2* v = (const fude_zoom_v2*)e.memory;
    const u32*          w = (const u32*)l.memory;
    b8 bridged = false;
    for(u32 i = 0; i + 1 < k; i++) {
        if(w[i + 1] != w[i]) continue;
        const fude_zoom_v2 m = { (v[i].x + v[i + 1].x) * 0.5, (v[i].y + v[i + 1].y) * 0.5 };
        const f64 cut_x = m.y / 9.0;   // the band's middle at that height
        if(fabs(m.x - cut_x) < 1.9) bridged = true;
    }
    CHECK(!bridged && k > 0);
    rde_arr_free(&e); rde_arr_free(&l);
}

static void test_filling(void) {
    rde_arr t = rde_arr_new(sizeof(fude_zoom_v2), NULL);
    static fude_zoom_v2 p[512];
    // A square, a star (concave), a figure 8 (crossing itself): their areas, exactly.
    const fude_zoom_v2 sq[4] = { { 0, 0 }, { 10, 0 }, { 10, 10 }, { 0, 10 } };
    CHECK(fude_zoom_fill_triangulate(sq, 4, &t) == 2 && fabs(tri_area(&t) - 100.0) < 1e-9);
    f64 shoelace = 0;
    for(u32 i = 0; i < 10; i++) { const f64 a = 6.283185307 * i / 10 + 1.5707963, r = i % 2 ? 20 : 50; p[i] = (fude_zoom_v2){ r * cos(a), r * sin(a) }; }
    for(u32 i = 0; i < 10; i++) shoelace += p[i].x * p[(i + 1) % 10].y - p[(i + 1) % 10].x * p[i].y;
    rde_arr_clear(&t); fude_zoom_fill_triangulate(p, 10, &t);
    CHECK(fabs(tri_area(&t) - fabs(shoelace) * 0.5) < 1e-6);
    for(u32 i = 0; i < 400; i++) { const f64 a = 6.283185307 * i / 400, d = 1 + sin(a) * sin(a); p[i % 512] = (fude_zoom_v2){ 100 * cos(a) / d, 100 * sin(a) * cos(a) / d }; }
    rde_arr_clear(&t); fude_zoom_fill_triangulate(p, 400, &t);
    CHECK(fabs(tri_area(&t) - 10000.0) < 10.0);   // a lemniscate's area is a²; this one is 400 corners
    CHECK(fude_zoom_fill_inside(p, 400, (fude_zoom_v2){ 50, 0 }) && fude_zoom_fill_inside(p, 400, (fude_zoom_v2){ -50, 0 }) && !fude_zoom_fill_inside(p, 400, (fude_zoom_v2){ 0, 40 }));
    // Cut to a box: a big square cut to the box is the box.
    const fude_zoom_v2 big[4] = { { -1e6, -1e6 }, { 1e6, -1e6 }, { 1e6, 1e6 }, { -1e6, 1e6 } };
    rde_arr cut = rde_arr_new(sizeof(fude_zoom_v2), NULL);
    const u32 k = fude_zoom_fill_clip(big, 4, (fude_zoom_box){ -5, -3, 5, 3 }, &cut);
    rde_arr_clear(&t); fude_zoom_fill_triangulate((const fude_zoom_v2*)cut.memory, k, &t);
    CHECK(fabs(tri_area(&t) - 60.0) < 1e-6);
    rde_arr_free(&cut); rde_arr_free(&t);

    // In a canvas: a fill and a shape filled in its own colour, through the file, undone, erased.
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_qpoint q[64];
    for(u32 i = 0; i < 64; i++) { const f64 a = 6.283185307 * i / 64; q[i] = (fude_zoom_qpoint){ (i32)llround((cos(a) - 1.0) * 1600.0), (i32)llround(sin(a) * 1600.0), 0, 0 }; }
    const u32 f = fude_zoom_scene_add_fill(&s, s.root, (fude_zoom_place){ { 100, 0 }, 0.0, 1.0 }, -4, q, 64, (rde_color){ 10, 20, 30, 255 }, 0);   // a circle, radius 100, round (0, 0)
    fude_zoom_history_push(&s, s.root, fude_zoom_scene_object(&s, f)->box, NULL, 0, &f, 1);
    CHECK(fude_zoom_scene_object(&s, f)->kind == FUDE_ZOOM_KIND_FILL && fabs(fude_zoom_scene_object(&s, f)->box.min_x + 100.0) < 0.1);
    const f64 rect[3] = { 50, 30, 0 };
    const u32 r = fude_zoom_scene_add_shape_fill(&s, s.root, (fude_zoom_place){ { 500, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, rect, 3, (rde_color){ 1, 2, 3, 255 }, 2.0f,
                                                 FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN, (rde_color){ 200, 100, 50, 255 }, 0);
    fude_zoom_history_push(&s, s.root, fude_zoom_scene_object(&s, r)->box, NULL, 0, &r, 1);
    mkdir("./saves", 0755);
    remove("./saves/fills.zoom"); remove("./saves/fills.zoom.bak");
    fude_zoom_file fz; memset(&fz, 0, sizeof fz); snprintf(fz.path, sizeof fz.path, "%s", "./saves/fills.zoom");
    CHECK(fude_zoom_file_flush(&fz, &s));
    fude_zoom_scene u; fude_zoom_scene_init(&u, 7); fude_zoom_file g;
    CHECK(fude_zoom_file_open(&g, "./saves/fills.zoom", &u) == FUDE_LOAD_OK);
    const u32 uf = fude_zoom_scene_find_object(&u, fude_zoom_scene_object(&s, f)->id);
    const u32 ur = fude_zoom_scene_find_object(&u, fude_zoom_scene_object(&s, r)->id);
    CHECK(uf != FUDE_ZOOM_NONE && ur != FUDE_ZOOM_NONE);
    if(uf != FUDE_ZOOM_NONE && ur != FUDE_ZOOM_NONE) {
        CHECK(fude_zoom_scene_object(&u, uf)->kind == FUDE_ZOOM_KIND_FILL && fude_zoom_scene_object(&u, uf)->count == 64 && fude_zoom_scene_object(&u, uf)->color.g == 20);
        CHECK((fude_zoom_scene_object(&u, ur)->flags & FUDE_ZOOM_FLAG_FILL_OWN) && fude_zoom_scene_object(&u, ur)->fill.r == 200 && fude_zoom_scene_object(&u, ur)->fill.b == 50);
        CHECK(fude_zoom_history_undo(&u) && !alive(&u, ur) && alive(&u, uf));
    }
    fude_zoom_scene_destroy(&u);
    // The eraser across the fill rubs out a band (a cut ring): what is left is
    // the circle less the band, and no longer has the middle.
    fude_zoom_eraser e; fude_zoom_eraser_init(&e);
    fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_PARTIAL);
    CHECK(fude_zoom_erase_step(&s, &e, s.root, (fude_zoom_v2){ -120, 0 }, (fude_zoom_v2){ 120, 0 }, 10.0, 0.5) == 1);
    fude_zoom_erase_end(&s, &e, s.root, (fude_zoom_box){ 0, 0, 1, 1 });
    CHECK(!alive(&s, f));
    u32 cutf = FUDE_ZOOM_NONE;
    for(u32 i = 0; i < fude_zoom_scene_object_count(&s); i++) if(fude_zoom_scene_object(&s, i)->kind == FUDE_ZOOM_KIND_FILL && alive(&s, i)) cutf = i;
    CHECK(cutf != FUDE_ZOOM_NONE && (fude_zoom_scene_object(&s, cutf)->channels & FUDE_ZOOM_CHANNEL_TIME));
    if(cutf != FUDE_ZOOM_NONE) {
        const fude_zoom_object* co = fude_zoom_scene_object(&s, cutf);
        rde_arr cqa, cpa, cra; fude_zoom_qpoint* cq = table(&cqa, sizeof(*cq), co->count); fude_zoom_v2* cp = table(&cpa, sizeof(*cp), co->count); u32* cr = table(&cra, sizeof(u32), co->count);
        fude_zoom_scene_points(&s, cutf, cq);
        for(u32 i = 0; i < co->count; i++) { cp[i] = fude_zoom_scene_point_at(co, &cq[i]); cr[i] = cq[i].time; }
        rde_arr ct = rde_arr_new(sizeof(fude_zoom_v2), NULL);
        fude_zoom_fill_triangulate_rings(cp, cr, co->count, &ct);
        const f64 circle = 0.5 * 64 * 100 * 100 * sin(6.283185307 / 64);
        CHECK(fabs(tri_area(&ct) - (circle - 20.0 * 200.0)) < circle * 0.02);   // less a band 20 wide across it (a little more at its rims)
        CHECK(!fude_zoom_fill_inside_rings(cp, cr, co->count, (fude_zoom_v2){ 0, 0 }) && fude_zoom_fill_inside_rings(cp, cr, co->count, (fude_zoom_v2){ 0, 50 }));
        rde_arr_free(&ct); rde_arr_free(&cqa); rde_arr_free(&cpa); rde_arr_free(&cra);
        // Undone: the whole circle back.
        CHECK(fude_zoom_history_undo(&s) && alive(&s, f) && !alive(&s, cutf));
        CHECK(fude_zoom_history_redo(&s) && !alive(&s, f) && alive(&s, cutf));
        // Rubbed out all over (in one sweep): gone, nothing left behind.
        fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_PARTIAL);
        for(f64 y = -110; y <= 110; y += 15) fude_zoom_erase_step(&s, &e, s.root, (fude_zoom_v2){ -120, y }, (fude_zoom_v2){ 120, y }, 12.0, 0.5);
        fude_zoom_erase_end(&s, &e, s.root, (fude_zoom_box){ 0, 0, 1, 1 });
        u32 left = 0;
        for(u32 i = 0; i < fude_zoom_scene_object_count(&s); i++) left += fude_zoom_scene_object(&s, i)->kind == FUDE_ZOOM_KIND_FILL && alive(&s, i) ? 1u : 0u;
        CHECK(left == 0);
    }
    fude_zoom_eraser_destroy(&e);
    fude_zoom_scene_destroy(&s);

    // A sweep too long for its width to be traced (its grid past fill.c's 4 million samples): the cut cannot be made,
    // and the fill is left as it is, not taken whole as if it had been rubbed out.
    {
        fude_zoom_scene ls; fude_zoom_scene_init(&ls, 7);
        const fude_zoom_qpoint lq[4] = { { 0, 0, 0, 0 }, { 32000, 0, 0, 0 }, { 32000, 32000, 0, 0 }, { 0, 32000, 0, 0 } };   // 2000 square
        const u32 lf = fude_zoom_scene_add_fill(&ls, ls.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, -4, lq, 4, (rde_color){ 9, 9, 9, 255 }, 0);
        fude_zoom_history_push(&ls, ls.root, fude_zoom_scene_object(&ls, lf)->box, NULL, 0, &lf, 1);
        const u32 actions = ls.action_count;
        fude_zoom_eraser le; fude_zoom_eraser_init(&le);
        fude_zoom_erase_begin(&le, FUDE_ZOOM_ERASE_PARTIAL);
        CHECK(fude_zoom_erase_step(&ls, &le, ls.root, (fude_zoom_v2){ 10, 10 }, (fude_zoom_v2){ 1990, 1990 }, 1.0, 0.5) == 0u);
        fude_zoom_erase_end(&ls, &le, ls.root, (fude_zoom_box){ 0, 0, 1, 1 });
        u32 fills = 0;
        for(u32 i = 0; i < fude_zoom_scene_object_count(&ls); i++) fills += fude_zoom_scene_object(&ls, i)->kind == FUDE_ZOOM_KIND_FILL && alive(&ls, i) ? 1u : 0u;
        CHECK(alive(&ls, lf) && fills == 1u && ls.action_count == actions);
        fude_zoom_eraser_destroy(&le);
        fude_zoom_scene_destroy(&ls);
    }

    // What a sweep costs: a fill of 600 corners, 240 steps across it, each step
    // cut again and its triangles made as the renderer would (printed: the
    // tablet needs it well under a frame).
    {
        fude_zoom_scene ps; fude_zoom_scene_init(&ps, 7);
        static fude_zoom_qpoint pq[600];
        for(u32 i = 0; i < 600; i++) { const f64 a = 6.283185307 * i / 600; pq[i] = (fude_zoom_qpoint){ (i32)llround((cos(a) * (1 + 0.1 * sin(a * 13)) - 1.0) * 4000.0), (i32)llround(sin(a) * 4000.0), 0, 0 }; }
        const u32 pf = fude_zoom_scene_add_fill(&ps, ps.root, (fude_zoom_place){ { 250, 0 }, 0.0, 1.0 }, -4, pq, 600, (rde_color){ 9, 9, 9, 255 }, 0);   // radius 250
        fude_zoom_history_push(&ps, ps.root, fude_zoom_scene_object(&ps, pf)->box, NULL, 0, &pf, 1);
        fude_zoom_eraser pe; fude_zoom_eraser_init(&pe);
        fude_zoom_erase_begin(&pe, FUDE_ZOOM_ERASE_PARTIAL);
        rde_arr tris = rde_arr_new(sizeof(fude_zoom_v2), NULL);
        const clock_t c0 = clock();
        f64 worst = 0;
        for(u32 k = 0; k < 240; k++) {
            const clock_t s0 = clock();
            const f64 t0 = (f64)k / 240.0, t1 = (f64)(k + 1) / 240.0;
            fude_zoom_erase_step(&ps, &pe, ps.root, (fude_zoom_v2){ -300 + 600 * t0, 200 * sin(t0 * 9) }, (fude_zoom_v2){ -300 + 600 * t1, 200 * sin(t1 * 9) }, 10.0, 0.5);
            // The fill standing now, triangulated.
            for(u32 i = fude_zoom_scene_object_count(&ps); i-- > 0;) {
                const fude_zoom_object* o = fude_zoom_scene_object(&ps, i);
                if(o->kind != FUDE_ZOOM_KIND_FILL || !(o->flags & FUDE_ZOOM_FLAG_ALIVE)) continue;
                rde_arr qa, pa, ra; fude_zoom_qpoint* q = table(&qa, sizeof(*q), o->count); fude_zoom_v2* p = table(&pa, sizeof(*p), o->count); u32* r = table(&ra, sizeof(u32), o->count);
                fude_zoom_scene_points(&ps, i, q);
                for(u32 j = 0; j < o->count; j++) { p[j] = fude_zoom_scene_point_at(o, &q[j]); r[j] = q[j].time; }
                rde_arr_clear(&tris);
                fude_zoom_fill_triangulate_rings(p, r, o->count, &tris);
                rde_arr_free(&qa); rde_arr_free(&pa); rde_arr_free(&ra);
                break;
            }
            worst = fmax(worst, (f64)(clock() - s0) * 1000.0 / CLOCKS_PER_SEC);
        }
        const f64 ms = (f64)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
        fude_zoom_erase_end(&ps, &pe, ps.root, (fude_zoom_box){ 0, 0, 1, 1 });
        printf("  a fill swept by the eraser: %.2f ms a step on average, %.2f at worst (240 steps, 600 corners)\n", ms / 240.0, worst);
        CHECK(worst < 16.0);
        rde_arr_free(&tris);
        fude_zoom_eraser_destroy(&pe);
        fude_zoom_scene_destroy(&ps);
    }

    // A stroke far wider than the eraser: rubbed along one side, it becomes its
    // outline with a notch there — its other side and the rest stay.
    {
        fude_zoom_scene ts; fude_zoom_scene_init(&ts, 7);
        const u32 th = line(&ts, -100, 0, 100, 0, 2.0, 20.0f);
        fude_zoom_eraser te; fude_zoom_eraser_init(&te);
        fude_zoom_erase_begin(&te, FUDE_ZOOM_ERASE_PARTIAL);
        fude_zoom_erase_step(&ts, &te, ts.root, (fude_zoom_v2){ -10, 18 }, (fude_zoom_v2){ 10, 18 }, 3.0, 0.5);
        fude_zoom_erase_end(&ts, &te, ts.root, (fude_zoom_box){ 0, 0, 1, 1 });
        CHECK(!alive(&ts, th));
        u32 tf = FUDE_ZOOM_NONE;
        for(u32 i = 0; i < fude_zoom_scene_object_count(&ts); i++) if(fude_zoom_scene_object(&ts, i)->kind == FUDE_ZOOM_KIND_FILL && alive(&ts, i)) tf = i;
        CHECK(tf != FUDE_ZOOM_NONE);
        if(tf != FUDE_ZOOM_NONE) {
            const fude_zoom_object* o = fude_zoom_scene_object(&ts, tf);
            rde_arr qa, pa, ra; fude_zoom_qpoint* q = table(&qa, sizeof(*q), o->count); fude_zoom_v2* p = table(&pa, sizeof(*p), o->count); u32* r = table(&ra, sizeof(u32), o->count);
            fude_zoom_scene_points(&ts, tf, q);
            for(u32 i = 0; i < o->count; i++) { p[i] = fude_zoom_scene_point_at(o, &q[i]); r[i] = q[i].time; }
            CHECK(!fude_zoom_fill_inside_rings(p, r, o->count, (fude_zoom_v2){ 0, 18 }));    // the notch
            CHECK(fude_zoom_fill_inside_rings(p, r, o->count, (fude_zoom_v2){ 0, -15 }));    // its other side
            CHECK(fude_zoom_fill_inside_rings(p, r, o->count, (fude_zoom_v2){ 50, 18 }));    // and along
            CHECK(fude_zoom_fill_inside_rings(p, r, o->count, (fude_zoom_v2){ -110, 0 }) && !fude_zoom_fill_inside_rings(p, r, o->count, (fude_zoom_v2){ 0, 25 }));   // its round end; not past its side
            rde_arr_free(&qa); rde_arr_free(&pa); rde_arr_free(&ra);
        }
        CHECK(fude_zoom_history_undo(&ts) && alive(&ts, th));   // one step: the stroke back
        fude_zoom_eraser_destroy(&te);
        fude_zoom_scene_destroy(&ts);
    }

    // A figure 8: the tap's lobe alone.
    {
        static fude_zoom_v2 eight[400];
        for(u32 i = 0; i < 400; i++) { const f64 a = 6.283185307 * i / 400, d = 1 + sin(a) * sin(a); eight[i] = (fude_zoom_v2){ 100 * cos(a) / d, 100 * sin(a) * cos(a) / d }; }
        rde_arr lobe = rde_arr_new(sizeof(fude_zoom_v2), NULL), lt = rde_arr_new(sizeof(fude_zoom_v2), NULL);
        const u32 ln = fude_zoom_fill_region(eight, 400, (fude_zoom_v2){ -50, 0 }, true, &lobe);
        CHECK(ln >= 3);
        fude_zoom_fill_triangulate((const fude_zoom_v2*)lobe.memory, ln, &lt);
        CHECK(fabs(tri_area(&lt) - 5000.0) < 20.0 && !fude_zoom_fill_inside((const fude_zoom_v2*)lobe.memory, ln, (fude_zoom_v2){ 50, 0 }));
        CHECK(fude_zoom_fill_region(eight, 400, (fude_zoom_v2){ 0, 60 }, true, &lobe) == 0);   // outside both
        // An "a": a bowl, then a tail running on past where it began. Its bowl
        // fills; the end is not joined back to the start (that is no loop it drew).
        static const fude_zoom_v2 a[] = {
            { 40, 40 }, { -20, 40 }, { -40, 0 }, { -20, -40 }, { 20, -40 }, { 35, 0 },   // the bowl, from its top right round
            { 30, 50 },                                                                  // up across its top: closed so
            { 35, -50 }, { 50, -60 },                                                    // the stem down and the tail
        };
        const u32 bn = fude_zoom_fill_region(a, 9, (fude_zoom_v2){ 0, 0 }, false, &lobe);
        CHECK(bn >= 3);
        // The bowl: from where the stem crosses its top, (31, 40), round to (35, 0).
        const fude_zoom_v2 bowl[6] = { { 31, 40 }, { -20, 40 }, { -40, 0 }, { -20, -40 }, { 20, -40 }, { 35, 0 } };
        f64 want = 0; for(u32 i = 0; i < 6; i++) want += bowl[i].x * bowl[(i + 1) % 6].y - bowl[(i + 1) % 6].x * bowl[i].y;
        rde_arr_clear(&lt); fude_zoom_fill_triangulate((const fude_zoom_v2*)lobe.memory, bn, &lt);
        CHECK(fabs(tri_area(&lt) - fabs(want) * 0.5) < 1.0);
        // Joined end to start, the same line has a loop round the tail too: not wanted for an "a".
        CHECK(fude_zoom_fill_region(a, 9, (fude_zoom_v2){ 40, -45 }, false, &lobe) == 0);
        rde_arr_free(&lobe); rde_arr_free(&lt);
    }
}

// --- export ----------------------------------------------------------------------------------------

// What an export walk hands on, in order: each thing's colour's red (the test's marks).
static u8  walked[16];
static u32 walked_n;
static void walked_stroke(any self, const fude_zoom_v2* p, const f64* r, u32 n, rde_color color) {
    (void)self; (void)p; (void)r; (void)n;
    if(walked_n < 16u) walked[walked_n++] = color.r;
}
static void walked_shape(any self, const fude_zoom_v2* p, u32 n, b8 closed, f64 radius, rde_color line_color, rde_color fill) {
    (void)self; (void)p; (void)n; (void)closed; (void)radius; (void)fill;
    if(walked_n < 16u) walked[walked_n++] = line_color.r;
}

// The SVG's colours as see-through as the things are: a shape's line and fill, each its own opacity, and a dot (a
// stroke of one point) its own (they were written solid; a dot of any alpha at 0.5).
static void test_svg_alpha(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const f64 rect[3] = { 50, 30, 0 };
    fude_zoom_scene_add_shape_fill(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, rect, 3, (rde_color){ 1, 2, 3, 128 }, 2.0f,
                                   FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN, (rde_color){ 255, 0, 0, 64 }, 0);
    const fude_zoom_qpoint dot = { 0, 0, 600, 0 };
    fude_zoom_scene_add_stroke(&s, s.root, (fude_zoom_v2){ 100, 100 }, 0, &dot, 1u, FUDE_ZOOM_CHANNEL_PRESSURE | FUDE_ZOOM_CHANNEL_TIME, (rde_color){ 7, 8, 9, 51 },
                               3.0f, FUDE_ZOOM_FLAG_FROM_PEN, 0, 0);
    // An opaque shape as before: no opacity written.
    fude_zoom_scene_add_shape_fill(&s, s.root, (fude_zoom_place){ { -150, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, rect, 3, (rde_color){ 4, 5, 6, 255 }, 2.0f,
                                   FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN, (rde_color){ 0, 255, 0, 255 }, 0);
    // A sticky note, its paper a quarter there and its ink half (both were written solid), and an
    // opaque text as before.
    fude_zoom_scene_add_text(&s, s.root, (fude_zoom_place){ { -300, 200 }, 0.0, 1.0 }, 12.0, 120.0, 40.0, FUDE_ZOOM_TEXT_STICKY, (rde_color){ 10, 20, 30, 128 },
                             (rde_color){ 255, 233, 140, 64 }, "half", 4u, 0);
    fude_zoom_scene_add_text(&s, s.root, (fude_zoom_place){ { 200, 200 }, 0.0, 1.0 }, 12.0, 120.0, 20.0, FUDE_ZOOM_TEXT_PLAIN, (rde_color){ 11, 21, 31, 255 },
                             (rde_color){ 0, 0, 0, 0 }, "whole", 5u, 0);
    fude_bytes b = fude_bytes_new(1024);
    CHECK(fude_zoom_export_svg(&s, (fude_zoom_v2){ 400, 300 }, (rde_color){ 255, 255, 255, 255 }, &b));
    rde_arr_add(&b, &(u8){ 0 });
    const c8* svg = (const c8*)b.memory;
    CHECK(strstr(svg, "fill=\"#ff0000\" fill-opacity=\"0.251\" stroke=\"#010203\" stroke-opacity=\"0.502\" stroke-width") != NULL);
    CHECK(strstr(svg, "fill=\"#070809\" fill-opacity=\"0.200\"") != NULL && strstr(svg, "fill-opacity=\"0.5\"") == NULL);
    CHECK(strstr(svg, "fill=\"#00ff00\" stroke=\"#040506\" stroke-width") != NULL);
    CHECK(strstr(svg, "fill=\"#ffe98c\" fill-opacity=\"0.251\"/>") != NULL);         // (the note's paper)
    CHECK(strstr(svg, "fill=\"#0a141e\" fill-opacity=\"0.502\">half</text>") != NULL); // (its ink)
    CHECK(strstr(svg, "fill=\"#0b151f\">whole</text>") != NULL);                      // (opaque: none written)
    rde_arr_free(&b);
    fude_zoom_scene_destroy(&s);
}

static void test_export(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const fude_zoom_v2 half = { 500, 400 };
    line(&s, -100, 0, 100, 0, 2.0, 3.0f);                               // a stroke
    const f64 rect[3] = { 50, 30, 0 };
    const u32 r = fude_zoom_scene_add_shape_fill(&s, s.root, (fude_zoom_place){ { 0, 200 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, rect, 3, (rde_color){ 1, 2, 3, 255 }, 2.0f,
                                                 FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN, (rde_color){ 255, 0, 0, 255 }, 0);
    (void)r;
    line(&s, 3000, 0, 3100, 0, 2.0, 3.0f);                              // off screen: left out
    // Deeper: a stroke in a child frame, drawn in its place.
    for(u32 k = 0; k < 7; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&s, half); }   // 128× in: a frame of its own
    CHECK(s.camera.frame != s.root);
    line(&s, s.camera.at.x - 100 / s.camera.z, s.camera.at.y + 60 / s.camera.z, s.camera.at.x + 100 / s.camera.z, s.camera.at.y + 60 / s.camera.z, 1.0 / s.camera.z, (f32)(2.0 / s.camera.z));
    for(u32 k = 0; k < 7; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 0.5); fude_zoom_camera_settle(&s, half); }
    fude_zoom_camera_look_at(&s, s.root, (fude_zoom_v2){ 0, 0 }, 1.0);
    fude_bytes b = fude_bytes_new(4096);
    CHECK(fude_zoom_export_svg(&s, half, (rde_color){ 240, 230, 210, 255 }, &b));
    rde_arr_add(&b, &(u8){ 0 });
    const c8* svg = (const c8*)b.memory;
    CHECK(strstr(svg, "<svg ") != NULL && strstr(svg, "viewBox=\"0 0 1000 800\"") != NULL && strstr(svg, "</svg>") != NULL);
    CHECK(strstr(svg, "fill=\"#ff0000\" stroke=\"#010203\"") != NULL);   // the rectangle, its own fill under its line
    // The stroke at the middle: from (400, 400) to (600, 400) in the SVG's own coordinates.
    CHECK(strstr(svg, "M400.00 400.00") != NULL && strstr(svg, "L600.00 400.00") != NULL);
    // Three paths only: the stroke, the rectangle, the child frame's stroke (the far one left out).
    u32 paths = 0; for(const c8* p = svg; (p = strstr(p, "<path")) != NULL; p++) paths++;
    CHECK(paths == 3);
    rde_arr_free(&b);

    // The same view as a PDF: a page the screen's size, the things on it as paths.
    fude_bytes pb = fude_bytes_new(4096);
    CHECK(fude_zoom_export_pdf(&s, half, (rde_color){ 240, 230, 210, 255 }, &pb));
    rde_arr_add(&pb, &(u8){ 0 });
    const c8* pdf = (const c8*)pb.memory;
    CHECK(strncmp(pdf, "%PDF-1.4", 8) == 0 && strstr(pdf, "/MediaBox [0 0 1000 800]") != NULL && strstr(pdf, "%%EOF") != NULL);
    CHECK(strstr(pdf, "400 400 m") != NULL && strstr(pdf, "600 400 l") != NULL);   // the stroke at the middle (its origin bottom left)
    rde_arr_free(&pb);

    // ...and for cutting: millimetres (here 2 a point), a layer for its colour, the far stroke left out.
    fude_bytes db = fude_bytes_new(4096);
    fude_zoom_export_check check;
    CHECK(fude_zoom_export_dxf(&s, half, 2.0, &db, &check));
    rde_arr_add(&db, &(u8){ 0 });
    const c8* dxf = (const c8*)db.memory;
    CHECK(strstr(dxf, "AC1009") != NULL && strstr(dxf, "$INSUNITS") != NULL && strstr(dxf, "POLYLINE") != NULL);
    CHECK(strstr(dxf, "-200") != NULL && strstr(dxf, "\r\n 10\r\n200\r\n") != NULL);   // the stroke, x from -200 mm to 200 mm
    CHECK(check.paths == 3u && check.open_paths == 2u);   // two strokes open; the rectangle closed
    rde_arr_free(&db);

    // A template to print: a tenth of a millimetre a point, 100 x 80 mm, one A4 page, as it is...
    fude_bytes tb = fude_bytes_new(4096);
    u32 pages = 0;
    CHECK(fude_zoom_export_tiles(&s, half, 0.1, 210.0, 297.0, (fude_zoom_v2){ 0.0, 0.0 }, "Square note", &tb, &pages) && pages == 1u);
    rde_arr_add(&tb, &(u8){ 0 });
    CHECK(strstr((const c8*)tb.memory, "/MediaBox [0 0 595.2756 841.8898]") != NULL);   // A4 exactly
    CHECK(strstr((const c8*)tb.memory, "1.020408 0 0 0.980392") == NULL);
    CHECK(strstr((const c8*)tb.memory, "(Square note)") != NULL);   // (the caller's words by the square, in its language)
    rde_arr_free(&tb);
    // ...and for a printer that prints 2% short across and 2% long down: drawn that much longer across, shorter down.
    tb = fude_bytes_new(4096);
    CHECK(fude_zoom_export_tiles(&s, half, 0.1, 210.0, 297.0, (fude_zoom_v2){ 0.98, 1.02 }, "Square note", &tb, &pages) && pages == 1u);
    rde_arr_add(&tb, &(u8){ 0 });
    CHECK(strstr((const c8*)tb.memory, "1.020408 0 0 0.980392") != NULL);
    rde_arr_free(&tb);
    // Over two pages (300 x 240 mm): the square's note on the first only, where the square is (it was on every page).
    tb = fude_bytes_new(4096);
    CHECK(fude_zoom_export_tiles(&s, half, 0.3, 210.0, 297.0, (fude_zoom_v2){ 0.0, 0.0 }, "Square note", &tb, &pages) && pages == 2u);
    rde_arr_add(&tb, &(u8){ 0 });
    u32 notes = 0;
    for(const c8* p = (const c8*)tb.memory; (p = strstr(p, "(Square note)")) != NULL; p++) notes++;
    CHECK(notes == 1u);
    rde_arr_free(&tb);
    // The printer's check: one page, its title and how to print it (the caller's words), its two
    // lines and their lengths.
    tb = fude_bytes_new(4096);
    CHECK(fude_zoom_export_print_check(210.0, 297.0, "Printer check", "Print at 100%", &tb));
    rde_arr_add(&tb, &(u8){ 0 });
    const c8* chk = (const c8*)tb.memory;
    CHECK(strstr(chk, "/Count 1") != NULL && strstr(chk, "Printer check") != NULL && strstr(chk, "Print at 100%") != NULL && strstr(chk, "X  150 mm") != NULL &&
          strstr(chk, "Y  200 mm") != NULL);
    rde_arr_free(&tb);
    fude_zoom_scene_destroy(&s);

    // In the order drawn, highlights too (Borja, from the tablet: "marker draws behind everything even if it is used
    // after what is painted on"; then "still behind others": under the writing it went over): writing, highlighted;
    // a board painted over it; highlighted again over the board, written on — each over what was there before it.
    fude_zoom_scene_init(&s, 7);
    const u32 ink1 = line(&s, -100, 0, 100, 0, 2.0, 2.0f);
    const u32 hi1  = line(&s, -100, 4, 100, 4, 2.0, 6.0f);
    const f64 brd[3] = { 120, 40, 0 };
    fude_zoom_scene_add_shape_fill(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, brd, 3, (rde_color){ 3, 0, 0, 255 }, 1.0f,
                                   FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN, (rde_color){ 200, 160, 90, 255 }, 0);
    const u32 hi2  = line(&s, -100, -4, 100, -4, 2.0, 6.0f);
    const u32 ink2 = line(&s, -100, -8, 100, -8, 2.0, 2.0f);
    const u32 marks[4] = { ink1, hi1, hi2, ink2 };
    const u8  reds[4]  = { 1, 2, 4, 5 };
    for(u32 k = 0; k < 4u; k++) {
        fude_zoom_object* o = (fude_zoom_object*)fude_zoom_scene_object(&s, marks[k]);
        o->color.r = reds[k];
        if(k == 1u || k == 2u) o->flags |= FUDE_ZOOM_FLAG_MARKER;
    }
    const fude_zoom_export_sink rec = { .self = NULL, .stroke = walked_stroke, .shape = walked_shape };
    walked_n = 0;
    fude_zoom_export_walk(&s, half, &rec);
    CHECK(walked_n == 5u && walked[0] == 1u && walked[1] == 2u && walked[2] == 3u && walked[3] == 4u && walked[4] == 5u);
    fude_zoom_scene_destroy(&s);
}

// --- the paint bucket -------------------------------------------------------------------------

static void test_bucket(void) {
    // A square of four separate lines, a gap of 4 at one corner; an island inside.
    const fude_zoom_bucket_ink ink[8] = {
        { { -100, -100 }, { 100, -100 }, 1.5 }, { { 100, -100 }, { 100, 100 }, 1.5 },
        { { 100, 100 }, { -100, 100 }, 1.5 },   { { -100, 100 }, { -100, -92 }, 1.5 },   // a gap of 5 between their edges
        { { 20, 20 }, { 50, 20 }, 1.5 }, { { 50, 20 }, { 50, 50 }, 1.5 }, { { 50, 50 }, { 20, 50 }, 1.5 }, { { 20, 50 }, { 20, 20 }, 1.5 },
    };
    const fude_zoom_box bounds = { -300, -300, 300, 300 };
    rde_arr out = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    rde_arr rings = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    // The gap closed (8 wide): filled to the lines' middles, the island left out.
    CHECK(fude_zoom_bucket_region(ink, 8, bounds, (fude_zoom_v2){ -50, -50 }, 1.0, 8.0, 1.5, &out, &rings) == FUDE_ZOOM_BUCKET_FILLED);
    const fude_zoom_v2* p = (const fude_zoom_v2*)out.memory;
    const u32* r = (const u32*)rings.memory;
    const u32 n = (u32)rde_arr_length(&out);
    CHECK(fude_zoom_fill_inside_rings(p, r, n, (fude_zoom_v2){ -50, -50 }) && fude_zoom_fill_inside_rings(p, r, n, (fude_zoom_v2){ 90, 90 }));
    CHECK(fude_zoom_fill_inside_rings(p, r, n, (fude_zoom_v2){ -99.5, 0 }));    // under the line: no seam
    CHECK(!fude_zoom_fill_inside_rings(p, r, n, (fude_zoom_v2){ -120, 0 }) && !fude_zoom_fill_inside_rings(p, r, n, (fude_zoom_v2){ 35, 35 }));   // outside; the island
    // A gap wider than closed: it runs out, nothing filled.
    CHECK(fude_zoom_bucket_region(ink, 8, bounds, (fude_zoom_v2){ -50, -50 }, 1.0, 2.0, 1.5, &out, &rings) == FUDE_ZOOM_BUCKET_OPEN);
    CHECK(rde_arr_length(&out) == 0);
    // On the ink itself.
    CHECK(fude_zoom_bucket_region(ink, 8, bounds, (fude_zoom_v2){ 0, -100 }, 1.0, 8.0, 1.5, &out, &rings) == FUDE_ZOOM_BUCKET_ON_INK);
    rde_arr_free(&out); rde_arr_free(&rings);
}

// --- offsets ------------------------------------------------------------------------------------

static void test_offsets(void) {
    const fude_zoom_v2 sq[4] = { { 0, 0 }, { 100, 0 }, { 100, 100 }, { 0, 100 } };
    rde_arr out = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    // Out by 5: 5 past each side, its corners round.
    CHECK(fude_zoom_fill_offset(sq, 4, 5.0, &out) > 8u);
    const fude_zoom_v2* p = (const fude_zoom_v2*)out.memory;
    u32 n = (u32)rde_arr_length(&out);
    CHECK(fude_zoom_fill_inside(p, n, (fude_zoom_v2){ 104, 50 }) && !fude_zoom_fill_inside(p, n, (fude_zoom_v2){ 106, 50 }));
    CHECK(fude_zoom_fill_inside(p, n, (fude_zoom_v2){ 102, 102 }) && !fude_zoom_fill_inside(p, n, (fude_zoom_v2){ 104.5, 104.5 }));   // round: under 5 from the corner
    // In by 5.
    CHECK(fude_zoom_fill_offset(sq, 4, -5.0, &out) >= 4u);
    p = (const fude_zoom_v2*)out.memory;
    n = (u32)rde_arr_length(&out);
    CHECK(fude_zoom_fill_inside(p, n, (fude_zoom_v2){ 50, 50 }) && fude_zoom_fill_inside(p, n, (fude_zoom_v2){ 94, 50 }) && !fude_zoom_fill_inside(p, n, (fude_zoom_v2){ 96, 50 }));
    rde_arr_free(&out);
}

// --- connectors ---------------------------------------------------------------------------------

static void test_connectors(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const f64 box[3] = { 20, 10, 0 };
    const u32 a = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, box, 3, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    const u32 b = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 100, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, box, 3, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    // Its ends on their edges, aimed at their middles: x 20 to x 80.
    const fude_zoom_v2 ea = fude_zoom_connect_edge(&s, a, fude_zoom_connect_middle(&s, b));
    const fude_zoom_v2 eb = fude_zoom_connect_edge(&s, b, fude_zoom_connect_middle(&s, a));
    CHECK(fabs(ea.x - 20) < 1e-6 && fabs(ea.y) < 1e-6 && fabs(eb.x - 80) < 1e-6);
    const u32 c = fude_zoom_connect_add(&s, s.root, ea, eb, a, b, FUDE_ZOOM_ARROW_END, (rde_color){ 1, 1, 1, 255 }, 0.5f);
    // b moved up 100 (as the lasso does): the connector follows, its end on b's bottom edge, its start on a's top.
    const fude_zoom_place bb = fude_zoom_scene_place_of(&s, b);
    const fude_zoom_place ba = fude_zoom_place_moved(bb, (fude_zoom_sim){ 1.0, 0.0, -100.0, 100.0 });   // to (0, 100)
    fude_zoom_scene_set_place(&s, b, ba);
    rde_arr ids = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr bef = rde_arr_new(sizeof(fude_zoom_place), rde_memory_allocator_get_default_std());
    rde_arr aft = rde_arr_new(sizeof(fude_zoom_place), rde_memory_allocator_get_default_std());
    CHECK(fude_zoom_connect_follow(&s, s.root, &b, 1, &ids, &bef, &aft) == 1u);
    CHECK(((const u32*)ids.memory)[0] == c);
    f64 n[FUDE_ZOOM_SHAPE_NUMBERS];
    CHECK(fude_zoom_scene_shape_numbers(&s, c, n, FUDE_ZOOM_SHAPE_NUMBERS) == FUDE_ZOOM_ARROW_NUMBERS(2u));
    {
        // Its ids kept in two halves each (scene.h's fude_zoom_id_put), as the scene keeps ids:
        // never one number's bits.
        fude_zoom_id from = 1u, to = 1u;
        CHECK(fude_zoom_connect_arrow_ends(n, FUDE_ZOOM_ARROW_NUMBERS(2u), 2u, &from, &to));
        CHECK(from == fude_zoom_scene_object(&s, a)->id && to == fude_zoom_scene_object(&s, b)->id);
        CHECK(fude_zoom_id_get(&n[6]) == from && fude_zoom_id_get(&n[8]) == to);
        CHECK(!fude_zoom_connect_arrow_ends(n, 7u, 2u, &from, &to) && from == 0u && to == 0u);   // (too few: none)
    }
    const fude_zoom_sim cs = fude_zoom_object_sim(fude_zoom_scene_object(&s, c));
    const fude_zoom_v2 p0 = fude_zoom_sim_apply(cs, (fude_zoom_v2){ n[1], n[2] }), p1 = fude_zoom_sim_apply(cs, (fude_zoom_v2){ n[3], n[4] });
    CHECK(fabs(p0.x) < 1e-6 && fabs(p0.y - 10) < 1e-6 && fabs(p1.x) < 1e-6 && fabs(p1.y - 90) < 1e-6);
    // Undone with the move as one step: the connector back where it was.
    const u32 moved[2] = { b, c };
    const fude_zoom_place before[2] = { bb, ((const fude_zoom_place*)bef.memory)[0] }, after[2] = { ba, ((const fude_zoom_place*)aft.memory)[0] };
    fude_zoom_history_push_moved(&s, s.root, (fude_zoom_box){ 0, 0, 1, 1 }, moved, before, after, 2);
    CHECK(fude_zoom_history_undo(&s));
    const fude_zoom_sim cu = fude_zoom_object_sim(fude_zoom_scene_object(&s, c));
    CHECK(fabs(fude_zoom_sim_apply(cu, (fude_zoom_v2){ n[3], n[4] }).x - 80) < 1e-6);
    // Not joined to what moved: left alone.
    rde_arr_clear(&ids);
    const u32 lone = fude_zoom_connect_add(&s, s.root, (fude_zoom_v2){ 0, 300 }, (fude_zoom_v2){ 50, 300 }, FUDE_ZOOM_NONE, FUDE_ZOOM_NONE, 0u, (rde_color){ 1, 1, 1, 255 }, 0.5f);
    (void)lone;
    CHECK(fude_zoom_connect_follow(&s, s.root, &a, 1, &ids, &bef, &aft) == 1u);   // only the joined one
    // An older connector (from a file made before its ids were kept in halves: each id one number's own bits) still
    // follows what it joins, and is written the new way when made again (its heads where they were).
    {
        const fude_zoom_id ia = fude_zoom_scene_object(&s, a)->id, ib = fude_zoom_scene_object(&s, b)->id, made_again = 12345u;
        f64 old[FUDE_ZOOM_ARROW_NUMBERS_BITS(2u)] = { 2.0, 0.0, 0.0, 60.0, 0.0, (f64)FUDE_ZOOM_ARROW_END, 0.0, 0.0 };
        memcpy(&old[6], &ia, sizeof ia); memcpy(&old[7], &ib, sizeof ib);
        const u32 oc = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 20, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ARROW, old,
                                                 FUDE_ZOOM_ARROW_NUMBERS_BITS(2u), (rde_color){ 1, 1, 1, 255 }, 0.5f, 0u, 0);
        fude_zoom_id from = 0u, to = 0u;
        CHECK(fude_zoom_connect_arrow_ends(old, FUDE_ZOOM_ARROW_NUMBERS_BITS(2u), 2u, &from, &to) && from == ia && to == ib);
        rde_arr_clear(&ids); rde_arr_clear(&bef); rde_arr_clear(&aft);
        CHECK(fude_zoom_connect_follow(&s, s.root, &a, 1, &ids, &bef, &aft) == 2u);   // (the new one and the older one)
        const u32 re = fude_zoom_connect_remap(&s, oc, &ia, &made_again, 1u, fude_zoom_scene_object(&s, oc)->layer);
        CHECK(re != FUDE_ZOOM_NONE && fude_zoom_scene_shape_numbers(&s, re, n, FUDE_ZOOM_SHAPE_NUMBERS) == FUDE_ZOOM_ARROW_NUMBERS(2u));
        CHECK(fude_zoom_connect_arrow_ends(n, FUDE_ZOOM_ARROW_NUMBERS(2u), 2u, &from, &to) && from == made_again && to == ib);
        CHECK(n[5] == (f64)FUDE_ZOOM_ARROW_END && fude_zoom_id_get(&n[6]) == made_again);
    }
    // Copied and pasted with what it joins, a connector is joined to the copies; with only one of them, its other end
    // is loose: a copy never follows the originals (it once kept their ids, so moving an original pulled the copy).
    {
        s.camera = (fude_zoom_camera){ s.root, { 0, 0 }, 1.0 };
        fude_zoom_selection sel; fude_zoom_select_init(&sel);
        for(u32 round = 0; round < 2u; round++) {
            const u32 picked[3] = { a, round == 0u ? b : c, c };
            fude_zoom_select_clear(&sel);
            for(u32 i = 0; i < (round == 0u ? 3u : 2u); i++) rde_arr_add(&sel.picks, &(fude_zoom_pick){ picked[i] });
            fude_zoom_select_update(&sel, &s);
            fude_zoom_select_copy(&sel, &s);
            const u32 base = fude_zoom_scene_object_count(&s);
            fude_zoom_select_paste(&sel, &s, (rde_vec_2F){ 0.0f, 300.0f });
            u32 rects[2] = { FUDE_ZOOM_NONE, FUDE_ZOOM_NONE }, arrow = FUDE_ZOOM_NONE, nr = 0;
            for(u32 i = base; i < fude_zoom_scene_object_count(&s); i++) {
                const fude_zoom_object* o = fude_zoom_scene_object(&s, i);
                if(o->channels == FUDE_ZOOM_SHAPE_ARROW) arrow = i;
                else if(o->channels == FUDE_ZOOM_SHAPE_RECT && nr < 2u) rects[nr++] = i;
            }
            CHECK(arrow != FUDE_ZOOM_NONE && nr == (round == 0u ? 2u : 1u));
            const u32 an = fude_zoom_scene_shape_numbers(&s, arrow, n, FUDE_ZOOM_SHAPE_NUMBERS);
            fude_zoom_id from = 1u, to = 1u;
            CHECK(an == FUDE_ZOOM_ARROW_NUMBERS(2u) && fude_zoom_connect_arrow_ends(n, an, 2u, &from, &to));
            CHECK(from == fude_zoom_scene_object(&s, rects[0])->id && to == (round == 0u ? fude_zoom_scene_object(&s, rects[1])->id : 0u));
        }
        fude_zoom_select_destroy(&sel);
    }
    rde_arr_free(&ids); rde_arr_free(&bef); rde_arr_free(&aft);
    fude_zoom_scene_destroy(&s);
}

// What the lasso holds moved onto another layer: copies where they were, in
// their order; a connector joined to one joined to its copy; one undo step.
static void test_layer_moves(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const f64 box[3] = { 20, 10, 0 };
    const u32 a = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, box, 3, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    const u32 b = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 100, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, box, 3, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    const u32 c = fude_zoom_connect_add(&s, s.root, (fude_zoom_v2){ 20, 0 }, (fude_zoom_v2){ 80, 0 }, a, b, FUDE_ZOOM_ARROW_END, (rde_color){ 1, 1, 1, 255 }, 0.5f);
    const u32 l = line(&s, 0, 50, 100, 50, 1.0, 1.0f);
    const u64 za = fude_zoom_scene_object(&s, a)->z;
    fude_zoom_selection sel; fude_zoom_select_init(&sel);
    rde_arr_add(&sel.picks, &(fude_zoom_pick){ a });
    rde_arr_add(&sel.picks, &(fude_zoom_pick){ l });
    CHECK(fude_zoom_select_to_layer(&sel, &s, 3) == 3u);   // a, the line, and the connector joined again
    CHECK(!alive(&s, a) && !alive(&s, l) && !alive(&s, c) && alive(&s, b));
    const u32 a2 = ((const fude_zoom_pick*)sel.picks.memory)[0].object, l2 = ((const fude_zoom_pick*)sel.picks.memory)[1].object;
    CHECK(alive(&s, a2) && alive(&s, l2) && fude_zoom_scene_object(&s, a2)->layer == 3 && fude_zoom_scene_object(&s, l2)->layer == 3);
    CHECK(fude_zoom_scene_object(&s, a2)->z == za && fude_zoom_scene_object(&s, a2)->box.max_x == fude_zoom_scene_object(&s, a)->box.max_x);
    u32 arrows = 0;
    for(u32 i = 0; i < fude_zoom_scene_object_count(&s); i++) {
        const fude_zoom_object* o = fude_zoom_scene_object(&s, i);
        if(!(o->flags & FUDE_ZOOM_FLAG_ALIVE) || o->kind != FUDE_ZOOM_KIND_SHAPE || o->channels != FUDE_ZOOM_SHAPE_ARROW) continue;
        f64 n[FUDE_ZOOM_SHAPE_NUMBERS];
        CHECK(fude_zoom_scene_shape_numbers(&s, i, n, FUDE_ZOOM_SHAPE_NUMBERS) == FUDE_ZOOM_ARROW_NUMBERS(2u));
        fude_zoom_id from = 0u, to = 0u;
        CHECK(fude_zoom_connect_arrow_ends(n, FUDE_ZOOM_ARROW_NUMBERS(2u), 2u, &from, &to));
        CHECK(from == fude_zoom_scene_object(&s, a2)->id && to == fude_zoom_scene_object(&s, b)->id && o->layer == 0);
        arrows++;
    }
    CHECK(arrows == 1u);
    // Moving b now: the joined-again connector follows it.
    rde_arr ids = rde_arr_new(sizeof(u32), NULL), bef = rde_arr_new(sizeof(fude_zoom_place), NULL), aft = rde_arr_new(sizeof(fude_zoom_place), NULL);
    fude_zoom_scene_set_place(&s, b, fude_zoom_place_moved(fude_zoom_scene_place_of(&s, b), (fude_zoom_sim){ 1.0, 0.0, 0.0, 100.0 }));
    CHECK(fude_zoom_connect_follow(&s, s.root, &b, 1, &ids, &bef, &aft) == 1u);
    rde_arr_free(&ids); rde_arr_free(&bef); rde_arr_free(&aft);
    fude_zoom_scene_set_place(&s, b, fude_zoom_place_moved(fude_zoom_scene_place_of(&s, b), (fude_zoom_sim){ 1.0, 0.0, 0.0, -100.0 }));
    // Already there: nothing to do.
    CHECK(fude_zoom_select_to_layer(&sel, &s, 3) == 0u);
    // One undo: as it was.
    CHECK(fude_zoom_history_undo(&s));
    CHECK(alive(&s, a) && alive(&s, l) && alive(&s, c) && !alive(&s, a2) && !alive(&s, l2));
    CHECK(fude_zoom_history_redo(&s));
    CHECK(!alive(&s, a) && alive(&s, a2) && alive(&s, l2) && !alive(&s, c));
    fude_zoom_select_destroy(&sel);
    fude_zoom_scene_destroy(&s);
}

// A lasso let go when a second finger makes a pinch (0.1.84: with fingers writing, the lasso tool kept the view still):
// a loop being drawn dropped, nothing taken; a drag dropped, the picks where they were, nothing to undo.
static void test_select_abort(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const f64 box[3] = { 20, 10, 0 };
    const u32 a = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, box, 3, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    fude_zoom_selection sel; fude_zoom_select_init(&sel);
    fude_zoom_select_down(&sel, &s, (rde_vec_2F){ 400, 400 });
    fude_zoom_select_moved(&sel, (rde_vec_2F){ 500, 400 });
    fude_zoom_select_moved(&sel, (rde_vec_2F){ 500, 500 });
    CHECK(fude_zoom_select_busy(&sel) && sel.grab == FUDE_ZOOM_GRAB_LOOP);
    fude_zoom_select_abort(&sel, &s);
    CHECK(!fude_zoom_select_busy(&sel) && rde_arr_length(&sel.loop) == 0u && !fude_zoom_select_any(&sel));
    rde_arr_add(&sel.picks, &(fude_zoom_pick){ a });
    fude_zoom_select_update(&sel, &s);
    const fude_zoom_place before = fude_zoom_scene_place_of(&s, a);
    fude_zoom_select_down(&sel, &s, (rde_vec_2F){ 0, 0 });
    CHECK(sel.grab == FUDE_ZOOM_GRAB_MOVE);
    fude_zoom_select_moved(&sel, (rde_vec_2F){ 60, 30 });
    fude_zoom_select_abort(&sel, &s);
    const fude_zoom_place after = fude_zoom_scene_place_of(&s, a);
    CHECK(!fude_zoom_select_busy(&sel) && fude_zoom_select_any(&sel) && after.t.x == before.t.x && after.t.y == before.t.y && !fude_zoom_history_undo(&s));
    fude_zoom_select_abort(&sel, &s);   // (nothing begun: nothing)
    CHECK(fude_zoom_select_any(&sel));
    fude_zoom_select_destroy(&sel);
    fude_zoom_scene_destroy(&s);
}

// Driving: the point at a dimension's end moved, and everything with an end or a corner there with it.
static void test_driving(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const rde_color c = { 1, 1, 1, 255 };
    const f64 ln[2] = { 100, 0 }, dim[3] = { 100, 0, 10 }, poly[6] = { 100, 0, 150, 50, 100, 50 }, rect[3] = { 25, 25, 0 }, up[2] = { 0, 100 };
    const u32 l = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, ln, 2, c, 1.0f, 0u, 0);
    const u32 d = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_DIMENSION, dim, 3, c, 0.5f, 0u, 0);
    const u32 p = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_POLYGON, poly, 6, c, 1.0f, 0u, 0);
    const u32 r = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 125, -25 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, rect, 3, c, 1.0f, 0u, 0);
    const u32 v = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 100, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, up, 2, c, 1.0f, 0u, 0);
    const u32 far = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 300, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, ln, 2, c, 1.0f, 0u, 0);
    rde_arr died = rde_arr_new(sizeof(u32), NULL), born = rde_arr_new(sizeof(u32), NULL);
    CHECK(fude_zoom_connect_drive(&s, s.root, (fude_zoom_v2){ 100, 0 }, (fude_zoom_v2){ 120, 0 }, 1e-6, d, &died, &born) == 4u);   // the dimension itself skipped
    CHECK(!alive(&s, l) && !alive(&s, p) && !alive(&s, r) && !alive(&s, v) && alive(&s, d) && alive(&s, far));
    f64 n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32* b = (const u32*)born.memory;
    // The line ends at 120 now; the polygon's corner is there; the rectangle's far corner stayed (150, -50).
    CHECK(fude_zoom_scene_shape_numbers(&s, b[0], n, 8) == 2u && fabs(n[0] - 120) < 1e-9 && fabs(n[1]) < 1e-9);
    CHECK(fude_zoom_scene_shape_numbers(&s, b[1], n, 8) == 6u && fabs(n[0] - 120) < 1e-9 && fabs(n[2] - 150) < 1e-9);
    CHECK(fude_zoom_scene_shape_numbers(&s, b[2], n, 8) == 3u && fabs(n[0] - 15) < 1e-9 && fabs(n[1] - 25) < 1e-9);
    const fude_zoom_place rp = fude_zoom_scene_place_of(&s, b[2]);
    CHECK(fabs(rp.t.x - 135) < 1e-9 && fabs(rp.t.y + 25) < 1e-9);
    // The line that started there starts at 120, its end where it was (100, 100).
    const fude_zoom_place vp = fude_zoom_scene_place_of(&s, b[3]);
    CHECK(fude_zoom_scene_shape_numbers(&s, b[3], n, 8) == 2u && fabs(vp.t.x - 120) < 1e-9 && fabs(vp.t.x + n[0] - 100) < 1e-9 && fabs(vp.t.y + n[1] - 100) < 1e-9);
    // Turned and scaled, the same: a line at 90 degrees, twice its size, its end at (0, 100).
    const f64 half[2] = { 0, -50 };
    const u32 t = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 50 }, 3.14159265358979323846 / 2.0, 2.0 }, FUDE_ZOOM_SHAPE_LINE, half, 2, c, 1.0f, 0u, 0);
    rde_arr_clear(&died); rde_arr_clear(&born);
    CHECK(fude_zoom_connect_drive(&s, s.root, (fude_zoom_v2){ 100, 50 }, (fude_zoom_v2){ 110, 50 }, 1e-6, FUDE_ZOOM_NONE, &died, &born) >= 1u);   // (and the polygon's corner)
    CHECK(!alive(&s, t));
    for(u32 i = 0; i < (u32)rde_arr_length(&born); i++) {
        const u32 o = ((const u32*)born.memory)[i];
        if(fude_zoom_scene_object(&s, o)->channels != FUDE_ZOOM_SHAPE_LINE) continue;
        const fude_zoom_sim os = fude_zoom_object_sim(fude_zoom_scene_object(&s, o));
        CHECK(fude_zoom_scene_shape_numbers(&s, o, n, 8) == 2u);
        const fude_zoom_v2 e = fude_zoom_sim_apply(os, (fude_zoom_v2){ n[0], n[1] }), a = fude_zoom_sim_apply(os, (fude_zoom_v2){ 0, 0 });
        CHECK(fabs(e.x - 110) < 1e-9 && fabs(e.y - 50) < 1e-9 && fabs(a.x) < 1e-9 && fabs(a.y - 50) < 1e-9);
    }
    // A rectangle's corner driven onto the line of the one across from it: flattened to nothing, so left as it was.
    const f64 sq[3] = { 10, 10, 0 };
    const u32 flat = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 500, 500 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, sq, 3, c, 1.0f, 0u, 0);
    rde_arr_clear(&died); rde_arr_clear(&born);
    CHECK(fude_zoom_connect_drive(&s, s.root, (fude_zoom_v2){ 510, 510 }, (fude_zoom_v2){ 490, 530 }, 1e-6, FUDE_ZOOM_NONE, &died, &born) == 0u && alive(&s, flat));
    rde_arr_free(&died); rde_arr_free(&born);
    fude_zoom_scene_destroy(&s);
}

// Boards: their numbers (a name kept in them), their outline, and the cut list.
static void test_boards(void) {
    f64 n[FUDE_ZOOM_SHAPE_NUMBERS];
    c8 name[FUDE_ZOOM_BOARD_NAME];
    u32 count = fude_zoom_board_numbers(n, 300, 150, 18, 0, "Lateral izquierdo");
    CHECK(count == 4u + 3u && n[0] == 300 && n[1] == 150 && n[2] == 18 && n[3] == 0);
    fude_zoom_board_name(n, count, name, sizeof name);
    CHECK(strcmp(name, "Lateral izquierdo") == 0);
    // Its look: its grain, its material over it; one from before materials (its grain alone) of none said.
    CHECK(fude_zoom_board_look(n, count) == 0u && fude_zoom_board_material(n, count) == FUDE_ZOOM_MATERIAL_NONE);
    count = fude_zoom_board_numbers(n, 300, 150, 18, (u8)(1u | (FUDE_ZOOM_MATERIAL_WALNUT << 1)), "Lateral izquierdo");
    CHECK(fude_zoom_board_grain(n, count) == 1u && fude_zoom_board_material(n, count) == FUDE_ZOOM_MATERIAL_WALNUT);
    fude_zoom_board_name(n, count, name, sizeof name);
    CHECK(strcmp(name, "Lateral izquierdo") == 0);
    n[3] = (f64)(fude_zoom_board_look(n, count) ^ 1u);   // (its grain turned: its material kept)
    CHECK(fude_zoom_board_grain(n, count) == 0u && fude_zoom_board_material(n, count) == FUDE_ZOOM_MATERIAL_WALNUT);
    n[3] = 1.0;
    CHECK(fude_zoom_board_grain(n, count) == 1u && fude_zoom_board_material(n, count) == FUDE_ZOOM_MATERIAL_NONE);
    count = fude_zoom_board_numbers(n, 300, 150, 18, (u8)(FUDE_ZOOM_MATERIAL_COUNT << 1), "Odd");   // (one it does not know: none said)
    CHECK(fude_zoom_board_material(n, count) == FUDE_ZOOM_MATERIAL_NONE);
    CHECK(fude_zoom_material_wood(FUDE_ZOOM_MATERIAL_WALNUT).r < fude_zoom_material_wood(FUDE_ZOOM_MATERIAL_MAPLE).r);
    // Too long: cut at a whole character, NUL kept.
    c8 longer[300] = { 0 };
    for(u32 i = 0; i < 120; i++) { longer[i * 2] = (c8)0xC3; longer[i * 2 + 1] = (c8)0xA1; }   // "á" 120 times
    count = fude_zoom_board_numbers(n, 1, 1, 1, 1, longer);
    CHECK(count == 4u + FUDE_ZOOM_BOARD_NAME / 8u);
    fude_zoom_board_name(n, count, name, sizeof name);
    CHECK(strlen(name) == FUDE_ZOOM_BOARD_NAME - 2u && ((u8)name[strlen(name) - 1u] == 0xA1));
    // Its outline: a rectangle, never a round-cornered one (its third number is its thickness).
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), NULL);
    b8 closed = false;
    count = fude_zoom_board_numbers(n, 300, 150, 18, 0, "Side");
    fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_BOARD, n, count, 64, &pts, &closed);
    CHECK(rde_arr_length(&pts) == 4u && closed && ((const fude_zoom_v2*)pts.memory)[2].x == 300 && ((const fude_zoom_v2*)pts.memory)[2].y == 150);
    rde_arr_free(&pts);
    // The cut list: two sides alike, a top (its width longer than its length: turned), one on a hidden layer left out.
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const rde_color c = { 1, 1, 1, 255 };
    count = fude_zoom_board_numbers(n, 300, 150, 18, 0, "Side");
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_BOARD, n, count, c, 0.5f, 0u, 0);
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 400 }, 0.7, 1.0 }, FUDE_ZOOM_SHAPE_BOARD, n, count, c, 0.5f, 0u, 0);   // (turned: the same part)
    count = fude_zoom_board_numbers(n, 150, 400, 18, 0, "Top, oak");
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 900, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_BOARD, n, count, c, 0.5f, 0u, 0);
    count = fude_zoom_board_numbers(n, 300, 150, 18, (u8)(FUDE_ZOOM_MATERIAL_WALNUT << 1), "Side");   // (the same part in walnut: a row of its own)
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 900 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_BOARD, n, count, c, 0.5f, 0u, 0);
    s.layer = 2;
    count = fude_zoom_board_numbers(n, 100, 100, 9, 0, "Hidden");
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, -900 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_BOARD, n, count, c, 0.5f, 0u, 0);
    s.layer = 0;
    s.layers_hidden = 4u;
    const c8* const header[7] = { "Part", "Qty", "Length (mm)", "Width (mm)", "Thickness (mm)", "Material", "Grain" };
    const c8* const grain[2] = { "along", "across" };
    const c8* const materials[FUDE_ZOOM_MATERIAL_COUNT] = { "", "Pine", "Oak", "Walnut", "Maple", "Beech", "Cherry", "Birch plywood", "MDF", "Melamine" };
    fude_bytes out = fude_bytes_new(256);
    CHECK(fude_zoom_export_cutlist(&s, 1.0, header, grain, materials, &out) == 4u);
    rde_arr_add(&out, &(u8){ 0 });
    const c8* csv = (const c8*)out.memory;
    CHECK(strstr(csv, "Part,Qty,Length (mm),Width (mm),Thickness (mm),Material,Grain\r\n") == csv);
    CHECK(strstr(csv, "Side,2,600.0,300.0,18.0,,along\r\n") != NULL);
    CHECK(strstr(csv, "Side,1,600.0,300.0,18.0,Walnut,along\r\n") != NULL);
    CHECK(strstr(csv, "\"Top, oak\",1,800.0,300.0,18.0,,across\r\n") != NULL);
    CHECK(strstr(csv, "Hidden") == NULL);
    rde_arr_free(&out);
    // Its name in an export, in its middle.
    s.layers_hidden = 0u;
    fude_bytes svg = fude_bytes_new(256);
    CHECK(fude_zoom_export_svg(&s, (fude_zoom_v2){ 2000, 2000 }, (rde_color){ 255, 255, 255, 255 }, &svg));
    rde_arr_add(&svg, &(u8){ 0 });
    CHECK(strstr((const c8*)svg.memory, ">Side<") != NULL && strstr((const c8*)svg.memory, "Top, oak") != NULL);
    rde_arr_free(&svg);
    // A template on Letter paper: its pages Letter's size.
    fude_bytes letter = fude_bytes_new(256);
    u32 pages = 0;
    CHECK(fude_zoom_export_tiles(&s, (fude_zoom_v2){ 500, 400 }, 0.1, 215.9, 279.4, (fude_zoom_v2){ 0, 0 }, NULL, &letter, &pages) && pages == 1u);
    rde_arr_add(&letter, &(u8){ 0 });
    CHECK(strstr((const c8*)letter.memory, "/MediaBox [0 0 612 792]") != NULL);
    rde_arr_free(&letter);
    // More than 26 rows (they go by letter): not made, even within 64 pages.
    letter = fude_bytes_new(256);
    CHECK(!fude_zoom_export_tiles(&s, (fude_zoom_v2){ 50, 3610 }, 1.0, 210.0, 297.0, (fude_zoom_v2){ 0, 0 }, NULL, &letter, &pages));
    rde_arr_free(&letter);
    // A board two frames down, the frame between deleted: not on the cut list any more.
    {
        fude_zoom_scene d; fude_zoom_scene_init(&d, 9);
        const fude_zoom_v2 half = { 500, 400 };
        for(u32 k = 0; k < 24; k++) { fude_zoom_camera_zoom_at(&d, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&d, half); }   // (two frames down)
        const u32 deep = d.camera.frame;
        CHECK(deep != d.root && fude_zoom_scene_frame(&d, deep)->parent != d.root && fude_zoom_scene_frame(&d, deep)->parent != FUDE_ZOOM_NONE);
        f64 bn[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 bc = fude_zoom_board_numbers(bn, 1, 1, 18, 0, "Deep");
        const u32 b = fude_zoom_scene_add_shape(&d, deep, (fude_zoom_place){ d.camera.at, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_BOARD, bn, bc, c, 0.1f, 0u, 0);
        fude_zoom_history_push(&d, deep, fude_zoom_scene_object(&d, b)->box, NULL, 0, &b, 1);
        fude_bytes list = fude_bytes_new(256);
        CHECK(fude_zoom_export_cutlist(&d, 1.0, header, grain, materials, &list) == 1u);
        rde_arr_free(&list);
        const u32 middle = fude_zoom_scene_frame(&d, deep)->parent;
        fude_zoom_scene_set_alive(&d, fude_zoom_scene_frame(&d, middle)->object, false);   // the frame between, deleted
        CHECK(!fude_zoom_scene_frame_shown(&d, deep) && fude_zoom_scene_frame_shown(&d, d.root));
        list = fude_bytes_new(256);
        CHECK(fude_zoom_export_cutlist(&d, 1.0, header, grain, materials, &list) == 0u);
        rde_arr_free(&list);
        fude_zoom_scene_destroy(&d);
    }
    fude_zoom_scene_destroy(&s);
}

// The diagram library: every symbol drawn (its outline closed and in its box), its text kept and split,
// a class's compartments; a connector's ends.
// The map (map.h): what is drawn and the view, root units; the areas by name, in order; the places by number;
// what a map of a shape shows; where a tap flies.
static void test_map(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const fude_zoom_v2 half = { 500, 400 };
    CHECK(fude_zoom_box_is_empty(fude_zoom_map_contents(&s)));
    line(&s, 0, 0, 100, 0, 2.0, 2.0f);
    line(&s, 3000, 1000, 3100, 1000, 2.0, 2.0f);   // far off, at the same depth
    fude_zoom_box c = fude_zoom_map_contents(&s);
    CHECK(c.min_x < 1.0 && c.max_x > 3099.0 && c.min_y < 1.0 && c.max_y > 999.0);
    // Two areas, the second made first is not: in the order made; a name its first line.
    f64 n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 area = fude_zoom_symbol_find("area");
    CHECK(area != FUDE_ZOOM_NONE && fude_zoom_symbol_info_of(area)->place == FUDE_ZOOM_SYMBOL_TEXT_CORNER);
    u32 cnt = fude_zoom_symbol_numbers(n, area, 200, 100, 20, "Kitchen\nfirst floor");
    const u32 a1 = fude_zoom_scene_add_shape_fill(&s, s.root, (fude_zoom_place){ { 50, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL, n, cnt, (rde_color){ 1, 2, 3, 255 }, 1.0f, 0, (rde_color){ 0, 0, 0, 0 }, 0);
    cnt = fude_zoom_symbol_numbers(n, area, 300, 300, 20, "Garden");
    const u32 a2 = fude_zoom_scene_add_shape_fill(&s, s.root, (fude_zoom_place){ { 3050, 1000 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL, n, cnt, (rde_color){ 1, 2, 3, 255 }, 1.0f, 0, (rde_color){ 0, 0, 0, 0 }, 0);
    cnt = fude_zoom_symbol_numbers(n, fude_zoom_symbol_find("process"), 60, 35, 15, "Not an area");
    fude_zoom_scene_add_shape_fill(&s, s.root, (fude_zoom_place){ { 0, 500 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL, n, cnt, (rde_color){ 1, 2, 3, 255 }, 1.0f, 0, (rde_color){ 0, 0, 0, 0 }, 0);
    fude_zoom_map_area areas[FUDE_ZOOM_MAP_AREAS];
    CHECK(fude_zoom_map_areas(&s, areas, FUDE_ZOOM_MAP_AREAS) == 2u && areas[0].object == a1 && areas[1].object == a2);
    CHECK(strcmp(areas[0].name, "Kitchen") == 0 && strcmp(areas[1].name, "Garden") == 0);
    CHECK(areas[0].box.min_x <= -150.0 + 1e-6 && areas[0].box.max_x >= 250.0 - 1e-6 && areas[1].box.max_y >= 1300.0 - 1e-6);
    CHECK(fude_zoom_map_is_area(&s, a1) && !fude_zoom_map_is_area(&s, a1 + 2u));
    // Places by number, not as marked.
    fude_zoom_scene_add_mark(&s, s.root, (fude_zoom_v2){ 10, 20 }, 2.0, 3u);
    fude_zoom_scene_add_mark(&s, s.root, (fude_zoom_v2){ 30, 40 }, 2.0, 1u);
    fude_zoom_scene_add_mark(&s, s.root, (fude_zoom_v2){ 50, 60 }, 2.0, 2u);
    fude_zoom_map_place places[2];
    CHECK(fude_zoom_map_places(&s, places, 2u) == 2u && places[0].number == 1u && places[1].number == 2u && places[0].at.x == 30.0);
    // ...and left out of what is drawn.
    c = fude_zoom_map_contents(&s);
    CHECK(c.min_x <= -150.0 + 1e-6 && c.max_x >= 3350.0 - 1e-6);
    // A map half as tall as wide: all of it, a margin round, that shape, the view in it.
    const fude_zoom_box view = fude_zoom_map_view(&s, half);
    CHECK(fabs(view.min_x + 500.0) < 1e-9 && fabs(view.max_y - 400.0) < 1e-9);
    const fude_zoom_box w = fude_zoom_map_world(c, view, 0.5);
    CHECK(fabs((w.max_y - w.min_y) / (w.max_x - w.min_x) - 0.5) < 1e-9);
    CHECK(w.min_x < c.min_x && w.max_x > c.max_x && w.min_y < view.min_y && w.max_y > c.max_y);
    // A tap far off: there, as far in as now (the view a fair part of the map); deep in, out to a ninth of it.
    fude_zoom_camera t = fude_zoom_map_tap_view(&s, half, w, (fude_zoom_v2){ 3000, 1000 });
    CHECK(t.frame == s.root && t.at.x == 3000.0 && fabs(t.z - 1.0) < 1e-9);
    s.camera.z = 1000.0;
    t = fude_zoom_map_tap_view(&s, half, w, (fude_zoom_v2){ 3000, 1000 });
    CHECK(fabs(2.0 * half.x / t.z - (w.max_x - w.min_x) / 9.0) < 1e-6);
    // Presented in order: the numbered first, by their numbers; the rest as made.
    fude_zoom_map_area order[4];
    memset(order, 0, sizeof(order));
    snprintf(order[0].name, sizeof(order[0].name), "Garden");
    snprintf(order[1].name, sizeof(order[1].name), "10 Shed");
    snprintf(order[2].name, sizeof(order[2].name), "Attic");
    snprintf(order[3].name, sizeof(order[3].name), " 2. Kitchen");
    fude_zoom_map_areas_order(order, 4u);
    CHECK(strcmp(order[0].name, " 2. Kitchen") == 0 && strcmp(order[1].name, "10 Shed") == 0 && strcmp(order[2].name, "Garden") == 0 && strcmp(order[3].name, "Attic") == 0);
    // The areas as a PDF, a page each, each its own size.
    const fude_zoom_camera views[2] = { fude_zoom_nav_framing((fude_zoom_v2){ 421, 210.5 }, s.root, areas[0].box, 0.96), fude_zoom_nav_framing((fude_zoom_v2){ 421, 421 }, s.root, areas[1].box, 0.96) };
    const fude_zoom_v2 halves[2] = { { 421, 210.5 }, { 421, 421 } };
    const fude_zoom_camera was = s.camera;
    fude_bytes pdf = fude_bytes_new(4096);
    CHECK(fude_zoom_export_pdf_views(&s, (rde_color){ 255, 255, 255, 255 }, views, halves, 2u, &pdf));
    rde_arr_add(&pdf, &(u8){ 0 });
    CHECK(strstr((const c8*)pdf.memory, "/Count 2") != NULL && strstr((const c8*)pdf.memory, "/MediaBox [0 0 842 421]") != NULL && strstr((const c8*)pdf.memory, "/MediaBox [0 0 842 842]") != NULL);
    CHECK(memcmp(&s.camera, &was, sizeof(was)) == 0);   // (the camera put back)
    rde_arr_free(&pdf);
    // Exported: a symbol's text centred as on the screen; an area's name from its corner.
    s.camera = (fude_zoom_camera){ s.root, { 1500, 500 }, 0.25 };
    fude_bytes svg = fude_bytes_new(4096);
    CHECK(fude_zoom_export_svg(&s, half, (rde_color){ 255, 255, 255, 255 }, &svg));
    rde_arr_add(&svg, &(u8){ 0 });
    const c8* centred = strstr((const c8*)svg.memory, ">Not an area<");
    const c8* garden  = strstr((const c8*)svg.memory, ">Garden<");
    CHECK(centred != NULL && garden != NULL);
    const c8* t1 = centred; while(t1 > (const c8*)svg.memory && strncmp(t1, "<text", 5) != 0) t1--;
    const c8* t2 = garden;  while(t2 > (const c8*)svg.memory && strncmp(t2, "<text", 5) != 0) t2--;
    CHECK(strstr(t1, "text-anchor=\"middle\"") != NULL && strstr(t1, "text-anchor=\"middle\"") < centred);
    CHECK(strstr(t2, "text-anchor") == NULL || strstr(t2, "text-anchor") > garden);
    rde_arr_free(&svg);
    // ...and in a DXF the same: the symbol's line justified to its middle (group 72 is 1), the area's name left (none).
    fude_bytes dxf = fude_bytes_new(4096);
    CHECK(fude_zoom_export_dxf(&s, half, 1.0, &dxf, NULL));
    rde_arr_add(&dxf, &(u8){ 0 });
    const c8* d1 = strstr((const c8*)dxf.memory, "\r\nNot an area\r\n");
    const c8* d2 = strstr((const c8*)dxf.memory, "\r\nGarden\r\n");
    CHECK(d1 != NULL && d2 != NULL);
    if(d1 != NULL && d2 != NULL) {
        const c8* d1_end = strstr(d1 + 2, "\r\n  0\r\n");
        const c8* d2_end = strstr(d2 + 2, "\r\n  0\r\n");
        const c8* d1_72  = strstr(d1, "\r\n 72\r\n1\r\n");
        const c8* d2_72  = strstr(d2, "\r\n 72\r\n");
        CHECK(d1_end != NULL && d1_72 != NULL && d1_72 < d1_end);
        CHECK(d2_end != NULL && (d2_72 == NULL || d2_72 > d2_end));
    }
    rde_arr_free(&dxf);
    CHECK(fabs(fude_zoom_pdf_text_width(10.0, "Hi") - (0.722 + 0.222) * 10.0) < 1e-9);
    s.camera = was;
    // Nothing drawn: the view's surroundings.
    const fude_zoom_box e = fude_zoom_map_world(fude_zoom_box_empty(), view, 0.8);
    CHECK(e.min_x < view.min_x - 900.0 && e.max_x > view.max_x + 900.0);
    fude_zoom_scene_destroy(&s);
}

// Measured sheets (sheet.h): their rulers' steps at any zoom (metric and inches), their marks on the screen (each
// side's ticks and numbers, the grid's lines, what is off the screen left out), the grid snapped to, papers found;
// exported: their rulers in a drawing, never in a cutting file, a page each at its scale; a dimension's number written.
static void test_sheets(void) {
    fude_zoom_sheet_step st;
    CHECK(fude_zoom_sheet_step_for(0.5, FUDE_ZOOM_SHEET_TICK_PT, false, &st) && st.step_mm == 5.0 && st.major == 2u && st.mid == 0u && strcmp(st.unit, "cm") == 0 && st.per_mm == 10.0);
    CHECK(fude_zoom_sheet_step_for(0.1, FUDE_ZOOM_SHEET_TICK_PT, false, &st) && fabs(st.step_mm - 1.0) < 1e-12 && st.major == 10u && st.mid == 5u && strcmp(st.unit, "cm") == 0);
    CHECK(fude_zoom_sheet_tick(&st, 10u) == 14.0 && fude_zoom_sheet_tick(&st, 5u) == 10.0 && fude_zoom_sheet_tick(&st, 3u) == 6.0);
    CHECK(fude_zoom_sheet_step_for(20.0, FUDE_ZOOM_SHEET_TICK_PT, false, &st) && fabs(st.step_mm - 200.0) < 1e-9 && st.major == 5u && strcmp(st.unit, "m") == 0 && st.per_mm == 1000.0);
    CHECK(fude_zoom_sheet_step_for(0.1, FUDE_ZOOM_SHEET_TICK_PT, true, &st) && fabs(st.step_mm - 25.4 / 16.0) < 1e-12 && st.major == 16u && st.halves && strcmp(st.unit, "in") == 0);
    CHECK(fude_zoom_sheet_tick(&st, 16u) == 14.0 && fude_zoom_sheet_tick(&st, 8u) == 11.5 && fude_zoom_sheet_tick(&st, 4u) == 9.0 && fude_zoom_sheet_tick(&st, 1u) == 4.5);
    CHECK(fude_zoom_sheet_step_for(10.0, FUDE_ZOOM_SHEET_TICK_PT, true, &st) && fabs(st.step_mm - 76.2) < 1e-9 && st.major == 4u && strcmp(st.unit, "ft") == 0);
    CHECK(!fude_zoom_sheet_step_for(0.0, 7.0, false, &st));
    c8 num[24];
    fude_zoom_sheet_number(2.50, num, sizeof(num));
    CHECK(strcmp(num, "2.5") == 0);
    // Its numbers, and back.
    f64 n[FUDE_ZOOM_SHAPE_NUMBERS];
    fude_zoom_sheet sh;
    CHECK(fude_zoom_sheet_numbers(n, -210.0, 148.5, 0.0, FUDE_ZOOM_SHEET_GRID) == 4u && fude_zoom_sheet_of(n, 4u, &sh));
    CHECK(sh.hw == 210.0 && sh.hh == 148.5 && sh.scale == 1.0 && sh.flags == FUDE_ZOOM_SHEET_GRID);
    // A3 across, a millimetre a unit, 2 points a millimetre: ticks 5 mm apart on all four sides, numbered every 2 cm
    // (not at its corners, nor by the far ones); its unit by its first corner.
    sh.flags = 0u;
    rde_arr lines = rde_arr_new(sizeof(fude_zoom_sheet_line), rde_memory_allocator_get_default_std());
    rde_arr labels = rde_arr_new(sizeof(fude_zoom_sheet_label), rde_memory_allocator_get_default_std());
    fude_zoom_sheet_label unit;
    const fude_zoom_sim two = { 2.0, 0.0, 0.0, 0.0 };
    const fude_zoom_box big = { -2000, -2000, 2000, 2000 };
    fude_zoom_sheet_marks(&sh, two, 1.0, false, big, &lines, &labels, &unit);
    CHECK(rde_arr_length(&lines) == 85u + 85u + 60u + 60u);
    CHECK(rde_arr_length(&labels) == 20u + 20u + 14u + 14u);
    const fude_zoom_sheet_label* lb = (const fude_zoom_sheet_label*)labels.memory;
    CHECK(strcmp(lb[0].text, "2") == 0 && lb[0].in.x == 0.0 && lb[0].in.y == 1.0 && fabs(lb[0].at.x - (-420.0 + 40.0)) < 1e-9 && fabs(lb[0].at.y - (-297.0 + 17.0)) < 1e-9);
    CHECK(strcmp(unit.text, "cm") == 0 && unit.at.x > -420.0 && unit.at.y > -297.0);
    // ...with its grid: a line a centimetre each way (its sides not: they are its outline), every tenth stronger.
    sh.flags = FUDE_ZOOM_SHEET_GRID;
    fude_zoom_sheet_marks(&sh, two, 1.0, false, big, &lines, &labels, NULL);
    u32 grid = 0, strong = 0;
    for(u32 i = 0; i < (u32)rde_arr_length(&lines); i++) {
        const u8 w = ((const fude_zoom_sheet_line*)lines.memory)[i].weight;
        grid += w != 0u ? 1u : 0u;
        strong += w == 2u ? 1u : 0u;
    }
    CHECK(grid == 41u + 29u && strong == 4u + 2u);
    // Only what is on the screen: a corner's.
    fude_zoom_sheet_marks(&sh, two, 1.0, false, (fude_zoom_box){ -430, -310, -300, -200 }, &lines, &labels, NULL);
    CHECK(rde_arr_length(&lines) > 0u && rde_arr_length(&lines) < 60u);
    // Too small on the screen: no rulers; small, their ticks only.
    fude_zoom_sheet_marks(&sh, (fude_zoom_sim){ 0.1, 0.0, 0.0, 0.0 }, 1.0, false, big, &lines, &labels, &unit);
    CHECK(rde_arr_length(&labels) == 0u && unit.text[0] == 0);
    fude_zoom_sheet_marks(&sh, (fude_zoom_sim){ 0.3, 0.0, 0.0, 0.0 }, 1.0, false, big, &lines, &labels, &unit);
    CHECK(rde_arr_length(&labels) == 0u && rde_arr_length(&lines) > 0u);
    // Turned a quarter: the bottom ruler's numbers go into it the way its own up is.
    fude_zoom_sheet_marks(&sh, (fude_zoom_sim){ 0.0, 2.0, 0.0, 0.0 }, 1.0, false, big, &lines, &labels, NULL);
    lb = (const fude_zoom_sheet_label*)labels.memory;
    CHECK(rde_arr_length(&labels) > 0u && fabs(lb[0].in.x + 1.0) < 1e-12 && fabs(lb[0].in.y) < 1e-12);
    // Zoomed in (40 points a millimetre) on its middle, its edges all off the screen: its top and left rulers stuck to
    // the screen's top and left on their bands, nothing else (the rest is the sheet's own pass), the unit by the number
    // nearest the screen's middle; its top edge on the screen: only the left one stuck.
    sh.flags = FUDE_ZOOM_SHEET_GRID;
    const fude_zoom_sim close = { 40.0, 0.0, 0.0, 0.0 };
    fude_zoom_box bands[2];
    u32 nb = 9;
    fude_zoom_sheet_marks_stuck(&sh, close, 1.0, false, (fude_zoom_box){ -500, -400, 500, 400 }, &lines, &labels, NULL, bands, &nb);
    CHECK(nb == 2u && fabs(bands[0].max_y - 400.0) < 1e-9 && fabs(bands[0].min_y - (400.0 - FUDE_ZOOM_SHEET_BAND_PT)) < 1e-9 && fabs(bands[1].min_x + 500.0) < 1e-9);
    b8 inside = true, unit_said = false;
    for(u32 i = 0; i < (u32)rde_arr_length(&lines); i++) {
        const fude_zoom_sheet_line* l = &((const fude_zoom_sheet_line*)lines.memory)[i];
        inside = inside && l->weight == 0u && (fabs(l->a.y - 400.0) < 1e-6 || fabs(l->a.x + 500.0) < 1e-6);
    }
    for(u32 i = 0; i < (u32)rde_arr_length(&labels); i++) {
        unit_said = unit_said || strstr(((const fude_zoom_sheet_label*)labels.memory)[i].text, " mm") != NULL;
    }
    CHECK(rde_arr_length(&lines) > 0u && inside && unit_said);
    fude_zoom_sheet_marks_stuck(&sh, close, 1.0, false, (fude_zoom_box){ -500, 148.5 * 40.0 - 300.0, 500, 148.5 * 40.0 + 100.0 }, &lines, &labels, NULL, bands, &nb);
    CHECK(nb == 1u && fabs(bands[0].min_x + 500.0) < 1e-9);
    // Turned: never stuck.
    fude_zoom_sheet_marks_stuck(&sh, (fude_zoom_sim){ 28.0, 28.0, 0.0, 0.0 }, 1.0, false, (fude_zoom_box){ -500, -400, 500, 400 }, &lines, &labels, NULL, bands, &nb);
    CHECK(nb == 0u && rde_arr_length(&lines) == 0u);
    rde_arr_free(&lines);
    rde_arr_free(&labels);
    // The grid snapped to (from its bottom left corner), off it nothing; a side's length to the ruler's ticks.
    fude_zoom_v2 g;
    CHECK(fude_zoom_sheet_grid_snap(&sh, 1.0, 0.5, false, (fude_zoom_v2){ 3.2, -7.9 }, &g) && fabs(g.x) < 1e-9 && fabs(g.y + 8.5) < 1e-9);
    CHECK(!fude_zoom_sheet_grid_snap(&sh, 1.0, 0.5, false, (fude_zoom_v2){ 300.0, 0.0 }, &g));
    sh.flags = 0u;
    CHECK(!fude_zoom_sheet_grid_snap(&sh, 1.0, 0.5, false, (fude_zoom_v2){ 3.2, -7.9 }, &g));
    CHECK(fabs(fude_zoom_sheet_round(123.4, 0.5, false) - 125.0) < 1e-9 && fabs(fude_zoom_sheet_round(1.0, 0.5, false) - 5.0) < 1e-9);
    // Papers, either way up.
    CHECK(fude_zoom_paper_find(297.0, 210.0) == FUDE_ZOOM_PAPER_A4 && fude_zoom_paper_find(420.2, 297.3) == FUDE_ZOOM_PAPER_A3 && fude_zoom_paper_find(100, 100) == FUDE_ZOOM_NONE);
    CHECK(fude_zoom_paper_find(215.9, 279.4) == FUDE_ZOOM_PAPER_LETTER);
    // On a canvas: a sheet (A3 across, its grid) and a dimension 100 mm long.
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 cnt = fude_zoom_sheet_numbers(n, 210.0, 148.5, 1.0, FUDE_ZOOM_SHEET_GRID);
    const u32 sheet = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_SHEET, n, cnt, (rde_color){ 10, 20, 30, 255 }, 0.5f, 0, 0);
    CHECK(fabs(fude_zoom_scene_object(&s, sheet)->box.max_x - 210.0) < 1.0 && fabs(fude_zoom_scene_object(&s, sheet)->box.min_y + 148.5) < 1.0);
    const f64 dim[3] = { 100.0, 0.0, 10.0 };
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { -50, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_DIMENSION, dim, 3u, (rde_color){ 10, 20, 30, 255 }, 0.5f, 0, 0);
    fude_zoom_units_style mm = { FUDE_ZOOM_UNIT_MM, 1u, 16u, false };
    fude_zoom_export_units(1.0, &mm);
    s.camera = (fude_zoom_camera){ s.root, { 0, 0 }, 2.0 };
    fude_bytes svg = fude_bytes_new(4096);
    CHECK(fude_zoom_export_svg(&s, (fude_zoom_v2){ 500, 400 }, (rde_color){ 255, 255, 255, 255 }, &svg));
    rde_arr_add(&svg, &(u8){ 0 });
    CHECK(strstr((const c8*)svg.memory, ">100 mm<") != NULL && strstr((const c8*)svg.memory, ">40<") != NULL && strstr((const c8*)svg.memory, ">cm<") != NULL);
    rde_arr_free(&svg);
    fude_bytes dxf = fude_bytes_new(4096);
    fude_zoom_export_check check;
    memset(&check, 0, sizeof(check));
    CHECK(fude_zoom_export_dxf(&s, (fude_zoom_v2){ 500, 400 }, 0.5, &dxf, &check));
    CHECK(check.paths == 0u);   // (neither the sheet nor the dimension is a cut)
    rde_arr_free(&dxf);
    // A page the sheet's size at its scale: A3 at 1:1 is 1190.55 × 841.89 points; the camera put back.
    const fude_zoom_camera was = s.camera;
    const fude_zoom_v2 half3 = { 420.0 * 72.0 / 25.4 * 0.5, 297.0 * 72.0 / 25.4 * 0.5 };
    const fude_zoom_camera view = fude_zoom_nav_framing(half3, s.root, (fude_zoom_box){ -210, -148.5, 210, 148.5 }, 1.0);
    CHECK(fabs(view.z - 72.0 / 25.4) < 1e-9);
    fude_bytes pdf = fude_bytes_new(4096);
    CHECK(fude_zoom_export_pdf_pages(&s, &view, &half3, 1u, (fude_zoom_v2){ 0.0, 0.0 }, NULL, &pdf));
    rde_arr_add(&pdf, &(u8){ 0 });
    CHECK(strstr((const c8*)pdf.memory, "/MediaBox [0 0 1190.55") != NULL && strstr((const c8*)pdf.memory, " 841.88") != NULL);
    CHECK(memcmp(&s.camera, &was, sizeof(was)) == 0);
    // ...its paper under it: written too (a ruled page's lines), the camera put back still.
    rde_arr ruled = rde_arr_new(sizeof(fude_zoom_paper_line), NULL);
    CHECK(fude_zoom_paper_sheet(FUDE_PAPER_LINES, (fude_zoom_box){ -210, -148.5, 210, 148.5 }, 20.0, 8.0, 5.0, 2.0, &ruled) == 36u);
    fude_bytes pdf_ruled = fude_bytes_new(4096);
    CHECK(fude_zoom_export_pdf_pages(&s, &view, &half3, 1u, (fude_zoom_v2){ 0.0, 0.0 }, &ruled, &pdf_ruled));
    CHECK(rde_arr_length(&pdf_ruled) > rde_arr_length(&pdf) && memcmp(&s.camera, &was, sizeof(was)) == 0);
    rde_arr_free(&pdf_ruled);
    rde_arr_free(&ruled);
    rde_arr_free(&pdf);
    fude_zoom_export_units(0.0, NULL);
    fude_zoom_scene_destroy(&s);
}

// Pieces (piece.h): kept and read back whole; their lines; the list on disk — added first, read again in order,
// one let go.
static void test_pieces(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 st = line(&s, 0, 0, 100, 0, 2.0, 2.0f);
    const f64 rect[3] = { 50, 30, 0 };
    const u32 sh = fude_zoom_scene_add_shape_fill(&s, s.root, (fude_zoom_place){ { 0, 200 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, rect, 3, (rde_color){ 1, 2, 3, 255 }, 2.0f,
                                                  FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN, (rde_color){ 255, 0, 0, 255 }, 0);
    const u32 objs[2] = { st, sh };
    rde_arr clips = rde_arr_new(sizeof(fude_zoom_clip), NULL);
    fude_zoom_select_clips_of(&s, objs, 2u, &clips);
    CHECK(rde_arr_length(&clips) == 2u);
    fude_zoom_piece piece;
    memset(&piece, 0, sizeof(piece));
    snprintf(piece.name, sizeof(piece.name), "Jig");
    piece.box = (fude_zoom_box){ -60, -40, 110, 240 };
    piece.items = clips;
    fude_bytes b = fude_bytes_new(256);
    fude_zoom_piece_write(&piece, &b);
    fude_zoom_piece back;
    memset(&back, 0, sizeof(back));
    CHECK(fude_zoom_piece_read(&back, (const u8*)b.memory, (u32)rde_arr_length(&b)));
    CHECK(strcmp(back.name, "Jig") == 0 && back.box.max_y == 240.0 && rde_arr_length(&back.items) == 2u);
    const fude_zoom_clip* c0 = (const fude_zoom_clip*)clips.memory;
    const fude_zoom_clip* r0 = (const fude_zoom_clip*)back.items.memory;
    CHECK(r0[0].look.kind == FUDE_ZOOM_KIND_STROKE && fude_zoom_clip_size(&r0[0]) == fude_zoom_clip_size(&c0[0]) && memcmp(fude_zoom_clip_data(&r0[0]), fude_zoom_clip_data(&c0[0]), fude_zoom_clip_size(&c0[0])) == 0 && r0[0].look.count == c0[0].look.count);
    CHECK(r0[1].look.kind == FUDE_ZOOM_KIND_SHAPE && r0[1].look.channels == FUDE_ZOOM_SHAPE_RECT && r0[1].look.fill.r == 255 && r0[1].on_screen.ty == c0[1].on_screen.ty);
    // Its lines: the stroke's middle (open, from 0 to 100), the rectangle's outline (closed).
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), NULL), lines = rde_arr_new(sizeof(fude_zoom_piece_line), NULL);
    CHECK(fude_zoom_piece_lines(&back, true, &pts, &lines) == 2u);
    const fude_zoom_piece_line* l = (const fude_zoom_piece_line*)lines.memory;
    const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
    CHECK(!l[0].closed && l[1].closed && l[1].count >= 4u);
    fude_zoom_box lb = fude_zoom_box_empty();
    for(u32 i = 0; i < l[0].count; i++) lb = fude_zoom_box_union(lb, (fude_zoom_box){ p[l[0].first + i].x, p[l[0].first + i].y, p[l[0].first + i].x, p[l[0].first + i].y });
    const fude_zoom_v2 o0 = fude_zoom_sim_apply(c0[0].on_screen, (fude_zoom_v2){ 0, 0 });
    CHECK(fabs(lb.min_x - o0.x) < 1e-6 && fabs((lb.max_x - lb.min_x) - 100.0 * fude_zoom_sim_scale(c0[0].on_screen)) < 1.0);
    rde_arr_free(&pts); rde_arr_free(&lines);
    // On disk: two kept (the newest first), read again, the first let go.
    const c8* dir = "./pieces_test/";
    rde_file_create_missing_dirs("./pieces_test/x.piece");
    rde_file_delete("./pieces_test/index.kana");
    fude_zoom_pieces lib;
    memset(&lib, 0, sizeof(lib));
    fude_zoom_pieces_load(&lib, dir);
    CHECK(lib.count == 0u);
    CHECK(fude_zoom_pieces_add(&lib, dir, "One", (const fude_zoom_clip*)clips.memory, 1u, piece.box) == 0u);
    CHECK(fude_zoom_pieces_add(&lib, dir, "Two", (const fude_zoom_clip*)clips.memory, 2u, piece.box) == 0u);
    CHECK(lib.count == 2u && strcmp(lib.list[0].name, "Two") == 0);
    fude_zoom_pieces again;
    memset(&again, 0, sizeof(again));
    fude_zoom_pieces_load(&again, dir);
    CHECK(again.count == 2u && strcmp(again.list[0].name, "Two") == 0 && strcmp(again.list[1].name, "One") == 0 && rde_arr_length(&again.list[0].items) == 2u);
    CHECK(fude_zoom_pieces_rename(&again, dir, 1u, "Uno"));   // (written: true)
    CHECK(!fude_zoom_pieces_rename(&again, dir, 7u, "None"));   // (no such piece)
    fude_zoom_pieces_remove(&again, dir, 0u);
    CHECK(again.count == 1u && strcmp(again.list[0].name, "Uno") == 0);
    fude_zoom_pieces third;
    memset(&third, 0, sizeof(third));
    fude_zoom_pieces_load(&third, dir);
    CHECK(third.count == 1u && strcmp(third.list[0].name, "Uno") == 0 && third.next_id >= 3u);
    fude_zoom_pieces_free(&lib); fude_zoom_pieces_free(&again); fude_zoom_pieces_free(&third);
    fude_zoom_piece_clear(&back);
    rde_arr_free(&b);
    fude_zoom_select_clips_free(&clips);
    rde_arr_free(&clips);
    fude_zoom_scene_destroy(&s);
}

// Charts from data (chart.h): read (a title, categories and their values, series' names, a name with spaces, a
// percent); a bar chart's bars, solid, each series its colour; a line chart's lines and dots; a pie's slices, its
// shares adding up; nothing to draw but its axes when it has no data. And the new families: Venn's
// regions, all in its box.
static void test_charts(void) {
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), NULL), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), NULL), labels = rde_arr_new(sizeof(fude_zoom_plot_label), NULL);
    const c8* data = "Sales\nJan 120 80\nFeb: 95, 110\nPine for the back\t30 40\nseries: Shop, Online";
    CHECK(fude_zoom_plot_is("bar chart") && fude_zoom_chart_is("pie chart") && !fude_zoom_chart_is("graph"));
    CHECK(fude_zoom_plot_lines("bar chart", data, 160, 110, &pts, &parts, &labels) == 2u);
    {
        const fude_zoom_symbol_part* q = (const fude_zoom_symbol_part*)parts.memory;
        const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
        u32 bars[2] = { 0, 0 };
        b8 inside = true;
        for(u32 i = 0; i < (u32)rde_arr_length(&parts); i++) {
            if(q[i].flags >= FUDE_ZOOM_CHART_SOLID) bars[(q[i].flags - FUDE_ZOOM_CHART_SOLID) % 2u]++;
        }
        for(u32 i = 0; i < (u32)rde_arr_length(&pts); i++) inside = inside && fabs(p[i].x) <= 160.0 && fabs(p[i].y) <= 110.0;
        CHECK(bars[0] == 3u && bars[1] == 3u && inside);
        const fude_zoom_plot_label* l = (const fude_zoom_plot_label*)labels.memory;
        b8 title = false, pine = false, shop = false, online = false;
        for(u32 i = 0; i < (u32)rde_arr_length(&labels); i++) {
            title  = title  || (strcmp(l[i].text, "Sales") == 0 && l[i].align == 2u);
            pine   = pine   || (strcmp(l[i].text, "Pine for the back") == 0 && l[i].align == 0u);
            shop   = shop   || (strcmp(l[i].text, "Shop") == 0 && l[i].align == 4u && l[i].curve == 1u);
            online = online || (strcmp(l[i].text, "Online") == 0 && l[i].curve == 2u);
        }
        CHECK(title && pine && shop && online);
    }
    // Lines: a line a series, a dot at each value.
    rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
    CHECK(fude_zoom_plot_lines("line chart", data, 160, 110, &pts, &parts, &labels) == 2u);
    {
        const fude_zoom_symbol_part* q = (const fude_zoom_symbol_part*)parts.memory;
        u32 lines = 0, dots = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&parts); i++) {
            lines += q[i].flags == 32u || q[i].flags == 33u ? 1u : 0u;
            dots  += q[i].flags >= FUDE_ZOOM_CHART_SOLID ? 1u : 0u;
        }
        CHECK(lines == 2u && dots == 6u);
    }
    // A pie: a slice a category, their shares (as said by them) adding up to the whole.
    rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
    CHECK(fude_zoom_plot_lines("pie chart", "Work 8\nSleep 8\nOther 4%\nNothing 0", 140, 120, &pts, &parts, &labels) == 1u);
    {
        const fude_zoom_symbol_part* q = (const fude_zoom_symbol_part*)parts.memory;
        u32 slices = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&parts); i++) slices += q[i].flags >= FUDE_ZOOM_CHART_SOLID ? 1u : 0u;
        const fude_zoom_plot_label* l = (const fude_zoom_plot_label*)labels.memory;
        b8 work = false, other = false;
        for(u32 i = 0; i < (u32)rde_arr_length(&labels); i++) {
            work  = work  || strcmp(l[i].text, "Work 40%") == 0;
            other = other || strcmp(l[i].text, "Other 20%") == 0;
        }
        CHECK(slices == 3u && work && other);
    }
    // Nothing but its axes: no data.
    rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
    CHECK(fude_zoom_plot_lines("bar chart", "", 160, 110, &pts, &parts, &labels) == 0u && rde_arr_length(&parts) > 0u);
    // Venn: its regions' boxes inside it, apart.
    fude_zoom_box b[8];
    CHECK(fude_zoom_symbol_text_boxes(fude_zoom_symbol_find("venn 2"), 120, 75, 3, b, 8) == 3u);
    CHECK(b[0].max_x < b[1].min_x && b[1].max_x < b[2].min_x && b[0].min_x >= -120.0 && b[2].max_x <= 120.0);
    CHECK(fude_zoom_symbol_text_boxes(fude_zoom_symbol_find("venn 3"), 110, 105, 4, b, 8) == 4u);
    for(u32 i = 0; i < 4u; i++) CHECK(b[i].min_x >= -110.0 && b[i].max_x <= 110.0 && b[i].min_y >= -105.0 && b[i].max_y <= 105.0);
    // Every new family: something drawn, in its box (a stamp's colours its own).
    u32 fams[4] = { 0, 0, 0, 0 };
    for(u32 k = 0; k < fude_zoom_symbol_count(); k++) {
        const fude_zoom_symbol_info* info = fude_zoom_symbol_info_of(k);
        const u32 f = info->family == FUDE_ZOOM_SYMBOL_FAMILY_CHARTS ? 0u : info->family == FUDE_ZOOM_SYMBOL_FAMILY_GEOMETRY ? 1u :
                      info->family == FUDE_ZOOM_SYMBOL_FAMILY_SCIENCE ? 2u : info->family == FUDE_ZOOM_SYMBOL_FAMILY_STAMPS ? 3u : 4u;
        if(f < 4u) fams[f]++;
    }
    CHECK(fams[0] == 3u && fams[1] == 14u && fams[2] == 18u && fams[3] == 12u);
    fude_zoom_symbol_parts(fude_zoom_symbol_find("check stamp"), 30, 30, 64, &pts, &parts);
    CHECK(rde_arr_length(&parts) == 1u && ((const fude_zoom_symbol_part*)parts.memory)[0].line.g > 150u && ((const fude_zoom_symbol_part*)parts.memory)[0].width > 0.0f);
    rde_arr_free(&pts); rde_arr_free(&parts); rde_arr_free(&labels);
}

// Sketching's own papers (paper.h): each in the view, isometric's three ways meeting in triangles, a honeycomb's sides
// all alike, polar's rings and rays, engineering's tenth lines heavy, music's staves of five; Cornell's and a planner's
// page layouts.
static void test_papers(void) {
    rde_arr lines = rde_arr_new(sizeof(fude_zoom_paper_line), NULL);
    const fude_zoom_box view = { -100, -80, 140, 90 };
    const u8 kinds[5] = { FUDE_PAPER_ISOMETRIC, FUDE_PAPER_HEX, FUDE_PAPER_POLAR, FUDE_PAPER_ENGINEERING, FUDE_PAPER_MUSIC };
    for(u32 k = 0; k < 5u; k++) {
        rde_arr_clear(&lines);
        const u32 n = fude_zoom_paper_lines(kinds[k], view, 20.0, 0.5, 2.0, &lines);
        CHECK(n > (kinds[k] == FUDE_PAPER_MUSIC ? 3u : 10u) && n == (u32)rde_arr_length(&lines));
        const fude_zoom_paper_line* l = (const fude_zoom_paper_line*)lines.memory;
        b8 inside = true;
        for(u32 i = 0; i < n; i++) {
            inside = inside && l[i].a.x >= view.min_x - 1e-9 && l[i].a.x <= view.max_x + 1e-9 && l[i].b.y >= view.min_y - 1e-9 && l[i].b.y <= view.max_y + 1e-9 &&
                     l[i].strength > 0.0f && l[i].strength <= 1.0f;
        }
        CHECK(inside);
    }
    // Isometric: vertical and 30° either side, its own lines a step apart (through the middle), the finer ones fainter.
    rde_arr_clear(&lines);
    fude_zoom_paper_lines(FUDE_PAPER_ISOMETRIC, view, 20.0, 0.25, 2.0, &lines);
    {
        const fude_zoom_paper_line* l = (const fude_zoom_paper_line*)lines.memory;
        u32 ways[3] = { 0, 0, 0 }, odd = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&lines); i++) {
            const f64 a = atan2(l[i].b.y - l[i].a.y, l[i].b.x - l[i].a.x) * 180.0 / 3.14159265358979323846;
            const f64 m = fmod(a + 360.0, 180.0);
            ways[fabs(m - 90.0) < 1e-6 ? 0 : fabs(m - 30.0) < 1e-6 ? 1 : fabs(m - 150.0) < 1e-6 ? 2 : 0] += 1u;
            CHECK(fabs(m - 90.0) < 1e-6 || fabs(m - 30.0) < 1e-6 || fabs(m - 150.0) < 1e-6);
            odd += l[i].strength == 0.25f ? 1u : 0u;
        }
        CHECK(ways[0] > 5u && ways[1] > 5u && ways[2] > 5u && odd > 0u);
        // (a vertical through the middle: one of its own)
        b8 middle = false;
        for(u32 i = 0; i < (u32)rde_arr_length(&lines); i++) {
            middle = middle || (fabs(l[i].a.x) < 1e-9 && fabs(l[i].b.x) < 1e-9 && l[i].strength == 1.0f);
        }
        CHECK(middle);
    }
    // Hexagons: every side as long as a hexagon's radius (those cut by the view's edge shorter).
    rde_arr_clear(&lines);
    fude_zoom_paper_lines(FUDE_PAPER_HEX, (fude_zoom_box){ -300, -300, 300, 300 }, 12.0, 0.0, 2.0, &lines);
    {
        const fude_zoom_paper_line* l = (const fude_zoom_paper_line*)lines.memory;
        u32 whole = 0, longer = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&lines); i++) {
            const f64 len = hypot(l[i].b.x - l[i].a.x, l[i].b.y - l[i].a.y);
            whole  += fabs(len - 12.0) < 1e-9 ? 1u : 0u;
            longer += len > 12.0 + 1e-9 ? 1u : 0u;
        }
        CHECK(whole > 1000u && longer == 0u);
        CHECK(rde_arr_length(&lines) < FUDE_ZOOM_PAPER_MOST);
    }
    // Polar: its rings' points as far from the middle as a ring's radius; its rays from the middle, every 15°.
    rde_arr_clear(&lines);
    fude_zoom_paper_lines(FUDE_PAPER_POLAR, (fude_zoom_box){ 30, 30, 200, 120 }, 20.0, 0.0, 2.0, &lines);
    {
        const fude_zoom_paper_line* l = (const fude_zoom_paper_line*)lines.memory;
        b8 ok = true;
        u32 rays = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&lines); i++) {
            const f64 ra = hypot(l[i].a.x, l[i].a.y), rb = hypot(l[i].b.x, l[i].b.y);
            const f64 cross = l[i].a.x * l[i].b.y - l[i].a.y * l[i].b.x;
            if(fabs(cross) < 1e-6 * ra * rb) {
                rays++;   // (along a ray)
                const f64 deg = atan2(l[i].a.y, l[i].a.x) * 180.0 / 3.14159265358979323846;
                ok = ok && (fabs(fmod(deg + 360.0, 15.0)) < 1e-6 || fabs(fmod(deg + 360.0, 15.0) - 15.0) < 1e-6);
            } else {
                // (a chord: each end on a ring, a multiple of the half step out — or on the view's edge, cut there)
                const fude_zoom_v2 ends[2] = { l[i].a, l[i].b };
                for(u32 e = 0; e < 2u; e++) {
                    const f64 r = hypot(ends[e].x, ends[e].y);
                    const b8 ring = fabs(fmod(r, 10.0)) < 1e-6 || fabs(fmod(r, 10.0) - 10.0) < 1e-6;
                    const b8 edge = fabs(ends[e].x - 30.0) < 1e-9 || fabs(ends[e].x - 200.0) < 1e-9 || fabs(ends[e].y - 30.0) < 1e-9 || fabs(ends[e].y - 120.0) < 1e-9;
                    ok = ok && (ring || edge);
                }
            }
        }
        CHECK(ok && rays >= 2u);   // (30°, 45°: in that corner of the view)
    }
    // Engineering at 2 points a unit: a line every 10 units (20 points apart: the finest at least 8), every fifth
    // stronger, heavy every tenth (200 points apart).
    rde_arr_clear(&lines);
    fude_zoom_paper_lines(FUDE_PAPER_ENGINEERING, (fude_zoom_box){ -1, -1, 251, 1 }, 1.0, 0.0, 2.0, &lines);
    {
        const fude_zoom_paper_line* l = (const fude_zoom_paper_line*)lines.memory;
        u32 vertical = 0, heavy = 0;
        f32 s10 = 0.0f, s50 = 0.0f;
        for(u32 i = 0; i < (u32)rde_arr_length(&lines); i++) {
            if(l[i].a.x != l[i].b.x) {
                continue;
            }
            vertical++;
            heavy += l[i].heavy ? 1u : 0u;
            s10 = l[i].a.x == 10.0 ? l[i].strength : s10;
            s50 = l[i].a.x == 50.0 ? l[i].strength : s50;
        }
        CHECK(vertical == 26u && heavy == 3u && s50 > s10 && s10 > 0.0f);   // (0, 100, 200 heavy)
    }
    // Music: staves of five a step apart, six steps between; gone when too fine to see.
    rde_arr_clear(&lines);
    fude_zoom_paper_lines(FUDE_PAPER_MUSIC, (fude_zoom_box){ 0, -199, 100, 1 }, 2.0, 0.0, 4.0, &lines);
    CHECK(rde_arr_length(&lines) == 50u && ((const fude_zoom_paper_line*)lines.memory)[1].a.y == -2.0);
    rde_arr_clear(&lines);
    CHECK(fude_zoom_paper_lines(FUDE_PAPER_MUSIC, (fude_zoom_box){ 0, -199, 100, 1 }, 2.0, 0.0, 1.0, &lines) == 0u);
    // Page layouts: Cornell's cue column and summary; a planner's boxes, one a line; neither elsewhere.
    rde_arr_clear(&lines);
    const fude_zoom_box page = { 0, 0, 210, 297 };
    CHECK(fude_zoom_paper_page(FUDE_PAPER_CORNELL, page, 280.0, 8.0, &lines) > 30u);
    {
        const fude_zoom_paper_line* l = (const fude_zoom_paper_line*)lines.memory;
        b8 cue = false, summary = false;
        for(u32 i = 0; i < (u32)rde_arr_length(&lines); i++) {
            cue     = cue || (l[i].heavy && fabs(l[i].a.x - 63.0) < 1e-9 && fabs(l[i].b.x - 63.0) < 1e-9);
            summary = summary || (l[i].heavy && fabs(l[i].a.y - 59.4) < 1e-9 && fabs(l[i].b.y - 59.4) < 1e-9);
        }
        CHECK(cue && summary);
    }
    rde_arr_clear(&lines);
    const u32 planner = fude_zoom_paper_page(FUDE_PAPER_PLANNER, page, 280.0, 8.0, &lines);
    CHECK(planner > 0u && (planner - 1u) % 5u == 0u);   // (its title's line, then each line and its box's four sides)
    CHECK(fude_zoom_paper_page(FUDE_PAPER_LINES, page, 280.0, 8.0, &lines) == 0u && fude_zoom_paper_is_page(FUDE_PAPER_CORNELL) && !fude_zoom_paper_is_page(FUDE_PAPER_HEX));
    // A notebook's page as printed (an A4 from y 0 down): ruled from under its margin, its margin's line last; squares
    // and dots from its top left, none on its edges; blank; the patterns inside it; engineering's millimetres.
    const fude_zoom_box a4 = { -105, -297, 105, 0 };
    rde_arr_clear(&lines);
    CHECK(fude_zoom_paper_sheet(FUDE_PAPER_LINES, a4, 20.0, 8.0, 5.0, 2.0, &lines) == 36u);
    {
        const fude_zoom_paper_line* l = (const fude_zoom_paper_line*)lines.memory;
        CHECK(l[0].a.y == -20.0 && l[0].a.x == -105.0 && l[0].b.x == 105.0 && l[34].a.y == -292.0 && l[0].kind == FUDE_ZOOM_PAPER_PLAIN);
        CHECK(l[35].kind == FUDE_ZOOM_PAPER_MARGIN && l[35].a.x == -85.0 && l[35].b.x == -85.0 && l[35].a.y == 0.0 && l[35].b.y == -297.0);
    }
    rde_arr_clear(&lines);
    CHECK(fude_zoom_paper_sheet(FUDE_PAPER_SQUARES, a4, 20.0, 8.0, 5.0, 2.0, &lines) == 59u + 41u);
    rde_arr_clear(&lines);
    CHECK(fude_zoom_paper_sheet(FUDE_PAPER_DOTS, a4, 20.0, 8.0, 5.0, 2.0, &lines) == 59u * 41u);
    {
        const fude_zoom_paper_line* l = (const fude_zoom_paper_line*)lines.memory;
        b8 dots = true;
        for(u32 i = 0; i < (u32)rde_arr_length(&lines); i++) {
            dots = dots && l[i].kind == FUDE_ZOOM_PAPER_DOT && l[i].a.x == l[i].b.x && l[i].a.y == l[i].b.y && l[i].a.x > -105.0 && l[i].a.x < 105.0 &&
                   l[i].a.y < 0.0 && l[i].a.y > -297.0;
        }
        CHECK(dots && l[0].a.x == -100.0 && l[0].a.y == -5.0);
    }
    rde_arr_clear(&lines);
    CHECK(fude_zoom_paper_sheet(FUDE_PAPER_NONE, a4, 20.0, 8.0, 5.0, 2.0, &lines) == 0u);
    CHECK(fude_zoom_paper_sheet(FUDE_PAPER_LINES, (fude_zoom_box){ 0, 0, 0, 10 }, 20.0, 8.0, 5.0, 2.0, &lines) == 0u);
    for(u8 paper = FUDE_PAPER_ISOMETRIC; paper < FUDE_PAPER_COUNT; paper++) {
        rde_arr_clear(&lines);
        CHECK(fude_zoom_paper_sheet(paper, a4, 20.0, 8.0, 5.0, 2.0, &lines) > 10u);
        const fude_zoom_paper_line* l = (const fude_zoom_paper_line*)lines.memory;
        b8 inside = true;
        for(u32 i = 0; i < (u32)rde_arr_length(&lines); i++) {
            inside = inside && fmin(l[i].a.x, l[i].b.x) >= -105.0 - 1e-6 && fmax(l[i].a.x, l[i].b.x) <= 105.0 + 1e-6 &&
                     fmin(l[i].a.y, l[i].b.y) >= -297.0 - 1e-6 && fmax(l[i].a.y, l[i].b.y) <= 1e-6 && l[i].strength > 0.0f;
        }
        CHECK(inside);
    }
    rde_arr_clear(&lines);
    fude_zoom_paper_sheet(FUDE_PAPER_ENGINEERING, a4, 20.0, 8.0, 5.0, 2.0, &lines);
    {
        const fude_zoom_paper_line* l = (const fude_zoom_paper_line*)lines.memory;
        u32 down = 0;
        f32 at_one = 0.0f, at_ten = 0.0f;
        for(u32 i = 0; i < (u32)rde_arr_length(&lines); i++) {
            if(l[i].a.x == l[i].b.x) {
                down++;
                at_one = l[i].a.x == 1.0 ? l[i].strength : at_one;
                at_ten = l[i].a.x == 10.0 ? l[i].strength : at_ten;
            }
        }
        CHECK(down == 211u && at_one > 0.0f && at_one < 0.5f && at_ten > 0.9f);   // (a line every millimetre; every tenth strong)
    }
    rde_arr_free(&lines);
}

// A text read as a note (rich.h): a made-up font, each character half its size wide (bold a little wider).
static f32 rich_measure(any self, const c8* text, u32 len, f32 px, u8 style) {
    (void)self;
    u32 chars = 0;
    for(u32 i = 0; i < len; i++) {
        chars += ((u8)text[i] & 0xC0u) != 0x80u ? 1u : 0u;
    }
    return (f32)chars * px * ((style & FUDE_ZOOM_RICH_BOLD) ? 0.55f : 0.5f);
}

static b8 rich_run_is(const fude_zoom_rich* r, const c8* text, u32 k, const c8* words, u8 style) {
    if(k >= (u32)rde_arr_length(&r->runs)) {
        return false;
    }
    const fude_zoom_rich_run* q = &((const fude_zoom_rich_run*)r->runs.memory)[k];
    const c8* from = (q->style & FUDE_ZOOM_RICH_OWN) ? FUDE_ZOOM_RICH_BULLET : &text[q->from];
    return q->len == (u32)strlen(words) && strncmp(from, words, q->len) == 0 && q->style == style;
}

static u32 rich_marks_of(const fude_zoom_rich* r, u8 kind) {
    u32 n = 0;
    for(u32 i = 0; i < (u32)rde_arr_length(&r->marks); i++) {
        n += ((const fude_zoom_rich_mark*)r->marks.memory)[i].kind == kind ? 1u : 0u;
    }
    return n;
}

static void test_rich(void) {
    fude_zoom_rich r;
    fude_zoom_rich_init(&r);
    // Plain: nothing marked; maths and snake_case stay as written.
    CHECK(!fude_zoom_rich_marked("Hello world", 11));
    CHECK(!fude_zoom_rich_marked("2 * 3 * 4 = 24", 14));
    CHECK(!fude_zoom_rich_marked("my_var_name and **not closed", 28));
    CHECK(!fude_zoom_rich_marked("a - b, 2026. was good", 21));
    CHECK(fude_zoom_rich_marked("**x**", 5) && fude_zoom_rich_marked("- one", 5) && fude_zoom_rich_marked("see https://x.org", 17));
    CHECK(fude_zoom_rich_marked("# Title", 7) && fude_zoom_rich_marked("[ ] milk", 8) && fude_zoom_rich_marked("| a |", 5));
    // A heading, then bold and italic in a line.
    const c8* t1 = "# Title\nSome **bold** and *it* here";
    fude_zoom_rich_lay(&r, t1, (u32)strlen(t1), 0.0f, 10.0f, 1.3f, rich_measure, NULL);
    CHECK(rde_arr_length(&r.runs) == 6u);
    CHECK(rich_run_is(&r, t1, 0, "Title", FUDE_ZOOM_RICH_BOLD) && ((const fude_zoom_rich_run*)r.runs.memory)[0].px == 15.0f);
    CHECK(rich_run_is(&r, t1, 1, "Some", 0) && rich_run_is(&r, t1, 2, "bold", FUDE_ZOOM_RICH_BOLD) && rich_run_is(&r, t1, 3, "and", 0));
    CHECK(rich_run_is(&r, t1, 4, "it", FUDE_ZOOM_RICH_ITALIC) && rich_run_is(&r, t1, 5, "here", 0));
    {
        const fude_zoom_rich_run* q = (const fude_zoom_rich_run*)r.runs.memory;
        // (the heading's line as tall as its letters; "Some" one space before "bold": 4 letters + a space)
        CHECK(fabsf(q[1].y - (15.0f * 1.3f + 10.0f * 0.95f)) < 1e-4f && fabsf(q[2].x - 25.0f) < 1e-4f);
        CHECK(fabsf(r.h - (15.0f * 1.3f + 13.0f)) < 1e-4f);
    }
    // Tasks: their boxes, where a tap ticks them; one done, struck and softer.
    const c8* t2 = "- [ ] milk\n[x] eggs";
    fude_zoom_rich_lay(&r, t2, (u32)strlen(t2), 0.0f, 10.0f, 1.3f, rich_measure, NULL);
    CHECK(rde_arr_length(&r.hits) == 2u && rich_marks_of(&r, FUDE_ZOOM_RICH_MARK_BOX) == 1u && rich_marks_of(&r, FUDE_ZOOM_RICH_MARK_TICKED) == 1u);
    {
        const fude_zoom_rich_hit* h = (const fude_zoom_rich_hit*)r.hits.memory;
        CHECK(h[0].kind == FUDE_ZOOM_RICH_HIT_TASK && h[0].at == 2u && h[1].kind == FUDE_ZOOM_RICH_HIT_DONE && h[1].at == 11u);
        CHECK(fude_zoom_rich_hit_at(&r, 5.0f, 6.0f, 0.0f) == 0u && fude_zoom_rich_hit_at(&r, 5.0f, 19.0f, 0.0f) == 1u && fude_zoom_rich_hit_at(&r, 80.0f, 6.0f, 0.0f) == FUDE_ZOOM_RICH_NONE);
    }
    CHECK(rich_run_is(&r, t2, 0, "milk", 0) && rich_run_is(&r, t2, 1, "eggs", FUDE_ZOOM_RICH_SOFT | FUDE_ZOOM_RICH_STRUCK));
    CHECK(rich_marks_of(&r, FUDE_ZOOM_RICH_MARK_LINE) == 1u);   // (its strike)
    c8 tick[32];
    snprintf(tick, sizeof tick, "%s", t2);
    CHECK(fude_zoom_rich_tick(tick, (u32)strlen(tick), 2u) && strcmp(tick, "- [x] milk\n[x] eggs") == 0);
    CHECK(fude_zoom_rich_tick(tick, (u32)strlen(tick), 11u) && strcmp(tick, "- [x] milk\n[ ] eggs") == 0);
    CHECK(!fude_zoom_rich_tick(tick, (u32)strlen(tick), 0u));
    // Bullets in levels, numbers as written.
    const c8* t3 = "- one\n  - two\n1. a\n10. b";
    fude_zoom_rich_lay(&r, t3, (u32)strlen(t3), 0.0f, 10.0f, 1.3f, rich_measure, NULL);
    CHECK(rde_arr_length(&r.runs) == 8u);
    {
        const fude_zoom_rich_run* q = (const fude_zoom_rich_run*)r.runs.memory;
        CHECK(rich_run_is(&r, t3, 0, FUDE_ZOOM_RICH_BULLET, FUDE_ZOOM_RICH_OWN) && fabsf(q[0].x - 2.0f) < 1e-4f && fabsf(q[1].x - 11.0f) < 1e-4f);
        CHECK(rich_run_is(&r, t3, 2, FUDE_ZOOM_RICH_BULLET, FUDE_ZOOM_RICH_OWN) && fabsf(q[2].x - 16.0f) < 1e-4f && fabsf(q[3].x - 25.0f) < 1e-4f);
        CHECK(rich_run_is(&r, t3, 4, "1.", 0) && rich_run_is(&r, t3, 6, "10.", 0) && rich_run_is(&r, t3, 7, "b", 0) && fabsf(q[7].x - 19.0f) < 1e-4f);
    }
    // Wrapping: words to the next line, joined runs; a word longer than a line cut between its letters, none lost.
    const c8* t4 = "aaaa bbbb cccc";
    fude_zoom_rich_lay(&r, t4, (u32)strlen(t4), 50.0f, 10.0f, 1.3f, rich_measure, NULL);
    CHECK(rde_arr_length(&r.runs) == 2u && rich_run_is(&r, t4, 0, "aaaa bbbb", 0) && rich_run_is(&r, t4, 1, "cccc", 0) && fabsf(r.h - 26.0f) < 1e-4f);
    const c8* t5 = "abcdefghijklmnop";
    fude_zoom_rich_lay(&r, t5, (u32)strlen(t5), 30.0f, 10.0f, 1.3f, rich_measure, NULL);
    {
        u32 letters = 0;
        for(u32 k = 0; k < (u32)rde_arr_length(&r.runs); k++) {
            letters += ((const fude_zoom_rich_run*)r.runs.memory)[k].len;
            CHECK(((const fude_zoom_rich_run*)r.runs.memory)[k].w <= 30.0f + 1e-4f);
        }
        CHECK(rde_arr_length(&r.runs) == 3u && letters == 16u && fabsf(r.h - 39.0f) < 1e-4f);
    }
    // Links: written as [words](address), and written out (its full stop the sentence's).
    const c8* t6 = "see [the site](example.com) and https://x.org/a.";
    fude_zoom_rich_lay(&r, t6, (u32)strlen(t6), 0.0f, 10.0f, 1.3f, rich_measure, NULL);
    CHECK(rich_run_is(&r, t6, 1, "the site", FUDE_ZOOM_RICH_LINK) && rich_run_is(&r, t6, 3, "https://x.org/a", FUDE_ZOOM_RICH_LINK) && rich_run_is(&r, t6, 4, ".", 0));
    CHECK(rde_arr_length(&r.hits) == 2u);
    {
        const fude_zoom_rich_hit* h = (const fude_zoom_rich_hit*)r.hits.memory;
        c8 a[64];
        fude_zoom_rich_address(t6, h[0].at, h[0].len, a, sizeof a);
        CHECK(h[0].kind == FUDE_ZOOM_RICH_HIT_LINK && strcmp(a, "https://example.com") == 0);
        fude_zoom_rich_address(t6, h[1].at, h[1].len, a, sizeof a);
        CHECK(strcmp(a, "https://x.org/a") == 0);
        fude_zoom_rich_address("mailto:me@x.org", 0, 15, a, sizeof a);
        CHECK(strcmp(a, "mailto:me@x.org") == 0);
    }
    // Code, struck, and marks left as written when they do not close.
    const c8* t7 = "`a b` ~~gone~~ **open";
    fude_zoom_rich_lay(&r, t7, (u32)strlen(t7), 0.0f, 10.0f, 1.3f, rich_measure, NULL);
    CHECK(rich_run_is(&r, t7, 0, "a b", FUDE_ZOOM_RICH_CODE) && rich_run_is(&r, t7, 1, "gone", FUDE_ZOOM_RICH_STRUCK) && rich_run_is(&r, t7, 2, "**open", 0));
    CHECK(rich_marks_of(&r, FUDE_ZOOM_RICH_MARK_GROUND) == 1u && rich_marks_of(&r, FUDE_ZOOM_RICH_MARK_LINE) == 1u);
    // A table: its header bold on its ground, its columns as wide as their widest cells, the second to the right.
    const c8* t8 = "| a | bb |\n|---|--:|\n| ccc | d |";
    fude_zoom_rich_lay(&r, t8, (u32)strlen(t8), 0.0f, 10.0f, 1.3f, rich_measure, NULL);
    CHECK(rde_arr_length(&r.runs) == 4u && rich_run_is(&r, t8, 0, "a", FUDE_ZOOM_RICH_BOLD) && rich_run_is(&r, t8, 1, "bb", FUDE_ZOOM_RICH_BOLD));
    CHECK(rich_run_is(&r, t8, 2, "ccc", 0) && rich_run_is(&r, t8, 3, "d", 0));
    CHECK(rich_marks_of(&r, FUDE_ZOOM_RICH_MARK_GROUND) == 1u && rich_marks_of(&r, FUDE_ZOOM_RICH_MARK_LINE) == 6u);
    {
        const fude_zoom_rich_run* q = (const fude_zoom_rich_run*)r.runs.memory;
        // (columns: 15 + 9 = 24 and 11 + 9 = 20 wide; "d" against the second's right, its room in from it)
        CHECK(fabsf(r.w - 44.0f) < 1e-3f && fabsf(q[3].x + q[3].w - (44.0f - 4.5f)) < 1e-3f && fabsf(q[2].x - 4.5f) < 1e-3f);
        CHECK(fabsf(r.h - 2.0f * (13.0f + 4.4f)) < 1e-3f);
    }
    // ...narrower than it would be: its cells wrapped, never wider than the room.
    const c8* t9 = "| one two three | four five six |";
    fude_zoom_rich_lay(&r, t9, (u32)strlen(t9), 60.0f, 10.0f, 1.3f, rich_measure, NULL);
    CHECK(r.w <= 60.0f + 1e-3f && r.h > 2.0f * 13.0f);
    // A rule, across what is widest.
    const c8* t10 = "abcd\n---\nb";
    fude_zoom_rich_lay(&r, t10, (u32)strlen(t10), 0.0f, 10.0f, 1.3f, rich_measure, NULL);
    CHECK(rich_marks_of(&r, FUDE_ZOOM_RICH_MARK_LINE) == 1u && fabsf(((const fude_zoom_rich_mark*)r.marks.memory)[0].x1 - 20.0f) < 1e-4f);
    // Nothing at all.
    fude_zoom_rich_lay(&r, "", 0u, 0.0f, 10.0f, 1.3f, rich_measure, NULL);
    CHECK(rde_arr_length(&r.runs) == 0u && r.w == 0.0f && r.h == 0.0f);
    // Marks put in by a tap: wrapped and unwrapped, lines made lists, tasks, numbers, headings and
    // back, a table, a link.
    {
        c8 out[512];
        u32 a = 0, b = 0;
        CHECK(fude_zoom_rich_format("make it bold", 12, 8, 12, FUDE_ZOOM_RICH_DO_BOLD, out, sizeof out, &a, &b) && strcmp(out, "make it **bold**") == 0 && a == 10u && b == 14u);
        c8 back[512];
        CHECK(fude_zoom_rich_format("make it **bold**", 16, 10, 14, FUDE_ZOOM_RICH_DO_BOLD, back, sizeof back, &a, &b) && strcmp(back, "make it bold") == 0 && a == 8u && b == 12u);
        CHECK(fude_zoom_rich_format("ab", 2, 1, 1, FUDE_ZOOM_RICH_DO_ITALIC, out, sizeof out, &a, &b) && strcmp(out, "a**b") == 0 && a == 2u && b == 2u);
        CHECK(fude_zoom_rich_format("a **b**", 7, 4, 5, FUDE_ZOOM_RICH_DO_ITALIC, out, sizeof out, &a, &b) && strcmp(out, "a ***b***") == 0);   // (bold's stars not italic's)
        CHECK(fude_zoom_rich_format("milk\neggs\nbread", 15, 2, 7, FUDE_ZOOM_RICH_DO_BULLETS, out, sizeof out, &a, &b) && strcmp(out, "- milk\n- eggs\nbread") == 0 && a == 4u && b == 11u);
        CHECK(fude_zoom_rich_format("- milk\n- eggs", 13, 0, 13, FUDE_ZOOM_RICH_DO_BULLETS, out, sizeof out, &a, &b) && strcmp(out, "milk\neggs") == 0);
        CHECK(fude_zoom_rich_format("- milk\neggs", 11, 0, 11, FUDE_ZOOM_RICH_DO_TASKS, out, sizeof out, &a, &b) && strcmp(out, "- [ ] milk\n- [ ] eggs") == 0);
        CHECK(fude_zoom_rich_format("- [x] milk\neggs", 15, 0, 15, FUDE_ZOOM_RICH_DO_TASKS, out, sizeof out, &a, &b) && strcmp(out, "- [x] milk\n- [ ] eggs") == 0);
        CHECK(fude_zoom_rich_format("- [x] milk\n- [ ] eggs", 21, 0, 21, FUDE_ZOOM_RICH_DO_TASKS, out, sizeof out, &a, &b) && strcmp(out, "milk\neggs") == 0);
        CHECK(fude_zoom_rich_format("  a\nb\nc", 8, 0, 8, FUDE_ZOOM_RICH_DO_NUMBERS, out, sizeof out, &a, &b) && strcmp(out, "  1. a\n2. b\n3. c") == 0);
        CHECK(fude_zoom_rich_format("1. a\n2. b", 9, 0, 9, FUDE_ZOOM_RICH_DO_NUMBERS, out, sizeof out, &a, &b) && strcmp(out, "a\nb") == 0);
        const c8* h[4] = { "# Title", "## Title", "### Title", "Title" };
        const c8* was = "- Title";
        for(u32 k = 0; k < 4u; k++) {
            c8 hh[64];
            CHECK(fude_zoom_rich_format(was, (u32)strlen(was), 3, 3, FUDE_ZOOM_RICH_DO_HEADING, hh, sizeof hh, &a, &b) && strcmp(hh, h[k]) == 0);
            snprintf(back, sizeof back, "%s", hh);
            was = back;
        }
        CHECK(fude_zoom_rich_format("one\ntwo", 7, 2, 2, FUDE_ZOOM_RICH_DO_TABLE, out, sizeof out, &a, &b) && strcmp(out, "one\n|  |  |\n|---|---|\n|  |  |\ntwo") == 0 && a == 6u);
        CHECK(fude_zoom_rich_format("see site", 8, 4, 8, FUDE_ZOOM_RICH_DO_LINK, out, sizeof out, &a, &b) && strcmp(out, "see [site](https://)") == 0 && a == 19u && a == b);
        CHECK(fude_zoom_rich_format("go ", 3, 3, 3, FUDE_ZOOM_RICH_DO_LINK, out, sizeof out, &a, &b) && strcmp(out, "go https://") == 0 && a == 11u);
        // (each one's output a note again: what it marks read back)
        CHECK(fude_zoom_rich_marked(out, (u32)strlen(out)));
        c8 tiny[8];
        CHECK(!fude_zoom_rich_format("make it bold", 12, 8, 12, FUDE_ZOOM_RICH_DO_BOLD, tiny, sizeof tiny, &a, &b));
    }
    // As it is being written (lay_raw): every byte in its runs, its marks faint, a caret's place at each byte and back.
    {
        const c8* w = "# Plan\n- [ ] **glue** it\n\n  indented line";
        const u32 wl = (u32)strlen(w);
        fude_zoom_rich_lay_raw(&r, w, wl, 0.0f, 10.0f, 1.3f, rich_measure, NULL);
        const fude_zoom_rich_run* q = (const fude_zoom_rich_run*)r.runs.memory;
        u32 covered = 0, marks = 0;
        for(u32 k = 0; k < (u32)rde_arr_length(&r.runs); k++) {
            covered += q[k].len;
            marks   += (q[k].style & FUDE_ZOOM_RICH_MARK) ? 1u : 0u;
        }
        // (every byte but the line breaks and the spaces between words)
        u32 spaces = 0;
        for(u32 i = 0; i < wl; i++) spaces += w[i] == ' ' || w[i] == '\n' ? 1u : 0u;
        CHECK(rde_arr_length(&r.lines) == 4u && marks >= 3u && covered + spaces >= wl - 1u && covered <= wl);
        // ("glue" bold, its stars faint)
        b8 glue = false, stars = false;
        for(u32 k = 0; k < (u32)rde_arr_length(&r.runs); k++) {
            glue  = glue  || (q[k].len == 4u && strncmp(&w[q[k].from], "glue", 4) == 0 && (q[k].style & FUDE_ZOOM_RICH_BOLD) && !(q[k].style & FUDE_ZOOM_RICH_MARK));
            stars = stars || (q[k].len == 2u && strncmp(&w[q[k].from], "**", 2) == 0 && (q[k].style & FUDE_ZOOM_RICH_MARK));
        }
        CHECK(glue && stars);
        // A caret at each byte, and the byte a tap there finds: the same.
        u32 same = 0, places = 0;
        f32 lasty = -1.0f;
        b8 down = true;
        for(u32 at = 0; at <= wl; at++) {
            f32 x, y, px;
            fude_zoom_rich_caret(&r, w, wl, at, rich_measure, NULL, &x, &y, &px);
            down = down && y >= lasty - 1e-4f;   // (never back up the page)
            lasty = y;
            places++;
            same += fude_zoom_rich_byte_at(&r, w, wl, x + 0.1f, y - px * 0.3f, rich_measure, NULL) == at ? 1u : 0u;
        }
        CHECK(down && same == places);
        // (a tap past a line's end: its end; under everything: the end)
        f32 x, y, px;
        fude_zoom_rich_caret(&r, w, wl, 6u, rich_measure, NULL, &x, &y, &px);
        CHECK(fude_zoom_rich_byte_at(&r, w, wl, x + 500.0f, y - 2.0f, rich_measure, NULL) == 6u);
        CHECK(fude_zoom_rich_byte_at(&r, w, wl, 0.0f, 1000.0f, rich_measure, NULL) == 26u);      // (its last line's start)
        CHECK(fude_zoom_rich_byte_at(&r, w, wl, 1000.0f, 1000.0f, rich_measure, NULL) == wl);   // (...its end)
        // Wrapped: the caret down a row at the word that went on.
        const c8* v = "aaaa bbbb cccc";
        fude_zoom_rich_lay_raw(&r, v, 14u, 50.0f, 10.0f, 1.3f, rich_measure, NULL);
        fude_zoom_rich_caret(&r, v, 14u, 10u, rich_measure, NULL, &x, &y, &px);
        CHECK(x == 0.0f && fabsf(y - (13.0f + 9.5f)) < 1e-4f);
        CHECK(fude_zoom_rich_byte_at(&r, v, 14u, 300.0f, 9.0f, rich_measure, NULL) == 9u);
        // Nothing yet: a caret at its start.
        fude_zoom_rich_lay_raw(&r, "", 0u, 0.0f, 10.0f, 1.3f, rich_measure, NULL);
        fude_zoom_rich_caret(&r, "", 0u, 0u, rich_measure, NULL, &x, &y, &px);
        CHECK(rde_arr_length(&r.lines) == 1u && x == 0.0f && fabsf(y - 9.5f) < 1e-4f && px == 10.0f);
    }
    fude_zoom_rich_free(&r);
    // Exported as drawn: its words without their marks, bold where they are, its boxes and lines; a PDF's bold face.
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const c8* note = "# Plan\n- [x] **glue** it\n| a | b |";
    fude_zoom_scene_add_text(&s, s.root, (fude_zoom_place){ { -80, 60 }, 0.0, 1.0 }, 12.0, 120.0, 60.0, FUDE_ZOOM_TEXT_STICKY, (rde_color){ 10, 20, 30, 255 },
                             (rde_color){ 255, 233, 140, 255 }, note, (u32)strlen(note), 0);
    // (a diagram shape's label too: a Kanban card's)
    f64 n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 cnt = fude_zoom_symbol_numbers(n, fude_zoom_symbol_find("process"), 60, 35, 10, "**Ship** it\n- [ ] pack");
    fude_zoom_scene_add_shape_fill(&s, s.root, (fude_zoom_place){ { 100, -60 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL, n, cnt, (rde_color){ 1, 2, 3, 255 }, 1.0f, 0,
                                   (rde_color){ 0, 0, 0, 0 }, 0);
    fude_bytes svg = fude_bytes_new(4096);
    CHECK(fude_zoom_export_svg(&s, (fude_zoom_v2){ 200, 150 }, (rde_color){ 255, 255, 255, 255 }, &svg));
    rde_arr_add(&svg, &(u8){ 0 });
    {
        const c8* text = (const c8*)svg.memory;
        const c8* plan = strstr(text, ">Plan<");
        const c8* glue = strstr(text, ">glue<");
        CHECK(plan != NULL && glue != NULL && strstr(text, "**") == NULL && strstr(text, "[x]") == NULL && strstr(text, "| a") == NULL);
        CHECK(strstr(text, ">a<") != NULL && strstr(text, ">b<") != NULL && strstr(text, "fill=\"#ffe98c\"") != NULL);
        const c8* t = glue; while(t > text && strncmp(t, "<text", 5) != 0) t--;
        CHECK(strstr(t, "font-weight=\"bold\"") != NULL && strstr(t, "font-weight=\"bold\"") < glue);
        const c8* ship = strstr(text, ">Ship<");
        CHECK(ship != NULL && strstr(text, ">pack<") != NULL && strstr(text, "[ ]") == NULL);
        t = ship; while(t > text && strncmp(t, "<text", 5) != 0) t--;
        CHECK(strstr(t, "font-weight=\"bold\"") != NULL && strstr(t, "font-weight=\"bold\"") < ship);
    }
    rde_arr_free(&svg);
    fude_bytes pdf = fude_bytes_new(4096);
    CHECK(fude_zoom_export_pdf(&s, (fude_zoom_v2){ 200, 150 }, (rde_color){ 255, 255, 255, 255 }, &pdf));
    rde_arr_add(&pdf, &(u8){ 0 });
    CHECK(strstr((const c8*)pdf.memory, "/BaseFont /Helvetica-Bold ") != NULL && strstr((const c8*)pdf.memory, "/BaseFont /Helvetica ") != NULL);
    rde_arr_free(&pdf);
    fude_zoom_scene_destroy(&s);
}

// Brackets and plain shapes (0.1.85): each in its box however it is stretched, a brace's curls and an arrow's head as
// they were, a bracket's text out beside it, a pair's between its two.
static void test_brackets_shapes(void) {
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), NULL), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), NULL);
    u32 brackets = 0, shapes = 0, bad = 0;
    for(u32 k = 0; k < fude_zoom_symbol_count(); k++) {
        const fude_zoom_symbol_info* info = fude_zoom_symbol_info_of(k);
        if(info->family != FUDE_ZOOM_SYMBOL_FAMILY_BRACKETS && info->family != FUDE_ZOOM_SYMBOL_FAMILY_SHAPES) {
            continue;
        }
        brackets += info->family == FUDE_ZOOM_SYMBOL_FAMILY_BRACKETS ? 1u : 0u;
        shapes   += info->family == FUDE_ZOOM_SYMBOL_FAMILY_SHAPES ? 1u : 0u;
        const f64 stretch[4][2] = { { 1, 1 }, { 1, 12 }, { 9, 1 }, { 0.3, 0.3 } };
        for(u32 t = 0; t < 4u; t++) {
            const f64 hw = info->w * 0.5 * stretch[t][0], hh = info->h * 0.5 * stretch[t][1];
            const u32 np = fude_zoom_symbol_parts(k, hw, hh, 64, &pts, &parts);
            b8 ok = np > 0u;
            const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
            for(u32 i = 0; i < (u32)rde_arr_length(&pts); i++) {
                ok = ok && fabs(p[i].x) <= hw + 1e-6 && fabs(p[i].y) <= hh + 1e-6 && isfinite(p[i].x) && isfinite(p[i].y);
            }
            // (no two points in a row the same: a line's turn there has no way)
            const fude_zoom_symbol_part* pa = (const fude_zoom_symbol_part*)parts.memory;
            for(u32 j = 0; j < np; j++) {
                for(u32 i = 1; i < pa[j].count; i++) {
                    ok = ok && hypot(p[pa[j].first + i].x - p[pa[j].first + i - 1u].x, p[pa[j].first + i].y - p[pa[j].first + i - 1u].y) > 1e-9;
                }
            }
            if(!ok) {
                printf("symbol %u (%s) stretched %g x %g: out of its box\n", k, info->id, stretch[t][0], stretch[t][1]);
                bad++;
            }
        }
    }
    CHECK(bad == 0u);
    CHECK(brackets == 22u && shapes == 29u);
    // A brace's curls: as round, ten times as tall.
    const u32 brace = fude_zoom_symbol_find("left brace");
    f64 top[2];
    for(u32 t = 0; t < 2u; t++) {
        const f64 hh = t == 0u ? 75.0 : 750.0;
        fude_zoom_symbol_parts(brace, 15, hh, 64, &pts, &parts);
        // (its first curl ends where its spine starts down: the first point at its spine's x)
        const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
        u32 i = 0;
        while(i < (u32)rde_arr_length(&pts) && fabs(p[i].x) > 1e-6) {
            i++;
        }
        top[t] = hh - p[i].y;
    }
    CHECK(fabs(top[0] - top[1]) < 1e-6 && top[0] > 5.0);
    // A parenthesis: its middle at its back, its ends at its front.
    fude_zoom_symbol_parts(fude_zoom_symbol_find("left parenthesis"), 15, 75, 64, &pts, &parts);
    {
        const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
        const u32 n = (u32)rde_arr_length(&pts);
        CHECK(n >= 5u && fabs(p[0].y + p[n - 1u].y) < 1e-6 && fabs(fabs(p[0].y) - 75.0) < 1e-6 && p[0].x > 10.0 && fabs(p[n / 2u].x + 10.5) < 1e-6);
    }
    // Their texts: out beside one, over or under a brace, between a pair.
    fude_zoom_box b[2];
    CHECK(fude_zoom_symbol_text_boxes(fude_zoom_symbol_find("left brace"), 15, 75, 1, b, 2) == 1u && b[0].max_x < -15.0 && b[0].min_y == -75.0);
    CHECK(fude_zoom_symbol_text_boxes(fude_zoom_symbol_find("right brace"), 15, 75, 1, b, 2) == 1u && b[0].min_x > 15.0);
    CHECK(fude_zoom_symbol_text_boxes(fude_zoom_symbol_find("over brace"), 110, 15, 1, b, 2) == 1u && b[0].min_y > 15.0);
    CHECK(fude_zoom_symbol_text_boxes(fude_zoom_symbol_find("under brace"), 110, 15, 1, b, 2) == 1u && b[0].max_y < -15.0);
    const u32 parens = fude_zoom_symbol_find("parentheses");
    CHECK(fude_zoom_symbol_text_boxes(parens, 90, 40, 1, b, 2) == 1u);
    fude_zoom_symbol_parts(parens, 90, 40, 64, &pts, &parts);
    {
        // (its left one's front: the most right of its first part's points)
        const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
        const fude_zoom_symbol_part* pa = (const fude_zoom_symbol_part*)parts.memory;
        f64 front = -1e9;
        for(u32 i = 0; i < pa[0].count; i++) {
            front = fmax(front, p[pa[0].first + i].x);
        }
        CHECK(rde_arr_length(&parts) == 2u && b[0].min_x > front && b[0].max_x < -front);
    }
    // Fitted ones reach every side of their box.
    const c8* fitted[] = { "star", "heart", "pentagon", "curved arrow", "burst" };
    for(u32 f = 0; f < 5u; f++) {
        fude_zoom_symbol_parts(fude_zoom_symbol_find(fitted[f]), 50, 30, 64, &pts, &parts);
        const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
        f64 x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
        const fude_zoom_symbol_part* pa = (const fude_zoom_symbol_part*)parts.memory;
        for(u32 i = 0; i < pa[0].count; i++) {
            x0 = fmin(x0, p[i].x); x1 = fmax(x1, p[i].x); y0 = fmin(y0, p[i].y); y1 = fmax(y1, p[i].y);
        }
        CHECK(fabs(x0 + 50) < 1e-6 && fabs(x1 - 50) < 1e-6 && fabs(y0 + 30) < 1e-6 && fabs(y1 - 30) < 1e-6);
    }
    // A block arrow's head as long as it was, four times as long an arrow.
    const u32 arrow = fude_zoom_symbol_find("block arrow");
    f64 head[2];
    for(u32 t = 0; t < 2u; t++) {
        const f64 hw = t == 0u ? 75.0 : 300.0;
        fude_zoom_symbol_parts(arrow, hw, 35, 64, &pts, &parts);
        head[t] = hw - ((const fude_zoom_v2*)pts.memory)[1].x;
    }
    CHECK(fabs(head[0] - head[1]) < 1e-6 && fabs(head[0] - 38.5) < 1e-6);
    // A ring: its hole joined to it.
    CHECK(fude_zoom_symbol_parts(fude_zoom_symbol_find("ring"), 48, 48, 64, &pts, &parts) == 2u &&
          (((const fude_zoom_symbol_part*)parts.memory)[1].flags & FUDE_ZOOM_SYMBOL_JOIN) != 0u);
    rde_arr_free(&pts);
    rde_arr_free(&parts);
}

static void test_symbols(void) {
    CHECK(fude_zoom_symbol_count() >= 130u && fude_zoom_symbol_info_of(fude_zoom_symbol_count()) == NULL);
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), NULL), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), NULL), outline = rde_arr_new(sizeof(fude_zoom_v2), NULL);
    u32 bad = 0;
    for(u32 k = 0; k < fude_zoom_symbol_count(); k++) {
        const fude_zoom_symbol_info* info = fude_zoom_symbol_info_of(k);
        const f64 hw = info->w * 0.5, hh = info->h * 0.5;
        const u32 np = fude_zoom_symbol_parts(k, hw, hh, 64, &pts, &parts);
        const u32 no = fude_zoom_symbol_outline(k, hw, hh, 64, &outline);
        b8 inside = np > 0u && no >= 2u && fude_zoom_symbol_find(info->id) == k;
        const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
        for(u32 i = 0; i < (u32)rde_arr_length(&pts); i++) {
            inside = inside && fabs(p[i].x) <= hw * 1.25 + 1e-6 && fabs(p[i].y) <= hh * 1.25 + 1e-6;   // (a brace's curl a hair out)
        }
        if(!inside) {
            printf("symbol %u (%s): %u parts, outline %u\n", k, info->id, np, no);
            bad++;
        }
    }
    CHECK(bad == 0u);
    CHECK(fude_zoom_symbol_find("no such one") == FUDE_ZOOM_NONE);
    // Found through a map by the whole id (symbol.c): not by a part of it, nor in another case, nor an empty one.
    CHECK(fude_zoom_symbol_find("ring") != FUDE_ZOOM_NONE && fude_zoom_symbol_find("rin") == FUDE_ZOOM_NONE && fude_zoom_symbol_find("ring ") == FUDE_ZOOM_NONE);
    CHECK(fude_zoom_symbol_find("Ring") == FUDE_ZOOM_NONE && fude_zoom_symbol_find("") == FUDE_ZOOM_NONE && fude_zoom_symbol_find(NULL) == FUDE_ZOOM_NONE);
    // Its numbers and text: a class's three parts.
    f64 n[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 cls = fude_zoom_symbol_find("class");
    const u32 count = fude_zoom_symbol_numbers(n, cls, 90, 65, 15, "User\n---\n+ id: int\n+ name: String\n---\n+ save()");
    CHECK(count <= FUDE_ZOOM_SHAPE_NUMBERS && (u32)n[0] == cls && n[1] == 90 && n[2] == 65 && n[3] == 15);
    c8 text[FUDE_ZOOM_SYMBOL_TEXT];
    fude_zoom_symbol_text(n, count, text, sizeof text);
    CHECK(strcmp(text, "User\n---\n+ id: int\n+ name: String\n---\n+ save()") == 0);
    u32 from[8], len[8];
    CHECK(fude_zoom_symbol_text_split(text, from, len, 8) == 3u);
    CHECK(len[0] == 4u && strncmp(&text[from[0]], "User", 4) == 0 && strncmp(&text[from[1]], "+ id: int\n+ name: String", len[1]) == 0 && len[1] == 24u);
    CHECK(strncmp(&text[from[2]], "+ save()", len[2]) == 0 && len[2] == 8u);
    CHECK(fude_zoom_symbol_text_split("", from, len, 8) == 1u && len[0] == 0u);
    fude_zoom_box boxes[8];
    CHECK(fude_zoom_symbol_text_boxes(cls, 90, 65, 3, boxes, 8) == 3u && boxes[0].max_y == 65 && boxes[1].max_y <= boxes[0].min_y + 1e-9 && boxes[2].min_y >= -65 - 1e-9);
    // Too long a text: cut at a whole character.
    c8 longer[1200];
    for(u32 i = 0; i < 590u; i++) { longer[2 * i] = (c8)0xC3; longer[2 * i + 1] = (c8)0xA9; }   // "é"s
    longer[1180] = 0;
    const u32 c2 = fude_zoom_symbol_numbers(n, cls, 1, 1, 1, longer);
    fude_zoom_symbol_text(n, c2, text, sizeof text);
    CHECK(c2 <= FUDE_ZOOM_SHAPE_NUMBERS && strlen(text) == FUDE_ZOOM_SYMBOL_TEXT - 2u && ((u8)text[strlen(text) - 1u]) == 0xA9u);
    // A connector's ends: each style something, the hollow and diamond ones the line stopping short.
    for(u32 st = 0; st < FUDE_ZOOM_HEAD_COUNT; st++) {
        fude_zoom_symbol_head(st, false, (fude_zoom_v2){ 100, 0 }, (fude_zoom_v2){ 0, 0 }, 10, &pts, &parts);
        CHECK(rde_arr_length(&parts) > 0u);
    }
    CHECK(fude_zoom_symbol_head(FUDE_ZOOM_HEAD_HOLLOW, false, (fude_zoom_v2){ 100, 0 }, (fude_zoom_v2){ 0, 0 }, 10, &pts, &parts) > 9.0);
    CHECK(fude_zoom_symbol_head(FUDE_ZOOM_HEAD_DIAMOND, false, (fude_zoom_v2){ 100, 0 }, (fude_zoom_v2){ 0, 0 }, 10, &pts, &parts) > 15.0);
    CHECK(fude_zoom_symbol_head(FUDE_ZOOM_HEAD_ONE_MANY, false, (fude_zoom_v2){ 100, 0 }, (fude_zoom_v2){ 0, 0 }, 10, &pts, &parts) == 0.0 && rde_arr_length(&parts) == 4u);
    const u32 heads = FUDE_ZOOM_ARROW_END | FUDE_ZOOM_ARROW_START | FUDE_ZOOM_ARROW_STYLES(FUDE_ZOOM_HEAD_ZERO_MANY, FUDE_ZOOM_HEAD_ONLY_ONE);
    CHECK(FUDE_ZOOM_ARROW_END_STYLE(heads) == FUDE_ZOOM_HEAD_ZERO_MANY && FUDE_ZOOM_ARROW_START_STYLE(heads) == FUDE_ZOOM_HEAD_ONLY_ONE && (heads & 15u) == (FUDE_ZOOM_ARROW_END | FUDE_ZOOM_ARROW_START));
    rde_arr_free(&pts);
    rde_arr_free(&parts);
    rde_arr_free(&outline);
}

// Handwriting for Find: pen strokes grouped into lines of writing, keyed by their strokes.
static void test_hand_lines(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    // "hello": five letters 10 tall, 8 apart; "world" 30 under it; a drawing's long line; a marker's stroke.
    u32 hello[5], world[5];
    for(u32 i = 0; i < 5u; i++) {
        hello[i] = line(&s, 100 + 8.0 * i, 0, 104 + 8.0 * i, 10, 1.0, 1.0f);
        world[i] = line(&s, 100 + 8.0 * i, -30, 104 + 8.0 * i, -20, 1.0, 1.0f);
    }
    line(&s, 0, -200, 300, -200, 1.0, 1.0f);
    const u32 marker = line(&s, 100, 50, 104, 60, 1.0, 1.0f);
    ((fude_zoom_object*)fude_zoom_scene_object(&s, marker))->flags |= FUDE_ZOOM_FLAG_MARKER;   // (a marker's)
    rde_arr lines = rde_arr_new(sizeof(fude_zoom_hand_line), NULL), strokes = rde_arr_new(sizeof(u32), NULL);
    CHECK(fude_zoom_hand_lines(&s, &lines, &strokes) == 2u);
    const fude_zoom_hand_line* l = (const fude_zoom_hand_line*)lines.memory;
    const u32* st = (const u32*)strokes.memory;
    CHECK(l[0].count == 5u && l[1].count == 5u && l[0].key != l[1].key && l[0].frame == s.root);
    b8 in_order = true;
    for(u32 i = 0; i < 5u; i++) {
        in_order = in_order && st[l[0].first + i] == hello[i] && st[l[1].first + i] == world[i];
    }
    CHECK(in_order);
    CHECK(l[0].box.min_y > l[1].box.max_y && fabs(l[0].height - 12.0) < 1e-9);
    const u64 k_hello = l[0].key, k_world = l[1].key;
    // The same again: the same keys. A letter more on "hello": its key changes, "world"'s does not.
    CHECK(fude_zoom_hand_lines(&s, &lines, &strokes) == 2u && ((const fude_zoom_hand_line*)lines.memory)[0].key == k_hello);
    line(&s, 140, 0, 144, 10, 1.0, 1.0f);
    CHECK(fude_zoom_hand_lines(&s, &lines, &strokes) == 2u);
    l = (const fude_zoom_hand_line*)lines.memory;
    CHECK(l[0].count == 6u && l[0].key != k_hello && l[1].key == k_world);
    // A word far to the right: a line of its own; one erased away: gone from its line.
    line(&s, 300, 0, 304, 10, 1.0, 1.0f);
    fude_zoom_scene_set_alive(&s, world[4], false);
    CHECK(fude_zoom_hand_lines(&s, &lines, &strokes) == 3u);
    l = (const fude_zoom_hand_line*)lines.memory;
    CHECK(l[1].count == 4u && l[1].key != k_world && l[2].count == 1u);
    rde_arr_free(&lines);
    rde_arr_free(&strokes);
    fude_zoom_scene_destroy(&s);
}

// The zoom video's frames as Android's encoder takes them: I420, BT.601 video range.
static void test_video_yuv(void) {
    // 4 × 2: white, black; red, blue (top row); the bottom row the same — two 2 × 2 blocks, each averaged.
    const u8 px[2][4][4] = {
        { { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 0, 0, 255 }, { 255, 0, 0, 255 } },
        { { 0, 0, 0, 255 },       { 0, 0, 0, 255 },       { 255, 0, 0, 255 }, { 255, 0, 0, 255 } },
    };
    u8 out[4 * 2 * 3 / 2];
    fude_zoom_video_i420(&px[0][0][0], 4, 2, 16, out);
    CHECK(out[0] == 235 && out[1] == 235 && out[4] == 16 && out[5] == 16);   // white, black: video range
    CHECK(out[2] == 82 && out[3] == 82);                                     // red
    CHECK(out[8] == 128 && out[10] == 128);                                  // grey's colour: none (white and black averaged)
    CHECK(out[9] == 90 && out[11] == 240);                                   // red's U and V
}

// Arcs: kept exact (an outline on its circle, its ends where it says), found from points on a
// circle (a line drawn along a compass), and a fillet's (two lines' corner rounded).
static void test_arcs(void) {
    const f64 pi = 3.141592653589793;
    // A quarter round, r 50, from 0: its ends at (50, 0) and (0, 50), every point on the circle; snapping's three.
    const f64 n[3] = { 50.0, 0.0, pi * 0.5 };
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), NULL);
    b8 closed = true;
    fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_ARC, n, 3, 96, &pts, &closed);
    const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
    u32 k = (u32)rde_arr_length(&pts);
    CHECK(!closed && k == 25u && fabs(p[0].x - 50) < 1e-9 && fabs(p[0].y) < 1e-9 && fabs(p[k - 1u].x) < 1e-9 && fabs(p[k - 1u].y - 50) < 1e-9);
    b8 on = true;
    for(u32 i = 0; i < k; i++) {
        on = on && fabs(hypot(p[i].x, p[i].y) - 50) < 1e-9;
    }
    CHECK(on);
    fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_ARC, n, 3, 1, &pts, &closed);
    p = (const fude_zoom_v2*)pts.memory;
    CHECK(rde_arr_length(&pts) == 3u && fabs(p[1].x - 50 * cos(pi * 0.25)) < 1e-9 && fabs(p[1].y - 50 * sin(pi * 0.25)) < 1e-9);
    // Found: points round (300, -200), r 80, from 30° clockwise 200° (as a compass's pencil put them, f32 a hair off).
    fude_zoom_v2 q[120];
    for(u32 i = 0; i < 120u; i++) {
        const f64 a = pi / 6.0 - (200.0 * pi / 180.0) * (f64)i / 119.0;
        q[i] = (fude_zoom_v2){ (f64)(f32)(300.0 + 80.0 * cos(a)), (f64)(f32)(-200.0 + 80.0 * sin(a)) };
    }
    fude_zoom_v2 c;
    f64 r, from, sweep;
    CHECK(fude_zoom_shape_fit_arc(q, 120, 0.01, &c, &r, &from, &sweep));
    CHECK(fabs(c.x - 300) < 1e-3 && fabs(c.y + 200) < 1e-3 && fabs(r - 80) < 1e-3 && fabs(from - pi / 6.0) < 1e-4 && fabs(sweep + 200.0 * pi / 180.0) < 1e-4);
    // All the way round (and a little past): a whole turn, no more.
    for(u32 i = 0; i < 120u; i++) {
        const f64 a = 2.1 * pi * (f64)i / 119.0;
        q[i] = (fude_zoom_v2){ 10.0 * cos(a), 10.0 * sin(a) };
    }
    CHECK(fude_zoom_shape_fit_arc(q, 120, 0.01, &c, &r, &from, &sweep) && fabs(sweep - 2.0 * pi) < 1e-12);
    // A line is on no circle; nor a wobbly hand.
    for(u32 i = 0; i < 120u; i++) {
        q[i] = (fude_zoom_v2){ (f64)i, 2.0 * (f64)i };
    }
    CHECK(!fude_zoom_shape_fit_arc(q, 120, 0.01, &c, &r, &from, &sweep));
    for(u32 i = 0; i < 120u; i++) {
        const f64 a = pi * (f64)i / 119.0;
        q[i] = (fude_zoom_v2){ (50.0 + 3.0 * sin(7.0 * a)) * cos(a), (50.0 + 3.0 * sin(7.0 * a)) * sin(a) };
    }
    CHECK(!fude_zoom_shape_fit_arc(q, 120, 0.5, &c, &r, &from, &sweep));
    // A fillet: a box's corner (the x axis from 100 to 0, the y axis 0 to 100), r 20 — the lines cut back
    // to 20 from the corner, the arc round (20, 20) a quarter turn between them, touching both.
    fude_zoom_v2 ak, ae, bk, be;
    CHECK(fude_zoom_shape_fillet((fude_zoom_v2){ 100, 0 }, (fude_zoom_v2){ 0, 0 }, (fude_zoom_v2){ 0, 0 }, (fude_zoom_v2){ 0, 100 }, 20.0, &ak, &ae, &bk, &be, &c, &from, &sweep));
    CHECK(ak.x == 100 && ak.y == 0 && fabs(ae.x - 20) < 1e-9 && fabs(ae.y) < 1e-9 && bk.x == 0 && bk.y == 100 && fabs(be.x) < 1e-9 && fabs(be.y - 20) < 1e-9);
    CHECK(fabs(c.x - 20) < 1e-9 && fabs(c.y - 20) < 1e-9 && fabs(fabs(sweep) - pi * 0.5) < 1e-9);
    const f64 fn[3] = { 20.0, from, sweep };
    fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_ARC, fn, 3, 1, &pts, &closed);
    p = (const fude_zoom_v2*)pts.memory;
    CHECK(fabs(p[0].x + c.x - ae.x) < 1e-9 && fabs(p[0].y + c.y - ae.y) < 1e-9 && fabs(p[2].x + c.x - be.x) < 1e-9 && fabs(p[2].y + c.y - be.y) < 1e-9);
    CHECK(hypot(p[1].x + c.x, p[1].y + c.y) < hypot(c.x, c.y));   // (its middle towards the corner: the round inside it)
    // Lines that only meet carried on (a gap at the corner), at 60°: the same, from where they would meet.
    const fude_zoom_v2 u = { cos(pi / 3.0), sin(pi / 3.0) };
    CHECK(fude_zoom_shape_fillet((fude_zoom_v2){ 10, 0 }, (fude_zoom_v2){ 200, 0 }, (fude_zoom_v2){ u.x * 200, u.y * 200 }, (fude_zoom_v2){ u.x * 10, u.y * 10 }, 15.0, &ak, &ae, &bk, &be, &c, &from, &sweep));
    const f64 back = 15.0 / tan(pi / 6.0);
    CHECK(fabs(ae.x - back) < 1e-9 && fabs(ae.y) < 1e-9 && fabs(be.x - u.x * back) < 1e-9 && fabs(be.y - u.y * back) < 1e-9 && fabs(c.y - 15.0) < 1e-9);
    CHECK(fabs(fabs(sweep) - (pi - pi / 3.0)) < 1e-9);
    // Too short for the radius; parallel.
    CHECK(!fude_zoom_shape_fillet((fude_zoom_v2){ 10, 0 }, (fude_zoom_v2){ 0, 0 }, (fude_zoom_v2){ 0, 0 }, (fude_zoom_v2){ 0, 100 }, 20.0, &ak, &ae, &bk, &be, &c, &from, &sweep));
    CHECK(!fude_zoom_shape_fillet((fude_zoom_v2){ 0, 0 }, (fude_zoom_v2){ 100, 0 }, (fude_zoom_v2){ 0, 10 }, (fude_zoom_v2){ 100, 10 }, 5.0, &ak, &ae, &bk, &be, &c, &from, &sweep));
    rde_arr_free(&pts);
}

// Paths: a smooth line through their nodes, closed or not; their nodes alone for snapping; driven by a node.
static void test_paths(void) {
    f64 n[FUDE_ZOOM_SHAPE_NUMBERS] = { 0, 0, 0, 100, 50, 200, 0, 300, 50 };
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), NULL);
    b8 closed = true;
    fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_PATH, n, 9, 120, &pts, &closed);
    const u32 k = (u32)rde_arr_length(&pts);
    const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
    CHECK(!closed && k == 3u * 40u + 1u);
    // Through every node: the stretches start on them, the last ends on the last.
    CHECK(p[0].x == 0 && p[0].y == 0 && fabs(p[40].x - 100) < 1e-9 && fabs(p[40].y - 50) < 1e-9 && fabs(p[80].x - 200) < 1e-9 && p[k - 1].x == 300 && p[k - 1].y == 50);
    // Smooth between them: past the node a little way it keeps on (no corner), never wildly off.
    for(u32 i = 0; i < k; i++) CHECK(p[i].y > -15 && p[i].y < 65);
    // Closed: as many stretches as nodes, no end point repeated.
    n[0] = FUDE_ZOOM_PATH_CLOSED;
    fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_PATH, n, 9, 120, &pts, &closed);
    CHECK(closed && rde_arr_length(&pts) == 4u * 30u);
    // Its nodes alone, for snapping (segments 1).
    fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_PATH, n, 9, 1, &pts, &closed);
    CHECK(rde_arr_length(&pts) == 4u && ((const fude_zoom_v2*)pts.memory)[3].x == 300);
    // Most of them: what a renderer takes.
    f64 many[FUDE_ZOOM_SHAPE_NUMBERS];
    many[0] = 0;
    for(u32 i = 0; i < FUDE_ZOOM_PATH_NODES; i++) { many[1 + 2 * i] = i * 10.0; many[2 + 2 * i] = (i & 1u) ? 10.0 : 0.0; }
    fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_PATH, many, 1 + 2 * FUDE_ZOOM_PATH_NODES, 360, &pts, &closed);
    CHECK(rde_arr_length(&pts) >= 2u * FUDE_ZOOM_PATH_NODES && rde_arr_length(&pts) <= 721u);
    rde_arr_free(&pts);
    // A corner: the line arrives at it straight along its stretch (smooth, it bends on the way).
    const f64 l3[7] = { 0, 0, 0, 100, 0, 100, 100 };
    f64 c3[7]; memcpy(c3, l3, sizeof c3);
    c3[0] = (f64)FUDE_ZOOM_PATH_CORNER(1);
    rde_arr ps = rde_arr_new(sizeof(fude_zoom_v2), NULL);
    f64 bend_smooth = 0, bend_corner = 0;
    fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_PATH, l3, 7, 64, &ps, &closed);
    for(u32 i = 0; i <= 32u; i++) bend_smooth = fmax(bend_smooth, fabs(((const fude_zoom_v2*)ps.memory)[i].y));
    fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_PATH, c3, 7, 64, &ps, &closed);
    for(u32 i = 0; i <= 32u; i++) bend_corner = fmax(bend_corner, fabs(((const fude_zoom_v2*)ps.memory)[i].y));
    CHECK(bend_smooth > 2.0 && bend_corner < 1e-9);
    // Its curve as Catmull-Rom's (what it always was), sampled the same: no handle kept, no change.
    {
        const fude_zoom_v2 nd[4] = { { 0, 0 }, { 100, 50 }, { 200, 0 }, { 300, 50 } };
        fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_PATH, (f64[9]){ 0, 0, 0, 100, 50, 200, 0, 300, 50 }, 9, 120, &ps, &closed);
        b8 same = rde_arr_length(&ps) == 121u;
        for(u32 i = 0; same && i < 3u; i++) {
            const fude_zoom_v2 a = nd[i > 0 ? i - 1u : 0u], b = nd[i], c = nd[i + 1u], d = nd[i + 2u < 4u ? i + 2u : 3u];
            for(u32 k = 0; k < 40u; k++) {
                const fude_zoom_v2 q = fude_zoom_shape_catmull(a, b, c, d, (f64)k / 40.0);
                const fude_zoom_v2 r = ((const fude_zoom_v2*)ps.memory)[i * 40u + k];
                same = same && fabs(q.x - r.x) < 1e-9 && fabs(q.y - r.y) < 1e-9;
            }
        }
        CHECK(same);
    }
    // A handle dragged out at the middle node: still through every node, leaving it the way the
    // handle points (flat out to the right: level there), the stretches bent by it; packed and back.
    {
        const fude_zoom_v2 nd[3] = { { 0, 0 }, { 100, 50 }, { 200, 0 } };
        const fude_zoom_v2 hd[3] = { { 0, 0 }, { 60, 0 }, { 0, 0 } };
        f64 hn[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 hc = fude_zoom_path_pack(0u, nd, hd, 3u, hn);
        CHECK(hc == 13u && ((u64)hn[0] & FUDE_ZOOM_PATH_HANDLES));
        u64 bits; fude_zoom_v2 bn[FUDE_ZOOM_PATH_NODES], bh[FUDE_ZOOM_PATH_NODES];
        CHECK(fude_zoom_path_unpack(hn, hc, &bits, bn, bh) == 3u && bn[1].x == 100 && bh[1].x == 60 && bh[0].x == 0);
        fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_PATH, hn, hc, 80, &ps, &closed);
        const fude_zoom_v2* hp = (const fude_zoom_v2*)ps.memory;
        const u32 hk = (u32)rde_arr_length(&ps);
        CHECK(hk == 81u && fabs(hp[40].x - 100) < 1e-9 && fabs(hp[40].y - 50) < 1e-9 && hp[80].x == 200);
        CHECK(fabs(hp[41].y - 50) < 0.5 && hp[41].x > 100);   // just past it: level, going right
        // ...and without the handle (packed with none): its own curve again, no handles kept.
        const fude_zoom_v2 none[3] = { { 0, 0 }, { 0, 0 }, { 0, 0 } };
        CHECK(fude_zoom_path_pack(0u, nd, none, 3u, hn) == 7u && !((u64)hn[0] & FUDE_ZOOM_PATH_HANDLES));
    }
    rde_arr_free(&ps);
    // Driven: the node at (200, 0) moved to (200, 40).
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    n[0] = 0;
    const u32 path = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 10, 10 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_PATH, n, 9, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    rde_arr died = rde_arr_new(sizeof(u32), NULL), born = rde_arr_new(sizeof(u32), NULL);
    CHECK(fude_zoom_connect_drive(&s, s.root, (fude_zoom_v2){ 210, 10 }, (fude_zoom_v2){ 210, 50 }, 1e-6, FUDE_ZOOM_NONE, &died, &born) == 1u);
    CHECK(!alive(&s, path));
    f64 got[FUDE_ZOOM_SHAPE_NUMBERS];
    CHECK(fude_zoom_scene_shape_numbers(&s, ((const u32*)born.memory)[0], got, FUDE_ZOOM_SHAPE_NUMBERS) == 9u && got[5] == 200 && fabs(got[6] - 40) < 1e-9 && got[3] == 100);
    rde_arr_free(&died); rde_arr_free(&born);
    fude_zoom_scene_destroy(&s);
}

// Sculpt: a stretch of a line redrawn.
static void test_sculpt(void) {
    fude_zoom_v2 line[101], bump[21], back[21], tail[21], head[21], off[5];
    for(u32 i = 0; i <= 100; i++) line[i] = (fude_zoom_v2){ (f64)i, 0.0 };
    for(u32 i = 0; i <= 20; i++) {
        const f64 t = (f64)i / 20.0;
        bump[i] = (fude_zoom_v2){ 30.0 + 40.0 * t, 20.0 * sin(3.14159265358979323846 * t) };
        back[20 - i] = bump[i];
        tail[i] = (fude_zoom_v2){ 60.0 + 20.0 * t, 40.0 * t };
        head[i] = (fude_zoom_v2){ 40.0 - 20.0 * t, 40.0 * t };
    }
    for(u32 i = 0; i < 5; i++) off[i] = (fude_zoom_v2){ 50.0 + i, 30.0 };
    rde_arr out = rde_arr_new(sizeof(fude_zoom_v2), NULL);
    // Begun and ended on it: the stretch between replaced.
    CHECK(fude_zoom_shape_sculpt(line, 101, bump, 21, 2.0, &out));
    const fude_zoom_v2* o = (const fude_zoom_v2*)out.memory;
    CHECK(rde_arr_length(&out) == 30u + 21u + 30u && o[29].x == 29 && o[30].x == 30 && fabs(o[40].y - 20) < 1e-9 && o[51].x == 71 && o[80].x == 100);
    // Drawn against its way: the same line.
    CHECK(fude_zoom_shape_sculpt(line, 101, back, 21, 2.0, &out));
    o = (const fude_zoom_v2*)out.memory;
    CHECK(rde_arr_length(&out) == 81u && o[30].x == 30 && o[50].x == 70 && o[51].x == 71);
    // Ending off it, onwards: its tail goes the new way.
    CHECK(fude_zoom_shape_sculpt(line, 101, tail, 21, 2.0, &out));
    o = (const fude_zoom_v2*)out.memory;
    CHECK(rde_arr_length(&out) == 60u + 21u && o[59].x == 59 && o[80].x == 80 && o[80].y == 40);
    // ...backwards: its head.
    CHECK(fude_zoom_shape_sculpt(line, 101, head, 21, 2.0, &out));
    o = (const fude_zoom_v2*)out.memory;
    CHECK(rde_arr_length(&out) == 21u + 60u && o[0].x == 20 && o[0].y == 40 && o[20].x == 40 && o[21].x == 41 && o[80].x == 100);
    // Begun off it: nothing.
    CHECK(!fude_zoom_shape_sculpt(line, 101, off, 5, 2.0, &out) && rde_arr_length(&out) == 0u);
    rde_arr_free(&out);
}

// Cutting a board: what is left of a region less its cuts, as pieces.
typedef struct { u32 pieces; f64 area[8]; u32 points[8]; u32 rings[8]; } cut_result;

static cut_result cut_run(const fude_zoom_v2* region, const u32* rings, u32 n, const fude_zoom_v2* cut, const u32* cut_rings, u32 cut_n) {
    cut_result r; memset(&r, 0, sizeof r);
    rde_arr out = rde_arr_new(sizeof(fude_zoom_v2), NULL), out_rings = rde_arr_new(sizeof(u32), NULL), out_piece = rde_arr_new(sizeof(u32), NULL);
    r.pieces = fude_zoom_cut_region(region, rings, n, cut, cut_rings, cut_n, &out, &out_rings, &out_piece);
    const fude_zoom_v2* p = (const fude_zoom_v2*)out.memory;
    const u32* rg = (const u32*)out_rings.memory;
    const u32* pc = (const u32*)out_piece.memory;
    for(u32 from = 0; from < (u32)rde_arr_length(&out);) {
        u32 to = from;
        while(to < (u32)rde_arr_length(&out) && pc[to] == pc[from] && rg[to] == rg[from]) to++;
        if(pc[from] < 8u) {
            r.area[pc[from]] += fude_zoom_cut_area(&p[from], to - from);
            r.points[pc[from]] += to - from;
            r.rings[pc[from]]++;
        }
        from = to;
    }
    rde_arr_free(&out); rde_arr_free(&out_rings); rde_arr_free(&out_piece);
    return r;
}

static void test_cuts(void) {
    const fude_zoom_v2 board[4] = { { 0, 0 }, { 100, 0 }, { 100, 50 }, { 0, 50 } };
    rde_arr band = rde_arr_new(sizeof(fude_zoom_v2), NULL);
    // A saw's cut right across, 2 wide: two pieces, each 49 x 50.
    const fude_zoom_v2 across[2] = { { 50, -10 }, { 50, 60 } };
    CHECK(fude_zoom_cut_band(across, 2, 2.0, &band) == 4u);
    cut_result r = cut_run(board, NULL, 4, (const fude_zoom_v2*)band.memory, NULL, 4);
    CHECK(r.pieces == 2u && fabs(r.area[0] - 2450) < 1e-6 && fabs(r.area[1] - 2450) < 1e-6 && r.points[0] == 4u && r.points[1] == 4u);
    // Partway in (a slot): one piece, the slot out of it.
    rde_arr_clear(&band);
    const fude_zoom_v2 partway[2] = { { 50, -10 }, { 50, 25 } };
    fude_zoom_cut_band(partway, 2, 2.0, &band);
    r = cut_run(board, NULL, 4, (const fude_zoom_v2*)band.memory, NULL, 4);
    CHECK(r.pieces == 1u && fabs(r.area[0] - (5000 - 50)) < 1e-6 && r.points[0] == 8u);
    // A notch over a corner; and the same snapped exactly onto the corner (edges along edges).
    const fude_zoom_v2 notch[4] = { { 80, -10 }, { 120, -10 }, { 120, 20 }, { 80, 20 } };
    r = cut_run(board, NULL, 4, notch, NULL, 4);
    CHECK(r.pieces == 1u && fabs(r.area[0] - 4600) < 1e-6 && r.points[0] == 6u);
    const fude_zoom_v2 snapped[4] = { { 80, 0 }, { 100, 0 }, { 100, 20 }, { 80, 20 } };
    r = cut_run(board, NULL, 4, snapped, NULL, 4);
    CHECK(r.pieces == 1u && fabs(r.area[0] - 4600) < 1e-6 && r.points[0] == 6u);
    // A hole inside: one piece, its outline and a hole.
    const fude_zoom_v2 hole[4] = { { 40, 20 }, { 60, 20 }, { 60, 30 }, { 40, 30 } };
    r = cut_run(board, NULL, 4, hole, NULL, 4);
    CHECK(r.pieces == 1u && r.rings[0] == 2u && fabs(r.area[0] - (5000 - 200)) < 1e-6);
    // Along an edge, outside: nothing taken.
    const fude_zoom_v2 along[4] = { { -10, 50 }, { 110, 50 }, { 110, 60 }, { -10, 60 } };
    r = cut_run(board, NULL, 4, along, NULL, 4);
    CHECK(r.pieces == 1u && fabs(r.area[0] - 5000) < 1e-6 && r.points[0] == 4u);
    // Two cuts at once (a notch's two saw cuts): the corner falls off as a piece of its own.
    rde_arr_clear(&band);
    const fude_zoom_v2 down[2] = { { 80, -10 }, { 80, 21 } }, side[2] = { { 79, 20 }, { 110, 20 } };
    fude_zoom_cut_band(down, 2, 2.0, &band);
    fude_zoom_cut_band(side, 2, 2.0, &band);
    const u32 two_rings[8] = { 1, 1, 1, 1, 2, 2, 2, 2 };
    r = cut_run(board, NULL, 4, (const fude_zoom_v2*)band.memory, two_rings, 8);
    CHECK(r.pieces == 2u);
    const f64 big = fmax(r.area[0], r.area[1]), small = fmin(r.area[0], r.area[1]);
    if(!(fabs(small - 19 * 19) < 1e-6 && fabs(big - (5000 - 80 - 361)) < 1e-6)) printf("two cuts: %g %g\n", r.area[0], r.area[1]);
    CHECK(fabs(small - 19 * 19) < 1e-6 && fabs(big - (5000 - 80 - 361)) < 1e-6);   // (the bands took 42 + 42 - 4)
    // A cut through a piece with a hole: across the hole, two pieces, each with a bite out (no holes).
    const fude_zoom_v2 holed[8] = { { 0, 0 }, { 100, 0 }, { 100, 50 }, { 0, 50 }, { 40, 20 }, { 40, 30 }, { 60, 30 }, { 60, 20 } };
    const u32 holed_rings[8] = { 0, 0, 0, 0, 1, 1, 1, 1 };
    rde_arr_clear(&band);
    fude_zoom_cut_band(across, 2, 2.0, &band);
    r = cut_run(holed, holed_rings, 8, (const fude_zoom_v2*)band.memory, NULL, 4);
    CHECK(r.pieces == 2u && r.rings[0] == 1u && r.rings[1] == 1u && fabs(r.area[0] + r.area[1] - (5000 - 100 - 200 + 20)) < 1e-6);
    rde_arr_free(&band);

    // Round loops (a pen's): a hole inside, a notch over the edge.
    {
        fude_zoom_v2 ring[49];
        for(u32 i = 0; i <= 48u; i++) { const f64 a = 6.283185307179586 * i / 48.0; ring[i] = (fude_zoom_v2){ 30 + cos(a) * 10, 25 + sin(a) * 10 }; }
        r = cut_run(board, NULL, 4, ring, NULL, 49);
        CHECK(r.pieces == 1u && r.rings[0] == 2u && r.area[0] > 5000 - 320 && r.area[0] < 5000 - 300);
        for(u32 i = 0; i <= 48u; i++) { const f64 a = 6.283185307179586 * i / 48.0; ring[i] = (fude_zoom_v2){ 80 + cos(a) * 10, 45 + sin(a) * 10 }; }
        r = cut_run(board, NULL, 4, ring, NULL, 49);
        CHECK(r.pieces == 1u && r.rings[0] == 1u && fabs(r.area[0] - 4747.86) < 1.0);   // (the circle less its cap above the edge: about 252.8 taken)
    }
    // A loop drawn by hand over a board's edge (its points as the app had them): a notch, the board kept.
    {
        static const fude_zoom_v2 hand[] = { { 276.49389851396484, 78.99825671827567 }, { 275.81808108394546, 89.309596096632603 }, { 273.80208027666015, 99.444507633803013 }, { 270.48049128358764, 109.22958377516044 }, { 265.91014969652099, 118.49738696729911 }, { 260.16916668718261, 127.08934787428153 }, { 253.35584461992187, 134.85845950758232 }, { 245.58674633806152, 141.67179874098076 }, { 236.9947682649414, 147.4127617231585 }, { 227.72697651689452, 151.98311999952568 }, { 217.94187748735351, 155.3047066084124 }, { 207.80696308916015, 157.32068256056084 }, { 197.49563801591796, 157.99651721632256 }, { 187.18432057207031, 157.32068256056084 }, { 177.04940998857421, 155.3047066084124 }, { 167.26430714433593, 151.98313525831475 }, { 157.99651539628906, 147.41274646436943 }, { 149.40453541582031, 141.6717834821917 }, { 141.63543904130859, 134.85845950758232 }, { 134.82211506669921, 127.08934787428153 }, { 129.08113301103515, 118.49737170851004 }, { 124.51072895830077, 109.22956851637137 }, { 121.18921101396484, 99.444500004408482 }, { 119.17320454423827, 89.309611355421666 }, { 118.49737751787109, 78.998249184414917 }, { 119.17320454423827, 68.686909710524205 }, { 121.18921101396484, 58.55200198805106 }, { 124.51078999345702, 48.766914402601856 }, { 129.08113301103515, 39.499115025160449 }, { 134.82211506669921, 30.90716937696709 }, { 141.63543904130859, 23.138057743666309 }, { 149.40453541582031, 16.324729954359668 }, { 157.99651539628906, 10.583759342787403 }, { 167.26436817949218, 6.0133705488420901 }, { 177.04940998857421, 2.6917991987444339 }, { 187.18432057207031, 0.67583850538505885 }, { 197.49563801591796, -3.7797711911480292e-06 }, { 207.8070241243164, 0.6758461347795901 }, { 217.94187748735351, 2.6917991987444339 }, { 227.72697651689452, 6.0133934370256839 }, { 236.9947682649414, 10.583774601576465 }, { 245.58674633806152, 16.324729954359668 }, { 253.35584461992187, 23.138073002455371 }, { 260.16916668718261, 30.90716937696709 }, { 265.91014969652099, 39.499145542738574 }, { 270.48049128358764, 48.766933476088184 }, { 273.80208027666015, 58.551982914564732 }, { 275.81808108394546, 68.686947857496861 }, { 276.49389851396484, 78.998271785997176 } };
        const fude_zoom_v2 wide[4] = { { -400, -150 }, { 400, -150 }, { 400, 150 }, { -400, 150 } };
        r = cut_run(wide, NULL, 4, hand, NULL, (u32)(sizeof(hand) / sizeof(hand[0])));   // (its last point a hair past its first: it crosses itself there)
        CHECK(r.pieces == 1u && fabs(r.area[0] - 220815) < 1.0 && r.points[0] == 47u);
        // A cut crossing itself (a figure 8): both its lobes out.
        const fude_zoom_v2 bowtie[4] = { { 100, 100 }, { 200, 200 }, { 200, 100 }, { 100, 200 } };
        r = cut_run(wide, NULL, 4, bowtie, NULL, 4);
        CHECK(r.pieces == 1u && fabs(r.area[0] - 237500) < 1e-6);
    }
    // A board sawn across (kerf 3): two plain boards, its name on both, in its place and order; the cut list counts
    // them as one part twice.
    fude_zoom_scene sc; fude_zoom_scene_init(&sc, 7);
    const rde_color ink = { 1, 1, 1, 255 };
    f64 bn[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 bc = fude_zoom_board_numbers(bn, 300, 150, 18, (u8)(FUDE_ZOOM_MATERIAL_OAK << 1), "Side");
    const i8  dashed_hatched = fude_zoom_q_of(FUDE_ZOOM_LINE_DASHED, FUDE_ZOOM_HATCH_DIAGONAL);   // (drawn dashed and hatched: its pieces are too)
    sc.style = dashed_hatched;
    const u32 bd = fude_zoom_scene_add_shape_fill(&sc, sc.root, (fude_zoom_place){ { 1000, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_BOARD, bn, bc, ink, 0.5f,
                                                  FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN, (rde_color){ 226, 202, 160, 255 }, 0);
    sc.style = 0;
    const u64 bz = fude_zoom_scene_object(&sc, bd)->z;
    rde_arr saw = rde_arr_new(sizeof(fude_zoom_v2), NULL), born = rde_arr_new(sizeof(u32), NULL);
    const fude_zoom_v2 cut_line[2] = { { 1000, -400 }, { 1000, 400 } };
    fude_zoom_cut_band(cut_line, 2, 3.0, &saw);
    CHECK(fude_zoom_cut_board(&sc, bd, (const fude_zoom_v2*)saw.memory, NULL, (u32)rde_arr_length(&saw), &born) == 2u);
    fude_zoom_scene_set_alive(&sc, bd, false);
    fude_zoom_history_push(&sc, sc.root, (fude_zoom_box){ 0, 0, 1, 1 }, &bd, 1, (const u32*)born.memory, 2);
    for(u32 i = 0; i < 2u; i++) {
        const u32 piece = ((const u32*)born.memory)[i];
        f64 pn[FUDE_ZOOM_SHAPE_NUMBERS];
        const u32 pc = fude_zoom_scene_shape_numbers(&sc, piece, pn, FUDE_ZOOM_SHAPE_NUMBERS);
        c8 nm[FUDE_ZOOM_BOARD_NAME];
        fude_zoom_board_name(pn, pc, nm, sizeof nm);
        CHECK(!fude_zoom_board_is_cut(pn, pc) && fabs(pn[0] - 149.25) < 1e-9 && fabs(pn[1] - 150) < 1e-9 && pn[2] == 18 && strcmp(nm, "Side") == 0);   // ((600 - 3) / 2 long)
        CHECK(fude_zoom_board_material(pn, pc) == FUDE_ZOOM_MATERIAL_OAK);   // (each piece still oak)
        CHECK(fude_zoom_scene_object(&sc, piece)->z == bz && fabs(fabs(fude_zoom_scene_place_of(&sc, piece).t.x - 1000) - 150.75) < 1e-9);
        CHECK(fude_zoom_scene_object(&sc, piece)->q == dashed_hatched && sc.style == 0);   // (its line and hatch kept; the scene's style put back)
    }
    const c8* const ch[7] = { "Part", "Qty", "L", "W", "T", "Material", "Grain" };
    const c8* const cg[2] = { "along", "across" };
    const c8* const cm[FUDE_ZOOM_MATERIAL_COUNT] = { "", "Pine", "Oak", "Walnut", "Maple", "Beech", "Cherry", "Birch plywood", "MDF", "Melamine" };
    fude_bytes list = fude_bytes_new(64);
    CHECK(fude_zoom_export_cutlist(&sc, 1.0, ch, cg, cm, &list) == 2u);
    rde_arr_add(&list, &(u8){ 0 });
    CHECK(strstr((const c8*)list.memory, "Side,2,300.0,298.5,18.0,Oak,across") != NULL);
    rde_arr_free(&list);
    // A notch out of one: a cut board, its outline its own (six corners).
    const u32 left = ((const u32*)born.memory)[0];
    const fude_zoom_v2 notch2[4] = { { 600, 100 }, { 760, 100 }, { 760, 200 }, { 600, 200 } };
    rde_arr_clear(&born);
    CHECK(fude_zoom_cut_board(&sc, left, notch2, NULL, 4, &born) == 1u);
    const u32 notched = ((const u32*)born.memory)[0];
    rde_arr all = rde_arr_new(sizeof(f64), NULL);
    const u32 an = fude_zoom_scene_shape_numbers_all(&sc, notched, &all);
    CHECK(fude_zoom_board_is_cut((const f64*)all.memory, an));
    CHECK(fude_zoom_board_material((const f64*)all.memory, an) == FUDE_ZOOM_MATERIAL_OAK);   // (its look passed whole: still oak)
    CHECK(fude_zoom_scene_object(&sc, notched)->q == dashed_hatched);   // (a notch: still dashed and hatched)
    rde_arr outline = rde_arr_new(sizeof(fude_zoom_v2), NULL);
    b8 oc = false;
    fude_zoom_scene_shape_outline(&sc, notched, 64, &outline, &oc);
    CHECK(oc && rde_arr_length(&outline) == 6u);
    rde_arr_free(&outline); rde_arr_free(&all);
    // The eraser across a board: two pieces; undone, the board back.
    fude_zoom_scene_set_alive(&sc, left, false);
    fude_zoom_history_push(&sc, sc.root, (fude_zoom_box){ 0, 0, 1, 1 }, &left, 1, &notched, 1);
    sc.style = fude_zoom_q_of(FUDE_ZOOM_LINE_DASH_DOT, FUDE_ZOOM_HATCH_CROSS);
    const u32 other = fude_zoom_scene_add_shape_fill(&sc, sc.root, (fude_zoom_place){ { 3000, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_BOARD, bn, bc, ink, 0.5f, FUDE_ZOOM_FLAG_FILLED, ink, 0);
    sc.style = 0;
    fude_zoom_history_push(&sc, sc.root, (fude_zoom_box){ 0, 0, 1, 1 }, NULL, 0, &other, 1);
    const u32 before = fude_zoom_scene_object_count(&sc);
    fude_zoom_eraser er; fude_zoom_eraser_init(&er);
    fude_zoom_erase_begin(&er, FUDE_ZOOM_ERASE_PARTIAL);
    CHECK(fude_zoom_erase_step(&sc, &er, sc.root, (fude_zoom_v2){ 3000, -300 }, (fude_zoom_v2){ 3000, 300 }, 2.0, 0.5) >= 1u);
    fude_zoom_erase_end(&sc, &er, sc.root, (fude_zoom_box){ 2600, -300, 3400, 300 });
    u32 boards_after = 0, styled_after = 0;
    for(u32 i = before; i < fude_zoom_scene_object_count(&sc); i++) {
        const fude_zoom_object* o = fude_zoom_scene_object(&sc, i);
        const b8 board = (o->flags & FUDE_ZOOM_FLAG_ALIVE) && o->kind == FUDE_ZOOM_KIND_SHAPE && o->channels == FUDE_ZOOM_SHAPE_BOARD;
        boards_after += board ? 1u : 0u;
        styled_after += board && o->q == fude_zoom_q_of(FUDE_ZOOM_LINE_DASH_DOT, FUDE_ZOOM_HATCH_CROSS) ? 1u : 0u;
    }
    CHECK(!alive(&sc, other) && boards_after == 2u);
    CHECK(styled_after == 2u);   // (rubbed in two, each piece still in its centre line and cross-hatched)
    CHECK(fude_zoom_history_undo(&sc) && alive(&sc, other));
    fude_zoom_eraser_destroy(&er);
    rde_arr_free(&saw); rde_arr_free(&born);
    fude_zoom_scene_destroy(&sc);
}

// Snapping onto where two things cross (a compass's constructions): ends and corners first.
static void test_snap_cross(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const rde_color c = { 1, 1, 1, 255 };
    const f64 diag[2] = { 100, 100 }, back[2] = { 100, -100 };
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, diag, 2, c, 1.0f, 0u, 0);
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 100 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, back, 2, c, 1.0f, 0u, 0);
    line(&s, 30, 0, 30, 100, 1.0, 1.0f);   // a stroke up through x 30 (crossing the first at (30, 30))
    fude_zoom_visible v; memset(&v, 0, sizeof v);
    v.frame = s.root; v.to_screen = (fude_zoom_sim){ 1.0, 0.0, 0.0, 0.0 };
    fude_zoom_snap h = fude_zoom_snap_find(&s, &v, 1, (fude_zoom_v2){ 53, 48 }, 14.0);
    CHECK(h.kind == FUDE_ZOOM_SNAP_CROSS && fabs(h.at.x - 50) < 1e-9 && fabs(h.at.y - 50) < 1e-9);
    h = fude_zoom_snap_find(&s, &v, 1, (fude_zoom_v2){ 33, 27 }, 14.0);
    CHECK(h.kind == FUDE_ZOOM_SNAP_CROSS && fabs(h.at.x - 30) < 1e-6 && fabs(h.at.y - 30) < 1e-6);
    h = fude_zoom_snap_find(&s, &v, 1, (fude_zoom_v2){ 4, 3 }, 14.0);   // the lines' and the stroke's ends there: an end wins
    CHECK(h.kind == FUDE_ZOOM_SNAP_END);
    fude_zoom_scene_destroy(&s);
}

// Repeat (select.h's put): copies moved, all in one undo step, the lasso on them too; mirrored — a polygon's corners
// and a stroke's points turned over across the line, a text kept the right way round in its mirrored place, the
// originals let go in the same step (a flip); and what the lasso holds moved without the pen (begin, end).
static void test_repeat(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    s.camera = (fude_zoom_camera){ s.root, { 0, 0 }, 1.0 };
    const f64 tri[6] = { 0, 0, 10, 0, 0, 20 };
    const u32 p = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_POLYGON, tri, 6u, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    const u32 l = line(&s, 0, 40, 10, 45, 1.0, 1.0f);
    const u32 t = fude_zoom_scene_add_text(&s, s.root, (fude_zoom_place){ { 0, 80 }, 0.0, 1.0 }, 9.0, 30.0, 12.0, FUDE_ZOOM_TEXT_PLAIN,
                                           (rde_color){ 1, 1, 1, 255 }, (rde_color){ 0, 0, 0, 0 }, "Hi", 2u, 0);
    fude_zoom_selection sel; fude_zoom_select_init(&sel);
    const u32 objs[3] = { p, l, t };
    for(u32 i = 0; i < 3u; i++) {
        rde_arr_add(&sel.picks, &(fude_zoom_pick){ objs[i] });
    }
    fude_zoom_select_update(&sel, &s);
    rde_arr clips = rde_arr_new(sizeof(fude_zoom_clip), rde_memory_allocator_get_default_std());
    fude_zoom_select_clips_of(&s, objs, 3u, &clips);
    CHECK(rde_arr_length(&clips) == 3u);
    // Two copies 100 and 200 across: six things, one step, all held.
    const u32 before = fude_zoom_scene_object_count(&s);
    const fude_zoom_sim moves[2] = { { 1, 0, 100, 0 }, { 1, 0, 200, 0 } };
    CHECK(fude_zoom_select_put(&sel, &s, (const fude_zoom_clip*)clips.memory, 3u, moves, 2u, NULL, NULL, 0u) == 6u);
    CHECK(rde_arr_length(&sel.picks) == 9u && fude_zoom_scene_object_count(&s) == before + 6u);
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    b8 closed;
    fude_zoom_scene_shape_outline(&s, before + 3u, 1u, &pts, &closed);   // (the second copy's polygon)
    CHECK(rde_arr_length(&pts) == 3u && fabs(((const fude_zoom_v2*)pts.memory)[1].x - 210.0) < 1e-9);
    CHECK(fude_zoom_history_undo(&s) && !alive(&s, before) && !alive(&s, before + 5u) && alive(&s, p));
    // Mirrored across the upright line x = 50, the originals let go: the polygon's corners there turned over...
    fude_zoom_select_clear(&sel);
    for(u32 i = 0; i < 3u; i++) {
        rde_arr_add(&sel.picks, &(fude_zoom_pick){ objs[i] });
    }
    const fude_zoom_select_mirror m = { { 50, 0 }, 3.14159265358979323846 * 0.5 };
    const u32 base = fude_zoom_scene_object_count(&s);
    CHECK(fude_zoom_select_put(&sel, &s, (const fude_zoom_clip*)clips.memory, 3u, NULL, 0u, &m, objs, 3u) == 3u);
    CHECK(!alive(&s, p) && !alive(&s, l) && !alive(&s, t) && rde_arr_length(&sel.picks) == 3u);
    rde_arr_clear(&pts);
    fude_zoom_scene_shape_outline(&s, base, 1u, &pts, &closed);
    const fude_zoom_v2* q = (const fude_zoom_v2*)pts.memory;
    CHECK(rde_arr_length(&pts) == 3u && fabs(q[0].x - 100.0) < 1e-9 && fabs(q[1].x - 90.0) < 1e-9 && fabs(q[2].x - 100.0) < 1e-9 && fabs(q[2].y - 20.0) < 1e-9);
    // ...the stroke's points (from 100, 40 back to 90, 45)...
    const fude_zoom_object* so = fude_zoom_scene_object(&s, base + 1u);
    rde_arr sqa; fude_zoom_qpoint* sq = table(&sqa, sizeof(*sq), so->count);
    CHECK(fude_zoom_scene_points(&s, base + 1u, sq));
    const fude_zoom_v2 s0 = fude_zoom_scene_point_at(so, &sq[0]), s1 = fude_zoom_scene_point_at(so, &sq[so->count - 1u]);
    CHECK(fabs(s0.x - 100.0) < 0.1 && fabs(s0.y - 40.0) < 0.1 && fabs(s1.x - 90.0) < 0.1 && fabs(s1.y - 45.0) < 0.1);
    rde_arr_free(&sqa);
    // ...the text upright, its box where the mirror puts it (its top left the mirror of its top right).
    const fude_zoom_object* to = fude_zoom_scene_object(&s, base + 2u);
    CHECK(to->kind == FUDE_ZOOM_KIND_TEXT && fabs(fude_zoom_select_wrap_test(to->rotation)) < 1e-9 && fabs(to->t.x - 70.0) < 1e-9 && fabs(to->t.y - 80.0) < 1e-9);
    // One step: undone, the originals are back and the mirrored ones gone.
    CHECK(fude_zoom_history_undo(&s) && alive(&s, p) && alive(&s, l) && alive(&s, t) && !alive(&s, base));
    // Moved without the pen: one step, as a drag would.
    fude_zoom_select_clear(&sel);
    rde_arr_add(&sel.picks, &(fude_zoom_pick){ p });
    fude_zoom_select_update(&sel, &s);
    fude_zoom_select_begin(&sel, &s);
    fude_zoom_select_end(&sel, &s, (fude_zoom_sim){ 0.0, 2.0, 5.0, 0.0 });   // (doubled, a quarter turn, 5 across)
    const fude_zoom_place pp = fude_zoom_scene_place_of(&s, p);
    CHECK(fabs(pp.scale - 2.0) < 1e-12 && fabs(pp.rotation - 3.14159265358979323846 * 0.5) < 1e-12 && fabs(pp.t.x - 5.0) < 1e-9 && sel.grab == FUDE_ZOOM_GRAB_NONE);
    CHECK(fude_zoom_history_undo(&s) && fabs(fude_zoom_scene_place_of(&s, p).scale - 1.0) < 1e-12);
    rde_arr_free(&pts);
    fude_zoom_select_clips_free(&clips);
    rde_arr_free(&clips);
    fude_zoom_select_destroy(&sel);
    fude_zoom_scene_destroy(&s);
}

// The lasso's copies in the order things are drawn, across frames too, whatever order it picked them in (paste,
// Duplicate and Repeat could put an overlapping thing on top that was under); a drag and Arrange taking along the
// dimensions of every frame the picks are in (a drag followed in the first 8 frames only, Arrange in the camera's).
static void test_select_frames(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    s.camera = (fude_zoom_camera){ s.root, { 0, 0 }, 1.0 };
    const f64 box[3] = { 20, 10, 0 };
    const rde_color ink = { 1, 1, 1, 255 };
    // Drawn in this order: a, then a frame (c inside it), then b over both.
    const u32 a = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, box, 3u, ink, 1.0f, 0u, 0);
    const u32 f = fude_zoom_scene_new_frame(&s, s.root, (fude_zoom_xform){ 10, 0, 1.0, 0.0 }, (fude_zoom_box){ -50, -50, 50, 50 });
    const u32 c = fude_zoom_scene_add_shape(&s, f, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, (const f64[2]){ 10, 10 }, 2u, ink, 1.0f, 0u, 0);
    const u32 b = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 5, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, box, 3u, ink, 1.0f, 0u, 0);
    fude_zoom_selection sel; fude_zoom_select_init(&sel);
    const u32 picked[3] = { b, c, a };   // (as a loop may pick them: not as they are drawn)
    for(u32 i = 0; i < 3u; i++) {
        rde_arr_add(&sel.picks, &(fude_zoom_pick){ picked[i] });
    }
    fude_zoom_select_update(&sel, &s);
    fude_zoom_select_copy(&sel, &s);
    const fude_zoom_clip* cl = (const fude_zoom_clip*)sel.clip.memory;
    CHECK(rde_arr_length(&sel.clip) == 3u && cl[0].look.id == fude_zoom_scene_object(&s, a)->id && cl[1].look.id == fude_zoom_scene_object(&s, c)->id &&
          cl[2].look.id == fude_zoom_scene_object(&s, b)->id);
    // Pasted: the copies made in that order, so the copy of b is over the copy of a.
    const u32 base = fude_zoom_scene_object_count(&s);
    fude_zoom_select_paste(&sel, &s, (rde_vec_2F){ 0.0f, 300.0f });
    CHECK(fude_zoom_scene_object_count(&s) == base + 3u && fude_zoom_scene_object(&s, base)->channels == FUDE_ZOOM_SHAPE_RECT &&
          fude_zoom_scene_object(&s, base + 1u)->channels == FUDE_ZOOM_SHAPE_ELLIPSE && fude_zoom_scene_object(&s, base + 2u)->channels == FUDE_ZOOM_SHAPE_RECT);
    CHECK(fude_zoom_scene_object(&s, base)->z < fude_zoom_scene_object(&s, base + 2u)->z);
    // Duplicate: one copy of what is held now (a alone), and the clipboard as it was (the three copied above, still
    // there to paste: Duplicate once replaced it).
    fude_zoom_select_clear(&sel);
    rde_arr_add(&sel.picks, &(fude_zoom_pick){ a });
    fude_zoom_select_update(&sel, &s);
    const u32 before_dup = fude_zoom_scene_object_count(&s);
    fude_zoom_select_duplicate(&sel, &s);
    CHECK(fude_zoom_scene_object_count(&s) == before_dup + 1u && fude_zoom_scene_object(&s, before_dup)->channels == FUDE_ZOOM_SHAPE_RECT);
    CHECK(rde_arr_length(&sel.picks) == 1u && ((const fude_zoom_pick*)sel.picks.memory)[0].object == before_dup);
    cl = (const fude_zoom_clip*)sel.clip.memory;
    CHECK(rde_arr_length(&sel.clip) == 3u && cl[0].look.id == fude_zoom_scene_object(&s, a)->id && cl[2].look.id == fude_zoom_scene_object(&s, b)->id);
    // Nine frames side by side, a rectangle in each with a dimension on its two lower corners: all nine moved 30 across
    // at once, every dimension goes along.
    fude_zoom_select_clear(&sel);
    u32 rects[9], dims[9];
    for(u32 k = 0; k < 9u; k++) {
        const u32 fk = fude_zoom_scene_new_frame(&s, s.root, (fude_zoom_xform){ 200.0 + 100.0 * k, 300, 1.0, 0.0 }, (fude_zoom_box){ -40, -40, 40, 40 });
        rects[k] = fude_zoom_scene_add_shape(&s, fk, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, box, 3u, ink, 1.0f, 0u, 0);
        dims[k]  = fude_zoom_connect_dimension(&s, fk, (fude_zoom_v2){ -20, -10 }, (fude_zoom_v2){ 20, -10 }, -5.0, rects[k], 0, rects[k], 1, ink, 0.5f);
        rde_arr_add(&sel.picks, &(fude_zoom_pick){ rects[k] });
    }
    fude_zoom_select_update(&sel, &s);
    fude_zoom_select_begin(&sel, &s);
    fude_zoom_select_end(&sel, &s, (fude_zoom_sim){ 1.0, 0.0, 30.0, 0.0 });
    u32 followed = 0;
    for(u32 k = 0; k < 9u; k++) {
        fude_zoom_v2 corner = { 0, 0 };
        const fude_zoom_v2 start = fude_zoom_sim_apply(fude_zoom_object_sim(fude_zoom_scene_object(&s, dims[k])), (fude_zoom_v2){ 0, 0 });
        followed += fude_zoom_connect_point(&s, rects[k], 0, &corner) && fabs(corner.x - 10.0) < 1e-9 && fabs(start.x - corner.x) < 1e-9 &&
                    fabs(start.y - corner.y) < 1e-9 ? 1u : 0u;
    }
    CHECK(followed == 9u);
    // Arranged in a frame that is not the camera's: the dimension on what moved goes along there too.
    fude_zoom_select_clear(&sel);
    const u32 g  = fude_zoom_scene_new_frame(&s, s.root, (fude_zoom_xform){ 0, -400, 1.0, 0.0 }, (fude_zoom_box){ -200, -200, 200, 200 });
    const u32 r1 = fude_zoom_scene_add_shape(&s, g, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, box, 3u, ink, 1.0f, 0u, 0);
    const u32 r2 = fude_zoom_scene_add_shape(&s, g, (fude_zoom_place){ { 100, 50 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, box, 3u, ink, 1.0f, 0u, 0);
    const u32 d2 = fude_zoom_connect_dimension(&s, g, (fude_zoom_v2){ 80, 40 }, (fude_zoom_v2){ 120, 40 }, -5.0, r2, 0, r2, 1, ink, 0.5f);
    rde_arr_add(&sel.picks, &(fude_zoom_pick){ r1 });
    rde_arr_add(&sel.picks, &(fude_zoom_pick){ r2 });
    fude_zoom_select_update(&sel, &s);
    CHECK(fude_zoom_select_arrange(&sel, &s, FUDE_ZOOM_ARRANGE_LEFT));
    fude_zoom_v2 lc = { 0, 0 };
    const fude_zoom_v2 ds = fude_zoom_sim_apply(fude_zoom_object_sim(fude_zoom_scene_object(&s, d2)), (fude_zoom_v2){ 0, 0 });
    CHECK(fude_zoom_connect_point(&s, r2, 0, &lc) && fabs(lc.x + 20.0) < 1e-9 && fabs(ds.x - lc.x) < 1e-9 && fabs(ds.y - lc.y) < 1e-9);
    fude_zoom_select_destroy(&sel);
    fude_zoom_scene_destroy(&s);
}

// Dimensions that follow (connect.h): drawn between two corners of a rectangle, a dimension keeps them — moved with
// it (the lasso's move), it measures where they are; made again (stretched), it is measured again in the same step;
// copied with what it measures, the copy is joined to the copies. A circle's size follows its circle.
static void test_dims(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    s.camera = (fude_zoom_camera){ s.root, { 0, 0 }, 1.0 };
    const f64 box[3] = { 50, 20, 0 };
    const u32 r = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, box, 3u, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    // Its corners (one segment's outline): bottom left 0, bottom right 1; its centre.
    fude_zoom_v2 p;
    CHECK(fude_zoom_connect_point(&s, r, 0, &p) && p.x == -50.0 && p.y == -20.0);
    CHECK(fude_zoom_connect_point(&s, r, 1, &p) && p.x == 50.0 && p.y == -20.0);
    CHECK(fude_zoom_connect_point(&s, r, FUDE_ZOOM_SNAP_CENTRE_KEY, &p) && fabs(p.x) < 1e-12 && fabs(p.y) < 1e-12);
    CHECK(!fude_zoom_connect_point(&s, r, 9, &p) && !fude_zoom_connect_point(&s, r, FUDE_ZOOM_SNAP_NO_KEY, &p));
    const u32 d = fude_zoom_connect_dimension(&s, s.root, (fude_zoom_v2){ -50, -20 }, (fude_zoom_v2){ 50, -20 }, -15.0, r, 0, r, 1, (rde_color){ 1, 1, 1, 255 }, 0.5f);
    f64 n[FUDE_ZOOM_SHAPE_NUMBERS];
    CHECK(fude_zoom_scene_shape_numbers(&s, d, n, FUDE_ZOOM_SHAPE_NUMBERS) == FUDE_ZOOM_DIMENSION_REFS && n[0] == 100.0 && n[2] == -15.0);
    // A dimension not on anything: its three numbers only.
    const u32 loose = fude_zoom_connect_dimension(&s, s.root, (fude_zoom_v2){ 0, 100 }, (fude_zoom_v2){ 10, 100 }, 5.0, FUDE_ZOOM_NONE, 0, FUDE_ZOOM_NONE, 0, (rde_color){ 1, 1, 1, 255 }, 0.5f);
    CHECK(fude_zoom_scene_shape_numbers(&s, loose, n, FUDE_ZOOM_SHAPE_NUMBERS) == 3u);
    // The rectangle moved 30 across and turned a quarter (the lasso's): the dimension's ends on its corners, one step.
    fude_zoom_selection sel; fude_zoom_select_init(&sel);
    rde_arr_add(&sel.picks, &(fude_zoom_pick){ r });
    fude_zoom_select_update(&sel, &s);
    fude_zoom_select_begin(&sel, &s);
    fude_zoom_select_end(&sel, &s, (fude_zoom_sim){ 0.0, 1.0, 30.0, 0.0 });
    const fude_zoom_object* od = fude_zoom_scene_object(&s, d);
    CHECK(fude_zoom_scene_shape_numbers(&s, d, n, FUDE_ZOOM_SHAPE_NUMBERS) == FUDE_ZOOM_DIMENSION_REFS);
    const fude_zoom_v2 a = fude_zoom_sim_apply(fude_zoom_object_sim(od), (fude_zoom_v2){ 0, 0 }), b = fude_zoom_sim_apply(fude_zoom_object_sim(od), (fude_zoom_v2){ n[0], n[1] });
    fude_zoom_v2 c0, c1;
    CHECK(fude_zoom_connect_point(&s, r, 0, &c0) && fude_zoom_connect_point(&s, r, 1, &c1));
    CHECK(fabs(a.x - c0.x) < 1e-9 && fabs(a.y - c0.y) < 1e-9 && fabs(b.x - c1.x) < 1e-9 && fabs(b.y - c1.y) < 1e-9);
    CHECK(fabs(c0.x - 50.0) < 1e-9 && fabs(c0.y + 50.0) < 1e-9);   // (bottom left turned: (20, -50) + 30 across)
    CHECK(fude_zoom_history_undo(&s));
    od = fude_zoom_scene_object(&s, d);
    CHECK(fabs(od->t.x + 50.0) < 1e-9 && fabs(od->rotation) < 1e-12);
    // Made again (stretched to 70 a side): measured again, its offset as it was.
    const u32 r2 = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 20, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, (const f64[3]){ 70, 20, 0 }, 3u, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    fude_zoom_scene_set_alive(&s, r, false);
    const fude_zoom_id oid = fude_zoom_scene_object(&s, r)->id, nid = fude_zoom_scene_object(&s, r2)->id;
    const u32 d2 = fude_zoom_connect_dim_remap(&s, d, &oid, &nid, 1u);
    CHECK(d2 != FUDE_ZOOM_NONE && fude_zoom_connect_dim_remap(&s, loose, &oid, &nid, 1u) == FUDE_ZOOM_NONE);
    CHECK(fude_zoom_scene_shape_numbers(&s, d2, n, FUDE_ZOOM_SHAPE_NUMBERS) == FUDE_ZOOM_DIMENSION_REFS && fabs(n[0] - 140.0) < 1e-9 && n[2] == -15.0);
    CHECK(fabs(fude_zoom_scene_object(&s, d2)->t.x + 50.0) < 1e-9);
    // Copied with it (put): the copy joined to the copy; copied without it: joined to nothing.
    rde_arr clips = rde_arr_new(sizeof(fude_zoom_clip), rde_memory_allocator_get_default_std());
    const u32 both[2] = { d2, r2 };
    fude_zoom_select_clips_of(&s, both, 2u, &clips);
    fude_zoom_select_clear(&sel);
    const fude_zoom_sim up = { 1, 0, 0, 200 };
    const u32 base = fude_zoom_scene_object_count(&s);
    CHECK(fude_zoom_select_put(&sel, &s, (const fude_zoom_clip*)clips.memory, 2u, &up, 1u, NULL, NULL, 0u) == 2u);
    // (the rectangle made first, then the dimension joined to it)
    CHECK(fude_zoom_scene_object(&s, base)->channels == FUDE_ZOOM_SHAPE_RECT && fude_zoom_scene_object(&s, base + 1u)->channels == FUDE_ZOOM_SHAPE_DIMENSION);
    fude_zoom_scene_shape_numbers(&s, base + 1u, n, FUDE_ZOOM_SHAPE_NUMBERS);
    fude_zoom_id joined = fude_zoom_id_get(&n[4]);   // (its from's id, in two halves: shape.h)
    CHECK(joined == fude_zoom_scene_object(&s, base)->id);
    fude_zoom_select_clips_free(&clips);
    fude_zoom_select_clips_of(&s, &d2, 1u, &clips);
    fude_zoom_select_put(&sel, &s, (const fude_zoom_clip*)clips.memory, 1u, &up, 1u, NULL, NULL, 0u);
    fude_zoom_scene_shape_numbers(&s, base + 2u, n, FUDE_ZOOM_SHAPE_NUMBERS);
    joined = fude_zoom_id_get(&n[4]);
    CHECK(joined == 0u);
    fude_zoom_select_clips_free(&clips);
    rde_arr_free(&clips);
    // A circle's size: its circle moved and grown, it goes along (its centre, its radius).
    const u32 ci = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 500, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, (const f64[2]){ 20, 20 }, 2u, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    f64 rn[4] = { 20, 0.0, 1.0, 0.0 };
    const fude_zoom_id cid = fude_zoom_scene_object(&s, ci)->id;
    memcpy(&rn[3], &cid, sizeof(cid));
    const u32 ra = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 500, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RADIAL, rn, 4u, (rde_color){ 1, 1, 1, 255 }, 0.5f, 0u, 0);
    fude_zoom_select_clear(&sel);
    rde_arr_add(&sel.picks, &(fude_zoom_pick){ ci });
    fude_zoom_select_update(&sel, &s);
    fude_zoom_select_begin(&sel, &s);
    fude_zoom_select_end(&sel, &s, (fude_zoom_sim){ 2.0, 0.0, -490.0, 10.0 });   // (twice as big round the origin: centre to (510, 10))
    const fude_zoom_object* oa = fude_zoom_scene_object(&s, ra);
    CHECK(fabs(oa->t.x - 510.0) < 1e-9 && fabs(oa->t.y - 10.0) < 1e-9 && fabs(oa->scale - 2.0) < 1e-12);
    fude_zoom_v2 cc; f64 cr;
    CHECK(fude_zoom_connect_round(&s, ci, &cc, &cr) && fabs(cr - 40.0) < 1e-12);
    fude_zoom_select_destroy(&sel);
    fude_zoom_scene_destroy(&s);
}

// 3D (stl.h): rings inside rings take turns — a plate with a hole, a peg in the hole — each solid as tall as asked,
// closed (every edge two triangles', once each way) and facing out (its volume the plate's less the hole's, and
// the peg's); from the view: a rectangle and a board solids (the board its thickness), a line and a symbol not.
static f64 stl_volume(const f32* t, u32 n) {
    f64 v = 0.0;
    for(u32 i = 0; i < n; i++, t += 9) {
        v += ((f64)t[0] * ((f64)t[4] * t[8] - (f64)t[5] * t[7]) - (f64)t[1] * ((f64)t[3] * t[8] - (f64)t[5] * t[6]) + (f64)t[2] * ((f64)t[3] * t[7] - (f64)t[4] * t[6])) / 6.0;
    }
    return v;
}

static b8 stl_closed(const f32* t, u32 n) {
    // Each edge's way round must be met once the other way (a closed surface, facing one way).
    for(u32 i = 0; i < n; i++) {
        for(u32 e = 0; e < 3u; e++) {
            const f32* a = &t[i * 9u + e * 3u];
            const f32* b = &t[i * 9u + ((e + 1u) % 3u) * 3u];
            u32 back = 0;
            for(u32 j = 0; j < n; j++) {
                for(u32 f = 0; f < 3u; f++) {
                    const f32* c = &t[j * 9u + f * 3u];
                    const f32* d = &t[j * 9u + ((f + 1u) % 3u) * 3u];
                    back += (memcmp(c, b, 12u) == 0 && memcmp(d, a, 12u) == 0) ? 1u : 0u;
                }
            }
            if(back != 1u) {
                return false;
            }
        }
    }
    return true;
}

static void test_stl(void) {
    // A plate (counter-clockwise), a hole in it (clockwise as given: turned as needed), a peg in the hole.
    const fude_zoom_v2 pts[12] = {
        { 0, 0 }, { 100, 0 }, { 100, 100 }, { 0, 100 },
        { 20, 20 }, { 20, 40 }, { 40, 40 }, { 40, 20 },
        { 25, 25 }, { 35, 25 }, { 35, 35 }, { 25, 35 },
    };
    const u32 rings[12] = { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2 };
    const f64 heights[3] = { 5.0, 5.0, 8.0 };
    rde_arr tris = rde_arr_new(sizeof(f32), rde_memory_allocator_get_default_std());
    u32 solids = 0;
    const u32 n = fude_zoom_stl_solids(pts, rings, 12u, heights, &tris, &solids);
    CHECK(solids == 2u && n > 0u);
    CHECK(fabs(stl_volume((const f32*)tris.memory, n) - ((100.0 * 100.0 - 20.0 * 20.0) * 5.0 + 10.0 * 10.0 * 8.0)) < 1e-3);
    CHECK(stl_closed((const f32*)tris.memory, n));
    // A round plate (64 corners) with two round holes (32 each, either way round): closed, its volume the polygons'.
    fude_zoom_v2 disc[128];
    u32 dr[128];
    f64 want = 0.0;
    for(u32 i = 0; i < 64u; i++) {
        disc[i] = (fude_zoom_v2){ 50.0 * cos(i * 6.283185307179586 / 64.0), 50.0 * sin(i * 6.283185307179586 / 64.0) };
        dr[i] = 0u;
    }
    for(u32 h = 0; h < 2u; h++) {
        for(u32 i = 0; i < 32u; i++) {
            const f64 a = (h == 0u ? 1.0 : -1.0) * i * 6.283185307179586 / 32.0;
            disc[64u + 32u * h + i] = (fude_zoom_v2){ (h == 0u ? -20.0 : 20.0) + 10.0 * cos(a), 10.0 * sin(a) };
            dr[64u + 32u * h + i] = 1u + h;
        }
    }
    for(u32 ring = 0; ring < 3u; ring++) {
        const u32 first = ring == 0u ? 0u : 64u + 32u * (ring - 1u), count = ring == 0u ? 64u : 32u;
        f64 area = 0.0;
        for(u32 i = 0; i < count; i++) {
            const fude_zoom_v2 u = disc[first + i], v = disc[first + (i + 1u) % count];
            area += (u.x * v.y - v.x * u.y) * 0.5;
        }
        want += ring == 0u ? fabs(area) : -fabs(area);
    }
    rde_arr_clear(&tris);
    const f64 dh[3] = { 4.0, 4.0, 4.0 };
    const u32 dn = fude_zoom_stl_solids(disc, dr, 128u, dh, &tris, &solids);
    CHECK(solids == 1u && fabs(stl_volume((const f32*)tris.memory, dn) - want * 4.0) < 1e-2 && stl_closed((const f32*)tris.memory, dn));
    rde_arr_free(&tris);
    // From the view: a rectangle (3 mm), a board 18 mm thick, a line and a symbol left out.
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    s.camera = (fude_zoom_camera){ s.root, { 0, 0 }, 1.0 };
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { -100, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, (const f64[3]){ 30, 20, 0 }, 3u, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    f64 bn[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 bc = fude_zoom_board_numbers(bn, 40, 25, 18.0, 0u, "Shelf");
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 100, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_BOARD, bn, bc, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 100 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, (const f64[2]){ 50, 0 }, 2u, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    f64 sn[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 sc = fude_zoom_symbol_numbers(sn, fude_zoom_symbol_find("process"), 30, 20, 10, "Step");
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, -100 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL, sn, sc, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    fude_bytes stl = fude_bytes_new(4096);
    fude_zoom_stl_said said;
    CHECK(fude_zoom_export_stl(&s, (fude_zoom_v2){ 400, 300 }, 1.0, 3.0, "Sketching", &stl, &said));
    CHECK(said.solids == 2u && rde_arr_length(&stl) == 84u + 50u * said.triangles);
    CHECK(strncmp((const c8*)stl.memory, "Sketching: a 3D model", 21) == 0);   // (the app that made it, as given)
    u32 count;
    memcpy(&count, (const u8*)stl.memory + 80u, 4u);
    CHECK(count == said.triangles);
    // Its volume: the rectangle 60 × 40 × 3, the board 80 × 50 × 18 (each triangle: its facing, then its corners).
    rde_arr ca; f32* corners = table(&ca, sizeof(f32), count * 9u);
    for(u32 i = 0; i < count; i++) {
        memcpy(&corners[i * 9u], (const u8*)stl.memory + 84u + i * 50u + 12u, 36u);
    }
    CHECK(fabs(stl_volume(corners, count) - (60.0 * 40.0 * 3.0 + 80.0 * 50.0 * 18.0)) < 1e-1);
    rde_arr_free(&ca);
    rde_arr_free(&stl);
    fude_zoom_scene_destroy(&s);
    // A shape's line: dashed, its pieces 7 widths long and 3.5 apart along it; a centre line's dash and dot.
    rde_arr d = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    const fude_zoom_v2 line[2] = { { 0, 0 }, { 105, 0 } };
    CHECK(fude_zoom_line_dashes(line, 2u, false, FUDE_ZOOM_LINE_DASHED, 2.0, (fude_zoom_box){ -1000, -1000, 1000, 1000 }, &d) == 5u);
    const fude_zoom_v2* q = (const fude_zoom_v2*)d.memory;
    CHECK(fabs(q[1].x - 14.0) < 1e-9 && fabs(q[2].x - 21.0) < 1e-9);
    CHECK(fude_zoom_line_dashes(line, 2u, false, FUDE_ZOOM_LINE_DASH_DOT, 2.0, (fude_zoom_box){ -1000, -1000, 1000, 1000 }, &d) == 6u);
    CHECK(fude_zoom_line_dashes(line, 2u, false, FUDE_ZOOM_LINE_SOLID, 2.0, (fude_zoom_box){ -1000, -1000, 1000, 1000 }, &d) == 0u);
    // Off the screen: none; partly on it: the pattern as counted from the line's start.
    CHECK(fude_zoom_line_dashes(line, 2u, false, FUDE_ZOOM_LINE_DASHED, 2.0, (fude_zoom_box){ 200, -10, 300, 10 }, &d) == 0u);
    CHECK(fude_zoom_line_dashes(line, 2u, false, FUDE_ZOOM_LINE_DASHED, 2.0, (fude_zoom_box){ 18, -10, 60, 10 }, &d) == 2u);   // (21-35, 42-56)
    q = (const fude_zoom_v2*)d.memory;
    CHECK(fabs(q[0].x - 21.0) < 1e-9 && fabs(q[1].x - 35.0) < 1e-9);
    rde_arr_free(&d);
}

// Reached from a point: square to what the pen is near (its foot), along a straight thing near or a way asked for;
// nothing of either far off; what is drawn there first.
static void test_snap_from(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const rde_color c = { 1, 1, 1, 255 };
    const f64 base[2] = { 500, 30 };
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { -250, -120 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, base, 2, c, 1.0f, 0u, 0);
    fude_zoom_visible v; memset(&v, 0, sizeof v);
    v.frame = s.root; v.to_screen = (fude_zoom_sim){ 1.0, 0.0, 0.0, 0.0 };
    const fude_zoom_v2 ways[2] = { { 1, 0 }, { 0, 1 } };
    // From (10, 150) to near the line: its foot, where the line from there is square to it.
    fude_zoom_snap h = fude_zoom_snap_find_from(&s, &v, 1, (fude_zoom_v2){ 30, -95 }, 14.0, (fude_zoom_v2){ 10, 150 }, ways, 2u, 0.0);
    const f64 t = (260.0 * 500.0 + 270.0 * 30.0) / (500.0 * 500.0 + 30.0 * 30.0);
    CHECK(h.kind == FUDE_ZOOM_SNAP_PERP && fabs(h.at.x - (-250.0 + 500.0 * t)) < 1e-9 && fabs(h.at.y - (-120.0 + 30.0 * t)) < 1e-9);
    CHECK(fabs((h.at.x - 10.0) * 500.0 + (h.at.y - 150.0) * 30.0) < 1e-6 && h.a.x == -250.0 && h.b.x == 250.0);
    // Along it, from above: brought onto the line from there its way.
    h = fude_zoom_snap_find_from(&s, &v, 1, (fude_zoom_v2){ 150, 124 }, 14.0, (fude_zoom_v2){ -200, 100 }, ways, 2u, 0.0);
    CHECK(h.kind == FUDE_ZOOM_SNAP_PARALLEL && fabs((h.at.y - 100.0) / (h.at.x + 200.0) - 30.0 / 500.0) < 1e-9);
    // Across (a way asked for), far from the line.
    h = fude_zoom_snap_find_from(&s, &v, 1, (fude_zoom_v2){ 300, 703 }, 14.0, (fude_zoom_v2){ 0, 700 }, ways, 2u, 0.0);
    CHECK(h.kind == FUDE_ZOOM_SNAP_PARALLEL && fabs(h.at.y - 700.0) < 1e-9 && fabs(h.at.x - 300.0) < 1e-9 && h.a.x == h.b.x);
    // Nothing there, no way near: none.
    h = fude_zoom_snap_find_from(&s, &v, 1, (fude_zoom_v2){ 300, 760 }, 14.0, (fude_zoom_v2){ 0, 700 }, ways, 2u, 0.0);
    CHECK(h.kind == FUDE_ZOOM_SNAP_NONE);
    // The line's end there: an end, not its foot.
    h = fude_zoom_snap_find_from(&s, &v, 1, (fude_zoom_v2){ 248, -92 }, 14.0, (fude_zoom_v2){ 240, 150 }, ways, 2u, 0.0);
    CHECK(h.kind == FUDE_ZOOM_SNAP_END);
    // Right on the line between its ends, with nothing better: its nearest point (reached from nowhere too).
    h = fude_zoom_snap_find(&s, &v, 1, (fude_zoom_v2){ 100, -95 }, 14.0);
    {
        const f64 ll = 500.0 * 500.0 + 30.0 * 30.0, tt = (350.0 * 500.0 + 25.0 * 30.0) / ll;   // (its nearest: the pen's foot on it)
        CHECK(h.kind == FUDE_ZOOM_SNAP_NEAREST && fabs(h.at.x - (-250.0 + 500.0 * tt)) < 1e-9 && fabs(h.at.y - (-120.0 + 30.0 * tt)) < 1e-9);
    }
    // Carried on past its end (far from what else is drawn): onto its line.
    h = fude_zoom_snap_find_from(&s, &v, 1, (fude_zoom_v2){ 400, -78 }, 14.0, (fude_zoom_v2){ 400, 300 }, NULL, 0u, 0.0);
    {
        const f64 ll = 500.0 * 500.0 + 30.0 * 30.0, tt = (650.0 * 500.0 + 42.0 * 30.0) / ll;   // (the pen's foot on its line, past its end)
        CHECK(h.kind == FUDE_ZOOM_SNAP_EXTENSION && fabs(h.at.x - (-250.0 + 500.0 * tt)) < 1e-9 && fabs(h.at.y - (-120.0 + 30.0 * tt)) < 1e-9);
    }
    // An angle's step: 30° from the way across, the pen a little off it.
    const fude_zoom_v2 across[1] = { { 1, 0 } };
    h = fude_zoom_snap_find_from(&s, &v, 1, (fude_zoom_v2){ 0 + 200.0 * cos(0.5236) + 2.0, 1500 + 200.0 * sin(0.5236) }, 14.0, (fude_zoom_v2){ 0, 1500 }, across, 1u, 15.0);
    CHECK(h.kind == FUDE_ZOOM_SNAP_PARALLEL && fabs(atan2(h.at.y - 1500.0, h.at.x) - 3.14159265358979323846 / 6.0) < 1e-9);
    // A circle near the pen: where the line from a point outside it touches it.
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 3000 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, (const f64[2]){ 50, 50 }, 2u, c, 1.0f, 0u, 0);
    const f64 tl = sqrt(200.0 * 200.0 - 50.0 * 50.0);   // (from (200, 3000): the touch point, 50 round from the centre)
    const fude_zoom_v2 touch = { 50.0 * 50.0 / 200.0, 3000.0 + 50.0 * tl / 200.0 };
    h = fude_zoom_snap_find_from(&s, &v, 1, (fude_zoom_v2){ touch.x + 3.0, touch.y + 4.0 }, 14.0, (fude_zoom_v2){ 200, 3000 }, NULL, 0u, 0.0);
    CHECK(h.kind == FUDE_ZOOM_SNAP_TANGENT && fabs(h.at.x - touch.x) < 1e-9 && fabs(h.at.y - touch.y) < 1e-9);
    fude_zoom_scene_destroy(&s);
}

// Electronics (circuit.h): parts as symbols, wires pin to pin, the circuit solved.
static u32 part_put(fude_zoom_scene* s, const c8* id, f64 x, f64 y, f64 hw, f64 hh, const c8* text) {
    f64 n[FUDE_ZOOM_SHAPE_NUMBERS + 200];
    const u32 kind = fude_zoom_symbol_find(id);
    CHECK(kind != FUDE_ZOOM_NONE);
    const u32 k = fude_zoom_symbol_numbers(n, kind, hw, hh, 2.0, text);
    return fude_zoom_scene_add_shape(s, s->root, (fude_zoom_place){ { x, y }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL, n, k, (rde_color){ 1, 1, 1, 255 }, 0.2f, 0u, 0);
}

static u32 wire_put(fude_zoom_scene* s, u32 a, u32 pa, u32 b, u32 pb) {
    fude_zoom_v2 p0 = { 0.0, 0.0 }, p1 = { 0.0, 0.0 }, r[6];
    CHECK(fude_zoom_part_pin_at(s, a, pa, &p0) && fude_zoom_part_pin_at(s, b, pb, &p1));
    const u32 m = fude_zoom_wire_route(p0, fude_zoom_part_side_at(s, a, pa), p1, fude_zoom_part_side_at(s, b, pb), 10.0, r);
    return fude_zoom_wire_add(s, s->root, r, m, a, (i32)pa, b, (i32)pb, (rde_color){ 1, 1, 1, 255 }, 0.1f);
}

static const fude_zoom_circuit_part* circuit_part(const fude_zoom_circuit* c, u32 object) {
    for(u32 i = 0; i < (u32)rde_arr_length(&c->parts); i++) {
        if(((const fude_zoom_circuit_part*)c->parts.memory)[i].object == object) return &((const fude_zoom_circuit_part*)c->parts.memory)[i];
    }
    return NULL;
}

static f64 circuit_volts(const fude_zoom_circuit* c, u32 object, u32 pin) {
    const fude_zoom_circuit_part* p = circuit_part(c, object);
    return p != NULL && p->node[pin] != FUDE_ZOOM_NONE ? fude_zoom_circuit_volts(c, p->node[pin]) : -999.0;
}

static void test_circuits(void) {
    f64 v = 0.0;
    CHECK(fude_zoom_circuit_value("4.7k", 0u, &v) && fabs(v - 4700.0) < 1e-9);
    CHECK(fude_zoom_circuit_value("4k7", 0u, &v) && fabs(v - 4700.0) < 1e-9);
    CHECK(fude_zoom_circuit_value("-5V", 0u, &v) && v == -5.0 && fude_zoom_circuit_value("\xE2\x88\x92" "12 V", 0u, &v) && v == -12.0);
    CHECK(fude_zoom_circuit_value("4-20mA", 1u, &v) && fabs(v - 0.02) < 1e-12 && fude_zoom_circuit_value("Q-1k", 0u, &v) && v == 1000.0);
    CHECK(fude_zoom_circuit_value("100uF", 0u, &v) && fabs(v - 1e-4) < 1e-15);
    CHECK(fude_zoom_circuit_value("5V 50Hz", 1u, &v) && fabs(v - 50.0) < 1e-12);
    CHECK(fude_zoom_circuit_value("10mA", 0u, &v) && fabs(v - 0.01) < 1e-15);
    CHECK(fude_zoom_circuit_value("2.2 M", 0u, &v) && fabs(v - 2.2e6) < 1e-6);
    CHECK(!fude_zoom_circuit_value("red", 0u, &v));
    // (as the Value card writes them)
    CHECK(fude_zoom_circuit_value("4.7k\xCE\xA9", 0u, &v) && fabs(v - 4700.0) < 1e-9);
    CHECK(fude_zoom_circuit_value("2.2M\xCE\xA9", 0u, &v) && fabs(v - 2.2e6) < 1e-6);
    CHECK(fude_zoom_circuit_value("100\xC2\xB5" "F", 0u, &v) && fabs(v - 1e-4) < 1e-15);
    CHECK(fude_zoom_circuit_value("1.5V", 0u, &v) && fabs(v - 1.5) < 1e-12);
    CHECK(fude_zoom_circuit_value("20mA", 0u, &v) && fabs(v - 0.02) < 1e-15);
    // A divider: 10 V over two 1 kΩ, its middle at 5 V.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 src = part_put(&s, "DC source", 0, 0, 20, 30, "10V");
        const u32 r1  = part_put(&s, "resistor", 100, 60, 30, 10, "1k");
        const u32 r2  = part_put(&s, "resistor", 200, 0, 30, 10, "1k");
        const u32 g   = part_put(&s, "ground", 0, -100, 20, 20, "");
        wire_put(&s, src, 0, r1, 0);
        wire_put(&s, r1, 1, r2, 0);
        wire_put(&s, r2, 1, g, 0);
        wire_put(&s, src, 1, g, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        CHECK(fude_zoom_circuit_build(&c, &s) == 4u && c.grounded);
        CHECK(fude_zoom_circuit_dc(&c));
        CHECK(fabs(circuit_volts(&c, r1, 1) - 5.0) < 1e-3 && fabs(circuit_volts(&c, src, 0) - 10.0) < 1e-3);
        // Each resistor's current 5 mA, into its first pin; the wires between them carry it.
        CHECK(fabs(circuit_part(&c, r1)->pin_i[0] - 0.005) < 1e-6 && fabs(circuit_part(&c, r2)->pin_i[0] - 0.005) < 1e-6);
        const fude_zoom_circuit_wire* w = (const fude_zoom_circuit_wire*)c.wires.memory;
        CHECK(fabs(fabs(w[1].current[0]) - 0.005) < 1e-6);
        // Played alone (its scope: the source, R1 and the ground; a lone resistor elsewhere not): their wires come too.
        const u32 lone = part_put(&s, "resistor", 900, 900, 30, 10, "1k");
        u8 scope[64] = { 0 };
        scope[src] = scope[r1] = scope[g] = 1u;
        CHECK(fude_zoom_circuit_build_in(&c, &s, scope) == 3u && circuit_part(&c, lone) == NULL && circuit_part(&c, r2) == NULL);
        CHECK(rde_arr_length(&c.wires) == 4u);   // (src–R1, R1–R2 and R2–ground by their ends on its parts, src–ground)
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
    // Played alone, wires reach on through wires (each found near the others in a grid, then walked): a wire from the
    // source's pin, a branch starting on its way, a branch off that one, a wire ending on that branch's way, and a
    // chain of 300 more each starting on the last one's way — all in; a wire far off, joined to none of them, out.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 src = part_put(&s, "DC source", 0, 0, 20, 30, "10V");
        fude_zoom_v2 p0;
        CHECK(fude_zoom_part_pin_at(&s, src, 0, &p0));
        const rde_color ink = { 1, 1, 1, 255 };
        const fude_zoom_v2 a[2] = { p0, { p0.x, p0.y + 200.0 } };
        const fude_zoom_v2 b[2] = { { p0.x, p0.y + 100.0 }, { p0.x + 300.0, p0.y + 100.0 } };
        const fude_zoom_v2 cc[2] = { { p0.x + 150.0, p0.y + 100.0 }, { p0.x + 150.0, p0.y + 400.0 } };
        const fude_zoom_v2 e[2] = { { p0.x + 500.0, p0.y + 300.0 }, { p0.x + 150.0, p0.y + 300.0 } };
        const fude_zoom_v2 far[2] = { { -5000.0, 5000.0 }, { -4900.0, 5000.0 } };
        u32 w[5];
        w[0] = fude_zoom_wire_add(&s, s.root, a, 2u, src, 0, FUDE_ZOOM_NONE, -1, ink, 0.1f);
        w[1] = fude_zoom_wire_add(&s, s.root, b, 2u, FUDE_ZOOM_NONE, -1, FUDE_ZOOM_NONE, -1, ink, 0.1f);
        w[2] = fude_zoom_wire_add(&s, s.root, cc, 2u, FUDE_ZOOM_NONE, -1, FUDE_ZOOM_NONE, -1, ink, 0.1f);
        w[3] = fude_zoom_wire_add(&s, s.root, e, 2u, FUDE_ZOOM_NONE, -1, FUDE_ZOOM_NONE, -1, ink, 0.1f);
        w[4] = fude_zoom_wire_add(&s, s.root, far, 2u, FUDE_ZOOM_NONE, -1, FUDE_ZOOM_NONE, -1, ink, 0.1f);
        fude_zoom_v2 last[2] = { cc[0], cc[1] };   // (the chain: each from halfway along the last, 100 on, a staircase)
        for(u32 k = 0; k < 300u; k++) {
            const fude_zoom_v2 mid = { (last[0].x + last[1].x) * 0.5, (last[0].y + last[1].y) * 0.5 };
            const fude_zoom_v2 next[2] = { mid, { mid.x + (k % 2u == 0u ? 100.0 : 0.0), mid.y + (k % 2u == 0u ? 0.0 : 100.0) } };
            fude_zoom_wire_add(&s, s.root, next, 2u, FUDE_ZOOM_NONE, -1, FUDE_ZOOM_NONE, -1, ink, 0.1f);
            last[0] = next[0]; last[1] = next[1];
        }
        rde_arr scope_arr = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
        rde_arr_resize(&scope_arr, fude_zoom_scene_object_count(&s));
        memset(scope_arr.memory, 0, fude_zoom_scene_object_count(&s));
        ((u8*)scope_arr.memory)[src] = 1u;
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        CHECK(fude_zoom_circuit_build_in(&c, &s, (const u8*)scope_arr.memory) == 1u);
        b8 seen[5] = { false, false, false, false, false };
        const fude_zoom_circuit_wire* cw = (const fude_zoom_circuit_wire*)c.wires.memory;
        for(u32 i = 0; i < (u32)rde_arr_length(&c.wires); i++) for(u32 k = 0; k < 5u; k++) seen[k] = seen[k] || cw[i].object == w[k];
        CHECK(rde_arr_length(&c.wires) == 304u && seen[0] && seen[1] && seen[2] && seen[3] && !seen[4]);
        fude_zoom_circuit_destroy(&c);
        rde_arr_free(&scope_arr);
        fude_zoom_scene_destroy(&s);
    }
    // Into a breadboard's hole: straight in, never past it and back (a hole has no side: FUDE_ZOOM_PIN_ANY).
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 bb = part_put(&s, "breadboard", 0, 0, 200, 120, "");
        const u32 r  = part_put(&s, "resistor", -400, -300, 30, 10, "1k");
        fude_zoom_v2 hole = { 0.0, 0.0 }, pin = { 0.0, 0.0 }, way[6];
        CHECK(fude_zoom_part_pin_at(&s, bb, 5u, &hole) && fude_zoom_part_pin_at(&s, r, 1u, &pin));   // (a hole on the top rail)
        CHECK(fude_zoom_part_side_at(&s, bb, 5u) == 255u);
        const u32 m = fude_zoom_wire_route(pin, fude_zoom_part_side_at(&s, r, 1u), hole, fude_zoom_part_side_at(&s, bb, 5u), 10.0, way);
        f64 top = -1e300;
        for(u32 i = 0; i < m; i++) { top = fmax(top, way[i].y); }
        CHECK(m >= 2u && hypot(way[m - 1u].x - hole.x, way[m - 1u].y - hole.y) < 1e-9 && top <= hole.y + 1e-9);
        fude_zoom_scene_destroy(&s);
    }
    // A device whose ids are large (a tablet's: its half above 2^20): a wire still keeps its parts exactly, and follows
    // one moved. (Before 0.1.49 an id was one number, its last bits lost.)
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 0x342F1234u);
        const u32 ra = part_put(&s, "resistor", 0, 0, 30, 10, "1k");
        const u32 rb = part_put(&s, "resistor", 200, 100, 30, 10, "1k");
        const u32 w = wire_put(&s, ra, 1, rb, 0);
        f64 n[1u + 2u * FUDE_ZOOM_WIRE_POINTS + 8u];
        const u32 c = fude_zoom_scene_shape_numbers(&s, w, n, 1u + 2u * FUDE_ZOOM_WIRE_POINTS + 8u);
        fude_zoom_id from = 0, to = 0;
        i32 fp = -1, tp = -1;
        CHECK(fude_zoom_wire_of(n, c, NULL, &from, &fp, &to, &tp) >= 2u && from == fude_zoom_scene_object(&s, ra)->id && to == fude_zoom_scene_object(&s, rb)->id);
        fude_zoom_place pl = fude_zoom_scene_place_of(&s, rb);
        pl.t.y += 50.0;
        fude_zoom_scene_set_place(&s, rb, pl);
        rde_arr died = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std()), born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        CHECK(fude_zoom_wire_follow(&s, &rb, 1u, &died, &born) == 1u);
        rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
        b8 closed;
        fude_zoom_scene_shape_outline(&s, ((const u32*)born.memory)[0], 1u, &pts, &closed);
        fude_zoom_v2 pin;
        CHECK(fude_zoom_part_pin_at(&s, rb, 0, &pin));
        const fude_zoom_v2 last = ((const fude_zoom_v2*)pts.memory)[rde_arr_length(&pts) - 1u];
        CHECK(hypot(last.x - pin.x, last.y - pin.y) < 1e-6);
        // An old save's wire (each id one number, rounded): its parts still found, by their pins where its ends are.
        f64 old[1u + 2u * 2u + 4u] = { 2.0, 0.0, 0.0, 0.0, 0.0, (f64)fude_zoom_scene_object(&s, ra)->id, 1.0, (f64)fude_zoom_scene_object(&s, rb)->id, 0.0 };
        fude_zoom_v2 a1, b0;
        fude_zoom_part_pin_at(&s, ra, 1, &a1);
        fude_zoom_part_pin_at(&s, rb, 0, &b0);
        CHECK(fude_zoom_wire_of(old, 9u, NULL, &from, &fp, &to, &tp) == 2u && fp == 1 && tp == 0);
        CHECK(fude_zoom_wire_part(&s, from, fp, s.root, a1) == ra && fude_zoom_wire_part(&s, to, tp, s.root, b0) == rb);
        // The longest wire: every number fits FUDE_ZOOM_WIRE_NUMBERS (it was two short once ids took two numbers), and
        // both ends read back.
        { fude_zoom_v2 longest[FUDE_ZOOM_WIRE_POINTS];
          for(u32 i = 0; i < FUDE_ZOOM_WIRE_POINTS; i++) longest[i] = (fude_zoom_v2){ (f64)i * 10.0, (f64)(i % 2u) * 5.0 };
          f64 nn[FUDE_ZOOM_WIRE_NUMBERS + 1u]; nn[FUDE_ZOOM_WIRE_NUMBERS] = 12345.0;
          const fude_zoom_id ia = fude_zoom_scene_object(&s, ra)->id, ib = fude_zoom_scene_object(&s, rb)->id;
          const u32 wrote = fude_zoom_wire_numbers(nn, longest, FUDE_ZOOM_WIRE_POINTS, ia, 1, ib, 0);
          CHECK(wrote == FUDE_ZOOM_WIRE_NUMBERS && nn[FUDE_ZOOM_WIRE_NUMBERS] == 12345.0);
          fude_zoom_id lf = 0, lt = 0; i32 lfp = -1, ltp = -1;
          CHECK(fude_zoom_wire_of(nn, wrote, NULL, &lf, &lfp, &lt, &ltp) == FUDE_ZOOM_WIRE_POINTS && lf == ia && lfp == 1 && lt == ib && ltp == 0);
          // A damaged file's numbers (a count, pins, an old id no type holds): no wire, or no pin and no part at an
          // end, never a cast of such a number (undefined: UBSan stops the suite on one).
          f64 bad[FUDE_ZOOM_WIRE_NUMBERS];
          memcpy(bad, nn, sizeof bad);
          bad[0] = NAN;
          CHECK(fude_zoom_wire_of(bad, wrote, NULL, &lf, &lfp, &lt, &ltp) == 0u);
          bad[0] = 1e300;
          CHECK(fude_zoom_wire_of(bad, wrote, NULL, &lf, &lfp, &lt, &ltp) == 0u);
          bad[0] = -5.0;
          CHECK(fude_zoom_wire_of(bad, wrote, NULL, &lf, &lfp, &lt, &ltp) == 0u);
          memcpy(bad, nn, sizeof bad);
          bad[1u + 2u * FUDE_ZOOM_WIRE_POINTS + 2u] = NAN; bad[1u + 2u * FUDE_ZOOM_WIRE_POINTS + 5u] = 1e300;
          CHECK(fude_zoom_wire_of(bad, wrote, NULL, &lf, &lfp, &lt, &ltp) == FUDE_ZOOM_WIRE_POINTS && lf == ia && lfp == -1 && lt == ib && ltp == -1);
          f64 old_bad[9] = { 2.0, 0.0, 0.0, 10.0, 0.0, 1e300, -1e300, NAN, 3e9 };
          CHECK(fude_zoom_wire_of(old_bad, 9u, NULL, &lf, &lfp, &lt, &ltp) == 2u && lf == 0u && lfp == -1 && lt == 0u && ltp == -1); }
        rde_arr_free(&died); rde_arr_free(&born); rde_arr_free(&pts);
        fude_zoom_scene_destroy(&s);
    }
    // An LED from a 9 V battery through 1 kΩ: about 7 mA, lit about 0.7.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 bat = part_put(&s, "battery", 0, 0, 20, 30, "9V");
        const u32 r   = part_put(&s, "resistor", 100, 60, 30, 10, "1k");
        const u32 led = part_put(&s, "LED", 200, 0, 30, 20, "red");
        wire_put(&s, bat, 0, r, 0);
        wire_put(&s, r, 1, led, 0);
        wire_put(&s, led, 1, bat, 1);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        CHECK(fude_zoom_circuit_dc(&c));
        const fude_zoom_circuit_part* l = circuit_part(&c, led);
        CHECK(l->pin_i[0] > 0.0068 && l->pin_i[0] < 0.0076);
        CHECK(fude_zoom_circuit_run(&c, 0.01, 100u) && fabs(l->shown - l->pin_i[0] / 0.01) < 1e-9);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
    // A capacitor charging: 5 V, 1 kΩ, 1 mF — after one time constant, 63 %.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 src = part_put(&s, "DC source", 0, 0, 20, 30, "5V");
        const u32 r   = part_put(&s, "resistor", 100, 60, 30, 10, "1k");
        const u32 cap = part_put(&s, "capacitor", 200, 0, 20, 20, "1mF");
        const u32 g   = part_put(&s, "ground", 0, -100, 20, 20, "");
        wire_put(&s, src, 0, r, 0);
        wire_put(&s, r, 1, cap, 0);
        wire_put(&s, cap, 1, g, 0);
        wire_put(&s, src, 1, g, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        CHECK(fude_zoom_circuit_run(&c, 1.0, 2000u));
        CHECK(fabs(c.time - 1.0) < 1e-6 && fabs(circuit_volts(&c, cap, 0) - 5.0 * (1.0 - exp(-1.0))) < 0.01);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
    // A 555 astable: 1 kΩ, 10 kΩ, 10 µF — about 6.9 Hz.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 src = part_put(&s, "DC source", -300, 0, 20, 30, "9V");
        const u32 g   = part_put(&s, "ground", -300, -200, 20, 20, "");
        const u32 t   = part_put(&s, "NE555", 0, 0, 70, 50, "NE555");
        const u32 r1  = part_put(&s, "resistor", 200, 150, 30, 10, "1k");
        const u32 r2  = part_put(&s, "resistor", 200, 80, 30, 10, "10k");
        const u32 cap = part_put(&s, "capacitor", 200, -150, 20, 20, "10uF");
        wire_put(&s, src, 1, g, 0);
        wire_put(&s, src, 0, t, 7);    // VCC
        wire_put(&s, t, 7, t, 3);      // RESET to VCC
        wire_put(&s, t, 0, g, 0);      // GND
        wire_put(&s, t, 7, r1, 0);     // VCC → R1
        wire_put(&s, r1, 1, t, 6);     // R1 → DISCH
        wire_put(&s, t, 6, r2, 0);     // DISCH → R2
        wire_put(&s, r2, 1, t, 5);     // R2 → THRES
        wire_put(&s, t, 5, t, 1);      // THRES = TRIG
        wire_put(&s, t, 1, cap, 0);
        wire_put(&s, cap, 1, g, 0);
        const u32 load = part_put(&s, "resistor", -150, -60, 30, 10, "1k");   // (its output into a load)
        wire_put(&s, t, 2, load, 1);
        wire_put(&s, load, 0, g, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        c.step = 1e-4;
        u32 rises = 0;
        b8 was = false;
        for(u32 k = 0; k < 2000u; k++) {
            CHECK(fude_zoom_circuit_run(&c, 1e-3, 20u));
            const b8 high = circuit_volts(&c, t, 2) > 4.0;
            rises += high && !was ? 1u : 0u;
            was = high;
        }
        // (its first cycle longer: charging from nothing to two thirds)
        CHECK(rises >= 11u && rises <= 15u);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
    // Logic: an input through a NOT gate to a probe; tapped, the probe the other way.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 in  = part_put(&s, "logic input", 0, 0, 30, 20, "0");
        const u32 nt  = part_put(&s, "NOT gate", 100, 0, 40, 30, "");
        const u32 pr  = part_put(&s, "logic probe", 200, 0, 20, 20, "");
        wire_put(&s, in, 0, nt, 0);
        wire_put(&s, nt, 1, pr, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        CHECK(fude_zoom_circuit_run(&c, 0.01, 10u) && circuit_part(&c, pr)->shown == 1.0);
        c8 say[32];
        u32 idx = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&c.parts); i++) if(((const fude_zoom_circuit_part*)c.parts.memory)[i].object == in) idx = i;
        CHECK(fude_zoom_circuit_tap(&c, idx, -1, say, sizeof(say)) && strcmp(say, "1") == 0);
        CHECK(fude_zoom_circuit_run(&c, 0.01, 10u) && circuit_part(&c, pr)->shown == 0.0);
        // Built again (the canvas changed): its state kept.
        fude_zoom_circuit_build(&c, &s);
        CHECK(fude_zoom_circuit_run(&c, 0.01, 10u) && circuit_part(&c, pr)->shown == 0.0);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
    // A breadboard: a resistor's pins in holes a1 and a7, the battery's wires in b1 and b7 — joined by the strips.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 bb = part_put(&s, "breadboard", 0, 0, 165, 92.5, "");
        fude_zoom_v2 h1 = { 0.0, 0.0 }, h7 = { 0.0, 0.0 }, b1 = { 0.0, 0.0 }, b7 = { 0.0, 0.0 };
        CHECK(fude_zoom_part_pin_at(&s, bb, 2u * FUDE_ZOOM_BREADBOARD_COLS + 0u, &h1) && fude_zoom_part_pin_at(&s, bb, 2u * FUDE_ZOOM_BREADBOARD_COLS + 6u, &h7));
        CHECK(fude_zoom_part_pin_at(&s, bb, 3u * FUDE_ZOOM_BREADBOARD_COLS + 0u, &b1) && fude_zoom_part_pin_at(&s, bb, 3u * FUDE_ZOOM_BREADBOARD_COLS + 6u, &b7));
        CHECK(fabs(h7.x - h1.x - 60.0) < 1e-3 && fabs(h1.y - b1.y - 10.0) < 1e-3);   // (holes 10 apart)
        const u32 r = part_put(&s, "resistor", (h1.x + h7.x) * 0.5, h1.y, 30, 6, "100");
        const u32 bat = part_put(&s, "battery", -300, 0, 20, 30, "5V");
        fude_zoom_v2 bp, bm, rr[6];
        fude_zoom_part_pin_at(&s, bat, 0u, &bp);
        fude_zoom_part_pin_at(&s, bat, 1u, &bm);
        u32 m = fude_zoom_wire_route(bp, FUDE_ZOOM_PIN_UP, b1, 255u, 10.0, rr);
        fude_zoom_wire_add(&s, s.root, rr, m, bat, 0, FUDE_ZOOM_NONE, -1, (rde_color){ 1, 1, 1, 255 }, 0.1f);
        m = fude_zoom_wire_route(bm, FUDE_ZOOM_PIN_DOWN, b7, 255u, 10.0, rr);
        fude_zoom_wire_add(&s, s.root, rr, m, bat, 1, FUDE_ZOOM_NONE, -1, (rde_color){ 1, 1, 1, 255 }, 0.1f);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        CHECK(fude_zoom_circuit_dc(&c));
        const f64 i = circuit_part(&c, r)->pin_i[0];
        CHECK(fabs(i - 5.0 / 100.5) < 1e-4);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
}

// A function of x as written: worked out.
static void test_calc(void) {
    fude_zoom_calc c;
    CHECK(fude_zoom_calc_parse(&c, "y = 2x + 3") && fabs(fude_zoom_calc_at(&c, 4.0) - 11.0) < 1e-12);
    CHECK(fude_zoom_calc_parse(&c, "x^2/4 - 1") && fabs(fude_zoom_calc_at(&c, 2.0) - 0.0) < 1e-12);
    CHECK(fude_zoom_calc_parse(&c, "-x^2") && fabs(fude_zoom_calc_at(&c, 3.0) + 9.0) < 1e-12);   // (−(x²))
    CHECK(fude_zoom_calc_parse(&c, "2^3^2") && fabs(fude_zoom_calc_at(&c, 0.0) - 512.0) < 1e-9);  // (right to left)
    CHECK(fude_zoom_calc_parse(&c, "3(x+1)(x-2)") && fabs(fude_zoom_calc_at(&c, 3.0) - 12.0) < 1e-12);
    CHECK(fude_zoom_calc_parse(&c, "f(x) = sin(x) + cos x") && fabs(fude_zoom_calc_at(&c, 0.5) - (sin(0.5) + cos(0.5))) < 1e-12);
    CHECK(fude_zoom_calc_parse(&c, "sqrt(abs(x))") && fabs(fude_zoom_calc_at(&c, -16.0) - 4.0) < 1e-12);
    CHECK(fude_zoom_calc_parse(&c, "e^-x") && fabs(fude_zoom_calc_at(&c, 1.0) - exp(-1.0)) < 1e-12);
    CHECK(fude_zoom_calc_parse(&c, "|x - 3|") && fabs(fude_zoom_calc_at(&c, 1.0) - 2.0) < 1e-12);
    CHECK(fude_zoom_calc_parse(&c, "2pi x") && fabs(fude_zoom_calc_at(&c, 1.0) - 2.0 * 3.14159265358979323846) < 1e-12);
    CHECK(fude_zoom_calc_parse(&c, "Y = 2·X − 1") && fabs(fude_zoom_calc_at(&c, 2.0) - 3.0) < 1e-12);
    CHECK(fude_zoom_calc_parse(&c, "1/x") && isnan(fude_zoom_calc_at(&c, 0.0)));
    CHECK(fude_zoom_calc_parse(&c, "ln(x)") && isnan(fude_zoom_calc_at(&c, -1.0)));
    CHECK(!fude_zoom_calc_parse(&c, "2 +") && !fude_zoom_calc_parse(&c, "sin(") && !fude_zoom_calc_parse(&c, "2 * * 3"));
    // Variables: a·sin(b·x) with a 2, b 3; not given, nothing.
    f64 vars[26];
    for(u32 i = 0; i < 26u; i++) vars[i] = NAN;
    vars[0] = 2.0; vars[1] = 3.0;
    CHECK(fude_zoom_calc_parse(&c, "y = a sin(b x)") && c.vars == 3u && fabs(fude_zoom_calc_at_with(&c, 0.5, vars) - 2.0 * sin(1.5)) < 1e-12);
    CHECK(isnan(fude_zoom_calc_at(&c, 0.5)));
    CHECK(fude_zoom_calc_parse(&c, "k(x - h)^2") && c.vars == ((1u << 10) | (1u << 7)));
}

// A graph's curves stay inside it, broken at an asymptote; a number line's ticks and numbers.
static void test_plot(void) {
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), heap), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), heap), labels = rde_arr_new(sizeof(fude_zoom_plot_label), heap);
    CHECK(fude_zoom_plot_is("graph") && fude_zoom_plot_is("number line") && !fude_zoom_plot_is("box"));
    CHECK(fude_zoom_plot_lines("graph", "y = x\nx -5 5\ny -5 5", 160.0, 110.0, &pts, &parts, &labels) == 1u);
    const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
    const fude_zoom_symbol_part* pa = (const fude_zoom_symbol_part*)parts.memory;
    u32 curves = 0, inside = 1;
    for(u32 i = 0; i < (u32)rde_arr_length(&parts); i++) {
        for(u32 k = 0; k < pa[i].count; k++) {
            const fude_zoom_v2 q = p[pa[i].first + k];
            inside &= (fabs(q.x) <= 160.0 + 1e-9 && fabs(q.y) <= 110.0 + 1e-9) ? 1u : 0u;
        }
        curves += pa[i].flags >= 32u ? 1u : 0u;
    }
    CHECK(inside == 1u && curves == 1u && rde_arr_length(&labels) > 4u);
    // 1/x: two pieces (no line drawn across x = 0).
    rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
    CHECK(fude_zoom_plot_lines("graph", "1/x\ny -5 5", 160.0, 110.0, &pts, &parts, &labels) == 1u);
    pa = (const fude_zoom_symbol_part*)parts.memory;
    curves = 0;
    for(u32 i = 0; i < (u32)rde_arr_length(&parts); i++) { curves += pa[i].flags >= 32u ? 1u : 0u; }
    CHECK(curves >= 2u);
    // A number line from -5 to 5: eleven numbers.
    rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
    fude_zoom_plot_lines("number line", "-5 5", 160.0, 25.0, &pts, &parts, &labels);
    const fude_zoom_plot_label* lb = (const fude_zoom_plot_label*)labels.memory;
    u32 found = 0;
    for(u32 i = 0; i < (u32)rde_arr_length(&labels); i++) { found += (strcmp(lb[i].text, "-5") == 0 || strcmp(lb[i].text, "5") == 0 || strcmp(lb[i].text, "0") == 0) ? 1u : 0u; }
    CHECK(rde_arr_length(&labels) == 11u && found == 3u);
    // Its two numbers plain decimals (calc.h's fude_zoom_calc_decimal): hex, "inf", "nan" and a number past what a
    // double holds are not a range (0x10 0x20 was read as 16 to 32, inf 5 left it with no numbers), so the line keeps
    // its own, as with nothing typed.
    rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
    fude_zoom_plot_lines("number line", "", 160.0, 25.0, &pts, &parts, &labels);
    const u32 own = (u32)rde_arr_length(&labels);
    CHECK(own > 2u);
    static const c8* const not_decimal[4] = { "0x10 0x20", "inf 5", "nan 5", "1e999 5" };
    for(u32 k = 0; k < 4u; k++) {
        rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
        fude_zoom_plot_lines("number line", not_decimal[k], 160.0, 25.0, &pts, &parts, &labels);
        lb = (const fude_zoom_plot_label*)labels.memory;
        u32 ends = 0, sixteen = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&labels); i++) {
            ends    += (strcmp(lb[i].text, "-10") == 0 || strcmp(lb[i].text, "10") == 0) ? 1u : 0u;
            sixteen += strcmp(lb[i].text, "16") == 0 ? 1u : 0u;
        }
        CHECK((u32)rde_arr_length(&labels) == own && ends == 2u && sixteen == 0u);
    }
    // Variables: y = a·x with a from 1 to 3; not playing, a is 1; a second and a half in, 2 (half
    // way); a slider's, its.
    fude_zoom_plot_var vs[4];
    CHECK(fude_zoom_plot_vars("graph", "y = a x\na = 1..3\nx = 2\n(1, 1)", vs, 4u) == 2u && vs[0].letter == 'a' && vs[0].range && vs[1].letter == 'x');
    fude_zoom_plot_play play;
    play.time = NAN;
    for(u32 i = 0; i < 26u; i++) play.held[i] = NAN;
    CHECK(fabs(fude_zoom_plot_var_value(&vs[0], &play) - 1.0) < 1e-12);
    play.time = 1.5;
    CHECK(fabs(fude_zoom_plot_var_value(&vs[0], &play) - 2.0) < 1e-12);
    play.held[0] = 2.5;
    CHECK(fabs(fude_zoom_plot_var_value(&vs[0], &play) - 2.5) < 1e-12);
    // Drawn with a = 2.5: the point marked at x = 2 is (2, 5), its numbers by it; the legend says a's value.
    rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
    CHECK(fude_zoom_plot_lines_play("graph", "y = a x\na = 1..3\nx = 2\ny -10 10", 160.0, 110.0, &play, &pts, &parts, &labels) == 1u);
    lb = (const fude_zoom_plot_label*)labels.memory;
    u32 said = 0;
    for(u32 i = 0; i < (u32)rde_arr_length(&labels); i++) { said += (strcmp(lb[i].text, "(2, 5)") == 0 || strcmp(lb[i].text, "a = 2.5") == 0) ? 1u : 0u; }
    CHECK(said == 2u);
    rde_arr_free(&pts); rde_arr_free(&parts); rde_arr_free(&labels);
}

// A graph's parts' points back in its view's numbers (plot.c's box: 0.92 and 0.88 of its half sizes).
static fude_zoom_v2 plot_view_of(fude_zoom_v2 q, f64 hw, f64 hh, f64 x0, f64 x1, f64 y0, f64 y1) {
    const f64 bx = hw * 0.92, by = hh * 0.88;
    return (fude_zoom_v2){ x0 + (q.x + bx) / (2.0 * bx) * (x1 - x0), y0 + (q.y + by) / (2.0 * by) * (y1 - y0) };
}

// How far a graph's curve k's points are from radius r round the origin, at most (its view as given); how many pieces.
static f64 plot_round(const c8* text, f64 r, f64 x0, f64 x1, f64 y0, f64 y1, u32* pieces) {
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), heap), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), heap);
    fude_zoom_plot_lines("graph", text, 160.0, 110.0, &pts, &parts, NULL);
    const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
    const fude_zoom_symbol_part* pa = (const fude_zoom_symbol_part*)parts.memory;
    f64 worst = 0.0;
    u32 n = 0, seen = 0;
    for(u32 i = 0; i < (u32)rde_arr_length(&parts); i++) {
        if(pa[i].flags < 32u || pa[i].flags >= 64u) continue;
        n++;
        for(u32 k = 0; k < pa[i].count; k++, seen++) {
            const fude_zoom_v2 v = plot_view_of(p[pa[i].first + k], 160.0, 110.0, x0, x1, y0, y1);
            worst = fmax(worst, fabs(hypot(v.x, v.y) - r));
        }
    }
    *pieces = n;
    rde_arr_free(&pts); rde_arr_free(&parts);
    return seen > 20u ? worst : 1e9;
}

// Whether a graph's labels say all of _want (NUL-separated, ending in an empty one).
static b8 plot_says(const c8* text, const c8* want) {
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), heap), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), heap), labels = rde_arr_new(sizeof(fude_zoom_plot_label), heap);
    fude_zoom_plot_lines("graph", text, 300.0, 220.0, &pts, &parts, &labels);
    const fude_zoom_plot_label* lb = (const fude_zoom_plot_label*)labels.memory;
    b8 all = true;
    for(const c8* w = want; *w != 0; w += strlen(w) + 1u) {
        b8 found = false;
        for(u32 i = 0; i < (u32)rde_arr_length(&labels); i++) found = found || strcmp(lb[i].text, w) == 0;
        if(!found) printf("  (not said: %s)\n", w);
        all = all && found;
    }
    rde_arr_free(&pts); rde_arr_free(&parts); rde_arr_free(&labels);
    return all;
}

// Its parts' flags counted: curves' (32 + k), dashed ones (+ 8), regions (64 + k).
static void plot_kinds(const c8* text, u32* curves, u32* dashed, u32* regions) {
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), heap), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), heap);
    fude_zoom_plot_lines("graph", text, 160.0, 110.0, &pts, &parts, NULL);
    const fude_zoom_symbol_part* pa = (const fude_zoom_symbol_part*)parts.memory;
    *curves = *dashed = *regions = 0;
    for(u32 i = 0; i < (u32)rde_arr_length(&parts); i++) {
        if(pa[i].flags >= 64u) (*regions)++;
        else if(pa[i].flags >= 32u) { (*curves)++; if((pa[i].flags - 32u) & 8u) (*dashed)++; }
    }
    rde_arr_free(&pts); rde_arr_free(&parts);
}

// Graphs beyond functions of x (0.1.81): numbers as maths writes them; equations of x and y on a grid, their pieces
// joined; curves of t and polar curves; a view round only those, a unit as long across as up; inequalities' regions,
// their edges dashed when strict; asymptotes broken; roots, turning points and crossings marked; an area's value; and
// nothing it is given, however broken, past its memory.
static void test_plot_curves(void) {
    printf("plot curves\n");
    fude_zoom_calc c;
    f64 v = 0.0;
    CHECK(fude_zoom_calc_number("2pi", &v) && fabs(v - 2.0 * 3.14159265358979323846) < 1e-12);
    CHECK(fude_zoom_calc_number("-pi/2", &v) && fabs(v + 1.5707963267948966) < 1e-12 && fude_zoom_calc_number("1e3", &v) && v == 1000.0);
    CHECK(!fude_zoom_calc_number("x", &v) && !fude_zoom_calc_number("a + 1", &v) && !fude_zoom_calc_number("2 +", &v) && !fude_zoom_calc_number("1/0", &v));
    CHECK(fude_zoom_calc_parse_expr(&c, "x^2 + y^2") && c.uses_x && c.vars == (1u << ('y' - 'a')));
    CHECK(!fude_zoom_calc_parse_expr(&c, "y = x"));   // (the whole of it: an = is not maths)
    CHECK(fude_zoom_calc_parse(&c, "y = 3") && !c.uses_x);
    // A circle, an equation: its points 2 from the middle, one line round.
    u32 pieces = 0;
    f64 off = plot_round("x^2 + y^2 = 4\nx -3 3\ny -3 3", 2.0, -3, 3, -3, 3, &pieces);
    printf("  x^2 + y^2 = 4: %u piece(s), %.5f off at most\n", pieces, off);
    CHECK(off < 0.01 && pieces == 1u);
    off = plot_round("(cos(t), sin(t))\nx -2 2\ny -2 2", 1.0, -2, 2, -2, 2, &pieces);
    CHECK(off < 1e-9 && pieces == 1u);
    off = plot_round("r = 2\nx -3 3\ny -3 3", 2.0, -3, 3, -3, 3, &pieces);
    CHECK(off < 1e-9 && pieces == 1u);
    off = plot_round("(2cos(t), 2sin(t))\nt 0 pi\nx -3 3\ny -3 3", 2.0, -3, 3, -3, 3, &pieces);   // (half of it: t as given)
    CHECK(off < 1e-9 && pieces == 1u);
    // Only an equation: round it, as long across as up — a circle round on the graph.
    {
        rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
        rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), heap), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), heap);
        fude_zoom_plot_lines("graph", "x^2 + y^2 = 9", 160.0, 110.0, &pts, &parts, NULL);
        const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
        const fude_zoom_symbol_part* pa = (const fude_zoom_symbol_part*)parts.memory;
        f64 wx = 0.0, wy = 0.0;
        for(u32 i = 0; i < (u32)rde_arr_length(&parts); i++) {
            if(pa[i].flags != 32u) continue;
            for(u32 k = 0; k < pa[i].count; k++) { wx = fmax(wx, fabs(p[pa[i].first + k].x)); wy = fmax(wy, fabs(p[pa[i].first + k].y)); }
        }
        printf("  x^2 + y^2 = 9 fitted: %.2f across, %.2f up\n", wx, wy);
        CHECK(wy > 50.0 && fabs(wx - wy) < 0.02 * wy);
        rde_arr_free(&pts); rde_arr_free(&parts);
    }
    // Inequalities: their regions; a strict one's edge dashed, the other's not.
    u32 curves = 0, dashed = 0, regions = 0;
    plot_kinds("y < x\nx -5 5\ny -5 5", &curves, &dashed, &regions);
    CHECK(regions >= 10u && curves >= 1u && dashed == curves);
    plot_kinds("x^2 + y^2 <= 4\nx -5 5\ny -5 5", &curves, &dashed, &regions);
    CHECK(regions >= 10u && curves == 1u && dashed == 0u);
    plot_kinds("y = x^2\nx -5 5\ny -5 5", &curves, &dashed, &regions);
    CHECK(regions == 0u && curves == 1u);
    // tan(x) from -5 to 5: broken at its four asymptotes — five pieces, none across.
    plot_kinds("tan(x)\nx -5 5\ny -5 5", &curves, &dashed, &regions);
    printf("  tan(x): %u pieces\n", curves);
    CHECK(curves == 5u);
    // 1/x with y as it goes: two.
    plot_kinds("1/x\nx -5 5\ny -5 5", &curves, &dashed, &regions);
    CHECK(curves == 2u);
    // Marks and the area.
    CHECK(plot_says("x^2 - 1\nroots\nx -3 3", "(-1, 0)\0(1, 0)\0"));
    CHECK(plot_says("x^3 - 3x\nextrema\nx -3 3\ny -4 4", "(-1, 2)\0(1, -2)\0"));
    CHECK(plot_says("y = x\ny = x^2\nintersections\nx -2 2", "(0, 0)\0(1, 1)\0"));
    CHECK(plot_says("x^2\narea 0 3\nx -1 4", "\xE2\x88\xAB[0, 3] = 9\0"));
    CHECK(plot_says("sin(x)\nintegral 0 pi\nx -1 4", "\xE2\x88\xAB[0, 3.14] = 2\0"));
    CHECK(plot_says("(pi, 1)\n(1, 1)\nx -1 4\ny -1 4", "(3.14, 1)\0(1, 1)\0"));   // (a point's numbers as maths writes them)
    // What a line is: f(x) = …, r = …, a bare equation of x and y, a seventh curve (six kept).
    CHECK(plot_says("f(x) = 2x\ng ( x ) = 3\nr = 1\nx^2 + y^2 - 1\nx = y^2", "f(x) = 2x\0g ( x ) = 3\0r = 1\0x^2 + y^2 - 1 = 0\0x = y^2\0"));
    {
        rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
        rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), heap), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), heap);
        CHECK(fude_zoom_plot_lines("graph", "x\n2x\n3x\n4x\n5x\n6x\n(cos(t), sin(t))", 160.0, 110.0, &pts, &parts, NULL) == 6u);
        // A thousand graphs of random pieces of maths: whatever they say, drawn inside their box, nothing past memory.
        static const c8* bits[] = { "x", "y", "t", "r", "=", "<", ">=", "(", ")", ",", "^", "2", "-", "+", "/", "sin", "cos(", "pi", "e",
                                    "\n", " ", "x ", "y ", "-5", "5", "area", "roots", "extrema", "intersections", "a", "a = 1..3", "θ", "|", "sqrt(" };
        u32 seed = 12345u;
        b8 inside = true;
        for(u32 g = 0; g < 1000u; g++) {
            c8 text[200] = { 0 };
            usize n = 0;
            const u32 count = 1u + (seed % 24u);
            for(u32 k = 0; k < count; k++) {
                seed = seed * 1664525u + 1013904223u;
                const c8* b = bits[(seed >> 8) % (sizeof(bits) / sizeof(bits[0]))];
                const usize bl = strlen(b);
                if(n + bl + 1u >= sizeof(text)) break;
                memcpy(text + n, b, bl);
                n += bl;
            }
            rde_arr_clear(&pts); rde_arr_clear(&parts);
            fude_zoom_plot_lines("graph", text, 160.0, 110.0, &pts, &parts, NULL);
            const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
            for(u32 i = 0; i < (u32)rde_arr_length(&pts); i++) inside = inside && fabs(p[i].x) <= 160.0 + 1e-6 && fabs(p[i].y) <= 110.0 + 1e-6;
        }
        CHECK(inside);
        rde_arr_free(&pts); rde_arr_free(&parts);
    }
}

// Mechanisms: what hinges where, a motor's speed, gears that touch and their ratio, a spring's ends, ropes over a
// pulley, a slider's rail and a rack's mesh.
static void test_mechanisms(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    // A crank on a motor at (0,0), a link pinned to its other end, a fixed pivot under the link's far end.
    const u32 motor = part_put(&s, "drive motor", 0, 0, 30, 30, "60 rpm");
    const u32 crank = part_put(&s, "link", 48, 0, 60, 12, "");          // (its holes at 0 and 96)
    const u32 rod   = part_put(&s, "link", 96 + 48, 0, 60, 12, "");     // (its holes at 96 and 192)
    const u32 piv   = part_put(&s, "fixed pivot", 192, -10, 20, 20, "");  // (its hole at y = -10 + 10 = 0)
    RDE_UNUSED(motor); RDE_UNUSED(crank); RDE_UNUSED(rod); RDE_UNUSED(piv);
    fude_zoom_mech_plan p; fude_zoom_mech_plan_init(&p);
    CHECK(fude_zoom_mech_plan_build(&p, &s, NULL) == 4u);
    const fude_zoom_mech_hinge* h = (const fude_zoom_mech_hinge*)p.hinges.memory;
    u32 grounds = 0, between = 0, motors = 0;
    for(u32 i = 0; i < (u32)rde_arr_length(&p.hinges); i++) {
        grounds += h[i].b == FUDE_ZOOM_NONE ? 1u : 0u;
        between += h[i].b != FUDE_ZOOM_NONE ? 1u : 0u;
        if(h[i].motor) { motors++; CHECK(fabs(h[i].speed - 2.0 * 3.14159265358979323846) < 1e-9 && fabs(h[i].at.x) < 1e-9); }
    }
    CHECK(rde_arr_length(&p.hinges) == 3u && grounds == 2u && between == 1u && motors == 1u);
    // Two gears whose pitch circles touch (20T: 50 round, 40T: 100: centres 150 apart), a third apart from them.
    const u32 g1 = part_put(&s, "gear 20T", 0, 500, 55, 55, "20T");
    const u32 g2 = part_put(&s, "gear 40T", 150, 500, 105, 105, "40T");
    part_put(&s, "gear 10T", 0, 900, 30, 30, "10T");
    RDE_UNUSED(g1); RDE_UNUSED(g2);
    fude_zoom_mech_plan_build(&p, &s, NULL);
    CHECK(rde_arr_length(&p.meshes) == 1u);
    CHECK(rde_arr_length(&p.hinges) == 6u);   // (each gear nothing holds on an axle of its own: they turn, they do not fall)
    const fude_zoom_mech_mesh* m = (const fude_zoom_mech_mesh*)p.meshes.memory;
    CHECK(fabs(m[0].ratio - 0.5) < 1e-12);   // (the 20T turning at 2: the 40T at -1, as RDE's gear joint keeps them)
    // A spring from the pivot's hole to a weight's: held by the ground at one end, the weight at the other.
    fude_zoom_scene t; fude_zoom_scene_init(&t, 7);
    part_put(&t, "fixed pivot", 0, -10, 20, 20, "");
    const u32 spring = part_put(&t, "spring", 0 + 45, 0, 50, 12, "");   // (its ends at 0 and 90)
    part_put(&t, "weight", 90, 0, 20, 20, "2 kg");
    fude_zoom_mech_plan_build(&p, &t, NULL);
    CHECK(rde_arr_length(&p.springs) == 1u);
    const fude_zoom_mech_spring* sp = (const fude_zoom_mech_spring*)p.springs.memory;
    CHECK(sp[0].a == FUDE_ZOOM_NONE && sp[0].b != FUDE_ZOOM_NONE && fabs(sp[0].length - 90.0) < 1e-4);
    CHECK(sp[0].body < rde_arr_length(&p.bodies) && ((const fude_zoom_mech_body*)p.bodies.memory)[sp[0].body].object == spring);   // (its own symbol)
    CHECK(fabs(((const fude_zoom_mech_body*)p.bodies.memory)[sp[0].b].value - 2.0) < 1e-12);
    // A pulley (its rope's groove 39 round), a rope down each side of it: a 1 kg weight on one, a 2
    // kg crate on the other.
    fude_zoom_scene u; fude_zoom_scene_init(&u, 7);
    part_put(&u, "pulley", 0, 0, 50, 50, "");
    const u32 r1 = part_put(&u, "rope", -39, -100, 100, 6, "");   // (turned upright below: its ends at y -5 and -195)
    const u32 r2 = part_put(&u, "rope", 39, -100, 100, 6, "");
    for(u32 k = 0; k < 2u; k++) {
        const u32 r = k == 0u ? r1 : r2;
        fude_zoom_place pl = fude_zoom_scene_place_of(&u, r);
        pl.rotation = 1.5707963267948966;
        fude_zoom_scene_set_place(&u, r, pl);
    }
    part_put(&u, "weight", -39, -200, 20, 20, "1 kg");
    part_put(&u, "crate", 39, -205, 25, 20, "2 kg");
    fude_zoom_mech_plan_build(&p, &u, NULL);
    CHECK(rde_arr_length(&p.ropes) == 2u);
    const fude_zoom_mech_rope* ro = (const fude_zoom_mech_rope*)p.ropes.memory;
    const fude_zoom_mech_body* bo = (const fude_zoom_mech_body*)p.bodies.memory;
    CHECK(ro[0].over != FUDE_ZOOM_NONE && ro[0].over == ro[1].over && ro[0].pair == 1u && ro[1].pair == 0u);
    CHECK(ro[0].a != FUDE_ZOOM_NONE && ro[0].b == FUDE_ZOOM_NONE && bo[ro[0].a].part->kind == FUDE_ZOOM_MECH_WEIGHT && fabs(ro[0].length - 190.0) < 1e-4);
    CHECK(ro[1].a != FUDE_ZOOM_NONE && bo[ro[1].a].part->kind == FUDE_ZOOM_MECH_CRATE && fabs(bo[ro[1].a].value - 2.0) < 1e-12);
    // (run as an Atwood machine on RDE's pulley joint, as Play runs it: the mech suite's test_pulleys)
    // A slider on a rail: along the rail, as far as its ends; a 20-tooth gear (pitch radius 50) over a rack: meshed,
    // the rack going on 50 a radian the gear turns anticlockwise (its turn − travel / 50 kept).
    fude_zoom_scene sv; fude_zoom_scene_init(&sv, 7);
    part_put(&sv, "rail", 100, 0, 150, 8, "");
    part_put(&sv, "slider", 60, 0, 30, 18, "");
    part_put(&sv, "gear 20T", 0, 300, 55, 55, "");
    part_put(&sv, "rack", 0, 245, 150, 15, "");   // (its pitch line 5 over its middle: at 250, the gear's pitch circle's bottom)
    fude_zoom_mech_plan_build(&p, &sv, NULL);
    CHECK(rde_arr_length(&p.slides) == 2u);
    const fude_zoom_mech_slide* sl = (const fude_zoom_mech_slide*)p.slides.memory;
    b8 rail_ok = false;
    for(u32 i = 0; i < 2u; i++) {
        if(((const fude_zoom_mech_body*)p.bodies.memory)[sl[i].body].part->kind == FUDE_ZOOM_MECH_SLIDER) {
            rail_ok = fabs(sl[i].axis.x - 1.0) < 1e-9 && fabs(sl[i].lower + 80.0) < 1e-4 && fabs(sl[i].upper - 160.0) < 1e-4;
        }
    }
    CHECK(rail_ok);
    const fude_zoom_mech_mesh* rm = (const fude_zoom_mech_mesh*)p.meshes.memory;
    CHECK(rde_arr_length(&p.meshes) == 1u && rm[0].rack && fabs(rm[0].ratio + 1.0 / 50.0) < 1e-4);
    fude_zoom_scene_destroy(&sv);
    fude_zoom_mech_plan_destroy(&p);
    fude_zoom_scene_destroy(&s);
    fude_zoom_scene_destroy(&t);
    fude_zoom_scene_destroy(&u);
}

// A stroke through _p (its first point its place), closed back to its first when _close.
static u32 poly_stroke(fude_zoom_scene* s, const fude_zoom_v2* p, u32 n, b8 close) {
    const i8  q = fude_zoom_quantum_for(s->camera.z);
    const f64 g = ldexp(1.0, q);
    const u32 m = n + (close ? 1u : 0u);
    rde_arr pts_arr = rde_arr_new(sizeof(fude_zoom_qpoint), rde_memory_allocator_get_default_std());
    rde_arr_resize(&pts_arr, m + 1u);
    fude_zoom_qpoint* pts = (fude_zoom_qpoint*)pts_arr.memory;
    for(u32 i = 0; i < m; i++) {
        const fude_zoom_v2 a = p[i % n];
        pts[i].x = (i32)llround((a.x - p[0].x) / g); pts[i].y = (i32)llround((a.y - p[0].y) / g);
        pts[i].pressure = 600; pts[i].time = i * 4u;
    }
    const u32 o = fude_zoom_scene_add_stroke(s, s->camera.frame, p[0], q, pts, m, FUDE_ZOOM_CHANNEL_PRESSURE | FUDE_ZOOM_CHANNEL_TIME,
                                             (rde_color){ 10, 20, 30, 255 }, 0.5f, FUDE_ZOOM_FLAG_FROM_PEN, 0, 0);
    rde_arr_free(&pts_arr);
    return o;
}

static u32 body_on(fude_zoom_scene* s, u32 target, u32 material, b8 fixed, f64 mass) {
    const fude_zoom_body_props bp = { material, fixed, mass, -1.0, -1.0 };
    return fude_zoom_props_add_body(s, target, &bp);
}

// The plan's bodies of kind _kind.
static u32 bodies_of(const fude_zoom_mech_plan* p, u8 kind, u32* out, u32 max) {
    u32 n = 0;
    const fude_zoom_mech_body* b = (const fude_zoom_mech_body*)p->bodies.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&p->bodies); i++) if(b[i].part->kind == kind && n < max) out[n++] = i;
    return n;
}

// The area of a drawn body's pieces together.
static f64 pieces_area(const fude_zoom_mech_plan* p, const fude_zoom_mech_body* b) {
    const u32* c = (const u32*)p->piece_counts.memory;
    const fude_zoom_v2* q = (const fude_zoom_v2*)p->piece_points.memory;
    u32 first = 0;
    for(u32 k = 0; k < b->piece; k++) first += c[k];
    f64 a = 0.0;
    for(u32 k = b->piece; k < b->piece + b->pieces; k++) {
        fude_sim_v2 v[8];
        for(u32 i = 0; i < c[k] && i < 8u; i++) v[i] = (fude_sim_v2){ q[first + i].x, q[first + i].y };
        CHECK(c[k] >= 3u && c[k] <= 8u && fude_sim_polygon_convex(v, c[k]));
        a += fude_sim_polygon_area(v, c[k]);
        first += c[k];
    }
    return a;
}

// Drawings made bodies (props.h): their properties kept and followed; their outlines; their pieces, centres, masses;
// fixed ground lines; pins joining them; ropes on them; only what is played.
static void test_drawn_bodies(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 0x342F1234u);   // (a tablet's large ids)
    const fude_zoom_v2 sq[4] = { { 0, 0 }, { 40, 0 }, { 40, 40 }, { 0, 40 } };
    const u32 a = poly_stroke(&s, sq, 4u, true);
    // Its outline: closed; an open line's not; an attribute's none.
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    b8 closed = false;
    CHECK(fude_zoom_scene_object_outline(&s, a, 32u, &pts, &closed) && closed && rde_arr_length(&pts) >= 4u);
    const fude_zoom_v2 zig[4] = { { 100, 0 }, { 150, 30 }, { 200, 0 }, { 250, 30 } };
    const u32 line_o = poly_stroke(&s, zig, 4u, false);
    CHECK(fude_zoom_scene_object_outline(&s, line_o, 32u, &pts, &closed) && !closed);
    // Properties: on it, found, read back; on the thing it is made again as; nothing once it is gone.
    const u32 pa = body_on(&s, a, 1u, false, 0.0);   // (steel)
    CHECK(!fude_zoom_scene_object_outline(&s, pa, 32u, &pts, &closed));
    CHECK(fude_zoom_props_find(&s, a, FUDE_ZOOM_PROPS_BODY) == pa && fude_zoom_props_target(&s, pa) == a && fude_zoom_props_kind(&s, pa) == FUDE_ZOOM_PROPS_BODY);
    fude_zoom_body_props bp;
    CHECK(fude_zoom_props_body(&s, pa, &bp) && bp.material == 1u && !bp.fixed && bp.mass == 0.0 && bp.friction < 0.0);
    CHECK(fude_zoom_props_find(&s, line_o, FUDE_ZOOM_PROPS_BODY) == FUDE_ZOOM_NONE && fude_zoom_props_kind(&s, a) == 0u);
    const u32 a2 = poly_stroke(&s, sq, 4u, true);
    const fude_zoom_id ida = fude_zoom_scene_object(&s, a)->id, ida2 = fude_zoom_scene_object(&s, a2)->id, other = 12345u;
    CHECK(fude_zoom_props_remap(&s, pa, &other, &other, 1u) == FUDE_ZOOM_NONE);
    const u32 pa2 = fude_zoom_props_remap(&s, pa, &ida, &ida2, 1u);
    CHECK(pa2 != FUDE_ZOOM_NONE && fude_zoom_props_target(&s, pa2) == a2);
    fude_zoom_scene_set_alive(&s, a2, false);
    CHECK(fude_zoom_props_target(&s, pa2) == FUDE_ZOOM_NONE);
    fude_zoom_scene_set_alive(&s, pa2, false);
    // In the plan: a steel square, 40 × 40, its centre its middle, its mass 7850 kg/m³ × 1600 mm² × 10 mm.
    fude_zoom_mech_plan p; fude_zoom_mech_plan_init(&p);
    fude_zoom_mech_plan_build(&p, &s, NULL);
    u32 drawn[16];
    CHECK(bodies_of(&p, FUDE_ZOOM_MECH_DRAWN, drawn, 16u) == 1u);
    const fude_zoom_mech_body* b = &((const fude_zoom_mech_body*)p.bodies.memory)[drawn[0]];
    CHECK(b->object == a && !b->fixed && fabs(b->at.x - 20.0) < 1e-3 && fabs(b->at.y - 20.0) < 1e-3 && fabs(b->mass - 7850.0 * 1600e-6 * 0.01) < 1e-6);
    CHECK(fabs(pieces_area(&p, b) - 1600.0) < 1e-3 && fabs(b->friction - fude_sim_material_at(1u)->friction) < 1e-12);
    // An L (concave): more than one piece, its area and centre as the L's; fixed; its mass as given.
    fude_zoom_scene_set_alive(&s, a, false);
    const fude_zoom_v2 ell[6] = { { 0, 0 }, { 60, 0 }, { 60, 20 }, { 20, 20 }, { 20, 80 }, { 0, 80 } };
    const u32 l = poly_stroke(&s, ell, 6u, true);
    body_on(&s, l, 0u, true, 3.5);
    fude_zoom_mech_plan_build(&p, &s, NULL);
    CHECK(bodies_of(&p, FUDE_ZOOM_MECH_DRAWN, drawn, 16u) == 1u);
    b = &((const fude_zoom_mech_body*)p.bodies.memory)[drawn[0]];
    fude_sim_v2 lv[6]; for(u32 i = 0; i < 6u; i++) lv[i] = (fude_sim_v2){ ell[i].x, ell[i].y };
    const fude_sim_v2 lc = fude_sim_polygon_centroid(lv, 6u);
    CHECK(b->pieces >= 2u && b->fixed && b->mass == 3.5 && fabs(pieces_area(&p, b) - 2400.0) < 1e-2 && hypot(b->at.x - lc.x, b->at.y - lc.y) < 1e-3);
    // An open line made a body: fixed ground, a thin piece along each stretch.
    body_on(&s, line_o, 4u, false, 0.0);
    fude_zoom_mech_plan_build(&p, &s, NULL);
    CHECK(bodies_of(&p, FUDE_ZOOM_MECH_DRAWN, drawn, 16u) == 2u);
    const fude_zoom_mech_body* lb = ((const fude_zoom_mech_body*)p.bodies.memory)[drawn[0]].object == line_o ? &((const fude_zoom_mech_body*)p.bodies.memory)[drawn[0]] : &((const fude_zoom_mech_body*)p.bodies.memory)[drawn[1]];
    CHECK(lb->object == line_o && lb->fixed && lb->pieces == 3u);
    // A figure eight (it crosses itself): its box, one piece.
    fude_zoom_scene_set_alive(&s, l, false);
    fude_zoom_scene_set_alive(&s, line_o, false);
    const fude_zoom_v2 bow[4] = { { 300, 0 }, { 340, 40 }, { 340, 0 }, { 300, 40 } };
    const u32 bw = poly_stroke(&s, bow, 4u, true);
    body_on(&s, bw, 2u, false, 0.0);
    fude_zoom_mech_plan_build(&p, &s, NULL);
    CHECK(bodies_of(&p, FUDE_ZOOM_MECH_DRAWN, drawn, 16u) == 1u);
    b = &((const fude_zoom_mech_body*)p.bodies.memory)[drawn[0]];
    CHECK(b->pieces >= 1u && pieces_area(&p, b) > 0.0);
    fude_zoom_scene_set_alive(&s, bw, false);
    // Pins: two squares overlapping, a pin where they do: hinged together; a pin on one alone: to the ground; a pin on
    // one over a wall: to the ground; a pin on nothing: nothing.
    const fude_zoom_v2 s1[4] = { { 1000, 0 }, { 1040, 0 }, { 1040, 40 }, { 1000, 40 } }, s2[4] = { { 1030, 0 }, { 1070, 0 }, { 1070, 40 }, { 1030, 40 } };
    const u32 q1 = poly_stroke(&s, s1, 4u, true), q2 = poly_stroke(&s, s2, 4u, true);
    body_on(&s, q1, 0u, false, 0.0); body_on(&s, q2, 0u, false, 0.0);
    part_put(&s, "pin", 1035, 20, 8, 8, "");
    fude_zoom_mech_plan_build(&p, &s, NULL);
    const fude_zoom_mech_hinge* h = (const fude_zoom_mech_hinge*)p.hinges.memory;
    const fude_zoom_mech_body* pb = (const fude_zoom_mech_body*)p.bodies.memory;
    CHECK(rde_arr_length(&p.hinges) == 1u && h[0].b != FUDE_ZOOM_NONE && pb[h[0].a].part->kind == FUDE_ZOOM_MECH_DRAWN && pb[h[0].b].part->kind == FUDE_ZOOM_MECH_DRAWN &&
          fabs(h[0].at.x - 1035.0) < 1e-6);
    part_put(&s, "pin", 1005, 20, 8, 8, "");   // (on the first alone: a nail)
    part_put(&s, "pin", 5000, 20, 8, 8, "");   // (on nothing)
    fude_zoom_mech_plan_build(&p, &s, NULL);
    h = (const fude_zoom_mech_hinge*)p.hinges.memory;
    u32 to_ground = 0;
    for(u32 i = 0; i < (u32)rde_arr_length(&p.hinges); i++) to_ground += h[i].b == FUDE_ZOOM_NONE ? 1u : 0u;
    CHECK(rde_arr_length(&p.hinges) == 2u && to_ground == 1u);
    // A pin through twenty squares overlapping (more than the sixteen a pin once joined, past which the rest were left
    // loose): each hinged to the first there, none to the ground.
    {
        fude_zoom_scene r20; fude_zoom_scene_init(&r20, 9);
        for(u32 i = 0; i < 20u; i++) {
            const f64 x = 2.0 * (f64)i;
            const fude_zoom_v2 sq[4] = { { x, 0 }, { x + 60, 0 }, { x + 60, 40 }, { x, 40 } };
            body_on(&r20, poly_stroke(&r20, sq, 4u, true), 0u, false, 0.0);
        }
        part_put(&r20, "pin", 50, 20, 8, 8, "");
        fude_zoom_mech_plan p20; fude_zoom_mech_plan_init(&p20);
        fude_zoom_mech_plan_build(&p20, &r20, NULL);
        const fude_zoom_mech_hinge* h20 = (const fude_zoom_mech_hinge*)p20.hinges.memory;
        u32 joined = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&p20.hinges); i++) joined += h20[i].a == h20[0].a && h20[i].b != FUDE_ZOOM_NONE && h20[i].b != h20[0].a ? 1u : 0u;
        CHECK(rde_arr_length(&p20.hinges) == 19u && joined == 19u);
        fude_zoom_mech_plan_destroy(&p20);
        fude_zoom_scene_destroy(&r20);
    }
    // Shapes made bodies (a rectangle, an ellipse, a polygon): closed, moving; a bar pinned near one end: hinged to the
    // ground there (a pendulum), still moving.
    {
        fude_zoom_scene r3; fude_zoom_scene_init(&r3, 9);
        const f64 rect[3] = { 70.0, 8.0, 0.0 }, ell[2] = { 26.0, 26.0 }, poly[6] = { -20, -10, 20, -10, 0, 25 };
        const u32 sr = fude_zoom_scene_add_shape(&r3, r3.root, (fude_zoom_place){ { 40, 240 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, rect, 3u, (rde_color){ 1, 1, 1, 255 }, 1.5f, 0u, 0);
        const u32 se = fude_zoom_scene_add_shape(&r3, r3.root, (fude_zoom_place){ { -265, 200 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, ell, 2u, (rde_color){ 1, 1, 1, 255 }, 1.5f, 0u, 0);
        const u32 sp = fude_zoom_scene_add_shape(&r3, r3.root, (fude_zoom_place){ { 400, 200 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_POLYGON, poly, 6u, (rde_color){ 1, 1, 1, 255 }, 1.5f, 0u, 0);
        rde_arr outl = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
        b8 cl_r = false, cl_e = false, cl_p = false;
        CHECK(fude_zoom_scene_object_outline(&r3, sr, 32u, &outl, &cl_r) && fude_zoom_scene_object_outline(&r3, se, 32u, &outl, &cl_e) &&
              fude_zoom_scene_object_outline(&r3, sp, 32u, &outl, &cl_p));
        CHECK(cl_r && cl_e && cl_p);
        rde_arr_free(&outl);
        body_on(&r3, sr, 2u, false, 0.0); body_on(&r3, se, 1u, false, 0.0); body_on(&r3, sp, 0u, false, 0.0);
        part_put(&r3, "pin", -26, 240, 8, 8, "");
        fude_zoom_mech_plan p3; fude_zoom_mech_plan_init(&p3);
        fude_zoom_mech_plan_build(&p3, &r3, NULL);
        u32 d3[8];
        CHECK(bodies_of(&p3, FUDE_ZOOM_MECH_DRAWN, d3, 8u) == 3u);
        const fude_zoom_mech_body* b3 = (const fude_zoom_mech_body*)p3.bodies.memory;
        u32 moving = 0;
        for(u32 i = 0; i < 3u; i++) moving += !b3[d3[i]].fixed;
        CHECK(moving == 3u);
        const fude_zoom_mech_hinge* h3 = (const fude_zoom_mech_hinge*)p3.hinges.memory;
        CHECK(rde_arr_length(&p3.hinges) == 1u && h3[0].b == FUDE_ZOOM_NONE && b3[h3[0].a].object == sr && fabs(h3[0].at.x + 26.0) < 1e-6);
        fude_zoom_mech_plan_destroy(&p3);
        fude_zoom_scene_destroy(&r3);
    }
    // A rope from a pivot's pin down to the first square: held by it there.
    part_put(&s, "fixed pivot", 1020, 190, 20, 20, "");   // (its pin at 200)
    const u32 r = part_put(&s, "rope", 1020, 110, 95, 6, "");   // (turned upright: its ends at 20 and 200)
    fude_zoom_place pl = fude_zoom_scene_place_of(&s, r); pl.rotation = 1.5707963267948966; fude_zoom_scene_set_place(&s, r, pl);
    fude_zoom_mech_plan_build(&p, &s, NULL);
    pb = (const fude_zoom_mech_body*)p.bodies.memory;
    const fude_zoom_mech_rope* ro = (const fude_zoom_mech_rope*)p.ropes.memory;
    CHECK(rde_arr_length(&p.ropes) == 1u && ((ro[0].a != FUDE_ZOOM_NONE && pb[ro[0].a].object == q1 && ro[0].b == FUDE_ZOOM_NONE) ||
                                             (ro[0].b != FUDE_ZOOM_NONE && pb[ro[0].b].object == q1 && ro[0].a == FUDE_ZOOM_NONE)));
    // Only what is played: the first square lassoed (its properties not): the second not in it.
    u8 scope[512] = { 0 };
    scope[q1] = 1u;
    fude_zoom_mech_plan_build(&p, &s, scope);
    CHECK(bodies_of(&p, FUDE_ZOOM_MECH_DRAWN, drawn, 16u) == 1u && ((const fude_zoom_mech_body*)p.bodies.memory)[drawn[0]].object == q1);
    // Random shapes drawn (stars of 3 to 60 corners, anywhere, any size): one body each, its pieces convex, at most
    // eight corners, as many as a body holds, their area within 3 % of the drawing's, its centre near the drawing's.
    u32 shapes = 0, shapes_right = 0;
    for(u32 k = 0; k < 600u; k++) {
        fude_zoom_scene r2; fude_zoom_scene_init(&r2, 7);
        const u32 n = 3u + (u32)(rndf() * 58.0);
        const f64 R = 5.0 + rndf() * 500.0, cx = (rndf() - 0.5) * 1e4, cy = (rndf() - 0.5) * 1e4;
        fude_zoom_v2 st[64]; fude_sim_v2 sv[64];
        for(u32 i = 0; i < n; i++) {
            const f64 ang = ((f64)i + 0.05 + 0.9 * rndf()) * 6.283185307179586 / (f64)n, rr = R * (0.3 + 0.7 * rndf());
            st[i] = (fude_zoom_v2){ cx + cos(ang) * rr, cy + sin(ang) * rr };
            sv[i] = (fude_sim_v2){ st[i].x, st[i].y };
        }
        const u32 o = poly_stroke(&r2, st, n, true);
        body_on(&r2, o, (u32)(rndf() * 8.0) % 8u, false, 0.0);
        fude_zoom_mech_plan pr; fude_zoom_mech_plan_init(&pr);
        fude_zoom_mech_plan_build(&pr, &r2, NULL);
        u32 d[4];
        b8 ok = bodies_of(&pr, FUDE_ZOOM_MECH_DRAWN, d, 4u) == 1u;
        if(ok) {
            const fude_zoom_mech_body* bb = &((const fude_zoom_mech_body*)pr.bodies.memory)[d[0]];
            const f64 area = fude_sim_polygon_area(sv, n), got = pieces_area(&pr, bb);
            const fude_sim_v2 c = fude_sim_polygon_centroid(sv, n);
            ok = bb->pieces >= 1u && bb->pieces <= RDE_PHYSICS_2D_MAX_COMPOUND_SHAPES + 1u && fabs(got - area) <= 0.03 * area && hypot(bb->at.x - c.x, bb->at.y - c.y) <= 0.03 * R && bb->mass > 0.0;
            if(!ok) printf("  star %u (%u corners): %u pieces, area %.1f of %.1f, centre %.2f off\n", k, n, bb->pieces, got, area, hypot(bb->at.x - c.x, bb->at.y - c.y));
        }
        shapes++;
        shapes_right += ok ? 1u : 0u;
        fude_zoom_mech_plan_destroy(&pr);
        fude_zoom_scene_destroy(&r2);
    }
    printf("  drawn bodies: %u random drawings\n", shapes);
    CHECK(shapes_right == shapes);
    rde_arr_free(&pts);
    fude_zoom_mech_plan_destroy(&p);
    fude_zoom_scene_destroy(&s);
}

// Constraints: a line kept parallel / square / the same length as another, round its middle, as the other turns.
static f64 line_angle(const fude_zoom_scene* s, u32 o, f64* len, fude_zoom_v2* mid) {
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    b8 closed;
    fude_zoom_scene_shape_outline(s, o, 1u, &pts, &closed);
    const fude_zoom_v2 a = ((const fude_zoom_v2*)pts.memory)[0], b = ((const fude_zoom_v2*)pts.memory)[1];
    rde_arr_free(&pts);
    if(len != NULL) *len = hypot(b.x - a.x, b.y - a.y);
    if(mid != NULL) *mid = (fude_zoom_v2){ (a.x + b.x) * 0.5, (a.y + b.y) * 0.5 };
    return atan2(b.y - a.y, b.x - a.x);
}

static void test_constraints(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const rde_color c = { 1, 1, 1, 255 };
    const u32 a = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, (const f64[2]){ 100, 0 }, 2u, c, 0.5f, 0u, 0);
    const u32 b = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 50 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, (const f64[2]){ 60.0 * cos(0.5), 60.0 * sin(0.5) }, 2u, c, 0.5f, 0u, 0);
    fude_zoom_v2 mid0, mid1;
    f64 len;
    line_angle(&s, b, NULL, &mid0);
    // Made parallel: turned round its middle onto the first's way.
    fude_zoom_sim m;
    CHECK(fude_zoom_connect_constrain(&s, FUDE_ZOOM_CONSTRAINT_PARALLEL, a, b, &m));
    fude_zoom_scene_set_place(&s, b, fude_zoom_place_moved(fude_zoom_scene_place_of(&s, b), m));
    CHECK(fabs(line_angle(&s, b, &len, &mid1)) < 1e-9 && fabs(len - 60.0) < 1e-9 && fabs(mid1.x - mid0.x) < 1e-9 && fabs(mid1.y - mid0.y) < 1e-9);
    CHECK(!fude_zoom_connect_constrain(&s, FUDE_ZOOM_CONSTRAINT_PARALLEL, a, b, &m));   // (so already)
    // Kept so: the first turned a quarter (moved), the second follows in the same step.
    const u32 k = fude_zoom_connect_constraint_add(&s, FUDE_ZOOM_CONSTRAINT_PARALLEL, a, b);
    u8 kind; u32 ka, kb;
    CHECK(fude_zoom_connect_constraint_of(&s, k, &kind, &ka, &kb) && kind == FUDE_ZOOM_CONSTRAINT_PARALLEL && ka == a && kb == b);
    const f64 q = 3.14159265358979323846 * 0.5;
    fude_zoom_scene_set_place(&s, a, (fude_zoom_place){ { 0, 0 }, q, 1.0 });
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    rde_arr objs = rde_arr_new(sizeof(u32), heap), before = rde_arr_new(sizeof(fude_zoom_place), heap), after = rde_arr_new(sizeof(fude_zoom_place), heap);
    CHECK(fude_zoom_connect_follow(&s, s.root, &a, 1u, &objs, &before, &after) == 1u && ((const u32*)objs.memory)[0] == b);
    CHECK(fabs(fabs(line_angle(&s, b, NULL, &mid1)) - q) < 1e-9 && fabs(mid1.x - mid0.x) < 1e-9);
    // The same length: stretched round its middle.
    CHECK(fude_zoom_connect_constrain(&s, FUDE_ZOOM_CONSTRAINT_EQUAL, a, b, &m));
    fude_zoom_scene_set_place(&s, b, fude_zoom_place_moved(fude_zoom_scene_place_of(&s, b), m));
    line_angle(&s, b, &len, NULL);
    CHECK(fabs(len - 100.0) < 1e-9);
    // Square: a quarter from the first's way.
    CHECK(fude_zoom_connect_constrain(&s, FUDE_ZOOM_CONSTRAINT_SQUARE, a, b, &m));
    fude_zoom_scene_set_place(&s, b, fude_zoom_place_moved(fude_zoom_scene_place_of(&s, b), m));
    CHECK(fabs(sin(line_angle(&s, b, NULL, NULL))) < 1e-9);
    rde_arr_free(&objs); rde_arr_free(&before); rde_arr_free(&after);
    fude_zoom_scene_destroy(&s);
}

// --- following many moved things (Phase 4, 2026-10-15) -----------------------------------------------------------
// fude_zoom_connect_follow as it was before: every object of the scene gone through for the frame's, each one's moved
// ends searched for in the list of what moved, every object again on each of the constraints' passes, and _objects
// searched from the front for a line followed again. The reference the new one (the frame's own objects, a byte an
// object, where each is in _objects) must equal exactly. Its helpers are connect.c's, copied (unchanged there).
typedef struct { u8 type; f64 n[FUDE_ZOOM_DIMENSION_REFS]; u32 count; fude_zoom_id id[2]; i32 key[2]; } cfr_dim;
static b8 cfr_dim_of(const fude_zoom_scene* s, u32 o, cfr_dim* d) {
    const fude_zoom_object* ob = fude_zoom_scene_object(s, o);
    if(!(ob->flags & FUDE_ZOOM_FLAG_ALIVE) || ob->kind != FUDE_ZOOM_KIND_SHAPE || (ob->channels != FUDE_ZOOM_SHAPE_DIMENSION && ob->channels != FUDE_ZOOM_SHAPE_RADIAL)) return false;
    memset(d, 0, sizeof *d); d->type = ob->channels; d->count = fude_zoom_scene_shape_numbers(s, o, d->n, FUDE_ZOOM_DIMENSION_REFS);
    if(d->type == FUDE_ZOOM_SHAPE_DIMENSION) return fude_zoom_connect_dimension_ends(d->n, d->count, d->id, d->key) && (d->id[0] != 0 || d->id[1] != 0);
    d->id[0] = fude_zoom_connect_radial_id(d->n, d->count); return d->id[0] != 0;
}
static b8 cfr_two(fude_zoom_v2 a, fude_zoom_v2 b, fude_zoom_v2 a2, fude_zoom_v2 b2, fude_zoom_sim* m) {
    const f64 old = rde_math_hypot_f64(b.x - a.x, b.y - a.y), nw = rde_math_hypot_f64(b2.x - a2.x, b2.y - a2.y);
    if(!(old > 0.0) || !(nw > 0.0)) return false;
    const f64 turn = rde_math_atan2_f64(b2.y - a2.y, b2.x - a2.x) - rde_math_atan2_f64(b.y - a.y, b.x - a.x), sc = nw / old;
    const f64 ca = rde_math_cos_f64(turn) * sc, sa = rde_math_sin_f64(turn) * sc;
    *m = (fude_zoom_sim){ ca, sa, a2.x - (ca * a.x - sa * a.y), a2.y - (sa * a.x + ca * a.y) };
    return true;
}
// (connect.c's dim_now, dim_ends and two in one)
static b8 cfr_dim_move(const fude_zoom_scene* s, u32 o, const cfr_dim* d, const u32* objs, fude_zoom_sim* m) {
    const fude_zoom_sim sim = fude_zoom_object_sim(fude_zoom_scene_object(s, o));
    const fude_zoom_v2 a = fude_zoom_sim_apply(sim, (fude_zoom_v2){ 0.0, 0.0 });
    const fude_zoom_v2 b = d->type == FUDE_ZOOM_SHAPE_RADIAL ? fude_zoom_sim_apply(sim, (fude_zoom_v2){ rde_math_cos_f64(d->n[1]) * d->n[0], rde_math_sin_f64(d->n[1]) * d->n[0] })
                                                             : fude_zoom_sim_apply(sim, (fude_zoom_v2){ d->n[0], d->n[1] });
    fude_zoom_v2 a2 = a, b2 = b;
    if(d->type == FUDE_ZOOM_SHAPE_RADIAL) {
        fude_zoom_v2 c; f64 r; const f64 was = rde_math_hypot_f64(b.x - a.x, b.y - a.y);
        if(!fude_zoom_connect_round(s, objs[0], &c, &r) || !(was > 0.0)) return false;
        a2 = c; b2 = (fude_zoom_v2){ c.x + (b.x - a.x) * r / was, c.y + (b.y - a.y) * r / was };
    } else {
        const b8 ka = objs[0] != FUDE_ZOOM_NONE && fude_zoom_connect_point(s, objs[0], d->key[0], &a2);
        const b8 kb = objs[1] != FUDE_ZOOM_NONE && fude_zoom_connect_point(s, objs[1], d->key[1], &b2);
        if(!ka && !kb) return false;
    }
    return cfr_two(a, b, a2, b2, m);
}
static void cfr_add(fude_zoom_scene* s, u32 i, fude_zoom_sim m, rde_arr* objects, rde_arr* before, rde_arr* after) {
    const fude_zoom_place place = fude_zoom_scene_place_of(s, i), next = fude_zoom_place_moved(place, m);
    rde_arr_add(objects, &i); rde_arr_add(before, (any)&place); rde_arr_add(after, (any)&next);
    fude_zoom_scene_set_place(s, i, next);
}
static u32 cfr_follow(fude_zoom_scene* s, u32 frame, const u32* moved, u32 count, rde_arr* objects, rde_arr* before, rde_arr* after) {
    if(count == 0) return 0;
    u32 made = 0;
    for(u32 i = 0; i < fude_zoom_scene_object_count(s); i++) {
        if(fude_zoom_scene_object(s, i)->frame != frame) continue;
        cfr_dim dim;
        if(cfr_dim_of(s, i, &dim)) {
            const u32 objs[2] = { dim.id[0] != 0 ? fude_zoom_scene_find_object(s, dim.id[0]) : FUDE_ZOOM_NONE, dim.id[1] != 0 ? fude_zoom_scene_find_object(s, dim.id[1]) : FUDE_ZOOM_NONE };
            b8 touched = false, self = false;
            for(u32 k = 0; k < count; k++) { touched = touched || (objs[0] != FUDE_ZOOM_NONE && moved[k] == objs[0]) || (objs[1] != FUDE_ZOOM_NONE && moved[k] == objs[1]); self = self || moved[k] == i; }
            fude_zoom_sim m;
            if(touched && !self && cfr_dim_move(s, i, &dim, objs, &m)) { cfr_add(s, i, m, objects, before, after); made++; }
            continue;
        }
        const fude_zoom_object* ob = fude_zoom_scene_object(s, i);
        if(ob->kind != FUDE_ZOOM_KIND_SHAPE || ob->channels != FUDE_ZOOM_SHAPE_ARROW || !(ob->flags & FUDE_ZOOM_FLAG_ALIVE)) continue;
        f64 n[FUDE_ZOOM_SHAPE_NUMBERS]; const u32 nc = fude_zoom_scene_shape_numbers(s, i, n, FUDE_ZOOM_SHAPE_NUMBERS), k = fude_zoom_arrow_points(n, nc);
        fude_zoom_id from, to;
        if(k < 2u || !fude_zoom_connect_arrow_ends(n, nc, k, &from, &to) || (from == 0 && to == 0)) continue;
        const u32 ao = from != 0 ? fude_zoom_scene_find_object(s, from) : FUDE_ZOOM_NONE, bo = to != 0 ? fude_zoom_scene_find_object(s, to) : FUDE_ZOOM_NONE;
        b8 touched = false, self = false;
        for(u32 q = 0; q < count; q++) { touched = touched || moved[q] == ao || moved[q] == bo; self = self || moved[q] == i; }
        if(!touched || self) continue;
        const fude_zoom_sim sim = fude_zoom_object_sim(ob);
        const fude_zoom_v2 a = fude_zoom_sim_apply(sim, (fude_zoom_v2){ n[1], n[2] }), b = fude_zoom_sim_apply(sim, (fude_zoom_v2){ n[2u * k - 1u], n[2u * k] });
        const b8 a_ok = ao != FUDE_ZOOM_NONE && (fude_zoom_scene_object(s, ao)->flags & FUDE_ZOOM_FLAG_ALIVE) && fude_zoom_scene_object(s, ao)->frame == frame;
        const b8 b_ok = bo != FUDE_ZOOM_NONE && (fude_zoom_scene_object(s, bo)->flags & FUDE_ZOOM_FLAG_ALIVE) && fude_zoom_scene_object(s, bo)->frame == frame;
        const fude_zoom_v2 am = a_ok ? fude_zoom_connect_middle(s, ao) : a, bm = b_ok ? fude_zoom_connect_middle(s, bo) : b;
        const fude_zoom_v2 a2 = a_ok ? fude_zoom_connect_edge(s, ao, bm) : a, b2 = b_ok ? fude_zoom_connect_edge(s, bo, am) : b;
        fude_zoom_sim m;
        if(cfr_two(a, b, a2, b2, &m)) { cfr_add(s, i, m, objects, before, after); made++; }
    }
    rde_arr now = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    memcpy(rde_arr_add_n(&now, count), moved, (size_t)count * sizeof(u32));
    for(u32 pass = 0; pass < 6u; pass++) {
        b8 changed = false;
        for(u32 i = 0; i < fude_zoom_scene_object_count(s); i++) {
            u8 kind; u32 a, b;
            if(fude_zoom_scene_object(s, i)->frame != frame || !fude_zoom_connect_constraint_of(s, i, &kind, &a, &b) || a == FUDE_ZOOM_NONE || b == FUDE_ZOOM_NONE) continue;
            b8 am = false, bm = false;
            for(u32 q = 0; q < (u32)rde_arr_length(&now); q++) { am = am || ((const u32*)now.memory)[q] == a; bm = bm || ((const u32*)now.memory)[q] == b; }
            fude_zoom_sim m;
            if(am == bm || !fude_zoom_connect_constrain(s, kind, am ? a : b, am ? b : a, &m)) continue;
            const u32 other = am ? b : a;
            const fude_zoom_place place = fude_zoom_scene_place_of(s, other), next = fude_zoom_place_moved(place, m);
            u32 at = FUDE_ZOOM_NONE;
            for(u32 q = 0; q < (u32)rde_arr_length(objects); q++) if(((const u32*)objects->memory)[q] == other) at = q;
            if(at != FUDE_ZOOM_NONE) ((fude_zoom_place*)after->memory)[at] = next;
            else { rde_arr_add(objects, (any)&other); rde_arr_add(before, (any)&place); rde_arr_add(after, (any)&next); made++; }
            fude_zoom_scene_set_place(s, other, next);
            rde_arr_add(&now, (any)&other);
            changed = true;
        }
        if(!changed) break;
    }
    rde_arr_free(&now);
    return made;
}
// A scene of _n things for seed _seed, the same every time: rectangles, circles and lines, most in the root and some in
// a child frame (made in between, so the root's objects are not one run of indexes); connectors between any two (some
// loose at one end, some to a thing in the other frame, some to a thing erased after); dimensions on their corners,
// ends and centres; RADIALs on circles; constraints (parallel, square, equal) along chains of lines, some longer than
// six, some to a line in the other frame. What may move goes into *_things.
static void cf_build(fude_zoom_scene* s, u32 seed, u32 n, u32* child, rde_arr* things) {
    rng = seed;
    fude_zoom_scene_init(s, 7);
    const rde_color c = { 1, 1, 1, 255 };
    *child = fude_zoom_scene_new_frame(s, s->root, (fude_zoom_xform){ 4000, 0, 1.0, 0.0 }, (fude_zoom_box){ -500, -500, 500, 500 });
    rde_arr_clear(things);
    rde_arr lines = rde_arr_new(sizeof(u32), NULL), circles = rde_arr_new(sizeof(u32), NULL);
    for(u32 i = 0; i < n; i++) {
        const u32 f = rnd() % 5u == 0 ? *child : s->root;
        const fude_zoom_place p = { { rndf() * 3000.0, rndf() * 3000.0 }, rndf() * 3.0, 1.0 };
        const u32 kind = rnd() % 4u;
        const f64 box[3] = { 10.0 + rndf() * 40.0, 10.0 + rndf() * 40.0, 0 }, ell[2] = { 5.0 + rndf() * 20.0, 5.0 + rndf() * 20.0 }, ln[2] = { 20.0 + rndf() * 80.0, rndf() * 40.0 - 20.0 };
        u32 o;
        if(kind < 2u) o = fude_zoom_scene_add_shape(s, f, p, FUDE_ZOOM_SHAPE_RECT, box, 3u, c, 1.0f, 0u, 0);
        else if(kind == 2u) { o = fude_zoom_scene_add_shape(s, f, p, FUDE_ZOOM_SHAPE_ELLIPSE, ell, 2u, c, 1.0f, 0u, 0); rde_arr_add(&circles, &o); }
        else { o = fude_zoom_scene_add_shape(s, f, p, FUDE_ZOOM_SHAPE_LINE, ln, 2u, c, 0.5f, 0u, 0); rde_arr_add(&lines, &o); }
        rde_arr_add(things, &o);
    }
    const u32 nt = (u32)rde_arr_length(things), nl = (u32)rde_arr_length(&lines), ncir = (u32)rde_arr_length(&circles);
    const u32* t = (const u32*)things->memory; const u32* l = (const u32*)lines.memory;
    for(u32 i = 0; i < n / 2u; i++) {
        const u32 a = t[rnd() % nt], b = rnd() % 9u == 0 ? FUDE_ZOOM_NONE : t[rnd() % nt];
        const u32 f = rnd() % 6u == 0 ? *child : s->root;
        fude_zoom_connect_add(s, f, fude_zoom_connect_middle(s, a), b != FUDE_ZOOM_NONE ? fude_zoom_connect_middle(s, b) : (fude_zoom_v2){ rndf() * 3000.0, 0 }, a, b, FUDE_ZOOM_ARROW_END, c, 0.5f);
        if(i % 7u == 0) { const u32 a2 = t[rnd() % nt]; fude_zoom_connect_add(s, s->root, (fude_zoom_v2){ 0, 0 }, (fude_zoom_v2){ 50, 50 }, FUDE_ZOOM_NONE, a2, 0u, c, 0.5f); }
    }
    const i32 keys[4] = { 0, 1, 2, FUDE_ZOOM_SNAP_CENTRE_KEY };
    for(u32 i = 0; i < n / 4u; i++) {
        const u32 a = t[rnd() % nt], b = t[rnd() % nt], f = fude_zoom_scene_object(s, a)->frame;
        fude_zoom_v2 pa, pb; const i32 ka = keys[rnd() % 4u], kb = keys[rnd() % 4u];
        if(!fude_zoom_connect_point(s, a, ka, &pa)) pa = fude_zoom_connect_middle(s, a);
        if(!fude_zoom_connect_point(s, b, kb, &pb)) pb = (fude_zoom_v2){ pa.x + 30.0, pa.y };
        fude_zoom_connect_dimension(s, f, pa, pb, -5.0, a, ka, b, kb, c, 0.5f);
    }
    for(u32 i = 0; i < ncir && i < n / 8u; i++) {
        const u32 ci = ((const u32*)circles.memory)[rnd() % ncir];
        f64 rn[FUDE_ZOOM_RADIAL_NUMBERS] = { 10.0 + rndf() * 10.0, rndf() * 6.0, 1.0, 0.0, 0.0 };
        fude_zoom_id_put(&rn[3], fude_zoom_scene_object(s, ci)->id);
        fude_zoom_scene_add_shape(s, fude_zoom_scene_object(s, ci)->frame, fude_zoom_scene_place_of(s, ci), FUDE_ZOOM_SHAPE_RADIAL, rn, FUDE_ZOOM_RADIAL_NUMBERS, c, 0.5f, 0u, 0);
    }
    for(u32 i = 0; i + 1u < nl; i++) {
        const u32 run = 1u + rnd() % 9u;   // chains up to nine lines long
        for(u32 j = 0; j < run && i + 1u < nl; j++, i++) fude_zoom_connect_constraint_add(s, (u8)(rnd() % 3u), l[i], rnd() % 13u == 0 ? l[rnd() % nl] : l[i + 1u]);
    }
    for(u32 i = 0; i < nt / 40u; i++) fude_zoom_scene_set_alive(s, t[rnd() % nt], false);   // (joined to things gone)
    rde_arr_free(&lines); rde_arr_free(&circles);
}
// The moves for seed _seed: about _share of the things (and now and then a connector, or a thing twice), each turned
// and slid a little, applied to _s; the moved into _moved.
static void cf_move(fude_zoom_scene* s, u32 seed, const rde_arr* things, u32 share, rde_arr* moved) {
    rng = seed * 7u + 1u;
    rde_arr_clear(moved);
    const u32 nt = (u32)rde_arr_length(things);
    for(u32 i = 0; i < nt; i++) {
        if(rnd() % share != 0) continue;
        u32 o = ((const u32*)things->memory)[i];
        if(rnd() % 50u == 0 && o + 1u < fude_zoom_scene_object_count(s)) o = fude_zoom_scene_object_count(s) - 1u - rnd() % 20u;   // (something else: a connector, a constraint)
        const f64 ang = rndf() * 0.6 - 0.3;
        const fude_zoom_sim m = { cos(ang), sin(ang), rndf() * 200.0 - 100.0, rndf() * 200.0 - 100.0 };
        fude_zoom_scene_set_place(s, o, fude_zoom_place_moved(fude_zoom_scene_place_of(s, o), m));
        rde_arr_add(moved, &o);
        if(rnd() % 40u == 0) rde_arr_add(moved, &o);
    }
}
// Follows the moves in both frames as the lasso does (the moved first in _objects), the new way or the reference.
static u32 cf_follow(fude_zoom_scene* s, u32 child, const rde_arr* moved, rde_arr* objs, rde_arr* bef, rde_arr* aft, b8 reference) {
    rde_arr_clear(objs); rde_arr_clear(bef); rde_arr_clear(aft);
    const u32 n = (u32)rde_arr_length(moved); const u32* m = (const u32*)moved->memory;
    for(u32 i = 0; i < n; i++) { const fude_zoom_place p = fude_zoom_scene_place_of(s, m[i]); rde_arr_add(objs, (any)&m[i]); rde_arr_add(bef, (any)&p); rde_arr_add(aft, (any)&p); }
    const u32 frames[2] = { s->root, child };
    u32 made = 0;
    for(u32 k = 0; k < 2u; k++) made += reference ? cfr_follow(s, frames[k], m, n, objs, bef, aft) : fude_zoom_connect_follow(s, frames[k], m, n, objs, bef, aft);
    return made;
}
static b8 cf_same_arr(const rde_arr* a, const rde_arr* b) {
    return rde_arr_length(a) == rde_arr_length(b) && (rde_arr_length(a) == 0 || memcmp(a->memory, b->memory, rde_arr_length(a) * a->data_type_size) == 0);
}
static void test_follow_many(void) {
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    rde_arr things = rde_arr_new(sizeof(u32), heap), moved = rde_arr_new(sizeof(u32), heap);
    rde_arr o1 = rde_arr_new(sizeof(u32), heap), b1 = rde_arr_new(sizeof(fude_zoom_place), heap), a1 = rde_arr_new(sizeof(fude_zoom_place), heap);
    rde_arr o2 = rde_arr_new(sizeof(u32), heap), b2 = rde_arr_new(sizeof(fude_zoom_place), heap), a2 = rde_arr_new(sizeof(fude_zoom_place), heap);
    // The same scene twice, the same moves: followed the new way and the old, everything must come out the same (what
    // was followed, in order, its places before and after, and every object's place), for scenes
    // and moves of all sizes.
    u32 followed = 0, differ = 0;
    for(u32 seed = 1; seed <= 12u; seed++) {
        fude_zoom_scene s1, s2; u32 c1, c2;
        cf_build(&s1, seed, 60u + seed * 40u, &c1, &things); cf_build(&s2, seed, 60u + seed * 40u, &c2, &things);
        for(u32 round = 0; round < 3u; round++) {   // (again from where the last left them)
            cf_move(&s1, seed * 3u + round, &things, 1u + (seed + round) % 6u, &moved); cf_move(&s2, seed * 3u + round, &things, 1u + (seed + round) % 6u, &moved);
            const u32 m1 = cf_follow(&s1, c1, &moved, &o1, &b1, &a1, false), m2 = cf_follow(&s2, c2, &moved, &o2, &b2, &a2, true);
            followed += m1;
            b8 same = m1 == m2 && cf_same_arr(&o1, &o2) && cf_same_arr(&b1, &b2) && cf_same_arr(&a1, &a2) && fude_zoom_scene_object_count(&s1) == fude_zoom_scene_object_count(&s2);
            for(u32 i = 0; same && i < fude_zoom_scene_object_count(&s1); i++) {
                const fude_zoom_place p1 = fude_zoom_scene_place_of(&s1, i), p2 = fude_zoom_scene_place_of(&s2, i);
                same = memcmp(&p1, &p2, sizeof p1) == 0;
            }
            differ += !same;
        }
        fude_zoom_scene_destroy(&s1); fude_zoom_scene_destroy(&s2);
    }
    printf("  follow: 36 moves, %u followed, %u different from the reference\n", followed, differ);
    CHECK(differ == 0 && followed > 500u);
    // Many things joined, half of them moved at once: the new way against the old.
    {
        fude_zoom_scene s1, s2; u32 c1, c2;
        cf_build(&s1, 99u, 4000u, &c1, &things); cf_build(&s2, 99u, 4000u, &c2, &things);
        cf_move(&s1, 5u, &things, 2u, &moved); cf_move(&s2, 5u, &things, 2u, &moved);
        clock_t t0 = clock(); const u32 m1 = cf_follow(&s1, c1, &moved, &o1, &b1, &a1, false);
        clock_t t1 = clock(); const u32 m2 = cf_follow(&s2, c2, &moved, &o2, &b2, &a2, true);
        clock_t t2 = clock();
        CHECK(m1 == m2 && cf_same_arr(&o1, &o2) && cf_same_arr(&a1, &a2));
        printf("  follow: %u things (%u objects), %u moved, %u followed: now %.1f ms, before %.1f ms\n", 4000u, fude_zoom_scene_object_count(&s1), (u32)rde_arr_length(&moved), m1,
               1000.0 * (double)(t1 - t0) / CLOCKS_PER_SEC, 1000.0 * (double)(t2 - t1) / CLOCKS_PER_SEC);
        fude_zoom_scene_destroy(&s1); fude_zoom_scene_destroy(&s2);
    }
    rde_arr_free(&things); rde_arr_free(&moved);
    rde_arr_free(&o1); rde_arr_free(&b1); rde_arr_free(&a1); rde_arr_free(&o2); rde_arr_free(&b2); rde_arr_free(&a2);
}

// Combine: two squares joined, overlapped, one cut out of the other; apart, joined they stay two.
static f64 combine_area(const rde_arr* out, const rde_arr* rings, const rde_arr* piece) {
    const fude_zoom_v2* p = (const fude_zoom_v2*)out->memory;
    const u32* r = (const u32*)rings->memory;
    const u32* k = (const u32*)piece->memory;
    const u32 n = (u32)rde_arr_length(out);
    f64 a = 0.0;
    for(u32 from = 0; from < n;) {
        u32 to = from;
        while(to < n && r[to] == r[from] && k[to] == k[from]) to++;
        a += fude_zoom_cut_area(&p[from], to - from);   // (holes clockwise: less)
        from = to;
    }
    return a;
}

static void test_combine(void) {
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    rde_arr out = rde_arr_new(sizeof(fude_zoom_v2), heap), rings = rde_arr_new(sizeof(u32), heap), piece = rde_arr_new(sizeof(u32), heap);
    const fude_zoom_v2 a[4] = { { 0, 0 }, { 10, 0 }, { 10, 10 }, { 0, 10 } };
    const fude_zoom_v2 b[4] = { { 5, 5 }, { 15, 5 }, { 15, 15 }, { 5, 15 } };
    u32 n = fude_zoom_cut_combine(a, NULL, 4u, b, NULL, 4u, FUDE_ZOOM_CUT_UNION, &out, &rings, &piece);
    CHECK(n == 1u && fabs(combine_area(&out, &rings, &piece) - 175.0) < 1e-9 && rde_arr_length(&out) == 8u);
    n = fude_zoom_cut_combine(a, NULL, 4u, b, NULL, 4u, FUDE_ZOOM_CUT_INTERSECT, &out, &rings, &piece);
    CHECK(n == 1u && fabs(combine_area(&out, &rings, &piece) - 25.0) < 1e-9 && rde_arr_length(&out) == 4u);
    n = fude_zoom_cut_combine(a, NULL, 4u, b, NULL, 4u, FUDE_ZOOM_CUT_SUBTRACT, &out, &rings, &piece);
    CHECK(n == 1u && fabs(combine_area(&out, &rings, &piece) - 75.0) < 1e-9);
    // A small square inside: subtracted, a hole; joined, just the big one.
    const fude_zoom_v2 c[4] = { { 3, 3 }, { 6, 3 }, { 6, 6 }, { 3, 6 } };
    n = fude_zoom_cut_combine(a, NULL, 4u, c, NULL, 4u, FUDE_ZOOM_CUT_SUBTRACT, &out, &rings, &piece);
    CHECK(n == 1u && fabs(combine_area(&out, &rings, &piece) - 91.0) < 1e-9 && rde_arr_length(&out) == 8u);
    n = fude_zoom_cut_combine(a, NULL, 4u, c, NULL, 4u, FUDE_ZOOM_CUT_UNION, &out, &rings, &piece);
    CHECK(n == 1u && fabs(combine_area(&out, &rings, &piece) - 100.0) < 1e-9 && rde_arr_length(&out) == 4u);
    // Apart: joined, two; overlapping nowhere, nothing kept.
    const fude_zoom_v2 d[4] = { { 20, 0 }, { 30, 0 }, { 30, 10 }, { 20, 10 } };
    n = fude_zoom_cut_combine(a, NULL, 4u, d, NULL, 4u, FUDE_ZOOM_CUT_UNION, &out, &rings, &piece);
    CHECK(n == 2u && fabs(combine_area(&out, &rings, &piece) - 200.0) < 1e-9);
    n = fude_zoom_cut_combine(a, NULL, 4u, d, NULL, 4u, FUDE_ZOOM_CUT_INTERSECT, &out, &rings, &piece);
    CHECK(n == 0u);
    rde_arr_free(&out); rde_arr_free(&rings); rde_arr_free(&piece);
}

// Trim and extend: where a line is crossed (shapes, strokes), the pieces kept; a chamfer's corner.
static void test_trim(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const rde_color c = { 1, 1, 1, 255 };
    // A line from (0,0) to (100,0); an upright line through x=30, a circle round (70,0) r10, a stroke across x=120.
    const u32 line = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, (const f64[2]){ 100, 0 }, 2u, c, 0.5f, 0u, 0);
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 30, -20 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, (const f64[2]){ 0, 40 }, 2u, c, 0.5f, 0u, 0);
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 70, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, (const f64[2]){ 10, 10 }, 2u, c, 0.5f, 0u, 0);
    f64 t[16];
    u32 n = fude_zoom_trim_crossings(&s, s.root, line, (fude_zoom_v2){ 0, 0 }, (fude_zoom_v2){ 100, 0 }, 0.0, 1.0, t, 16u);
    CHECK(n == 3u && fabs(t[0] - 0.3) < 1e-9 && fabs(t[1] - 0.6) < 2e-3 && fabs(t[2] - 0.8) < 2e-3);
    // Tapped between the upright line and the circle: that piece away, the rest kept.
    f64 keep[4];
    u32 k = fude_zoom_trim_keep(t, n, 0.45, keep);
    CHECK(k == 2u && keep[0] == 0.0 && fabs(keep[1] - 0.3) < 1e-9 && fabs(keep[2] - t[1]) < 1e-12 && keep[3] == 1.0);
    // Tapped before the first crossing: only what is past it kept; with none at all, nothing.
    k = fude_zoom_trim_keep(t, n, 0.1, keep);
    CHECK(k == 1u && fabs(keep[0] - 0.3) < 1e-9 && keep[1] == 1.0);
    CHECK(fude_zoom_trim_keep(NULL, 0u, 0.5, keep) == 0u);
    // Past its end (extend): the line beyond 100 meets nothing yet; a guide at x=150 is met at 1.5.
    n = fude_zoom_trim_crossings(&s, s.root, line, (fude_zoom_v2){ 0, 0 }, (fude_zoom_v2){ 100, 0 }, 1.0 + 1e-9, 50.0, t, 16u);
    CHECK(n == 0u);
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 150, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_GUIDE, (const f64[3]){ 0, 1, 500 }, 3u, c, 0.1f, 0u, 0);
    n = fude_zoom_trim_crossings(&s, s.root, line, (fude_zoom_v2){ 0, 0 }, (fude_zoom_v2){ 100, 0 }, 1.0 + 1e-9, 50.0, t, 16u);
    CHECK(n == 1u && fabs(t[0] - 1.5) < 1e-9);
    // A chamfer 10 back from where (0,0)→(100,0) and (0,-50)→(0,50)... (0,5)→(0,50) meet: each cut back from (0,0).
    fude_zoom_v2 ak, ae, bk, be;
    CHECK(fude_zoom_shape_chamfer((fude_zoom_v2){ 100, 0 }, (fude_zoom_v2){ 20, 0 }, (fude_zoom_v2){ 0, 5 }, (fude_zoom_v2){ 0, 50 }, 10.0, &ak, &ae, &bk, &be));
    CHECK(ak.x == 100.0 && fabs(ae.x - 10.0) < 1e-9 && fabs(ae.y) < 1e-9 && bk.y == 50.0 && fabs(be.y - 10.0) < 1e-9 && fabs(be.x) < 1e-9);
    // 0: carried on to meet; too far back: none.
    CHECK(fude_zoom_shape_chamfer((fude_zoom_v2){ 100, 0 }, (fude_zoom_v2){ 20, 0 }, (fude_zoom_v2){ 0, 5 }, (fude_zoom_v2){ 0, 50 }, 0.0, &ak, &ae, &bk, &be));
    CHECK(fabs(ae.x) < 1e-9 && fabs(be.y) < 1e-9);
    CHECK(!fude_zoom_shape_chamfer((fude_zoom_v2){ 100, 0 }, (fude_zoom_v2){ 20, 0 }, (fude_zoom_v2){ 0, 5 }, (fude_zoom_v2){ 0, 50 }, 60.0, &ak, &ae, &bk, &be));
    fude_zoom_scene_destroy(&s);
}

// Guides: as long as their reach, crossed and snapped along, never ends of their own, never exported.
static void test_guides(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const rde_color c = { 1, 1, 1, 255 };
    // Up through (100, 0), 2000 each way; a line across it.
    const u32 g = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 100, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_GUIDE, (const f64[3]){ 0, 5, 2000 }, 3u, c, 0.5f, 0u, 0);
    const fude_zoom_object* o = fude_zoom_scene_object(&s, g);
    CHECK(fabs(o->box.min_y + 2000.0) < 1.0 && fabs(o->box.max_y - 2000.0) < 1.0 && o->box.max_x - o->box.min_x < 2.0);
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { -300, 40 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_LINE, (const f64[2]){ 800, 0 }, 2u, c, 1.0f, 0u, 0);
    fude_zoom_visible v; memset(&v, 0, sizeof v);
    v.frame = s.root; v.to_screen = (fude_zoom_sim){ 1.0, 0.0, 0.0, 0.0 };
    // Where the line crosses it.
    fude_zoom_snap h = fude_zoom_snap_find(&s, &v, 1, (fude_zoom_v2){ 104, 37 }, 14.0);
    CHECK(h.kind == FUDE_ZOOM_SNAP_CROSS && fabs(h.at.x - 100.0) < 1e-9 && fabs(h.at.y - 40.0) < 1e-9);
    // Along it, far from anything else; its middle (where it was put) no snap of its own.
    h = fude_zoom_snap_find(&s, &v, 1, (fude_zoom_v2){ 103, 900 }, 14.0);
    CHECK(h.kind == FUDE_ZOOM_SNAP_NEAREST && fabs(h.at.x - 100.0) < 1e-9 && fabs(h.at.y - 900.0) < 1e-9);
    h = fude_zoom_snap_find(&s, &v, 1, (fude_zoom_v2){ 101, 3 }, 14.0);
    CHECK(h.kind == FUDE_ZOOM_SNAP_NEAREST);
    // Its end: none (2000 up).
    h = fude_zoom_snap_find(&s, &v, 1, (fude_zoom_v2){ 101, 1999 }, 14.0);
    CHECK(h.kind == FUDE_ZOOM_SNAP_NEAREST);
    // Exported: only the line.
    fude_bytes svg = fude_bytes_new(256);
    CHECK(fude_zoom_export_svg(&s, (fude_zoom_v2){ 400, 300 }, (rde_color){ 255, 255, 255, 255 }, &svg));
    rde_arr_add(&svg, &(u8){ 0 });
    u32 paths = 0;
    for(const c8* q = (const c8*)svg.memory; (q = strstr(q, "<path")) != NULL; q++) {
        paths++;
    }
    CHECK(paths == 1u);
    rde_arr_free(&svg);
    fude_zoom_scene_destroy(&s);
}

// Fitting parts into a board: all that fit, inside it, never over each other, a kerf apart.
static b8 nest_ok(f64 bw, f64 bh, f64 kerf, const fude_zoom_nest_place* p, u32 n) {
    for(u32 i = 0; i < n; i++) {
        if(!p[i].placed) continue;
        if(p[i].x < -1e-9 || p[i].y < -1e-9 || p[i].x + p[i].w > bw + 1e-9 || p[i].y + p[i].h > bh + 1e-9) return false;
        for(u32 j = i + 1; j < n; j++) {
            if(!p[j].placed) continue;
            const b8 apart = p[i].x + p[i].w + kerf <= p[j].x + 1e-9 || p[j].x + p[j].w + kerf <= p[i].x + 1e-9 ||
                             p[i].y + p[i].h + kerf <= p[j].y + 1e-9 || p[j].y + p[j].h + kerf <= p[i].y + 1e-9;
            if(!apart) return false;
        }
    }
    return true;
}

static void test_nest(void) {
    // The list of parts typed, a line at a time: how many after the sizes or first, before the name (the hint's own
    // "2x Side 600 x 300" was read as 600 × 2, 300 of them).
    {
        fude_zoom_nest_line pl;
        const c8* after = "Side 600 x 300 x 2";
        CHECK(fude_zoom_nest_read_line(after, strlen(after), FUDE_ZOOM_UNIT_MM, 200u, &pl) && strcmp(pl.name, "Side") == 0 &&
              pl.length == 600.0 && pl.width == 300.0 && pl.count == 2u);
        const c8* first = "2x Side 600 x 300";
        CHECK(fude_zoom_nest_read_line(first, strlen(first), FUDE_ZOOM_UNIT_MM, 200u, &pl) && strcmp(pl.name, "Side") == 0 &&
              pl.length == 600.0 && pl.width == 300.0 && pl.count == 2u);
        const c8* times = "3 \xC3\x97 Top shelf 800 \xC3\x97 250";   // (3 × Top shelf 800 × 250)
        CHECK(fude_zoom_nest_read_line(times, strlen(times), FUDE_ZOOM_UNIT_MM, 200u, &pl) && strcmp(pl.name, "Top shelf") == 0 &&
              pl.length == 800.0 && pl.width == 250.0 && pl.count == 3u);
        const c8* cm = "4X Back 60 x 30";   // (in centimetres)
        CHECK(fude_zoom_nest_read_line(cm, strlen(cm), FUDE_ZOOM_UNIT_CM, 200u, &pl) && pl.count == 4u && fabs(pl.length - 600.0) < 1e-9 && fabs(pl.width - 300.0) < 1e-9);
        const c8* many = "999x Slat 400 x 40";   // (held to the most asked for)
        CHECK(fude_zoom_nest_read_line(many, strlen(many), FUDE_ZOOM_UNIT_MM, 200u, &pl) && pl.count == 200u && pl.length == 400.0);
        const c8* bare = "600 x 300";   // (no name, no count: one; its first number a size)
        CHECK(fude_zoom_nest_read_line(bare, strlen(bare), FUDE_ZOOM_UNIT_MM, 200u, &pl) && pl.count == 1u && pl.length == 600.0 && pl.width == 300.0 && pl.name[0] == 0);
        const c8* sizes = "2 x 3 x 4";   // (three numbers: the first two the sizes, the last how many)
        CHECK(fude_zoom_nest_read_line(sizes, strlen(sizes), FUDE_ZOOM_UNIT_MM, 200u, &pl) && pl.count == 4u && pl.length == 3.0 && pl.width == 2.0);
        const c8* heading = "Cabinet";   // (no sizes: no part)
        CHECK(!fude_zoom_nest_read_line(heading, strlen(heading), FUDE_ZOOM_UNIT_MM, 200u, &pl) && pl.count == 0u);
        const c8* box = "Box 600 x 300; 2x Lid 300 x 300";   // (only the first part, up to the ';', read)
        CHECK(fude_zoom_nest_read_line(box, 13u, FUDE_ZOOM_UNIT_MM, 200u, &pl) && strcmp(pl.name, "Box") == 0 && pl.count == 1u);
    }
    fude_zoom_nest_part parts[40];
    fude_zoom_nest_place out[40];
    // Four quarters of a board, no kerf: all of it.
    for(u32 i = 0; i < 4; i++) parts[i] = (fude_zoom_nest_part){ 500, 500, false };
    CHECK(fude_zoom_nest(1000, 1000, 0.0, parts, 4, out) == 4u && nest_ok(1000, 1000, 0.0, out, 4));
    // With a 3 mm kerf they no longer do; 498.5 each, they do again.
    CHECK(fude_zoom_nest(1000, 1000, 3.0, parts, 4, out) == 1u);
    for(u32 i = 0; i < 4; i++) parts[i] = (fude_zoom_nest_part){ 498.5, 498.5, false };
    CHECK(fude_zoom_nest(1000, 1000, 3.0, parts, 4, out) == 4u && nest_ok(1000, 1000, 3.0, out, 4));
    // Turned to fit, when it may be; not when its grain must run along the board.
    parts[0] = (fude_zoom_nest_part){ 300, 800, true };
    CHECK(fude_zoom_nest(1000, 300, 0.0, parts, 1, out) == 1u && out[0].turned && out[0].w == 800 && out[0].h == 300);
    parts[0].can_turn = false;
    CHECK(fude_zoom_nest(1000, 300, 0.0, parts, 1, out) == 0u && !out[0].placed);
    // Many at random: what fits is inside, apart, and as much wood as any order put down.
    const u32 kept = rng;   // (the other tests' numbers left as they were)
    rng = 99u;
    for(u32 round = 0; round < 20u; round++) {
        const u32 n = 5u + rnd() % 30u;
        for(u32 i = 0; i < n; i++) parts[i] = (fude_zoom_nest_part){ 50.0 + rndf() * 700.0, 50.0 + rndf() * 500.0, (rnd() & 1u) != 0 };
        const u32 fit = fude_zoom_nest(2440, 1220, 3.2, parts, n, out);
        CHECK(fit >= 1u && nest_ok(2440, 1220, 3.2, out, n));
        u32 placed = 0;
        for(u32 i = 0; i < n; i++) {
            placed += out[i].placed ? 1u : 0u;
            if(out[i].placed) CHECK((out[i].turned ? out[i].w == parts[i].h : out[i].w == parts[i].w) && (!out[i].turned || parts[i].can_turn));
        }
        CHECK(placed == fit);
    }
    rng = kept;
    // A sheet of 2440 x 1220 and a cabinet's parts: a good share of it used.
    const f64 cab[][2] = { { 720, 560 }, { 720, 560 }, { 764, 560 }, { 764, 560 }, { 720, 764 }, { 700, 300 }, { 700, 300 }, { 700, 300 } };
    // (three slats 380 × 230 on a 720 × 400 board: one fits lengthwise; turned, all three side by side — 0.1.84)
    for(u32 i = 0; i < 3; i++) parts[i] = (fude_zoom_nest_part){ 380, 230, false };
    CHECK(fude_zoom_nest(720, 400, 0.0, parts, 3, out) == 1u);
    for(u32 i = 0; i < 3; i++) parts[i].can_turn = true;
    CHECK(fude_zoom_nest(720, 400, 0.0, parts, 3, out) == 3u && nest_ok(720, 400, 0.0, out, 3) && out[0].turned && out[1].turned && out[2].turned);
    for(u32 i = 0; i < 8; i++) parts[i] = (fude_zoom_nest_part){ cab[i][0], cab[i][1], false };
    CHECK(fude_zoom_nest(2440, 1220, 3.2, parts, 8, out) >= 6u && nest_ok(2440, 1220, 3.2, out, 8));
}

// --- texts ------------------------------------------------------------------------------------------

// --- layers ----------------------------------------------------------------------------------------

static void test_layers(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_eraser e; fude_zoom_eraser_init(&e);
    // New things go on the current layer.
    const u32 front = line(&s, 0, 0, 100, 0, 1.0, 1.0f);
    s.layer = 1;
    const u32 back = line(&s, 0, 20, 100, 20, 1.0, 1.0f);
    s.layer = 0;
    CHECK(fude_zoom_scene_object(&s, front)->layer == 0 && fude_zoom_scene_object(&s, back)->layer == 1);
    // The pile: never moved, a layer's index is its place (layer 1 over 0, whatever was drawn first);
    // ranked, as the ranks say; the flags' other bits untouched by a rank.
    CHECK(fude_zoom_scene_draw_key(&s, fude_zoom_scene_object(&s, back)) > fude_zoom_scene_draw_key(&s, fude_zoom_scene_object(&s, front)));
    CHECK(fude_zoom_layer_rank_of(0u) == -1 && fude_zoom_layer_rank_of(fude_zoom_layer_with_rank(FUDE_ZOOM_LAYER_LOCKED, 0u)) == 0);
    CHECK((fude_zoom_layer_with_rank(FUDE_ZOOM_LAYER_LOCKED | FUDE_ZOOM_LAYER_HIDDEN, 5u) & 3u) == 3u);
    {
        const u32 r0 = fude_zoom_scene_add_layer(&s, s.root, 0, fude_zoom_layer_with_rank(0u, 1u), "");
        const u32 r1 = fude_zoom_scene_add_layer(&s, s.root, 1, fude_zoom_layer_with_rank(0u, 0u), "");
        fude_zoom_scene_layers_refresh(&s);
        CHECK(s.layer_rank[0] == 1u && s.layer_rank[1] == 0u);
        CHECK(fude_zoom_scene_draw_key(&s, fude_zoom_scene_object(&s, front)) > fude_zoom_scene_draw_key(&s, fude_zoom_scene_object(&s, back)));
        fude_zoom_scene_set_alive(&s, r0, false);
        fude_zoom_scene_set_alive(&s, r1, false);
        fude_zoom_scene_layers_refresh(&s);
        CHECK(s.layer_rank[0] == 0u && s.layer_rank[1] == 1u);   // gone: back to their indexes
    }
    // Its state an object: hidden, then refreshed into the scene's bits.
    const u32 l1 = fude_zoom_scene_add_layer(&s, s.root, 1, FUDE_ZOOM_LAYER_HIDDEN, "Back");
    fude_zoom_history_push(&s, s.root, (fude_zoom_box){ 0, 0, 0, 0 }, NULL, 0, &l1, 1);
    fude_zoom_scene_layers_refresh(&s);
    CHECK(s.layers_hidden == 2u && s.layers_locked == 0u);
    CHECK(fude_zoom_scene_hides(&s, fude_zoom_scene_object(&s, back)) && !fude_zoom_scene_touchable(&s, fude_zoom_scene_object(&s, back)));
    CHECK(!fude_zoom_scene_hides(&s, fude_zoom_scene_object(&s, front)) && fude_zoom_scene_touchable(&s, fude_zoom_scene_object(&s, front)));
    u16 index; u8 flags; c8 name[16];
    CHECK(fude_zoom_scene_layer_of(&s, l1, &index, &flags, name, sizeof name) && index == 1 && flags == FUDE_ZOOM_LAYER_HIDDEN && strcmp(name, "Back") == 0);
    // A sweep across both: the hidden one is not touched.
    fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_STROKE);
    fude_zoom_erase_step(&s, &e, s.root, (fude_zoom_v2){ 50, -10 }, (fude_zoom_v2){ 50, 30 }, 3.0, 0.5);
    fude_zoom_erase_end(&s, &e, s.root, (fude_zoom_box){ 0, -10, 100, 30 });
    CHECK(!alive(&s, front) && alive(&s, back));
    // Locked instead (shown, still not touched); then a close-up cut of it, unlocked, stays on its layer.
    const u32 l1b = fude_zoom_scene_add_layer(&s, s.root, 1, FUDE_ZOOM_LAYER_LOCKED, "Back");
    fude_zoom_scene_set_alive(&s, l1, false);
    fude_zoom_history_push(&s, s.root, (fude_zoom_box){ 0, 0, 0, 0 }, &l1, 1, &l1b, 1);
    fude_zoom_scene_layers_refresh(&s);
    CHECK(s.layers_hidden == 0u && s.layers_locked == 2u);
    CHECK(!fude_zoom_scene_hides(&s, fude_zoom_scene_object(&s, back)) && !fude_zoom_scene_touchable(&s, fude_zoom_scene_object(&s, back)));
    fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_PARTIAL);
    fude_zoom_erase_step(&s, &e, s.root, (fude_zoom_v2){ 50, 10 }, (fude_zoom_v2){ 50, 30 }, 2.0, 0.5);
    fude_zoom_erase_end(&s, &e, s.root, (fude_zoom_box){ 0, 10, 100, 30 });
    CHECK(alive(&s, back));
    CHECK(fude_zoom_history_undo(&s));   // unlocked again (the step that locked it undone)
    fude_zoom_scene_layers_refresh(&s);
    CHECK(s.layers_hidden == 2u && s.layers_locked == 0u);
    CHECK(fude_zoom_history_undo(&s));   // the sweep: the front line back, the back still hidden
    CHECK(alive(&s, front));
    CHECK(fude_zoom_history_undo(&s));   // the hiding
    fude_zoom_scene_layers_refresh(&s);
    CHECK(s.layers_hidden == 0u && s.layers_locked == 0u);
    const u32 mark = fude_zoom_scene_object_count(&s);
    fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_PARTIAL);
    fude_zoom_erase_step(&s, &e, s.root, (fude_zoom_v2){ 50, 10 }, (fude_zoom_v2){ 50, 30 }, 2.0, 0.5);
    fude_zoom_erase_end(&s, &e, s.root, (fude_zoom_box){ 0, 10, 100, 30 });
    CHECK(!alive(&s, back));
    u32 cut = 0;
    for(u32 i = mark; i < fude_zoom_scene_object_count(&s); i++) {
        const fude_zoom_object* o = fude_zoom_scene_object(&s, i);
        if((o->flags & FUDE_ZOOM_FLAG_ALIVE) && o->kind == FUDE_ZOOM_KIND_FILL) {
            CHECK(o->layer == 1);
            cut++;
        }
    }
    CHECK(cut >= 1u && s.layer == 0);
    // Through the file: the layer's state and each thing's layer kept.
    CHECK(fude_zoom_history_undo(&s));   // the cut undone: the line back
    const u32 l1c = fude_zoom_scene_add_layer(&s, s.root, 1, FUDE_ZOOM_LAYER_HIDDEN | FUDE_ZOOM_LAYER_LOCKED, "Back");
    fude_zoom_history_push(&s, s.root, (fude_zoom_box){ 0, 0, 0, 0 }, NULL, 0, &l1c, 1);
    mkdir("./saves", 0755);
    remove("./saves/layers.zoom"); remove("./saves/layers.zoom.bak");
    fude_zoom_file fz; memset(&fz, 0, sizeof fz); snprintf(fz.path, sizeof fz.path, "%s", "./saves/layers.zoom");
    CHECK(fude_zoom_file_flush(&fz, &s));
    fude_zoom_scene r; fude_zoom_scene_init(&r, 8); fude_zoom_file g;
    CHECK(fude_zoom_file_open(&g, "./saves/layers.zoom", &r) == FUDE_LOAD_OK);
    fude_zoom_scene_layers_refresh(&r);
    CHECK(r.layers_hidden == 2u && r.layers_locked == 2u);
    u32 on_back = 0;
    for(u32 i = 0; i < fude_zoom_scene_object_count(&r); i++) {
        const fude_zoom_object* o = fude_zoom_scene_object(&r, i);
        on_back += (o->flags & FUDE_ZOOM_FLAG_ALIVE) && o->kind == FUDE_ZOOM_KIND_STROKE && o->layer == 1 ? 1u : 0u;
    }
    CHECK(on_back == 1u);
    fude_zoom_scene_destroy(&r);
    fude_zoom_eraser_destroy(&e);
    fude_zoom_scene_destroy(&s);
}

// Clear (the toolbar's bin with nothing lassoed: fude_zoom_scene_clear) takes what is drawn on the layers a tool may
// touch, at every depth, as one undo step. It leaves the frames, the places, the layers themselves (their records:
// names, eyes, locks and order) and what a hidden or a locked layer holds; it took all of those but frames and places
// once. A second Clear finds nothing to take and makes no step; undone, everything is back as it was.
static void test_clear(void) {
    printf("clear\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 a = line(&s, 0, 0, 100, 0, 1.0, 1.0f);
    s.layer = 1; const u32 hidden = line(&s, 0, 20, 100, 20, 1.0, 1.0f);
    s.layer = 2; const u32 locked = line(&s, 0, 40, 100, 40, 1.0, 1.0f);
    s.layer = 3; const u32 d = line(&s, 0, 60, 100, 60, 1.0, 1.0f);
    // Deeper: a frame with a line on layer 0 and one on the locked layer.
    const u32 f = fude_zoom_scene_new_frame(&s, s.root, (fude_zoom_xform){ 50, 50, 1.0 / 1024.0, 0.0 }, (fude_zoom_box){ 40, 40, 60, 60 });
    s.camera.frame = f;
    s.layer = 0; const u32 deep = line(&s, 0, 0, 100, 0, 1.0, 1.0f);
    s.layer = 2; const u32 deep_locked = line(&s, 0, 10, 100, 10, 1.0, 1.0f);
    s.camera.frame = s.root;
    s.layer = 0;
    const u32 mark = fude_zoom_scene_add_mark(&s, s.root, (fude_zoom_v2){ 5, 5 }, 2.0, 1u);
    const u32 rec[4] = { fude_zoom_scene_add_layer(&s, s.root, 1, fude_zoom_layer_with_rank(FUDE_ZOOM_LAYER_HIDDEN, 2u), "Hidden"),
                         fude_zoom_scene_add_layer(&s, s.root, 2, fude_zoom_layer_with_rank(FUDE_ZOOM_LAYER_LOCKED, 3u), "Locked"),
                         fude_zoom_scene_add_layer(&s, s.root, 3, fude_zoom_layer_with_rank(0u, 0u), "Bottom"),
                         fude_zoom_scene_add_layer(&s, s.root, 0, fude_zoom_layer_with_rank(0u, 1u), "First") };
    fude_zoom_history_push(&s, s.root, (fude_zoom_box){ 0, 0, 0, 0 }, NULL, 0, rec, 4u);
    fude_zoom_scene_layers_refresh(&s);
    u8 ranks[FUDE_ZOOM_LAYERS];
    memcpy(ranks, s.layer_rank, sizeof ranks);
    CHECK(s.layers_hidden == 2u && s.layers_locked == 4u && ranks[3] == 0u && ranks[0] == 1u);
    // What the old Clear took: everything alive but frames and places, the layers' records and their things with it.
    u32 old_took = 0;
    for(u32 i = 0; i < fude_zoom_scene_object_count(&s); i++) {
        const fude_zoom_object* o = fude_zoom_scene_object(&s, i);
        old_took += o->kind != FUDE_ZOOM_KIND_FRAME && o->kind != FUDE_ZOOM_KIND_MARK && (o->flags & FUDE_ZOOM_FLAG_ALIVE) ? 1u : 0u;
    }
    CHECK(old_took == 10u);
    rde_arr died = rde_arr_new(sizeof(u32), NULL);
    const u32 steps = s.action_count;
    const u32 n = fude_zoom_scene_clear(&s, &died);
    fude_zoom_history_push(&s, s.root, (fude_zoom_box){ 0, 0, 100, 100 }, (const u32*)died.memory, (u32)rde_arr_length(&died), NULL, 0);
    fude_zoom_scene_layers_refresh(&s);
    const u32* took = (const u32*)died.memory;
    CHECK(n == 3u && rde_arr_length(&died) == 3u && took[0] == a && took[1] == d && took[2] == deep && s.action_count == steps + 1u);
    CHECK(!alive(&s, a) && !alive(&s, d) && !alive(&s, deep));
    CHECK(alive(&s, hidden) && alive(&s, locked) && alive(&s, deep_locked) && alive(&s, mark) && alive(&s, fude_zoom_scene_frame(&s, f)->object));
    CHECK(alive(&s, rec[0]) && alive(&s, rec[1]) && alive(&s, rec[2]) && alive(&s, rec[3]));
    CHECK(s.layers_hidden == 2u && s.layers_locked == 4u && memcmp(ranks, s.layer_rank, sizeof ranks) == 0);
    u16 index; u8 flags; c8 name[16];
    CHECK(fude_zoom_scene_layer_of(&s, rec[2], &index, &flags, name, sizeof name) && index == 3 && strcmp(name, "Bottom") == 0);
    // Again: nothing left that it may take, and no step.
    rde_arr_clear(&died);
    CHECK(fude_zoom_scene_clear(&s, &died) == 0u && rde_arr_length(&died) == 0u);
    fude_zoom_history_push(&s, s.root, (fude_zoom_box){ 0, 0, 100, 100 }, NULL, 0, NULL, 0);
    CHECK(s.action_count == steps + 1u);
    // Undone: all of it back.
    CHECK(fude_zoom_history_undo(&s) && alive(&s, a) && alive(&s, d) && alive(&s, deep) && alive(&s, hidden) && alive(&s, locked));
    rde_arr_free(&died);
    fude_zoom_scene_destroy(&s);
}

static void test_texts(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const c8* words = "Mesa de roble\nTablero 1200 x 800";
    const u32 t = fude_zoom_scene_add_text(&s, s.root, (fude_zoom_place){ { 10, 50 }, 0.0, 2.0 }, 9.0, 100.0, 24.0, FUDE_ZOOM_TEXT_STICKY,
                                           (rde_color){ 1, 2, 3, 255 }, (rde_color){ 255, 233, 140, 255 }, words, (u32)strlen(words), 0);
    f64 size, w, h; u8 style; const c8* back; u32 len;
    CHECK(fude_zoom_scene_text(&s, t, &size, &w, &h, &style, &back, &len));
    CHECK(size == 9.0 && w == 100.0 && h == 24.0 && style == FUDE_ZOOM_TEXT_STICKY && len == strlen(words) && memcmp(back, words, len) == 0);
    // Its box: from its top left, down and right, at its scale.
    const fude_zoom_object* o = fude_zoom_scene_object(&s, t);
    CHECK(fabs(o->box.min_x - 10) < 1e-9 && fabs(o->box.max_x - 210) < 1e-9 && fabs(o->box.max_y - 50) < 1e-9 && fabs(o->box.min_y - 2) < 1e-9);
    CHECK(fude_zoom_scene_object(&s, t)->fill.g == 233);
    // Through the file and back (a picture's way: its payload kept as it came).
    mkdir("./saves", 0755);
    remove("./saves/texts.zoom"); remove("./saves/texts.zoom.bak");
    fude_zoom_file fz; memset(&fz, 0, sizeof fz); snprintf(fz.path, sizeof fz.path, "%s", "./saves/texts.zoom");
    CHECK(fude_zoom_file_flush(&fz, &s));
    fude_zoom_scene r; fude_zoom_scene_init(&r, 8); fude_zoom_file g;
    CHECK(fude_zoom_file_open(&g, "./saves/texts.zoom", &r) == FUDE_LOAD_OK);
    u32 found = 0;
    for(u32 i = 0; i < fude_zoom_scene_object_count(&r); i++) {
        if(fude_zoom_scene_object(&r, i)->kind == FUDE_ZOOM_KIND_TEXT && fude_zoom_scene_text(&r, i, &size, NULL, NULL, &style, &back, &len)) {
            found += size == 9.0 && style == FUDE_ZOOM_TEXT_STICKY && len == strlen(words) && memcmp(back, words, len) == 0 ? 1u : 0u;
        }
    }
    CHECK(found == 1u);
    fude_zoom_scene_destroy(&r);
    fude_zoom_scene_destroy(&s);
}

// --- pictures --------------------------------------------------------------------------------------

static void test_pictures(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    // Its file's bytes, as they came (the scene keeps them; only the renderer decodes).
    static u8 bytes[3000];
    for(u32 i = 0; i < sizeof bytes; i++) bytes[i] = (u8)(rnd() & 0xFF);
    const u32 im = fude_zoom_scene_add_image(&s, s.root, (fude_zoom_place){ { 200, 100 }, 0.0, 1.0 }, 80, 60, bytes, sizeof bytes, 0);
    fude_zoom_history_push(&s, s.root, fude_zoom_scene_object(&s, im)->box, NULL, 0, &im, 1);
    CHECK(fude_zoom_scene_object(&s, im)->kind == FUDE_ZOOM_KIND_IMAGE);
    CHECK(fabs(fude_zoom_scene_object(&s, im)->box.min_x - 120) < 1e-9 && fabs(fude_zoom_scene_object(&s, im)->box.max_y - 160) < 1e-9);
    f64 hw = 0, hh = 0; const u8* got = NULL; u32 n = 0;
    CHECK(fude_zoom_scene_image(&s, im, &hw, &hh, &got, &n) && hw == 80 && hh == 60 && n == sizeof bytes && memcmp(got, bytes, n) == 0);
    // Found where it is.
    rde_arr found = rde_arr_new(sizeof(u32), NULL);
    fude_zoom_scene_query(&s, s.root, (fude_zoom_box){ 190, 90, 210, 110 }, &found);
    CHECK(rde_arr_length(&found) == 1);
    rde_arr_free(&found);
    // The eraser passes over it and leaves it.
    fude_zoom_eraser e; fude_zoom_eraser_init(&e);
    fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_STROKE);
    CHECK(fude_zoom_erase_step(&s, &e, s.root, (fude_zoom_v2){ 150, 100 }, (fude_zoom_v2){ 250, 100 }, 5.0, 0.5) == 0);
    fude_zoom_erase_end(&s, &e, s.root, (fude_zoom_box){ 0, 0, 1, 1 });
    fude_zoom_eraser_destroy(&e);
    CHECK(alive(&s, im));
    // Turned a quarter and halved round its middle: corners where they should be, box with them.
    const fude_zoom_place b0 = fude_zoom_scene_place_of(&s, im);
    const fude_zoom_sim turn = { 0.0, 0.5, 200.0 - (0.0 * 200.0 - 0.5 * 100.0), 100.0 - (0.5 * 200.0 + 0.0 * 100.0) };
    const fude_zoom_place a0 = fude_zoom_place_moved(b0, turn);
    fude_zoom_scene_set_place(&s, im, a0);
    fude_zoom_history_push_moved(&s, s.root, (fude_zoom_box){ 0, 0, 1, 1 }, &im, &b0, &a0, 1);
    fude_zoom_v2 c[4];
    fude_zoom_scene_image_corners(&s, im, c);
    // Its bottom-left (-80, -60), turned to (30, -40) halved: from the middle (200, 100).
    CHECK(fabs(c[0].x - 230) < 1e-9 && fabs(c[0].y - 60) < 1e-9);
    CHECK(fabs(fude_zoom_scene_object(&s, im)->box.max_x - 230) < 1e-9 && fabs(fude_zoom_scene_object(&s, im)->box.max_y - 140) < 1e-9);
    // A copy, as the clipboard makes (the same bytes, its own object), deleted again.
    const u32 cp = fude_zoom_scene_add_image(&s, s.root, (fude_zoom_place){ { 500, 100 }, 0.3, 2.0 }, hw, hh, got, n, 0);
    fude_zoom_history_push(&s, s.root, (fude_zoom_box){ 0, 0, 1, 1 }, NULL, 0, &cp, 1);
    fude_zoom_scene_set_alive(&s, cp, false);
    fude_zoom_history_push(&s, s.root, (fude_zoom_box){ 0, 0, 1, 1 }, &cp, 1, NULL, 0);
    CHECK(!alive(&s, cp));

    // Through the file: the bytes, its place, and the history (undo the delete, then the turn).
    mkdir("./saves", 0755);
    remove("./saves/pictures.zoom"); remove("./saves/pictures.zoom.bak");
    fude_zoom_file fz; memset(&fz, 0, sizeof fz); snprintf(fz.path, sizeof fz.path, "%s", "./saves/pictures.zoom");
    CHECK(fude_zoom_file_flush(&fz, &s));
    fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
    CHECK(fude_zoom_file_open(&g, "./saves/pictures.zoom", &t) == FUDE_LOAD_OK);
    const u32 ti = fude_zoom_scene_find_object(&t, fude_zoom_scene_object(&s, im)->id);
    const u32 tc = fude_zoom_scene_find_object(&t, fude_zoom_scene_object(&s, cp)->id);
    CHECK(ti != FUDE_ZOOM_NONE && tc != FUDE_ZOOM_NONE);
    if(ti != FUDE_ZOOM_NONE && tc != FUDE_ZOOM_NONE) {
        f64 thw = 0, thh = 0; const u8* tb = NULL; u32 tn = 0;
        CHECK(fude_zoom_scene_image(&t, ti, &thw, &thh, &tb, &tn) && thw == 80 && thh == 60 && tn == sizeof bytes && memcmp(tb, bytes, tn) == 0);
        CHECK(fabs(fude_zoom_scene_object(&t, ti)->scale - 0.5) < 1e-12 && fabs(fude_zoom_scene_object(&t, ti)->rotation - 1.5707963267948966) < 1e-12);
        CHECK(alive(&t, ti) && !alive(&t, tc));
        CHECK(fude_zoom_history_undo(&t) && alive(&t, tc));
        CHECK(fude_zoom_history_undo(&t) && !alive(&t, tc));
        CHECK(fude_zoom_history_undo(&t) && fabs(fude_zoom_scene_object(&t, ti)->scale - 1.0) < 1e-12 && fabs(fude_zoom_scene_object(&t, ti)->t.x - 200) < 1e-9);
    }
    // Compacted: still all there.
    CHECK(fude_zoom_file_compact(&g, &t));
    fude_zoom_scene u; fude_zoom_scene_init(&u, 7); fude_zoom_file h;
    CHECK(fude_zoom_file_open(&h, "./saves/pictures.zoom", &u) == FUDE_LOAD_OK);
    const u32 ui = fude_zoom_scene_find_object(&u, fude_zoom_scene_object(&s, im)->id);
    CHECK(ui != FUDE_ZOOM_NONE);
    if(ui != FUDE_ZOOM_NONE) {
        const u8* ub = NULL; u32 un = 0;
        CHECK(fude_zoom_scene_image(&u, ui, NULL, NULL, &ub, &un) && un == sizeof bytes && memcmp(ub, bytes, un) == 0);
    }
    fude_zoom_scene_destroy(&u);
    fude_zoom_scene_destroy(&t);
    fude_zoom_scene_destroy(&s);
}

// --- the file ----------------------------------------------------------------------------------

// Its bytes (none when it is not there): rde_arr_free after.
static rde_arr read_all(const char* p) {
    rde_arr d = rde_arr_new(sizeof(u8), NULL);
    FILE* f = fopen(p, "rb"); if(!f) return d;
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    rde_arr_resize(&d, (usize)sz); rde_arr_resize(&d, fread(d.memory, 1, (size_t)sz, f)); fclose(f); return d;
}
static void write_all(const char* p, const u8* d, u32 n) { FILE* f = fopen(p, "wb"); fwrite(d, 1, n, f); fclose(f); }

// What a state is, for comparing: the alive strokes (ids, counts) and the history's place.
typedef struct { u64 sig; u32 actions, action_count; u64 size; } state;
static state state_of(const fude_zoom_scene* s, const fude_zoom_file* f) {
    return (state){ signature(s), (u32)rde_arr_length(&s->actions), s->action_count, f != NULL ? f->size : 0 };
}

static void test_file(void) {
    mkdir("./saves", 0755);
    const char* path = "./saves/canvas.zoom";
    remove(path);
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_eraser e; fude_zoom_eraser_init(&e);
    fude_zoom_file f; memset(&f, 0, sizeof f); snprintf(f.path, sizeof f.path, "%s", path);

    // Nothing there yet.
    { fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g; CHECK(fude_zoom_file_open(&g, path, &t) == FUDE_LOAD_MISSING); fude_zoom_scene_destroy(&t); }

    // Strokes in two frames, sweeps, undos; flushed (journal) now and then, a
    // checkpoint in between. Each saved state remembered with the file's size.
    enum { MAXS = 64 };
    static state saved[MAXS]; u32 nsaved = 0;
    const fude_zoom_v2 half = { 500, 400 };
    for(u32 round = 0; round < 24; round++) {
        for(u32 k = 0; k < 3; k++) random_step(&s, &e);
        if(round == 6) { for(u32 k = 0; k < 10; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&s, half); } }
        if(round == 14) { for(u32 k = 0; k < 10; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 0.5); fude_zoom_camera_settle(&s, half); } }
        if(round % 5 == 4) fude_zoom_history_undo(&s);
        CHECK(fude_zoom_file_flush(&f, &s));
        if(round % 8 == 7) CHECK(fude_zoom_file_checkpoint(&f, &s));
        saved[nsaved++] = state_of(&s, &f);
    }
    const state last = state_of(&s, &f);

    // Opened again: the same strokes, the same history, and undo still works.
    {
        fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
        CHECK(fude_zoom_file_open(&g, path, &t) == FUDE_LOAD_OK);
        const state got = state_of(&t, &g);
        CHECK(got.sig == last.sig);
        CHECK(got.actions == last.actions && got.action_count == last.action_count);
        // Point for point, every alive stroke.
        b8 same = true;
        for(u32 i = 0; i < fude_zoom_scene_object_count(&s); i++) {
            const fude_zoom_object* o = fude_zoom_scene_object(&s, i);
            if(o->kind != FUDE_ZOOM_KIND_STROKE || !(o->flags & FUDE_ZOOM_FLAG_ALIVE)) continue;
            const u32 j = fude_zoom_scene_find_object(&t, o->id);
            if(j == FUDE_ZOOM_NONE) { same = false; continue; }
            const fude_zoom_object* p = fude_zoom_scene_object(&t, j);
            rde_arr qaa, qba; fude_zoom_qpoint* qa = table(&qaa, sizeof(*qa), o->count); fude_zoom_qpoint* qb = table(&qba, sizeof(*qb), o->count);
            fude_zoom_scene_points(&s, i, qa); fude_zoom_scene_points(&t, j, qb);
            if(p->count != o->count || memcmp(qa, qb, o->count * sizeof(*qa)) || p->t.x != o->t.x || p->t.y != o->t.y || p->q != o->q) same = false;
            rde_arr_free(&qaa); rde_arr_free(&qba);
        }
        CHECK(same);
        // Undo after reopening brings back what the original's undo would.
        if(fude_zoom_history_can_undo(&s)) {
            fude_zoom_history_undo(&s); fude_zoom_history_undo(&t);
            CHECK(signature(&s) == signature(&t));
            fude_zoom_history_redo(&s); fude_zoom_history_redo(&t);
        }
        // And it carries on: more drawn, flushed, opened a third time.
        fude_zoom_file_flush(&g, &t);
        for(u32 k = 0; k < 5; k++) random_step(&t, &e);
        CHECK(fude_zoom_file_flush(&g, &t));
        const state t_state = state_of(&t, &g);
        fude_zoom_scene u; fude_zoom_scene_init(&u, 7); fude_zoom_file h;
        CHECK(fude_zoom_file_open(&h, path, &u) == FUDE_LOAD_OK);
        CHECK(signature(&u) == t_state.sig && u.action_count == t_state.action_count);
        fude_zoom_scene_destroy(&u);
        fude_zoom_scene_destroy(&t);
    }

    // Cut off at every byte: it opens to the last saved state the cut keeps
    // (or nothing, before the first footer), never anything else, never a crash.
    // The file now holds round two's continuation too: rebuild it from the original.
    remove(path); remove("./saves/canvas.zoom.bak");
    fude_zoom_scene_destroy(&s); fude_zoom_scene_init(&s, 7); rng = 999u;
    memset(&f, 0, sizeof f); snprintf(f.path, sizeof f.path, "%s", path);
    nsaved = 0;
    for(u32 round = 0; round < 10; round++) {
        for(u32 k = 0; k < 2; k++) random_step(&s, &e);
        if(round % 4 == 3) fude_zoom_history_undo(&s);
        fude_zoom_file_flush(&f, &s);
        saved[nsaved++] = state_of(&s, &f);   // safe once its journal is on disk
        if(round == 5) fude_zoom_file_checkpoint(&f, &s);
    }
    rde_arr whole = read_all(path); const u32 total = (u32)rde_arr_length(&whole);
    CHECK(total == f.size);
    printf("  file: %u bytes, every cut tried\n", total);
    const char* cut = "./saves/cut.zoom";
    b8 all_ok = true;
    for(u32 len = 0; len <= total; len++) {
        remove(cut); remove("./saves/cut.zoom.bak"); remove("./saves/cut.zoom.bad"); remove("./saves/cut.zoom.tmp");
        write_all(cut, whole.memory, len);
        fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
        const FUDE_LOAD_ r = fude_zoom_file_open(&g, cut, &t);
        // The newest saved state whose bytes are all within the cut.
        i32 want = -1;
        for(u32 k = 0; k < nsaved; k++) if(saved[k].size <= len) want = (i32)k;
        if(want < 0) {
            if(r == FUDE_LOAD_OK && alive_count(&t) != 0) all_ok = false;
        } else {
            if(r != FUDE_LOAD_OK || signature(&t) != saved[want].sig || t.action_count != saved[want].action_count) {
                if(all_ok) printf("  cut at %u: load %d, wanted state %d\n", len, (int)r, want);
                all_ok = false;
            }
        }
        fude_zoom_scene_destroy(&t);
    }
    CHECK(all_ok);

    // A flipped byte in the last journal chunk: opens to the state before it.
    {
        rde_arr ba; u8* bad = table(&ba, sizeof(u8), total); memcpy(bad, whole.memory, total);
        bad[total - 6] ^= 0x5A;
        remove(cut); write_all(cut, bad, total);
        fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
        CHECK(fude_zoom_file_open(&g, cut, &t) == FUDE_LOAD_OK);
        i32 want = -1;
        for(u32 k = 0; k < nsaved; k++) if(saved[k].size < total) want = (i32)k;
        CHECK(want >= 0 && signature(&t) == saved[want].sig);
        fude_zoom_scene_destroy(&t);
        rde_arr_free(&ba);
    }
    // Not a canvas at all: kept as .bad, nothing loaded.
    {
        remove(cut); remove("./saves/cut.zoom.bak"); write_all(cut, (const u8*)"KANA garbage that is not a canvas", 33);
        fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
        CHECK(fude_zoom_file_open(&g, cut, &t) == FUDE_LOAD_CORRUPT);
        struct stat st; CHECK(stat("./saves/cut.zoom.bad", &st) == 0);
        fude_zoom_scene_destroy(&t);
    }
    // Only the backup there (a kill between a compaction's two renames): the canvas is read from it, not opened empty.
    {
        remove(cut); remove("./saves/cut.zoom.bad"); write_all("./saves/cut.zoom.bak", whole.memory, total);
        fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
        CHECK(fude_zoom_file_open(&g, cut, &t) == FUDE_LOAD_RECOVERED);
        CHECK(signature(&t) == saved[nsaved - 1u].sig);
        struct stat st; CHECK(stat(cut, &st) == 0);   // written afresh in its place
        fude_zoom_scene_destroy(&t);
        remove(cut); remove("./saves/cut.zoom.bak");
        fude_zoom_scene u; fude_zoom_scene_init(&u, 7); fude_zoom_file h;
        CHECK(fude_zoom_file_open(&h, cut, &u) == FUDE_LOAD_MISSING);   // neither: nothing saved yet
        fude_zoom_scene_destroy(&u);
    }
    rde_arr_free(&whole);

    // Compaction: the same canvas, smaller, and still opening the same.
    const state before = state_of(&s, &f);
    const u64 big = f.size;
    CHECK(fude_zoom_file_compact(&f, &s));
    CHECK(f.size <= big);
    {
        fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
        CHECK(fude_zoom_file_open(&g, path, &t) == FUDE_LOAD_OK);
        CHECK(signature(&t) == before.sig && t.action_count == before.action_count);
        fude_zoom_scene_destroy(&t);
    }

    // A million points: what it takes on disk.
    {
        remove("./saves/big.zoom");
        fude_zoom_scene b; fude_zoom_scene_init(&b, 7);
        fude_zoom_file bf; memset(&bf, 0, sizeof bf); snprintf(bf.path, sizeof bf.path, "%s", "./saves/big.zoom");
        static fude_zoom_qpoint q[512];
        u64 pts = 0;
        while(pts < 1000000u) {
            pen_stroke(q, 400, 16.0);
            const u32 o = fude_zoom_scene_add_stroke(&b, 0, (fude_zoom_v2){ rndf() * 1e4, rndf() * 1e4 }, -4, q, 400, FUDE_ZOOM_CHANNEL_PRESSURE,
                                                     (rde_color){ 0, 0, 0, 255 }, 1.0f, FUDE_ZOOM_FLAG_PRESSURE, 0, 0);
            fude_zoom_history_push(&b, 0, fude_zoom_scene_object(&b, o)->box, NULL, 0, &o, 1);
            pts += 400;
        }
        CHECK(fude_zoom_file_compact(&bf, &b));
        printf("  a million points: %.2f MB on disk (%.2f bytes a point, everything included)\n", (f64)bf.size / 1e6, (f64)bf.size / (f64)pts);
        CHECK(bf.size < 2500000u);
        fude_zoom_scene_destroy(&b);
    }

    fude_zoom_eraser_destroy(&e);
    fude_zoom_scene_destroy(&s);
}

// A write that fails (a full disk) though its bytes, or some of them, reached the file: while g_sync_fails is set,
// rde_file_sync (RDE's, replaced in this suite) says the write failed. Every other time it says it worked, and the
// bytes reach the file as it closes, which is all a test needs.
static b8 g_sync_fails = false;
b8 rde_file_sync(rde_file* _file) { (void)_file; return !g_sync_fails; }

// A flush that fails part way, then more drawn and saved: reopened, every step after it is there, the failed one's
// too, and the file is whole (zfile.c writes it whole again rather than appending after what the failure left).
static void test_file_failed_write(void) {
    mkdir("./saves", 0755);
    const char* path = "./saves/failed.zoom";
    for(u32 cut = 0; cut <= 7u; cut += 7u) {   // 0: the failed chunk all on disk; 7: its last 7 bytes missing
        remove(path); remove("./saves/failed.zoom.bak"); remove("./saves/failed.zoom.tmp");
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        fude_zoom_file f; memset(&f, 0, sizeof f); snprintf(f.path, sizeof f.path, "%s", path);
        for(u32 k = 0; k < 4; k++) line(&s, k * 10.0, 0, k * 10.0 + 30, 20, 0.7, 1.0f);
        CHECK(fude_zoom_file_flush(&f, &s));   // the file, written whole
        line(&s, 0, 50, 40, 60, 0.7, 1.0f);
        CHECK(fude_zoom_file_flush(&f, &s));   // a journal appended
        line(&s, 0, 80, 40, 90, 0.7, 1.0f);
        g_sync_fails = true;
        CHECK(!fude_zoom_file_flush(&f, &s));  // this one fails...
        g_sync_fails = false;
        if(cut > 0) {                          // ...with only part of it on disk
            rde_arr whole = read_all(path);
            CHECK(rde_arr_length(&whole) > cut);
            write_all(path, whole.memory, (u32)rde_arr_length(&whole) - cut);
            rde_arr_free(&whole);
        }
        // Carrying on: more drawn and flushed, a checkpoint, an undo, more again.
        for(u32 k = 0; k < 3; k++) {
            line(&s, 60, k * 10.0, 90, k * 10.0 + 5, 0.7, 1.0f);
            CHECK(fude_zoom_file_flush(&f, &s));
        }
        CHECK(fude_zoom_file_checkpoint(&f, &s));
        fude_zoom_history_undo(&s);
        line(&s, 100, 0, 120, 30, 0.7, 1.0f);
        CHECK(fude_zoom_file_flush(&f, &s));
        const state want = state_of(&s, &f);
        fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
        CHECK(fude_zoom_file_open(&g, path, &t) == FUDE_LOAD_OK);
        CHECK(!g.repaired);
        CHECK(signature(&t) == want.sig && t.action_count == want.action_count && (u32)rde_arr_length(&t.actions) == want.actions);
        CHECK(alive_count(&t) == alive_count(&s));
        CHECK(g.size == f.size);
        fude_zoom_scene_destroy(&t);
        fude_zoom_scene_destroy(&s);
    }
}

// --- the index kept clean, a canvas loaded whole, the camera's children -------------------------------------------

// Whether a frame's index is past the point where it is built again (scene.c's fude_zoom_scene_reindex): more than 64
// dead and moved-away entries together, and more than half its objects.
static b8 index_over(const fude_zoom_frame* f) {
    const u32 waste = f->dead + f->stale;
    return waste > 64u && waste * 2u > (u32)rde_arr_length(&f->arrival);
}

// A frame's index against what the frame holds. Its entries: one for each object not garbage, one for each dead
// (garbage still in it) and one for each moved-away entry; with nothing moved away, each object not garbage in it
// exactly once. Its answers: _queries random boxes over _span, each finding exactly the alive objects whose boxes meet
// it, each once (brute force). False at the first that does not hold.
static b8 index_matches(const fude_zoom_scene* s, u32 frame, fude_zoom_box span, u32 queries) {
    const fude_zoom_frame* f = fude_zoom_scene_frame(s, frame);
    const u32 n = fude_zoom_scene_object_count(s);
    rde_arr found = rde_arr_new(sizeof(u32), NULL);
    rde_arr marks; u8* mark = table(&marks, sizeof(u8), n + 1u);
    memset(mark, 0, n + 1u);
    u32 kept = 0;
    for(u32 i = 0; i < (u32)rde_arr_length(&f->arrival); i++) {
        kept += (fude_zoom_scene_object(s, ((const u32*)f->arrival.memory)[i])->flags & FUDE_ZOOM_FLAG_GARBAGE) ? 0u : 1u;
    }
    fude_zoom_index_query(&f->index, (fude_zoom_box){ -1e300, -1e300, 1e300, 1e300 }, &found);
    b8 ok = f->index.count == kept + f->dead + f->stale && (u32)rde_arr_length(&found) == f->index.count;
    if(f->stale == 0) {
        u32 once = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&found); i++) {
            const u32 o = ((const u32*)found.memory)[i];
            if(!(fude_zoom_scene_object(s, o)->flags & FUDE_ZOOM_FLAG_GARBAGE)) { ok = ok && mark[o] == 0; mark[o] = 1; once++; }
        }
        ok = ok && once == kept;
    }
    for(u32 k = 0; k < queries && ok; k++) {
        const f64 w = span.max_x - span.min_x, h = span.max_y - span.min_y;
        const f64 x = span.min_x + rndf() * w, y = span.min_y + rndf() * h;
        const fude_zoom_box q = { x, y, x + rndf() * w * 0.3, y + rndf() * h * 0.3 };
        rde_arr_clear(&found);
        memset(mark, 0, n + 1u);
        fude_zoom_scene_query(s, frame, q, &found);
        for(u32 i = 0; i < (u32)rde_arr_length(&found); i++) {
            const u32 o = ((const u32*)found.memory)[i];
            ok = ok && mark[o] == 0;
            mark[o] = 1;
        }
        for(u32 o = 0; o < n; o++) {
            const fude_zoom_object* ob = fude_zoom_scene_object(s, o);
            const b8 want = ob->frame == frame && ob->kind != FUDE_ZOOM_KIND_FRAME && (ob->flags & FUDE_ZOOM_FLAG_ALIVE) && fude_zoom_box_overlaps(ob->box, q);
            ok = ok && (mark[o] != 0) == want;
        }
    }
    rde_arr_free(&marks);
    rde_arr_free(&found);
    return ok;
}

// One random change, of the kinds that leave index entries behind or put objects out of their keys' order as they
// come, in the camera's frame: a stroke, a shape, a text, a picture, a copy elsewhere, a copy on another layer (its key
// the original's, the original let go in the same step), a move, an erase (pieces right after what they were cut
// from; garbage once the history lets the step go), an undo or a redo. Returns false when an add or a copy left an
// entry its object moved away from (each is put into the index once, with its own box).
static b8 varied_step(fude_zoom_scene* s, fude_zoom_eraser* e) {
    static const u8 picture[48] = { 0xFF, 0xD8, 0xFF };
    static const f64 rect[3] = { 20, 10, 0 }, round[2] = { 15, 15 };
    const u32 fr = s->camera.frame;
    const u32 stale = fude_zoom_scene_frame(s, fr)->stale;
    const fude_zoom_place at = { { rndf() * 200, rndf() * 200 }, rndf() * 6.28, 0.5 + rndf() };
    u32 pick = FUDE_ZOOM_NONE;   // something alive here to copy or move
    for(u32 tries = 0; tries < 30 && pick == FUDE_ZOOM_NONE && fude_zoom_scene_object_count(s) > 0; tries++) {
        const u32 o = rnd() % fude_zoom_scene_object_count(s);
        const fude_zoom_object* ob = fude_zoom_scene_object(s, o);
        if(ob->frame == fr && (ob->flags & FUDE_ZOOM_FLAG_ALIVE) && ob->kind != FUDE_ZOOM_KIND_FRAME && ob->kind != FUDE_ZOOM_KIND_LAYER && ob->kind != FUDE_ZOOM_KIND_MARK) pick = o;
    }
    u32 made = FUDE_ZOOM_NONE;
    switch(rnd() % 10) {
        case 0: case 1: {
            const f64 x = rndf() * 200, y = rndf() * 200, a = rndf() * 6.28, len = 10 + rndf() * 60;
            line(s, x, y, x + cos(a) * len, y + sin(a) * len, 0.7, (f32)(0.5 + rndf()));
            break;
        }
        case 2: {
            const b8 r = rnd() % 2 == 0;
            made = fude_zoom_scene_add_shape(s, fr, at, r ? FUDE_ZOOM_SHAPE_RECT : FUDE_ZOOM_SHAPE_ELLIPSE, r ? rect : round, r ? 3u : 2u,
                                             (rde_color){ 1, 2, 3, 255 }, 1.5f, r ? FUDE_ZOOM_FLAG_FILLED : 0u, 0);
            break;
        }
        case 3: made = fude_zoom_scene_add_text(s, fr, at, 12.0, 80.0, 30.0, FUDE_ZOOM_TEXT_STICKY, (rde_color){ 1, 2, 3, 255 }, (rde_color){ 250, 240, 160, 255 }, "a note", 6, 0); break;
        case 4: made = fude_zoom_scene_add_image(s, fr, at, 30.0, 20.0, picture, sizeof picture, 0); break;
        case 5:
            if(pick != FUDE_ZOOM_NONE) {
                made = fude_zoom_scene_copy_stroke(s, pick, fr, fude_zoom_place_moved(fude_zoom_scene_place_of(s, pick), (fude_zoom_sim){ 1.0, 0.0, rndf() * 40 - 20, rndf() * 40 - 20 }));
            }
            break;
        case 6:
            if(pick != FUDE_ZOOM_NONE) {
                const u32 c = fude_zoom_scene_copy_to_layer(s, pick, (u16)(1u + rnd() % 3u), 0);
                if(fude_zoom_scene_frame(s, fr)->stale != stale) return false;
                if(c != FUDE_ZOOM_NONE) {
                    fude_zoom_scene_set_alive(s, pick, false);
                    fude_zoom_history_push(s, fr, fude_zoom_scene_object(s, c)->box, &pick, 1, &c, 1);
                }
            }
            break;
        case 7:
            if(pick != FUDE_ZOOM_NONE) {
                const fude_zoom_place before = fude_zoom_scene_place_of(s, pick);
                const fude_zoom_place after  = fude_zoom_place_moved(before, (fude_zoom_sim){ 1.0, 0.0, rndf() * 60 - 30, rndf() * 60 - 30 });
                fude_zoom_scene_set_place(s, pick, after);
                fude_zoom_history_push_moved(s, fr, fude_zoom_scene_object(s, pick)->box, &pick, &before, &after, 1);
            }
            break;
        case 8: {
            fude_zoom_erase_begin(e, (FUDE_ZOOM_ERASE_)(rnd() % 3));
            for(u32 k = 0; k < 1 + rnd() % 4; k++) {
                const f64 x = rndf() * 200, y = rndf() * 200;
                fude_zoom_erase_step(s, e, fr, (fude_zoom_v2){ x, y }, (fude_zoom_v2){ x + rndf() * 40 - 20, y + rndf() * 40 - 20 }, 1 + rndf() * 4, 0.3);
            }
            fude_zoom_erase_end(s, e, fr, (fude_zoom_box){ 0, 0, 200, 200 });
            break;
        }
        default:
            if(rnd() % 2 == 0) fude_zoom_history_undo(s); else fude_zoom_history_redo(s);
            break;
    }
    if(made != FUDE_ZOOM_NONE) {
        if(fude_zoom_scene_frame(s, fr)->stale != stale) return false;
        fude_zoom_history_push(s, fr, fude_zoom_scene_object(s, made)->box, NULL, 0, &made, 1);
    }
    return true;
}

// The index kept clean: adds (strokes, shapes, texts, pictures), copies (elsewhere, and in place on another layer),
// erases (pieces cut; the history's oldest steps let go, so what they kept becomes garbage) and moves, at random, in a
// root and a child by turns. Adding and copying leave no entry behind (each is put into the index once, with its own
// box); after every step, the dead and moved-away entries are never past the point where the index is built again;
// and every query finds what brute force does, each once.
static void test_index_clean(void) {
    printf("the index kept clean\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_eraser e; fude_zoom_eraser_init(&e);
    s.history_budget = 40000;   // so the oldest steps go, and what only they kept becomes garbage
    line(&s, 0, 0, 50, 50, 0.7, 1.0f);
    const u32 child = fude_zoom_scene_new_frame(&s, s.root, (fude_zoom_xform){ 300.0, 300.0, FUDE_ZOOM_CHILD_SCALE, 0.0 }, (fude_zoom_box){ 300, 300, 301, 301 });
    fude_zoom_camera_look_at(&s, child, (fude_zoom_v2){ 0, 0 }, 1.0);
    line(&s, 0, 0, 50, 50, 0.7, 1.0f);   // (something in it, so the camera leaving does not drop it)
    const u32 frames[2] = { s.root, child };
    const fude_zoom_box span = { -50, -50, 250, 250 };
    b8 added_once = true, clean = true, answers = true;
    u32 rebuilds = 0, waste_was[2] = { 0, 0 };
    for(u32 step = 0; step < 2400; step++) {
        const u32 fr = frames[(step / 150u) % 2u];
        if(s.camera.frame != fr) fude_zoom_camera_look_at(&s, fr, (fude_zoom_v2){ 0, 0 }, 1.0);
        const b8 once = varied_step(&s, &e);
        added_once = added_once && once;
        for(u32 k = 0; k < 2; k++) {
            const fude_zoom_frame* f = fude_zoom_scene_frame(&s, frames[k]);
            clean = clean && !index_over(f);
            rebuilds += f->dead + f->stale < waste_was[k] ? 1u : 0u;
            waste_was[k] = f->dead + f->stale;
        }
        if(step % 80 == 79) answers = answers && index_matches(&s, s.root, span, 12) && index_matches(&s, child, span, 12);
    }
    u32 garbage = 0;
    for(u32 i = 0; i < fude_zoom_scene_object_count(&s); i++) garbage += (fude_zoom_scene_object(&s, i)->flags & FUDE_ZOOM_FLAG_GARBAGE) ? 1u : 0u;
    printf("  %u objects (%u garbage), the indices built again %u times\n", fude_zoom_scene_object_count(&s), garbage, rebuilds);
    CHECK(added_once);
    CHECK(clean);
    CHECK(answers);
    CHECK(rebuilds >= 4u && garbage > 200u);
    fude_zoom_eraser_destroy(&e);
    fude_zoom_scene_destroy(&s);
}

// What a frame's order was when a canvas was loaded one object at a time (fude_zoom_order_insert for each, before
// loading sorted each order once at the end): each of its objects, in the order they were loaded (their indices: a
// loaded canvas's objects are all made while it loads, in the order they come), put after every one whose key is at
// most its own. Into _out (u32, cleared first).
static void order_by_insertion(const fude_zoom_scene* s, u32 frame, rde_arr* out) {
    const rde_arr* order = &fude_zoom_scene_frame(s, frame)->order;
    rde_arr came = rde_arr_new(sizeof(u32), NULL);
    if(rde_arr_length(order) > 0) memcpy(rde_arr_add_n(&came, rde_arr_length(order)), order->memory, rde_arr_length(order) * sizeof(u32));
    rde_arr_sort(&came, fude_zoom_by_u32);
    rde_arr_clear(out);
    for(u32 i = 0; i < (u32)rde_arr_length(&came); i++) {
        const u32 o = ((const u32*)came.memory)[i];
        const u64 z = fude_zoom_scene_object(s, o)->z;
        u32 lo = 0, hi = (u32)rde_arr_length(out);
        while(lo < hi) {
            const u32 mid = (lo + hi) / 2u;
            if(fude_zoom_scene_object(s, ((const u32*)out->memory)[mid])->z <= z) lo = mid + 1u; else hi = mid;
        }
        rde_arr_insert(out, lo, (any)&o);
    }
    rde_arr_free(&came);
}

// The ids of a frame's alive objects, in its order (what is drawn, and over what): the same in a canvas and in the
// canvas reopened, whose indices differ. Into _out (u64, cleared first).
static void drawn_ids(const fude_zoom_scene* s, u32 frame, rde_arr* out) {
    const rde_arr* order = &fude_zoom_scene_frame(s, frame)->order;
    rde_arr_clear(out);
    for(u32 i = 0; i < (u32)rde_arr_length(order); i++) {
        const fude_zoom_object* o = fude_zoom_scene_object(s, ((const u32*)order->memory)[i]);
        if(o->flags & FUDE_ZOOM_FLAG_ALIVE) rde_arr_add(out, (any)&o->id);
    }
}

// Whether two u32 or u64 arrays hold the same.
static b8 same_arr(const rde_arr* a, const rde_arr* b) {
    return rde_arr_length(a) == rde_arr_length(b) && (rde_arr_length(a) == 0 || memcmp(a->memory, b->memory, rde_arr_length(a) * a->data_type_size) == 0);
}

// A canvas loaded whole (zfile.c: its objects sorted into their frames' orders once, and each frame's index built
// once, at the end) is the canvas a load one object at a time made, and the canvas that was saved. In every frame: the
// order one-at-a-time insertion gives; the same alive objects in the same order as the saved canvas; an index with
// nothing dead or moved away in it that answers as brute force does, and as the saved canvas's does; and the same
// bytes when saved again, and again after that. The canvas has what puts objects out of their keys' order as they come
// (pieces an eraser cut, copies on another layer, frames made in between, frames made on top), a new root, and a
// journal after its last checkpoint that replays frames, moves, erases, undos and dropped steps. Peeked (Find's way
// in), it is the same canvas.
static void test_load_whole(void) {
    printf("a canvas loaded whole\n");
    mkdir("./saves", 0755);
    const char* path = "./saves/whole.zoom";
    remove(path); remove("./saves/whole.zoom.bak");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_eraser e; fude_zoom_eraser_init(&e);
    fude_zoom_file f; memset(&f, 0, sizeof f); snprintf(f.path, sizeof f.path, "%s", path);
    s.history_budget = 120000;
    const fude_zoom_v2 half = { 500, 400 };
    for(u32 round = 0; round < 60; round++) {
        for(u32 k = 0; k < 15; k++) varied_step(&s, &e);
        const u32 way = rnd() % 4;
        if(way == 0 && fude_zoom_scene_frame(&s, s.camera.frame)->parent == FUDE_ZOOM_NONE) {
            // Into a child (one there, or a new one), drawn in on top of what came later in the parent.
            fude_zoom_camera_look_at(&s, s.camera.frame, (fude_zoom_v2){ rndf() * 200, rndf() * 200 }, 1.0);
            for(u32 k = 0; k < 7; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&s, half); }
            fude_zoom_camera_drawing_frame(&s, (fude_zoom_box){ s.camera.at.x - 1, s.camera.at.y - 1, s.camera.at.x + 1, s.camera.at.y + 1 });
            line(&s, s.camera.at.x - 10, s.camera.at.y, s.camera.at.x + 10, s.camera.at.y + 3, 0.5, 0.5f);
        } else if(way == 1 && fude_zoom_scene_frame(&s, s.camera.frame)->parent != FUDE_ZOOM_NONE) {
            for(u32 k = 0; k < 7; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 0.5); fude_zoom_camera_settle(&s, half); }
        }
        if(round == 30) {   // out past the root: a new root above
            fude_zoom_camera_look_at(&s, s.root, (fude_zoom_v2){ 100, 100 }, 1.0);
            for(u32 k = 0; k < 8; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 0.5); fude_zoom_camera_settle(&s, half); }
            for(u32 k = 0; k < 8; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&s, half); }
        }
        if(round % 3 == 2) CHECK(fude_zoom_file_flush(&f, &s));
        if(round % 10 == 9 && round < 50) CHECK(fude_zoom_file_checkpoint(&f, &s));
    }
    CHECK(fude_zoom_file_flush(&f, &s));

    fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
    CHECK(fude_zoom_file_open(&g, path, &t) == FUDE_LOAD_OK && !g.repaired);
    rde_arr ref = rde_arr_new(sizeof(u32), NULL), a = rde_arr_new(sizeof(u64), NULL), b = rde_arr_new(sizeof(u64), NULL);
    rde_arr qs = rde_arr_new(sizeof(u32), NULL), qt = rde_arr_new(sizeof(u32), NULL);
    b8 inserted = true, drawn = true, indexed = true, answered = true;
    u32 objects = 0, out_of_order = 0;
    for(u32 fi = 0; fi < fude_zoom_scene_frame_count(&t); fi++) {
        const fude_zoom_frame* tf = fude_zoom_scene_frame(&t, fi);
        order_by_insertion(&t, fi, &ref);
        inserted = inserted && same_arr(&ref, &tf->order);
        objects += (u32)rde_arr_length(&tf->order);
        for(u32 i = 1; i < (u32)rde_arr_length(&tf->order); i++) {
            out_of_order += ((const u32*)tf->order.memory)[i] < ((const u32*)tf->order.memory)[i - 1] ? 1u : 0u;
        }
        const u32 sf = fude_zoom_scene_find_frame(&s, tf->id);
        if(sf == FUDE_ZOOM_NONE) { drawn = drawn && rde_arr_length(&tf->order) == 0; continue; }
        drawn_ids(&s, sf, &a); drawn_ids(&t, fi, &b);
        drawn = drawn && same_arr(&a, &b);
        indexed = indexed && tf->dead == 0 && tf->stale == 0 && index_matches(&t, fi, (fude_zoom_box){ -300, -300, 300, 300 }, 20);
        // The same answers as the saved canvas's index (by id: its indices differ).
        for(u32 k = 0; k < 20; k++) {
            const f64 x = rndf() * 400 - 200, y = rndf() * 400 - 200;
            const fude_zoom_box q = { x, y, x + rndf() * 120, y + rndf() * 120 };
            rde_arr_clear(&qs); rde_arr_clear(&qt); rde_arr_clear(&a); rde_arr_clear(&b);
            fude_zoom_scene_query(&s, sf, q, &qs); fude_zoom_scene_query(&t, fi, q, &qt);
            for(u32 i = 0; i < (u32)rde_arr_length(&qs); i++) rde_arr_add(&a, (any)&fude_zoom_scene_object(&s, ((const u32*)qs.memory)[i])->id);
            for(u32 i = 0; i < (u32)rde_arr_length(&qt); i++) rde_arr_add(&b, (any)&fude_zoom_scene_object(&t, ((const u32*)qt.memory)[i])->id);
            rde_arr_sort(&a, rde_sort_compare_u64); rde_arr_sort(&b, rde_sort_compare_u64);
            answered = answered && same_arr(&a, &b);
        }
    }
    printf("  %u frames, %u objects in their orders, %u of them come before one loaded earlier\n", fude_zoom_scene_frame_count(&t), objects, out_of_order);
    CHECK(inserted);
    CHECK(drawn);
    CHECK(indexed);
    CHECK(answered);
    CHECK(out_of_order > 20u);

    // Peeked: the same canvas, index for index.
    {
        fude_zoom_scene v; fude_zoom_scene_init(&v, 7);
        CHECK(fude_zoom_file_peek(path, &v) == FUDE_LOAD_OK);
        b8 same = fude_zoom_scene_frame_count(&v) == fude_zoom_scene_frame_count(&t);
        for(u32 fi = 0; fi < fude_zoom_scene_frame_count(&t) && same; fi++) {
            same = same_arr(&fude_zoom_scene_frame(&t, fi)->order, &fude_zoom_scene_frame(&v, fi)->order);
            for(u32 k = 0; k < 10 && same; k++) {
                const f64 x = rndf() * 400 - 200, y = rndf() * 400 - 200;
                rde_arr_clear(&qs); rde_arr_clear(&qt);
                fude_zoom_scene_query(&t, fi, (fude_zoom_box){ x, y, x + 100, y + 100 }, &qs);
                fude_zoom_scene_query(&v, fi, (fude_zoom_box){ x, y, x + 100, y + 100 }, &qt);
                same = same_arr(&qs, &qt);
            }
        }
        CHECK(same);
        fude_zoom_scene_destroy(&v);
    }

    // Saved again and opened, twice: the same bytes. (The first save's frames are in the order the file and the
    // journal brought them, which its chunks follow; once saved in the file's order, the canvas saves the same.)
    {
        const char* names[3] = { "./saves/whole2.zoom", "./saves/whole3.zoom", "./saves/whole4.zoom" };
        fude_zoom_scene u[3];
        for(u32 k = 0; k < 3; k++) {
            remove(names[k]);
            fude_zoom_scene* from = k == 0 ? &t : &u[k - 1];
            fude_zoom_file to; memset(&to, 0, sizeof to); snprintf(to.path, sizeof to.path, "%s", names[k]);
            CHECK(fude_zoom_file_compact(&to, from));
            fude_zoom_scene_init(&u[k], 7); fude_zoom_file h;
            CHECK(fude_zoom_file_open(&h, names[k], &u[k]) == FUDE_LOAD_OK);
        }
        rde_arr b3 = read_all(names[1]), b4 = read_all(names[2]);
        CHECK(rde_arr_length(&b3) > 1000u && same_arr(&b3, &b4));
        rde_arr_free(&b3); rde_arr_free(&b4);
        for(u32 k = 0; k < 3; k++) fude_zoom_scene_destroy(&u[k]);
    }
    rde_arr_free(&ref); rde_arr_free(&a); rde_arr_free(&b); rde_arr_free(&qs); rde_arr_free(&qt);
    fude_zoom_scene_destroy(&t);
    fude_zoom_eraser_destroy(&e);
    fude_zoom_scene_destroy(&s);
}

// The file's CRC-32 (zfile.c: eight bytes a step) against the byte-at-a-time one it replaced, kept here: the check
// value of "123456789", then random bytes of every length to 300 from every start to 7 (so every tail and every
// alignment), and a megabyte; the eight-byte steps timed against the one-byte ones.
static u32 crc_bytewise(const u8* d, u32 n) {
    static u32 t[256];
    for(u32 i = 0; i < 256u; i++) { u32 c = i; for(u32 k = 0; k < 8u; k++) c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1; t[i] = c; }
    u32 crc = 0xFFFFFFFFu;
    for(u32 i = 0; i < n; i++) crc = t[(crc ^ d[i]) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}
static void test_crc(void) {
    printf("the file's CRC-32\n");
    CHECK(fude_zoom_crc32((const u8*)"123456789", 9) == 0xCBF43926u);
    CHECK(fude_zoom_crc32(NULL, 0) == 0u);
    rde_arr ba; u8* b = table(&ba, sizeof(u8), 1u << 20);
    for(u32 i = 0; i < (1u << 20); i++) b[i] = (u8)rnd();
    b8 same = true;
    for(u32 at = 0; at < 8u; at++) {
        for(u32 n = 0; n <= 300u; n++) same = same && fude_zoom_crc32(b + at, n) == crc_bytewise(b + at, n);
    }
    CHECK(same);
    const clock_t c0 = clock();
    const u32 fast = fude_zoom_crc32(b, 1u << 20);
    const f64 ms_fast = (f64)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
    const clock_t c1 = clock();
    const u32 slow = crc_bytewise(b, 1u << 20);
    const f64 ms_slow = (f64)(clock() - c1) * 1000.0 / CLOCKS_PER_SEC;
    CHECK(fast == slow);
    printf("  a megabyte: %.2f ms (a byte at a time: %.2f ms)\n", ms_fast, ms_slow);
    rde_arr_free(&ba);
}

// How many chunks the file at _path has, and of them how many are buckets (BKTS): each read by its header.
static u32 file_chunks(const char* path, u32* buckets) {
    rde_arr d = read_all(path);
    const u8* p = (const u8*)d.memory;
    u32 n = 0;
    *buckets = 0;
    for(u64 at = FUDE_FILE_HEADER_SIZE; at + 8u <= rde_arr_length(&d); n++) {
        u32 tag, size;
        memcpy(&tag, p + at, 4); memcpy(&size, p + at + 4u, 4);
        *buckets += tag == FUDE_TAG('B', 'K', 'T', 'S') ? 1u : 0u;
        at += 8u + size;
    }
    rde_arr_free(&d);
    return n;
}

// Loading a big canvas, timed (the app logs it as "canvas: ... (loaded in N ms)"): 30,000 strokes in its home frame,
// 6,000 pieces cut from old ones long after (each goes back in among the oldest: a key just after its stroke's), and
// 3,000 frames of three strokes each, saved through many checkpoints and a journal, so the index lists thousands of
// buckets among thousands of chunks. With it, what the old way spent on two of its parts, kept here to compare: each
// object put into its frame's order one at a time (a binary search, then a move of everything after its place), and
// into its frame's index one at a time before the index was built packed anyway.
static void test_load_big(void) {
    printf("a big canvas loaded\n");
    mkdir("./saves", 0755);
    const char* path = "./saves/big_load.zoom";
    remove(path); remove("./saves/big_load.zoom.bak");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_file f; memset(&f, 0, sizeof f); snprintf(f.path, sizeof f.path, "%s", path);
    enum { STROKES = 30000, PIECES = 6000, FRAMES = 3000 };
    static fude_zoom_qpoint q[32];
    const u32 root = s.root;
    for(u32 i = 0; i < STROKES; i++) {
        pen_stroke(q, 32, 16.0);
        fude_zoom_scene_add_stroke(&s, root, (fude_zoom_v2){ rndf() * 1e4, rndf() * 1e4 }, -4, q, 32, FUDE_ZOOM_CHANNEL_PRESSURE, (rde_color){ 0, 0, 0, 255 }, 1.0f,
                                   FUDE_ZOOM_FLAG_PRESSURE, 0, 0);
        if(i == 100) CHECK(fude_zoom_file_flush(&f, &s));   // the file, written whole
        if(i % 3000 == 2999) CHECK(fude_zoom_file_checkpoint(&f, &s));
    }
    for(u32 k = 0; k < FRAMES; k++) {
        const f64 x = rndf() * 1e4, y = rndf() * 1e4;
        const u32 fr = fude_zoom_scene_new_frame(&s, root, (fude_zoom_xform){ x, y, FUDE_ZOOM_CHILD_SCALE, 0.0 }, (fude_zoom_box){ x, y, x + 0.5, y + 0.5 });
        for(u32 j = 0; j < 3; j++) {
            pen_stroke(q, 32, 16.0);
            fude_zoom_scene_add_stroke(&s, fr, (fude_zoom_v2){ rndf() * 300, rndf() * 300 }, -4, q, 32, FUDE_ZOOM_CHANNEL_PRESSURE, (rde_color){ 0, 0, 0, 255 }, 1.0f,
                                       FUDE_ZOOM_FLAG_PRESSURE, 0, 0);
        }
        if(k % 500 == 499) CHECK(fude_zoom_file_checkpoint(&f, &s));
    }
    for(u32 p = 0; p < PIECES; p++) {
        const u64 z = fude_zoom_scene_object(&s, rnd() % STROKES)->z + 1u + p;
        pen_stroke(q, 16, 16.0);
        fude_zoom_scene_add_stroke(&s, root, (fude_zoom_v2){ rndf() * 1e4, rndf() * 1e4 }, -4, q, 16, FUDE_ZOOM_CHANNEL_PRESSURE, (rde_color){ 0, 0, 0, 255 }, 1.0f,
                                   FUDE_ZOOM_FLAG_PRESSURE, 0, z);
        if(p % 2000 == 1999) CHECK(fude_zoom_file_checkpoint(&f, &s));
    }
    for(u32 k = 0; k < 200; k++) {   // a journal after the last checkpoint: some moved
        const u32 o = rnd() % STROKES;
        fude_zoom_scene_set_place(&s, o, fude_zoom_place_moved(fude_zoom_scene_place_of(&s, o), (fude_zoom_sim){ 1.0, 0.0, 5.0, 5.0 }));
    }
    CHECK(fude_zoom_file_flush(&f, &s));
    u32 buckets = 0;
    const u32 chunks = file_chunks(path, &buckets);

    const clock_t c0 = clock();
    fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
    CHECK(fude_zoom_file_open(&g, path, &t) == FUDE_LOAD_OK);
    const f64 ms = (f64)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
    printf("  %u objects in %u frames, %.1f MB, %u chunks (%u of them buckets): opened in %.1f ms\n", fude_zoom_scene_object_count(&t),
           fude_zoom_scene_frame_count(&t), (f64)g.size / 1e6, chunks, buckets, ms);
    CHECK(chunks > 3000u && fude_zoom_scene_object_count(&t) == fude_zoom_scene_object_count(&s));

    // The old way's parts: into the orders one at a time (also the reference for the orders loading gives), and into
    // the indices one at a time.
    rde_arr ref = rde_arr_new(sizeof(u32), NULL);
    b8 inserted = true;
    const clock_t c1 = clock();
    for(u32 fi = 0; fi < fude_zoom_scene_frame_count(&t); fi++) {
        order_by_insertion(&t, fi, &ref);
        inserted = inserted && same_arr(&ref, &fude_zoom_scene_frame(&t, fi)->order);
    }
    const f64 ms_order = (f64)(clock() - c1) * 1000.0 / CLOCKS_PER_SEC;
    CHECK(inserted);
    const clock_t c2 = clock();
    for(u32 fi = 0; fi < fude_zoom_scene_frame_count(&t); fi++) {
        const fude_zoom_frame* tf = fude_zoom_scene_frame(&t, fi);
        fude_zoom_index one; fude_zoom_index_init(&one);
        for(u32 i = 0; i < (u32)rde_arr_length(&tf->arrival); i++) {
            const u32 o = ((const u32*)tf->arrival.memory)[i];
            fude_zoom_index_insert(&one, o, fude_zoom_scene_object(&t, o)->box);
        }
        fude_zoom_index_destroy(&one);
    }
    const f64 ms_index = (f64)(clock() - c2) * 1000.0 / CLOCKS_PER_SEC;
    printf("  the old way also spent: %.1f ms putting them into their orders one at a time, %.1f ms into their indices\n", ms_order, ms_index);
    rde_arr_free(&ref);
    fude_zoom_scene_destroy(&t);
    fude_zoom_scene_destroy(&s);
}

// The camera's way into a child (scene.c's fude_zoom_camera_child_at), as a walk of every object in the frame's order
// finds it: the most recently visited alive child whose anchor holds the point, the first in the order on a tie;
// FUDE_ZOOM_NONE when none does.
static u32 child_by_order(const fude_zoom_scene* s, u32 frame, fude_zoom_v2 at) {
    const fude_zoom_frame* f = fude_zoom_scene_frame(s, frame);
    u32 best = FUDE_ZOOM_NONE;
    for(u32 i = 0; i < (u32)rde_arr_length(&f->order); i++) {
        const fude_zoom_object* o = fude_zoom_scene_object(s, ((const u32*)f->order.memory)[i]);
        if(o->kind != FUDE_ZOOM_KIND_FRAME || !(o->flags & FUDE_ZOOM_FLAG_ALIVE)) continue;
        const fude_zoom_frame* c = fude_zoom_scene_frame(s, o->child);
        if(!fude_zoom_box_contains(c->anchor, at)) continue;
        if(best == FUDE_ZOOM_NONE || c->visited > fude_zoom_scene_frame(s, best)->visited) best = o->child;
    }
    return best;
}

// Zooms in at 1000 random points of frame _frame (over _span, in its units), and pans out of its children to 1000 more,
// each against child_by_order: zoomed in, the camera goes into the child that walk picks, or into a new one where none
// holds the point; panned out of a child's anchor, it is handed to the sibling that walk picks there, or stays where
// none does. Adds what happened to _counts (into a child, into a new one, handed over); false when it went anywhere
// else.
static b8 children_walk(fude_zoom_scene* s, u32 frame, fude_zoom_box span, u32* counts) {
    const fude_zoom_v2 half = { 500, 400 };
    const f64 w = span.max_x - span.min_x, h = span.max_y - span.min_y;
    b8 same = true;
    for(u32 k = 0; k < 1000; k++) {
        const fude_zoom_v2 at = { span.min_x + rndf() * w, span.min_y + rndf() * h };
        fude_zoom_camera_look_at(s, frame, at, 100.0);   // past FUDE_ZOOM_Z_MAX: into a child as it settles
        const u32 want = child_by_order(s, frame, at);     // (the frame left, if made for the camera, dropped first)
        const u32 frames = fude_zoom_scene_frame_count(s);
        fude_zoom_camera_settle(s, half);
        if(want != FUDE_ZOOM_NONE) { same = same && s->camera.frame == want; counts[0]++; }
        else { same = same && fude_zoom_scene_frame_count(s) == frames + 1u && s->camera.frame == frames; counts[1]++; }
    }
    for(u32 k = 0; k < 1000; k++) {
        u32 c = FUDE_ZOOM_NONE;   // a random alive child with something in it
        while(c == FUDE_ZOOM_NONE) {
            const fude_zoom_object* ob = fude_zoom_scene_object(s, rnd() % fude_zoom_scene_object_count(s));
            if(ob->kind == FUDE_ZOOM_KIND_FRAME && (ob->flags & FUDE_ZOOM_FLAG_ALIVE) && ob->frame == frame && fude_zoom_scene_frame_used(s, ob->child)) c = ob->child;
        }
        const fude_zoom_v2 at = { span.min_x + rndf() * w, span.min_y + rndf() * h };
        const fude_zoom_xform xf = fude_zoom_scene_frame(s, c)->xf;
        fude_zoom_camera_look_at(s, c, (fude_zoom_v2){ (at.x - xf.ox) / xf.scale, (at.y - xf.oy) / xf.scale }, 1.0);
        const u32 other = child_by_order(s, frame, at);
        const b8  stays = fude_zoom_box_contains(fude_zoom_scene_frame(s, c)->anchor, at) || other == FUDE_ZOOM_NONE;
        fude_zoom_camera_settle(s, half);
        same = same && s->camera.frame == (stays ? c : other);
        counts[2] += stays ? 0u : 1u;
    }
    return same;
}

// Makes _n children of _frame, anchored at random over _span (each 2% to 20% of it across), each with a stroke in it,
// one in eight deleted, a quarter visited at some time before now (the rest never, as after opening a canvas), and
// strokes of _frame's own among them.
static void children_made(fude_zoom_scene* s, u32 frame, fude_zoom_box span, u32 n) {
    static fude_zoom_qpoint q[16];
    const f64 w = span.max_x - span.min_x, h = span.max_y - span.min_y;
    for(u32 k = 0; k < n; k++) {
        const f64 x = span.min_x + rndf() * w, y = span.min_y + rndf() * h, cw = (0.02 + rndf() * 0.18) * w, ch = (0.02 + rndf() * 0.18) * h;
        const u32 c = fude_zoom_scene_new_frame(s, frame, (fude_zoom_xform){ x, y, FUDE_ZOOM_CHILD_SCALE, 0.0 }, (fude_zoom_box){ x, y, x + cw, y + ch });
        pen_stroke(q, 16, 16.0);
        fude_zoom_scene_add_stroke(s, c, (fude_zoom_v2){ 0, 0 }, -4, q, 16, 0, (rde_color){ 0, 0, 0, 255 }, 1.0f, 0, 0, 0);
        if(rnd() % 8 == 0) fude_zoom_scene_set_alive(s, fude_zoom_scene_frame(s, c)->object, false);
        fude_zoom_scene_frame(s, c)->visited = rnd() % 4 == 0 ? 1u + rnd() % 40u : 0u;
        for(u32 j = 0; j < rnd() % 4; j++) {
            pen_stroke(q, 16, 16.0);
            fude_zoom_scene_add_stroke(s, frame, (fude_zoom_v2){ span.min_x + rndf() * w, span.min_y + rndf() * h }, -4, q, 16, 0, (rde_color){ 0, 0, 0, 255 }, 1.0f, 0, 0, 0);
        }
    }
}

// The camera's children (children_walk): 300 overlapping children of the canvas's first root, among its strokes; then
// a new root above it, and 100 more children of the new root over the old one, which is one of its children too. The
// same again after the canvas is reopened, when nothing has been visited and the new root lists its children in
// another order than their keys' (the old root, the first by its key, comes last: the file has its frames parents
// first).
static void test_child_at(void) {
    printf("the camera's children\n");
    mkdir("./saves", 0755);
    remove("./saves/children.zoom"); remove("./saves/children.zoom.bak");
    fude_zoom_scene s0; fude_zoom_scene_init(&s0, 7);
    const u32 first = s0.root;
    const fude_zoom_box span = { -50, -50, 1050, 1050 };
    children_made(&s0, first, (fude_zoom_box){ 0, 0, 1000, 1000 }, 300);
    fude_zoom_scene_new_root(&s0);
    const fude_zoom_box old = fude_zoom_scene_frame(&s0, first)->anchor;
    const fude_zoom_box over = { old.min_x - 0.2 * (old.max_x - old.min_x), old.min_y - 0.2 * (old.max_y - old.min_y),
                                 old.max_x + 0.2 * (old.max_x - old.min_x), old.max_y + 0.2 * (old.max_y - old.min_y) };
    children_made(&s0, s0.root, over, 100);
    s0.clock = 100;
    fude_zoom_file f; memset(&f, 0, sizeof f); snprintf(f.path, sizeof f.path, "%s", "./saves/children.zoom");
    CHECK(fude_zoom_file_flush(&f, &s0));
    fude_zoom_scene s1; fude_zoom_scene_init(&s1, 7); fude_zoom_file g;
    CHECK(fude_zoom_file_open(&g, "./saves/children.zoom", &s1) == FUDE_LOAD_OK);
    s1.clock = 100;
    {
        // Reopened, the new root's first child by key is its last listed.
        const fude_zoom_frame* r = fude_zoom_scene_frame(&s1, s1.root);
        const u32 last = ((const u32*)r->kids.memory)[rde_arr_length(&r->kids) - 1u];
        CHECK(fude_zoom_scene_object(&s1, last)->child == fude_zoom_scene_find_frame(&s1, fude_zoom_scene_frame(&s0, first)->id));
        CHECK(((const u32*)r->order.memory)[0] == last);
    }
    fude_zoom_scene* scenes[2] = { &s0, &s1 };
    for(u32 w = 0; w < 2; w++) {
        fude_zoom_scene* s = scenes[w];
        const u32 at_first = fude_zoom_scene_find_frame(s, fude_zoom_scene_frame(&s0, first)->id);
        u32 inner[3] = { 0, 0, 0 }, outer[3] = { 0, 0, 0 };
        CHECK(children_walk(s, at_first, span, inner));
        CHECK(children_walk(s, s->root, over, outer));
        printf("  %s: zoomed into a child %u and %u times, into a new one %u and %u, handed to a sibling %u and %u\n", w == 0 ? "made" : "reopened",
               inner[0], outer[0], inner[1], outer[1], inner[2], outer[2]);
        CHECK(inner[0] > 300u && inner[1] > 30u && inner[2] > 300u && outer[0] > 300u && outer[1] > 30u && outer[2] > 300u);
    }
    fude_zoom_scene_destroy(&s1);
    fude_zoom_scene_destroy(&s0);
}

// A piece renamed while its file cannot be written (a full disk: g_sync_fails): said so (false; it was not), its new
// name kept until the app closes and its old one in its file; written again, true, and the file has the new name.
static void test_piece_rename_failed(void) {
    printf("piece renamed, its file not written\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 st = line(&s, 0, 0, 100, 0, 2.0, 2.0f);
    rde_arr clips = rde_arr_new(sizeof(fude_zoom_clip), NULL);
    fude_zoom_select_clips_of(&s, &st, 1u, &clips);
    const c8* dir = "./pieces_rename_test/";
    rde_file_create_missing_dirs("./pieces_rename_test/x.piece");
    rde_file_delete("./pieces_rename_test/index.kana");
    fude_zoom_pieces lib;
    memset(&lib, 0, sizeof(lib));
    fude_zoom_pieces_load(&lib, dir);
    CHECK(fude_zoom_pieces_add(&lib, dir, "Before", (const fude_zoom_clip*)clips.memory, 1u, fude_zoom_scene_object(&s, st)->box) == 0u);
    g_sync_fails = true;
    CHECK(!fude_zoom_pieces_rename(&lib, dir, 0u, "After"));
    g_sync_fails = false;
    CHECK(strcmp(lib.list[0].name, "After") == 0);
    fude_zoom_pieces back;
    memset(&back, 0, sizeof(back));
    fude_zoom_pieces_load(&back, dir);
    CHECK(back.count == 1u && strcmp(back.list[0].name, "Before") == 0);
    fude_zoom_pieces_free(&back);
    CHECK(fude_zoom_pieces_rename(&lib, dir, 0u, "After"));
    fude_zoom_pieces_load(&back, dir);
    CHECK(back.count == 1u && strcmp(back.list[0].name, "After") == 0);
    fude_zoom_pieces_free(&back);
    fude_zoom_pieces_free(&lib);
    fude_zoom_select_clips_free(&clips);
    rde_arr_free(&clips);
    fude_zoom_scene_destroy(&s);
}

// --- logic on the canvas (logic.h): gates, latches, flip-flops, chips on a breadboard, custom parts, all run by the
// simulation's digital engine; bridged where they meet LEDs, resistors, switches and batteries --------------------

#include "../support/parts.h"

// A part's pin by its name (a chip's, a custom part's); NONE: none so named.
static u32 pin_named(const fude_zoom_scene* s, u32 o, const c8* name) {
    const fude_zoom_part* p = fude_zoom_part_of(s, o);
    for(u32 k = 0; p != NULL && k < p->pin_count; k++) if(p->pins[k].name != NULL && strcmp(p->pins[k].name, name) == 0) return k;
    return FUDE_ZOOM_NONE;
}

static fude_zoom_circuit_part* cpart(fude_zoom_circuit* c, u32 object) {
    for(u32 i = 0; i < (u32)rde_arr_length(&c->parts); i++) if(((fude_zoom_circuit_part*)c->parts.memory)[i].object == object) return &((fude_zoom_circuit_part*)c->parts.memory)[i];
    return NULL;
}

// A logic input set (as tapped) and the circuit run a step on.
static void set_in(fude_zoom_circuit* c, u32 object, u32 v) {
    fude_zoom_circuit_part* p = cpart(c, object);
    CHECK(p != NULL);
    if(p != NULL) p->switch_on = v & 1u;
}
static b8 step(fude_zoom_circuit* c) { return fude_zoom_circuit_run(c, 1e-3, 4u); }
static u32 probe(fude_zoom_circuit* c, u32 object) { const fude_zoom_circuit_part* p = cpart(c, object); return p != NULL && p->shown > 0.5 ? 1u : 0u; }

// A chip seated on breadboard bb across its gap (rows e and f), its pin 1 at column col (as a person seats it: turned a
// quarter, its notch to the left, pins 1 to n/2 along row f, the rest back along row e).
static u32 seat_chip(fude_zoom_scene* s, u32 bb, const c8* id, u32 col, u32 pins) {
    fude_zoom_v2 e0, f0, f1;
    const u32 cols = FUDE_ZOOM_BREADBOARD_COLS;
    CHECK(fude_zoom_part_pin_at(s, bb, 6u * cols + col, &e0) && fude_zoom_part_pin_at(s, bb, 7u * cols + col, &f0) && fude_zoom_part_pin_at(s, bb, 7u * cols + col + 1u, &f1));
    const f64 pitch = f1.x - f0.x, half = (f64)(pins / 2u);
    const f64 hw = (e0.y - f0.y) * 0.5, hh = pitch * (half + 1.0) * 0.5;
    const u32 o = part_put(s, id, f0.x + pitch * (half - 1.0) * 0.5, (e0.y + f0.y) * 0.5, hw, hh, "");
    fude_zoom_place pl = fude_zoom_scene_place_of(s, o); pl.rotation = 1.5707963267948966; fude_zoom_scene_set_place(s, o, pl);
    // (each pin on its hole)
    for(u32 k = 0; k < pins; k++) {
        fude_zoom_v2 at, hole;
        const u32 h = k < pins / 2u ? 7u * cols + col + k : 6u * cols + col + (pins - 1u - k);
        CHECK(fude_zoom_part_pin_at(s, o, k, &at) && fude_zoom_part_pin_at(s, bb, h, &hole));
        CHECK(hypot(at.x - hole.x, at.y - hole.y) < 1e-3);   // (pins' places are f32s)
    }
    return o;
}

// A chip's pin k's strip's free hole (a row away from the chip, on its side), for a wire.
static u32 strip_hole(u32 col, u32 pins, u32 k, u32 rows_away) {
    const u32 cols = FUDE_ZOOM_BREADBOARD_COLS;
    return k < pins / 2u ? (7u + rows_away) * cols + col + k : (6u - rows_away) * cols + col + (pins - 1u - k);
}

// A straight wire from part a's pin pa to b's pb (no route round: on a breadboard, its ends' columns kept clear).
static u32 wire_line(fude_zoom_scene* s, u32 a, u32 pa, u32 b, u32 pb) {
    fude_zoom_v2 r[2];
    CHECK(fude_zoom_part_pin_at(s, a, pa, &r[0]) && fude_zoom_part_pin_at(s, b, pb, &r[1]));
    return fude_zoom_wire_add(s, s->root, r, 2u, a, (i32)pa, b, (i32)pb, (rde_color){ 1, 1, 1, 255 }, 0.1f);
}

static fude_zoom_v2 hole_at(const fude_zoom_scene* s, u32 bb, u32 row, u32 col) {
    fude_zoom_v2 p = { 0, 0 };
    CHECK(fude_zoom_part_pin_at(s, bb, row * FUDE_ZOOM_BREADBOARD_COLS + col, &p));
    return p;
}

static void test_canvas_logic(void) {
    // Every gate's table, drawn: two switches into it, a probe on its output.
    {
        const c8* const gates[8] = { "AND gate", "OR gate", "NAND gate", "NOR gate", "XOR gate", "XNOR gate", "NOT gate", "buffer" };
        u32 right = 0, cases = 0;
        for(u32 g = 0; g < 8u; g++) {
            fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
            const u32 a = part_put(&s, "logic input", 0, 40, 30, 20, "0"), b = part_put(&s, "logic input", 0, -40, 30, 20, "0");
            const u32 gt = part_put(&s, gates[g], 150, 0, 40, 30, ""), pr = part_put(&s, "logic probe", 300, 0, 20, 20, "");
            const b8 one = g >= 6u;
            wire_put(&s, a, 0, gt, 0);
            if(!one) wire_put(&s, b, 0, gt, 1);
            wire_put(&s, gt, one ? 1u : 2u, pr, 0);
            fude_zoom_circuit c; fude_zoom_circuit_init(&c);
            fude_zoom_circuit_build(&c, &s);
            for(u32 v = 0; v < 4u; v++) {
                const u32 x = v & 1u, y = v >> 1;
                set_in(&c, a, x); set_in(&c, b, y);
                CHECK(step(&c));
                const u32 want = g == 0 ? (x & y) : g == 1 ? (x | y) : g == 2 ? !(x & y) : g == 3 ? !(x | y) : g == 4 ? (x ^ y) : g == 5 ? !(x ^ y) : g == 6 ? !x : x;
                cases++; right += probe(&c, pr) == want;
            }
            fude_zoom_circuit_destroy(&c);
            fude_zoom_scene_destroy(&s);
        }
        CHECK(right == cases && cases == 32u);
    }
    // A latch of two NANDs drawn, crossed: set, held, reset, held — only logic on its nodes (one net each).
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 ns = part_put(&s, "logic input", 0, 60, 30, 20, "1"), nr = part_put(&s, "logic input", 0, -60, 30, 20, "1");
        const u32 g1 = part_put(&s, "NAND gate", 150, 60, 40, 30, ""), g2 = part_put(&s, "NAND gate", 150, -60, 40, 30, "");
        const u32 q = part_put(&s, "logic probe", 320, 60, 20, 20, ""), nq = part_put(&s, "logic probe", 320, -60, 20, 20, "");
        wire_put(&s, ns, 0, g1, 0); wire_put(&s, nr, 0, g2, 1);
        wire_put(&s, g1, 2, g2, 0); wire_put(&s, g2, 2, g1, 1);   // (crossed)
        wire_put(&s, g1, 2, q, 0); wire_put(&s, g2, 2, nq, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        set_in(&c, ns, 0); CHECK(step(&c) && probe(&c, q) == 1u && probe(&c, nq) == 0u);
        set_in(&c, ns, 1); CHECK(step(&c) && probe(&c, q) == 1u && probe(&c, nq) == 0u);
        set_in(&c, nr, 0); CHECK(step(&c) && probe(&c, q) == 0u && probe(&c, nq) == 1u);
        set_in(&c, nr, 1); CHECK(step(&c) && probe(&c, q) == 0u && probe(&c, nq) == 1u);
        // Built again (the canvas changed): what it held kept.
        fude_zoom_circuit_build(&c, &s);
        CHECK(step(&c) && probe(&c, q) == 0u && probe(&c, nq) == 1u);
        CHECK(c.logic->on && rde_arr_length(&c.logic->bridges) == 0u);   // (nothing analog: no bridge)
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
    // A ripple counter of four T flip-flops (each clocked by the last's /Q), a switch its clock; and a D flip-flop.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 one = part_put(&s, "logic input", -200, 200, 30, 20, "1"), ck = part_put(&s, "logic input", -200, 0, 30, 20, "0");
        u32 t[4], pr[4];
        for(u32 i = 0; i < 4u; i++) {
            t[i] = part_put(&s, "T flip-flop", (f64)i * 200.0, 0, 40, 40, "");
            pr[i] = part_put(&s, "logic probe", (f64)i * 200.0 + 60.0, 150, 20, 20, "");
            wire_put(&s, one, 0, t[i], 0);
            wire_put(&s, i == 0u ? ck : t[i - 1u], i == 0u ? 0u : 3u, t[i], 1);
            wire_put(&s, t[i], 2, pr[i], 0);
        }
        const u32 d = part_put(&s, "logic input", -200, -200, 30, 20, "0"), df = part_put(&s, "D flip-flop", 0, -200, 40, 40, "");
        const u32 dq = part_put(&s, "logic probe", 150, -200, 20, 20, "");
        wire_put(&s, d, 0, df, 0); wire_put(&s, ck, 0, df, 1); wire_put(&s, df, 2, dq, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        u32 right = 0, dq_ref = 0;
        for(u32 k = 1; k <= 40u; k++) {
            const u32 dv = (k * 7u + 3u) % 5u < 2u;
            set_in(&c, d, dv); CHECK(step(&c));
            set_in(&c, ck, 1); CHECK(step(&c));
            dq_ref = dv;
            set_in(&c, ck, 0); CHECK(step(&c));
            const u32 got = probe(&c, pr[0]) | probe(&c, pr[1]) << 1 | probe(&c, pr[2]) << 2 | probe(&c, pr[3]) << 3;
            right += got == (k & 15u) && probe(&c, dq) == dq_ref;
            if(k == 20u) fude_zoom_circuit_build(&c, &s);   // (made again mid-count: it counts on)
        }
        CHECK(right == 40u);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
    // Logic meeting the analog circuit: a switch's output through a resistor lights an LED (a bridge out, 5 V); a
    // battery through a push switch, a resistor pulling down, into a gate (a bridge in).
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 in = part_put(&s, "logic input", 0, 0, 30, 20, "0");
        const u32 r = part_put(&s, "resistor", 150, 0, 30, 6, "330"), led = part_put(&s, "LED", 300, 0, 20, 20, "red"), gnd = part_put(&s, "ground", 400, -100, 20, 20, "");
        wire_put(&s, in, 0, r, 0); wire_put(&s, r, 1, led, 0); wire_put(&s, led, 1, gnd, 0);
        const u32 bat = part_put(&s, "battery", 0, -300, 20, 30, "5V"), sw = part_put(&s, "SPST switch", 150, -250, 30, 20, "off");
        const u32 pd = part_put(&s, "resistor", 300, -350, 30, 6, "10k"), g = part_put(&s, "AND gate", 450, -250, 40, 30, "");
        const u32 en = part_put(&s, "logic input", 300, -150, 30, 20, "1"), pr = part_put(&s, "logic probe", 600, -250, 20, 20, "");
        wire_put(&s, bat, 0, sw, 0); wire_put(&s, sw, 1, g, 0); wire_put(&s, sw, 1, pd, 0); wire_put(&s, pd, 1, gnd, 0); wire_put(&s, bat, 1, gnd, 0);
        wire_put(&s, en, 0, g, 1); wire_put(&s, g, 2, pr, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        CHECK(step(&c) && cpart(&c, led)->shown < 0.05 && probe(&c, pr) == 0u);
        set_in(&c, in, 1);
        CHECK(step(&c) && cpart(&c, led)->shown > 0.5);
        const f64 i_led = cpart(&c, led)->pin_i[0];
        CHECK(i_led > 0.005 && i_led < 0.012);   // (about (5 − 2) / (330 + 25) A)
        cpart(&c, sw)->switch_on = 1u;
        CHECK(step(&c) && probe(&c, pr) == 1u);
        set_in(&c, en, 0);
        CHECK(step(&c) && probe(&c, pr) == 0u);
        cpart(&c, sw)->switch_on = 0u; set_in(&c, en, 1);
        CHECK(step(&c) && probe(&c, pr) == 0u);   // (pulled down)
        u32 first, n = fude_zoom_logic_bridges_of(c.logic, (u32)(cpart(&c, g) - (fude_zoom_circuit_part*)c.parts.memory), &first);
        CHECK(n == 1u && ((const fude_zoom_bridge*)c.logic->bridges.memory)[first].way == FUDE_ZOOM_BRIDGE_IN);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
    // A chip, unpowered, then powered: a 74HC04's inverter into an LED — dark without its supply, lit with it.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 ic = part_put(&s, "74HC04", 0, 0, 100, 80, "");
        CHECK(fude_zoom_part_of(&s, ic) != NULL && fude_zoom_part_of(&s, ic)->pin_count == 14u && pin_named(&s, ic, "VCC") == 13u && pin_named(&s, ic, "GND") == 6u);
        const u32 in = part_put(&s, "logic input", -300, 100, 30, 20, "0");
        const u32 r = part_put(&s, "resistor", 300, 100, 30, 6, "330"), led = part_put(&s, "LED", 450, 100, 20, 20, "green"), gnd = part_put(&s, "ground", 500, -200, 20, 20, "");
        wire_put(&s, in, 0, ic, pin_named(&s, ic, "1A")); wire_put(&s, ic, pin_named(&s, ic, "1Y"), r, 0); wire_put(&s, r, 1, led, 0); wire_put(&s, led, 1, gnd, 0);
        wire_put(&s, ic, pin_named(&s, ic, "GND"), gnd, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        CHECK(step(&c) && cpart(&c, led)->shown < 0.05);   // (no supply: its output not driven)
        const u32 bat = part_put(&s, "battery", -300, -200, 20, 30, "5V");
        wire_put(&s, bat, 0, ic, pin_named(&s, ic, "VCC")); wire_put(&s, bat, 1, gnd, 0);
        fude_zoom_circuit_build(&c, &s);
        CHECK(step(&c) && cpart(&c, led)->shown > 0.5);    // (input low: output high)
        set_in(&c, in, 1);
        CHECK(step(&c) && cpart(&c, led)->shown < 0.05);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
    // A 74HC161 seated on a breadboard, a battery on its top rails (jumpered to the bottom ones), a 100 Hz clock into
    // it, four LEDs on its outputs (each through a resistor to the minus rail): after n periods, n on its LEDs. Every
    // wire straight, along its own column on the board (a wire's end on another wire would join them).
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 bb = part_put(&s, "breadboard", 0, 0, 165, 92.5, "");
        const u32 col = 6u, pins = 16u, cols = FUDE_ZOOM_BREADBOARD_COLS;
        const u32 ic = seat_chip(&s, bb, "74HC161", col, pins);
        const fude_zoom_v2 top = hole_at(&s, bb, 0u, 2u), low = hole_at(&s, bb, 13u, 2u);
        const u32 bat = part_put(&s, "battery", top.x - 140.0, top.y + 120.0, 20, 30, "5V");
        wire_line(&s, bat, 0, bb, 0u * cols + 2u); wire_line(&s, bat, 1, bb, 1u * cols + 2u);
        wire_line(&s, bb, 0u * cols + 1u, bb, 12u * cols + 1u);    // (plus, top to bottom, down column 2)
        wire_line(&s, bb, 1u * cols + 28u, bb, 13u * cols + 28u);  // (minus, down column 29)
        // Each pin to its rail, along its column: row f's down to the bottom rails, row e's up to the top ones.
        // (~CLR 1, ENP 7, ~LOAD 9, ENT 10, VCC 16 to plus; A B C D 3–6 and GND 8 to minus)
        const u32 hi[5] = { 0u, 6u, 8u, 9u, 15u }, lo[5] = { 2u, 3u, 4u, 5u, 7u };
        for(u32 k = 0; k < 5u; k++) {
            const u32 ch = hi[k] < 8u ? col + hi[k] : col + 15u - hi[k], cl = col + lo[k];
            wire_line(&s, bb, strip_hole(col, pins, hi[k], 3u), bb, (hi[k] < 8u ? 12u : 0u) * cols + ch);
            wire_line(&s, bb, strip_hole(col, pins, lo[k], 3u), bb, 13u * cols + cl);
        }
        // The clock below the board under CLK's column; its minus to the bottom minus rail far along.
        const fude_zoom_v2 ck_hole = hole_at(&s, bb, 10u, col + 1u);
        const u32 clk = part_put(&s, "clock", ck_hole.x, low.y - 120.0, 20, 30, "100Hz");
        wire_line(&s, clk, 0, bb, 10u * cols + col + 1u);
        wire_line(&s, clk, 1, bb, 13u * cols + 22u);
        const c8* const q[4] = { "QA", "QB", "QC", "QD" };
        u32 led[4];
        for(u32 i = 0; i < 4u; i++) {
            const u32 k = pin_named(&s, ic, q[i]);
            CHECK(k == 13u - i);
            const fude_zoom_v2 h = hole_at(&s, bb, 3u, col + 15u - k);
            const u32 r = part_put(&s, "resistor", h.x + 7.0, top.y + 80.0 + (f64)i * 45.0, 30, 6, "330");
            led[i] = part_put(&s, "LED", h.x + 107.0, top.y + 80.0 + (f64)i * 45.0, 20, 20, "red");
            wire_line(&s, bb, 3u * cols + col + 15u - k, r, 0); wire_line(&s, r, 1, led[i], 0);
            wire_line(&s, led[i], 1, bb, 1u * cols + 20u + i);
        }
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        CHECK(c.logic->on && c.grounded == false);
        u32 counted_right = 0;
        for(u32 n = 1; n <= 12u; n++) {
            CHECK(fude_zoom_circuit_run(&c, 0.01, 1000u));   // (a period: one rising edge in each)
            u32 lit = 0;
            for(u32 i = 0; i < 4u; i++) lit |= (cpart(&c, led[i])->shown > 0.5 ? 1u : 0u) << i;
            counted_right += lit == (n & 15u) ? 1u : 0u;
        }
        CHECK(counted_right == 12u);
        // Its supply's draw and its LEDs': current from the battery.
        CHECK(cpart(&c, bat)->pin_i[0] != 0.0);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
    // Custom parts placed on the canvas by name: a 4-bit adder of full adders (every sum), and a 4 × 4 memory of
    // registers of flip-flops of latches of gates (random writes and reads).
    {
        for(u32 i = 0; i < PARTS_N; i++) {
            c8 err[160];
            CHECK(fude_zoom_logic_learn(PARTS_ALL[i], strlen(PARTS_ALL[i]), err, sizeof err) != NULL);
        }
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 add = part_put(&s, "custom part", 0, 0, 60, 100, "4-bit adder");
        const fude_zoom_part* ap = fude_zoom_part_of(&s, add);
        CHECK(fude_zoom_part_custom(ap) && ap->pin_count == 14u && pin_named(&s, add, "CI") == 8u && pin_named(&s, add, "CO") == 13u);
        CHECK(ap->pins[0].side == FUDE_ZOOM_PIN_LEFT && ap->pins[13].side == FUDE_ZOOM_PIN_RIGHT);
        const c8* const ins[9] = { "A0", "A1", "A2", "A3", "B0", "B1", "B2", "B3", "CI" };
        const c8* const outs[5] = { "S0", "S1", "S2", "S3", "CO" };
        u32 in[9], out[5];
        for(u32 i = 0; i < 9u; i++) { in[i] = part_put(&s, "logic input", -300, 200 - (f64)i * 50, 30, 20, "0"); wire_put(&s, in[i], 0, add, pin_named(&s, add, ins[i])); }
        for(u32 i = 0; i < 5u; i++) { out[i] = part_put(&s, "logic probe", 300, 200 - (f64)i * 50, 20, 20, ""); wire_put(&s, add, pin_named(&s, add, outs[i]), out[i], 0); }
        const u32 ram = part_put(&s, "custom part", 0, -600, 60, 100, "4 x 4 memory of registers");
        const c8* const rins[8] = { "A0", "A1", "D0", "D1", "D2", "D3", "WE", "CLK" };
        u32 rin[8], rout[4];
        for(u32 i = 0; i < 8u; i++) { rin[i] = part_put(&s, "logic input", -300, -400 - (f64)i * 50, 30, 20, "0"); wire_put(&s, rin[i], 0, ram, pin_named(&s, ram, rins[i])); }
        for(u32 i = 0; i < 4u; i++) { c8 n[4]; snprintf(n, sizeof n, "Q%u", i); rout[i] = part_put(&s, "logic probe", 300, -400 - (f64)i * 50, 20, 20, ""); wire_put(&s, ram, pin_named(&s, ram, n), rout[i], 0); }
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        CHECK(c.logic->on);
        u32 sums = 0;
        for(u32 v = 0; v < 512u; v++) {
            for(u32 i = 0; i < 9u; i++) set_in(&c, in[i], (v >> i) & 1u);
            CHECK(step(&c));
            const u32 want = (v & 15u) + ((v >> 4) & 15u) + (v >> 8);
            u32 got = 0;
            for(u32 i = 0; i < 5u; i++) got |= probe(&c, out[i]) << i;
            sums += got == want;
        }
        CHECK(sums == 512u);
        u32 mem[4] = { 0, 0, 0, 0 }, known = 0, reads = 0, reads_right = 0;
        for(u32 k = 0; k < 300u; k++) {
            const u32 a = (rnd() % 4u);
            set_in(&c, rin[0], a & 1u); set_in(&c, rin[1], a >> 1);
            if((rnd() % 3u) == 0u) {
                const u32 v = (rnd() % 16u);
                for(u32 i = 0; i < 4u; i++) set_in(&c, rin[2u + i], (v >> i) & 1u);
                set_in(&c, rin[6], 1); CHECK(step(&c));
                set_in(&c, rin[7], 1); CHECK(step(&c));
                set_in(&c, rin[7], 0); set_in(&c, rin[6], 0); CHECK(step(&c));
                mem[a] = v; known |= 1u << a;
            } else {
                CHECK(step(&c));
                if(known & (1u << a)) {
                    u32 got = 0;
                    for(u32 i = 0; i < 4u; i++) got |= probe(&c, rout[i]) << i;
                    reads++; reads_right += got == mem[a];
                }
            }
        }
        CHECK(reads > 50u && reads_right == reads);
        // A custom part the library does not know: drawn as a block, no pins, left out of the circuit (nothing breaks).
        const u32 lost = part_put(&s, "custom part", 600, 0, 60, 60, "no such part");
        CHECK(fude_zoom_part_of(&s, lost) != NULL && fude_zoom_part_of(&s, lost)->pin_count == 0u);
        fude_zoom_circuit_build(&c, &s);
        CHECK(step(&c));
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
    // Make part, as a person does it: a full adder drawn of gates (inputs and probes named A, B, CI, S, CO) made a
    // part; four of it placed and joined into a 4-bit adder, drawn; that made a part too (a part of parts, made on the
    // canvas); one of it placed: every sum right. Nothing to make one of, and nothing marked its inputs: said why.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 a = part_put(&s, "logic input", 0, 200, 30, 20, "A"), b = part_put(&s, "logic input", 0, 100, 30, 20, "B");
        const u32 ci = part_put(&s, "logic input", 0, 0, 30, 20, "CI");
        const u32 x1 = part_put(&s, "XOR gate", 200, 150, 40, 30, ""), x2 = part_put(&s, "XOR gate", 400, 100, 40, 30, "");
        const u32 a1 = part_put(&s, "AND gate", 200, -50, 40, 30, ""), a2 = part_put(&s, "AND gate", 400, -50, 40, 30, "");
        const u32 o1 = part_put(&s, "OR gate", 600, -50, 40, 30, "");
        const u32 sp = part_put(&s, "logic probe", 700, 150, 20, 20, "S"), cp = part_put(&s, "logic probe", 800, -50, 20, 20, "CO");
        wire_put(&s, a, 0, x1, 0); wire_put(&s, b, 0, x1, 1); wire_put(&s, x1, 2, x2, 0); wire_put(&s, ci, 0, x2, 1); wire_put(&s, x2, 2, sp, 0);
        wire_put(&s, a, 0, a1, 0); wire_put(&s, b, 0, a1, 1); wire_put(&s, x1, 2, a2, 0); wire_put(&s, ci, 0, a2, 1);
        wire_put(&s, a1, 2, o1, 0); wire_put(&s, a2, 2, o1, 1); wire_put(&s, o1, 2, cp, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        u32 problem = 9;
        fude_sim_def* fa = fude_zoom_logic_make(&c, &s, "user/drawn-fa", "Drawn full adder", &problem);
        CHECK(fa != NULL && problem == FUDE_ZOOM_MAKE_OK);
        if(fa != NULL) {
            const fude_sim_port* fp = (const fude_sim_port*)fa->ports.memory;
            CHECK(rde_arr_length(&fa->ports) == 5u && strcmp(fp[0].name, "A") == 0 && strcmp(fp[1].name, "B") == 0 && strcmp(fp[2].name, "CI") == 0 &&
                  strcmp(fp[3].name, "S") == 0 && strcmp(fp[4].name, "CO") == 0 && fp[0].dir == FUDE_SIM_IN && fp[4].dir == FUDE_SIM_OUT);
            CHECK(rde_arr_length(&fa->insts) == 5u);
            // (saved as text, learnt back: what Make part keeps in My parts)
            rde_arr text = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
            fude_sim_def_write(fa, &text);
            c8 err[160];
            CHECK(fude_zoom_logic_learn((const c8*)text.memory, rde_arr_length(&text), err, sizeof err) != NULL);
            rde_arr_free(&text);
            fude_sim_def_free(fa);
        }
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
        // The 4-bit adder of four of them, drawn.
        fude_zoom_scene s2; fude_zoom_scene_init(&s2, 7);
        u32 fas[4], ain[4], bin[4], sout[4];
        const u32 cin = part_put(&s2, "logic input", -300, 900, 30, 20, "CI");
        for(u32 i = 0; i < 4u; i++) {
            c8 an[4], bn[4], sn[4];
            snprintf(an, sizeof an, "A%u", i); snprintf(bn, sizeof bn, "B%u", i); snprintf(sn, sizeof sn, "S%u", i);
            fas[i] = part_put(&s2, "custom part", 0, 700 - (f64)i * 250, 60, 80, "Drawn full adder");
            ain[i] = part_put(&s2, "logic input", -300, 760 - (f64)i * 250 - 10, 30, 20, an);
            bin[i] = part_put(&s2, "logic input", -300, 720 - (f64)i * 250 - 10, 30, 20, bn);
            sout[i] = part_put(&s2, "logic probe", 300, 700 - (f64)i * 250, 20, 20, sn);
            wire_put(&s2, ain[i], 0, fas[i], pin_named(&s2, fas[i], "A")); wire_put(&s2, bin[i], 0, fas[i], pin_named(&s2, fas[i], "B"));
            wire_put(&s2, fas[i], pin_named(&s2, fas[i], "S"), sout[i], 0);
            if(i > 0u) wire_put(&s2, fas[i - 1u], pin_named(&s2, fas[i - 1u], "CO"), fas[i], pin_named(&s2, fas[i], "CI"));
        }
        wire_put(&s2, cin, 0, fas[0], pin_named(&s2, fas[0], "CI"));
        const u32 cout = part_put(&s2, "logic probe", 300, -300, 20, 20, "CO");
        wire_put(&s2, fas[3], pin_named(&s2, fas[3], "CO"), cout, 0);
        fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s2);
        fude_sim_def* add = fude_zoom_logic_make(&c, &s2, "user/drawn-add4", "Drawn 4-bit adder", &problem);
        CHECK(add != NULL && rde_arr_length(&add->ports) == 14u && rde_arr_length(&add->insts) == 4u);
        if(add != NULL) {
            rde_arr text = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
            fude_sim_def_write(add, &text);
            c8 err[160];
            CHECK(fude_zoom_logic_learn((const c8*)text.memory, rde_arr_length(&text), err, sizeof err) != NULL);
            rde_arr_free(&text);
            fude_sim_def_free(add);
        }
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s2);
        // One of it placed, its pins as made (inputs top to bottom on the left as they were drawn: CI, A0, B0, A1…).
        fude_zoom_scene s3; fude_zoom_scene_init(&s3, 7);
        const u32 ad = part_put(&s3, "custom part", 0, 0, 60, 160, "Drawn 4-bit adder");
        CHECK(fude_zoom_part_of(&s3, ad)->pin_count == 14u && pin_named(&s3, ad, "CI") == 0u && pin_named(&s3, ad, "A0") == 1u);
        const c8* const ins[9] = { "A0", "A1", "A2", "A3", "B0", "B1", "B2", "B3", "CI" };
        const c8* const outs[5] = { "S0", "S1", "S2", "S3", "CO" };
        u32 in[9], out[5];
        for(u32 i = 0; i < 9u; i++) { in[i] = part_put(&s3, "logic input", -300, 300 - (f64)i * 60, 30, 20, "0"); wire_put(&s3, in[i], 0, ad, pin_named(&s3, ad, ins[i])); }
        for(u32 i = 0; i < 5u; i++) { out[i] = part_put(&s3, "logic probe", 300, 300 - (f64)i * 60, 20, 20, ""); wire_put(&s3, ad, pin_named(&s3, ad, outs[i]), out[i], 0); }
        fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s3);
        u32 sums = 0;
        for(u32 v = 0; v < 512u; v++) {
            for(u32 i = 0; i < 9u; i++) set_in(&c, in[i], (v >> i) & 1u);
            CHECK(step(&c));
            u32 got = 0;
            for(u32 i = 0; i < 5u; i++) got |= probe(&c, out[i]) << i;
            sums += got == (v & 15u) + ((v >> 4) & 15u) + (v >> 8) ? 1u : 0u;
        }
        CHECK(sums == 512u);
        // Made again with the same id (Edit part, done again): a newer version, the instance following it.
        const fude_sim_def* was = fude_zoom_logic_find("Drawn 4-bit adder");
        const u32 version = was != NULL ? was->version : 0u;
        const c8* again = "fude-part 1\nid user/drawn-add4\nname Drawn 4-bit adder\nport X logic in\nport Y logic out 1 right\ninst not N \"\" X Y\nend\n";
        c8 err2[160];
        CHECK(fude_zoom_logic_learn(again, strlen(again), err2, sizeof err2) != NULL);
        CHECK(fude_zoom_logic_find("Drawn 4-bit adder")->version == version + 1u && fude_zoom_part_of(&s3, ad)->pin_count == 2u);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s3);
        // Nothing to make one of; and no inputs or outputs marked.
        fude_zoom_scene s4; fude_zoom_scene_init(&s4, 7);
        part_put(&s4, "resistor", 0, 0, 30, 6, "1k");
        fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s4);
        CHECK(fude_zoom_logic_make(&c, &s4, "user/x", "X", &problem) == NULL && problem == FUDE_ZOOM_MAKE_NOTHING);
        part_put(&s4, "NOT gate", 200, 0, 40, 30, "");
        fude_zoom_circuit_build(&c, &s4);
        CHECK(fude_zoom_logic_make(&c, &s4, "user/x", "X", &problem) == NULL && problem == FUDE_ZOOM_MAKE_NO_PORTS);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s4);
    }
    // Borja's tablet circuit: A through a NOT into an AND; A and B into an OR (A's wire branching off its wire to the
    // NOT, a junction); the OR into the AND (branching off the OR's wire on) and into a last OR with the AND: A or B.
    // Every way its switches can be, through Play's own path (built, reset, built again) and through taps.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 a = part_put(&s, "logic input", 0, 100, 30, 20, "0"), b = part_put(&s, "logic input", 0, -60, 30, 20, "0");
        const u32 nt = part_put(&s, "NOT gate", 180, 100, 40, 30, ""), og = part_put(&s, "OR gate", 180, -40, 40, 30, "");
        const u32 ag = part_put(&s, "AND gate", 420, 80, 40, 30, ""), fo = part_put(&s, "OR gate", 640, 40, 40, 30, "");
        const u32 pr = part_put(&s, "logic probe", 800, 40, 20, 20, "");
        const u32 wa = wire_put(&s, a, 0, nt, 0);            // (A to the NOT)
        wire_put(&s, b, 0, og, 1);
        wire_put(&s, nt, 1, ag, 0);
        const u32 wo = wire_put(&s, og, 2, fo, 1);           // (the OR's output on to the last OR)
        wire_put(&s, ag, 2, fo, 0);
        wire_put(&s, fo, 2, pr, 0);
        // (from the OR's first input up to a point along A's wire; from the AND's second input down
        // to the OR's wire on)
        fude_zoom_v2 pa[FUDE_ZOOM_WIRE_POINTS], po[FUDE_ZOOM_WIRE_POINTS], q0 = { 0.0, 0.0 }, q1 = { 0.0, 0.0 };
        rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
        f64 nn[1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u];
        u32 cnt = fude_zoom_scene_shape_numbers(&s, wa, nn, 1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u);
        fude_zoom_id f0, t0; i32 fp0, tp0;
        const u32 ka = fude_zoom_wire_of(nn, cnt, pa, &f0, &fp0, &t0, &tp0);
        cnt = fude_zoom_scene_shape_numbers(&s, wo, nn, 1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u);
        const u32 ko = fude_zoom_wire_of(nn, cnt, po, &f0, &fp0, &t0, &tp0);
        CHECK(ka >= 2u && ko >= 2u);
        // (a point along each: the middle of its longest piece, in the root frame: wires here are at the root)
        fude_zoom_v2 mid_a = pa[0], mid_o = po[0];
        f64 best = -1.0;
        for(u32 k = 0; k + 1u < ka; k++) { const f64 l = hypot(pa[k + 1].x - pa[k].x, pa[k + 1].y - pa[k].y); if(l > best) { best = l; mid_a = (fude_zoom_v2){ (pa[k].x + pa[k + 1].x) * 0.5, (pa[k].y + pa[k + 1].y) * 0.5 }; } }
        best = -1.0;
        for(u32 k = 0; k + 1u < ko; k++) { const f64 l = hypot(po[k + 1].x - po[k].x, po[k + 1].y - po[k].y); if(l > best) { best = l; mid_o = (fude_zoom_v2){ (po[k].x + po[k + 1].x) * 0.5, (po[k].y + po[k + 1].y) * 0.5 }; } }
        const fude_zoom_object* wao = fude_zoom_scene_object(&s, wa);
        mid_a = fude_zoom_sim_apply(fude_zoom_object_sim(wao), mid_a);
        mid_o = fude_zoom_sim_apply(fude_zoom_object_sim(fude_zoom_scene_object(&s, wo)), mid_o);
        CHECK(fude_zoom_part_pin_at(&s, og, 0, &q0) && fude_zoom_part_pin_at(&s, ag, 1, &q1));
        const fude_zoom_v2 j1[2] = { q0, mid_a }, j2[2] = { q1, mid_o };
        fude_zoom_wire_add(&s, s.root, j1, 2u, og, 0, FUDE_ZOOM_NONE, -1, (rde_color){ 1, 1, 1, 255 }, 0.1f);
        fude_zoom_wire_add(&s, s.root, j2, 2u, ag, 1, FUDE_ZOOM_NONE, -1, (rde_color){ 1, 1, 1, 255 }, 0.1f);
        rde_arr_free(&pts);
        RDE_UNUSED(po); RDE_UNUSED(pa);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        // (as Play makes it: built, reset, built again)
        fude_zoom_circuit_build(&c, &s);
        fude_zoom_circuit_reset(&c);
        fude_zoom_circuit_build(&c, &s);
        u32 right = 0;
        for(u32 round = 0; round < 3u; round++) {
            for(u32 v = 0; v < 4u; v++) {
                const u32 av = (v >> 1) & 1u, bv = v & 1u;
                set_in(&c, a, av); set_in(&c, b, bv);
                CHECK(step(&c));
                const u32 got = probe(&c, pr);
                if(got != (av | bv)) printf("  A %u B %u: probe %u\n", av, bv, got);
                right += got == (av | bv);
            }
        }
        CHECK(right == 12u);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
    // ...as Borja drew it on the tablet: the branch to the OR started not on A's wire but on the NOT gate's input lead
    // (the gate's own line between its pin and its body, right of where A's wire ends): on the pin all the same. And a
    // wire left dangling: its end loose (Play shows it so), the others not.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 a = part_put(&s, "logic input", 0, 100, 30, 20, "0"), b = part_put(&s, "logic input", 0, -60, 30, 20, "0");
        const u32 nt = part_put(&s, "NOT gate", 180, 100, 40, 30, ""), og = part_put(&s, "OR gate", 180, -40, 40, 30, "");
        const u32 pr = part_put(&s, "logic probe", 400, -40, 20, 20, "");
        wire_put(&s, a, 0, nt, 0);
        wire_put(&s, b, 0, og, 1);
        wire_put(&s, og, 2, pr, 0);
        fude_zoom_v2 pin = { 0.0, 0.0 }, q0 = { 0.0, 0.0 };
        CHECK(fude_zoom_part_pin_at(&s, nt, 0, &pin) && fude_zoom_part_pin_at(&s, og, 0, &q0));
        const fude_zoom_v2 on_lead = { pin.x + 0.25 * 40.0, pin.y };   // (a quarter of its half width in: on its lead)
        const fude_zoom_v2 jl[3] = { q0, { on_lead.x, q0.y }, on_lead };
        const u32 branch = fude_zoom_wire_add(&s, s.root, jl, 3u, og, 0, FUDE_ZOOM_NONE, -1, (rde_color){ 1, 1, 1, 255 }, 0.1f);
        const fude_zoom_v2 jd[2] = { { 600, 300 }, { 700, 300 } };
        const u32 dangling = fude_zoom_wire_add(&s, s.root, jd, 2u, FUDE_ZOOM_NONE, -1, FUDE_ZOOM_NONE, -1, (rde_color){ 1, 1, 1, 255 }, 0.1f);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        u32 right = 0;
        for(u32 v = 0; v < 4u; v++) {
            set_in(&c, a, v >> 1); set_in(&c, b, v & 1u);
            CHECK(step(&c));
            right += probe(&c, pr) == ((v >> 1) | (v & 1u)) ? 1u : 0u;
        }
        CHECK(right == 4u);
        const fude_zoom_circuit_wire* cw = (const fude_zoom_circuit_wire*)c.wires.memory;
        u32 loose = 0, checked = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&c.wires); i++) {
            if(cw[i].object == dangling) { CHECK(cw[i].loose[0] && cw[i].loose[1]); checked++; }
            else if(cw[i].object == branch) { CHECK(!cw[i].loose[0] && !cw[i].loose[1]); checked++; }
            else loose += cw[i].loose[0] + cw[i].loose[1];
        }
        CHECK(checked == 2u && loose == 0u);
        // A wire's end inside a part's body, past its lead: not on it (only what looks joined is).
        const fude_zoom_v2 inside[2] = { { 180, 300 }, { 180.0 - 0.1 * 40.0, 100 } };   // (near the NOT's middle)
        const u32 wi = fude_zoom_wire_add(&s, s.root, inside, 2u, FUDE_ZOOM_NONE, -1, FUDE_ZOOM_NONE, -1, (rde_color){ 1, 1, 1, 255 }, 0.1f);
        fude_zoom_circuit_build(&c, &s);
        cw = (const fude_zoom_circuit_wire*)c.wires.memory;
        for(u32 i = 0; i < (u32)rde_arr_length(&c.wires); i++) if(cw[i].object == wi) CHECK(cw[i].loose[1]);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
    // Level and plumb (parts' pins lined up as they are moved): against a brute-force model of it, many random moves:
    // each way the nearest target within reach pulled to, independently; nothing within reach, the move as it was;
    // pulled, a mover exactly on the line said.
    {
        u32 cases = 0, right = 0;
        for(u32 k = 0; k < 3000u; k++) {
            fude_zoom_v2 mv[12], tg[40];
            const u32 nm = 1u + rnd() % 12u, ntg = rnd() % 40u;
            for(u32 i = 0; i < nm; i++) mv[i] = (fude_zoom_v2){ (rndf() - 0.5) * 800.0, (rndf() - 0.5) * 800.0 };
            for(u32 i = 0; i < ntg; i++) tg[i] = (fude_zoom_v2){ (rndf() - 0.5) * 800.0, (rndf() - 0.5) * 800.0 };
            const f64 dx = (rndf() - 0.5) * 200.0, dy = (rndf() - 0.5) * 200.0, reach = 2.0 + rndf() * 12.0;
            f64 ox, oy, lx = NAN, ly = NAN;
            fude_zoom_snap_align(mv, nm, tg, ntg, dx, dy, reach, reach, &ox, &oy, &lx, &ly);
            // (the model: the smallest |gap| each way under reach)
            f64 bx = reach, by = reach, ex = 0.0, ey = 0.0;
            b8 hx = false, hy = false;
            for(u32 i = 0; i < nm; i++) for(u32 j = 0; j < ntg; j++) {
                const f64 gx = tg[j].x - (mv[i].x + dx), gy = tg[j].y - (mv[i].y + dy);
                if(fabs(gx) < bx) { bx = fabs(gx); ex = gx; hx = true; }
                if(fabs(gy) < by) { by = fabs(gy); ey = gy; hy = true; }
            }
            b8 ok = fabs(ox - (dx + ex)) < 1e-9 && fabs(oy - (dy + ey)) < 1e-9 && (hx ? !isnan(lx) : isnan(lx)) && (hy ? !isnan(ly) : isnan(ly));
            // (pulled: some mover now exactly on each line)
            if(ok && hx) { b8 on = false; for(u32 i = 0; i < nm; i++) on = on || fabs(mv[i].x + ox - lx) < 1e-9; ok = on; }
            if(ok && hy) { b8 on = false; for(u32 i = 0; i < nm; i++) on = on || fabs(mv[i].y + oy - ly) < 1e-9; ok = on; }
            // (and never further than reach from where the pen put it)
            ok = ok && fabs(ox - dx) < reach && fabs(oy - dy) < reach;
            cases++; right += ok;
        }
        CHECK(right == cases);
        // Two parts drawn: one's pin a few units off level with the other's: aligned, their pins exactly level.
        const fude_zoom_v2 a_pins[2] = { { 100, 52.5 }, { 160, 52.5 } }, b_pins[1] = { { 300, 50 } };
        f64 ox, oy, lx = NAN, ly = NAN;
        fude_zoom_snap_align(a_pins, 2u, b_pins, 1u, 20.0, 0.0, 9.0, 13.0, &ox, &oy, &lx, &ly);
        CHECK(fabs(oy + 2.5) < 1e-12 && ly == 50.0 && isnan(lx) && ox == 20.0);
        // Held: dragged on past reach but within keep, the same line; past keep, let go; a nearer target taken over it.
        fude_zoom_snap_align(a_pins, 2u, b_pins, 1u, 20.0, -11.0, 9.0, 13.0, &ox, &oy, &lx, &ly);   // (13.5 − … : 11 off now)
        CHECK(ly == 50.0 && fabs(oy + 2.5) < 1e-12);
        fude_zoom_snap_align(a_pins, 2u, b_pins, 1u, 20.0, -20.0, 9.0, 13.0, &ox, &oy, &lx, &ly);   // (17.5 off: let go)
        CHECK(isnan(ly) && oy == -20.0);
        const fude_zoom_v2 two[2] = { { 300, 50 }, { 400, 45 } };
        ly = 50.0;
        fude_zoom_snap_align(a_pins, 2u, two, 2u, 20.0, -7.0, 9.0, 13.0, &ox, &oy, &lx, &ly);   // (45 is 0.5 off, 50 is 4.5)
        CHECK(ly == 45.0 && fabs(oy + 7.5) < 1e-12);
        // A nearer target found by the first mover is not given up for the held line a later mover is within keep of
        // (the second pin 8 off the line held, the first 1 off a target: the target).
        const fude_zoom_v2 two_pins[2] = { { 0, 0 }, { 100, 20 } }, one_target[1] = { { 500, 1 } };
        ly = 28.0; lx = NAN;
        fude_zoom_snap_align(two_pins, 2u, one_target, 1u, 0.0, 0.0, 9.0, 13.0, &ox, &oy, &lx, &ly);
        CHECK(ly == 1.0 && fabs(oy - 1.0) < 1e-12 && isnan(lx) && ox == 0.0);
        // ...and with nothing nearer, the held line is kept from whichever mover is within keep of it, past reach.
        ly = 28.0;
        fude_zoom_snap_align(two_pins, 2u, one_target, 0u, 0.0, 0.0, 9.0, 13.0, &ox, &oy, &lx, &ly);
        CHECK(ly == 28.0 && fabs(oy - 8.0) < 1e-12);
        // The grid: to the nearest line from its origin; none (a step of 0): as it was.
        CHECK(fude_zoom_snap_grid(23.0, 3.0, 10.0) == 23.0 && fude_zoom_snap_grid(27.9, 3.0, 10.0) == 23.0 && fude_zoom_snap_grid(28.1, 3.0, 10.0) == 33.0);
        CHECK(fude_zoom_snap_grid(-7.2, 3.0, 10.0) == -7.0 && fude_zoom_snap_grid(5.5, 0.0, 0.0) == 5.5);
        u32 grid_right = 0;
        for(u32 k = 0; k < 2000u; k++) {
            const f64 v = (rndf() - 0.5) * 1e4, o = (rndf() - 0.5) * 100.0, st = 0.5 + rndf() * 40.0, g = fude_zoom_snap_grid(v, o, st);
            const f64 q = (g - o) / st;
            grid_right += fabs(q - round(q)) < 1e-6 && fabs(g - v) <= st * 0.5 + 1e-9 ? 1u : 0u;
        }
        CHECK(grid_right == 2000u);
    }
    // A wire joins two things or is not (Borja): what deleting takes with it — the wires at a deleted part's pins; the
    // branches ending on a deleted wire and on nothing else (and theirs in turn); not a branch that ends on another
    // wire too, nor anything elsewhere — in the same undo step; the eraser taking a wire whole, its branches with it.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 bat = part_put(&s, "battery", 0, 0, 20, 30, "5V");
        const u32 r1 = part_put(&s, "resistor", 200, 100, 30, 6, "1k"), r2 = part_put(&s, "resistor", 200, -100, 30, 6, "1k");
        const u32 led = part_put(&s, "LED", 400, 0, 20, 20, "red");
        const u32 w1 = wire_put(&s, bat, 0, r1, 0), w2 = wire_put(&s, r1, 1, led, 0), w3 = wire_put(&s, led, 1, r2, 1), w4 = wire_put(&s, r2, 0, bat, 1);
        // (a branch off w2 ending on it — a junction — and a branch off that branch; a branch off w3
        // that also ends on w4)
        fude_zoom_v2 p2a, p2b;
        CHECK(fude_zoom_part_pin_at(&s, r1, 1, &p2a) && fude_zoom_part_pin_at(&s, led, 0, &p2b));
        f64 nn[1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u];
        fude_zoom_v2 pts[FUDE_ZOOM_WIRE_POINTS];
        u32 cnt = fude_zoom_scene_shape_numbers(&s, w2, nn, 1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u);
        const u32 k2 = fude_zoom_wire_of(nn, cnt, pts, NULL, NULL, NULL, NULL);
        const fude_zoom_sim w2s = fude_zoom_object_sim(fude_zoom_scene_object(&s, w2));
        fude_zoom_v2 on2 = fude_zoom_sim_apply(w2s, (fude_zoom_v2){ (pts[0].x + pts[1].x) * 0.5, (pts[0].y + pts[1].y) * 0.5 });
        RDE_UNUSED(k2);
        const fude_zoom_v2 b1[2] = { on2, { on2.x, on2.y + 150.0 } };
        const u32 br1 = fude_zoom_wire_add(&s, s.root, b1, 2u, FUDE_ZOOM_NONE, -1, FUDE_ZOOM_NONE, -1, (rde_color){ 1, 1, 1, 255 }, 0.1f);
        const fude_zoom_v2 b2[2] = { { on2.x, on2.y + 75.0 }, { on2.x + 120.0, on2.y + 75.0 } };   // (its end on br1)
        const u32 br2 = fude_zoom_wire_add(&s, s.root, b2, 2u, FUDE_ZOOM_NONE, -1, FUDE_ZOOM_NONE, -1, (rde_color){ 1, 1, 1, 255 }, 0.1f);
        RDE_UNUSED(p2a); RDE_UNUSED(p2b); RDE_UNUSED(w3); RDE_UNUSED(w4);
        const u32 other = part_put(&s, "capacitor", -400, 300, 20, 10, "1uF");
        fude_zoom_selection sel; fude_zoom_select_init(&sel);
        // Deleting the first resistor: w1 and w2 (its pins'), br1 (on w2 only), br2 (on br1 only) — the rest stay.
        rde_arr_add(&sel.picks, &(fude_zoom_pick){ r1 });
        const u32 before = s.history_pushes;
        fude_zoom_select_delete(&sel, &s);
        CHECK(s.history_pushes == before + 1u);
        const u32 gone[5] = { r1, w1, w2, br1, br2 }, kept[6] = { bat, r2, led, w3, w4, other };
        for(u32 i = 0; i < 5u; i++) CHECK(!(fude_zoom_scene_object(&s, gone[i])->flags & FUDE_ZOOM_FLAG_ALIVE));
        for(u32 i = 0; i < 6u; i++) CHECK(fude_zoom_scene_object(&s, kept[i])->flags & FUDE_ZOOM_FLAG_ALIVE);
        // Undone: all of it back, at once.
        CHECK(fude_zoom_history_undo(&s));
        for(u32 i = 0; i < 5u; i++) CHECK(fude_zoom_scene_object(&s, gone[i])->flags & FUDE_ZOOM_FLAG_ALIVE);
        // Deleting something unrelated takes no wire.
        rde_arr_add(&sel.picks, &(fude_zoom_pick){ other });
        fude_zoom_select_delete(&sel, &s);
        u32 wires_alive = 0;
        for(u32 i = 0; i < fude_zoom_scene_object_count(&s); i++) {
            const fude_zoom_object* o = fude_zoom_scene_object(&s, i);
            wires_alive += (o->flags & FUDE_ZOOM_FLAG_ALIVE) && o->kind == FUDE_ZOOM_KIND_SHAPE && o->channels == FUDE_ZOOM_SHAPE_WIRE;
        }
        CHECK(wires_alive == 6u);
        // A branch ending on two wires: one of them deleted, it stays (still joined).
        const fude_zoom_v2 b3[2] = { { on2.x, on2.y + 150.0 }, { on2.x, on2.y + 75.0 } };   // (from br1's far end back onto br1 and br2's start)
        const u32 br3 = fude_zoom_wire_add(&s, s.root, b3, 2u, FUDE_ZOOM_NONE, -1, FUDE_ZOOM_NONE, -1, (rde_color){ 1, 1, 1, 255 }, 0.1f);
        rde_arr died = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_scene_set_alive(&s, br2, false);
        rde_arr_add(&died, (any)&br2);
        fude_zoom_wire_orphans(&s, &died);
        CHECK(fude_zoom_scene_object(&s, br3)->flags & FUDE_ZOOM_FLAG_ALIVE);   // (its end still on br1)
        rde_arr_free(&died);
        // The eraser across w3: taken whole (not cut into a plain line), nothing of it left as a stroke.
        const u32 count_before = fude_zoom_scene_object_count(&s);
        fude_zoom_eraser e; fude_zoom_eraser_init(&e);
        fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_PARTIAL);
        fude_zoom_v2 l0, l1;
        CHECK(fude_zoom_part_pin_at(&s, led, 1, &l0) && fude_zoom_part_pin_at(&s, r2, 1, &l1));
        cnt = fude_zoom_scene_shape_numbers(&s, w3, nn, 1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u);
        const u32 k3 = fude_zoom_wire_of(nn, cnt, pts, NULL, NULL, NULL, NULL);
        const fude_zoom_sim w3s = fude_zoom_object_sim(fude_zoom_scene_object(&s, w3));
        const fude_zoom_v2 mid3 = fude_zoom_sim_apply(w3s, (fude_zoom_v2){ (pts[k3 / 2u - 1u].x + pts[k3 / 2u].x) * 0.5, (pts[k3 / 2u - 1u].y + pts[k3 / 2u].y) * 0.5 });
        fude_zoom_erase_step(&s, &e, s.root, (fude_zoom_v2){ mid3.x - 3.0, mid3.y - 3.0 }, (fude_zoom_v2){ mid3.x + 3.0, mid3.y + 3.0 }, 2.0, 0.5);
        fude_zoom_erase_end(&s, &e, s.root, (fude_zoom_box){ -1000, -1000, 1000, 1000 });
        CHECK(!(fude_zoom_scene_object(&s, w3)->flags & FUDE_ZOOM_FLAG_ALIVE));
        u32 born_lines = 0;
        for(u32 i = count_before; i < fude_zoom_scene_object_count(&s); i++) born_lines += (fude_zoom_scene_object(&s, i)->flags & FUDE_ZOOM_FLAG_ALIVE) ? 1u : 0u;
        CHECK(born_lines == 0u);
        fude_zoom_eraser_destroy(&e);
        fude_zoom_select_destroy(&sel);
        fude_zoom_scene_destroy(&s);
    }
    // ...at random: chains of parts wired in a row, branches on branches; any of them deleted: afterwards no wire alive
    // keeps a dead part's pin or ends only on dead wires; and every wire alive before whose ends are all still held is
    // alive.
    {
        u32 rounds = 0, right = 0;
        for(u32 k = 0; k < 60u; k++) {
            fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
            const u32 np = 3u + rnd() % 6u;
            u32 parts[8];
            for(u32 i = 0; i < np; i++) parts[i] = part_put(&s, "resistor", (f64)i * 150.0, (f64)(rnd() % 5u) * 40.0, 30, 6, "1k");
            u32 wires[32], nw = 0;
            for(u32 i = 0; i + 1u < np; i++) wires[nw++] = wire_put(&s, parts[i], 1, parts[i + 1u], 0);
            // (branches: from a point along a random wire, straight down; some from a point along a branch)
            for(u32 b = 0; b < 6u && nw < 32u; b++) {
                const u32 host = wires[rnd() % nw];
                f64 nn[1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u];
                fude_zoom_v2 pts[FUDE_ZOOM_WIRE_POINTS];
                const u32 cnt = fude_zoom_scene_shape_numbers(&s, host, nn, 1u + 2u * FUDE_ZOOM_WIRE_POINTS + 4u);
                const u32 kk = fude_zoom_wire_of(nn, cnt, pts, NULL, NULL, NULL, NULL);
                if(kk < 2u) continue;
                const u32 seg = rnd() % (kk - 1u);
                const fude_zoom_sim hs = fude_zoom_object_sim(fude_zoom_scene_object(&s, host));
                const fude_zoom_v2 at = fude_zoom_sim_apply(hs, (fude_zoom_v2){ (pts[seg].x + pts[seg + 1u].x) * 0.5, (pts[seg].y + pts[seg + 1u].y) * 0.5 });
                const fude_zoom_v2 bp[2] = { at, { at.x + 7.0 * (f64)(b + 1u), at.y - 300.0 - 17.0 * (f64)b } };
                wires[nw++] = fude_zoom_wire_add(&s, s.root, bp, 2u, FUDE_ZOOM_NONE, -1, FUDE_ZOOM_NONE, -1, (rde_color){ 1, 1, 1, 255 }, 0.1f);
            }
            // Delete one or two things at random.
            fude_zoom_selection sel; fude_zoom_select_init(&sel);
            const u32 victims = 1u + rnd() % 2u;
            for(u32 v = 0; v < victims; v++) {
                const u32 pick = rnd() % (np + nw);
                rde_arr_add(&sel.picks, &(fude_zoom_pick){ pick < np ? parts[pick] : wires[pick - np] });
            }
            fude_zoom_select_delete(&sel, &s);
            // The circuit built over what is left: no wire with a loose end that was not loose before deleting.
            fude_zoom_circuit c; fude_zoom_circuit_init(&c);
            fude_zoom_circuit_build(&c, &s);
            const fude_zoom_circuit_wire* cw = (const fude_zoom_circuit_wire*)c.wires.memory;
            b8 ok = true;
            for(u32 i = 0; i < (u32)rde_arr_length(&c.wires); i++) {
                // (branches' far ends are loose by making: only their joined end matters — its first)
                b8 is_branch = false;
                for(u32 j = np - 1u; j < nw; j++) is_branch = is_branch || wires[j] == cw[i].object;
                ok = ok && !cw[i].loose[0] && (is_branch || !cw[i].loose[1]);
            }
            rounds++; right += ok;
            fude_zoom_circuit_destroy(&c);
            fude_zoom_select_destroy(&sel);
            fude_zoom_scene_destroy(&s);
        }
        CHECK(right == rounds);
    }
    // A ring of three NOT gates (no rest: it oscillates as fast as its gates): the circuit still steps, nothing hangs.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        u32 n[3];
        for(u32 i = 0; i < 3u; i++) n[i] = part_put(&s, "NOT gate", (f64)i * 150.0, 0, 40, 30, "");
        for(u32 i = 0; i < 3u; i++) wire_put(&s, n[i], 1, n[(i + 1u) % 3u], 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        CHECK(fude_zoom_circuit_run(&c, 0.01, 20u));
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
}

// Play's tags are as big as the parts look: half the typical part's smaller half-size on the screen, at most a tag
// beside the pen's, none below a pin name's least.
static void test_play_tags(void) {
    const f64 most = 13.0, least = 6.0;
    // A circuit drawn: three logic inputs, five gates, a probe and a breadboard (its size not the typical one's).
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        u32 parts[10], np = 0;
        for(u32 i = 0; i < 3u; i++) parts[np++] = part_put(&s, "logic input", -200.0, (f64)i * 80.0, 20, 15, "0");
        for(u32 i = 0; i < 5u; i++) parts[np++] = part_put(&s, "AND gate", (f64)i * 90.0, 0.0, 30, 30, "");
        parts[np++] = part_put(&s, "logic probe", 500.0, 0.0, 15, 15, "");
        parts[np++] = part_put(&s, "breadboard", 0.0, -600.0, 400, 150, "");
        f64 got[6];
        const f64 zooms[6] = { 4.0, 1.0, 0.5, 0.4, 0.3, 0.1 };
        for(u32 z = 0; z < 6u; z++) {
            f64 sizes[10];
            for(u32 i = 0; i < np; i++) {
                f64 n[3] = { 0.0, 1.0, 1.0 };
                fude_zoom_scene_shape_numbers(&s, parts[i], n, 3u);
                sizes[i] = fmin(fabs(n[1]), fabs(n[2])) * fude_zoom_sim_scale(fude_zoom_object_sim(fude_zoom_scene_object(&s, parts[i]))) * zooms[z];
            }
            got[z] = fude_zoom_circuit_tag_px(sizes, np, most, least);
        }
        // (sorted: 15 15 15 15 30 30 30 30 30 150 — the typical part a gate, 30: letters 15 at 1, 7.5 at a half)
        CHECK(got[0] == most && got[1] == most);
        CHECK(fabs(got[2] - 7.5) < 1e-9);
        CHECK(got[3] == 6.0);
        CHECK(got[4] == 0.0 && got[5] == 0.0);
        fude_zoom_scene_destroy(&s);
    }
    // One huge part among small ones: the small ones' size.
    {
        f64 sizes[10] = { 10, 10, 10, 10, 10, 10, 10, 10, 10, 4000 };
        CHECK(fude_zoom_circuit_tag_px(sizes, 10u, most, least) == 0.0);   // (5: under the least)
        f64 twice[10] = { 20, 20, 20, 20, 20, 20, 20, 20, 20, 8000 };
        CHECK(fude_zoom_circuit_tag_px(twice, 10u, most, least) == 10.0);
    }
    // Nothing to go by: none.
    {
        f64 junk[5] = { 0.0, -3.0, NAN, INFINITY, -INFINITY };
        CHECK(fude_zoom_circuit_tag_px(junk, 5u, most, least) == 0.0);
        CHECK(fude_zoom_circuit_tag_px(junk, 0u, most, least) == 0.0);
        f64 some[6] = { NAN, 24.0, 0.0, INFINITY, 24.0, -1.0 };
        CHECK(fude_zoom_circuit_tag_px(some, 6u, most, least) == 12.0);   // (the sizes there are, only)
    }
    // Random circuits at random zooms: 0 or between the least and the most; never smaller zoomed in; the parts' order
    // nothing to it; twice as big on the screen (both in range), twice as big.
    {
        u32 rounds = 0, right = 0;
        for(u32 r = 0; r < 500u; r++) {
            const u32 n = 1u + rnd() % 40u;
            f64 a[40], b[40], c[40], d[40];
            for(u32 i = 0; i < n; i++) {
                a[i] = 2.0 + rndf() * (rnd() % 8u == 0u ? 600.0 : 40.0);
                if(rnd() % 20u == 0u) a[i] = rnd() % 2u ? NAN : 0.0;
            }
            const f64 k = 0.05 + rndf() * 4.0;
            for(u32 i = 0; i < n; i++) { b[i] = a[i] * k; c[i] = a[i] * k * 1.7; d[i] = a[n - 1u - i] * k; }
            const f64 pb = fude_zoom_circuit_tag_px(b, n, most, least);
            const f64 pc = fude_zoom_circuit_tag_px(c, n, most, least);
            const f64 pd = fude_zoom_circuit_tag_px(d, n, most, least);
            b8 ok = (pb == 0.0 || (pb >= least && pb <= most)) && pc >= pb && pd == pb;
            for(u32 i = 0; i < n; i++) { b[i] = a[i] * k; c[i] = a[i] * k * 2.0; }
            const f64 p1 = fude_zoom_circuit_tag_px(b, n, 1e9, 0.0), p2 = fude_zoom_circuit_tag_px(c, n, 1e9, 0.0);
            ok = ok && (p1 == 0.0 ? p2 == 0.0 : fabs(p2 - 2.0 * p1) < 1e-9 * p2);
            rounds++; right += ok;
        }
        CHECK(right == rounds);
    }
}

// --- the examples (Insert → Examples): each drawn as the app draws it, run as Play runs it
// ------------------------------

// The parts of example e's drawing that are id (in the order drawn; with text: only those saying
// it): into out. How many.
static u32 example_parts(const fude_zoom_scene* s, const c8* id, const c8* text, u32* out, u32 most) {
    u32 n = 0;
    for(u32 o = 0; o < fude_zoom_scene_object_count(s) && n < most; o++) {
        const fude_zoom_part* p = fude_zoom_part_of(s, o);
        if(p == NULL || strcmp(p->id, id) != 0) continue;
        if(text != NULL) {
            f64 num[FUDE_ZOOM_SHAPE_NUMBERS + 200];
            const u32 k = fude_zoom_scene_shape_numbers(s, o, num, FUDE_ZOOM_SHAPE_NUMBERS + 200);
            c8 t[FUDE_ZOOM_SYMBOL_TEXT];
            fude_zoom_symbol_text(num, k, t, sizeof(t));
            if(strcmp(t, text) != 0) continue;
        }
        out[n++] = o;
    }
    return n;
}
static u32 example_part(const fude_zoom_scene* s, const c8* id, const c8* text) {
    u32 o = FUDE_ZOOM_NONE;
    CHECK(example_parts(s, id, text, &o, 1u) == 1u);
    return o;
}

static void example_open(fude_zoom_scene* s, fude_zoom_circuit* c, u32 e) {
    fude_zoom_scene_init(s, 7);
    rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    CHECK(fude_zoom_example_build(s, s->root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, e, NULL, &born) > 3u);
    rde_arr_free(&born);
    fude_zoom_circuit_init(c);
    CHECK(fude_zoom_circuit_build(c, s) > 0u);
}
static void example_close(fude_zoom_scene* s, fude_zoom_circuit* c) {
    fude_zoom_circuit_destroy(c);
    fude_zoom_scene_destroy(s);
}

// Every custom part of the tests' library (a half adder to a 4 × 4 memory of registers of flip-flops of latches of
// gates, counters): its inside drawn from its definition, made a part again — the same ports, in order — and the two
// side by side on the same inputs (clocks among them) for 200 random steps: every output the same at every step.
static void test_logic_draw(void) {
    for(u32 i = 0; i < PARTS_N; i++) {
        c8 err[160];
        CHECK(fude_zoom_logic_learn(PARTS_ALL[i], strlen(PARTS_ALL[i]), err, sizeof err) != NULL);
    }
    u32 checked = 0, matched = 0;
    for(u32 i = 0; i < PARTS_N; i++) {
        c8 err[160];
        const fude_sim_def* def = fude_zoom_logic_learn(PARTS_ALL[i], strlen(PARTS_ALL[i]), err, sizeof err);
        if(def == NULL) continue;
        // Its inside, drawn; made a part again.
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_placer pl = fude_zoom_placer_make(&s, s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, &born);
        CHECK(fude_zoom_logic_draw(def, &pl) > 2u);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        u32 problem = 0;
        c8 id[48];
        snprintf(id, sizeof id, "user/redrawn-%u", i);
        fude_sim_def* again = fude_zoom_logic_make(&c, &s, id, id, &problem);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
        rde_arr_free(&born);
        CHECK(again != NULL);
        if(again == NULL) continue;
        // The same ports, in order.
        b8 same = rde_arr_length(&again->ports) == rde_arr_length(&def->ports);
        for(u32 k = 0; same && k < (u32)rde_arr_length(&def->ports); k++) {
            const fude_sim_port* a = &((const fude_sim_port*)def->ports.memory)[k], *b = &((const fude_sim_port*)again->ports.memory)[k];
            same = strcmp(a->name, b->name) == 0 && a->dir == b->dir;
        }
        if(!same) printf("  %s: its ports not the same drawn again\n", def->name);
        CHECK(same);
        rde_arr text = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
        fude_sim_def_write(again, &text);
        fude_sim_def_free(again);
        CHECK(fude_zoom_logic_learn((const c8*)text.memory, (u32)rde_arr_length(&text), err, sizeof err) != NULL);
        rde_arr_free(&text);
        // Both on the same inputs, an output probe each.
        fude_zoom_scene t; fude_zoom_scene_init(&t, 7);
        const u32 np = (u32)rde_arr_length(&def->ports);
        const u32 one = part_put(&t, "custom part", 0, 0, 60, 30.0 * (f64)np, def->name);
        const u32 two = part_put(&t, "custom part", 0, -100.0 * (f64)np, 60, 30.0 * (f64)np, id);
        u32 ins[64], outs[2][64], ni = 0, no = 0;
        for(u32 k = 0; k < np && k < 64u; k++) {
            const fude_sim_port* q = &((const fude_sim_port*)def->ports.memory)[k];
            if(q->dir == FUDE_SIM_OUT) {
                outs[0][no] = part_put(&t, "logic probe", 400, 60.0 * (f64)k, 15, 15, "");
                outs[1][no] = part_put(&t, "logic probe", 400, -100.0 * (f64)np + 60.0 * (f64)k, 15, 15, "");
                wire_put(&t, one, k, outs[0][no], 0);
                wire_put(&t, two, k, outs[1][no], 0);
                no++;
            } else {
                ins[ni] = part_put(&t, "logic input", -400, 60.0 * (f64)k, 20, 15, "0");
                wire_put(&t, ins[ni], 0, one, k);
                wire_put(&t, ins[ni], 0, two, k);
                ni++;
            }
        }
        fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &t);
        u32 differ = 0;
        for(u32 step_n = 0; step_n < 200u; step_n++) {
            for(u32 k = 0; k < ni; k++) set_in(&c, ins[k], rnd() & 1u);
            CHECK(step(&c) && step(&c) && step(&c));
            for(u32 k = 0; k < no; k++) differ += probe(&c, outs[0][k]) != probe(&c, outs[1][k]) ? 1u : 0u;
        }
        if(differ != 0u) printf("  %s: drawn again, %u outputs differ\n", def->name, differ);
        checked++;
        matched += differ == 0u && same ? 1u : 0u;
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&t);
    }
    CHECK(checked == PARTS_N && matched == checked);
}

// A custom part's inside drawn (fude_zoom_logic_draw): its wires come in the order of its nets' groups' names, and a
// group a bridge (a buffer of no delay, J…) made is named by the net it was joined INTO, the bridge's second — the name
// logic.c's fzlg_groups keeps over rde_disjoint_set's own (its smallest net), as the drawing always named them — so the
// canvas gets its objects in the order it always did. Here J1 joins Y (net 2) into m (net 4): the group {Y, m} is named
// 4, so its wire (U2 to Y) comes after Z's (U1 to Z, net 3); named by its smallest net it would come before.
static void test_logic_draw_order(void) {
    fude_sim_def* def = fude_sim_def_new("user/order", "order");
    fude_sim_def_port(def, "A", FUDE_SIM_LOGIC, FUDE_SIM_IN, 1u);    // net 0
    fude_sim_def_port(def, "B", FUDE_SIM_LOGIC, FUDE_SIM_IN, 1u);    // net 1
    fude_sim_def_port(def, "Y", FUDE_SIM_LOGIC, FUDE_SIM_OUT, 1u);   // net 2
    fude_sim_def_port(def, "Z", FUDE_SIM_LOGIC, FUDE_SIM_OUT, 1u);   // net 3
    const c8* u1[2] = { "A", "Z" };
    const c8* u2[2] = { "B", "m" };                                   // (m: net 4)
    const c8* j1[2] = { "Y", "m" };
    fude_sim_def_inst(def, "not", "U1", "", u1, 2u);
    fude_sim_def_inst(def, "not", "U2", "", u2, 2u);
    fude_sim_def_inst(def, "buf", "J1", "delay=0", j1, 2u);
    CHECK(rde_arr_length(&def->nets) == 5u);
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_placer pl = fude_zoom_placer_make(&s, s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, &born);
    CHECK(fude_zoom_logic_draw(def, &pl) > 2u);
    // Its wires, in the order made, each by the ids at its two ends.
    fude_zoom_id ends[8][2];
    u32 nw = 0;
    for(u32 b = 0; b < (u32)rde_arr_length(&born); b++) {
        const u32 o = ((const u32*)born.memory)[b];
        const fude_zoom_object* ob = fude_zoom_scene_object(&s, o);
        if(ob->kind != FUDE_ZOOM_KIND_SHAPE || ob->channels != FUDE_ZOOM_SHAPE_WIRE || nw >= 8u) continue;
        f64 n[FUDE_ZOOM_WIRE_NUMBERS];
        const u32 k = fude_zoom_scene_shape_numbers(&s, o, n, FUDE_ZOOM_WIRE_NUMBERS);
        i32 fp = -1, tp = -1;
        CHECK(fude_zoom_wire_of(n, k, NULL, &ends[nw][0], &fp, &ends[nw][1], &tp) >= 2u);
        nw++;
    }
    u32 nots[2] = { FUDE_ZOOM_NONE, FUDE_ZOOM_NONE };
    CHECK(example_parts(&s, "NOT gate", NULL, nots, 2u) == 2u);   // (U1, then U2: drawn in their order)
    const u32 parts[6] = { nots[0], nots[1], example_part(&s, "logic input", "A"), example_part(&s, "logic input", "B"),
                           example_part(&s, "logic probe", "Y"), example_part(&s, "logic probe", "Z") };
    fude_zoom_id ids[6] = { 0, 0, 0, 0, 0, 0 };   // U1, U2, A, B, Y, Z
    b8 all = true;
    for(u32 k = 0; k < 6u; k++) {
        all = all && parts[k] != FUDE_ZOOM_NONE;
        ids[k] = parts[k] != FUDE_ZOOM_NONE ? fude_zoom_scene_object(&s, parts[k])->id : 0u;
    }
    CHECK(all);
    #define JOINS(_w, _p, _q) ((ends[_w][0] == ids[_p] && ends[_w][1] == ids[_q]) || (ends[_w][0] == ids[_q] && ends[_w][1] == ids[_p]))
    CHECK(nw == 4u);
    CHECK(all && nw == 4u && JOINS(0, 2, 0) && JOINS(1, 3, 1));   // (groups 0 and 1: A to U1, B to U2)
    CHECK(all && nw == 4u && JOINS(2, 0, 5));                     // (group 3: U1 to Z)
    CHECK(all && nw == 4u && JOINS(3, 1, 4));                     // (group 4, Y and m joined into m: U2 to Y)
    #undef JOINS
    rde_arr_free(&born);
    fude_zoom_scene_destroy(&s);
    fude_sim_def_free(def);
}

// The circuit's nodes, blocks and references come from RDE's disjoint sets (circuit_build.c, circuit_blocks.c), each
// set named by its smallest terminal or node, and are numbered by walking those in order: so the same parts joined by
// the same wires give every pin the same node, block, unknown's slot and vertex, and the same volts, whatever order
// the wires were drawn in (so whatever order their ends are joined in). Two islands: a divider on ground, and a
// battery, a resistor and an LED on none (held at a reference); the same wires in order, then backwards.
static void test_circuit_nodes_order(void) {
    typedef struct { u32 a, pa, b, pb; } link;
    const link links[7] = { { 0, 0, 1, 0 }, { 1, 1, 2, 0 }, { 2, 1, 3, 0 }, { 0, 1, 3, 0 }, { 4, 0, 5, 0 }, { 5, 1, 6, 0 }, { 6, 1, 4, 1 } };
    fude_zoom_scene s[2];
    fude_zoom_circuit c[2];
    u32 obj[2][7];
    for(u32 run = 0; run < 2u; run++) {
        fude_zoom_scene_init(&s[run], 7);
        u32* o = obj[run];
        o[0] = part_put(&s[run], "DC source", 0, 0, 20, 30, "10V");
        o[1] = part_put(&s[run], "resistor", 100, 60, 30, 10, "1k");
        o[2] = part_put(&s[run], "resistor", 200, 0, 30, 10, "2k");
        o[3] = part_put(&s[run], "ground", 0, -100, 20, 20, "");
        o[4] = part_put(&s[run], "battery", 600, 0, 20, 30, "9V");
        o[5] = part_put(&s[run], "resistor", 700, 60, 30, 10, "1k");
        o[6] = part_put(&s[run], "LED", 800, 0, 30, 20, "red");
        for(u32 k = 0; k < 7u; k++) {
            const link l = links[run == 0u ? k : 6u - k];   // (each drawn the same way: the same route, only later)
            wire_put(&s[run], o[l.a], l.pa, o[l.b], l.pb);
        }
        fude_zoom_circuit_init(&c[run]);
        CHECK(fude_zoom_circuit_build(&c[run], &s[run]) == 7u);
        CHECK(fude_zoom_circuit_dc(&c[run]));
    }
    // What the islands are: ground's pin node 0, the divider's middle one node, the two islands apart, one reference.
    const fude_zoom_circuit_part* g  = circuit_part(&c[0], obj[0][3]);
    const fude_zoom_circuit_part* r1 = circuit_part(&c[0], obj[0][1]), *r2 = circuit_part(&c[0], obj[0][2]);
    const fude_zoom_circuit_part* r3 = circuit_part(&c[0], obj[0][5]);
    CHECK(g != NULL && r1 != NULL && r2 != NULL && r3 != NULL);
    if(g != NULL && r1 != NULL && r2 != NULL && r3 != NULL) {
        CHECK(g->node[0] == 0u && r1->node[1] == r2->node[0] && r1->node[1] != 0u && r1->node[1] != FUDE_ZOOM_NONE);
        CHECK(r1->block != FUDE_ZOOM_NONE && r3->block != FUDE_ZOOM_NONE && r1->block != r3->block);
        CHECK(rde_arr_length(&c[0].block_reference) == 1u);
    }
    // The same numbers in both, pin by pin.
    CHECK(c[0].nodes == c[1].nodes && c[0].blocks == c[1].blocks && c[0].vertices == c[1].vertices);
    for(u32 i = 0; i < 7u; i++) {
        const fude_zoom_circuit_part* p0 = circuit_part(&c[0], obj[0][i]), *p1 = circuit_part(&c[1], obj[1][i]);
        CHECK(p0 != NULL && p1 != NULL);
        if(p0 == NULL || p1 == NULL) continue;
        CHECK(p0->block == p1->block);
        for(u32 k = 0; k < p0->part->pin_count; k++) {
            CHECK(p0->node[k] == p1->node[k]);
            CHECK(fabs(circuit_volts(&c[0], obj[0][i], k) - circuit_volts(&c[1], obj[1][i], k)) < 1e-9);
        }
    }
    const rde_arr* same[4][2] = { { &c[0].node_block, &c[1].node_block }, { &c[0].node_slot, &c[1].node_slot },
                                  { &c[0].block_reference, &c[1].block_reference }, { &c[0].pin_vertex, &c[1].pin_vertex } };
    for(u32 k = 0; k < 4u; k++) {
        const usize n = rde_arr_length(same[k][0]);
        CHECK(n == rde_arr_length(same[k][1]) && (n == 0u || memcmp(same[k][0]->memory, same[k][1]->memory, n * sizeof(u32)) == 0));
    }
    for(u32 run = 0; run < 2u; run++) {
        fude_zoom_circuit_destroy(&c[run]);
        fude_zoom_scene_destroy(&s[run]);
    }
}

// What a person keeps (custom.h): which canvas is whose template, read back as written; a piece's symbols' families
// (the library's Custom tab shows it in its area's panel), as it is made, made again and read; a custom part made again
// keeping its pins in order; renamed (found by either name), forgotten (by neither).
static void test_custom(void) {
    // The template map.
    const c8* path = "./pieces_test/templates.txt";
    rde_file_create_missing_dirs(path);
    fude_zoom_templates t;
    fude_zoom_templates_init(&t);
    fude_zoom_templates_set(&t, FUDE_ZOOM_CUSTOM_PIECE, "7", 101u);
    fude_zoom_templates_set(&t, FUDE_ZOOM_CUSTOM_PART, "user/adder", 102u);
    fude_zoom_templates_set(&t, FUDE_ZOOM_CUSTOM_PART, "user/adder", 103u);    // (its canvas another: in its place)
    fude_zoom_templates_set(&t, FUDE_ZOOM_CUSTOM_PART, "user/a b", 104u);      // (a key with a space: none is made so)
    fude_zoom_templates_set(&t, FUDE_ZOOM_CUSTOM_PART, "7", 105u);             // (a part's and a piece's keys apart)
    CHECK(rde_arr_length(&t.list) == 3u);
    CHECK(fude_zoom_templates_canvas(&t, FUDE_ZOOM_CUSTOM_PIECE, "7") == 101u && fude_zoom_templates_canvas(&t, FUDE_ZOOM_CUSTOM_PART, "user/adder") == 103u);
    CHECK(fude_zoom_templates_canvas(&t, FUDE_ZOOM_CUSTOM_PART, "7") == 105u && fude_zoom_templates_canvas(&t, FUDE_ZOOM_CUSTOM_PIECE, "8") == 0u);
    const fude_zoom_template* e = fude_zoom_templates_of(&t, 103u);
    CHECK(e != NULL && e->kind == FUDE_ZOOM_CUSTOM_PART && strcmp(e->key, "user/adder") == 0 && fude_zoom_templates_of(&t, 102u) == NULL && fude_zoom_templates_of(&t, 0u) == NULL);
    CHECK(fude_zoom_templates_write(&t, path));
    fude_zoom_templates r;
    fude_zoom_templates_init(&r);
    fude_zoom_templates_set(&r, FUDE_ZOOM_CUSTOM_PIECE, "99", 9u);   // (what was there: forgotten as it is read)
    fude_zoom_templates_read(&r, path);
    CHECK(rde_arr_length(&r.list) == 3u && fude_zoom_templates_canvas(&r, FUDE_ZOOM_CUSTOM_PART, "user/adder") == 103u &&
          fude_zoom_templates_canvas(&r, FUDE_ZOOM_CUSTOM_PIECE, "7") == 101u && fude_zoom_templates_canvas(&r, FUDE_ZOOM_CUSTOM_PIECE, "99") == 0u);
    fude_zoom_templates_rekey(&r, FUDE_ZOOM_CUSTOM_PART, "user/adder", "user/summer");
    CHECK(fude_zoom_templates_canvas(&r, FUDE_ZOOM_CUSTOM_PART, "user/adder") == 0u && fude_zoom_templates_canvas(&r, FUDE_ZOOM_CUSTOM_PART, "user/summer") == 103u);
    CHECK(fude_zoom_templates_drop(&r, FUDE_ZOOM_CUSTOM_PIECE, "7") == 101u && fude_zoom_templates_drop(&r, FUDE_ZOOM_CUSTOM_PIECE, "7") == 0u && rde_arr_length(&r.list) == 2u);
    // A move refused changes nothing: to what could not be a key (the template kept, not lost), onto a key with a
    // template of its own (neither canvas left belonging to nobody), or from a key with none.
    fude_zoom_templates_set(&r, FUDE_ZOOM_CUSTOM_PART, "user/other", 120u);
    CHECK(!fude_zoom_templates_rekey(&r, FUDE_ZOOM_CUSTOM_PART, "user/summer", "user/a b") && fude_zoom_templates_canvas(&r, FUDE_ZOOM_CUSTOM_PART, "user/summer") == 103u);
    CHECK(!fude_zoom_templates_rekey(&r, FUDE_ZOOM_CUSTOM_PART, "user/summer", "") && fude_zoom_templates_canvas(&r, FUDE_ZOOM_CUSTOM_PART, "user/summer") == 103u);
    CHECK(!fude_zoom_templates_rekey(&r, FUDE_ZOOM_CUSTOM_PART, "user/summer", "user/other"));
    CHECK(fude_zoom_templates_canvas(&r, FUDE_ZOOM_CUSTOM_PART, "user/summer") == 103u && fude_zoom_templates_canvas(&r, FUDE_ZOOM_CUSTOM_PART, "user/other") == 120u);
    CHECK(fude_zoom_templates_of(&r, 103u) != NULL && fude_zoom_templates_of(&r, 120u) != NULL);
    CHECK(!fude_zoom_templates_rekey(&r, FUDE_ZOOM_CUSTOM_PART, "user/none", "user/new") && fude_zoom_templates_canvas(&r, FUDE_ZOOM_CUSTOM_PART, "user/new") == 0u);
    CHECK(fude_zoom_templates_rekey(&r, FUDE_ZOOM_CUSTOM_PART, "user/summer", "user/summer") && fude_zoom_templates_canvas(&r, FUDE_ZOOM_CUSTOM_PART, "user/summer") == 103u);
    CHECK(fude_zoom_templates_drop(&r, FUDE_ZOOM_CUSTOM_PART, "user/other") == 120u && rde_arr_length(&r.list) == 2u);
    // A file of lines that are not its: left out; a missing one: none.
    FILE* f = fopen(path, "wb");
    fputs("p 3 7\nnonsense\nu user/x 0\nq user/y 4\nu user/z 12", f);
    fclose(f);
    fude_zoom_templates_read(&r, path);
    CHECK(rde_arr_length(&r.list) == 2u && fude_zoom_templates_canvas(&r, FUDE_ZOOM_CUSTOM_PIECE, "3") == 7u && fude_zoom_templates_canvas(&r, FUDE_ZOOM_CUSTOM_PART, "user/z") == 12u);
    rde_file_delete(path);
    fude_zoom_templates_read(&r, path);
    CHECK(rde_arr_length(&r.list) == 0u);
    fude_zoom_templates_free(&t);
    fude_zoom_templates_free(&r);

    // A piece's families: a drawing's none; with a gear and a resistor, theirs.
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_placer pl = fude_zoom_placer_make(&s, s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, &born);
    const u32 st   = line(&s, 0, 0, 100, 0, 2.0, 2.0f);
    const u32 gear = fude_zoom_placer_part(&pl, "gear 20T", 200, 0, 0, 0, 0, "");
    const u32 res  = fude_zoom_placer_part(&pl, "resistor", 400, 0, 0, 0, 0, "1k");
    CHECK(gear != FUDE_ZOOM_NONE && res != FUDE_ZOOM_NONE);
    const u32 drawing[1] = { st }, both[3] = { st, gear, res };
    rde_arr a = rde_arr_new(sizeof(fude_zoom_clip), NULL), b = rde_arr_new(sizeof(fude_zoom_clip), NULL);
    fude_zoom_select_clips_of(&s, drawing, 1u, &a);
    fude_zoom_select_clips_of(&s, both, 3u, &b);
    const u32 mech = 1u << FUDE_ZOOM_SYMBOL_FAMILY_MECHANISMS, elec = 1u << FUDE_ZOOM_SYMBOL_FAMILY_ELECTRONICS;
    const fude_zoom_box box = { -100, -100, 500, 100 };
    const c8* dir = "./pieces_test/";
    rde_file_delete("./pieces_test/index.kana");
    fude_zoom_pieces lib;
    memset(&lib, 0, sizeof(lib));
    fude_zoom_pieces_load(&lib, dir);
    CHECK(fude_zoom_pieces_add(&lib, dir, "Sketch", (const fude_zoom_clip*)a.memory, 1u, box) == 0u);
    CHECK(lib.list[0].families == 0u);
    CHECK(fude_zoom_pieces_add(&lib, dir, "Gear and resistor", (const fude_zoom_clip*)b.memory, 3u, box) == 0u);
    CHECK(lib.list[0].families == (mech | elec));
    // Made again (its template left): its things, its families; its id, its name, its place kept — on disk too.
    const u32 id = lib.list[1].id;
    CHECK(fude_zoom_pieces_replace(&lib, dir, 1u, (const fude_zoom_clip*)b.memory, 2u, box));
    CHECK(lib.list[1].id == id && strcmp(lib.list[1].name, "Sketch") == 0 && rde_arr_length(&lib.list[1].items) == 2u && lib.list[1].families == mech);
    CHECK(!fude_zoom_pieces_replace(&lib, dir, 1u, (const fude_zoom_clip*)b.memory, 0u, box) && !fude_zoom_pieces_replace(&lib, dir, 9u, (const fude_zoom_clip*)b.memory, 1u, box));
    fude_zoom_pieces again;
    memset(&again, 0, sizeof(again));
    fude_zoom_pieces_load(&again, dir);
    CHECK(again.count == 2u && again.list[1].id == id && rde_arr_length(&again.list[1].items) == 2u && again.list[1].families == mech && again.list[0].families == (mech | elec));
    // A piece of its own file (a custom part's drawing): as it was.
    CHECK(fude_zoom_piece_save(&again.list[0], "./pieces_test/own.piece"));
    fude_zoom_piece own;
    memset(&own, 0, sizeof(own));
    CHECK(fude_zoom_piece_load(&own, "./pieces_test/own.piece"));
    CHECK(strcmp(own.name, "Gear and resistor") == 0 && rde_arr_length(&own.items) == 3u && own.box.max_x == 500.0 && own.families == (mech | elec));
    CHECK(!fude_zoom_piece_load(&own, "./pieces_test/none.piece"));
    rde_file_delete("./pieces_test/own.piece");
    fude_zoom_piece_clear(&own);
    fude_zoom_pieces_free(&lib); fude_zoom_pieces_free(&again);
    fude_zoom_select_clips_free(&a); fude_zoom_select_clips_free(&b);
    rde_arr_free(&a); rde_arr_free(&b); rde_arr_free(&born);
    fude_zoom_scene_destroy(&s);

    // Made again: its pins in their old order by their names (new ones after them), each still on its own net.
    c8 err[160];
    const c8* old_text = "fude-part 1\nid user/keeper\nname Keeper\nport A logic in\nport B logic in\nport Y logic out 1 right\ninst and G \"\" A B Y\nend\n";
    const c8* new_text = "fude-part 1\nid user/keeper\nname Keeper\nport Y logic out 1 right\nport C logic in\nport B logic in\nport A logic in\n"
                         "inst and G \"inputs=3\" A B C Y\nend\n";
    // (each part learnt, renamed or forgotten moves the library's revision on, which Play's kept answer is keyed by:
    // what is a part changes with the scene untouched; a refusal leaves it)
    const u32 rev = fude_zoom_logic_revision();
    CHECK(fude_zoom_logic_learn("not a part", 10u, err, sizeof err) == NULL && fude_zoom_logic_revision() == rev);
    const fude_sim_def* old_def = fude_zoom_logic_learn(old_text, strlen(old_text), err, sizeof err);
    CHECK(fude_zoom_logic_revision() == rev + 1u);
    fude_sim_def* new_def = fude_sim_def_read(new_text, strlen(new_text), err, sizeof err);
    CHECK(old_def != NULL && new_def != NULL);
    if(old_def != NULL && new_def != NULL) {
        u32 net_of[4] = { 0 };   // (A, B, C, Y: their nets as read)
        const c8* names = "ABCY";
        for(u32 k = 0; k < 4u; k++) {
            const fude_sim_port* q = &((const fude_sim_port*)new_def->ports.memory)[k];
            net_of[strchr(names, q->name[0]) - names] = ((const u32*)new_def->port_nets.memory)[k];
        }
        fude_zoom_logic_keep_order(new_def, old_def);
        const c8* want[4] = { "A", "B", "Y", "C" };
        b8 order = true, nets = true;
        for(u32 k = 0; k < 4u; k++) {
            const fude_sim_port* q = &((const fude_sim_port*)new_def->ports.memory)[k];
            order = order && strcmp(q->name, want[k]) == 0;
            nets  = nets && ((const u32*)new_def->port_nets.memory)[k] == net_of[strchr(names, q->name[0]) - names];
        }
        CHECK(order && nets);
        CHECK(((const fude_sim_port*)new_def->ports.memory)[2].dir == FUDE_SIM_OUT && ((const fude_sim_port*)new_def->ports.memory)[3].dir == FUDE_SIM_IN);
        fude_zoom_logic_keep_order(new_def, NULL);   // (nothing before it: as it is)
        CHECK(strcmp(((const fude_sim_port*)new_def->ports.memory)[0].name, "A") == 0);
        fude_sim_def_free(new_def);
    }
    // Renamed: found by its new name and by its old one (what is drawn of it says that), its id kept.
    CHECK(fude_zoom_logic_find("Keeper") == old_def);
    CHECK(fude_zoom_logic_rename("user/keeper", "Guardian") && !fude_zoom_logic_rename("user/nobody", "X") && !fude_zoom_logic_rename("user/keeper", ""));
    CHECK(fude_zoom_logic_revision() == rev + 2u);
    CHECK(fude_zoom_logic_find("Guardian") == old_def && fude_zoom_logic_find("Keeper") == old_def && fude_zoom_logic_find("user/keeper") == old_def);
    c8 slug[40];
    fude_zoom_logic_slug("  Half adder #2 (v3) ", slug, sizeof slug);
    CHECK(strcmp(slug, "half-adder-2-v3") == 0);
    fude_zoom_logic_slug("¿?", slug, sizeof slug);
    CHECK(strcmp(slug, "part") == 0);
    // Forgotten: found by neither; a chip is not a person's to forget.
    CHECK(fude_zoom_logic_forget_part("user/keeper") && !fude_zoom_logic_forget_part("user/keeper"));
    CHECK(fude_zoom_logic_revision() == rev + 3u);
    CHECK(fude_zoom_logic_find("Guardian") == NULL && fude_zoom_logic_find("Keeper") == NULL && fude_zoom_logic_find("user/keeper") == NULL);
    CHECK(!fude_zoom_logic_forget_part("74HC00"));
    // Made again under its first name after: a new one, found.
    const fude_sim_def* fresh = fude_zoom_logic_learn(old_text, strlen(old_text), err, sizeof err);
    CHECK(fresh != NULL && fude_zoom_logic_find("Keeper") == fresh);
}

// An LED of every colour lit from 5 V through 330 Ω: at its forward voltage, its current what is left over the resistor
// (a blue one's 3 V too: its exponent counted from there — straightened from 0 V, it never conducted).
static void test_led_colours(void) {
    static const c8* const col[8] = { "red", "orange", "yellow", "green", "blue", "purple", "pink", "white" };
    static const f64 vf[8] = { 1.8, 2.0, 2.1, 2.2, 3.0, 3.1, 3.0, 3.0 };
    u32 right = 0;
    for(u32 i = 0; i < 8u; i++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 rail = part_put(&s, "supply rail", 0, 100, 30, 10, "5V");
        const u32 r = part_put(&s, "resistor", 100, 50, 30, 10, "330");
        const u32 led = part_put(&s, "LED", 200, 50, 30, 20, col[i]);
        const u32 g = part_put(&s, "ground", 300, 0, 20, 20, "");
        wire_put(&s, rail, 0, r, 0); wire_put(&s, r, 1, led, 0); wire_put(&s, led, 1, g, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(fude_zoom_circuit_run(&c, 0.02, 200u));
        const fude_zoom_circuit_part* p = cpart(&c, led);
        const f64 v = fude_zoom_circuit_volts(&c, p->node[0]) - fude_zoom_circuit_volts(&c, p->node[1]), want = (5.0 - vf[i]) / 330.0;
        right += fabs(v - vf[i]) < 0.1 && fabs(p->pin_i[0] - want) < 0.1 * want && p->shown > 0.4 ? 1u : 0u;
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    CHECK(right == 8u);
}

// Kirchhoff at every node: the currents the parts' pins push into it add up to nothing (the largest left, amperes).
static f64 kirchhoff_worst(const fude_zoom_circuit* c) {
    rde_arr sum_arr = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std());
    rde_arr_resize(&sum_arr, c->nodes);
    f64* sum = (f64*)sum_arr.memory;
    const fude_zoom_circuit_part* p = (const fude_zoom_circuit_part*)c->parts.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&c->parts); i++) {
        for(u32 k = 0; k < p[i].part->pin_count && k < 64u; k++) {
            if(p[i].node[k] != FUDE_ZOOM_NONE && p[i].node[k] < c->nodes) sum[p[i].node[k]] += p[i].pin_i[k];
        }
    }
    f64 worst = 0.0;
    for(u32 n = 1; n < c->nodes; n++) worst = fmax(worst, fabs(sum[n]));
    rde_arr_free(&sum_arr);
    return worst;
}

// Circuits past what a dense system could take (and the old 512 nodes): a divider of 2000 resistors — each node's
// voltage exactly where it should be —, a 30 × 30 grid of them, a battery lighting an LED with no ground of its own on
// a canvas whose ground is elsewhere (its minus held at 0 V). Solved sparse, quickly, Kirchhoff kept at every node.
static void test_big_circuits(void) {
    printf("big circuits\n");
    {
        const u32 n = 2000;
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        rde_arr ids = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        const u32 rail = part_put(&s, "supply rail", -100, 0, 30, 10, "10V");
        for(u32 i = 0; i < n; i++) {
            const u32 r = part_put(&s, "resistor", 100.0 * (f64)i, 0, 30, 10, "100");   // (in a row: each wire short and clear)
            rde_arr_add(&ids, (any)&r);
        }
        const u32 g = part_put(&s, "ground", 100.0 * (f64)n + 50.0, -60, 20, 20, "");
        const u32* id = (const u32*)ids.memory;
        wire_put(&s, rail, 0, id[0], 0);
        for(u32 i = 0; i + 1u < n; i++) wire_put(&s, id[i], 1, id[i + 1u], 0);
        wire_put(&s, id[n - 1u], 1, g, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        printf("  divider: %u nodes, %u blocks\n", c.nodes, c.blocks);
        CHECK(c.nodes >= n);
        const clock_t t0 = clock();
        CHECK(fude_zoom_circuit_run(&c, 0.01, 100u));
        const f64 ms = 1000.0 * (f64)(clock() - t0) / CLOCKS_PER_SEC;
        printf("  divider: 10 steps in %.1f ms\n", ms);
        CHECK(ms < 2000.0);
        f64 worst = 0.0;
        for(u32 i = 0; i < n; i++) {
            const f64 want = 10.0 * (f64)(n - i - 1u) / (f64)n;   // (each resistor's far end)
            worst = fmax(worst, fabs(fude_zoom_circuit_volts(&c, cpart(&c, id[i])->node[1]) - want));
        }
        printf("  divider: furthest from its voltage %.2e V, Kirchhoff off by %.2e A at worst\n", worst, kirchhoff_worst(&c));
        CHECK(worst < 1e-3);
        CHECK(kirchhoff_worst(&c) < 1e-9);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
        rde_arr_free(&ids);
    }
    {
        const u32 side = 30;
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        rde_arr across = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std()), down = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        // Each crossing joined to the next one right and the next one down by a resistor (its pin 0 at the crossing).
        for(u32 r = 0; r < side; r++) for(u32 k = 0; k < side; k++) {
            const u32 a = part_put(&s, "resistor", 200.0 * k + 50.0, -200.0 * r, 30, 10, "100");
            const u32 b = part_put(&s, "resistor", 200.0 * k, -200.0 * r - 50.0, 30, 10, "100");
            rde_arr_add(&across, (any)&a); rde_arr_add(&down, (any)&b);
        }
        const u32* ac = (const u32*)across.memory; const u32* dn = (const u32*)down.memory;
        for(u32 r = 0; r < side; r++) for(u32 k = 0; k < side; k++) {
            const u32 i = r * side + k;
            wire_put(&s, ac[i], 0, dn[i], 0);
            if(k + 1u < side) wire_put(&s, ac[i], 1, ac[i + 1u], 0);
            if(r + 1u < side) wire_put(&s, dn[i], 1, dn[i + side], 0);
        }
        const u32 rail = part_put(&s, "supply rail", -200, 200, 30, 10, "5V");
        const u32 g = part_put(&s, "ground", 200.0 * side + 200.0, -200.0 * side - 200.0, 20, 20, "");
        wire_put(&s, rail, 0, ac[0], 0);
        wire_put(&s, ac[side * side - 1u], 1, g, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        printf("  grid: %u nodes, %u blocks\n", c.nodes, c.blocks);
        CHECK(fude_zoom_circuit_run(&c, 0.01, 100u));
        // Between its supply and ground, and falling from the corner it is fed at to the one it leaves by.
        const f64 v0 = fude_zoom_circuit_volts(&c, cpart(&c, ac[0])->node[0]);
        const f64 v1 = fude_zoom_circuit_volts(&c, cpart(&c, ac[side * side - 1u])->node[1]);
        const f64 vm = fude_zoom_circuit_volts(&c, cpart(&c, ac[(side / 2u) * side + side / 2u])->node[0]);
        printf("  grid: %.3f V, %.3f V in the middle, %.3f V; Kirchhoff off by %.2e A at worst\n", v0, vm, v1, kirchhoff_worst(&c));
        CHECK(v0 > vm && vm > v1 && v0 <= 5.0 && v1 >= 0.0);
        CHECK(kirchhoff_worst(&c) < 1e-9);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
        rde_arr_free(&across); rde_arr_free(&down);
    }
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 bat = part_put(&s, "battery", 0, 0, 20, 30, "9V");
        const u32 r = part_put(&s, "resistor", 100, 50, 30, 10, "470");
        const u32 led = part_put(&s, "LED", 200, 0, 30, 20, "red");
        wire_put(&s, bat, 0, r, 0); wire_put(&s, r, 1, led, 0); wire_put(&s, led, 1, bat, 1);
        const u32 rail = part_put(&s, "supply rail", 0, 400, 30, 10, "5V");   // (elsewhere: a grounded circuit)
        const u32 r2 = part_put(&s, "resistor", 100, 400, 30, 10, "1k");
        const u32 g = part_put(&s, "ground", 200, 350, 20, 20, "");
        wire_put(&s, rail, 0, r2, 0); wire_put(&s, r2, 1, g, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        CHECK(c.blocks == 1u);   // (the rail's circuit: its node held, ground the other side — nothing unknown)
        CHECK(fude_zoom_circuit_run(&c, 0.05, 100u));
        const fude_zoom_circuit_part* b = cpart(&c, bat);
        CHECK(fabs(fude_zoom_circuit_volts(&c, b->node[1])) < 1e-6);   // (its minus: 0 V)
        CHECK(cpart(&c, led)->shown > 0.3);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
}

// The events a circuit had (and cleared): how many of kind _kind on part object _object (FUDE_ZOOM_NONE: any).
static u32 events_of(fude_zoom_circuit* c, u8 kind, u32 object) {
    u32 n = 0;
    const fude_zoom_circuit_event* e = (const fude_zoom_circuit_event*)c->events.memory;
    const fude_zoom_circuit_part* p = (const fude_zoom_circuit_part*)c->parts.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&c->events); i++) {
        n += e[i].kind == kind && (object == FUDE_ZOOM_NONE || p[e[i].part].object == object) ? 1u : 0u;
    }
    return n;
}

// A circuit stepped _seconds in 10 ms runs (each run as Play's frames: its steps at most 400).
static b8 run_for(fude_zoom_circuit* c, f64 seconds) {
    b8 ok = true;
    for(f64 t = 0.0; t < seconds - 1e-9 && ok; t += 0.01) ok = fude_zoom_circuit_run(c, 0.01, 400u);
    return ok;
}

// The examples made to show what goes wrong do just that, and nothing else: the LED with no resistor burnt (its
// current), the one behind 470 Ω lit; the ¼ W resistor burnt after a moment, the 1 W one not; the lamps lit until a
// switch shorts them — behind the fuse it blows (nothing said: it took the short), with none the battery says so and
// the switch burns; the backwards electrolytic and the 6.3 V one popped (the wrong way round, past its volts), the 16 V
// one well; the BC547 burnt, the TIP120's lamp lit. And the LC rings: twice its supply and back at 1.6 Hz, still
// swinging seconds later.
static u32 cindex(const fude_zoom_circuit* c, u32 object) {
    for(u32 i = 0; i < (u32)rde_arr_length(&c->parts); i++) if(((const fude_zoom_circuit_part*)c->parts.memory)[i].object == object) return i;
    return FUDE_ZOOM_NONE;
}

static void test_examples_go_wrong(void) {
    printf("examples that go wrong\n");
    fude_zoom_scene s; fude_zoom_circuit c;
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_LED_RESISTOR);
        CHECK(run_for(&c, 0.5));
        CHECK(cpart(&c, example_part(&s, "LED", "red"))->shown > 0.5 && !cpart(&c, example_part(&s, "LED", "red"))->burnt);
        CHECK(cpart(&c, example_part(&s, "LED", "green"))->burnt);
        CHECK(rde_arr_length(&c.events) == 1u && ((const fude_zoom_circuit_event*)c.events.memory)[0].limit == FUDE_ZOOM_LIMIT_CURRENT);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_RESISTOR_WATTS);
        u32 r[2];
        CHECK(example_parts(&s, "resistor", NULL, r, 2u) == 2u);
        CHECK(run_for(&c, 0.2));
        CHECK(!cpart(&c, r[0])->burnt && cpart(&c, r[0])->stress > 2.0 && cpart(&c, r[0])->heat > 0.0);   // (heating: not yet)
        CHECK(run_for(&c, 2.0));
        CHECK(cpart(&c, r[0])->burnt && !cpart(&c, r[1])->burnt && fabs(cpart(&c, r[1])->stress - 0.52) < 0.02);
        CHECK(rde_arr_length(&c.events) == 1u);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_SHORT_FUSE);
        u32 sw[2], lamp[2];
        CHECK(example_parts(&s, "SPST switch", NULL, sw, 2u) == 2u && example_parts(&s, "lamp", NULL, lamp, 2u) == 2u);
        CHECK(run_for(&c, 0.5));
        CHECK(cpart(&c, lamp[0])->shown > 0.5 && cpart(&c, lamp[1])->shown > 0.5 && rde_arr_length(&c.events) == 0u);
        c8 say[32];
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[0]), -1, say, sizeof say));
        CHECK(run_for(&c, 0.5));
        CHECK(cpart(&c, example_part(&s, "fuse", NULL))->state[0] > 0.5 && cpart(&c, lamp[0])->shown < 0.01 && rde_arr_length(&c.events) == 0u);
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[1]), -1, say, sizeof say));
        CHECK(run_for(&c, 0.5));
        CHECK(events_of(&c, FUDE_ZOOM_CIRCUIT_OVER, example_part(&s, "battery", NULL)) == 0u);   // (the fused one's battery: never)
        u32 bats[2];
        CHECK(example_parts(&s, "battery", NULL, bats, 2u) == 2u);
        CHECK(events_of(&c, FUDE_ZOOM_CIRCUIT_OVER, bats[1]) == 1u && cpart(&c, sw[1])->burnt);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_CAP_POLARITY);
        u32 caps[3];
        CHECK(example_parts(&s, "electrolytic", NULL, caps, 3u) == 3u);
        CHECK(run_for(&c, 1.0));
        CHECK(!cpart(&c, caps[0])->burnt && cpart(&c, caps[1])->burnt && cpart(&c, caps[2])->burnt);
        CHECK(events_of(&c, FUDE_ZOOM_CIRCUIT_BURNT, caps[1]) == 1u && events_of(&c, FUDE_ZOOM_CIRCUIT_BURNT, caps[2]) == 1u && rde_arr_length(&c.events) == 2u);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_TRANSISTOR_SIZE);
        u32 lamp[2];
        CHECK(example_parts(&s, "lamp", NULL, lamp, 2u) == 2u);
        CHECK(run_for(&c, 1.0));
        CHECK(cpart(&c, example_part(&s, "NPN", "BC547"))->burnt && !cpart(&c, example_part(&s, "NPN", "TIP120"))->burnt);
        CHECK(cpart(&c, lamp[1])->shown > 0.5 && cpart(&c, lamp[0])->shown < 0.05);
        CHECK(rde_arr_length(&c.events) == 1u);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_RINGING);
        const u32 vm = example_part(&s, "voltmeter", NULL);
        f64 top = -1e9, bottom = 1e9, late = -1e9;
        for(f64 t = 0.0; t < 6.0; t += 0.01) {
            CHECK(fude_zoom_circuit_run(&c, 0.01, 400u));
            const f64 v = cpart(&c, vm)->shown;
            if(t < 0.5) top = fmax(top, v);
            if(t > 0.45 && t < 0.8) bottom = fmin(bottom, v);
            if(t > 5.0) late = fmax(late, v);
        }
        printf("  ringing: up to %.2f V, back to %.2f V, still up to %.2f V after 5 s\n", top, bottom, late);
        CHECK(top > 9.0 && bottom < 1.0 && late > 8.0);
        CHECK(rde_arr_length(&c.events) == 0u);
        example_close(&s, &c);
    }
}

// A SERVO on its own (circuit.h): a pulse source's pulses timed exactly (the steps land on its edges) — 1 ms 45°,
// 1.5 ms 90°, 2 ms 135°, 0.5 ms 0°, 2.5 ms 180° —; turning at its speed (60° in 0.1 s at 4.8 V: 45° in 75 ms at 5 V,
// a little faster); drawing more turning than at rest; on 3 V, still; on 12 V, past its 7 V: burnt.
static void test_servo_alone(void) {
    printf("servo\n");
    const c8* const pulse[5] = { "50Hz 5V 1ms", "50Hz 5V 1.5ms", "50Hz 5V 2ms", "50Hz 5V 0.5ms", "50Hz 5V 2.5ms" };
    const f64 want[5] = { 45.0, 90.0, 135.0, 0.0, 180.0 };
    for(u32 k = 0; k < 7u; k++) {
        const c8* supply = k == 5u ? "3V" : (k == 6u ? "12V" : "5V");
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 sv = part_put(&s, "servo", 0, 0, 60, 40, "SG90 servo");
        const u32 rail = part_put(&s, "supply rail", -300, 200, 30, 10, supply);
        const u32 g = part_put(&s, "ground", -300, -200, 20, 20, "");
        const u32 clk = part_put(&s, "clock", -500, 0, 20, 30, pulse[k < 5u ? k : 0u]);
        wire_put(&s, rail, 0, sv, 1); wire_put(&s, sv, 0, g, 0); wire_put(&s, clk, 0, sv, 2); wire_put(&s, clk, 1, g, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        const fude_zoom_circuit_part* p = cpart(&c, sv);
        CHECK(p != NULL && p->part->model == FUDE_ZOOM_MODEL_SERVO);
        if(k == 0u) {
            // On its way: after its first pulse (ending at 1 ms), 45° to go at 625°/s.
            CHECK(run_for(&c, 0.04));
            CHECK(p->shown < 89.0 && p->shown > 60.0);
            CHECK(fabs(p->pin_i[1]) > 0.1);   // (turning: more than at rest)
        }
        CHECK(run_for(&c, k == 6u ? 1.0 : 0.5));
        if(k < 5u) {
            if(fabs(p->shown - want[k]) > 0.1) printf("  servo %s: %.3f degrees\n", pulse[k], p->shown);
            CHECK(fabs(p->shown - want[k]) < 0.1);
            CHECK(fabs(fabs(p->pin_i[1]) - 0.01) < 1e-3);   // (there: at rest, 10 mA)
        } else if(k == 5u) {
            CHECK(fabs(p->shown - 90.0) < 1e-9 && fabs(p->pin_i[1]) < 1e-3);   // (3 V: too little to work)
        } else {
            CHECK(p->burnt);   // (12 V: past its 7 V)
        }
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
}

// DISPLAYS (display.h). A one-pin part (a rail of _volts; NULL: a ground) beside pin _pa of _a, _dx, _dy from it, a
// wire straight between them.
static u32 tie(fude_zoom_scene* s, u32 a, u32 pa, f64 dx, f64 dy, const c8* volts) {
    fude_zoom_v2 at, tp;
    CHECK(fude_zoom_part_pin_at(s, a, pa, &at));
    const u32 t = volts == NULL ? part_put(s, "ground", at.x + dx, at.y + dy - 20.0, 20, 20, "") : part_put(s, "supply rail", at.x + dx, at.y + dy + 10.0, 30, 10, volts);
    CHECK(fude_zoom_part_pin_at(s, t, 0, &tp));
    const fude_zoom_v2 r[2] = { at, tp };
    fude_zoom_wire_add(s, s->root, r, 2u, a, (i32)pa, t, 0, (rde_color){ 1, 1, 1, 255 }, 0.1f);
    return t;
}

// A resistor of _ohms out to the left of a pin on its part's left (its near end _dx from it), its far end tied (tie).
static void resist(fude_zoom_scene* s, u32 a, u32 pa, f64 dx, const c8* ohms, const c8* volts) {
    fude_zoom_v2 at, rp;
    CHECK(fude_zoom_part_pin_at(s, a, pa, &at));
    const u32 r = part_put(s, "resistor", at.x - dx - 20.0, at.y, 20, 7, ohms);
    CHECK(fude_zoom_part_pin_at(s, r, 1, &rp));
    const fude_zoom_v2 w[2] = { at, rp };
    fude_zoom_wire_add(s, s->root, w, 2u, a, (i32)pa, r, 1, (rde_color){ 1, 1, 1, 255 }, 0.1f);
    tie(s, r, 0, -30.0, 0.0, volts);
}

// A wire from pin _pa of _a through the points given to pin _pb of _b (laid by hand: over no other pin).
static u32 wire_by(fude_zoom_scene* s, u32 a, u32 pa, u32 b, u32 pb, const fude_zoom_v2* via, u32 n) {
    fude_zoom_v2 r[FUDE_ZOOM_WIRE_POINTS];
    CHECK(fude_zoom_part_pin_at(s, a, pa, &r[0]) && fude_zoom_part_pin_at(s, b, pb, &r[n + 1u]));
    for(u32 i = 0; i < n; i++) r[1u + i] = via[i];
    return fude_zoom_wire_add(s, s->root, r, n + 2u, a, (i32)pa, b, (i32)pb, (rde_color){ 1, 1, 1, 255 }, 0.1f);
}

static const f32* glow_of(fude_zoom_circuit* c, u32 object) { return (const f32*)fude_zoom_circuit_store(c, cpart(c, object)); }

// The n-th part of model _model in circuit _c (FUDE_ZOOM_NONE: none).
static u32 nth_of(const fude_zoom_circuit* c, u8 model, u32 n) {
    const fude_zoom_circuit_part* p = (const fude_zoom_circuit_part*)c->parts.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&c->parts); i++) {
        if(p[i].part->model == model && n-- == 0u) return i;
    }
    return FUDE_ZOOM_NONE;
}

// The display examples (examples.h), played: the digit counting 0–9 then blank; the matrix's rows their numbers; the
// bar six high; the panel meters' readings; the LCD on and an H written by its button.
static void test_display_examples(void) {
    printf("display examples\n");
    {
        fude_zoom_scene s; fude_zoom_circuit c;
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_DIGIT_COUNTER);
        const u32 d = nth_of(&c, FUDE_ZOOM_MODEL_SEG_PANEL, 0);
        CHECK(d != FUDE_ZOOM_NONE);
        static const u8 digits[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7C, 0x07, 0x7F, 0x67 };
        c8 seen[64] = { 0 };
        u32 n = 0;
        u8 last = 0xFF;
        for(u32 k = 0; k < 90u && n + 1u < sizeof seen; k++) {   // (9 s: 18 counts)
            CHECK(run_for(&c, 0.1));
            const f32* g = (const f32*)fude_zoom_circuit_store(&c, &((fude_zoom_circuit_part*)c.parts.memory)[d]);
            u8 m = 0;
            for(u32 i = 0; g != NULL && i < 7u; i++) m |= (u8)(g[i] > 0.5f ? 1u << i : 0u);
            if(m == last) continue;
            last = m;
            c8 ch = '?';
            for(u32 v = 0; v < 10u; v++) if(digits[v] == m) ch = (c8)('0' + v);
            if(m == 0) ch = '_';
            seen[n++] = ch;
        }
        printf("  counter on a digit: %s\n", seen);
        CHECK(strstr(seen, "0123456789_01") != NULL);   // (0 to 9, six counts blank, on again: 18 counts in 9 s)
        example_close(&s, &c);
    }
    {
        fude_zoom_scene s; fude_zoom_circuit c;
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_MATRIX_SCAN);
        const u32 m = nth_of(&c, FUDE_ZOOM_MODEL_LED_MATRIX, 0);
        CHECK(run_for(&c, 0.15));
        const f32* g = (const f32*)fude_zoom_circuit_store(&c, &((fude_zoom_circuit_part*)c.parts.memory)[m]);
        f64 on_least = 1.0, off_most = 0.0;
        for(u32 r = 0; g != NULL && r < 8u; r++) {
            for(u32 col = 0; col < 3u; col++) {
                const f64 b = g[r * 3u + col];
                if((r >> col) & 1u) on_least = fmin(on_least, b); else off_most = fmax(off_most, b);
            }
        }
        printf("  matrix scanned: its rows' numbers lit at %.3f at least, the rest %.4f at most\n", on_least, off_most);
        CHECK(on_least > 0.04 && off_most < 0.01);
        example_close(&s, &c);
    }
    {
        fude_zoom_scene s; fude_zoom_circuit c;
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_BAR_METER);
        const u32 b = nth_of(&c, FUDE_ZOOM_MODEL_BAR_GRAPH, 0);
        CHECK(run_for(&c, 0.2));
        const f32* g = (const f32*)fude_zoom_circuit_store(&c, &((fude_zoom_circuit_part*)c.parts.memory)[b]);
        u32 lit = 0;
        for(u32 i = 0; g != NULL && i < 10u; i++) { lit += g[i] > 0.5f; CHECK(i < 6u ? g[i] > 0.5f : g[i] < 0.02f); }
        printf("  bar graph meter: %u lit\n", lit);
        example_close(&s, &c);
    }
    {
        fude_zoom_scene s; fude_zoom_circuit c;
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_PANEL_METERS);
        CHECK(run_for(&c, 0.05));
        const fude_zoom_circuit_part* p = (const fude_zoom_circuit_part*)c.parts.memory;
        const f64 v = p[nth_of(&c, FUDE_ZOOM_MODEL_PANEL_METER, 0)].shown, a = p[nth_of(&c, FUDE_ZOOM_MODEL_PANEL_METER, 1)].shown;
        printf("  panel meters: %.3f V, %.3f mA\n", v, a * 1e3);
        CHECK(fabs(v - 4.5) < 0.05 && fabs(a - 9.0 / 1010.5) < 1e-5);
        example_close(&s, &c);
    }
    {
        fude_zoom_scene s; fude_zoom_circuit c;
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_LCD_BY_HAND);
        const u32 l = nth_of(&c, FUDE_ZOOM_MODEL_CHAR_LCD, 0), e = nth_of(&c, FUDE_ZOOM_MODEL_BUTTON, 0);
        fude_zoom_circuit_part* p = (fude_zoom_circuit_part*)c.parts.memory;
        CHECK(run_for(&c, 0.05));
        const fude_zoom_lcd* lc = (const fude_zoom_lcd*)fude_zoom_circuit_store(&c, &p[l]);
        CHECK(lc != NULL && lc->powered && !(lc->control & FUDE_ZOOM_LCD_D));
        // E pressed and let go: 00001111 sent (the display on, its cursor blinking).
        p[e].switch_on = 1u; CHECK(run_for(&c, 0.02));
        p[e].switch_on = 0u; CHECK(run_for(&c, 0.02));
        CHECK(lc != NULL && (lc->control & FUDE_ZOOM_LCD_D) && (lc->control & FUDE_ZOOM_LCD_B));
        // RS on, 01001000, E: an H.
        const u8 h = 0x48;
        u32 in = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&c.parts); i++) {
            if(p[i].part->model != FUDE_ZOOM_MODEL_LOGIC_IN) continue;
            // (the logic inputs in order: RS, RW, D0 … D7)
            p[i].switch_on = in == 0u ? 1u : (in == 1u ? 0u : (h >> (in - 2u)) & 1u);
            in++;
        }
        CHECK(in == 10u);
        CHECK(run_for(&c, 0.02));
        p[e].switch_on = 1u; CHECK(run_for(&c, 0.02));
        p[e].switch_on = 0u; CHECK(run_for(&c, 0.02));
        CHECK(lc != NULL && fude_zoom_lcd_shown(lc, 16, 2, 0, 0, NULL) == 'H');
        printf("  LCD by hand: on, an H at the top left\n");
        example_close(&s, &c);
    }
    {
        // RC on an oscilloscope: CH1 the square (0 and 5 V), CH2 the capacitor between them, charging toward each.
        fude_zoom_scene s; fude_zoom_circuit c;
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_SCOPE_RC);
        const u32 sp = nth_of(&c, FUDE_ZOOM_MODEL_SCOPE, 0);
        CHECK(sp != FUDE_ZOOM_NONE && run_for(&c, 0.2));
        const fude_zoom_circuit_part* p = &((const fude_zoom_circuit_part*)c.parts.memory)[sp];
        const f32* smp = (const f32*)fude_zoom_circuit_store(&c, p);
        f64 lo1 = 1e300, hi1 = -1e300, lo2 = 1e300, hi2 = -1e300;
        u32 between = 0;
        for(u32 i = 0; smp != NULL && i < FUDE_ZOOM_SCOPE_SAMPLES; i++) {
            lo1 = fmin(lo1, smp[i]); hi1 = fmax(hi1, smp[i]);
            lo2 = fmin(lo2, smp[FUDE_ZOOM_SCOPE_SAMPLES + i]); hi2 = fmax(hi2, smp[FUDE_ZOOM_SCOPE_SAMPLES + i]);
            between += smp[FUDE_ZOOM_SCOPE_SAMPLES + i] > 0.5f && smp[FUDE_ZOOM_SCOPE_SAMPLES + i] < 4.5f;
        }
        printf("  RC on a scope: CH1 %.2f to %.2f V, CH2 %.2f to %.2f V (%u samples on its way)\n", lo1, hi1, lo2, hi2, between);
        CHECK(fabs(lo1) < 0.05 && fabs(hi1 - 5.0) < 0.05 && lo2 > -0.05 && lo2 < 0.2 && hi2 > 4.8 && hi2 < 5.05 && between > 40u);
        example_close(&s, &c);
    }
    {
        // Night light and thermometer: a room's light, the LED off; dark, on; the meter 2.50 V at 25
        // °C, under 1.5 V at 60.
        fude_zoom_scene s; fude_zoom_circuit c;
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_LIGHT_TEMP);
        fude_zoom_circuit_part* p = (fude_zoom_circuit_part*)c.parts.memory;
        const u32 ldr = nth_of(&c, FUDE_ZOOM_MODEL_LDR, 0), ntc = nth_of(&c, FUDE_ZOOM_MODEL_THERMISTOR, 0), led = nth_of(&c, FUDE_ZOOM_MODEL_LED, 0), pm = nth_of(&c, FUDE_ZOOM_MODEL_PANEL_METER, 0);
        CHECK(ldr != FUDE_ZOOM_NONE && ntc != FUDE_ZOOM_NONE && led != FUDE_ZOOM_NONE && pm != FUDE_ZOOM_NONE);
        CHECK(run_for(&c, 0.05));
        const f64 room = p[led].shown, warm = p[pm].shown;
        p[ldr].state[0] = 1.0;
        p[ntc].state[0] = 60.0;
        CHECK(run_for(&c, 0.05));
        printf("  night light: the LED %.2f in a room's light, %.2f in the dark; the thermometer %.2f V at 25, %.2f V at 60\n", room, p[led].shown, warm, p[pm].shown);
        CHECK(room < 0.05 && p[led].shown > 0.5 && fabs(warm - 2.5) < 0.02 && p[pm].shown < 1.5);
        example_close(&s, &c);
    }
}

static void test_display_sizes(void) {
    printf("display sizes\n");
    const u32 panel = fude_zoom_symbol_find("7-segment panel"), matrix = fude_zoom_symbol_find("LED matrix"), lcd = fude_zoom_symbol_find("character LCD");
    CHECK(panel != FUDE_ZOOM_NONE && matrix != FUDE_ZOOM_NONE && lcd != FUDE_ZOOM_NONE);
    // A panel: its digits as its text says (none: 4; past 8: 8), its commons anodes when it says "CA"; its segments'
    // pins down its left, a common under each digit, 20 apart.
    const struct { const c8* text; u32 digits; } pt[5] = { { "4 digits", 4 }, { "", 4 }, { "1", 1 }, { "12 digits", 8 }, { "2 digits CA green", 2 } };
    for(u32 k = 0; k < 5u; k++) {
        const fude_zoom_part* p = fude_zoom_part_of_text(panel, pt[k].text);
        CHECK(p != NULL && p->model == FUDE_ZOOM_MODEL_SEG_PANEL && fude_zoom_display_made(p));
        CHECK(fude_zoom_display_cols(p) == pt[k].digits && p->pin_count == 8u + pt[k].digits && fude_zoom_display_lights(p) == 8u * pt[k].digits);
        f64 w, h;
        CHECK(fude_zoom_display_room(p, &w, &h) && fabs(w - (60.0 * pt[k].digits + 60.0)) < 1e-9 && fabs(h - 180.0) < 1e-9);
        CHECK(strcmp(p->pins[0].name, "a") == 0 && strcmp(p->pins[7].name, "dp") == 0 && strcmp(p->pins[8].name, "D1") == 0);
        for(u32 i = 0; i < p->pin_count; i++) {
            const f64 x = (f64)p->pins[i].u * w * 0.5, y = (f64)p->pins[i].v * h * 0.5;
            CHECK(fabs(remainder(x - (f64)p->pins[0].u * w * 0.5, 20.0)) < 1e-4 && fabs(remainder(y - (f64)p->pins[0].v * h * 0.5, 20.0)) < 1e-4);
        }
        u32 an, ca;
        CHECK(fude_zoom_display_led(p, 2u, &an, &ca));   // (digit 1's segment c)
        CHECK(k == 4u ? (an == 8u && ca == 2u) : (an == 2u && ca == 8u));
        CHECK(!fude_zoom_display_led(p, 8u * pt[k].digits, &an, &ca));
        CHECK(fude_zoom_part_of_text(panel, pt[k].text) == p);   // (made once, kept)
    }
    // A matrix: columns × rows ("5x7": 5 across, 7 down; up to 32 each), its rows' pins down its left, its columns'
    // along its bottom; its rows anodes, cathodes when it says "CC".
    const struct { const c8* text; u32 cols, rows; } mt[5] = { { "8x8", 8, 8 }, { "5x7", 5, 7 }, { "16 × 8", 16, 8 }, { "40x40", 32, 32 }, { "red", 8, 8 } };
    for(u32 k = 0; k < 5u; k++) {
        const fude_zoom_part* p = fude_zoom_part_of_text(matrix, mt[k].text);
        CHECK(fude_zoom_display_cols(p) == mt[k].cols && fude_zoom_display_rows(p) == mt[k].rows && p->pin_count == mt[k].cols + mt[k].rows);
        CHECK(fude_zoom_display_lights(p) == mt[k].cols * mt[k].rows && p->pin_count <= 64u);
        f64 w, h;
        CHECK(fude_zoom_display_room(p, &w, &h) && fabs(w - (20.0 * mt[k].cols + 40.0)) < 1e-9 && fabs(h - (20.0 * mt[k].rows + 40.0)) < 1e-9);
        CHECK(p->pins[0].side == FUDE_ZOOM_PIN_LEFT && p->pins[mt[k].rows].side == FUDE_ZOOM_PIN_DOWN && strcmp(p->pins[mt[k].rows].name, "C1") == 0);
    }
    u32 an, ca;
    CHECK(fude_zoom_display_led(fude_zoom_part_of_text(matrix, "5x7"), 7u, &an, &ca) && an == 1u && ca == 7u + 2u);       // (row 2, column 3)
    CHECK(fude_zoom_display_led(fude_zoom_part_of_text(matrix, "5x7 CC"), 7u, &an, &ca) && an == 7u + 2u && ca == 1u);
    // An LCD: columns × rows (8 to 40; 1, 2 or 4 rows; 80 characters at most), its 16 pins along its top.
    const struct { const c8* text; u32 cols, rows; } lt[6] = { { "16x2", 16, 2 }, { "20x4", 20, 4 }, { "40x4", 20, 4 }, { "8x1", 8, 1 }, { "50x2", 40, 2 }, { "", 16, 2 } };
    for(u32 k = 0; k < 6u; k++) {
        const fude_zoom_part* p = fude_zoom_part_of_text(lcd, lt[k].text);
        CHECK(fude_zoom_display_cols(p) == lt[k].cols && fude_zoom_display_rows(p) == lt[k].rows && p->pin_count == 16u);
        CHECK(strcmp(p->pins[0].name, "VSS") == 0 && strcmp(p->pins[5].name, "E") == 0 && strcmp(p->pins[15].name, "K") == 0 && p->pins[3].side == FUDE_ZOOM_PIN_UP);
    }
    // As the catalogue has them: the size each comes at its room.
    f64 w, h;
    CHECK(fude_zoom_display_room(fude_zoom_part_find("LED matrix"), &w, &h) && fabs(w - (f64)fude_zoom_symbol_info_of(matrix)->w) < 1e-9 &&
          fabs(h - (f64)fude_zoom_symbol_info_of(matrix)->h) < 1e-9);
    CHECK(fude_zoom_display_room(fude_zoom_part_find("7-segment panel"), &w, &h) && fabs(w - (f64)fude_zoom_symbol_info_of(panel)->w) < 1e-9);
    CHECK(fude_zoom_display_room(fude_zoom_part_find("character LCD"), &w, &h) && fabs(w - (f64)fude_zoom_symbol_info_of(lcd)->w) < 1e-9 &&
          fabs(h - (f64)fude_zoom_symbol_info_of(lcd)->h) < 1e-9);
    // Its grid on the canvas, a 16 × 16 drawn as it comes: 10 (its pins 20 apart).
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 m = part_put(&s, "LED matrix", 0, 0, 180, 180, "16x16");
    CHECK(fabs(fude_zoom_part_unit(&s, m) - 10.0) < 1e-9 && fude_zoom_part_of(&s, m)->pin_count == 32u);
    fude_zoom_scene_destroy(&s);
}

static void test_display_leds(void) {
    printf("display LEDs\n");
    // A 4-digit panel (common cathodes): segments b and c from 5 V through 330 Ω each, digit 2's common grounded — a 1
    // on it, nothing on the others.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 p = part_put(&s, "7-segment panel", 0, 0, 150, 90, "4 digits");
        resist(&s, p, 1, 30.0, "330", "5V");
        resist(&s, p, 2, 30.0, "330", "5V");
        tie(&s, p, 9, 0.0, -30.0, NULL);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.15));
        const f32* g = glow_of(&c, p);
        CHECK(g != NULL);
        if(g != NULL) {
            for(u32 k = 0; k < 32u; k++) {
                const b8 on = k == 9u || k == 10u;
                if(on ? !(g[k] > 0.85f && g[k] < 1.1f) : !(g[k] < 0.02f)) printf("  panel light %u: %.3f\n", k, (f64)g[k]);
                CHECK(on ? (g[k] > 0.85f && g[k] < 1.1f) : g[k] < 0.02f);
            }
        }
        CHECK(!cpart(&c, p)->burnt && cpart(&c, p)->measure[FUDE_ZOOM_LIMIT_CURRENT] > 0.008 && cpart(&c, p)->measure[FUDE_ZOOM_LIMIT_CURRENT] < 0.011);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // Common anodes ("CA"): digit 1's common on 5 V, segment a to ground through 330 Ω.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 p = part_put(&s, "7-segment panel", 0, 0, 90, 90, "2 digits CA");
        resist(&s, p, 0, 30.0, "330", NULL);
        tie(&s, p, 8, 0.0, -30.0, "5V");
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.15));
        const f32* g = glow_of(&c, p);
        CHECK(g != NULL && g[0] > 0.85f && g[1] < 0.02f && g[8] < 0.02f);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // Multiplexed: digit 1's common a 100 Hz clock (lit while it is low), digit 2's grounded, segment a through 330 Ω
    // for both — digit 1 seen steady, dimmer (lit half the time, sharing the resistor then), digit 2 brighter.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 p = part_put(&s, "7-segment panel", 0, 0, 90, 90, "2 digits");
        resist(&s, p, 0, 30.0, "330", "5V");
        fude_zoom_v2 d1;
        CHECK(fude_zoom_part_pin_at(&s, p, 8, &d1));
        const u32 clk = part_put(&s, "clock", d1.x, d1.y - 60.0, 20, 30, "100Hz 5V");
        wire_put(&s, p, 8, clk, 0);
        tie(&s, clk, 1, 0.0, -30.0, NULL);
        tie(&s, p, 9, 0.0, -30.0, NULL);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.3));
        const f32* g = glow_of(&c, p);
        const f64 a1 = g[0], a2 = g[8];
        CHECK(run_for(&c, 0.0025));
        g = glow_of(&c, p);
        printf("  multiplexed: digit 1 %.2f then %.2f, digit 2 %.2f then %.2f\n", a1, (f64)g[0], a2, (f64)g[8]);
        CHECK(a1 > 0.15 && a1 < 0.45 && a2 > 0.55 && a2 < 0.95);
        CHECK(fabs((f64)g[0] - a1) < 0.15 && fabs((f64)g[8] - a2) < 0.15);   // (steady: the eye holds it)
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // Its limits LED by LED: segment a through 82 Ω with all four commons grounded — 40 mA shared, 10 mA each: well;
    // with one common, all 40 mA in one LED: burnt.
    for(u32 k = 0; k < 2u; k++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 p = part_put(&s, "7-segment panel", 0, 0, 150, 90, "4 digits");
        resist(&s, p, 0, 30.0, "82", "5V");
        for(u32 d = 0; d < (k == 0u ? 4u : 1u); d++) tie(&s, p, 8u + d, 0.0, -30.0, NULL);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.5));
        CHECK(k == 0u ? !cpart(&c, p)->burnt : cpart(&c, p)->burnt);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // A 3 × 2 matrix: row 1 through 330 Ω from 5 V, column 2 grounded — that dot alone; its rows cathodes ("CC"): row 1
    // grounded, column 3 on 1.8 V (a red LED's at 10 mA) — that one.
    for(u32 k = 0; k < 2u; k++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 m = part_put(&s, "LED matrix", 0, 0, 50, 40, k == 0u ? "3x2" : "3x2 CC");
        if(k == 0u) {
            resist(&s, m, 0, 30.0, "330", "5V");
            tie(&s, m, 3, 0.0, -30.0, NULL);
        } else {
            tie(&s, m, 0, -30.0, 0.0, NULL);
            tie(&s, m, 4, 0.0, -30.0, "1.8V");
        }
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.15));
        const f32* g = glow_of(&c, m);
        const u32 lit = k == 0u ? 1u : 2u;
        for(u32 i = 0; g != NULL && i < 6u; i++) CHECK(i == lit ? (g[i] > 0.85f && g[i] < 1.1f) : g[i] < 0.02f);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
}

// A CD4511 (sim/chips.c): every BCD in on logic inputs, its segments on probes — 0–9 as it draws them (6 and 9 without
// tails), 10–15 blank; LT low all lit, BI low none; LE high holds what it had.
static void test_cd4511(void) {
    printf("CD4511\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 ic = part_put(&s, "CD4511", 0, 0, 100, 90, "CD4511");
    const fude_zoom_part* pt = fude_zoom_part_of(&s, ic);
    CHECK(pt != NULL && pt->pin_count == 16u && strcmp(pt->pins[6].name, "A") == 0 && strcmp(pt->pins[12].name, "a") == 0);
    u32 in[7], out[7];   // (B C ~LT ~BI LE D A; e d c b a g f)
    for(u32 k = 0; k < 7u; k++) {
        fude_zoom_v2 at;
        CHECK(fude_zoom_part_pin_at(&s, ic, k, &at));
        in[k] = part_put(&s, "logic input", at.x - 60.0, at.y, 30, 10, "0");
        wire_put(&s, in[k], 0, ic, k);
        CHECK(fude_zoom_part_pin_at(&s, ic, 8u + k, &at));
        out[k] = part_put(&s, "logic probe", at.x + 50.0, at.y, 20, 10, "");
        wire_put(&s, ic, 8u + k, out[k], 0);
    }
    tie(&s, ic, 7, -30.0, 0.0, NULL);
    tie(&s, ic, 15, 30.0, 0.0, "5V");
    fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
    static const u8 want[16] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7C, 0x07, 0x7F, 0x67, 0, 0, 0, 0, 0, 0 };   // (a bit 0 … g bit 6)
    static const u32 seg_probe[7] = { 4, 3, 2, 1, 0, 6, 5 };   // (a … g: their probes)
    set_in(&c, in[2], 1); set_in(&c, in[3], 1);   // (LT, BI high: neither)
    for(u32 v = 0; v < 16u; v++) {
        set_in(&c, in[6], v & 1u); set_in(&c, in[0], (v >> 1) & 1u); set_in(&c, in[1], (v >> 2) & 1u); set_in(&c, in[5], (v >> 3) & 1u);
        CHECK(step(&c) && step(&c));
        u8 got = 0;
        for(u32 g = 0; g < 7u; g++) got |= (u8)(probe(&c, out[seg_probe[g]]) << g);
        if(got != want[v]) printf("  CD4511 %u: segments %02X, not %02X\n", v, got, want[v]);
        CHECK(got == want[v]);
    }
    // 5 in, LE high (held), 3 in: still 5; LE low: 3.
    set_in(&c, in[6], 1); set_in(&c, in[0], 0); set_in(&c, in[1], 1); set_in(&c, in[5], 0);
    CHECK(step(&c) && step(&c));
    set_in(&c, in[4], 1);
    CHECK(step(&c));
    set_in(&c, in[6], 1); set_in(&c, in[0], 1); set_in(&c, in[1], 0);
    CHECK(step(&c) && step(&c));
    u8 got = 0;
    for(u32 g = 0; g < 7u; g++) got |= (u8)(probe(&c, out[seg_probe[g]]) << g);
    CHECK(got == want[5]);
    set_in(&c, in[4], 0);
    CHECK(step(&c) && step(&c));
    got = 0;
    for(u32 g = 0; g < 7u; g++) got |= (u8)(probe(&c, out[seg_probe[g]]) << g);
    CHECK(got == want[3]);
    // LT low: all lit (BI low too); LT high, BI low: none.
    set_in(&c, in[2], 0); set_in(&c, in[3], 0);
    CHECK(step(&c) && step(&c));
    got = 0;
    for(u32 g = 0; g < 7u; g++) got |= (u8)(probe(&c, out[seg_probe[g]]) << g);
    CHECK(got == 0x7F);
    set_in(&c, in[2], 1);
    CHECK(step(&c) && step(&c));
    got = 0;
    for(u32 g = 0; g < 7u; g++) got |= (u8)(probe(&c, out[seg_probe[g]]) << g);
    CHECK(got == 0);
    fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
}

// An LM3914 driving a bar graph: 5 V; its reference (1.25 V) on RHI, RLO grounded — steps of 125 mV; R1 1.2 kΩ (its
// LEDs' current ten times what REF OUT gives: 11.7 mA); SIG 0.6 V — four lit as a bar (MODE on V+), the fourth alone as
// a dot (MODE left open); SIG 2 V: all ten; 0.1 V: none.
static void test_lm3914(void) {
    printf("LM3914\n");
    const f64 sig[4] = { 0.6, 0.6, 2.0, 0.1 };
    const u32 lit_n[4] = { 4, 1, 10, 0 };
    for(u32 k = 0; k < 4u; k++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 bar = part_put(&s, "LED bar graph", 0, 0, 70, 110, "red");
        const u32 lm  = part_put(&s, "LM3914", 500, -400, 100, 100, "LM3914");
        for(u32 i = 0; i < 10u; i++) tie(&s, bar, i, -30.0, 0.0, "5V");   // (its anodes)
        tie(&s, lm, 1, -30.0, 0.0, NULL);    // V−
        tie(&s, lm, 2, -30.0, 0.0, "5V");    // V+
        tie(&s, lm, 3, -30.0, 0.0, NULL);    // RLO
        c8 v[16]; snprintf(v, sizeof v, "%gV", sig[k]);
        tie(&s, lm, 4, -30.0, 0.0, v);       // SIG
        const fude_zoom_v2 tie_r[2] = { { 380, -420 }, { 380, -440 } };
        wire_by(&s, lm, 5, lm, 6, tie_r, 2u);   // (RHI on REF OUT)
        const u32 r1 = part_put(&s, "resistor", 330, -440, 20, 7, "1.2k");
        wire_put(&s, lm, 6, r1, 1);
        const u32 g1 = part_put(&s, "ground", 310, -490, 20, 20, "");
        wire_put(&s, r1, 0, g1, 0);
        tie(&s, lm, 7, -30.0, 0.0, NULL);    // REF ADJ
        if(k != 1u) tie(&s, lm, 8, -30.0, 0.0, "5V");   // MODE: a bar
        // Its cathodes to its outputs: LED1 on the chip's left, LED2–LED10 round to its right, each its own way.
        for(u32 i = 0; i < 10u; i++) {
            const f64 y = 90.0 - 20.0 * (f64)i;
            if(i == 0u) {
                const fude_zoom_v2 via[2] = { { 150, y }, { 150, -320 } };
                wire_by(&s, bar, 10, lm, 0, via, 2u);
            } else {
                const f64 x = 160.0 + 12.0 * (f64)i, low = -700.0 - 12.0 * (f64)i, out = 700.0 + 12.0 * (f64)i, at = -300.0 - 20.0 * (f64)i;
                const fude_zoom_v2 via[4] = { { x, y }, { x, low }, { out, low }, { out, at } };
                wire_by(&s, bar, 10u + i, lm, 18u - i, via, 4u);
            }
        }
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.2));
        const f32* g = glow_of(&c, bar);
        u32 n = 0, top = 0;
        for(u32 i = 0; g != NULL && i < 10u; i++) {
            if(g[i] > 0.5f) { n++; top = i + 1u; }
            CHECK(g[i] > 0.5f || g[i] < 0.02f);
        }
        printf("  SIG %.1f V%s: %u lit, the highest %u (%.2f bright), its LEDs' current %.1f mA\n", sig[k], k == 1u ? " (a dot)" : "", n, top, g != NULL ? (f64)g[top > 0u ? top - 1u : 0u] : 0.0,
               cpart(&c, lm)->state[1] * 1e3);
        CHECK(n == lit_n[k] && (n == 0u || top == (k == 2u ? 10u : 4u)));
        CHECK(fabs(cpart(&c, lm)->state[1] - 0.01167) < 0.0008);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
}

// Panel meters: a 20 V one on 9 V shows 9.00; a 200 mA one in a row with 100 Ω on 5 V, 49.5 (its shunt 1 Ω); a 2 V one
// on 9 V, a 1 alone. What it shows of a reading, its 3½ digits.
static void test_panel_meter(void) {
    printf("panel meter\n");
    const fude_zoom_meter m20 = fude_zoom_display_meter("20V"), m200m = fude_zoom_display_meter("200mA"), m2 = fude_zoom_display_meter("2V");
    CHECK(fabs(m20.full - 20.0) < 1e-12 && !m20.amps && m20.decimals == 2u && fabs(m20.times - 1.0) < 1e-12);
    CHECK(fabs(m200m.full - 0.2) < 1e-12 && m200m.amps && m200m.decimals == 1u && fabs(m200m.times - 1000.0) < 1e-12);
    CHECK(m2.decimals == 3u && fude_zoom_display_meter("200V").decimals == 1u && fude_zoom_display_meter("200mV").decimals == 1u);
    const struct { const fude_zoom_meter* m; f64 v; const c8* digits; i32 point; b8 minus; } t[7] = {
        { &m20, 9.0, " 900", 1, false }, { &m20, -0.5, " 050", 1, true }, { &m20, 0.0, " 000", 1, false }, { &m20, 19.994, "1999", 1, false },
        { &m20, 19.996, "1   ", -1, false }, { &m200m, 0.0495, " 495", 2, false }, { &m2, 1.2345, "1235", 0, false },
    };
    for(u32 k = 0; k < 7u; k++) {
        fude_zoom_meter_shown sh;
        fude_zoom_display_meter_show(t[k].m, t[k].v, &sh);
        c8 d[5] = { sh.digit[0], sh.digit[1], sh.digit[2], sh.digit[3], 0 };
        i32 pt = -1;
        for(u32 i = 0; i < 4u; i++) if(sh.point[i]) pt = (i32)i;
        if(strcmp(d, t[k].digits) != 0 || pt != t[k].point || sh.minus != t[k].minus) printf("  meter %g: \"%s\" point %d minus %d\n", t[k].v, d, pt, sh.minus);
        CHECK(strcmp(d, t[k].digits) == 0 && pt == t[k].point && sh.minus == t[k].minus);
    }
    for(u32 k = 0; k < 3u; k++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 bat = part_put(&s, "battery", -200, 0, 20, 30, k == 1u ? "5V" : "9V");
        const u32 me  = part_put(&s, "panel meter", 0, 0, 80, 40, k == 0u ? "20V" : (k == 1u ? "200mA" : "2V"));
        if(k == 1u) {
            const u32 r = part_put(&s, "resistor", 0, -150, 30, 10, "100");
            wire_put(&s, bat, 0, me, 0); wire_put(&s, me, 1, r, 1); wire_put(&s, r, 0, bat, 1);
        } else {
            wire_put(&s, bat, 0, me, 0); wire_put(&s, me, 1, bat, 1);
        }
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.05));
        const fude_zoom_circuit_part* p = cpart(&c, me);
        const f64 want = k == 1u ? 5.0 / 101.5 : 9.0;   // (the battery's own 0.5 Ω, the shunt's 1 Ω, 100 Ω)
        if(fabs(p->shown - want) > 1e-3 * want) printf("  panel meter %u reads %g, not %g\n", k, p->shown, want);
        CHECK(fabs(p->shown - want) < 1e-3 * want && !p->burnt);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
}

// SENSORS AND CHANGEOVER SWITCHES. An LDR's resistance by its light (a GL5528's), a thermistor's by its temperature
// (10 kΩ, B 3950), their sliders' scales; each under 10 kΩ from 5 V: the divider as each says, tapped through its
// presets. An SPDT's common on 5 V into A's 1 kΩ, or (tapped) B's; a DPDT's two poles together.
static void test_sensors_switches(void) {
    printf("sensors and changeover switches\n");
    CHECK(fabs(fude_zoom_circuit_ldr_ohms(10.0) - 20000.0) < 1e-6 && fabs(fude_zoom_circuit_ldr_ohms(100.0) - 20000.0 * pow(10.0, -0.7)) < 1e-6);
    CHECK(fabs(fude_zoom_circuit_ldr_ohms(1.0) - 20000.0 * pow(10.0, 0.7)) < 1e-6 && fude_zoom_circuit_ldr_ohms(1e-3) == 1e6 && fude_zoom_circuit_ldr_ohms(1e5) == 100.0);
    CHECK(fabs(fude_zoom_circuit_ntc_ohms(10000.0, 3950.0, 25.0) - 10000.0) < 1e-6);
    CHECK(fabs(fude_zoom_circuit_ntc_ohms(10000.0, 3950.0, 0.0) - 10000.0 * exp(3950.0 * (1.0 / 273.15 - 1.0 / 298.15))) < 1e-6);
    CHECK(fude_zoom_circuit_ntc_ohms(10000.0, 3950.0, 100.0) < 700.0 && fude_zoom_circuit_ntc_ohms(10000.0, 3950.0, 100.0) > 690.0);
    CHECK(fabs(fude_zoom_circuit_sensor_along(FUDE_ZOOM_MODEL_LDR, 100.0) - 0.4) < 1e-12 && fabs(fude_zoom_circuit_sensor_at(FUDE_ZOOM_MODEL_LDR, 0.4) - 100.0) < 1e-9);
    CHECK(fabs(fude_zoom_circuit_sensor_along(FUDE_ZOOM_MODEL_THERMISTOR, 50.0) - 0.5) < 1e-12 && fabs(fude_zoom_circuit_sensor_at(FUDE_ZOOM_MODEL_THERMISTOR, 0.5) - 50.0) < 1e-9);
    for(u32 k = 0; k < 2u; k++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 sen = part_put(&s, k == 0u ? "LDR" : "thermistor", 0, 0, 30, 20, k == 0u ? "GL5528" : "10k NTC");
        resist(&s, sen, 0, 30.0, "10k", "5V");
        tie(&s, sen, 1, 30.0, 0.0, NULL);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        const u32 idx = cindex(&c, sen);
        for(u32 t = 0; t < 4u; t++) {
            CHECK(run_for(&c, 0.01));
            const fude_zoom_circuit_part* p = cpart(&c, sen);
            const f64 r = k == 0u ? fude_zoom_circuit_ldr_ohms(p->state[0]) : fude_zoom_circuit_ntc_ohms(10000.0, 3950.0, p->state[0]);
            const f64 want = 5.0 * r / (r + 10000.0), got = fude_zoom_circuit_volts(&c, p->node[0]);
            if(fabs(got - want) > 1e-3) printf("  %s at %.0f: %.4f V, not %.4f\n", k == 0u ? "LDR" : "NTC", p->state[0], got, want);
            CHECK(fabs(got - want) < 1e-3);
            const f64 was = p->state[0];
            c8 say[32];
            CHECK(fude_zoom_circuit_tap(&c, idx, -1, say, sizeof say) && say[0] != 0);
            CHECK(cpart(&c, sen)->state[0] != was);
        }
        CHECK(k == 0u ? cpart(&c, sen)->state[0] == 10000.0 : cpart(&c, sen)->state[0] == 25.0);   // (round its presets: three, four)
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    for(u32 k = 0; k < 2u; k++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const b8 dp = k == 1u;
        const u32 sw = part_put(&s, dp ? "DPDT switch" : "SPDT switch", 0, 0, 30, dp ? 60 : 30, "A");
        const u32 poles = dp ? 2u : 1u;
        u32 ra[2], rb[2];
        for(u32 pl = 0; pl < poles; pl++) {
            fude_zoom_v2 at;
            tie(&s, sw, 3u * pl, -30.0, 0.0, "5V");
            CHECK(fude_zoom_part_pin_at(&s, sw, 3u * pl + 1u, &at));
            ra[pl] = part_put(&s, "resistor", at.x + 50.0, at.y, 20, 7, "1k");
            wire_put(&s, sw, 3u * pl + 1u, ra[pl], 0);
            tie(&s, ra[pl], 1, 30.0, 0.0, NULL);
            CHECK(fude_zoom_part_pin_at(&s, sw, 3u * pl + 2u, &at));
            rb[pl] = part_put(&s, "resistor", at.x + 120.0, at.y, 20, 7, "1k");
            wire_put(&s, sw, 3u * pl + 2u, rb[pl], 0);
            tie(&s, rb[pl], 1, 30.0, 0.0, NULL);
        }
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        for(u32 t = 0; t < 2u; t++) {
            CHECK(run_for(&c, 0.01));
            for(u32 pl = 0; pl < poles; pl++) {
                const f64 ia = fabs(cpart(&c, ra[pl])->pin_i[0]), ib = fabs(cpart(&c, rb[pl])->pin_i[0]);
                CHECK(t == 0u ? (fabs(ia - 5e-3) < 1e-4 && ib < 1e-6) : (fabs(ib - 5e-3) < 1e-4 && ia < 1e-6));
            }
            c8 say[8];
            CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw), -1, say, sizeof say) && strcmp(say, t == 0u ? "B" : "A") == 0);
        }
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
}

// A SOLENOID (circuit.h), alone: its coil R and L in series (12 V: 24 Ω, 48 mH — 2 ms) — its current rising as
// 1 − e^(−t/τ), to its rated 0.5 A; its plunger in at its rated volts, not at 10 V (83%: short of the 87% it needs from
// all the way out), held in down to 8 V, out at 6 V; past its watts (24 V: 24 W of its 9) it heats. A STEPPER, its
// coils to ground through switches, COM on 5 V: a step on for each coil in turn, back for the one before, half a step
// for two, none for the opposite one, and no faster than its 500 steps a second; its shaft 2048 steps a turn. A ROTARY
// ENCODER's contacts in quadrature — a tap a detent clockwise: A low, both, B, neither; back, B first. A SLOTTED
// SENSOR: its LED on 5 V through 330 Ω (11.5 mA); its transistor pulling C low through 10k while the slot is clear, let
// go (5 V) while it is blocked.
static void src_across(fude_zoom_scene* s, u32 part, u32 pa, u32 pb, const c8* volts, u32* src) {
    fude_zoom_v2 at;
    CHECK(fude_zoom_part_pin_at(s, part, pa, &at));
    *src = part_put(s, "DC source", at.x - 120.0, at.y - 40.0, 20, 30, volts);
    wire_put(s, *src, 0, part, pa);
    wire_put(s, *src, 1, part, pb);
    tie(s, *src, 1, 0.0, -40.0, NULL);
}

static void test_mech_parts_alone(void) {
    printf("solenoid, stepper, encoder, slotted sensor (alone)\n");
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 so = part_put(&s, "solenoid", 0, 0, 50, 20, "12V");
        u32 src;
        src_across(&s, so, 0, 1, "12V", &src);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        const fude_zoom_circuit_part* p = cpart(&c, so);
        CHECK(fabs(p->value[1] - 24.0) < 1e-12 && fabs(p->value[2] - 0.048) < 1e-12);
        CHECK(fabs(fude_zoom_limits_typical(p->part, "12V", p->value).most[FUDE_ZOOM_LIMIT_POWER] - 9.0) < 1e-9);
        f64 worst = 0.0;
        for(u32 k = 1; k <= 40u; k++) {
            CHECK(fude_zoom_circuit_run(&c, 2.5e-4, 400u));
            const f64 t = 2.5e-4 * (f64)k, want = 0.5 * (1.0 - exp(-t / 0.002));
            worst = fmax(worst, fabs(cpart(&c, so)->state[0] - want));
        }
        printf("  12 V on its coil: its current within %.4f A of 0.5 (1 - e^(-t/2ms)) over 10 ms\n", worst);
        CHECK(worst < 0.01);
        CHECK(run_for(&c, 0.05));
        CHECK(fabs(cpart(&c, so)->state[0] - 0.5) < 1e-3 && cpart(&c, so)->state[2] == 1.0 && cpart(&c, so)->shown == 1.0);
        CHECK(fabs(fude_zoom_circuit_solenoid_pull(cpart(&c, so)) - 1.4) < 0.01);   // (all the way in at its rated current)
        // (held in at 8 V; out at 6 V; not in again at 10 V)
        const f64 volts[3] = { 8.0, 6.0, 10.0 }, in[3] = { 1.0, 0.0, 0.0 };
        for(u32 k = 0; k < 3u; k++) {
            cpart(&c, src)->value[0] = volts[k];
            CHECK(run_for(&c, 0.1));
            if(cpart(&c, so)->state[2] != in[k]) printf("  at %.0f V: its plunger %.2f in, not %.0f\n", volts[k], cpart(&c, so)->state[2], in[k]);
            CHECK(cpart(&c, so)->state[2] == in[k]);
        }
        cpart(&c, src)->value[0] = 24.0;
        CHECK(run_for(&c, 0.2));
        CHECK(cpart(&c, so)->state[2] == 1.0 && cpart(&c, so)->stress > 2.5 && cpart(&c, so)->heat > 0.0);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 st = part_put(&s, "stepper motor", 0, 0, 80, 60, "28BYJ-48");
        tie(&s, st, 4, -40.0, 0.0, "5V");
        u32 sw[4];
        for(u32 k = 0; k < 4u; k++) {
            fude_zoom_v2 at;
            CHECK(fude_zoom_part_pin_at(&s, st, k, &at));
            sw[k] = part_put(&s, "SPST switch", at.x - 200.0 - 70.0 * (f64)k, at.y, 20, 10, "off");
            wire_by(&s, st, k, sw[k], 1, NULL, 0);
            tie(&s, sw[k], 0, -10.0, -30.0 - 10.0 * (f64)k, NULL);
        }
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(cpart(&c, st)->value[0] == 2048.0 && cpart(&c, st)->state[7] == -1.0);
        c8 say[16];
        // (each switch tapped on — and the one before off —, given 20 ms: its steps after each)
        const u32 order[9] = { 0, 1, 2, 3, 0, 3, 2, 2, 0 };
        const f64 want[9]  = { 0, 1, 2, 3, 4, 3, 2, 2.5, 2.5 };   // (the 8th: D and C both on; the 9th: A alone, opposite C+D's… half a turn off: stays)
        u32 on = FUDE_ZOOM_NONE;
        for(u32 k = 0; k < 9u; k++) {
            if(k == 7u) {
                CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[3]), -1, say, sizeof say));   // (D on again with C: half a step back)
                on = 3u;
            } else if(k == 8u) {
                // (both off, then A alone: C+D's field was at 225°, A's at 0° — 135° the short way: on to 4… it goes)
                CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[2]), -1, say, sizeof say));
                CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[3]), -1, say, sizeof say));
                CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[0]), -1, say, sizeof say));
                on = 0u;
            } else {
                if(on != FUDE_ZOOM_NONE) CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[on]), -1, say, sizeof say));
                CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[order[k]]), -1, say, sizeof say));
                on = order[k];
            }
            CHECK(run_for(&c, 0.02));
            const f64 got = cpart(&c, st)->state[2], should = k == 8u ? 4.0 : want[k];
            if(fabs(got - should) > 1e-6) printf("  stepper, move %u: %.4f steps, not %.1f\n", k, got, should);
            CHECK(fabs(got - should) < 1e-6);
        }
        CHECK(fabs(cpart(&c, st)->state[1] - 4.0 * 360.0 / 2048.0) < 1e-9);   // (its shaft: 4 steps of 2048 a turn)
        f64 strength = 0.0;
        CHECK(fabs(fude_zoom_circuit_stepper_field(cpart(&c, st), &strength)) < 1e-9 && fabs(strength - 1.0) < 0.01);
        // (the opposite coil alone: C, half a turn of its field from A — it stays)
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[0]), -1, say, sizeof say));
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[2]), -1, say, sizeof say));
        CHECK(run_for(&c, 0.05));
        CHECK(fabs(cpart(&c, st)->state[2] - 4.0) < 1e-6);
        // (D then: a step back… in no less than 2 ms — its 500 steps a second)
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[2]), -1, say, sizeof say));
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[3]), -1, say, sizeof say));
        CHECK(fude_zoom_circuit_run(&c, 5e-4, 400u));
        const f64 early = cpart(&c, st)->state[2];
        CHECK(early < 4.0 && early >= 4.0 - 0.25 - 1e-9);
        CHECK(run_for(&c, 0.02) && fabs(cpart(&c, st)->state[2] - 3.0) < 1e-6);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    {
        b8 a, b;
        const f64 at[6] = { 0.0, 0.3, 0.6, 0.8, -0.2, 7.55 };
        const b8 wa[6] = { false, true, true, false, false, true }, wb[6] = { false, false, true, true, true, true };
        for(u32 k = 0; k < 6u; k++) {
            fude_zoom_circuit_encoder_contacts(at[k], &a, &b);
            CHECK(a == wa[k] && b == wb[k]);
        }
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 en = part_put(&s, "rotary encoder", 0, 0, 40, 40, "20");
        tie(&s, en, 1, -40.0, -20.0, NULL);
        resist(&s, en, 0, 60.0, "10k", "5V");
        resist(&s, en, 2, 60.0, "10k", "5V");
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        for(u32 way = 0; way < 2u; way++) {
            c8 say[16];
            if(way == 0u) {
                CHECK(fude_zoom_circuit_tap(&c, cindex(&c, en), -1, say, sizeof say) && strcmp(say, "1") == 0);
            } else {
                cpart(&c, en)->state[1] = 0.0;   // (back a detent)
            }
            u32 seen[8], n = 0, last = 99u;
            for(u32 t = 0; t < 80u; t++) {
                CHECK(fude_zoom_circuit_run(&c, 1e-3, 400u));
                const fude_zoom_circuit_part* p = cpart(&c, en);
                const u32 now = (fude_zoom_circuit_volts(&c, p->node[0]) > 2.5 ? 2u : 0u) | (fude_zoom_circuit_volts(&c, p->node[2]) > 2.5 ? 1u : 0u);
                if(now != last && n < 8u) seen[n++] = now;
                last = now;
            }
            // (A high and B high: 3; A low: 1; both low: 0; B low: 2 — clockwise 3 1 0 2 3, back 3 2 0 1 3)
            const u32 cw[5] = { 3, 1, 0, 2, 3 }, ccw[5] = { 3, 2, 0, 1, 3 };
            CHECK(n == 5u);
            for(u32 k = 0; k < 5u && k < n; k++) CHECK(seen[k] == (way == 0u ? cw[k] : ccw[k]));
            CHECK(fabs(cpart(&c, en)->state[0] - (way == 0u ? 1.0 : 0.0)) < 1e-9);
        }
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 sl = part_put(&s, "slotted sensor", 0, 0, 40, 30, "ITR9608");
        resist(&s, sl, 0, 60.0, "330", "5V");
        tie(&s, sl, 1, -30.0, -20.0, NULL);
        fude_zoom_v2 at;
        CHECK(fude_zoom_part_pin_at(&s, sl, 2, &at));
        const u32 pu = part_put(&s, "resistor", at.x + 80.0, at.y, 20, 7, "10k");
        wire_put(&s, sl, 2, pu, 0);
        tie(&s, pu, 1, 30.0, 0.0, "5V");
        tie(&s, sl, 3, 30.0, -20.0, NULL);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(fabs(cpart(&c, sl)->limits.most[FUDE_ZOOM_LIMIT_CURRENT] - 0.05) < 1e-12);
        for(u32 k = 0; k < 3u; k++) {
            CHECK(run_for(&c, 0.02));
            const fude_zoom_circuit_part* p = cpart(&c, sl);
            const f64 vc = fude_zoom_circuit_volts(&c, p->node[2]);
            printf("  slotted sensor %s: its LED %.2f mA, C at %.3f V\n", p->state[0] > 0.5 ? "blocked" : "clear", p->pin_i[0] * 1e3, vc);
            CHECK(fabs(p->pin_i[0] - 0.0115) < 0.001);
            CHECK(k == 1u ? vc > 4.95 : vc < 0.3);
            c8 say[8];
            CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sl), -1, say, sizeof say));
        }
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // A coil's current broken (circuit.h's FUDE_ZOOM_ARC): a switch opened on the solenoid with no diode arcs at 50 V —
    // said once, its spark's time kept —, the current gone in a millisecond, solved all along; a diode across the coil
    // (its cathode on the plus) carries it instead — 13 V over the switch at most, the current dying away in its 2 ms,
    // no spark. A 2N2222 (saturated: 9 mA into its base) switching it off with no diode breaks down at 50 V (a quarter
    // past its 40) and burns (25 W of its ½ W); with the diode, 12.7 V over it, nothing burnt.
    for(u32 k = 0; k < 4u; k++) {
        const b8 diode = (k & 1u) != 0u, npn = k >= 2u;
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 so = part_put(&s, "solenoid", 0, 0, 50, 20, "12V");
        tie(&s, so, 0, -40.0, 30.0, "12V");
        u32 sw = FUDE_ZOOM_NONE, q = FUDE_ZOOM_NONE;
        fude_zoom_v2 at;
        CHECK(fude_zoom_part_pin_at(&s, so, 1, &at));
        if(npn) {
            q = part_put(&s, "NPN", at.x + 60.0, at.y - 80.0, 30, 30, "2N2222");
            wire_put(&s, so, 1, q, 1);
            tie(&s, q, 2, 0.0, -30.0, NULL);
            fude_zoom_v2 bp;
            CHECK(fude_zoom_part_pin_at(&s, q, 0, &bp));
            const u32 rb = part_put(&s, "resistor", bp.x - 60.0, bp.y, 20, 7, "470");   // (9 mA: saturated)
            wire_put(&s, rb, 1, q, 0);
            sw = part_put(&s, "SPST switch", bp.x - 160.0, bp.y, 20, 10, "on");
            wire_put(&s, sw, 1, rb, 0);
            tie(&s, sw, 0, -30.0, 0.0, "5V");
        } else {
            sw = part_put(&s, "SPST switch", at.x + 80.0, at.y - 80.0, 20, 10, "on");
            wire_put(&s, so, 1, sw, 0);
            tie(&s, sw, 1, 30.0, -20.0, NULL);
        }
        if(diode) {
            const u32 d = part_put(&s, "diode", 0, 80, 20, 10, "1N4007");
            wire_put(&s, d, 0, so, 1);   // (its anode on the coil's low end, its cathode on its plus)
            wire_put(&s, d, 1, so, 0);
        }
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.05) && fabs(cpart(&c, so)->state[0] - 0.5) < 0.01 && (!npn || !cpart(&c, q)->burnt));
        rde_arr_clear(&c.events);
        c8 say[16];
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw), -1, say, sizeof say));
        f64 peak = 0.0, at1ms = 0.0;
        b8 solved = true;
        for(u32 t = 1; t <= 40u; t++) {
            solved = solved && fude_zoom_circuit_run(&c, 1e-4, 400u);
            const fude_zoom_circuit_part* p = npn ? cpart(&c, q) : cpart(&c, sw);
            const f64 v = npn ? fude_zoom_circuit_volts(&c, p->node[1]) - fude_zoom_circuit_volts(&c, p->node[2]) :
                                fude_zoom_circuit_volts(&c, p->node[0]) - fude_zoom_circuit_volts(&c, p->node[1]);
            peak = fmax(peak, fabs(v));
            if(t == 10u) at1ms = cpart(&c, so)->state[0];
        }
        u32 arcs = 0, burns = 0;
        for(u32 e = 0; e < (u32)rde_arr_length(&c.events); e++) {
            arcs  += ((const fude_zoom_circuit_event*)c.events.memory)[e].kind == FUDE_ZOOM_CIRCUIT_ARC ? 1u : 0u;
            burns += ((const fude_zoom_circuit_event*)c.events.memory)[e].kind == FUDE_ZOOM_CIRCUIT_BURNT ? 1u : 0u;
        }
        printf("  broken by %s%s: %.2f V over it at most, %.3f A through the coil after 1 ms, %u sparks, %u burnt\n", npn ? "a 2N2222" : "a switch",
               diode ? " (a diode across the coil)" : "", peak, at1ms, arcs, burns);
        CHECK(solved);
        if(!diode) {
            CHECK(peak > 49.0 && peak < 51.5);
            CHECK(fabs(at1ms) < 1e-3);
            CHECK(npn ? (burns == 1u && cpart(&c, q)->burnt && arcs == 0u) : (arcs == 1u && fude_zoom_circuit_sparked(cpart(&c, sw)) > 0.05));   // (burnt, it flashes over at 50 V)
        } else {
            CHECK(peak < 13.5 && at1ms > 0.15 && arcs == 0u && burns == 0u);
            CHECK(fude_zoom_circuit_sparked(cpart(&c, sw)) == 0.0 && (!npn || !cpart(&c, q)->burnt));
        }
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // Their limits: a solenoid's watts, a stepper's coils' current, an encoder's contacts', a slotted sensor's LED's.
    CHECK(fude_zoom_limits_kinds(fude_zoom_part_find("solenoid")) == (1u << FUDE_ZOOM_LIMIT_POWER));
    CHECK(fude_zoom_limits_kinds(fude_zoom_part_find("stepper motor")) == (1u << FUDE_ZOOM_LIMIT_CURRENT));
    CHECK(fude_zoom_limits_kinds(fude_zoom_part_find("rotary encoder")) == (1u << FUDE_ZOOM_LIMIT_CURRENT));
    CHECK(fude_zoom_limits_kinds(fude_zoom_part_find("slotted sensor")) == ((1u << FUDE_ZOOM_LIMIT_CURRENT) | (1u << FUDE_ZOOM_LIMIT_REVERSE)));
    CHECK(fude_zoom_limits_named(fude_zoom_part_find("stepper motor"), "28BYJ-48") == 0 && fude_zoom_limits_named(fude_zoom_part_find("slotted sensor"), "H21A1") == 1);
}

// Batch 0.1.75's parts (circuit.h). A JK flip-flop: J and K 1 toggles each rising clock, J alone sets, K alone resets,
// neither holds; an SR latch: S sets, R resets, neither holds. A CD4017 on the canvas: a step on each clock, Q0 first.
// An LM393: its output let go (pulled up to 5 V) while IN− is under IN+, sinking under 0.3 V while it is over;
// unpowered, let go. An LM358 as a follower: 2 V in, 2 V out; 4.5 V in, 3.45 V out (its output 1.5 V under VCC at most,
// less 50 mV). An SCR: off until its gate fires it, on after its gate lets go, off once its anode's current stops — and
// off when it comes back. A TRIAC on AC fired by a resistor from MT2: both ways; without it, nothing. A 2:1
// transformer: 24 V of AC in, 12 V out (a 1k load), its primary's current half its secondary's and its magnetising
// current. A bridge: both halves of the AC out of +, two diodes' drops less, never backwards; a capacitor on it,
// steady. An optocoupler: its LED lit, C pulled low; dark, C high. A photodiode's current 50 nA a lux, a
// phototransistor's 2 µA. A speaker's sound: a sample each 1/48000 s of the circuit's time, a 440 Hz tone's crossings,
// half full scale at 50 mA.
static u32 rail_at(fude_zoom_scene* s, f64 x, f64 y, const c8* volts) { return part_put(s, "supply rail", x, y, 30, 10, volts); }

static f64 swing_of(fude_zoom_circuit* c, f64 seconds, u32 obj, u32 pa, u32 pb, f64* lo, f64* hi) {
    *lo = 1e300; *hi = -1e300;
    for(f64 t = 0.0; t < seconds - 1e-9; t += 2e-4) {
        CHECK(fude_zoom_circuit_run(c, 2e-4, 400u));
        const fude_zoom_circuit_part* p = cpart(c, obj);
        const f64 v = fude_zoom_circuit_volts(c, p->node[pa]) - (pb == FUDE_ZOOM_NONE ? 0.0 : fude_zoom_circuit_volts(c, p->node[pb]));
        *lo = fmin(*lo, v); *hi = fmax(*hi, v);
    }
    return *hi - *lo;
}

static void test_batch5_parts(void) {
    printf("flip-flops, CD4017, comparator, thyristors, transformer, bridge, light, speaker\n");
    {
        // An island ground reaches nowhere in a block it reaches (across an optocoupler; a transformer's): held at 0 V
        // at its source's minus, solved as any — the optocoupler's other side's LED lit by what its own LED's current
        // lets through (its 9 V's minus, ground or not, the same), the transformer's floating secondary
        // at half its primary.
        for(u32 k = 0; k < 3u; k++) {
            fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
            fude_zoom_circuit c; fude_zoom_circuit_init(&c);
            if(k < 2u) {
                const u32 op = part_put(&s, "optocoupler", 0, 0, 50, 30, "PC817");
                const u32 b1 = part_put(&s, "battery", -200, 0, 20, 30, "5V"), r1 = part_put(&s, "resistor", -100, 60, 30, 10, "470");
                wire_put(&s, b1, 0, r1, 0); wire_put(&s, r1, 1, op, 0); wire_put(&s, op, 1, b1, 1);
                tie(&s, b1, 1, 0.0, -30.0, NULL);
                const u32 b2 = part_put(&s, "battery", 200, 0, 20, 30, "9V"), r2 = part_put(&s, "resistor", 200, 100, 30, 10, "1k");
                const u32 led = part_put(&s, "LED", 100, 100, 30, 10, "green");
                wire_put(&s, b2, 0, r2, 1); wire_put(&s, r2, 0, led, 0); wire_put(&s, led, 1, op, 2); wire_put(&s, op, 3, b2, 1);
                if(k == 1u) tie(&s, b2, 1, 0.0, -30.0, NULL);
                fude_zoom_circuit_build(&c, &s);
                const u32 refs = (u32)rde_arr_length(&c.block_reference);
                CHECK(refs == (k == 0u ? 1u : 0u));
                if(refs == 1u) CHECK(((const u32*)c.block_reference.memory)[0] == circuit_part(&c, b2)->node[1]);
                CHECK(run_for(&c, 0.1));
                const f64 i = circuit_part(&c, led)->pin_i[0];
                printf("  an optocoupler's other side %s: %.2f mA, its battery's minus at %.3f V\n", k == 0u ? "apart" : "grounded", i * 1e3, circuit_volts(&c, b2, 1));
                CHECK(i > 5e-3 && i < 7.5e-3 && fabs(circuit_volts(&c, b2, 1)) < 1e-6);
            } else {
                const u32 tx = part_put(&s, "transformer", 0, 0, 40, 40, "2:1");
                const u32 ac = part_put(&s, "AC source", -150, 0, 20, 30, "10V 50Hz"), rl = part_put(&s, "resistor", 120, 0, 10, 30, "1k");
                wire_put(&s, ac, 0, tx, 0); wire_put(&s, ac, 1, tx, 1);
                tie(&s, ac, 1, 0.0, -30.0, NULL);
                wire_put(&s, tx, 2, rl, 0); wire_put(&s, tx, 3, rl, 1);
                fude_zoom_circuit_build(&c, &s);
                CHECK(rde_arr_length(&c.block_reference) == 1u);
                CHECK(run_for(&c, 0.2));
                f64 p = 0.0, q = 0.0;
                for(u32 n = 0; n < 40u; n++) {
                    CHECK(fude_zoom_circuit_run(&c, 0.0005, 40u));
                    p = fmax(p, fabs(circuit_volts(&c, tx, 0) - circuit_volts(&c, tx, 1)));
                    q = fmax(q, fabs(circuit_volts(&c, tx, 2) - circuit_volts(&c, tx, 3)));
                }
                printf("  a transformer's floating secondary: %.2f V of its primary's %.2f\n", q, p);
                CHECK(p > 9.0 && q / p > 0.45 && q / p < 0.51);
            }
            fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
        }
    }
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 jk = part_put(&s, "JK flip-flop", 0, 0, 40, 40, "");
        const u32 in[3] = { part_put(&s, "logic input", -150, 20, 30, 8, "1"), part_put(&s, "logic input", -150, 0, 30, 8, "0"), part_put(&s, "logic input", -150, -20, 30, 8, "1") };
        for(u32 k = 0; k < 3u; k++) wire_put(&s, in[k], 0, jk, k);
        const u32 pr = part_put(&s, "logic probe", 100, 20, 20, 8, "");
        wire_put(&s, jk, 3, pr, 0);
        const u32 sr = part_put(&s, "SR latch", 0, -200, 40, 40, "");
        const u32 si = part_put(&s, "logic input", -150, -180, 30, 8, "0"), ri = part_put(&s, "logic input", -150, -220, 30, 8, "0");
        wire_put(&s, si, 0, sr, 0);
        wire_put(&s, ri, 0, sr, 1);
        const u32 ps = part_put(&s, "logic probe", 100, -180, 20, 8, "");
        wire_put(&s, sr, 2, ps, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(step(&c));
        // (J K: 11 toggles, 10 sets, 01 resets, 00 holds — each after a rising clock)
        const u32 jks[6][2] = { { 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 0 }, { 0, 1 }, { 0, 0 } };
        u32 q = probe(&c, pr), right = 0;
        for(u32 k = 0; k < 6u; k++) {
            set_in(&c, in[0], jks[k][0]); set_in(&c, in[2], jks[k][1]);
            CHECK(step(&c));
            set_in(&c, in[1], 1u); CHECK(step(&c)); CHECK(step(&c));
            set_in(&c, in[1], 0u); CHECK(step(&c));
            const u32 want = jks[k][0] && jks[k][1] ? 1u - q : (jks[k][0] ? 1u : (jks[k][1] ? 0u : q));
            right += probe(&c, pr) == want;
            q = probe(&c, pr);
        }
        CHECK(right == 6u);
        const u32 srs[5][3] = { { 1, 0, 1 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 0, 0 }, { 1, 0, 1 } };   // (S, R, Q after)
        right = 0;
        for(u32 k = 0; k < 5u; k++) {
            set_in(&c, si, srs[k][0]); set_in(&c, ri, srs[k][1]);
            CHECK(step(&c)); CHECK(step(&c));
            right += probe(&c, ps) == srs[k][2];
        }
        CHECK(right == 5u);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 ch = part_put(&s, "CD4017", 0, 0, 100, 90, "CD4017");
        tie(&s, ch, 15, 30.0, 0.0, "5V");
        tie(&s, ch, 7, -30.0, 0.0, NULL);
        tie(&s, ch, 12, 30.0, -10.0, NULL);   // (INH)
        tie(&s, ch, 14, 30.0, -10.0, NULL);   // (RST)
        fude_zoom_v2 at;
        CHECK(fude_zoom_part_pin_at(&s, ch, 13, &at));
        const u32 ck = part_put(&s, "logic input", at.x + 200.0, at.y, 30, 8, "0");
        wire_put(&s, ck, 0, ch, 13);
        const u32 outs[5] = { 2, 1, 3, 6, 9 };   // (Q0, Q1, Q2, Q3, Q4)
        for(u32 k = 0; k < 5u; k++) resist(&s, ch, outs[k], 30.0, "100k", NULL);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(step(&c));
        u32 right = 0;
        for(u32 n = 0; n < 7u; n++) {
            u32 hot = 0, which = 99u;
            for(u32 k = 0; k < 5u; k++) if(fude_zoom_circuit_volts(&c, cpart(&c, ch)->node[outs[k]]) > 2.5) { hot++; which = k; }
            const b8 ok = n < 5u ? (hot == 1u && which == n) : hot == 0u;
            if(!ok) printf("  CD4017 after %u clocks: %u of Q0-Q4 high (Q%u)\n", n, hot, which);
            right += ok;
            set_in(&c, ck, 1u); CHECK(step(&c)); CHECK(step(&c));
            set_in(&c, ck, 0u); CHECK(step(&c));
        }
        CHECK(right == 7u);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    for(u32 k = 0; k < 3u; k++) {
        // (LM393: OUT1 IN1− IN1+ GND down its left, VCC at its right's top; IN1+ at 2.5 V, IN1− at 2 or
        // 3 V; k 2: unpowered)
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 cmp = part_put(&s, "LM393", 0, 0, 100, 50, "LM393");
        if(k < 2u) tie(&s, cmp, 7, 30.0, 0.0, "5V");
        tie(&s, cmp, 3, -40.0, -20.0, NULL);
        resist(&s, cmp, 2, 30.0, "1k", "2.5V");
        resist(&s, cmp, 1, 120.0, "1k", k == 1u ? "3V" : "2V");
        resist(&s, cmp, 0, 210.0, "10k", "5V");
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.01));
        const f64 out = fude_zoom_circuit_volts(&c, cpart(&c, cmp)->node[0]);
        printf("  LM393, IN- %s IN+%s: OUT1 %.3f V\n", k == 1u ? "over" : "under", k == 2u ? " (no supply)" : "", out);
        CHECK(k == 1u ? out < 0.3 : out > 4.9);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    for(u32 k = 0; k < 2u; k++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 amp = part_put(&s, "LM358", 0, 0, 100, 50, "LM358");
        tie(&s, amp, 7, 30.0, 0.0, "5V");
        tie(&s, amp, 3, -40.0, -20.0, NULL);
        wire_put(&s, amp, 0, amp, 1);   // (a follower: OUT1 into IN1−)
        resist(&s, amp, 2, 30.0, "1k", k == 0u ? "2V" : "4.5V");
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.01));
        const f64 out = fude_zoom_circuit_volts(&c, cpart(&c, amp)->node[0]);
        CHECK(fabs(out - (k == 0u ? 2.0 : 3.45)) < 0.01);   // (3.5 less its 50 mV)
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    {
        // (SCR: 12 V through 100 Ω into A, K to ground; its gate from a 5 V rail through 1k, the rail switched)
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 scr = part_put(&s, "SCR", 0, 0, 30, 20, "C106");
        fude_zoom_v2 a, g;
        CHECK(fude_zoom_part_pin_at(&s, scr, 0, &a) && fude_zoom_part_pin_at(&s, scr, 2, &g));
        const u32 ra = part_put(&s, "resistor", a.x - 60.0, a.y, 20, 7, "100"), sup = rail_at(&s, a.x - 120.0, a.y + 30.0, "12V");
        wire_put(&s, ra, 1, scr, 0);
        wire_by(&s, sup, 0, ra, 0, (const fude_zoom_v2[]){ { a.x - 120.0, a.y } }, 1u);
        tie(&s, scr, 1, 30.0, -20.0, NULL);
        const u32 rg = part_put(&s, "resistor", g.x, g.y - 60.0, 20, 7, "1k"), gate = rail_at(&s, g.x + 80.0, g.y - 30.0, "0V");
        wire_by(&s, scr, 2, rg, 1, (const fude_zoom_v2[]){ { g.x + 40.0, g.y - 20.0 }, { g.x + 40.0, g.y - 60.0 } }, 2u);
        wire_by(&s, rg, 0, gate, 0, (const fude_zoom_v2[]){ { g.x - 40.0, g.y - 60.0 }, { g.x - 40.0, g.y - 100.0 }, { g.x + 80.0, g.y - 100.0 } }, 3u);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.02));
        const f64 off = cpart(&c, scr)->pin_i[0];
        cpart(&c, gate)->value[0] = 5.0;   // (its gate fired)
        CHECK(run_for(&c, 0.02));
        const f64 on = cpart(&c, scr)->pin_i[0];
        cpart(&c, gate)->value[0] = 0.0;   // (let go: it stays on)
        CHECK(run_for(&c, 0.02));
        const f64 kept = cpart(&c, scr)->pin_i[0];
        cpart(&c, sup)->value[0] = 0.0;    // (its anode's current stopped: off)
        CHECK(run_for(&c, 0.02));
        cpart(&c, sup)->value[0] = 12.0;
        CHECK(run_for(&c, 0.02));
        const f64 again = cpart(&c, scr)->pin_i[0];
        printf("  SCR: %.4f A off, %.4f A fired, %.4f A after its gate let go, %.4f A after its supply came back\n", off, on, kept, again);
        CHECK(fabs(off) < 1e-4 && on > 0.1 && fabs(kept - on) < 1e-3 && fabs(again) < 1e-4);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    for(u32 k = 0; k < 2u; k++) {
        // (TRIAC: 12 V of AC through 100 Ω into MT2, MT1 to ground; fired through 1k from MT2 — or not)
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 tr = part_put(&s, "TRIAC", 0, 0, 30, 20, "BT136");
        fude_zoom_v2 a, g;
        CHECK(fude_zoom_part_pin_at(&s, tr, 0, &a) && fude_zoom_part_pin_at(&s, tr, 2, &g));
        const u32 ac = part_put(&s, "AC source", a.x - 160.0, a.y - 40.0, 20, 30, "12V 50Hz");
        const u32 rl = part_put(&s, "resistor", a.x - 60.0, a.y, 20, 7, "100");
        wire_put(&s, rl, 1, tr, 0);
        wire_by(&s, ac, 0, rl, 0, (const fude_zoom_v2[]){ { a.x - 160.0, a.y } }, 1u);
        tie(&s, ac, 1, 0.0, -30.0, NULL);
        tie(&s, tr, 1, 30.0, -20.0, NULL);
        if(k == 0u) {
            const u32 rg = part_put(&s, "resistor", a.x - 10.0, g.y - 40.0, 20, 7, "1k");
            wire_by(&s, tr, 2, rg, 1, (const fude_zoom_v2[]){ { g.x, g.y - 40.0 } }, 1u);
            wire_by(&s, rg, 0, tr, 0, (const fude_zoom_v2[]){ { a.x - 40.0, g.y - 40.0 }, { a.x - 40.0, a.y - 5.0 }, { a.x - 5.0, a.y - 5.0 } }, 3u);
        }
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        f64 lo = 1e300, hi = -1e300;
        for(f64 t = 0.0; t < 0.1; t += 2e-4) {
            CHECK(fude_zoom_circuit_run(&c, 2e-4, 400u));
            lo = fmin(lo, cpart(&c, rl)->pin_i[0]); hi = fmax(hi, cpart(&c, rl)->pin_i[0]);
        }
        printf("  TRIAC on AC%s: its load's current %.4f to %.4f A\n", k == 0u ? ", fired from MT2" : ", its gate unjoined", lo, hi);
        CHECK(k == 0u ? (hi > 0.08 && lo < -0.08) : (hi < 1e-3 && lo > -1e-3));
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    {
        // (a 2:1 transformer: 24 V of AC across its primary, 1k on its secondary — a 100 Ω one, ¼ W, would burn)
        const u32 k = 0u;
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 tx = part_put(&s, "transformer", 0, 0, 40, 40, "2:1");
        const u32 src = part_put(&s, k == 0u ? "AC source" : "DC source", -150, 0, 20, 30, k == 0u ? "24V 50Hz" : "5V");
        wire_put(&s, src, 0, tx, 0);
        wire_put(&s, src, 1, tx, 1);
        tie(&s, tx, 1, -20.0, -30.0, NULL);
        const u32 ld = part_put(&s, "resistor", 120, 0, 20, 7, "1k");
        wire_by(&s, tx, 2, ld, 0, (const fude_zoom_v2[]){ { 60.0, 20.0 }, { 60.0, 0.0 } }, 2u);
        wire_by(&s, tx, 3, ld, 1, (const fude_zoom_v2[]){ { 70.0, -20.0 }, { 70.0, -40.0 }, { 170.0, -40.0 }, { 170.0, 0.0 } }, 4u);
        tie(&s, tx, 3, 20.0, -50.0, NULL);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.2));
        f64 lo, hi;
        swing_of(&c, 0.04, tx, 2, 3, &lo, &hi);
        f64 ip = 0.0, is = 0.0;
        for(f64 t = 0.0; t < 0.02; t += 2e-4) {
            CHECK(fude_zoom_circuit_run(&c, 2e-4, 400u));
            ip = fmax(ip, fabs(cpart(&c, tx)->pin_i[0])); is = fmax(is, fabs(cpart(&c, tx)->pin_i[2]));
        }
        printf("  transformer 2:1 on %s: its secondary %.3f to %.3f V; its currents %.4f A in, %.4f A out\n", k == 0u ? "24 V AC" : "5 V DC", lo, hi, ip, is);
        CHECK(hi > 11.6 && hi < 12.1 && lo < -11.6 && lo > -12.1);
        CHECK(ip > 0.5 * is && ip < 0.5 * is + 0.03);   // (and its magnetising current, a quarter turn behind)
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    for(u32 k = 0; k < 2u; k++) {
        // (a bridge: 12 V of AC on AC1 and AC2, 1k from + to −, − the ground; k 1, 1000 µF across too)
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 br = part_put(&s, "bridge rectifier", 0, 0, 40, 40, "DB107");
        const u32 ac = part_put(&s, "AC source", -150, 0, 20, 30, "12V 50Hz");
        wire_by(&s, ac, 0, br, 0, (const fude_zoom_v2[]){ { -150.0, 60.0 }, { 0.0, 60.0 } }, 2u);
        wire_by(&s, ac, 1, br, 2, (const fude_zoom_v2[]){ { -150.0, -60.0 }, { 0.0, -60.0 } }, 2u);
        const u32 ld = part_put(&s, "resistor", 120, 80, 20, 7, "1k");
        wire_by(&s, br, 1, ld, 1, (const fude_zoom_v2[]){ { 160.0, 0.0 }, { 160.0, 80.0 } }, 2u);
        wire_by(&s, ld, 0, br, 3, (const fude_zoom_v2[]){ { 80.0, 80.0 }, { 80.0, 110.0 }, { -70.0, 110.0 }, { -70.0, 0.0 } }, 4u);
        tie(&s, br, 3, -40.0, -30.0, NULL);
        if(k == 1u) {
            const u32 cp = part_put(&s, "electrolytic", 200, -80, 20, 10, "1000uF");
            wire_by(&s, br, 1, cp, 0, (const fude_zoom_v2[]){ { 140.0, 0.0 }, { 140.0, -80.0 } }, 2u);
            wire_by(&s, cp, 1, br, 3, (const fude_zoom_v2[]){ { 240.0, -80.0 }, { 240.0, -130.0 }, { -90.0, -130.0 }, { -90.0, 0.0 } }, 4u);
        }
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.3));
        f64 lo, hi;
        swing_of(&c, 0.04, br, 1, 3, &lo, &hi);
        printf("  bridge rectifier%s: + over - %.3f to %.3f V\n", k == 1u ? " with 1000 uF" : "", lo, hi);
        CHECK(hi > 10.2 && hi < 11.0 && (k == 0u ? lo > -0.05 && lo < 0.05 : lo > hi - 0.2));
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 op = part_put(&s, "optocoupler", 0, 0, 50, 30, "PC817");
        fude_zoom_v2 a;
        CHECK(fude_zoom_part_pin_at(&s, op, 0, &a));
        const u32 rl = part_put(&s, "resistor", a.x - 60.0, a.y, 20, 7, "1k"), led = rail_at(&s, a.x - 120.0, a.y + 30.0, "5V");
        wire_put(&s, rl, 1, op, 0);
        wire_by(&s, led, 0, rl, 0, (const fude_zoom_v2[]){ { a.x - 120.0, a.y } }, 1u);
        tie(&s, op, 1, -30.0, -20.0, NULL);
        fude_zoom_v2 cc;
        CHECK(fude_zoom_part_pin_at(&s, op, 2, &cc));
        const u32 pu = part_put(&s, "resistor", cc.x + 60.0, cc.y, 20, 7, "10k");
        wire_put(&s, op, 2, pu, 0);
        tie(&s, pu, 1, 30.0, 0.0, "5V");
        tie(&s, op, 3, 30.0, -20.0, NULL);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.01));
        const f64 lit = fude_zoom_circuit_volts(&c, cpart(&c, op)->node[2]);
        cpart(&c, led)->value[0] = 0.0;
        CHECK(run_for(&c, 0.01));
        const f64 dark = fude_zoom_circuit_volts(&c, cpart(&c, op)->node[2]);
        printf("  optocoupler: C at %.3f V lit, %.3f V dark\n", lit, dark);
        CHECK(lit < 0.3 && dark > 4.95);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    for(u32 k = 0; k < 2u; k++) {
        // (a photodiode backwards on 5 V through 10k; a phototransistor on 5 V through 1k)
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const b8 diode = k == 0u;
        const u32 ph = part_put(&s, diode ? "photodiode" : "phototransistor", 0, 0, 30, diode ? 20 : 30, diode ? "BPW34" : "TEPT5600");
        const u32 top = diode ? 1u : 0u, bottom = diode ? 0u : 1u;
        fude_zoom_v2 at;
        CHECK(fude_zoom_part_pin_at(&s, ph, top, &at));
        const u32 r = part_put(&s, "resistor", at.x + 80.0, at.y + 40.0, 20, 7, diode ? "10k" : "1k");
        wire_by(&s, ph, top, r, 0, (const fude_zoom_v2[]){ { at.x, at.y + 40.0 } }, 1u);
        tie(&s, r, 1, 30.0, 0.0, "5V");
        tie(&s, ph, bottom, diode ? -30.0 : 0.0, diode ? -20.0 : -30.0, NULL);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        const f64 lux[2] = { 100.0, 1000.0 };
        for(u32 l = 0; l < 2u; l++) {
            cpart(&c, ph)->state[0] = lux[l];
            CHECK(run_for(&c, 0.01));
            const f64 i = fabs(cpart(&c, r)->pin_i[0]), want = (diode ? FUDE_ZOOM_PHOTODIODE_A_LUX : FUDE_ZOOM_PHOTOTRANSISTOR_A_LUX) * lux[l];
            if(fabs(i - want) > 0.03 * want) printf("  %s at %.0f lux: %.3g A, not %.3g\n", diode ? "photodiode" : "phototransistor", lux[l], i, want);
            CHECK(fabs(i - want) < 0.03 * want);
        }
        CHECK(fude_zoom_circuit_lit(cpart(&c, ph)->part->model));
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    for(u32 k = 0; k < 2u; k++) {
        // (a speaker on 0.4 V of 440 Hz — 50 mA —; k 1: a resistor instead — no sound)
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 sp = part_put(&s, k == 0u ? "speaker" : "resistor", 0, 0, 30, 20, k == 0u ? "8 ohm" : "8");
        const u32 ac = part_put(&s, "AC source", -150, 0, 20, 30, "0.4V 440Hz");
        wire_put(&s, ac, 0, sp, 0);
        wire_put(&s, ac, 1, sp, 1);
        tie(&s, ac, 1, 0.0, -30.0, NULL);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        fude_zoom_circuit_sound(&c, 48000.0);
        CHECK(run_for(&c, 0.2));
        const u32 n = (u32)rde_arr_length(&c.audio);
        if(k == 1u) {
            CHECK(n == 0u);
        } else {
            const f32* a = (const f32*)c.audio.memory;
            u32 cross = 0;
            f64 peak = 0.0;
            for(u32 i = n / 2u + 1u; i < n; i++) {
                cross += (a[i - 1u] < 0.0f) != (a[i] < 0.0f) ? 1u : 0u;
                peak = fmax(peak, fabs((f64)a[i]));
            }
            const f64 span = (f64)(n - n / 2u - 1u) / 48000.0;
            printf("  speaker: %u samples in 0.2 s, %u crossings in %.3f s (%.1f Hz), its peak %.3f\n", n, cross, span, (f64)cross / (2.0 * span), peak);
            CHECK(n >= 9590u && n <= 9610u);
            CHECK(fabs((f64)cross / (2.0 * span) - 440.0) < 10.0 && fabs(peak - 0.5) < 0.05);
        }
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
}

// JUMPS (circuit.h's be_next and step_now): a jump anywhere makes the next step backward Euler and an eighth of the
// one it was in (1 µs at least). Within a step, two ways: a TRIAC on AC fired through its gate and let go at each
// crossing (its own step after the solve jumps: fzc_after), and a 100 Hz clock into a NOT gate driving 1k into 1 µF
// (the logic settling within the step). On every step taken by the trapezoidal rule in which one happened, the next is
// planned at an eighth of it, and taken by backward Euler, no longer. Not again and again, though: a latch of two NOR
// gates with both inputs low and both outputs on a load runs as an oscillation (an edge in every step, so every step
// backward Euler), and still steps at its longest, a millisecond, as before; cut at each edge, its steps would shrink
// to 1 µs and Play to a crawl.
static void test_circuit_jumps(void) {
    printf("jumps within a step\n");
    for(u32 way = 0; way < 2u; way++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        u32 watch;   // (what jumps: the TRIAC, or the gate whose output follows the clock)
        if(way == 0u) {
            const u32 tr = part_put(&s, "TRIAC", 0, 0, 30, 20, "BT136");
            fude_zoom_v2 a, g;
            CHECK(fude_zoom_part_pin_at(&s, tr, 0, &a) && fude_zoom_part_pin_at(&s, tr, 2, &g));
            const u32 ac = part_put(&s, "AC source", a.x - 160.0, a.y - 40.0, 20, 30, "12V 50Hz");
            const u32 rl = part_put(&s, "resistor", a.x - 60.0, a.y, 20, 7, "100");
            wire_put(&s, rl, 1, tr, 0);
            wire_by(&s, ac, 0, rl, 0, (const fude_zoom_v2[]){ { a.x - 160.0, a.y } }, 1u);
            tie(&s, ac, 1, 0.0, -30.0, NULL);
            tie(&s, tr, 1, 30.0, -20.0, NULL);
            const u32 rg = part_put(&s, "resistor", a.x - 10.0, g.y - 40.0, 20, 7, "1k");
            wire_by(&s, tr, 2, rg, 1, (const fude_zoom_v2[]){ { g.x, g.y - 40.0 } }, 1u);
            wire_by(&s, rg, 0, tr, 0, (const fude_zoom_v2[]){ { a.x - 40.0, g.y - 40.0 }, { a.x - 40.0, a.y - 5.0 }, { a.x - 5.0, a.y - 5.0 } }, 3u);
            watch = tr;
        } else {
            const u32 clk = part_put(&s, "clock", -200, 0, 20, 30, "100Hz");
            const u32 nt  = part_put(&s, "NOT gate", 0, 0, 40, 30, "");
            const u32 r   = part_put(&s, "resistor", 150, 0, 30, 10, "1k");
            const u32 cap = part_put(&s, "capacitor", 300, 0, 30, 10, "1uF");
            wire_put(&s, clk, 0, nt, 0);
            tie(&s, clk, 1, 0.0, -30.0, NULL);
            wire_put(&s, nt, 1, r, 0);
            wire_put(&s, r, 1, cap, 0);
            tie(&s, cap, 1, 30.0, -20.0, NULL);
            watch = nt;
        }
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        u32 jumps = 0, right = 0;
        for(u32 k = 0; k < 20000u && c.time < 0.06; k++) {
            const b8 trapezoidal = !c.be_next;   // (the rule this step is taken by, as asked for now)
            const fude_zoom_circuit_part* p = cpart(&c, watch);
            const f64 was = way == 0u ? p->state[0] : fude_zoom_circuit_volts(&c, p->node[1]);
            CHECK(fude_zoom_circuit_advance(&c, 1.0, 1u, false));
            const f64 now = way == 0u ? p->state[0] : fude_zoom_circuit_volts(&c, p->node[1]);
            if(!trapezoidal || (way == 0u ? now == was : (now > 2.5) == (was > 2.5))) continue;
            const f64 eighth = fmax(c.step_last / 8.0, 1e-6);   // (FZC_STEP_FIRST at least)
            const b8 planned = c.be_next && c.step_now == eighth;
            CHECK(fude_zoom_circuit_advance(&c, 1.0, 1u, false));
            jumps++;
            right += planned && c.be_last && c.step_last <= eighth ? 1u : 0u;
        }
        printf("  %s: %u jumps in trapezoidal steps in 60 ms, %u followed by backward Euler at an eighth\n", way == 0u ? "a TRIAC fired and let go" : "a clock's edges through a NOT gate",
               jumps, right);
        CHECK(jumps >= (way == 0u ? 6u : 10u) && right == jumps);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 n1 = part_put(&s, "NOR gate", 0, 0, 30, 20, ""), n2 = part_put(&s, "NOR gate", 0, -200, 30, 20, "");
        tie(&s, n1, 0, -40.0, 0.0, NULL);
        tie(&s, n2, 1, -40.0, 0.0, NULL);
        wire_put(&s, n1, 2, n2, 0);
        wire_put(&s, n2, 2, n1, 1);
        const u32 r1 = part_put(&s, "resistor", 200, 0, 30, 10, "1k"), r2 = part_put(&s, "resistor", 200, -200, 30, 10, "1k");
        const u32 cap = part_put(&s, "capacitor", 350, 0, 30, 10, "1uF");
        wire_put(&s, n1, 2, r1, 0);
        wire_put(&s, r1, 1, cap, 0);
        tie(&s, cap, 1, 30.0, -20.0, NULL);
        wire_put(&s, n2, 2, r2, 0);
        tie(&s, r2, 1, 30.0, -20.0, NULL);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        u32 steps = 0, by_euler = 0;
        while(c.time < 0.05 && steps < 10000u) {
            CHECK(fude_zoom_circuit_advance(&c, 1.0, 1u, false));
            steps++;
            by_euler += c.be_last ? 1u : 0u;
        }
        printf("  a NOR latch oscillating: %u steps in 50 ms, %u by backward Euler, the last %.3g s\n", steps, by_euler, c.step_last);
        CHECK(steps < 100u && by_euler == steps && c.step_last == c.step);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
}

// The batch's examples work as they say: the decade counter's LEDs one at a time, 0 to 9 and round, a step each tick;
// the SR latch set and reset by its inputs and holding between, the JK changing on each tick (its /Q the other way);
// the comparator's LED off in a room's light, on in the dark; the SCR's lamp lit by its gate and staying lit until its
// switch opens, the TRIAC's lit on AC with its switch closed; the power supply's 7805 at 5 V, its capacitor above 9 V,
// its LED lit; the optocoupler's green LED lit with its switch (the circuits apart), the photodiode's and the
// phototransistor's meters lower as the light grows; the speaker's tone at about 464 Hz.
static void test_batch5_examples(void) {
    printf("batch 5 examples\n");
    fude_zoom_scene s; fude_zoom_circuit c;
    c8 say[64];
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_DECADE);
        u32 leds[10];
        CHECK(example_parts(&s, "LED", NULL, leds, 10u) == 10u);
        static const u32 count[10] = { 5, 1, 0, 2, 6, 7, 3, 9, 4, 8 };   // (each LED's, as drawn: its outputs' lanes in order)
        u32 seq[64], n = 0, bad = 0;
        for(u32 k = 0; k < 300u; k++) {
            CHECK(run_for(&c, 0.01));
            u32 lit = 0, which = FUDE_ZOOM_NONE;
            for(u32 i = 0; i < 10u; i++) if(cpart(&c, leds[i])->shown > 0.3) { lit++; which = count[i]; }
            if(lit != 1u) { bad++; continue; }
            if(n == 0u || seq[n - 1u] != which) { if(n < 64u) seq[n++] = which; }
        }
        u32 wrong = 0;
        for(u32 i = 1; i < n; i++) wrong += seq[i] != (seq[i - 1u] + 1u) % 10u ? 1u : 0u;
        printf("  decade counter: %u steps in 3 s (from %u), %u out of order, %u samples not one LED\n", n - 1u, n != 0u ? seq[0] : 99u, wrong, bad);
        CHECK(n >= 12u && n <= 14u && seq[0] <= 1u && wrong == 0u && bad <= 3u);   // (its clock's first edge at once: 1)
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_FLIPFLOPS);
        u32 in[4], pr[4];
        CHECK(example_parts(&s, "logic input", NULL, in, 4u) == 4u && example_parts(&s, "logic probe", NULL, pr, 4u) == 4u);
        CHECK(run_for(&c, 0.02));
        const u32 q0 = probe(&c, pr[0]), n0 = probe(&c, pr[1]);
        set_in(&c, in[0], 1); CHECK(run_for(&c, 0.02));
        const u32 q1 = probe(&c, pr[0]), n1 = probe(&c, pr[1]);
        set_in(&c, in[0], 0); CHECK(run_for(&c, 0.02));
        const u32 q2 = probe(&c, pr[0]);
        set_in(&c, in[1], 1); CHECK(run_for(&c, 0.02));
        const u32 q3 = probe(&c, pr[0]), n3 = probe(&c, pr[1]);
        set_in(&c, in[1], 0); CHECK(run_for(&c, 0.02));
        const u32 q4 = probe(&c, pr[0]);
        CHECK(q0 == 0u && n0 == 1u && q1 == 1u && n1 == 0u && q2 == 1u && q3 == 0u && n3 == 1u && q4 == 0u);
        u32 changes = 0, apart = 0, was = probe(&c, pr[2]);
        for(u32 k = 0; k < 400u; k++) {
            CHECK(run_for(&c, 0.01));
            const u32 q = probe(&c, pr[2]);
            changes += q != was ? 1u : 0u;
            apart += q != probe(&c, pr[3]) ? 1u : 0u;
            was = q;
        }
        printf("  flip-flops: the SR latch %u%u %u%u %u %u%u %u; the JK changed %u times in 4 s at 1 Hz\n", q0, n0, q1, n1, q2, q3, n3, q4, changes);
        CHECK(changes >= 3u && changes <= 5u && apart == 400u);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_COMPARATOR);
        fude_zoom_circuit_part* p = (fude_zoom_circuit_part*)c.parts.memory;
        const u32 ldr = nth_of(&c, FUDE_ZOOM_MODEL_LDR, 0), led = nth_of(&c, FUDE_ZOOM_MODEL_LED, 0);
        CHECK(ldr != FUDE_ZOOM_NONE && led != FUDE_ZOOM_NONE);
        p[ldr].state[0] = 300.0;
        CHECK(run_for(&c, 0.05));
        const f64 room = p[led].shown;
        p[ldr].state[0] = 3.0;
        CHECK(run_for(&c, 0.05));
        const f64 dark = p[led].shown;
        p[ldr].state[0] = 300.0;
        CHECK(run_for(&c, 0.05));
        printf("  comparator: the LED %.2f at 300 lux, %.2f at 3, %.2f at 300 again\n", room, dark, p[led].shown);
        CHECK(room < 0.05 && dark > 0.5 && p[led].shown < 0.05);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_THYRISTORS);
        u32 lamps[2], sw[2];
        CHECK(example_parts(&s, "lamp", NULL, lamps, 2u) == 2u && example_parts(&s, "SPST switch", NULL, sw, 2u) == 2u);
        const u32 g = example_part(&s, "logic input", NULL);
        CHECK(run_for(&c, 0.3));
        const f64 off = cpart(&c, lamps[0])->shown, off2 = cpart(&c, lamps[1])->shown;
        set_in(&c, g, 1); CHECK(run_for(&c, 0.3));
        const f64 fired = cpart(&c, lamps[0])->shown;
        set_in(&c, g, 0); CHECK(run_for(&c, 0.3));
        const f64 held = cpart(&c, lamps[0])->shown;
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[0]), -1, say, sizeof say));
        CHECK(run_for(&c, 0.3));
        const f64 opened = cpart(&c, lamps[0])->shown;
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[0]), -1, say, sizeof say));
        CHECK(run_for(&c, 0.3));
        const f64 closed = cpart(&c, lamps[0])->shown;
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw[1]), -1, say, sizeof say));
        CHECK(run_for(&c, 0.5));
        f64 ac = 0.0;   // (at its most over a cycle: at 10 ms it crosses 0)
        for(u32 k = 0; k < 20u; k++) {
            CHECK(fude_zoom_circuit_run(&c, 0.001, 40u));
            ac = fmax(ac, cpart(&c, lamps[1])->shown);
        }
        printf("  thyristors: the SCR's lamp %.2f, fired %.2f, held %.2f, its switch open %.2f, closed again %.2f; the TRIAC's %.2f, on %.2f\n", off, fired, held, opened, closed, off2, ac);
        CHECK(off < 0.05 && fired > 0.5 && held > 0.5 && opened < 0.05 && closed < 0.05 && off2 < 0.05 && ac > 0.3);
        CHECK(rde_arr_length(&c.events) == 0u);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_POWER_SUPPLY);
        const u32 reg = example_part(&s, "7805", NULL), cap = example_part(&s, "electrolytic", NULL), led = example_part(&s, "LED", NULL);
        CHECK(run_for(&c, 1.0));
        f64 lo = 1e9, hi = -1e9, out_lo = 1e9, out_hi = -1e9;
        for(u32 k = 0; k < 40u; k++) {
            CHECK(fude_zoom_circuit_run(&c, 0.001, 40u));
            const f64 vc = circuit_volts(&c, cap, 0) - circuit_volts(&c, cap, 1), vo = circuit_volts(&c, reg, 2);
            lo = fmin(lo, vc); hi = fmax(hi, vc); out_lo = fmin(out_lo, vo); out_hi = fmax(out_hi, vo);
        }
        printf("  power supply: its capacitor %.2f to %.2f V, the 7805's out %.3f to %.3f V, the LED %.2f\n", lo, hi, out_lo, out_hi, cpart(&c, led)->shown);
        CHECK(lo > 9.0 && hi < 18.0 && fabs(out_lo - 5.0) < 0.1 && fabs(out_hi - 5.0) < 0.1 && cpart(&c, led)->shown > 0.3);
        CHECK(rde_arr_length(&c.events) == 0u);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_LIGHT_LINK);
        const u32 led = example_part(&s, "LED", NULL), sw = example_part(&s, "SPST switch", NULL);
        u32 m[2];
        CHECK(example_parts(&s, "voltmeter", NULL, m, 2u) == 2u);
        fude_zoom_circuit_part* p = (fude_zoom_circuit_part*)c.parts.memory;
        const u32 pd = nth_of(&c, FUDE_ZOOM_MODEL_PHOTODIODE, 0), pt = nth_of(&c, FUDE_ZOOM_MODEL_PHOTOTRANSISTOR, 0);
        CHECK(pd != FUDE_ZOOM_NONE && pt != FUDE_ZOOM_NONE);
        CHECK(run_for(&c, 0.1));
        const f64 off = cpart(&c, led)->shown;
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw), -1, say, sizeof say));
        CHECK(run_for(&c, 0.1));
        const f64 on = cpart(&c, led)->shown;
        const f64 lux[2] = { 10.0, 500.0 };
        f64 v[2][2];
        for(u32 l = 0; l < 2u; l++) {
            p[pd].state[0] = lux[l]; p[pt].state[0] = lux[l];
            CHECK(run_for(&c, 0.1));
            v[l][0] = cpart(&c, m[0])->shown; v[l][1] = cpart(&c, m[1])->shown;
        }
        printf("  light: the optocoupler's LED %.2f, with its switch %.2f; the photodiode's meter %.2f V at 10 lux, %.2f at 500; the phototransistor's %.2f, %.2f\n", off, on, v[0][0], v[1][0], v[0][1], v[1][1]);
        CHECK(off < 0.05 && on > 0.3);
        CHECK(fabs(v[0][0] - (5.0 - 10.0 * FUDE_ZOOM_PHOTODIODE_A_LUX * 1e5)) < 0.05 && fabs(v[1][0] - (5.0 - 500.0 * FUDE_ZOOM_PHOTODIODE_A_LUX * 1e5)) < 0.1);
        CHECK(fabs(v[0][1] - (5.0 - 10.0 * FUDE_ZOOM_PHOTOTRANSISTOR_A_LUX * 1e3)) < 0.05 && fabs(v[1][1] - (5.0 - 500.0 * FUDE_ZOOM_PHOTOTRANSISTOR_A_LUX * 1e3)) < 0.1);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_SPEAKER_TONE);
        fude_zoom_circuit_sound(&c, 48000.0);
        CHECK(run_for(&c, 0.5));
        const u32 n = (u32)rde_arr_length(&c.audio);
        const f32* a = (const f32*)c.audio.memory;
        u32 cross = 0;
        f64 peak = 0.0;
        for(u32 i = n / 2u + 1u; i < n; i++) {
            cross += (a[i - 1u] < 0.0f) != (a[i] < 0.0f) ? 1u : 0u;
            peak = fmax(peak, fabs((f64)a[i]));
        }
        const f64 span = (f64)(n - n / 2u - 1u) / 48000.0, hz = (f64)cross / (2.0 * span);
        printf("  speaker tone: %u samples, %.0f Hz, its peak %.3f\n", n, hz, peak);
        CHECK(n > 23900u && hz > 400.0 && hz < 520.0 && peak > 0.05 && peak <= 1.0);
        CHECK(rde_arr_length(&c.events) == 0u);
        example_close(&s, &c);
        // Its pace: a frame's sixtieth of a second within 2000 steps (Play's most while a speaker is heard) — and
        // within 400 (Play's otherwise): its steps follow how its capacitor's curve bends, not each 0.05 V it goes
        // (that took over 400 a frame, in slow motion, its sound stuttering).
        for(u32 k = 0; k < 2u; k++) {
            example_open(&s, &c, FUDE_ZOOM_EXAMPLE_SPEAKER_TONE);
            u32 short_frames = 0;
            for(u32 f = 0; f < 60u; f++) {
                const f64 was = c.time;
                CHECK(fude_zoom_circuit_run(&c, 1.0 / 60.0, k == 0u ? 2000u : 400u));
                short_frames += c.time - was < 1.0 / 60.0 - 1e-9 ? 1u : 0u;
            }
            printf("  speaker tone at %u steps a frame: %u of 60 frames short\n", k == 0u ? 2000u : 400u, short_frames);
            CHECK(short_frames == 0u);
            example_close(&s, &c);
        }
    }
}

// The big electronics examples. THE CROSSROADS' LIGHTS: two cycles of its ten steps, at every one (the CD4017's one
// output high) each light as its step says — N–S red 5–9, yellow 4, green 0–3; E–W red 0–4, yellow 9, green 5–8; WALK
// 5–8 — and its number in binary into the CD4511; the beeper heard only while WALK is lit. INH held:
// it stays; RST: back to
// 0. THE PIANO: its keys' notes (C4, E4, A4, C5) within 3% of their frequencies (its 555's OUT), each heard; none held,
// silent; two held, higher than either; the volume pot turned down, quieter.
static u32 freq_of_audio(fude_zoom_circuit* c, f64* peak) {
    const u32 n = (u32)rde_arr_length(&c->audio);
    const f32* a = (const f32*)c->audio.memory;
    u32 cross = 0;
    *peak = 0.0;
    for(u32 i = 1; i < n; i++) {
        cross += (a[i - 1u] < 0.0f) != (a[i] < 0.0f) ? 1u : 0u;
        *peak = fmax(*peak, fabs((f64)a[i]));
    }
    return cross;
}

static void test_big_examples(void) {
    printf("big electronics examples\n");
    fude_zoom_scene s; fude_zoom_circuit c;
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_TRAFFIC);
        fude_zoom_circuit_sound(&c, 8000.0);
        const u32 ch = example_part(&s, "CD4017", "CD4017"), dec = example_part(&s, "CD4511", "CD4511");
        u32 red[2], yellow[2], green[2];
        CHECK(example_parts(&s, "LED", "red", red, 2u) == 2u && example_parts(&s, "LED", "yellow", yellow, 2u) == 2u &&
              example_parts(&s, "LED", "green", green, 2u) == 2u);
        const u32 walk = example_part(&s, "LED", "white");
        u32 ins[5];
        CHECK(example_parts(&s, "logic input", "0", ins, 5u) == 3u);   // (INH, RST, LE)
        static const u32 qpin[10] = { 2, 1, 3, 6, 9, 0, 4, 5, 8, 10 };
        u32 seen = 0, wrong_lights = 0, wrong_bcd = 0, samples = 0, quiet_walk = 0, loud_else = 0;
        for(u32 k = 0; k < 300u; k++) {   // (15 s: two cycles)
            rde_arr_clear(&c.audio);
            CHECK(run_for(&c, 0.05));
            u32 high = 0, n = 0;
            for(u32 q = 0; q < 10u; q++) if(circuit_volts(&c, ch, qpin[q]) > 2.5) { high++; n = q; }
            if(high != 1u) continue;
            samples++;
            seen |= 1u << n;
            const b8 want[7] = { n >= 5u, n == 4u, n <= 3u, n <= 4u, n == 9u, n >= 5u && n <= 8u, n >= 5u && n <= 8u };
            const u32 led[7] = { red[0], yellow[0], green[0], red[1], yellow[1], green[1], walk };
            for(u32 l = 0; l < 7u; l++) {
                const f64 b = cpart(&c, led[l])->shown;
                if(want[l] ? b < 0.1 : b > 0.02) { wrong_lights++; if(wrong_lights < 4u) printf("  step %u: light %u at %.3f\n", n, l, b); }
            }
            const u32 bcd = (circuit_volts(&c, dec, 6) > 2.5 ? 1u : 0u) | (circuit_volts(&c, dec, 0) > 2.5 ? 2u : 0u) | (circuit_volts(&c, dec, 1) > 2.5 ? 4u : 0u) |
                            (circuit_volts(&c, dec, 5) > 2.5 ? 8u : 0u);
            wrong_bcd += bcd != n ? 1u : 0u;
            f64 peak;
            freq_of_audio(&c, &peak);
            if(want[6] && peak < 0.02) quiet_walk++;
            if(!want[6] && peak > 0.005 && n != 5u && n != 9u) loud_else++;   // (as WALK comes on and goes off: its edge heard)
        }
        printf("  crossroads: %u samples, steps seen %03x, %u lights wrong, %u numbers wrong, WALK quiet %u, beeps elsewhere %u\n", samples, seen,
               wrong_lights, wrong_bcd, quiet_walk, loud_else);
        CHECK(seen == 0x3FFu && samples > 250u && wrong_lights == 0u && wrong_bcd == 0u && quiet_walk == 0u && loud_else == 0u);
        // (INH high: held; RST high then low: at 0)
        u32 at = 0;
        for(u32 q = 0; q < 10u; q++) if(circuit_volts(&c, ch, qpin[q]) > 2.5) at = q;
        set_in(&c, ins[0], 1);
        CHECK(run_for(&c, 2.0));
        u32 held = 0;
        for(u32 q = 0; q < 10u; q++) if(circuit_volts(&c, ch, qpin[q]) > 2.5) held = q;
        set_in(&c, ins[0], 0);
        set_in(&c, ins[1], 1);
        CHECK(run_for(&c, 0.2));
        set_in(&c, ins[1], 0);
        CHECK(run_for(&c, 0.05));
        printf("  INH held it at %u (it was %u); RST: Q0 %.2f V\n", held, at, circuit_volts(&c, ch, qpin[0]));
        CHECK(held == at && circuit_volts(&c, ch, qpin[0]) > 2.5);
        CHECK(rde_arr_length(&c.events) == 0u);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_PIANO);
        fude_zoom_circuit_sound(&c, 48000.0);
        u32 keys[13];
        CHECK(example_parts(&s, "push button", NULL, keys, 13u) == 13u);
        static const f64 note[13] = { 261.6, 277.2, 293.7, 311.1, 329.6, 349.2, 370.0, 392.0, 415.3, 440.0, 466.2, 493.9, 523.3 };
        static const u32 tried[4] = { 0u, 4u, 9u, 12u };
        const u32 t555 = example_part(&s, "NE555", "NE555");
        u32 off_note = 0;
        f64 peaks[4] = { 0 };
        for(u32 t = 0; t < 4u; t++) {
            // (its pitch: OUT's rises over 0.4 s; its sound: what the speaker made meanwhile — the coupling capacitor's
            // spikes, a note's own way to sound)
            set_in(&c, keys[tried[t]], 1);
            CHECK(run_for(&c, 0.15));
            rde_arr_clear(&c.audio);
            u32 rises = 0;
            b8 was = circuit_volts(&c, t555, 2) > 4.5;
            for(u32 n = 0; n < 4000u; n++) {
                CHECK(fude_zoom_circuit_run(&c, 1e-4, 400u));
                const b8 h = circuit_volts(&c, t555, 2) > 4.5;
                rises += h && !was ? 1u : 0u;
                was = h;
            }
            freq_of_audio(&c, &peaks[t]);
            const f64 hz = (f64)rises / 0.4;
            printf("  key %u: %.1f Hz (%.1f), its peak %.3f\n", tried[t], hz, note[tried[t]], peaks[t]);
            off_note += fabs(hz / note[tried[t]] - 1.0) > 0.03 ? 1u : 0u;
            set_in(&c, keys[tried[t]], 0);
            CHECK(run_for(&c, 0.15));
        }
        CHECK(off_note == 0u && peaks[0] > 0.05 && peaks[3] > 0.05);
        rde_arr_clear(&c.audio);
        CHECK(run_for(&c, 0.3));
        f64 quiet;
        freq_of_audio(&c, &quiet);
        set_in(&c, keys[0], 1); set_in(&c, keys[4], 1);
        CHECK(run_for(&c, 0.15));
        rde_arr_clear(&c.audio);
        CHECK(run_for(&c, 0.3));
        f64 both_peak;
        freq_of_audio(&c, &both_peak);
        u32 both_rises = 0;
        b8 both_was = circuit_volts(&c, t555, 2) > 4.5;
        for(u32 n = 0; n < 4000u; n++) {
            CHECK(fude_zoom_circuit_run(&c, 1e-4, 400u));
            const b8 h = circuit_volts(&c, t555, 2) > 4.5;
            both_rises += h && !both_was ? 1u : 0u;
            both_was = h;
        }
        const f64 both = (f64)both_rises / 0.4;
        // (the volume pot a step round: 75% to 100%, its wiper at the end — and on to 0%: the most resistance, quieter)
        const u32 pot = example_part(&s, "potentiometer", NULL);
        c8 say[16];
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, pot), -1, say, sizeof say));
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, pot), -1, say, sizeof say));
        CHECK(run_for(&c, 0.15));
        rde_arr_clear(&c.audio);
        CHECK(run_for(&c, 0.3));
        f64 low_peak;
        freq_of_audio(&c, &low_peak);
        printf("  none held: its peak %.4f; C4 and E4 held: %.1f Hz, its peak %.3f; the volume at %s: %.3f\n", quiet, both, both_peak, say, low_peak);
        CHECK(quiet < 0.002 && both > note[4] * 1.1 && low_peak < 0.6 * both_peak);
        CHECK(rde_arr_length(&c.events) == 0u);
        example_close(&s, &c);
    }
}

// AN OSCILLOSCOPE (display.h): its text read; its sampling from steps of any length (a ramp's samples on a line); on a
// 50 Hz sine of 5 V (5 ms a division: two and a half cycles a sweep), triggered rising through its middle — its first
// sample there, rising, 160 samples a cycle, ±5 V —, or through 2.5 V when told; CH2 a steady 3 V; on steady 1 V alone,
// sweeping all the same (auto); the circuit's steps no longer than a 50th of its sweep.
static void test_scope(void) {
    printf("oscilloscope\n");
    fude_zoom_scope sc = fude_zoom_display_scope("1ms 2V");
    CHECK(fabs(sc.time_div - 1e-3) < 1e-15 && fabs(sc.volt_div - 2.0) < 1e-12 && isnan(sc.level));
    sc = fude_zoom_display_scope("200us 500mV trig 1.5V");
    CHECK(fabs(sc.time_div - 2e-4) < 1e-15 && fabs(sc.volt_div - 0.5) < 1e-12 && fabs(sc.level - 1.5) < 1e-12);
    sc = fude_zoom_display_scope("2V trig -1V 10ms");
    CHECK(fabs(sc.time_div - 1e-2) < 1e-15 && fabs(sc.volt_div - 2.0) < 1e-12 && fabs(sc.level + 1.0) < 1e-12);
    sc = fude_zoom_display_scope("");
    CHECK(fabs(sc.time_div - 1e-3) < 1e-15 && fabs(sc.volt_div - 2.0) < 1e-12);
    {
        // A ramp of 1 V a second, stepped unevenly, its trigger 0.05 V: its samples on the ramp from
        // where it rose through.
        f64 st[8];
        f32 smp[2u * FUDE_ZOOM_SCOPE_SAMPLES];
        const fude_zoom_scope r = { .time_div = 1e-3, .volt_div = 1.0, .level = 0.05 };
        fude_zoom_scope_step(&r, st, smp, 0.0, 0.0, 0.0, true);
        f64 t = 0.0;
        for(u32 k = 0; t < 0.07; k++) {
            t += (k % 3u == 0u) ? 0.00037 : 0.00011;
            fude_zoom_scope_step(&r, st, smp, t, t, -t, false);
        }
        f64 worst = 0.0;
        for(u32 i = 0; i < FUDE_ZOOM_SCOPE_SAMPLES; i++) {
            worst = fmax(worst, fabs((f64)smp[i] - (0.05 + 0.01 * (f64)i / (f64)FUDE_ZOOM_SCOPE_SAMPLES * 1.0)));
            worst = fmax(worst, fabs((f64)smp[FUDE_ZOOM_SCOPE_SAMPLES + i] + (0.05 + 0.01 * (f64)i / (f64)FUDE_ZOOM_SCOPE_SAMPLES)));
        }
        CHECK(worst < 1e-5);
    }
    for(u32 k = 0; k < 3u; k++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 ac = part_put(&s, k == 2u ? "DC source" : "AC source", -300, 0, 20, 30, k == 2u ? "1V" : "5V 50Hz");
        const u32 sp = part_put(&s, "oscilloscope", 0, 0, 110, 80, k == 1u ? "5ms 2V trig 2.5V" : "5ms 2V");
        wire_put(&s, ac, 0, sp, 0);
        wire_put(&s, ac, 1, sp, 2);
        const u32 g = part_put(&s, "ground", -300, -150, 20, 20, "");
        wire_put(&s, ac, 1, g, 0);
        if(k == 0u) tie(&s, sp, 1, -40.0, 0.0, "3V");
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(c.step <= 0.05 / 50.0 + 1e-12);   // (its sweep in 50 steps at least: its samples straight across a step)
        CHECK(run_for(&c, k == 2u ? 0.5 : 0.31));
        const fude_zoom_circuit_part* p = cpart(&c, sp);
        const f32* smp = (const f32*)fude_zoom_circuit_store(&c, p);
        CHECK(smp != NULL);
        if(smp == NULL) { fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s); continue; }
        // (the last full sweep: those taken since it began, and the rest from the sweep before — a cycle is the same)
        f64 lo = 1e300, hi = -1e300;
        for(u32 i = 0; i < FUDE_ZOOM_SCOPE_SAMPLES; i++) { lo = fmin(lo, smp[i]); hi = fmax(hi, smp[i]); }
        if(k == 2u) {
            printf("  on steady 1 V: %.3f to %.3f, sweeping (at %u)\n", lo, hi, fude_zoom_scope_at(p->state));
            CHECK(fabs(lo - 1.0) < 1e-3 && fabs(hi - 1.0) < 1e-3);
        } else {
            // Rising crossings of its level along the sweep: a cycle (20 ms) 160 samples apart.
            const f64 level = k == 1u ? 2.5 : 0.0;
            u32 first = FUDE_ZOOM_NONE, second = FUDE_ZOOM_NONE;
            for(u32 i = 1; i < FUDE_ZOOM_SCOPE_SAMPLES; i++) {
                if(smp[i - 1u] < level && smp[i] >= level) { if(first == FUDE_ZOOM_NONE) first = i; else if(second == FUDE_ZOOM_NONE) second = i; }
            }
            printf("  on 50 Hz, 5 V%s: %.3f to %.3f V, its first sample %.3f (then %.3f), a cycle %d samples\n", k == 1u ? " (trig 2.5 V)" : "", lo, hi, smp[0], smp[1],
                   second != FUDE_ZOOM_NONE && first != FUDE_ZOOM_NONE ? (i32)(second - first) : -1);
            CHECK(fabs(hi - 5.0) < 0.1 && fabs(lo + 5.0) < 0.1);
            CHECK(fabs((f64)smp[0] - level) < 0.15 && smp[1] > smp[0]);   // (triggered: rising through its level)
            CHECK(first != FUDE_ZOOM_NONE && second != FUDE_ZOOM_NONE && abs((i32)(second - first) - 160) <= 2);
            if(k == 0u) {
                f64 w = 0.0;
                for(u32 i = 0; i < FUDE_ZOOM_SCOPE_SAMPLES; i++) w = fmax(w, fabs((f64)smp[FUDE_ZOOM_SCOPE_SAMPLES + i] - 3.0));
                CHECK(w < 1e-3);   // (CH2: the steady 3 V)
            }
        }
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
}

// A notebook's pages (pages.h): one under the other at their real size, a millimetre a unit; the page a point is on;
// one page more than the last one written on — what was drawn on it in the home frame, or zoomed in (a frame of its
// own); the paper's lines at their true spacing, finer ones fading in as the view goes in, fading out further out than
// that; kept in <id>.pages.
// A canvas's words (words.h): counted and found as Find matches, an excerpt round the first match (cut at whole
// characters, … where it was), kept and read back; a canvas looked through without being opened, its file untouched.
static void test_words(void) {
    printf("words\n");
    const c8* hay = "Milk and milk\nand MILK, then aaaa";
    CHECK(fude_zoom_words_count(hay, (u32)strlen(hay), "milk") == 3u && fude_zoom_words_count(hay, (u32)strlen(hay), "aa") == 2u);
    CHECK(fude_zoom_words_count(hay, (u32)strlen(hay), "") == 0u && fude_zoom_words_count(hay, 4u, "milk") == 1u && fude_zoom_words_count(hay, 3u, "milk") == 0u);
    CHECK(fude_zoom_words_find(hay, (u32)strlen(hay), "MILK,") == 18u && fude_zoom_words_find(hay, (u32)strlen(hay), "cheese") == FUDE_ZOOM_WORDS_NONE);
    c8 out[200];
    CHECK(fude_zoom_words_excerpt(hay, (u32)strlen(hay), "then", 60u, out, sizeof(out)) && strcmp(out, "and MILK, then aaaa") == 0);   // (its line, whole)
    CHECK(!fude_zoom_words_excerpt(hay, (u32)strlen(hay), "cheese", 60u, out, sizeof(out)) && out[0] == 0);
    const c8* line = "Una línea muy larga con café, té y más palabras antes de la palabra buscada: tornillos, y muchas más después de ella";
    CHECK(fude_zoom_words_excerpt(line, (u32)strlen(line), "tornillos", 30u, out, sizeof(out)));
    CHECK(strncmp(out, "\xE2\x80\xA6", 3) == 0 && strstr(out, "tornillos") != NULL && strcmp(out + strlen(out) - 3u, "\xE2\x80\xA6") == 0);
    {
        u32 chars = 0; b8 whole = true;
        for(u32 i = 0; out[i] != 0; i++) { chars += ((u8)out[i] & 0xC0u) != 0x80u; }
        for(u32 i = 1; out[i] != 0; i++) { whole = whole && !(((u8)out[i - 1] & 0xC0u) == 0xC0u && ((u8)out[i] & 0xC0u) != 0x80u); }
        CHECK(chars <= 30u + 2u && whole);
    }
    c8 tiny[12];   // (the room it has: whole characters only)
    CHECK(fude_zoom_words_excerpt(line, (u32)strlen(line), "café", 0u, tiny, sizeof(tiny)) && strlen(tiny) <= 11u);
    // Room for less than its own marks: whatever fits, never past the end (the room once wrapped round to 4 GB).
    for(u32 n = 1; n <= 8u; n++) {
        c8 small[9]; memset(small, 'x', sizeof small);
        fude_zoom_words_excerpt(line, (u32)strlen(line), "tornillos", 30u, small, n);
        CHECK(strlen(small) < n && small[n] == 'x');
    }
    // Kept.
    mkdir("./saves", 0755);
    remove("./saves/w.words");
    rde_arr got = rde_arr_new(sizeof(c8), NULL);
    u64 size = 7u, left = 9u;
    CHECK(!fude_zoom_words_load("./saves/w.words", &size, &left, &got) && size == 0u && left == 0u);
    CHECK(fude_zoom_words_save("./saves/w.words", (1ull << 33) + 5u, 1791763200ull, hay, (u32)strlen(hay)));
    CHECK(fude_zoom_words_load("./saves/w.words", &size, &left, &got) && size == (1ull << 33) + 5u && left == 1791763200ull && strcmp((const c8*)got.memory, hay) == 0);
    FILE* f = fopen("./saves/w.words", "wb"); if(f) { fputs("not words", f); fclose(f); }
    CHECK(!fude_zoom_words_load("./saves/w.words", &size, &left, &got));
    rde_arr_free(&got);
    // Looked through: a canvas's text read, its file as it was.
    {
        remove("./saves/peek.zoom");
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        fude_zoom_file g;
        CHECK(fude_zoom_file_open(&g, "./saves/peek.zoom", &s) == FUDE_LOAD_MISSING);
        fude_zoom_scene_add_text(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, 12.0, 100.0, 20.0, FUDE_ZOOM_TEXT_PLAIN, (rde_color){ 0, 0, 0, 255 },
                                 (rde_color){ 0, 0, 0, 0 }, "buy screws", 10u, 0u);
        CHECK(fude_zoom_file_compact(&g, &s));
        const u64 was = g.size;
        fude_zoom_scene_destroy(&s);
        fude_zoom_scene t; fude_zoom_scene_init(&t, 9);
        CHECK(fude_zoom_file_peek("./saves/peek.zoom", &t) == FUDE_LOAD_OK);
        b8 found = false;
        for(u32 i = 0; i < fude_zoom_scene_object_count(&t); i++) {
            const c8* w = NULL; u32 n = 0;
            found = found || (fude_zoom_scene_text(&t, i, NULL, NULL, NULL, NULL, &w, &n) && n == 10u && memcmp(w, "buy screws", 10) == 0);
        }
        CHECK(found);
        fude_zoom_scene_destroy(&t);
        FILE* z = fopen("./saves/peek.zoom", "rb");
        long now = -1;
        if(z) { fseek(z, 0, SEEK_END); now = ftell(z); fclose(z); }
        CHECK(now == (long)was);
        fude_zoom_scene u; fude_zoom_scene_init(&u, 9);
        CHECK(fude_zoom_file_peek("./saves/none.zoom", &u) == FUDE_LOAD_MISSING);
        fude_zoom_scene_destroy(&u);
    }
}

// The home screen's list (homelist.h): a folder's folders first in their order, then its canvases the most recently
// left first (when the page left them: their words file), the newest made first of those never left; a notebook told
// by its pages file; a folder's count; the grid's columns.
static void test_home_list(void) {
    printf("home list\n");
    mkdir("./saves", 0755);
    mkdir("./saves/notes", 0755);
    fude_notes n; fude_notes_init(&n); fude_notes_load(&n);
    while(rde_arr_length(&n.notes) > 0u) {   // (from a clean index)
        fude_notes_remove(&n, ((const fude_note*)n.notes.memory)[0].id);
        if(rde_arr_length(&n.notes) == 1u && ((const fude_note*)n.notes.memory)[0].kind == FUDE_NOTE_CANVAS) { break; }
    }
    const u32 keep = rde_arr_length(&n.notes) > 0u ? ((const fude_note*)n.notes.memory)[0].id : 0u;
    const u32 a = fude_notes_add(&n, FUDE_NOTE_CANVAS, 0, "Old one");
    const u32 f = fude_notes_add(&n, FUDE_NOTE_FOLDER, 0, "Projects");
    const u32 b = fude_notes_add(&n, FUDE_NOTE_CANVAS, 0, "Book");
    const u32 c = fude_notes_add(&n, FUDE_NOTE_CANVAS, f, "Inside");
    CHECK(a != 0u && f != 0u && b != 0u && c != 0u);
    c8 path[RDE_MAX_PATH];
    fude_zoom_home_side_path(a, "words", path, sizeof(path));
    CHECK(strstr(path, "notes/") != NULL && strcmp(path + strlen(path) - 6u, ".words") == 0);
    CHECK(fude_zoom_words_save(path, 0u, 4000000000ull, "x", 1u));   // (left long after the others were made)
    fude_zoom_home_side_path(b, "pages", path, sizeof(path));
    fude_zoom_pages p; fude_zoom_pages_init(&p); fude_zoom_pages_size(&p, FUDE_ZOOM_PAGES_A5);
    CHECK(fude_zoom_pages_save(&p, path));
    rde_arr cards = rde_arr_new(sizeof(fude_zoom_home_card), NULL);
    const u32 count = fude_zoom_home_gather(&n, 0u, &cards);
    const fude_zoom_home_card* k = (const fude_zoom_home_card*)cards.memory;
    CHECK(count == (keep != 0u ? 4u : 3u));
    CHECK(k[0].note == f && k[0].kind == FUDE_NOTE_FOLDER && k[0].inside == 1u);
    CHECK(k[1].note == a && k[1].edited == 4000000000ull && !k[1].notebook);   // (left last: first)
    CHECK(k[2].note == b && k[2].notebook && k[2].kind == FUDE_NOTE_CANVAS);   // (never left, the newest made)
    CHECK(fude_zoom_home_gather(&n, f, &cards) == 1u && ((const fude_zoom_home_card*)cards.memory)[0].note == c);
    CHECK(fude_zoom_home_gather(&n, 999u, &cards) == 0u);
    // The date from the words file's start alone (fude_zoom_words_left), its words not read: a file whose words were
    // cut short still dates its canvas (it was read whole, failed, and the canvas was dated by when it was made).
    {
        c8 wp[RDE_MAX_PATH];
        fude_zoom_home_side_path(b, "words", wp, sizeof(wp));
        CHECK(fude_zoom_words_save(wp, 7u, 4100000000ull, "twenty bytes, words.", 20u));
        u64 left = 0u;
        CHECK(fude_zoom_words_left(wp, &left) && left == 4100000000ull);
        u8 head[40];   // (12 of header, 16 of size and time, 4 of length: 8 of its 20 bytes of words kept)
        FILE* rf = fopen(wp, "rb");
        CHECK(rf != NULL && fread(head, 1, sizeof(head), rf) == sizeof(head));
        if(rf != NULL) fclose(rf);
        FILE* wf = fopen(wp, "wb");
        CHECK(wf != NULL && fwrite(head, 1, sizeof(head), wf) == sizeof(head));
        if(wf != NULL) fclose(wf);
        u64 was = 0u;
        rde_arr wo = rde_arr_new(sizeof(c8), NULL);
        CHECK(!fude_zoom_words_load(wp, &was, &left, &wo) && left == 0u);   // (its words: not there whole)
        rde_arr_free(&wo);
        CHECK(fude_zoom_words_left(wp, &left) && left == 4100000000ull);
        CHECK(fude_zoom_home_gather(&n, 0u, &cards) == count && ((const fude_zoom_home_card*)cards.memory)[1].note == b &&
              ((const fude_zoom_home_card*)cards.memory)[1].edited == 4100000000ull);   // (left last now: first after the folder)
        wf = fopen(wp, "wb");   // (too short to hold the time: none)
        CHECK(wf != NULL && fwrite(head, 1, 20u, wf) == 20u);
        if(wf != NULL) fclose(wf);
        CHECK(!fude_zoom_words_left(wp, &left) && left == 0u);
        fude_zoom_home_side_path(b, "pages", wp, sizeof(wp));   // (not a words file: none)
        CHECK(!fude_zoom_words_left(wp, &left) && left == 0u);
        fude_zoom_home_side_path(b, "nothing", wp, sizeof(wp));   // (no file: none)
        CHECK(!fude_zoom_words_left(wp, &left));
        fude_zoom_home_side_path(b, "words", wp, sizeof(wp));
        remove(wp);
    }
    rde_arr_free(&cards);
    fude_notes_destroy(&n);
    // The grid: as many at least 210 wide as fit, 16 apart; one at the least.
    CHECK(fude_zoom_home_columns(1232.0f, 210.0f, 16.0f) == 5u && fude_zoom_home_columns(800.0f, 210.0f, 16.0f) == 3u);
    CHECK(fude_zoom_home_columns(100.0f, 210.0f, 16.0f) == 1u && fude_zoom_home_columns(0.0f, 210.0f, 16.0f) == 1u && fude_zoom_home_columns(436.0f, 210.0f, 16.0f) == 2u);
}

// --- the test ink (inktest.h) ---------------------------------------------------------------------------------------

// The developer's --gate as look.c had it before the test ink moved to inktest.c (2026-10-15), kept here as the
// reference the moved code must match, line for line: its colours, its rose curve, its stepwise zoom and its fill.
static const rde_color ink_ref_colors[] = {
    { 231, 76, 60, 255 }, { 241, 196, 15, 255 }, { 46, 204, 113, 255 }, { 52, 152, 219, 255 }, { 155, 89, 182, 255 }, { 230, 126, 34, 255 },
};
static void ink_ref_curve(fude_zoom_scene* _s, fude_zoom_v2 _c, f64 _r, f64 _phase, u32 _petals, rde_color _color, f64 _width) {
    const u32 _n = 400u;
    const i8  _q = fude_zoom_quantum_for(_s->camera.z);
    const f64 _g = rde_math_ldexp_f64(1.0, _q);
    fude_zoom_qpoint _pts[400];
    fude_zoom_v2     _first = { 0.0, 0.0 };
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _a = 2.0 * RDE_PI * (f64)_i / (f64)(_n - 1u);
        const f64 _k = _r * (0.55 + 0.45 * rde_math_cos_f64((f64)_petals * _a + _phase));
        const fude_zoom_v2 _p = { _c.x + rde_math_cos_f64(_a) * _k, _c.y + rde_math_sin_f64(_a) * _k };
        if(_i == 0) {
            _first = _p;
        }
        _pts[_i] = (fude_zoom_qpoint){ (i32)rde_math_round_to_i64_f64((_p.x - _first.x) / _g), (i32)rde_math_round_to_i64_f64((_p.y - _first.y) / _g), (u16)(700 + 300 * rde_math_sin_f64(_a * 3.0)), _i * 4u };
    }
    const u32 _o = fude_zoom_scene_add_stroke(_s, _s->camera.frame, _first, _q, _pts, _n, FUDE_ZOOM_CHANNEL_PRESSURE | FUDE_ZOOM_CHANNEL_TIME, _color,
                                              (f32)(_width / _s->camera.z), FUDE_ZOOM_FLAG_PRESSURE, 0, 0);
    fude_zoom_history_push(_s, _s->camera.frame, fude_zoom_scene_object(_s, _o)->box, NULL, 0, &_o, 1);
}
static void ink_ref_zoom_by(fude_zoom_scene* _s, f64 _factor, fude_zoom_v2 _half) {
    const u32 _steps = (u32)rde_math_ceil_f64(rde_math_abs_f64(rde_math_log2_f64(_factor))) + 1u;
    const f64 _step  = rde_math_pow_f64(_factor, 1.0 / (f64)_steps);
    for(u32 _i = 0; _i < _steps; _i++) {
        fude_zoom_camera_zoom_at(_s, (fude_zoom_v2){ 0.0, 0.0 }, _step);
        fude_zoom_camera_settle(_s, _half);
    }
}
static void ink_ref_gate(fude_zoom_scene* _s, u64 _want, fude_zoom_v2 _half) {
    u64 _made = 0;
    u32 _seed = 7u;
    for(u32 _l = 0; _l < 4u && _made < _want; _l++) {
        const fude_zoom_v2 _c = _s->camera.at;
        const f64 _u = 1.0 / _s->camera.z;
        const u64 _share = (_want - _made) / (4u - _l);
        for(u64 _p = 0; _p < _share; _p += 400u) {
            _seed = _seed * 1664525u + 1013904223u;
            const f64 _x = _c.x + ((f64)(_seed >> 8 & 0xFFFF) / 65535.0 - 0.5) * 2000.0 * _u;
            _seed = _seed * 1664525u + 1013904223u;
            const f64 _y = _c.y + ((f64)(_seed >> 8 & 0xFFFF) / 65535.0 - 0.5) * 1500.0 * _u;
            ink_ref_curve(_s, (fude_zoom_v2){ _x, _y }, 40.0 * _u, (f64)(_seed & 255u), 3u + (_seed % 5u), ink_ref_colors[_seed % 6u], 2.0);
        }
        _made += _share;
        if(_l < 3u) {
            ink_ref_zoom_by(_s, 1024.0, _half);
        }
    }
    ink_ref_zoom_by(_s, 1.0 / 1024.0 / 1024.0 / 1024.0, _half);
}

// Whether two scenes are the same to the bit: their frames (ids, parents, places, anchors, orders), their objects
// (every field, and each payload's bytes: a stroke's encoded points), the camera, the history and the journal.
static b8 ink_same(const fude_zoom_scene* a, const fude_zoom_scene* b) {
    b8 same = fude_zoom_scene_frame_count(a) == fude_zoom_scene_frame_count(b) && fude_zoom_scene_object_count(a) == fude_zoom_scene_object_count(b);
    for(u32 f = 0; same && f < fude_zoom_scene_frame_count(a); f++) {
        const fude_zoom_frame* fa = fude_zoom_scene_frame(a, f);
        const fude_zoom_frame* fb = fude_zoom_scene_frame(b, f);
        same = fa->id == fb->id && fa->parent == fb->parent && fa->object == fb->object && memcmp(&fa->xf, &fb->xf, sizeof fa->xf) == 0 &&
               memcmp(&fa->anchor, &fb->anchor, sizeof fa->anchor) == 0 && fa->removed == fb->removed && rde_arr_length(&fa->order) == rde_arr_length(&fb->order) &&
               memcmp(fa->order.memory, fb->order.memory, rde_arr_length(&fa->order) * sizeof(u32)) == 0;
    }
    for(u32 i = 0; same && i < fude_zoom_scene_object_count(a); i++) {
        const fude_zoom_object* oa = fude_zoom_scene_object(a, i);
        const fude_zoom_object* ob = fude_zoom_scene_object(b, i);
        same = oa->id == ob->id && oa->gesture == ob->gesture && oa->frame == ob->frame && oa->child == ob->child && oa->count == ob->count && oa->z == ob->z &&
               memcmp(&oa->t, &ob->t, sizeof oa->t) == 0 && memcmp(&oa->box, &ob->box, sizeof oa->box) == 0 && memcmp(&oa->color, &ob->color, sizeof oa->color) == 0 &&
               oa->radius == ob->radius && oa->layer == ob->layer && oa->kind == ob->kind && oa->flags == ob->flags && oa->channels == ob->channels && oa->q == ob->q &&
               oa->rotation == ob->rotation && oa->scale == ob->scale && oa->history == ob->history && (oa->blob == 0u) == (ob->blob == 0u);
        if(same && oa->blob != 0u) {
            const fude_zoom_blob* ba = &((const fude_zoom_blob*)a->blobs.memory)[oa->blob - 1u];
            const fude_zoom_blob* bb = &((const fude_zoom_blob*)b->blobs.memory)[ob->blob - 1u];
            same = fude_zoom_blob_size(ba) == fude_zoom_blob_size(bb) && memcmp(fude_zoom_blob_bytes(ba), fude_zoom_blob_bytes(bb), fude_zoom_blob_size(ba)) == 0;
        }
    }
    same = same && a->camera.frame == b->camera.frame && memcmp(&a->camera.at, &b->camera.at, sizeof a->camera.at) == 0 && a->camera.z == b->camera.z;
    same = same && a->root == b->root && a->home == b->home && a->action_count == b->action_count && a->history_pushes == b->history_pushes &&
           a->history_bytes == b->history_bytes && rde_arr_length(&a->targets) == rde_arr_length(&b->targets) &&
           memcmp(a->targets.memory, b->targets.memory, rde_arr_length(&a->targets) * sizeof(u32)) == 0;
    return same && fude_bytes_size(&a->journal) == fude_bytes_size(&b->journal) && memcmp(a->journal.memory, b->journal.memory, fude_bytes_size(&a->journal)) == 0;
}

// A file's bytes, whole (none: empty).
static rde_arr ink_file_bytes(const c8* path) {
    rde_arr out = rde_arr_new(sizeof(u8), NULL);
    FILE* f = fopen(path, "rb");
    if(f != NULL) {
        u8 chunk[4096];
        size_t got;
        while((got = fread(chunk, 1, sizeof chunk, f)) > 0) {
            memcpy(rde_arr_add_n(&out, got), chunk, got);
        }
        fclose(f);
    }
    return out;
}

// The test ink (inktest.h: Settings' test canvas, the developer's --gate) is the gate's as look.c drew it, stroke for
// stroke: from one point to tens of thousands, the same frames, objects, boxes, encoded points, camera, history and
// journal as the reference above; a stroke for every 400 points of each depth's share, at four depths, and the camera
// back where it began; a million points timed. Settings' Make adds a canvas of its own and opens it, and never touches
// another: the other notes as they were, in their order, even one of the same name, their files byte for byte after
// the new one is filled and saved; a second Make makes another.
static void test_inktest(void) {
    printf("test ink\n");
    const fude_zoom_v2 half = { 512, 384 };
    const u64 counts[] = { 1u, 399u, 400u, 401u, 1600u, 4001u, 25000u };
    for(u32 c = 0; c < sizeof(counts) / sizeof(counts[0]); c++) {
        fude_zoom_scene a; fude_zoom_scene_init(&a, 7);
        fude_zoom_scene b; fude_zoom_scene_init(&b, 7);
        const fude_zoom_camera began = b.camera;
        ink_ref_gate(&a, counts[c], half);
        fude_zoom_inktest_fill(&b, counts[c], half);
        CHECK(ink_same(&a, &b));
        u64 made = 0, strokes = 0;
        for(u32 l = 0; l < 4u; l++) {
            const u64 share = (counts[c] - made) / (4u - l);
            strokes += (share + 399u) / 400u;
            made    += share;
        }
        u64 points = 0;
        CHECK(fude_zoom_scene_alive_strokes(&b, &points) == strokes && points == strokes * 400u && b.history_pushes == strokes);
        u32 depths = 0;
        for(u32 i = 0; i < fude_zoom_scene_object_count(&b); i++) {
            const fude_zoom_object* o = fude_zoom_scene_object(&b, i);
            if(o->kind == FUDE_ZOOM_KIND_STROKE) {
                depths |= 1u << fude_zoom_scene_depth(&b, o->frame);
            }
        }
        CHECK(counts[c] < 1600u || depths == 0xFu);   // (from four strokes, one at each depth)
        CHECK(b.camera.frame == began.frame && fabs(b.camera.z - began.z) < 1e-9 && fabs(b.camera.at.x - began.at.x) < 1e-6 && fabs(b.camera.at.y - began.at.y) < 1e-6);
        fude_zoom_scene_destroy(&a);
        fude_zoom_scene_destroy(&b);
    }
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const clock_t c0 = clock();
        fude_zoom_inktest_fill(&s, FUDE_ZOOM_INKTEST_POINTS, half);
        const f64 ms = (f64)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
        u64 points = 0;
        CHECK(fude_zoom_scene_alive_strokes(&s, &points) == 2500u && points == 1000000u);
        printf("  a million points of test ink: %.0f ms, %u frames, a journal of %.1f MB\n", ms, fude_zoom_scene_frame_count(&s), fude_bytes_size(&s.journal) / 1048576.0);
        fude_zoom_scene_destroy(&s);
    }

    // Make, among the person's canvases: one named as the test canvas is, a folder with one in it, each drawn on and
    // saved, the one in the folder open.
    mkdir("./saves", 0755);
    mkdir("./saves/notes", 0755);
    fude_notes n; fude_notes_init(&n); fude_notes_load(&n);
    const u32 named  = fude_notes_add(&n, FUDE_NOTE_CANVAS, 0, "Drawing test");
    const u32 folder = fude_notes_add(&n, FUDE_NOTE_FOLDER, 0, "Work");
    const u32 inside = fude_notes_add(&n, FUDE_NOTE_CANVAS, folder, "Plans");
    const u32 theirs[2] = { named, inside };
    rde_arr before[2];
    for(u32 k = 0; k < 2u; k++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        line(&s, -50, 10.0 * k, 50, 10.0 * k, 2.0, 2.0f);
        c8 path[RDE_MAX_PATH];
        fude_zoom_home_side_path(theirs[k], "zoom", path, sizeof(path));
        fude_zoom_file f; memset(&f, 0, sizeof f); snprintf(f.path, sizeof f.path, "%s", path);
        CHECK(fude_zoom_file_flush(&f, &s) && fude_zoom_file_checkpoint(&f, &s));
        fude_zoom_scene_destroy(&s);
        before[k] = ink_file_bytes(path);
        CHECK(rde_arr_length(&before[k]) > 0u);
    }
    fude_notes_open(&n, inside);
    const u32 count = (u32)rde_arr_length(&n.notes);
    rde_arr was = rde_arr_new(sizeof(fude_note), NULL);
    memcpy(rde_arr_add_n(&was, count), n.notes.memory, count * sizeof(fude_note));
    const u32 made = fude_zoom_inktest_canvas(&n, "Drawing test");
    const fude_note* m = fude_notes_find(&n, made);
    CHECK(made != 0u && n.open == made && m != NULL && m->kind == FUDE_NOTE_CANVAS && m->parent == 0u && strcmp(m->name, "Drawing test") == 0);
    CHECK(rde_arr_length(&n.notes) == count + 1u && memcmp(n.notes.memory, was.memory, count * sizeof(fude_note)) == 0);
    for(u32 i = 0; i < count; i++) {
        CHECK(((const fude_note*)was.memory)[i].id != made);
    }
    // ...filled as the page fills it (it opens empty: nothing of anyone's is drawn over), and saved.
    {
        c8 path[RDE_MAX_PATH];
        fude_zoom_home_side_path(made, "zoom", path, sizeof(path));
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        fude_zoom_file f;
        CHECK(fude_zoom_file_open(&f, path, &s) == FUDE_LOAD_MISSING && fude_zoom_scene_object_count(&s) == 0u);
        fude_zoom_inktest_fill(&s, 4000u, half);
        u64 drawn = 0;
        const u32 strokes = fude_zoom_scene_alive_strokes(&s, &drawn);
        CHECK(fude_zoom_file_flush(&f, &s) && fude_zoom_file_checkpoint(&f, &s));
        fude_zoom_scene_destroy(&s);
        fude_zoom_scene t; fude_zoom_scene_init(&t, 7);
        u64 points = 0;
        CHECK(strokes == 12u && drawn == 4800u);   // (1000 a depth: three strokes each)
        CHECK(fude_zoom_file_open(&f, path, &t) == FUDE_LOAD_OK && fude_zoom_scene_alive_strokes(&t, &points) == strokes && points == drawn);
        fude_zoom_scene_destroy(&t);
    }
    for(u32 k = 0; k < 2u; k++) {
        c8 path[RDE_MAX_PATH];
        fude_zoom_home_side_path(theirs[k], "zoom", path, sizeof(path));
        rde_arr after = ink_file_bytes(path);
        CHECK(rde_arr_length(&after) == rde_arr_length(&before[k]) && memcmp(after.memory, before[k].memory, rde_arr_length(&after)) == 0);
        rde_arr_free(&after);
        rde_arr_free(&before[k]);
    }
    // A second Make: another new canvas, not the one made before.
    const u32 again = fude_zoom_inktest_canvas(&n, "Drawing test");
    CHECK(again != made && again != named && again != inside && n.open == again && rde_arr_length(&n.notes) == count + 2u);
    CHECK(memcmp(n.notes.memory, was.memory, count * sizeof(fude_note)) == 0);
    // (the list left as it was found: what this test added, with its files, taken out)
    fude_notes_remove(&n, again);
    fude_notes_remove(&n, made);
    fude_notes_remove(&n, folder);
    fude_notes_remove(&n, named);
    rde_arr_free(&was);
    fude_notes_destroy(&n);
}

// Recordings (0.1.93): IMA ADPCM back as it went in, near enough (a tone's error a few per cent of it, silence
// silent, a block's first sample exact, its tail held when short, full scale never wrapping); a canvas's recordings
// and links kept, read back, numbered past those there; a recording's length by its file's whole blocks.
// The links found through a map (Phase 4, 2026-10-15) against the way before, kept here as the reference: the links in
// an array, an object's found by searching from the newest, a link added only when that finds none. After adds (some
// objects linked again), removes, a reload and a file holding an object twice (its newest found), every object's
// answer must be the same, and the links the same; and 10,000 links added both ways, timed.
typedef struct { rde_arr links; } aur_ref;
static b8 aur_link_of(const aur_ref* r, fude_zoom_id o, u32* num, u32* ms) {
    const fude_zoom_audio_link* l = (const fude_zoom_audio_link*)r->links.memory;
    for(u32 i = (u32)rde_arr_length(&r->links); i-- > 0u;) if(l[i].object == o) { *num = l[i].recording; *ms = l[i].at_ms; return true; }
    return false;
}
static void aur_add(aur_ref* r, fude_zoom_id o, u32 num, u32 ms) {
    u32 n, m;
    if(aur_link_of(r, o, &n, &m)) return;
    const fude_zoom_audio_link l = { o, num, ms };
    rde_arr_add(&r->links, (any)&l);
}
static void aur_remove(aur_ref* r, u32 num) {
    fude_zoom_audio_link* l = (fude_zoom_audio_link*)r->links.memory; u32 w = 0;
    for(u32 i = 0; i < (u32)rde_arr_length(&r->links); i++) if(l[i].recording != num) l[w++] = l[i];
    rde_arr_resize(&r->links, w);
}
// Every id from 1 to _ids asked of both; true when every answer is the same and so are the links.
static b8 aur_same(const fude_zoom_audio* a, const aur_ref* r, u32 ids) {
    if(!cf_same_arr(&a->links, &r->links)) return false;
    for(u32 i = 0; i <= ids; i++) {
        u32 n1 = 0, m1 = 0, n2 = 0, m2 = 0;
        const fude_zoom_id o = 0x500000000ull + i;
        const b8 f1 = fude_zoom_audio_link_of(a, o, &n1, &m1), f2 = aur_link_of(r, o, &n2, &m2);
        if(f1 != f2 || n1 != n2 || m1 != m2) return false;
    }
    return true;
}
static void test_audio_links(void) {
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    fude_zoom_audio a; fude_zoom_audio_init(&a);
    aur_ref r = { rde_arr_new(sizeof(fude_zoom_audio_link), heap) };
    rng = 4242u;
    for(u32 rec = 1; rec <= 6u; rec++) {   // six recordings, each linking objects new and old
        CHECK(fude_zoom_audio_begin(&a, 1791763200ull + rec, FUDE_ZOOM_AUDIO_RATE) == rec);
        for(u32 i = 0; i < 400u; i++) {
            const fude_zoom_id o = 0x500000000ull + rnd() % 1500u;
            const u32 ms = rnd() % 600000u;
            fude_zoom_audio_add_link(&a, o, rec, ms); aur_add(&r, o, rec, ms);
        }
    }
    CHECK(aur_same(&a, &r, 1600u));
    fude_zoom_audio_remove(&a, 2u); aur_remove(&r, 2u);   // (the links after them closed up)
    fude_zoom_audio_remove(&a, 5u); aur_remove(&r, 5u);
    CHECK(aur_same(&a, &r, 1600u));
    for(u32 i = 0; i < 300u; i++) {   // linked again after the removes: those freed take new links
        const fude_zoom_id o = 0x500000000ull + rnd() % 1600u;
        fude_zoom_audio_add_link(&a, o, 6u, i); aur_add(&r, o, 6u, i);
    }
    CHECK(aur_same(&a, &r, 1600u));
    mkdir("./saves", 0755);
    CHECK(fude_zoom_audio_save(&a, "./saves/links.audio"));
    fude_zoom_audio b; fude_zoom_audio_init(&b);
    CHECK(fude_zoom_audio_load(&b, "./saves/links.audio") && aur_same(&b, &r, 1600u));
    // A file with an object linked twice (no add makes one, but a file may hold it): its newest is found, as before,
    // and still after a remove; adding to it is refused as before.
    fude_zoom_audio d; fude_zoom_audio_init(&d);
    const fude_zoom_audio_link twice[3] = { { 0x500000007ull, 1u, 10u }, { 0x500000008ull, 1u, 20u }, { 0x500000007ull, 3u, 30u } };
    fude_zoom_audio_begin(&d, 1u, FUDE_ZOOM_AUDIO_RATE); fude_zoom_audio_begin(&d, 2u, FUDE_ZOOM_AUDIO_RATE); fude_zoom_audio_begin(&d, 3u, FUDE_ZOOM_AUDIO_RATE);
    memcpy(rde_arr_add_n(&d.links, 3u), twice, sizeof twice);   // (written as a damaged or older file might be)
    CHECK(fude_zoom_audio_save(&d, "./saves/twice.audio") && fude_zoom_audio_load(&b, "./saves/twice.audio"));
    aur_ref r2 = { rde_arr_new(sizeof(fude_zoom_audio_link), heap) };
    memcpy(rde_arr_add_n(&r2.links, 3u), twice, sizeof twice);
    u32 num = 0, ms = 0;
    CHECK(aur_same(&b, &r2, 10u) && fude_zoom_audio_link_of(&b, 0x500000007ull, &num, &ms) && num == 3u && ms == 30u);
    fude_zoom_audio_add_link(&b, 0x500000007ull, 2u, 99u); aur_add(&r2, 0x500000007ull, 2u, 99u);
    CHECK(aur_same(&b, &r2, 10u));
    fude_zoom_audio_remove(&b, 3u); aur_remove(&r2, 3u);   // (its older one found again)
    CHECK(aur_same(&b, &r2, 10u) && fude_zoom_audio_link_of(&b, 0x500000007ull, &num, &ms) && num == 1u && ms == 10u);
    fude_zoom_audio_add_link(&b, 0x500000009ull, 1u, 5u); aur_add(&r2, 0x500000009ull, 1u, 5u);
    CHECK(aur_same(&b, &r2, 10u));
    fude_zoom_audio_clear(&b);   // (another canvas: nothing linked)
    CHECK(rde_arr_length(&b.links) == 0u && b.next == 1u && !fude_zoom_audio_link_of(&b, 0x500000008ull, &num, &ms));
    CHECK(!fude_zoom_audio_load(&b, "./saves/none.audio") && !fude_zoom_audio_link_of(&b, 0x500000008ull, &num, &ms));
    // 10,000 objects linked, as a long recording does, both ways.
    fude_zoom_audio big; fude_zoom_audio_init(&big);
    aur_ref rb = { rde_arr_new(sizeof(fude_zoom_audio_link), heap) };
    clock_t t0 = clock();
    for(u32 i = 0; i < 10000u; i++) fude_zoom_audio_add_link(&big, 0x600000000ull + i, 1u, i);
    clock_t t1 = clock();
    for(u32 i = 0; i < 10000u; i++) aur_add(&rb, 0x600000000ull + i, 1u, i);
    clock_t t2 = clock();
    CHECK(cf_same_arr(&big.links, &rb.links) && fude_zoom_audio_link_of(&big, 0x600000000ull + 1234u, &num, &ms) && ms == 1234u);
    printf("  audio: 10,000 links added: now %.1f ms, before %.1f ms\n", 1000.0 * (double)(t1 - t0) / CLOCKS_PER_SEC, 1000.0 * (double)(t2 - t1) / CLOCKS_PER_SEC);
    fude_zoom_audio_free(&a); fude_zoom_audio_free(&b); fude_zoom_audio_free(&d); fude_zoom_audio_free(&big);
    rde_arr_free(&r.links); rde_arr_free(&r2.links); rde_arr_free(&rb.links);
}

// Counts _what in the _n bytes at _b.
static u32 count_in(const u8* _b, u32 _n, const c8* _what) {
    const usize _w = strlen(_what);
    u32 _found = 0;
    for(u32 _i = 0; _i + _w <= _n; _i++) {
        _found += memcmp(&_b[_i], _what, _w) == 0 ? 1u : 0u;
    }
    return _found;
}

// The PDF export's pictures (export_formats.c): each put in once however often it is drawn. Seventy pictures, each on
// both pages of a two-view export: seventy images in the file. (The writer remembered 64 once, so the six past them
// went in again on every page they were drawn on.)
static void test_pdf_pictures_once(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    // A JPEG's head, as much as the writer reads: 16 x 16, grey; each picture's last byte its own.
    u8 jpeg[24] = { 0xFF, 0xD8, 0xFF, 0xC0, 0x00, 0x0B, 0x08, 0x00, 0x10, 0x00, 0x10, 0x01, 0x01, 0x11, 0x00, 0xFF, 0xD9 };
    for(u32 i = 0; i < 70u; i++) {
        jpeg[23] = (u8)i;
        fude_zoom_scene_add_image(&s, s.root, (fude_zoom_place){ { -450.0 + 50.0 * (f64)(i % 10u), -300.0 + 50.0 * (f64)(i / 10u) }, 0.0, 1.0 }, 20, 20, jpeg, sizeof jpeg, 0);
    }
    const fude_zoom_camera views[2] = { { s.root, { 0, 0 }, 1.0 }, { s.root, { 0, 0 }, 1.0 } };
    const fude_zoom_v2 halves[2] = { { 500, 400 }, { 500, 400 } };
    fude_bytes pdf = fude_bytes_new(4096);
    CHECK(fude_zoom_export_pdf_views(&s, (rde_color){ 255, 255, 255, 255 }, views, halves, 2u, &pdf));
    const u32 images = count_in((const u8*)pdf.memory, fude_bytes_size(&pdf), "/Subtype /Image");
    CHECK(images == 70u);
    printf("  PDF: 70 pictures drawn on two pages: %u images in the file\n", images);
    rde_arr_free(&pdf);
    fude_zoom_scene_destroy(&s);
}

// The microphone, a stand-in for RDE's (voice.c's only user here): it gives the samples the test puts in it, and says
// how many it lost, as RDE's counts those its ring dropped.
struct rde_audio_recorder { u32 have, at; u64 dropped; f32 samples[2000]; };
static struct rde_audio_recorder fake_mic;
rde_audio_recorder* rde_audio_recorder_open(u32 _rate, u32 _channels, u32 _frames, rde_memory_allocator* _a) {
    (void)_rate; (void)_channels; (void)_frames; (void)_a; return &fake_mic;
}
void rde_audio_recorder_close(rde_audio_recorder* _d) { (void)_d; }
b8   rde_audio_recorder_start(rde_audio_recorder* _d) { (void)_d; return true; }
void rde_audio_recorder_stop(rde_audio_recorder* _d) { (void)_d; }
u32  rde_audio_recorder_get_sample_rate(const rde_audio_recorder* _d) { (void)_d; return FUDE_ZOOM_AUDIO_RATE; }
u32  rde_audio_recorder_get_channels(const rde_audio_recorder* _d) { (void)_d; return 1u; }
u64  rde_audio_recorder_get_dropped_frames(const rde_audio_recorder* _d) { return _d->dropped; }
u32  rde_audio_recorder_read(rde_audio_recorder* _d, f32* _out, u32 _frames) {
    const u32 _n = _d->have - _d->at < _frames ? _d->have - _d->at : _frames;
    memcpy(_out, &_d->samples[_d->at], sizeof(f32) * _n);
    _d->at += _n;
    return _n;
}

// Puts _n samples of _value in the stand-in microphone.
static void fake_mic_say(u32 _n, f32 _value) {
    fake_mic.have = _n;
    fake_mic.at   = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        fake_mic.samples[_i] = _value;
    }
}

// Recording through a stall (voice.h): the microphone's ring lost 3,200 samples (0.2 s) between two pumps. The pump
// says so (for the page's notice), only once, and writes them as silence between what came before and after, so the
// recording keeps its time: what was said after the stall is where it was said, not 0.2 s early.
static void test_voice_lost(void) {
    printf("voice\n");
    const c8* path = "./voice_lost.rec";
    rde_file_delete(path);
    memset(&fake_mic, 0, sizeof(fake_mic));
    fude_zoom_voice_recorder r;
    CHECK(fude_zoom_voice_record(&r, path, FUDE_ZOOM_AUDIO_RATE));
    fake_mic_say(1000u, 0.5f);
    CHECK(fude_zoom_voice_record_pump(&r) == 0u && r.frames == 1000u);
    fake_mic.dropped = 3200u;
    fake_mic_say(1000u, -0.25f);
    CHECK(fude_zoom_voice_record_pump(&r) == 3200u && r.frames == 5200u);
    fake_mic_say(0u, 0.0f);
    CHECK(fude_zoom_voice_record_pump(&r) == 0u);   // (lost once, said once)
    const u32 frames = fude_zoom_voice_record_stop(&r);
    CHECK(frames == 11u * FUDE_ZOOM_ADPCM_FRAMES);   // (5,200 samples: 11 blocks, the last filled out)
    // Read back: loud, then silent where the stall was, then the rest, each where it was said.
    fude_zoom_voice_player p;
    CHECK(fude_zoom_voice_open(&p, path, FUDE_ZOOM_AUDIO_RATE, frames));
    i16 pcm[11u * FUDE_ZOOM_ADPCM_FRAMES];
    u8  block[FUDE_ZOOM_ADPCM_BLOCK];
    for(u32 b = 0; b < 11u; b++) {
        CHECK(rde_file_read_at(p.file, (u64)b * FUDE_ZOOM_ADPCM_BLOCK, block, FUDE_ZOOM_ADPCM_BLOCK) == FUDE_ZOOM_ADPCM_BLOCK);
        fude_zoom_adpcm_decode(block, &pcm[b * FUDE_ZOOM_ADPCM_FRAMES]);
    }
    fude_zoom_voice_close(&p);
    CHECK(pcm[800] > 14000 && pcm[800] < 18500);                       // (0.5: about 16,383)
    CHECK(pcm[2500] > -400 && pcm[2500] < 400 && pcm[3900] > -400 && pcm[3900] < 400);   // (the stall: silence)
    CHECK(pcm[4800] < -6000 && pcm[4800] > -10500);                    // (-0.25, after it: where it was said)
    rde_file_delete(path);
}

static void test_audio(void) {
    printf("audio\n");
    i16 in[FUDE_ZOOM_ADPCM_FRAMES * 4], out[FUDE_ZOOM_ADPCM_FRAMES];
    u8 block[FUDE_ZOOM_ADPCM_BLOCK];
    for(u32 i = 0; i < FUDE_ZOOM_ADPCM_FRAMES * 4; i++) {
        in[i] = (i16)lround(10000.0 * sin(2.0 * 3.14159265358979 * 440.0 * (f64)i / 16000.0));
    }
    fude_zoom_adpcm_state st = { 0 };
    f64 err = 0.0, sig = 0.0;
    for(u32 b = 0; b < 4u; b++) {
        fude_zoom_adpcm_encode(&st, &in[b * FUDE_ZOOM_ADPCM_FRAMES], FUDE_ZOOM_ADPCM_FRAMES, block);
        fude_zoom_adpcm_decode(block, out);
        CHECK(out[0] == in[b * FUDE_ZOOM_ADPCM_FRAMES]);
        for(u32 i = (b == 0u ? 64u : 0u); i < FUDE_ZOOM_ADPCM_FRAMES; i++) {   // (the very first block: its step settling)
            const f64 d = (f64)out[i] - (f64)in[b * FUDE_ZOOM_ADPCM_FRAMES + i];
            err += d * d;
            sig += (f64)in[b * FUDE_ZOOM_ADPCM_FRAMES + i] * (f64)in[b * FUDE_ZOOM_ADPCM_FRAMES + i];
        }
    }
    printf("  a 440 Hz tone: signal to error %.1f dB\n", 10.0 * log10(sig / fmax(err, 1e-9)));
    CHECK(10.0 * log10(sig / fmax(err, 1e-9)) > 24.0);
    // Silence, silent.
    i16 quiet[FUDE_ZOOM_ADPCM_FRAMES];
    memset(quiet, 0, sizeof(quiet));
    fude_zoom_adpcm_state q = { 0 };
    fude_zoom_adpcm_encode(&q, quiet, FUDE_ZOOM_ADPCM_FRAMES, block);
    fude_zoom_adpcm_decode(block, out);
    i32 most = 0;
    for(u32 i = 0; i < FUDE_ZOOM_ADPCM_FRAMES; i++) { most = abs(out[i]) > most ? abs(out[i]) : most; }
    CHECK(most <= 1);
    // Short: the rest of the block holds the last sample (within a step).
    for(u32 i = 0; i < 100u; i++) { quiet[i] = (i16)(i * 50); }
    fude_zoom_adpcm_state h = { 0 };
    fude_zoom_adpcm_encode(&h, quiet, 100u, block);
    fude_zoom_adpcm_decode(block, out);
    CHECK(abs(out[FUDE_ZOOM_ADPCM_FRAMES - 1u] - 4950) < 200);
    // Full scale, square: clamped, never wrapped round.
    for(u32 i = 0; i < FUDE_ZOOM_ADPCM_FRAMES; i++) { quiet[i] = (i / 8u) % 2u ? 32767 : -32768; }
    fude_zoom_adpcm_state f = { 88 };
    fude_zoom_adpcm_encode(&f, quiet, FUDE_ZOOM_ADPCM_FRAMES, block);
    fude_zoom_adpcm_decode(block, out);
    b8 sane = true;
    for(u32 i = 16u; i < FUDE_ZOOM_ADPCM_FRAMES; i++) { sane = sane && ((quiet[i] > 0) == (out[i] > -16000) || abs(out[i]) < 16000); }
    CHECK(sane);
    // A canvas's recordings and links.
    fude_zoom_audio a; fude_zoom_audio_init(&a);
    const u32 r1 = fude_zoom_audio_begin(&a, 1791763200ull, FUDE_ZOOM_AUDIO_RATE), r2 = fude_zoom_audio_begin(&a, 1791763900ull, 22050u);
    CHECK(r1 == 1u && r2 == 2u && fude_zoom_audio_find(&a, 2u)->rate == 22050u && fude_zoom_audio_find(&a, 3u) == NULL);
    fude_zoom_audio_find(&a, r1)->frames = 160000u;
    fude_zoom_audio_add_link(&a, (fude_zoom_id)0x700000001ull, r1, 1500u);
    fude_zoom_audio_add_link(&a, (fude_zoom_id)0x700000002ull, r1, 9000u);
    fude_zoom_audio_add_link(&a, (fude_zoom_id)0x700000001ull, r2, 5u);   // (linked again: its first kept)
    fude_zoom_audio_add_link(&a, (fude_zoom_id)0x700000003ull, r2, 42u);
    u32 num = 0, ms = 0;
    CHECK(fude_zoom_audio_link_of(&a, (fude_zoom_id)0x700000001ull, &num, &ms) && num == r1 && ms == 1500u);
    CHECK(!fude_zoom_audio_link_of(&a, (fude_zoom_id)0x700000009ull, &num, &ms));
    CHECK(fude_zoom_audio_link_count(&a, r1) == 2u && fude_zoom_audio_link_count(&a, r2) == 1u);
    mkdir("./saves", 0755);
    CHECK(fude_zoom_audio_save(&a, "./saves/c.audio"));
    fude_zoom_audio b; fude_zoom_audio_init(&b);
    CHECK(fude_zoom_audio_load(&b, "./saves/c.audio") && rde_arr_length(&b.recordings) == 2u && rde_arr_length(&b.links) == 3u && b.next == 3u);
    CHECK(fude_zoom_audio_find(&b, r1)->started == 1791763200ull && fude_zoom_audio_find(&b, r1)->frames == 160000u);
    CHECK(fude_zoom_audio_link_of(&b, (fude_zoom_id)0x700000002ull, &num, &ms) && num == r1 && ms == 9000u);
    fude_zoom_audio_remove(&b, r1);   // (its links with it)
    CHECK(rde_arr_length(&b.recordings) == 1u && rde_arr_length(&b.links) == 1u && !fude_zoom_audio_link_of(&b, (fude_zoom_id)0x700000002ull, &num, &ms));
    CHECK(fude_zoom_audio_begin(&b, 0u, FUDE_ZOOM_AUDIO_RATE) == 3u);   // (never a number used before)
    FILE* fp = fopen("./saves/c.audio", "wb"); if(fp) { fputs("not audio", fp); fclose(fp); }
    CHECK(!fude_zoom_audio_load(&b, "./saves/c.audio") && rde_arr_length(&b.recordings) == 0u && b.next == 1u);
    CHECK(!fude_zoom_audio_load(&b, "./saves/none.audio"));
    CHECK(fude_zoom_audio_frames_in(FUDE_ZOOM_ADPCM_BLOCK * 3u + 10u) == 3u * FUDE_ZOOM_ADPCM_FRAMES && fude_zoom_audio_frames_in(0u) == 0u);
    fude_zoom_audio_free(&a);
    fude_zoom_audio_free(&b);
    test_audio_links();
}

// A step's tag (0.1.94: the app's own, page.c's pages added or taken out): kept with it — a step with nothing in it but
// its tag too —, there again for undo and redo, kept in the file (its journal, and compacted); nothing and no tag: no
// step. What a recording said (transcript.h, 0.1.95): phrases trimmed, a sound's tag left out; joined, a pause a
// paragraph; lines no wider than asked, each with its first words' time; its tail for the next piece; kept and read
// back; a piece cut where it is quietest.
static void test_transcript(void) {
    printf("transcript\n");
    fude_zoom_transcript t; fude_zoom_transcript_init(&t);
    CHECK(fude_zoom_transcript_add(&t, 0u, 2400u, "  Hoy vamos a ver   "));
    CHECK(!fude_zoom_transcript_add(&t, 2400u, 3000u, " [BLANK_AUDIO] ") && !fude_zoom_transcript_add(&t, 3000u, 3500u, "(music)") && !fude_zoom_transcript_add(&t, 3500u, 3600u, "  "));
    CHECK(!fude_zoom_transcript_add(&t, 3600u, 3700u, "\xE2\x99\xAA \xE2\x99\xAA") && !fude_zoom_transcript_add(&t, 0u, 0u, NULL));
    CHECK(fude_zoom_transcript_add(&t, 2500u, 5000u, "los tornillos de cabeza plana."));
    CHECK(fude_zoom_transcript_add(&t, 9000u, 12000u, "[pause] Luego, la cola."));   // (words with a tag: kept)
    CHECK(rde_arr_length(&t.segments) == 3u);
    rde_arr out = rde_arr_new(sizeof(c8), NULL), lines = rde_arr_new(sizeof(fude_zoom_transcript_line), NULL);
    fude_zoom_transcript_join(&t, 2000u, &out);
    CHECK(strcmp((const c8*)out.memory, "Hoy vamos a ver los tornillos de cabeza plana.\n\n[pause] Luego, la cola.") == 0);
    const u32 n = fude_zoom_transcript_lines(&t, 20u, 2000u, &out, &lines);
    const fude_zoom_transcript_line* l = (const fude_zoom_transcript_line*)lines.memory;
    const c8* w = (const c8*)out.memory;
    printf("  lines:\n%s\n", w);
    CHECK(strcmp(w, "Hoy vamos a ver los\ntornillos de cabeza\nplana.\n\n[pause] Luego, la\ncola.") == 0);
    CHECK(n == 6u && l[0].t0_ms == 0u && l[1].t0_ms == 2500u && l[2].t0_ms == 2500u && l[3].len == 0u && l[4].t0_ms == 9000u);
    CHECK(strncmp(w + l[1].at, "tornillos de cabeza", l[1].len) == 0 && l[1].len == 19u);
    // (a word longer than a line: on its own)
    fude_zoom_transcript u; fude_zoom_transcript_init(&u);
    fude_zoom_transcript_add(&u, 0u, 100u, "a supercalifragilistico b");
    fude_zoom_transcript_lines(&u, 8u, 2000u, &out, &lines);
    CHECK(strcmp((const c8*)out.memory, "a\nsupercalifragilistico\nb") == 0);
    // (characters, not bytes: "plaña" is five)
    fude_zoom_transcript_clear(&u);
    fude_zoom_transcript_add(&u, 0u, 100u, "\xC3\xB1\xC3\xB1\xC3\xB1\xC3\xB1 \xC3\xB1\xC3\xB1");
    fude_zoom_transcript_lines(&u, 7u, 2000u, &out, &lines);
    CHECK(rde_arr_length(&lines) == 1u);
    // Its tail, from a word's start.
    c8 tail[64];
    fude_zoom_transcript_tail(&t, 12u, tail, sizeof(tail));
    CHECK(strcmp(tail, "la cola.") == 0);
    fude_zoom_transcript_tail(&t, 1000u, tail, 10u);
    CHECK(strcmp(tail, "la cola.") == 0);   // (what 10 bytes hold, from a word's start)
    fude_zoom_transcript_tail(&t, 30u, tail, sizeof(tail));
    CHECK(strcmp(tail, "plana. [pause] Luego, la cola.") == 0);   // (phrases joined by a space, not run together)
    // Kept and read back.
    snprintf(t.language, sizeof(t.language), "es");
    t.done = 192000u;
    mkdir("./saves", 0755);
    CHECK(fude_zoom_transcript_save(&t, "./saves/r.text"));
    fude_zoom_transcript v; fude_zoom_transcript_init(&v);
    CHECK(fude_zoom_transcript_load(&v, "./saves/r.text") && rde_arr_length(&v.segments) == 3u && strcmp(v.language, "es") == 0 && v.done == 192000u && !v.complete);
    fude_zoom_transcript_join(&v, 2000u, &out);
    CHECK(strstr((const c8*)out.memory, "cabeza plana.\n\n[pause]") != NULL);
    FILE* f = fopen("./saves/r.text", "wb"); if(f) { fputs("not a transcript", f); fclose(f); }
    CHECK(!fude_zoom_transcript_load(&v, "./saves/r.text") && rde_arr_length(&v.segments) == 0u);
    // Appended (another piece's phrases after it).
    fude_zoom_transcript_append(&v, &t);
    fude_zoom_transcript_append(&v, &t);
    CHECK(rde_arr_length(&v.segments) == 6u);
    // Cut where it is quietest near its end: a tone with a gap of silence 1 s before its end.
    const u32 rate = 16000u, total = rate * 10u;
    f32* pcm = (f32*)malloc(sizeof(f32) * total);
    for(u32 i = 0; i < total; i++) { pcm[i] = (i >= rate * 9u - 2000u && i < rate * 9u + 2000u) ? 0.0f : 0.5f * (f32)sin((f64)i * 0.2); }
    const u32 cut = fude_zoom_transcript_cut(pcm, total, rate, 3000u);
    CHECK(cut > rate * 9u - 2000u && cut < rate * 9u + 2000u);
    CHECK(fude_zoom_transcript_cut(pcm, rate * 2u, rate, 3000u) == rate * 2u);   // (shorter than the window: all of it)
    // (rates of 10 to 19 a second, where a tenth of a second is one sample: it still ends, inside the window)
    for(u32 slow = 10u; slow < 20u; slow++) {
        const u32 c = fude_zoom_transcript_cut(pcm, 100u, slow, 1000u);
        CHECK(c >= 100u - slow && c <= 100u);
    }
    free(pcm);
    // A damaged file with a phrase of no bytes among its others (which add never makes): read, that one passed over.
    {
        u8 b[128]; u32 k = 0;
        #define TRANSCRIPT_U32(v) do { const u32 x_ = (v); b[k++] = (u8)x_; b[k++] = (u8)(x_ >> 8); b[k++] = (u8)(x_ >> 16); b[k++] = (u8)(x_ >> 24); } while(0)
        memcpy(b, "KANA", 4u); k = 4u;
        TRANSCRIPT_U32(1u); TRANSCRIPT_U32(0x534E5254u);   // (version 1, 'TRNS')
        b[k++] = 2u; b[k++] = 'e'; b[k++] = 'n';
        TRANSCRIPT_U32(16000u); b[k++] = 1u;
        TRANSCRIPT_U32(3u);
        TRANSCRIPT_U32(0u);   TRANSCRIPT_U32(500u);  TRANSCRIPT_U32(2u); b[k++] = 'h'; b[k++] = 'i';
        TRANSCRIPT_U32(500u); TRANSCRIPT_U32(900u);  TRANSCRIPT_U32(0u);
        TRANSCRIPT_U32(900u); TRANSCRIPT_U32(1500u); TRANSCRIPT_U32(3u); memcpy(b + k, "you", 3u); k += 3u;
        #undef TRANSCRIPT_U32
        FILE* g = fopen("./saves/empty.text", "wb"); if(g) { fwrite(b, 1, k, g); fclose(g); }
        CHECK(fude_zoom_transcript_load(&v, "./saves/empty.text") && rde_arr_length(&v.segments) == 2u && v.complete && strcmp(v.language, "en") == 0);
        fude_zoom_transcript_join(&v, 2000u, &out);
        CHECK(strcmp((const c8*)out.memory, "hi you") == 0);
    }
    rde_arr_free(&out); rde_arr_free(&lines);
    fude_zoom_transcript_free(&t); fude_zoom_transcript_free(&u); fude_zoom_transcript_free(&v);
}

static void test_history_tags(void) {
    printf("history tags\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    mkdir("./saves", 0755);
    remove("./saves/tags.zoom"); remove("./saves/tags.zoom.bak");
    fude_zoom_file f;
    CHECK(fude_zoom_file_open(&f, "./saves/tags.zoom", &s) == FUDE_LOAD_MISSING);
    const u64 pushes = s.history_pushes;
    fude_zoom_history_push_tagged(&s, s.root, (fude_zoom_box){ 0, 0, 1, 1 }, NULL, 0u, NULL, 0u, NULL, NULL, NULL, 0u, 0u);
    CHECK(s.history_pushes == pushes && !fude_zoom_history_can_undo(&s));
    fude_zoom_history_push_tagged(&s, s.root, (fude_zoom_box){ 0, 0, 1, 1 }, NULL, 0u, NULL, 0u, NULL, NULL, NULL, 0u, 0x50470001u);
    CHECK(fude_zoom_history_can_undo(&s) && fude_zoom_history_next_undo(&s)->tag == 0x50470001u);
    const u32 st = line(&s, 0, 0, 10, 0, 1.0, 0.5f);   // (a step of its own, untagged)
    const fude_zoom_place was = fude_zoom_scene_place_of(&s, st);
    fude_zoom_place now = was;
    now.t.y -= 305.0;
    fude_zoom_scene_set_place(&s, st, now);
    fude_zoom_history_push_tagged(&s, s.root, (fude_zoom_box){ 0, -305, 10, 1 }, NULL, 0u, NULL, 0u, &st, &was, &now, 1u, 0x5047FFFFu);
    CHECK(s.action_count == 3u && fude_zoom_history_next_undo(&s)->tag == 0x5047FFFFu && (u16)(fude_zoom_history_next_undo(&s)->tag & 0xFFFFu) == (u16)-1);
    CHECK(fude_zoom_history_undo(&s) && fabs(fude_zoom_scene_place_of(&s, st).t.y - was.t.y) < 1e-9 && fude_zoom_history_next_redo(&s)->tag == 0x5047FFFFu);
    CHECK(fude_zoom_history_next_undo(&s)->tag == 0u);
    CHECK(fude_zoom_history_undo(&s) && fude_zoom_history_undo(&s) && fude_zoom_history_next_redo(&s)->tag == 0x50470001u && !fude_zoom_history_can_undo(&s));
    CHECK(fude_zoom_history_redo(&s) && fude_zoom_history_next_undo(&s)->tag == 0x50470001u);
    CHECK(fude_zoom_history_redo(&s) && fude_zoom_history_redo(&s) && fabs(fude_zoom_scene_place_of(&s, st).t.y - now.t.y) < 1e-9);
    // Through the journal...
    CHECK(fude_zoom_file_flush(&f, &s));
    fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
    CHECK(fude_zoom_file_open(&g, "./saves/tags.zoom", &t) == FUDE_LOAD_OK);
    CHECK(t.action_count == 3u && fude_zoom_history_next_undo(&t)->tag == 0x5047FFFFu);
    CHECK(fude_zoom_history_undo(&t) && fude_zoom_history_undo(&t) && fude_zoom_history_next_undo(&t)->tag == 0x50470001u);
    // ...and written whole.
    CHECK(fude_zoom_file_compact(&g, &t));
    fude_zoom_scene u; fude_zoom_scene_init(&u, 7); fude_zoom_file h;
    CHECK(fude_zoom_file_open(&h, "./saves/tags.zoom", &u) == FUDE_LOAD_OK);
    CHECK(fude_zoom_history_next_undo(&u)->tag == 0x50470001u && fude_zoom_history_can_redo(&u));
    fude_zoom_scene_destroy(&s);
    fude_zoom_scene_destroy(&t);
    fude_zoom_scene_destroy(&u);
}

static void test_pages(void) {
    printf("pages\n");
    fude_zoom_pages p; fude_zoom_pages_init(&p);
    CHECK(!p.on && p.count == 1u && p.w == 210.0 && p.h == 297.0);
    fude_zoom_pages_size(&p, FUDE_ZOOM_PAGES_A5);
    CHECK(p.on && p.w == 148.0 && p.h == 210.0 && p.count == 1u);
    fude_zoom_box b = fude_zoom_pages_box(&p, 0u);
    CHECK(b.min_x == -74.0 && b.max_x == 74.0 && b.max_y == 0.0 && b.min_y == -210.0);
    b = fude_zoom_pages_box(&p, 1u);
    CHECK(b.max_y == -218.0 && b.min_y == -428.0);
    p.count = 3u;
    const fude_zoom_box all = fude_zoom_pages_bounds(&p);
    CHECK(all.max_y == 0.0 && all.min_y == -2.0 * 218.0 - 210.0 && all.min_x == -74.0);
    CHECK(fude_zoom_pages_at(&p, (fude_zoom_v2){ 0, -100 }) == 0u && fude_zoom_pages_at(&p, (fude_zoom_v2){ 0, -214 }) == 0u &&   // (the gap: the page above)
          fude_zoom_pages_at(&p, (fude_zoom_v2){ 30, -300 }) == 1u && fude_zoom_pages_at(&p, (fude_zoom_v2){ 0, -99999 }) == 2u &&
          fude_zoom_pages_at(&p, (fude_zoom_v2){ 0, 50 }) == 0u);
    fude_zoom_pages_custom(&p, -1.0, 100.0);   // (no such size: as it was)
    CHECK(p.w == 148.0);
    // As many as it needs.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        fude_zoom_pages q; fude_zoom_pages_init(&q); fude_zoom_pages_size(&q, FUDE_ZOOM_PAGES_A4);
        CHECK(!fude_zoom_pages_grow(&q, &s) && q.count == 1u);
        fude_zoom_camera_look_at(&s, s.home, (fude_zoom_v2){ 0, -150 }, 3.0);
        line(&s, -50, -150, 50, -150, 1.0, 0.5f);
        CHECK(fude_zoom_pages_grow(&q, &s) && q.count == 2u && !fude_zoom_pages_grow(&q, &s));
        // (the second page written on zoomed in: in a frame of its own)
        const fude_zoom_box p2 = fude_zoom_pages_box(&q, 1u);
        fude_zoom_camera_look_at(&s, s.home, (fude_zoom_v2){ 20, (p2.min_y + p2.max_y) * 0.5 }, 3.0);
        for(u32 k = 0; k < 12; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&s, (fude_zoom_v2){ 500, 400 }); }
        CHECK(s.camera.frame != s.home);
        { const f64 d = 20.0 / s.camera.z; line(&s, s.camera.at.x - d, s.camera.at.y, s.camera.at.x + d, s.camera.at.y, d / 40.0, (f32)(1.0 / s.camera.z)); }
        CHECK(fude_zoom_pages_used(&q, &s, 1u) && fude_zoom_pages_grow(&q, &s) && q.count == 3u && !fude_zoom_pages_used(&q, &s, 2u));
        // (beside the pages, not on one: no page more)
        fude_zoom_camera_look_at(&s, s.home, (fude_zoom_v2){ 400, -1000 }, 3.0);
        line(&s, 380, -1000, 420, -1000, 1.0, 0.5f);
        CHECK(!fude_zoom_pages_grow(&q, &s) && q.count == 3u);
        printf("  A4: %u pages after two written on (the second zoomed in ×4096)\n", q.count);
        fude_zoom_scene_destroy(&s);
    }
    // Rearranged (0.1.92): a page put in, taken out, moved — what is on them goes with them (a drawing zoomed into
    // too), a page's things let go when it is; one undo step puts it all back.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        fude_zoom_pages q; fude_zoom_pages_init(&q); fude_zoom_pages_size(&q, FUDE_ZOOM_PAGES_A4);
        q.count = 4u;
        const f64 step = 297.0 + FUDE_ZOOM_PAGES_GAP;
        fude_zoom_camera_look_at(&s, s.home, (fude_zoom_v2){ 0, -150 }, 3.0);
        u32 on[3];
        for(u32 k = 0; k < 3u; k++) {
            on[k] = line(&s, -50, -150 - (f64)k * step, 50, -150 - (f64)k * step, 1.0, 0.5f);
        }
        // (page 3 written on zoomed in: a frame of its own)
        fude_zoom_camera_look_at(&s, s.home, (fude_zoom_v2){ 20, -150 - 3.0 * step }, 3.0);
        for(u32 k = 0; k < 12; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&s, (fude_zoom_v2){ 500, 400 }); }
        { const f64 d = 20.0 / s.camera.z; line(&s, s.camera.at.x - d, s.camera.at.y, s.camera.at.x + d, s.camera.at.y, d / 40.0, (f32)(1.0 / s.camera.z)); }
        u32 deep = FUDE_ZOOM_NONE;
        for(u32 k = 0; k < (u32)rde_arr_length(&s.objects); k++) {
            const fude_zoom_object* o = fude_zoom_scene_object(&s, k);
            deep = o->kind == FUDE_ZOOM_KIND_FRAME && o->frame == s.home && (o->flags & FUDE_ZOOM_FLAG_ALIVE) ? k : deep;
        }
        fude_zoom_camera_look_at(&s, s.home, (fude_zoom_v2){ 0, -150 }, 3.0);
        CHECK(deep != FUDE_ZOOM_NONE && fude_zoom_pages_of(&q, &s, deep) == 3u);
        CHECK(fude_zoom_pages_of(&q, &s, on[0]) == 0u && fude_zoom_pages_of(&q, &s, on[1]) == 1u && fude_zoom_pages_of(&q, &s, on[2]) == 2u);
        rde_arr died = rde_arr_new(sizeof(u32), NULL), moved = rde_arr_new(sizeof(u32), NULL);
        rde_arr before = rde_arr_new(sizeof(fude_zoom_place), NULL), after = rde_arr_new(sizeof(fude_zoom_place), NULL);
        #define PAGES_DO(_do, _i, _j) (rde_arr_clear(&died), rde_arr_clear(&moved), rde_arr_clear(&before), rde_arr_clear(&after), \
                                       fude_zoom_pages_rearrange(&q, &s, _do, _i, _j, &died, &moved, &before, &after))
        #define PAGES_KEEP() fude_zoom_history_push_all(&s, s.home, fude_zoom_pages_bounds(&q), (const u32*)died.memory, (u32)rde_arr_length(&died), NULL, 0u, \
                                                        (const u32*)moved.memory, (const fude_zoom_place*)before.memory, (const fude_zoom_place*)after.memory, (u32)rde_arr_length(&moved))
        // A page before the second: the second, third and fourth's things a page down.
        CHECK(PAGES_DO(FUDE_ZOOM_PAGES_INSERT, 1u, 0u) && q.count == 5u && rde_arr_length(&died) == 0u && rde_arr_length(&moved) == 3u);
        PAGES_KEEP();
        CHECK(fude_zoom_pages_of(&q, &s, on[0]) == 0u && fude_zoom_pages_of(&q, &s, on[1]) == 2u && fude_zoom_pages_of(&q, &s, on[2]) == 3u &&
              fude_zoom_pages_of(&q, &s, deep) == 4u && !fude_zoom_pages_used(&q, &s, 1u));
        CHECK(fabs(fude_zoom_scene_place_of(&s, on[1]).t.y - (-150.0 - 2.0 * step)) < 1e-6);   // (a line's place: its first point, a page down)
        CHECK(fude_zoom_history_undo(&s) && fude_zoom_pages_of(&q, &s, on[1]) == 1u && fude_zoom_pages_of(&q, &s, deep) == 3u);
        q.count = 4u;
        // The second taken out: its line let go, the third and fourth's up a page.
        CHECK(PAGES_DO(FUDE_ZOOM_PAGES_DELETE, 1u, 0u) && q.count == 3u && rde_arr_length(&died) == 1u && ((const u32*)died.memory)[0] == on[1] &&
              rde_arr_length(&moved) == 2u);
        PAGES_KEEP();
        CHECK(!(fude_zoom_scene_object(&s, on[1])->flags & FUDE_ZOOM_FLAG_ALIVE) && fude_zoom_pages_of(&q, &s, on[2]) == 1u && fude_zoom_pages_of(&q, &s, deep) == 2u);
        CHECK(fude_zoom_history_undo(&s) && (fude_zoom_scene_object(&s, on[1])->flags & FUDE_ZOOM_FLAG_ALIVE));
        q.count = 4u;
        CHECK(fude_zoom_pages_of(&q, &s, on[1]) == 1u && fude_zoom_pages_of(&q, &s, on[2]) == 2u && fude_zoom_pages_of(&q, &s, deep) == 3u);
        // The first made the third: the second and third's up one.
        CHECK(PAGES_DO(FUDE_ZOOM_PAGES_MOVE, 0u, 2u) && q.count == 4u && rde_arr_length(&died) == 0u && rde_arr_length(&moved) == 3u);
        CHECK(fude_zoom_pages_of(&q, &s, on[0]) == 2u && fude_zoom_pages_of(&q, &s, on[1]) == 0u && fude_zoom_pages_of(&q, &s, on[2]) == 1u &&
              fude_zoom_pages_of(&q, &s, deep) == 3u);
        // ...and back up: the fourth (the drawing zoomed into) made the first.
        CHECK(PAGES_DO(FUDE_ZOOM_PAGES_MOVE, 3u, 0u) && fude_zoom_pages_of(&q, &s, deep) == 0u && fude_zoom_pages_of(&q, &s, on[1]) == 1u &&
              fude_zoom_pages_of(&q, &s, on[0]) == 3u);
        // No such page; a notebook's only page taken out: emptied, still one.
        CHECK(!PAGES_DO(FUDE_ZOOM_PAGES_DELETE, 4u, 0u) && !PAGES_DO(FUDE_ZOOM_PAGES_MOVE, 0u, 9u) && !PAGES_DO(FUDE_ZOOM_PAGES_INSERT, 5u, 0u));
        CHECK(PAGES_DO(FUDE_ZOOM_PAGES_INSERT, 4u, 0u) && q.count == 5u && rde_arr_length(&moved) == 0u);   // (at the end)
        q.count = 1u;
        CHECK(PAGES_DO(FUDE_ZOOM_PAGES_DELETE, 0u, 0u) && q.count == 1u && rde_arr_length(&died) >= 1u);
        #undef PAGES_DO
        #undef PAGES_KEEP
        rde_arr_free(&died); rde_arr_free(&moved); rde_arr_free(&before); rde_arr_free(&after);
        fude_zoom_scene_destroy(&s);
    }
    // Paper (out 10 points, in 40): 8 mm lines 36 points apart, as a page fills a tablet's width, drawn as they are, at
    // their full strength; zoomed in 5 times (180 points), every 2 mm (45: the finest an eighth of the way in); far out
    // (6 points), faded — none at a quarter of 10.
    f64 fade = 0.0;
    CHECK(fude_zoom_pages_spacing(8.0, 36.0 / 8.0, 10.0, 40.0, &fade) == 8.0 && fade == 1.0);
    CHECK(fude_zoom_pages_spacing(8.0, 180.0 / 8.0, 10.0, 40.0, &fade) == 2.0 && fabs(fade - 0.125) < 1e-12);
    CHECK(fude_zoom_pages_spacing(8.0, 6.0 / 8.0, 10.0, 40.0, &fade) == 8.0 && fabs(fade - (6.0 - 2.5) / 7.5) < 1e-12);
    CHECK(fude_zoom_pages_spacing(8.0, 2.0 / 8.0, 10.0, 40.0, &fade) == 8.0 && fade == 0.0);
    CHECK(fude_zoom_pages_spacing(8.0, 1e12, 10.0, 40.0, &fade) * 1e12 < 80.0);   // (deep in: as fine as it needs)
    // Kept.
    mkdir("./saves", 0755);
    remove("./saves/book.pages");
    fude_zoom_pages r;
    CHECK(!fude_zoom_pages_load(&r, "./saves/book.pages") && !r.on);
    p.count = 42u;
    CHECK(fude_zoom_pages_save(&p, "./saves/book.pages") && fude_zoom_pages_load(&r, "./saves/book.pages"));
    CHECK(r.on && r.w == 148.0 && r.h == 210.0 && r.count == 42u);
    FILE* f = fopen("./saves/book.pages", "wb"); if(f) { fputs("not pages", f); fclose(f); }
    CHECK(!fude_zoom_pages_load(&r, "./saves/book.pages") && !r.on && r.count == 1u);
}


// The maths font (mathfont.h: Noto Sans Math, as the apps ship it) and the typesetter (tex.h).
static u8* maths_font_bytes(u32* size) {
    c8 path[1024];
    snprintf(path, sizeof path, "%s/../sketching/assets/fonts/NotoSansMath-Regular.ttf", FUDE_ROOT);
    FILE* f = fopen(path, "rb");
    if(f == NULL) return NULL;
    fseek(f, 0, SEEK_END); const long n = ftell(f); fseek(f, 0, SEEK_SET);
    u8* d = (u8*)malloc((size_t)n);
    *size = (u32)fread(d, 1, (size_t)n, f);
    fclose(f);
    return d;
}

// Its characters' glyphs (the basic plane's and the maths alphabets' past it), advances and boxes; its MATH table's
// constants, italic corrections, bigger forms and the pieces a taller one is built of; outlines as closed polygons,
// holes among them (o, B), within their glyph's box; and a damaged font read as none, never past its bytes.
static void test_maths_font(void) {
    printf("maths font\n");
    u32 size = 0;
    u8* bytes = maths_font_bytes(&size);
    CHECK(bytes != NULL);
    if(bytes == NULL) return;
    fude_zoom_mfont f;
    CHECK(fude_zoom_mfont_load(&f, bytes, size) && f.units_per_em == 1000u && f.glyphs > 4000u && f.math != 0u);
    const u32 A = fude_zoom_mfont_glyph(&f, 'A'), xi = fude_zoom_mfont_glyph(&f, 0x1D465), integral = fude_zoom_mfont_glyph(&f, 0x222B);
    CHECK(A != 0u && xi != 0u && integral != 0u && fude_zoom_mfont_glyph(&f, 0x10FFFF) == 0u);
    i32 box[4];
    CHECK(fude_zoom_mfont_box(&f, A, box) && box[2] > box[0] && box[3] > 600 && fude_zoom_mfont_advance(&f, A) > 500);
    CHECK(!fude_zoom_mfont_box(&f, fude_zoom_mfont_glyph(&f, ' '), box));   // (a space: no outline)
    const i32 axis = fude_zoom_mfont_constant(&f, FUDE_ZOOM_MATH_AXIS_HEIGHT), rule = fude_zoom_mfont_constant(&f, FUDE_ZOOM_MATH_FRACTION_RULE_THICKNESS);
    const i32 sp = fude_zoom_mfont_constant(&f, FUDE_ZOOM_MATH_SCRIPT_PERCENT), ssp = fude_zoom_mfont_constant(&f, FUDE_ZOOM_MATH_SCRIPTSCRIPT_PERCENT);
    printf("  axis %d, fraction rule %d, scripts %d%% and %d%%, ∫'s italic correction %d\n", axis, rule, sp, ssp, fude_zoom_mfont_italic(&f, integral));
    CHECK(axis > 200 && axis < 350 && rule > 30 && rule < 120 && sp >= 50 && sp <= 80 && ssp >= 40 && ssp < sp);
    CHECK(fude_zoom_mfont_italic(&f, integral) > 0 && fude_zoom_mfont_italic(&f, A) == 0);
    u32 vg[32]; i32 vs[32];
    const u32 nv = fude_zoom_mfont_variants(&f, fude_zoom_mfont_glyph(&f, '('), true, vg, vs, 32u);
    b8 growing = nv >= 3u;
    for(u32 i = 1; i < nv; i++) growing = growing && vs[i] > vs[i - 1u];
    CHECK(growing);
    fude_zoom_mfont_part parts[8];
    const u32 np = fude_zoom_mfont_assembly(&f, fude_zoom_mfont_glyph(&f, '('), true, parts, 8u);
    b8 extender = false;
    for(u32 i = 0; i < np; i++) extender = extender || parts[i].extender;
    CHECK(np >= 2u && extender && fude_zoom_mfont_min_overlap(&f) > 0);
    CHECK(fude_zoom_mfont_assembly(&f, A, true, parts, 8u) == 0u && fude_zoom_mfont_variants(&f, A, true, vg, vs, 32u) == 0u);
    // outlines
    rde_arr pts = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std()), ends = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    const u32 o = fude_zoom_mfont_glyph(&f, 'o'), B = fude_zoom_mfont_glyph(&f, 'B');
    CHECK(fude_zoom_mfont_outline(&f, o, 2.0, &pts, &ends) == 2u);
    rde_arr_clear(&pts); rde_arr_clear(&ends);
    CHECK(fude_zoom_mfont_outline(&f, B, 2.0, &pts, &ends) == 3u);
    fude_zoom_mfont_box(&f, B, box);
    b8 inside = rde_arr_length(&pts) > 20u;
    const f64* p = (const f64*)pts.memory;
    for(u32 i = 0; i + 1u < (u32)rde_arr_length(&pts); i += 2u) inside = inside && p[i] >= box[0] - 1.0 && p[i] <= box[2] + 1.0 && p[i + 1u] >= box[1] - 1.0 && p[i + 1u] <= box[3] + 1.0;
    CHECK(inside);
    // (finer: more points, the same contours)
    const u32 coarse = (u32)rde_arr_length(&pts);
    rde_arr_clear(&pts); rde_arr_clear(&ends);
    CHECK(fude_zoom_mfont_outline(&f, B, 0.2, &pts, &ends) == 3u && (u32)rde_arr_length(&pts) > coarse);
    rde_arr_free(&pts); rde_arr_free(&ends);
    // (a damaged font: none; a short one read as zeros, never past its end)
    fude_zoom_mfont bad;
    CHECK(!fude_zoom_mfont_load(&bad, bytes, 40u) && fude_zoom_mfont_glyph(&bad, 'A') == 0u);
    u8 junk[64]; memset(junk, 0xFF, sizeof junk);
    CHECK(!fude_zoom_mfont_load(&bad, junk, sizeof junk));
    CHECK(!fude_zoom_mfont_load(&bad, NULL, size) && bad.size == 0u && fude_zoom_mfont_glyph(&bad, 'A') == 0u);   // (nothing read)
    // Offsets near 2^32, as a damaged font's may be, where an offset plus a length wraps round to a small number: read
    // as zeros (no glyph), never 4 GB past the bytes.
    {
        u8* dmg = (u8*)malloc(size);
        memcpy(dmg, bytes, size);
        u32 cmap = 0, head = 0, loca = 0;
        const u32 tables = (u32)dmg[4] << 8 | dmg[5];
        for(u32 i = 0; i < tables; i++) {
            const u8* r = dmg + 12u + 16u * i;
            const u32 off = (u32)r[8] << 24 | (u32)r[9] << 16 | (u32)r[10] << 8 | r[11];
            if(memcmp(r, "cmap", 4) == 0) cmap = off;
            if(memcmp(r, "head", 4) == 0) head = off;
            if(memcmp(r, "loca", 4) == 0) loca = off;
        }
        CHECK(cmap != 0u && head != 0u && loca != 0u);
        // Every character map's subtable said to be at 0xFFFFFFFE.
        const u32 maps = (u32)dmg[cmap + 2u] << 8 | dmg[cmap + 3u];
        for(u32 i = 0; i < maps; i++) {
            const u32 v = 0xFFFFFFFEu - cmap;
            u8* w = dmg + cmap + 8u + 8u * i;
            w[0] = (u8)(v >> 24); w[1] = (u8)(v >> 16); w[2] = (u8)(v >> 8); w[3] = (u8)v;
        }
        fude_zoom_mfont d;
        CHECK(fude_zoom_mfont_load(&d, dmg, size) && d.cmap4 == 0u && d.cmap12 == 0u && fude_zoom_mfont_glyph(&d, 'A') == 0u);
        // Glyph 1's end in loca (made 32-bit) near 2^32: no glyph, rather than one whose bytes run round to the start.
        dmg[head + 50u] = 0u; dmg[head + 51u] = 1u;
        memset(dmg + loca + 4u, 0, 4u);
        dmg[loca + 8u] = 0xFFu; dmg[loca + 9u] = 0xFFu; dmg[loca + 10u] = 0xFFu; dmg[loca + 11u] = 0xF0u;
        CHECK(fude_zoom_mfont_load(&d, dmg, size) && d.long_loca);
        i32 db[4] = { 1, 1, 1, 1 };
        CHECK(!fude_zoom_mfont_box(&d, 1u, db) && db[0] == 0 && db[2] == 0);
        rde_arr dp = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std()), de = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        CHECK(fude_zoom_mfont_outline(&d, 1u, 2.0, &dp, &de) == 0u && rde_arr_length(&dp) == 0u);
        rde_arr_free(&dp); rde_arr_free(&de);
        free(dmg);
    }
    free(bytes);
}

static const fude_zoom_tex_glyph* tex_glyphs(const fude_zoom_tex_box* b) { return (const fude_zoom_tex_glyph*)b->glyphs.memory; }
static u32 tex_count(const fude_zoom_tex_box* b) { return (u32)rde_arr_length(&b->glyphs); }
static void tex(const fude_zoom_mfont* f, const c8* s, b8 display, fude_zoom_tex_box* b) { fude_zoom_tex_set(f, s, (u32)strlen(s), display, b); }

// LaTeX set as TeX sets it: TeX's spaces between atoms (an operation's medium, a relation's thick, none for a sign or
// in scripts); fractions over and under the axis, smaller in text; roots over what they hold; scripts up and down by
// the font's shifts, apart by its least gap; limits above and below in display, beside in text; brackets grown to what
// they hold; matrices' rows and columns; the alphabets and primes; what it does not know set as written and counted —
// and nothing it is given, however broken, past its memory (a thousand random formulas).
static void test_tex(void) {
    printf("tex\n");
    u32 size = 0;
    u8* bytes = maths_font_bytes(&size);
    CHECK(bytes != NULL);
    if(bytes == NULL) return;
    fude_zoom_mfont f;
    fude_zoom_mfont_load(&f, bytes, size);
    const f64 em = 1000.0;
    #define ADV(cp) (fude_zoom_mfont_advance(&f, fude_zoom_mfont_glyph(&f, (cp))) / em)
    fude_zoom_tex_box b; fude_zoom_tex_box_init(&b);
    tex(&f, "x", false, &b);
    CHECK(tex_count(&b) == 1u && tex_glyphs(&b)[0].glyph == fude_zoom_mfont_glyph(&f, 0x1D465) && fabs(b.width - ADV(0x1D465)) < 1e-9 && b.unknown == 0u);
    // spaces: a + b (medium either side), a = b (thick), -x (a sign: none), x^{a+b} (scripts: none)
    tex(&f, "a+b", false, &b);
    const f64 apb = ADV(0x1D44E) + ADV('+') + ADV(0x1D44F);
    CHECK(tex_count(&b) == 3u && fabs(b.width - (apb + 2.0 * 4.0 / 18.0)) < 1e-6);
    tex(&f, "a=b", false, &b);
    CHECK(fabs(b.width - (ADV(0x1D44E) + ADV('=') + ADV(0x1D44F) + 2.0 * 5.0 / 18.0)) < 1e-6);
    tex(&f, "-x", false, &b);
    CHECK(tex_glyphs(&b)[0].glyph == fude_zoom_mfont_glyph(&f, 0x2212) &&
          fabs(b.width - (ADV(0x2212) + ADV(0x1D465) + fude_zoom_mfont_italic(&f, fude_zoom_mfont_glyph(&f, 0x1D465)) / em)) < 1e-6);   // (the x's slant after it, as TeX adds)
    tex(&f, "x^{a+b}", false, &b);
    const fude_zoom_tex_glyph* g = tex_glyphs(&b);
    CHECK(tex_count(&b) == 4u && fabs(g[1].size - 0.6) < 1e-9 && fabs((g[3].x - g[1].x) - 0.6 * (ADV(0x1D44E) + ADV('+'))) < 1e-6);   // (in a script: no space round +)
    // scripts
    tex(&f, "x^2", false, &b);
    CHECK(tex_count(&b) == 2u && tex_glyphs(&b)[1].y > 0.3 && tex_glyphs(&b)[1].size < 1.0);
    tex(&f, "x_i", false, &b);
    CHECK(tex_glyphs(&b)[1].y < -0.1);
    tex(&f, "x_i^2", false, &b);
    g = tex_glyphs(&b);
    {
        i32 sup_box[4], sub_box[4];
        fude_zoom_mfont_box(&f, g[1].glyph, sub_box); fude_zoom_mfont_box(&f, g[2].glyph, sup_box);
        const u32 sup = g[1].y > g[2].y ? 1u : 2u, sub = 3u - sup;
        fude_zoom_mfont_box(&f, g[sup].glyph, sup_box); fude_zoom_mfont_box(&f, g[sub].glyph, sub_box);
        const f64 gap = (g[sup].y + sup_box[1] / em * g[sup].size) - (g[sub].y + sub_box[3] / em * g[sub].size);
        CHECK(gap >= fude_zoom_mfont_constant(&f, FUDE_ZOOM_MATH_SUB_SUPERSCRIPT_GAP_MIN) / em - 1e-9);
    }
    // fractions: one rule on the axis, the numerator over it, the denominator under; in text smaller
    tex(&f, "\\frac{a}{b}", true, &b);
    const f64 axis = fude_zoom_mfont_constant(&f, FUDE_ZOOM_MATH_AXIS_HEIGHT) / em;
    const fude_zoom_tex_rule* r = (const fude_zoom_tex_rule*)b.rules.memory;
    CHECK(rde_arr_length(&b.rules) == 1u && fabs(r[0].y + r[0].h * 0.5 - axis) < 1e-9 && tex_glyphs(&b)[0].y > axis && tex_glyphs(&b)[1].y < axis);
    const f64 display_h = b.height + b.depth;
    tex(&f, "\\frac{a}{b}", false, &b);
    CHECK(b.height + b.depth < display_h && tex_glyphs(&b)[0].size < 1.0);
    tex(&f, "\\binom{n}{k}", true, &b);
    CHECK(rde_arr_length(&b.rules) == 0u && tex_count(&b) == 4u);   // (no rule; two parentheses)
    // roots: a rule over what it holds, the sign as tall
    tex(&f, "\\sqrt{x}", true, &b);
    r = (const fude_zoom_tex_rule*)b.rules.memory;
    CHECK(rde_arr_length(&b.rules) == 1u && r[0].y > 0.4 && b.height > r[0].y + r[0].h - 1e-9);
    tex(&f, "\\sqrt{\\frac{a}{b}}", true, &b);
    CHECK(tex_glyphs(&b)[0].glyph != fude_zoom_mfont_glyph(&f, 0x221A));   // (a bigger form)
    tex(&f, "\\sqrt[3]{x}", true, &b);
    CHECK(tex_count(&b) == 3u && tex_glyphs(&b)[0].size < 0.6);   // (its degree, scriptscript)
    // limits: above and below in display, beside in text; an integral's beside even in display
    tex(&f, "\\sum_{i=1}^{n} i", true, &b);
    g = tex_glyphs(&b);
    {
        const fude_zoom_tex_glyph* sum = &g[0];
        b8 above = false, below = false;
        for(u32 i = 1; i < tex_count(&b); i++) { above = above || (g[i].size < 1.0 && g[i].y > 0.5); below = below || (g[i].size < 1.0 && g[i].y < -0.5); }
        CHECK(above && below && sum->size == 1.0);
    }
    tex(&f, "\\sum_{i=1}^{n} i", false, &b);
    g = tex_glyphs(&b);
    CHECK(g[1].x > ADV(0x2211) * 0.9);   // (n beside it)
    tex(&f, "\\int_0^1", true, &b);
    g = tex_glyphs(&b);
    CHECK(tex_count(&b) == 3u && g[1].x > 0.0 && g[2].x > 0.0 && g[0].glyph != fude_zoom_mfont_glyph(&f, 0x222B));   // (its display form; limits beside)
    tex(&f, "\\lim_{x \\to 0} x", true, &b);
    CHECK(tex_glyphs(&b)[3].y < -0.3);   // (x → 0 under lim)
    // brackets grown to what they hold
    tex(&f, "\\left( \\frac{a}{b} \\right)", true, &b);
    g = tex_glyphs(&b);
    CHECK(g[0].glyph != fude_zoom_mfont_glyph(&f, '(') && b.height + b.depth > display_h * 0.9);
    tex(&f, "\\left( \\sum_{k=1}^{n} \\frac{a_k}{b_k} \\begin{matrix} 1 \\\\ 2 \\\\ 3 \\\\ 4 \\\\ 5 \\\\ 6 \\end{matrix} \\right)", true, &b);
    CHECK(b.height + b.depth > 5.0 && b.unknown == 0u);   // (past its biggest form: built of its pieces)
    // matrices: 2 × 2, its rows apart, its columns apart
    tex(&f, "\\begin{pmatrix} a & b \\\\ c & d \\end{pmatrix}", true, &b);
    g = tex_glyphs(&b);
    {
        f64 ya = 0, yc = 0, xa = 0, xb = 0;
        for(u32 i = 0; i < tex_count(&b); i++) {
            if(g[i].glyph == fude_zoom_mfont_glyph(&f, 0x1D44E)) { ya = g[i].y; xa = g[i].x; }
            if(g[i].glyph == fude_zoom_mfont_glyph(&f, 0x1D44F)) { xb = g[i].x; }
            if(g[i].glyph == fude_zoom_mfont_glyph(&f, 0x1D450)) { yc = g[i].y; }
        }
        CHECK(tex_count(&b) == 6u && ya - yc > 1.0 && xb - xa > 0.8 && b.unknown == 0u);
    }
    tex(&f, "\\begin{aligned} x &= 1 \\\\ y &= 2 \\end{aligned}", true, &b);
    CHECK(tex_count(&b) == 6u && b.unknown == 0u);
    tex(&f, "f(x) = \\begin{cases} 1 & x > 0 \\\\ 0 & \\text{else} \\end{cases}", true, &b);
    CHECK(b.unknown == 0u && tex_count(&b) > 12u);
    // alphabets, Greek, primes
    tex(&f, "\\mathbb{R} \\mathcal{L} \\mathbf{v} \\alpha \\Gamma f'", true, &b);
    g = tex_glyphs(&b);
    CHECK(g[0].glyph == fude_zoom_mfont_glyph(&f, 0x211D) && g[1].glyph == fude_zoom_mfont_glyph(&f, 0x2112) && g[2].glyph == fude_zoom_mfont_glyph(&f, 0x1D42F) &&
          g[3].glyph == fude_zoom_mfont_glyph(&f, 0x1D6FC) && g[4].glyph == fude_zoom_mfont_glyph(&f, 0x393) && g[6].glyph == fude_zoom_mfont_glyph(&f, 0x2032));
    tex(&f, "\\text{if } x", true, &b);
    CHECK(tex_glyphs(&b)[0].glyph == fude_zoom_mfont_glyph(&f, 'i') && tex_count(&b) == 3u);
    tex(&f, "\\not= \\not\\in", true, &b);
    CHECK(tex_glyphs(&b)[0].glyph == fude_zoom_mfont_glyph(&f, 0x2260) && tex_glyphs(&b)[1].glyph == fude_zoom_mfont_glyph(&f, 0x2209));
    // what it does not know: set as written, counted
    tex(&f, "\\foo + x", true, &b);
    CHECK(b.unknown == 1u && tex_count(&b) >= 6u);
    tex(&f, "{x", true, &b);
    CHECK(b.unknown >= 1u && tex_count(&b) == 1u);
    tex(&f, "x}}", true, &b);
    CHECK(b.unknown >= 1u && tex_count(&b) == 1u);
    tex(&f, "", true, &b);
    CHECK(tex_count(&b) == 0u && b.width == 0.0);
    // a thousand random formulas: never past its memory, its box always finite
    static const c8* const bits[] = { "x", "^", "_", "{", "}", "\\frac", "\\sqrt", "[", "]", "\\left(", "\\right)", "\\begin{pmatrix}", "\\end{pmatrix}", "&", "\\\\",
                                      "\\sum", "\\int", "\\hat", "\\text{a b}", "\\mathbb", "'", "+", "=", "\\alpha", "\\left.", "\\right|", "\\overbrace", "\\big(", "1", " " };
    b8 finite = true;
    for(u32 k = 0; k < 1000u; k++) {
        c8 s[256]; u32 n = 0;
        const u32 parts = 1u + rnd() % 24u;
        for(u32 i = 0; i < parts && n + 24u < sizeof s; i++) { const c8* bit = bits[rnd() % (sizeof(bits) / sizeof(bits[0]))]; const usize l = strlen(bit); memcpy(s + n, bit, l); n += (u32)l; }
        s[n] = 0;
        fude_zoom_tex_set(&f, s, n, (k & 1u) != 0u, &b);
        finite = finite && isfinite(b.width) && isfinite(b.height) && isfinite(b.depth) && b.width < 1000.0 && b.height < 1000.0;
    }
    CHECK(finite);
    #undef ADV
    fude_zoom_tex_box_free(&b);
    free(bytes);
}

// A random sum as people write them (_depth deep at most), appended to _out at *_n.
static void random_sum(u32* seed, u32 depth, c8* out, usize* n, usize size) {
    #define RS_PUT(str) do { const c8* s_ = (str); const usize l_ = strlen(s_); if(*n + l_ + 1u < size) { memcpy(out + *n, s_, l_); *n += l_; } } while(0)
    *seed = *seed * 1664525u + 1013904223u;
    const u32 r = (*seed >> 8) % (depth == 0u ? 5u : 11u);
    static const c8* atoms[] = { "x", "2", "0.5", "pi", "a" };
    static const c8* ops[] = { " + ", " - ", "*", "/", "^" };
    static const c8* calls[] = { "sin(", "sqrt(", "abs(", "ln(" };
    if(r < 5u) {
        RS_PUT(atoms[r]);
    } else if(r < 8u) {
        random_sum(seed, depth - 1u, out, n, size);
        *seed = *seed * 1664525u + 1013904223u;
        RS_PUT(ops[(*seed >> 8) % 5u]);
        RS_PUT("(");
        random_sum(seed, depth - 1u, out, n, size);
        RS_PUT(")");
    } else if(r == 8u) {
        RS_PUT("-(");
        random_sum(seed, depth - 1u, out, n, size);
        RS_PUT(")");
    } else if(r == 9u) {
        *seed = *seed * 1664525u + 1013904223u;
        RS_PUT(calls[(*seed >> 8) % 4u]);
        random_sum(seed, depth - 1u, out, n, size);
        RS_PUT(")");
    } else {
        RS_PUT("3");
        random_sum(seed, depth - 1u, out, n, size);   // (side by side: 3x, 3sin(x), 3(…))
    }
    #undef RS_PUT
}

// LaTeX worked out (texcalc.h): fractions, roots with degrees, powers, \cdot and \times, \pi, functions and \left(
// \right) as calc's words; what calc does not know refused; a sum ending in "=" answered, short; and a formula so
// answered set with its answer after it.
static void test_texcalc(void) {
    printf("texcalc\n");
    c8 out[256];
    #define TC(tex, want) do { const b8 ok_ = fude_zoom_texcalc((tex), (u32)strlen(tex), out, sizeof out); if(!ok_ || strcmp(out, (want)) != 0) printf("  %s -> %s (want %s)\n", (tex), ok_ ? out : "(refused)", (want)); CHECK(ok_ && strcmp(out, (want)) == 0); } while(0)
    TC("\\frac{3}{4}", "((3)/(4))");
    TC("x^{2} + 1", "x^(2) + 1");
    TC("2^3", "2^(3)");
    TC("\\sqrt{2}", "sqrt(2)");
    TC("\\sqrt[3]{8}", "((8)^(1/(3)))");
    TC("3 \\cdot 4 \\times 5 \\div 6", "3 * 4 * 5 / 6");
    TC("\\sin x + \\cos(\\pi)", "sin  x + cos (pi)");
    TC("\\left( 1 + 2 \\right)^{2}", "( 1 + 2 )^(2)");
    TC("y = \\sqrt{x}", "y = sqrt(x)");
    TC("y \\le x^2", "y <= x^(2)");
    TC("\\left. x \\right|", " x |");
    #undef TC
    const c8* refused[] = { "\\infty", "x_1", "n!", "\\sum_{i=1}^{n} i", "a \\\\ b", "\\frac{1}{", "{x", "x}", "\\alpha", "f'(x)" };
    for(u32 i = 0; i < sizeof(refused) / sizeof(refused[0]); i++) {
        CHECK(!fude_zoom_texcalc(refused[i], (u32)strlen(refused[i]), out, sizeof out));
    }
    CHECK(!fude_zoom_texcalc("\\frac{1}{2}", 11u, out, 8u));   // (no room: refused, nothing past it)
    #define TA(tex, want) do { const b8 ok_ = fude_zoom_texcalc_answer((tex), (u32)strlen(tex), out, sizeof out); if(!ok_ || strcmp(out, (want)) != 0) printf("  %s -> %s (want %s)\n", (tex), ok_ ? out : "(none)", (want)); CHECK(ok_ && strcmp(out, (want)) == 0); } while(0)
    TA("\\frac{3}{4} + 2^3 =", "8.75");
    TA("1/3 =", "0.3333333333");
    TA("\\sqrt{2} =  ", "1.414213562");
    TA("\\sqrt[3]{27} - 5 =", "-2");
    TA("\\sin(\\pi) =", "0");
    TA("0.1 + 0.2 =", "0.3");
    TA("2\\pi =", "6.283185307");
    TA("10^{20} =", "1e+20");
    #undef TA
    // Tidied for a graph: as few brackets as its order needs, each side of its relation.
    #define TT(tex, want) do { const b8 ok_ = fude_zoom_texcalc_tidy((tex), (u32)strlen(tex), out, sizeof out); if(!ok_ || strcmp(out, (want)) != 0) printf("  %s -> %s (want %s)\n", (tex), ok_ ? out : "(refused)", (want)); CHECK(ok_ && strcmp(out, (want)) == 0); } while(0)
    TT("y = \\frac{x^{2}}{4} - 1", "y = x^2/4 - 1");
    TT("x^{2} + y^{2} = 9", "x^2 + y^2 = 9");
    TT("y = 2\\sin(3x)", "y = 2sin(3x)");
    TT("y = -\\left(x + 1\\right)^{2}", "y = -(x + 1)^2");
    TT("y \\le \\frac{\\pi}{2}", "y <= pi/2");
    TT("y = \\frac{1}{x + 1}", "y = 1/(x + 1)");
    TT("y = \\frac{a - b}{c d}", "y = (a - b)/(c*d)");
    TT("y = 2^{-x}", "y = 2^(-x)");
    TT("y = (2^{x})^{3}", "y = (2^x)^3");
    TT("y = 1 - (x - 1)", "y = 1 - (x - 1)");
    TT("r = 1 + \\cos(\\theta)", "r = 1 + cos(x)");
    TT("\\sqrt[3]{x}", "x^(1/3)");
    #undef TT
    // (written back, read again: the same everywhere — a thousand random sums)
    {
        u32 seed = 77u, same = 0, tried = 0;
        for(u32 g = 0; g < 1000u; g++) {
            c8 text[512];
            usize n = 0;
            random_sum(&seed, 4u, text, &n, sizeof text);
            text[n] = 0;
            fude_zoom_calc c1, c2;
            if(!fude_zoom_calc_parse_expr(&c1, text)) { printf("  %s: not read\n", text); continue; }
            c8 back[512];
            tried++;
            if(!fude_zoom_calc_write(&c1, back, sizeof back) || !fude_zoom_calc_parse_expr(&c2, back)) { printf("  %s -> %s: not read again\n", text, back); continue; }
            f64 vars[26]; for(u32 i = 0; i < 26u; i++) vars[i] = 0.7 + 0.1 * i;
            b8 ok = c1.count == c2.count;
            for(u32 i = 0; i < 5u; i++) {
                const f64 x = -2.3 + 1.1 * i, v1 = fude_zoom_calc_at_with(&c1, x, vars), v2 = fude_zoom_calc_at_with(&c2, x, vars);
                ok = ok && ((isnan(v1) && isnan(v2)) || v1 == v2 || fabs(v1 - v2) <= 1e-9 * fmax(1.0, fabs(v1)));
            }
            if(!ok) printf("  %s -> %s: not the same\n", text, back);
            same += ok ? 1u : 0u;
        }
        printf("  %u of %u random sums the same written back\n", same, tried);
        CHECK(tried == 1000u && same == tried);
    }
    const c8* unanswered[] = { "x + 1 =", "1 + 1", "a = 1 + 1 =", "1 / 0 =", "=", "\\infty =", "y < 2 =" };
    for(u32 i = 0; i < sizeof(unanswered) / sizeof(unanswered[0]); i++) {
        CHECK(!fude_zoom_texcalc_answer(unanswered[i], (u32)strlen(unanswered[i]), out, sizeof out));
    }
    // calc's words as LaTeX (a graph's legend): fractions, powers, calls, brackets only where its order needs them.
    #define TL(src, xname, want) do { fude_zoom_calc c_; const b8 ok_ = fude_zoom_calc_parse_expr(&c_, (src)) && fude_zoom_calc_latex(&c_, (xname), out, sizeof out); if(!ok_ || strcmp(out, (want)) != 0) printf("  %s -> %s (want %s)\n", (src), ok_ ? out : "(refused)", (want)); CHECK(ok_ && strcmp(out, (want)) == 0); } while(0)
    TL("x^2/4 - 1", "x", "\\frac{x^{2}}{4} - 1");
    TL("2sin(3x)", "x", "2 \\sin\\left(3 x\\right)");
    TL("sqrt(abs(x))", "x", "\\sqrt{\\left|x\\right|}");
    TL("-(x + 1)^2", "x", "-\\left(x + 1\\right)^{2}");
    TL("2*3", "x", "2 \\cdot 3");
    TL("(sin(x))^2", "x", "\\left(\\sin x\\right)^{2}");
    TL("exp(-x)", "x", "e^{-x}");
    TL("1 + cos(t)", "\\theta", "1 + \\cos \\theta");
    TL("x pi / 2", "x", "\\frac{x \\pi}{2}");
    TL("1.5e20 x", "x", "1.5 \\times 10^{20} x");
    TL("a - (b - c)", "x", "a - \\left(b - c\\right)");
    TL("(1/2)^x", "x", "\\left(\\frac{1}{2}\\right)^{x}");
    #undef TL
    // A graph's legend: each line's LaTeX.
    {
        rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
        rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), heap), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), heap), labels = rde_arr_new(sizeof(fude_zoom_plot_label), heap);
        fude_zoom_plot_lines("graph", "x^2 + y^2 = 4\nr = 1 + cos(θ)\n(cos(t), sin(t))\ny >= x\nf(x) = x/2\nx^2 - 1\na = 2\narea 0 1", 300.0, 220.0, &pts, &parts, &labels);
        const c8* want[] = { "x^{2} + y^{2} = 4", "r = 1 + \\cos \\theta", "\\left(\\cos t, \\sin t\\right)", "y \\ge x", "f(x) = \\frac{x}{2}", "y = x^{2} - 1",
                             "a = 2", "\\int_{0}^{1} y \\, dx = 0" };
        const fude_zoom_plot_label* lb = (const fude_zoom_plot_label*)labels.memory;
        for(u32 w = 0; w < sizeof(want) / sizeof(want[0]); w++) {
            b8 found = false;
            for(u32 i = 0; i < (u32)rde_arr_length(&labels); i++) found = found || (lb[i].align == 4u && strncmp(lb[i].latex, want[w], strlen(want[w])) == 0);
            if(!found) printf("  legend lacks %s\n", want[w]);
            CHECK(found);
        }
        rde_arr_free(&pts); rde_arr_free(&parts); rde_arr_free(&labels);
    }
    // Set with its answer: "2 + 2 =" as "2 + 2 = 4" — its glyphs one more than without it.
    c8 path[1024];
    snprintf(path, sizeof path, "%s/../sketching/assets/fonts/NotoSansMath-Regular.ttf", FUDE_ROOT);
    fude_zoom_formulas f;
    fude_zoom_formulas_init(&f);
    if(fude_zoom_formulas_load(&f, path)) {
        const u32 plain = tex_count(fude_zoom_formulas_set(&f, "2 + 2", 5u));
        const fude_zoom_tex_box* answered = fude_zoom_formulas_set(&f, "2 + 2 =", 7u);
        CHECK(plain == 3u && tex_count(answered) == 5u);
        const u32 open = tex_count(fude_zoom_formulas_set(&f, "x + 2 =", 7u));   // (not a sum: as written)
        CHECK(open == 4u);
        // Every random sum's LaTeX set with nothing unknown in it, in text style as a legend is.
        u32 seed = 991u, clean = 0;
        for(u32 g = 0; g < 1000u; g++) {
            c8 text[512], tex[512];
            usize n = 0;
            random_sum(&seed, 4u, text, &n, sizeof text);
            text[n] = 0;
            fude_zoom_calc c;
            if(!fude_zoom_calc_parse_expr(&c, text) || !fude_zoom_calc_latex(&c, "x", tex, sizeof tex)) { printf("  %s: no LaTeX\n", text); continue; }
            const fude_zoom_tex_box* set = fude_zoom_formulas_set_as(&f, tex, (u32)strlen(tex), false);
            if(set != NULL && set->unknown == 0u && tex_count(set) > 0u) clean++;
            else printf("  %s -> %s: %u unknown\n", text, tex, set != NULL ? set->unknown : 99u);
        }
        CHECK(clean == 1000u);
    } else {
        CHECK(false);
    }
    fude_zoom_formulas_free(&f);
}

// Formulas written by hand (handmath.h): the shapes known (a maths alphabet's letter as its plain one, the thick
// Hershey ones single strokes), a font's glyphs' characters read back (a bigger bracket a bracket), and formulas
// written out — every glyph a shape, every point near the set formula's box, the same seed the same strokes.
static void test_handmath(void) {
    printf("handmath\n");
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), heap), ends = rde_arr_new(sizeof(u32), heap);
    const u32* cps = NULL;
    CHECK(fude_zoom_handmath_symbols(&cps) >= 170u && cps != NULL);
    for(u32 i = 1; i < fude_zoom_handmath_symbols(&cps); i++) CHECK(cps[i] > cps[i - 1u]);   // (in order)
    CHECK(fude_zoom_handmath_base(0x1D465u) == 'x' && fude_zoom_handmath_base(0x1D6FCu) == 0x03B1u && fude_zoom_handmath_base(0x210Eu) == 'h' &&
          fude_zoom_handmath_base(0x1D7D0u) == '2' && fude_zoom_handmath_base(0x211Du) == 'R' && fude_zoom_handmath_base('+') == '+');
    CHECK(fude_zoom_handmath_shape('x', &pts, &ends) >= 1u);
    const u32 xs = (u32)rde_arr_length(&pts);
    rde_arr_clear(&pts); rde_arr_clear(&ends);
    CHECK(fude_zoom_handmath_shape(0x1D465u, &pts, &ends) >= 1u && (u32)rde_arr_length(&pts) == xs);   // (𝑥 as x)
    rde_arr_clear(&pts); rde_arr_clear(&ends);
    CHECK(fude_zoom_handmath_shape(0x2211u, &pts, &ends) == 1u);   // (∑ one stroke, not Hershey's six)
    rde_arr_clear(&pts); rde_arr_clear(&ends);
    CHECK(fude_zoom_handmath_shape(0x222Cu, &pts, &ends) == 2u);   // (∬ two ∫s)
    rde_arr_clear(&pts); rde_arr_clear(&ends);
    CHECK(fude_zoom_handmath_shape(0x4E00u, &pts, &ends) == 0u && rde_arr_length(&pts) == 0u);
    u32 size = 0;
    u8* bytes = maths_font_bytes(&size);
    CHECK(bytes != NULL);
    if(bytes == NULL) return;
    fude_zoom_mfont f;
    fude_zoom_mfont_load(&f, bytes, size);
    fude_zoom_handmath_chars chars;
    fude_zoom_handmath_chars_init(&chars);
    fude_zoom_handmath_chars_read(&chars, &f);
    CHECK(fude_zoom_handmath_char(&chars, fude_zoom_mfont_glyph(&f, 0x1D465u)) == 'x' && fude_zoom_handmath_char(&chars, fude_zoom_mfont_glyph(&f, 0x2211u)) == 0x2211u);
    u32 vg[16]; i32 vs[16];
    const u32 nv = fude_zoom_mfont_variants(&f, fude_zoom_mfont_glyph(&f, '('), true, vg, vs, 16u);
    CHECK(nv >= 2u && fude_zoom_handmath_char(&chars, vg[nv - 1u]) == '(');
    static const c8* formulas[] = { "x = \\frac{-b \\pm \\sqrt{b^2-4ac}}{2a}", "\\sum_{n=1}^{\\infty} \\frac{1}{n^2} = \\frac{\\pi^2}{6}", "\\int_0^1 e^{-x^2}\\,dx",
                                    "\\left( \\frac{a}{b} \\right)^{2}", "\\vec{F} = m\\vec{a}", "\\forall x \\in \\mathbb{R} : x^2 \\ge 0", "\\alpha\\beta\\gamma \\Delta\\Omega",
                                    "\\oint_C \\nabla \\times \\vec{E} \\cdot d\\vec{l}", "\\{ x : x \\notin \\emptyset \\}", "f'(x) \\approx \\frac{f(x+h)-f(x)}{h}",
                                    "\\begin{pmatrix} a & b \\\\ c & d \\end{pmatrix}", "\\lim_{x \\to 0} \\frac{\\sin x}{x} = 1" };
    fude_zoom_tex_box b; fude_zoom_tex_box_init(&b);
    u32 missing_all = 0;
    for(u32 k = 0; k < sizeof(formulas) / sizeof(formulas[0]); k++) {
        fude_zoom_tex_set(&f, formulas[k], (u32)strlen(formulas[k]), true, &b);
        rde_arr_clear(&pts); rde_arr_clear(&ends);
        u32 missing = 0;
        const u32 n = fude_zoom_handmath_write(&chars, &f, &b, 40.0, 7u, &pts, &ends, &missing);
        CHECK(n > 0u && rde_arr_length(&ends) == n && ((const u32*)ends.memory)[n - 1u] == (u32)rde_arr_length(&pts));
        const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
        b8 inside = true;
        for(u32 i = 0; i < (u32)rde_arr_length(&pts); i++) {
            inside = inside && p[i].x > -0.3 * 40.0 && p[i].x < (b.width + 0.3) * 40.0 && p[i].y > -(b.depth + 0.3) * 40.0 && p[i].y < (b.height + 0.3) * 40.0;
        }
        if(missing > 0u || !inside) printf("  %s: %u missing, %s\n", formulas[k], missing, inside ? "inside" : "outside");
        CHECK(inside);
        missing_all += missing;
    }
    CHECK(missing_all == 0u);
    // (seeded: the same strokes again; another seed, other ones)
    fude_zoom_tex_set(&f, "x^2 + y^2", 9u, true, &b);
    rde_arr again = rde_arr_new(sizeof(fude_zoom_v2), heap), again_ends = rde_arr_new(sizeof(u32), heap);
    rde_arr_clear(&pts); rde_arr_clear(&ends);
    fude_zoom_handmath_write(&chars, &f, &b, 40.0, 3u, &pts, &ends, NULL);
    fude_zoom_handmath_write(&chars, &f, &b, 40.0, 3u, &again, &again_ends, NULL);
    CHECK(rde_arr_length(&pts) == rde_arr_length(&again) && memcmp(pts.memory, again.memory, sizeof(fude_zoom_v2) * rde_arr_length(&pts)) == 0);
    rde_arr_clear(&again); rde_arr_clear(&again_ends);
    fude_zoom_handmath_write(&chars, &f, &b, 40.0, 4u, &again, &again_ends, NULL);
    CHECK(rde_arr_length(&pts) == rde_arr_length(&again) && memcmp(pts.memory, again.memory, sizeof(fude_zoom_v2) * rde_arr_length(&pts)) != 0);
    rde_arr_free(&again); rde_arr_free(&again_ends);
    fude_zoom_tex_box_free(&b);
    fude_zoom_handmath_chars_free(&chars);
    rde_arr_free(&pts); rde_arr_free(&ends);
    free(bytes);
}

// Handwritten maths read (mathread.h): each known symbol's own shape read as itself; formulas written out by hand
// (handmath.h, three hands each) read back as they were set — nine in ten at least —, their layout too (fractions,
// roots, scripts, limits, functions); nothing read from nothing; and a thousand scribbles read
// without going past memory.
static void test_mathread(void) {
    printf("mathread\n");
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), heap), ends = rde_arr_new(sizeof(u32), heap);
    c8 out[1024];
    CHECK(!fude_zoom_mathread(NULL, NULL, 0u, out, sizeof out, NULL));
    // (the shapes themselves)
    static const u32 own[] = { '0', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'd', 'e', 'f', 'g', 'h', 'k', 'm', 'n', 'p', 'q', 'r', 't', 'y',
                               '+', '=', '<', '>', 0x2211u, 0x222Bu, 0x221Eu, 0x03B1u, 0x03B2u, 0x03C0u, 0x03B8u };
    u32 known = 0;
    for(u32 i = 0; i < sizeof(own) / sizeof(own[0]); i++) {
        rde_arr_clear(&pts); rde_arr_clear(&ends);
        const u32 n = fude_zoom_handmath_shape(own[i], &pts, &ends);
        f64 d = 0.0;
        const u32 got = fude_zoom_mathread_symbol((const fude_zoom_v2*)pts.memory, (const u32*)ends.memory, n, &d);
        if(got != own[i]) printf("  U+%04X read as U+%04X (%.2f)\n", own[i], got, d);
        known += got == own[i] ? 1u : 0u;
    }
    CHECK(known == sizeof(own) / sizeof(own[0]));
    // (formulas written by hand, read back)
    u32 size = 0;
    u8* bytes = maths_font_bytes(&size);
    CHECK(bytes != NULL);
    if(bytes == NULL) return;
    fude_zoom_mfont f;
    fude_zoom_mfont_load(&f, bytes, size);
    fude_zoom_handmath_chars chars;
    fude_zoom_handmath_chars_init(&chars);
    fude_zoom_handmath_chars_read(&chars, &f);
    static const c8* set[] = { "a+b=c", "x^{2}+y^{2}=1", "\\frac{1}{2}", "\\sqrt{x+1}", "2x-3=7", "\\frac{a+b}{c}", "x_{1}+x_{2}", "e^{x}", "\\int_{0}^{1}x",
                               "\\alpha+\\beta", "y=\\sin x", "f(x)=x^{2}", "3\\times 4", "\\frac{dy}{dx}=2x", "a^{2}-b^{2}=(a-b)(a+b)", "\\pi r^{2}",
                               "\\lambda\\le\\mu", "\\Delta x\\neq 0", "x^{2}-5x+6=0", "\\frac{x}{y}+\\frac{1}{x}", "2^{n}+1", "y=mx+b", "(a+b)^{2}", "\\frac{\\pi}{4}",
                               "3a-2b=c", "\\theta+\\phi", "F=ma", "E=mc^{2}", "\\Delta y", "a\\cdot b", "x\\ge 1", "\\frac{1}{1+x^{2}}" };
    fude_zoom_tex_box b; fude_zoom_tex_box_init(&b);
    u32 right = 0, all = 0;
    for(u32 k = 0; k < sizeof(set) / sizeof(set[0]); k++) {
        fude_zoom_tex_set(&f, set[k], (u32)strlen(set[k]), false, &b);
        for(u32 seed = 0; seed < 3u; seed++) {
            rde_arr_clear(&pts); rde_arr_clear(&ends);
            const u32 n = fude_zoom_handmath_write(&chars, &f, &b, 40.0, seed * 7u + 1u, &pts, &ends, NULL);
            u32 syms = 0;
            fude_zoom_mathread((const fude_zoom_v2*)pts.memory, (const u32*)ends.memory, n, out, sizeof out, &syms);
            c8 a[256], g[1024];
            usize ai = 0, gi = 0;
            for(const c8* q = set[k]; *q; q++) if(*q != ' ') a[ai++] = *q;
            for(const c8* q = out; *q; q++) if(*q != ' ') g[gi++] = *q;
            a[ai] = 0; g[gi] = 0;
            all++;
            if(strcmp(a, g) == 0) right++;
            else if(seed == 0u) printf("  %s read as %s\n", set[k], out);
        }
    }
    printf("  %u of %u formulas written by hand read back\n", right, all);
    CHECK(right * 10u >= all * 9u);
    fude_zoom_tex_box_free(&b);
    // (Borja's first real formula, √(b²), its strokes traced from his screenshot, 2026-10-11: what it reads now,
    // printed — not checked until the reader is trained on his handwriting: docs/product_split.md §10)
    {
        static const f64 real[][2] = { { 655, 1432 }, { 661, 1444 }, { 668, 1455 }, { 671, 1420 }, { 674, 1380 }, { 677, 1340 }, { 679, 1322 }, { 700, 1321 },
                                       { 740, 1321 }, { 780, 1320 }, { 815, 1318 }, { 838, 1317 }, { 840, 1320 },                                       // √
                                       { 742, 1350 }, { 732, 1365 }, { 724, 1385 }, { 723, 1400 }, { 727, 1418 }, { 735, 1430 }, { 745, 1437 },             // (
                                       { 766, 1352 }, { 766, 1370 }, { 765, 1390 }, { 764, 1418 }, { 764, 1405 }, { 770, 1393 }, { 785, 1388 }, { 795, 1398 },
                                       { 794, 1410 }, { 785, 1417 }, { 766, 1418 },                                                                       // b
                                       { 803, 1357 }, { 810, 1350 }, { 818, 1350 }, { 822, 1358 }, { 818, 1368 }, { 806, 1383 }, { 815, 1384 }, { 824, 1384 }, // 2
                                       { 840, 1348 }, { 850, 1360 }, { 857, 1378 }, { 858, 1395 }, { 852, 1415 }, { 845, 1428 }, { 838, 1437 } };           // )
        static const u32 real_ends[5] = { 13, 20, 31, 39, 46 };
        fude_zoom_v2 rp[46];
        for(u32 i = 0; i < 46u; i++) rp[i] = (fude_zoom_v2){ real[i][0], 2000.0 - real[i][1] };
        fude_zoom_mathread(rp, real_ends, 5u, out, sizeof out, NULL);
        printf("  Borja's first, \\sqrt{(b^{2})}, reads as %s (not checked yet)\n", out);
    }
    // (scribbles)
    u32 seed = 4242u;
    b8 bounded = true;
    for(u32 g = 0; g < 1000u; g++) {
        rde_arr_clear(&pts); rde_arr_clear(&ends);
        seed = seed * 1664525u + 1013904223u;
        const u32 strokes = 1u + (seed >> 8) % 40u;
        for(u32 s = 0; s < strokes; s++) {
            seed = seed * 1664525u + 1013904223u;
            const u32 n = 1u + (seed >> 8) % 30u;
            for(u32 i = 0; i < n; i++) {
                seed = seed * 1664525u + 1013904223u;
                const fude_zoom_v2 v = { (f64)((seed >> 8) % 1000u) * 0.3, (f64)((seed >> 4) % 700u) * 0.3 };
                rde_arr_add(&pts, &v);
            }
            const u32 e = (u32)rde_arr_length(&pts);
            rde_arr_add(&ends, &e);
        }
        out[0] = 0;
        fude_zoom_mathread((const fude_zoom_v2*)pts.memory, (const u32*)ends.memory, strokes, out, sizeof out, NULL);
        bounded = bounded && strlen(out) < sizeof out;
    }
    CHECK(bounded);
    fude_zoom_handmath_chars_free(&chars);
    rde_arr_free(&pts); rde_arr_free(&ends);
    free(bytes);
}

// Shoelace: a ring's signed area (counter-clockwise positive).
static f64 ring_area(const fude_zoom_v2* p, u32 n) {
    f64 a = 0.0;
    for(u32 i = 0; i < n; i++) a += p[i].x * p[(i + 1u) % n].y - p[(i + 1u) % n].x * p[i].y;
    return 0.5 * a;
}

// Formulas on the canvas (formula.h): no font read, nothing set and nothing drawn; a formula set once and kept by its
// words; its box its setting's with the pad round it; a glyph's outline as one ring (its contours joined: an o's hole
// a hole, filled even-odd) or a ring a contour (a cutter's), and its triangles as much area as its ink, cut finer the
// bigger it is on screen, kept; exports writing a formula as its glyphs' outlines — as its LaTeX with no font.
static void test_formulas(void) {
    printf("formulas\n");
    c8 path[1024];
    snprintf(path, sizeof path, "%s/../sketching/assets/fonts/NotoSansMath-Regular.ttf", FUDE_ROOT);
    fude_zoom_formulas f;
    fude_zoom_formulas_init(&f);
    const fude_zoom_v2* t = NULL;
    CHECK(!fude_zoom_formulas_load(&f, "./no/such/font.ttf") && fude_zoom_formulas_set(&f, "x", 1u) == NULL);
    CHECK(fude_zoom_formulas_mesh(&f, 5u, 20.0, &t) == 0u && t == NULL);
    CHECK(fude_zoom_formulas_load(&f, path) && f.ready);
    const fude_zoom_tex_box* a = fude_zoom_formulas_set(&f, "x^2", 3u);
    CHECK(a != NULL && tex_count(a) == 2u && rde_arr_length(&f.boxes) == 1u);
    CHECK(fude_zoom_formulas_set(&f, "x^2", 3u) == a && rde_arr_length(&f.boxes) == 1u);   // (kept: not set again)
    const fude_zoom_tex_box* b = fude_zoom_formulas_set(&f, "\\frac{1}{2}", 11u);
    CHECK(b != NULL && rde_arr_length(&b->rules) == 1u && rde_arr_length(&f.boxes) == 2u);
    f64 w = 0.0, h = 0.0;
    fude_zoom_formulas_size(b, 10.0, &w, &h);
    CHECK(fabs(w - (b->width + 2.0 * FUDE_ZOOM_FORMULA_PAD) * 10.0) < 1e-9 && fabs(h - (b->height + b->depth + 2.0 * FUDE_ZOOM_FORMULA_PAD) * 10.0) < 1e-9);
    CHECK(h > 20.0 && w > 5.0);   // (a fraction: two lines' height)
    fude_zoom_formulas_size(NULL, 10.0, &w, &h);   // (no font: an em, padded)
    CHECK(fabs(w - 12.4) < 1e-9 && fabs(h - 12.4) < 1e-9);
    // outlines
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), heap), rings = rde_arr_new(sizeof(u32), heap);
    const u32 o = fude_zoom_mfont_glyph(&f.font, 'o'), B = fude_zoom_mfont_glyph(&f.font, 'B');
    const u32 apart = fude_zoom_formulas_outline(&f, o, 0.5, true, &pts, &rings);
    CHECK(apart > 20u && rde_arr_length(&rings) == apart);
    const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
    const u32* r = (const u32*)rings.memory;
    u32 inner = 0;
    while(inner < apart && r[inner] == 0u) inner++;
    CHECK(inner < apart && r[apart - 1u] == 1u);
    const f64 outer_a = ring_area(p, inner), inner_a = ring_area(p + inner, apart - inner);
    const f64 ink = fabs(outer_a + inner_a);
    CHECK(outer_a * inner_a < 0.0 && ink > 0.2 * fabs(outer_a) && ink < fabs(outer_a));   // (the hole wound the other way)
    rde_arr_clear(&pts); rde_arr_clear(&rings);
    CHECK(fude_zoom_formulas_outline(&f, o, 0.5, false, &pts, &rings) == apart + 2u);   // (each contour closed on its start)
    b8 one = true;
    for(u32 i = 0; i < (u32)rde_arr_length(&rings); i++) one = one && ((const u32*)rings.memory)[i] == 0u;
    CHECK(one);
    rde_arr_clear(&pts); rde_arr_clear(&rings);
    const u32 b_apart = fude_zoom_formulas_outline(&f, B, 0.5, true, &pts, &rings);
    rde_arr_clear(&pts);
    CHECK(fude_zoom_formulas_outline(&f, B, 0.5, false, &pts, NULL) == b_apart + 4u);   // (three closed, one back along the joins)
    // triangles: an o's ink (its hole left out), finer the bigger, kept
    const u32 fine = fude_zoom_formulas_mesh(&f, o, 1000.0, &t);
    const fude_zoom_v2* kept = t;
    f64 area = 0.0;
    for(u32 i = 0; i < fine; i++) area += fabs(ring_area(&t[3u * i], 3u));
    printf("  o: %u triangles at 1000 points, area %.0f against its outline's %.0f\n", fine, area, ink);
    CHECK(fine > 20u && fabs(area - ink) < 0.02 * ink);
    const u32 coarse = fude_zoom_formulas_mesh(&f, o, 12.0, &t);
    CHECK(coarse > 4u && coarse < fine);
    CHECK(fude_zoom_formulas_mesh(&f, o, 1000.0, &t) == fine && t == kept);
    CHECK(fude_zoom_formulas_mesh(&f, fude_zoom_mfont_glyph(&f.font, ' '), 100.0, &t) == 0u);   // (a space: nothing)
    rde_arr_free(&pts); rde_arr_free(&rings);
    // exports: its glyphs (x, =, the fraction's 1 and 2: four paths, its rule a fifth), never its words
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const c8* words = "x = \\frac{1}{2}";
    const fude_zoom_tex_box* set = fude_zoom_formulas_set(&f, words, (u32)strlen(words));
    fude_zoom_formulas_size(set, 20.0, &w, &h);
    const u32 glyphs = tex_count(set), rules = (u32)rde_arr_length(&set->rules);
    fude_zoom_scene_add_text(&s, s.root, (fude_zoom_place){ { -50, 30 }, 0.0, 1.0 }, 20.0, w, h, FUDE_ZOOM_TEXT_FORMULA, (rde_color){ 10, 20, 30, 255 },
                             (rde_color){ 0, 0, 0, 0 }, words, (u32)strlen(words), 0);
    for(u32 pass = 0; pass < 2u; pass++) {
        fude_zoom_export_formulas(pass == 0u ? &f : NULL);
        fude_bytes svg = fude_bytes_new(4096);
        CHECK(fude_zoom_export_svg(&s, (fude_zoom_v2){ 200, 150 }, (rde_color){ 255, 255, 255, 255 }, &svg));
        rde_arr_add(&svg, &(u8){ 0 });
        const c8* text = (const c8*)svg.memory;
        u32 paths = 0; for(const c8* q = text; (q = strstr(q, "<path")) != NULL; q++) paths++;
        if(pass == 0u) {
            CHECK(glyphs == 4u && rules == 1u && paths == glyphs + rules && strstr(text, "frac") == NULL && strstr(text, "fill=\"#0a141e\"") != NULL);
        } else {
            CHECK(paths == 0u && strstr(text, "frac") != NULL);   // (no font: its words)
        }
        rde_arr_free(&svg);
    }
    fude_zoom_export_formulas(&f);
    fude_bytes pdf = fude_bytes_new(4096);
    CHECK(fude_zoom_export_pdf(&s, (fude_zoom_v2){ 200, 150 }, (rde_color){ 255, 255, 255, 255 }, &pdf) && rde_arr_length(&pdf) > 500u);
    rde_arr_free(&pdf);
    fude_zoom_export_formulas(NULL);
    fude_zoom_scene_destroy(&s);
    fude_zoom_formulas_free(&f);
    CHECK(!f.ready && f.bytes == NULL);
}


// The apps made from the canvas (product.h): Sketching every topic and choice; Notes (InfiNote) writing, PDFs, maths
// and diagrams — no project's topic, part, joint, saw or cut list; Workshop (InfiniWorkshop) the projects, with
// diagrams and Kanban too — no maths. A canvas from another app opens in one of this one's topics; a topic's choices
// are the app's of its own, in its order.
static void test_products(void) {
    printf("products\n");
    // (General's lists, as page.c's FUDE_ZOOM_TOPICS has them)
    static const u8 tools[9] = { 0, 1, 2, 3, 4, 5, 6, 7, FUDE_ZOOM_TOOL_PAGES };
    static const u8 inserts[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 20 };
    static const u8 instruments[15] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
    static const u8 exports[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    u8 out[32];
    const fude_zoom_product* all = fude_zoom_product_of(FUDE_ZOOM_PRODUCT_SKETCHING);
    CHECK(fude_zoom_product_keep(inserts, 16u, all->inserts, out) == 16u && memcmp(out, inserts, 16u) == 0);
    CHECK(fude_zoom_product_keep(instruments, 15u, all->instruments, out) == 15u && fude_zoom_product_keep(exports, 12u, all->exports, out) == 12u);
    fude_zoom_product_set(FUDE_ZOOM_PRODUCT_SKETCHING);
    for(u8 t = 0; t < FUDE_ZOOM_TOPIC_COUNT; t++) CHECK(fude_zoom_product_has_topic(t) && fude_zoom_product_topic(t) == t);

    fude_zoom_product_set(FUDE_ZOOM_PRODUCT_NOTES);
    const fude_zoom_product* notes = fude_zoom_product_get();
    u32 offered = 0;
    for(u8 t = 0; t < FUDE_ZOOM_TOPIC_COUNT; t++) offered |= fude_zoom_product_has_topic(t) ? 1u << t : 0u;
    CHECK(offered == ((1u << FUDE_ZOOM_TOPIC_GENERAL) | (1u << FUDE_ZOOM_TOPIC_PDF) | (1u << FUDE_ZOOM_TOPIC_MATHS) | (1u << FUDE_ZOOM_TOPIC_DIAGRAMS)));
    CHECK(fude_zoom_product_topic(FUDE_ZOOM_TOPIC_ELECTRONICS) == FUDE_ZOOM_TOPIC_GENERAL && fude_zoom_product_topic(FUDE_ZOOM_TOPIC_MATHS) == FUDE_ZOOM_TOPIC_MATHS);
    const u32 n_in = fude_zoom_product_keep(inserts, 16u, notes->inserts, out);
    b8 hobby_in = false, text_in = false;
    for(u32 i = 0; i < n_in; i++) {
        hobby_in = hobby_in || out[i] == FUDE_ZOOM_INSERT_BOARD || out[i] == FUDE_ZOOM_INSERT_HOLES || out[i] == FUDE_ZOOM_INSERT_FINGERS ||
                   out[i] == FUDE_ZOOM_INSERT_DOVETAIL || out[i] == FUDE_ZOOM_INSERT_SHEET || out[i] == FUDE_ZOOM_INSERT_EXAMPLES;
        text_in = text_in || out[i] == FUDE_ZOOM_INSERT_TEXT;
        if(i > 0u) CHECK(out[i] > out[i - 1u]);   // (in the topic's order)
    }
    CHECK(n_in == 10u && !hobby_in && text_in);
    const u32 n_ins = fude_zoom_product_keep(instruments, 15u, notes->instruments, out);
    b8 saw = false;
    for(u32 i = 0; i < n_ins; i++) saw = saw || out[i] == FUDE_ZOOM_TOOLKIT_SAW || out[i] == FUDE_ZOOM_TOOLKIT_TRIM;
    CHECK(n_ins == 11u && !saw);
    const u32 n_out = fude_zoom_product_keep(exports, 12u, notes->exports, out);
    b8 cad = false;
    for(u32 i = 0; i < n_out; i++) cad = cad || out[i] == FUDE_ZOOM_OUT_DXF || out[i] == FUDE_ZOOM_OUT_STL || out[i] == FUDE_ZOOM_OUT_CUT_LIST || out[i] == FUDE_ZOOM_OUT_PARTS;
    CHECK(n_out == 7u && !cad && fude_zoom_product_keep(tools, 9u, notes->tools, out) == 9u && !notes->hobby && !notes->workshop);
    CHECK((notes->tools & ((1u << FUDE_ZOOM_TOOL_CIRCUIT) | (1u << FUDE_ZOOM_TOOL_MOTION) | (1u << FUDE_ZOOM_TOOL_PLAN))) == 0u);

    fude_zoom_product_set(FUDE_ZOOM_PRODUCT_WORKSHOP);
    const fude_zoom_product* shop = fude_zoom_product_get();
    // (graphs too — its Maths topic, called Graphs —, but no formulas)
    CHECK(fude_zoom_product_has_topic(FUDE_ZOOM_TOPIC_MATHS) && fude_zoom_product_has_topic(FUDE_ZOOM_TOPIC_DIAGRAMS) &&
          fude_zoom_product_has_topic(FUDE_ZOOM_TOPIC_ELECTRONICS) && fude_zoom_product_has_topic(FUDE_ZOOM_TOPIC_SEWING) &&
          fude_zoom_product_topic(FUDE_ZOOM_TOPIC_MATHS) == FUDE_ZOOM_TOPIC_MATHS);
    CHECK((shop->inserts & (1u << FUDE_ZOOM_INSERT_MATHS)) != 0u && (shop->inserts & (1u << FUDE_ZOOM_INSERT_FORMULA)) == 0u);
    CHECK((notes->inserts & (1u << FUDE_ZOOM_INSERT_FORMULA)) == 0u);   // (hidden for now: the handwriting reader first)
    const u32 w_in = fude_zoom_product_keep(inserts, 16u, shop->inserts, out);
    b8 board = false, examples = false, kanban = false;
    for(u32 i = 0; i < w_in; i++) {
        board = board || out[i] == FUDE_ZOOM_INSERT_BOARD;
        examples = examples || out[i] == FUDE_ZOOM_INSERT_EXAMPLES;
        kanban = kanban || out[i] == FUDE_ZOOM_INSERT_KANBAN || out[i] == FUDE_ZOOM_INSERT_MERMAID || out[i] == FUDE_ZOOM_INSERT_DIAGRAM;
    }
    CHECK(w_in == 16u && board && examples && kanban && shop->hobby && shop->workshop && fude_zoom_product_keep(tools, 9u, shop->tools, out) == 8u);
    // (every app's first topic its own; none past the last)
    for(u8 k = 0; k < FUDE_ZOOM_PRODUCT_COUNT; k++) {
        fude_zoom_product_set(k);
        CHECK(fude_zoom_product_has_topic(fude_zoom_product_get()->first) && fude_zoom_product_get() == fude_zoom_product_of(k));
    }
    fude_zoom_product_set(200u);
    CHECK(fude_zoom_product_get() == fude_zoom_product_of(FUDE_ZOOM_PRODUCT_SKETCHING));
    // (each app's own names and recordings: what the canvas asks, not which app it is)
    CHECK(fude_zoom_product_of(FUDE_ZOOM_PRODUCT_NOTES)->audio && !fude_zoom_product_of(FUDE_ZOOM_PRODUCT_WORKSHOP)->audio &&
          fude_zoom_product_of(FUDE_ZOOM_PRODUCT_SKETCHING)->audio);
    CHECK(fude_zoom_product_of(FUDE_ZOOM_PRODUCT_WORKSHOP)->general_name != fude_zoom_product_of(FUDE_ZOOM_PRODUCT_NOTES)->general_name);
    printf("  Notes: Insert %u of General's 16, instruments %u of 15, exports %u of 12; Workshop: Insert %u\n", n_in, n_ins, n_out, w_in);
}


// What the page draws each frame from attribute a (as page_circuit.c's logic probes' lights and page_mech.c's bodies'
// marks ask, word for word), as a number: 0, nothing; else what it is drawn as (a light's target and colour, a body's
// target and whether it is fixed).
static u64 attribute_drawn(const fude_zoom_scene* s, u32 a) {
    const fude_zoom_object* ob = fude_zoom_scene_object(s, a);
    rde_color lit;
    if((ob->flags & FUDE_ZOOM_FLAG_ALIVE) && ob->kind == FUDE_ZOOM_KIND_SHAPE && ob->channels == FUDE_ZOOM_SHAPE_PROPS && fude_zoom_props_light(s, a, &lit)) {
        const u32 t = fude_zoom_props_target(s, a);
        const fude_zoom_part* part = t != FUDE_ZOOM_NONE ? fude_zoom_part_of(s, t) : NULL;
        if(part != NULL && part->model == FUDE_ZOOM_MODEL_LOGIC_OUT && !fude_zoom_scene_hides(s, fude_zoom_scene_object(s, t))) {
            return 1ull << 62 | (u64)t << 32 | (u64)lit.r << 24 | (u64)lit.g << 16 | (u64)lit.b << 8 | (u64)lit.a;
        }
    }
    fude_zoom_body_props bp;
    if(fude_zoom_props_kind(s, a) == FUDE_ZOOM_PROPS_BODY && fude_zoom_props_body(s, a, &bp)) {
        const u32 t = fude_zoom_props_target(s, a);
        if(t != FUDE_ZOOM_NONE && !fude_zoom_scene_hides(s, fude_zoom_scene_object(s, t)) && fude_zoom_scene_frame_shown(s, fude_zoom_scene_object(s, t)->frame)) {
            return 2ull << 62 | (u64)t << 32 | (bp.fixed ? 1u : 0u);
        }
    }
    return 0;
}

// A frame's drawing from the attributes: each (object, what it is drawn as) in order, from every object (_list NULL:
// as the page did) or from those _list holds (as it does now). How many into _out (room for every object).
static u32 attributes_drawn(const fude_zoom_scene* s, const rde_arr* list, u64* out) {
    u32 n = 0;
    const u32 count = list != NULL ? (u32)rde_arr_length(list) : fude_zoom_scene_object_count(s);
    for(u32 k = 0; k < count; k++) {
        const u32 a = list != NULL ? ((const u32*)list->memory)[k] : k;
        const u64 d = attribute_drawn(s, a);
        if(d != 0u) {
            out[n++] = (u64)a;
            out[n++] = d;
        }
    }
    return n;
}

// The canvas's attributes gathered once each as objects are made (props.h's fude_zoom_props_gather), which the page
// walks each frame for its logic probes' lights and its bodies' marks instead of every object: what they draw is what
// every object walked draws, in the same order, after each of 600 random changes (strokes and parts drawn, lights set
// and set again, bodies made, limits kept, attributes and probes deleted, undone, redone); the list is every PROPS
// shape and nothing else, each object looked at once. On another scene (fewer objects than looked
// through), it starts again.
static void test_attributes_gathered(void) {
    printf("attributes gathered\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    rde_arr kept = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    u32 looked = 0, probes[64], np = 0, bad = 0, drawn_most = 0, walks = 0;
    u32 seed = 7u;
    #define GATHER_RND(n) ((((seed = seed * 1664525u + 1013904223u) >> 8) % (n)))
    u64* all = malloc(4096u * sizeof(u64));
    u64* some = malloc(4096u * sizeof(u64));
    for(u32 step = 0; step < 600u; step++) {
        const f64 x = (f64)GATHER_RND(2000u), y = (f64)GATHER_RND(2000u);
        switch(GATHER_RND(9u)) {
        case 0: {   // a few strokes (a canvas's most usual objects)
            fude_zoom_qpoint q[3] = { { 0, 0, 0, 0 }, { 40, 10, 0, 0 }, { 80, 0, 0, 0 } };
            for(u32 k = 0; k < 8u; k++) {
                const u32 o = fude_zoom_scene_add_stroke(&s, s.root, (fude_zoom_v2){ x + 10.0 * k, y }, 0, q, 3u, 0u, (rde_color){ 1, 2, 3, 255 }, 2.0f, 0u, 0, 0);
                fude_zoom_history_push(&s, s.root, fude_zoom_scene_object(&s, o)->box, NULL, 0, &o, 1);
            }
            break;
        }
        case 1:   // a logic probe
            if(np < 64u) probes[np++] = part_put(&s, "logic probe", x, y, 20, 20, "Q");
            break;
        case 2: case 3: {   // a light set on a probe (again: the old one erased)
            if(np == 0u) break;
            const u32 p = probes[GATHER_RND(np)];
            if(!alive(&s, p)) break;
            u32 died[1], nd = 0;
            const u32 old = fude_zoom_props_find(&s, p, FUDE_ZOOM_PROPS_LIGHT);
            if(old != FUDE_ZOOM_NONE) { fude_zoom_scene_set_alive(&s, old, false); died[nd++] = old; }
            const u32 born = fude_zoom_props_add_light(&s, p, (rde_color){ (u8)GATHER_RND(256u), 40, 200, 255 });
            fude_zoom_history_push(&s, s.root, fude_zoom_scene_object(&s, p)->box, died, nd, &born, 1);
            break;
        }
        case 4: {   // a drawing made a body (fixed or not), or a part's limits kept
            const f64 rect[3] = { 30, 20, 0 };
            const u32 r = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { x, y }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, rect, 3, (rde_color){ 1, 2, 3, 255 }, 2.0f, 0u, 0);
            const fude_zoom_body_props bp = { 0u, GATHER_RND(2u) == 1u, 0.0, -1.0, -1.0 };
            const u32 born[2] = { r, fude_zoom_props_add_body(&s, r, &bp) };
            fude_zoom_history_push(&s, s.root, fude_zoom_scene_object(&s, r)->box, NULL, 0, born, 2);
            if(GATHER_RND(3u) == 0u) {
                const u32 res = part_put(&s, "resistor", x, y + 50.0, 30, 10, "1k");
                const f64 most[4] = { 0.25, 0.0, 0.0, 0.0 };
                fude_zoom_props_add_limits(&s, res, most, -1);
            }
            break;
        }
        case 5: case 6: {   // an attribute, or a probe, deleted
            const u32 o = GATHER_RND(fude_zoom_scene_object_count(&s));
            if(!alive(&s, o) || (fude_zoom_props_kind(&s, o) == 0u && fude_zoom_part_of(&s, o) == NULL)) break;
            fude_zoom_scene_set_alive(&s, o, false);
            fude_zoom_history_push(&s, s.root, fude_zoom_scene_object(&s, o)->box, &o, 1, NULL, 0);
            break;
        }
        case 7:
            fude_zoom_history_undo(&s);
            break;
        default:
            fude_zoom_history_redo(&s);
            break;
        }
        fude_zoom_props_gather(&s, &looked, &kept);
        // (every PROPS shape, in order, each once)
        u32 m = 0;
        b8 same = looked == fude_zoom_scene_object_count(&s);
        for(u32 o = 0; o < fude_zoom_scene_object_count(&s) && same; o++) {
            const fude_zoom_object* ob = fude_zoom_scene_object(&s, o);
            if(ob->kind == FUDE_ZOOM_KIND_SHAPE && ob->channels == FUDE_ZOOM_SHAPE_PROPS) {
                same = m < (u32)rde_arr_length(&kept) && ((const u32*)kept.memory)[m] == o;
                m++;
            }
        }
        same = same && m == (u32)rde_arr_length(&kept);
        const u32 na = attributes_drawn(&s, NULL, all), nk = attributes_drawn(&s, &kept, some);
        same = same && na == nk && memcmp(all, some, na * sizeof(u64)) == 0;
        bad += same ? 0u : 1u;
        drawn_most = na / 2u > drawn_most ? na / 2u : drawn_most;
        walks++;
    }
    #undef GATHER_RND
    printf("  %u changes: %u objects, %u of them attributes (walked instead), %u drawn at most; %u frames not as before\n", walks, fude_zoom_scene_object_count(&s),
           (u32)rde_arr_length(&kept), drawn_most, bad);
    CHECK(bad == 0u && drawn_most > 5u && (u32)rde_arr_length(&kept) * 2u < fude_zoom_scene_object_count(&s));
    // Another scene, with fewer objects than were looked through: gathered from its first again.
    {
        fude_zoom_scene t; fude_zoom_scene_init(&t, 7);
        const u32 p = part_put(&t, "logic probe", 0, 0, 20, 20, "Q");
        const u32 l = fude_zoom_props_add_light(&t, p, (rde_color){ 0, 200, 0, 255 });
        fude_zoom_props_gather(&t, &looked, &kept);
        CHECK(looked == fude_zoom_scene_object_count(&t) && rde_arr_length(&kept) == 1u && ((const u32*)kept.memory)[0] == l);
        const u32 na = attributes_drawn(&t, NULL, all), nk = attributes_drawn(&t, &kept, some);
        CHECK(na == 2u && na == nk && memcmp(all, some, na * sizeof(u64)) == 0);
        fude_zoom_scene_destroy(&t);
    }
    free(all);
    free(some);
    rde_arr_free(&kept);
    fude_zoom_scene_destroy(&s);
}

// The wires' currents with the circuit's scratch kept (circuit.h's wire_inject and the others, cleared and sized at
// each call, zeroed as new arrays were): after a second and a half of frames, each worked out after each advance on
// that scratch (dirtied first: longer than needed, every byte 0xAB), every piece's current is bit for bit what a
// circuit stepped the same and working them out once, on new scratch, has. On the big examples. The time: the wires'
// currents worked out 2,000 times, the scratch kept, against new arrays each time (as before); and how the stamps'
// finds stand to the steps (circuit_stamp.c's fzc_entry: replaying them was measured, and did not pay).
static void test_wire_scratch(void) {
    printf("wire currents, scratch kept\n");
    static const u32 examples[2] = { FUDE_ZOOM_EXAMPLE_EVERYTHING, FUDE_ZOOM_EXAMPLE_TRAFFIC };
    for(u32 e = 0; e < 2u; e++) {
        fude_zoom_scene s; fude_zoom_circuit a, b;
        example_open(&s, &a, examples[e]);
        fude_zoom_circuit_init(&b);
        CHECK(fude_zoom_circuit_build(&b, &s) > 0u);
        rde_arr* dirty[7] = { &a.wire_inject, &a.wire_parent, &a.wire_order, &a.wire_seen, &a.wire_count, &a.wire_adj, &a.wire_fill };
        for(u32 k = 0; k < 7u; k++) memset(rde_arr_add_n(dirty[k], 5000u), 0xAB, 5000u * dirty[k]->data_type_size);
        b8 ok = true;
        for(u32 f = 0; f < 90u; f++) {
            ok = ok && fude_zoom_circuit_advance(&a, 1.0 / 60.0, 4000u, true) && fude_zoom_circuit_advance(&b, 1.0 / 60.0, 4000u, false);
        }
        ok = ok && fude_zoom_circuit_advance(&b, 0.0, 0u, true);   // (its wires' currents: once, on new scratch)
        const u32 nw = (u32)rde_arr_length(&a.wires);
        const fude_zoom_circuit_wire* wa = (const fude_zoom_circuit_wire*)a.wires.memory;
        const fude_zoom_circuit_wire* wb = (const fude_zoom_circuit_wire*)b.wires.memory;
        u32 differ = 0, pieces = 0, carrying = 0;
        for(u32 i = 0; i < nw && nw == (u32)rde_arr_length(&b.wires); i++) {
            differ += memcmp(wa[i].current, wb[i].current, sizeof(wa[i].current)) != 0 ? 1u : 0u;
            for(u32 k = 0; k < wa[i].pieces; k++) { pieces++; carrying += fabs(wa[i].current[k]) > 1e-9 ? 1u : 0u; }
        }
        CHECK(ok && nw > 40u && nw == (u32)rde_arr_length(&b.wires) && differ == 0u && carrying > 10u);
        CHECK(rde_arr_length(&a.v) == rde_arr_length(&b.v) && memcmp(a.v.memory, b.v.memory, rde_arr_length(&a.v) * sizeof(f64)) == 0);
        // (timed: the wires' currents alone, the scratch kept, then new arrays each time as before)
        const clock_t t0 = clock();
        for(u32 r = 0; r < 2000u; r++) fude_zoom_circuit_advance(&a, 0.0, 0u, true);
        const clock_t t1 = clock();
        for(u32 r = 0; r < 2000u; r++) {
            for(u32 k = 0; k < 7u; k++) {
                const usize size = dirty[k]->data_type_size;
                rde_arr_free(dirty[k]);
                *dirty[k] = rde_arr_new(size, rde_memory_allocator_get_default_std());
            }
            fude_zoom_circuit_advance(&a, 0.0, 0u, true);
        }
        const clock_t t2 = clock();
        printf("  example %u: %u wires, %u pieces (%u carrying), the same bit for bit; 2000 times: %.2f ms kept, %.2f ms new arrays\n", examples[e], nw, pieces, carrying,
               1000.0 * (f64)(t1 - t0) / CLOCKS_PER_SEC, 1000.0 * (f64)(t2 - t1) / CLOCKS_PER_SEC);
        if(e == 0u) {
            // The stamps' finds (sparse.h's binary search along a row) against the steps: a step at a time, its Newton
            // iterations counted; then each block's every place found once for each iteration (a stamp writes each
            // place at least once an iteration).
            u32 iterations = 0;
            const clock_t s0 = clock();
            for(u32 k = 0; k < 400u; k++) { fude_zoom_circuit_steps(&a, 1u, false); iterations += a.solves; }
            const clock_t s1 = clock();
            u32 finds = 0, sink = 0;
            const fude_sim_sparse* sys = (const fude_sim_sparse*)a.block_system.memory;
            for(u32 it = 0; it < iterations; it++) {
                for(u32 bl = 0; bl < a.blocks; bl++) {
                    const u32* order = (const u32*)sys[bl].order.memory;
                    const u32* row_at = (const u32*)sys[bl].row_at.memory;
                    const u32* col = (const u32*)sys[bl].col.memory;
                    for(u32 p = 0; p < sys[bl].n; p++) {
                        for(u32 x = row_at[p]; x < row_at[p + 1u]; x++) { sink += fude_sim_sparse_find(&sys[bl], order[p], order[col[x]]); finds++; }
                    }
                }
            }
            const clock_t s2 = clock();
            printf("  its 400 steps: %u Newton iterations, %.1f ms; their places found once each an iteration (%u finds): %.1f ms (%u)\n", iterations,
                   1000.0 * (f64)(s1 - s0) / CLOCKS_PER_SEC, finds, 1000.0 * (f64)(s2 - s1) / CLOCKS_PER_SEC, sink & 1u);
        }
        fude_zoom_circuit_destroy(&b);
        example_close(&s, &a);
    }
}

// NODES ONLY LOGIC IS ON (circuit_blocks.c's fzc_outside) are no unknowns of the analog solve: nothing analog stamps
// them. After every frame each is in no block, at its level's volts (5 V high, 0 V low, released or unknown). The full
// adder of gates, logic alone, has no blocks at all; Everything at once has them beside its analog blocks. (That the
// rest solves as it did, each node and each part's reading within the solve's tolerance, was measured over every
// circuit example on its own when they were taken out: 12 of 1,503 values differed, by 3e-9 at most.)
static void test_logic_nodes(void) {
    printf("nodes only logic is on\n");
    static const u32 examples[2] = { FUDE_ZOOM_EXAMPLE_FULL_ADDER, FUDE_ZOOM_EXAMPLE_EVERYTHING };
    for(u32 e = 0; e < 2u; e++) {
        fude_zoom_scene s; fude_zoom_circuit c;
        example_open(&s, &c, examples[e]);
        u32 logic = 0, wrong = 0;
        for(u32 f = 0; f < 30u; f++) {
            CHECK(fude_zoom_circuit_run(&c, 1.0 / 60.0, 400u));
            for(u32 k = 1; k < c.nodes; k++) {
                const u8 level = fude_zoom_logic_node(c.logic, k);
                if(level == 0xFFu) continue;
                logic += f == 0u ? 1u : 0u;
                wrong += ((const u32*)c.node_block.memory)[k] != FUDE_ZOOM_NONE || fude_zoom_circuit_volts(&c, k) != (level == FUDE_SIM_1 ? FUDE_ZOOM_LOGIC_V : 0.0) ? 1u : 0u;
            }
        }
        printf("  example %u: %u nodes only logic is on, %u analog blocks, %u wrong\n", examples[e], logic, c.blocks, wrong);
        CHECK(logic > 0u && wrong == 0u && (e == 0u ? c.blocks == 0u : c.blocks > 0u));
        example_close(&s, &c);
    }
}

// The solver's shortcuts (circuit.h). RAILS HELD: a rail's node known, its current what the rest draw (Kirchhoff kept);
// a capacitor straight across one charged at once and then still — not ringing, not holding the steps back. BLOCKS
// apart through pins that only sense, joined once a stamp is found to write across (an oscilloscope's ground off
// ground). QUIET blocks: a circuit at rest left as it is while another goes on (an RC still charging, exactly), woken
// by its switch tapped; the crossroads beeping in a few hundred steps a frame, most of it quiet (its lights still
// right: test_big_examples).
static void test_quiet_blocks(void) {
    printf("quiet blocks\n");
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 rail = part_put(&s, "supply rail", 0, 300, 30, 10, "5V");
        const u32 r = part_put(&s, "resistor", 150, 250, 30, 10, "1k");
        const u32 led = part_put(&s, "LED", 300, 200, 30, 20, "red");
        const u32 g = part_put(&s, "ground", 450, 100, 20, 20, "");
        const u32 cap = part_put(&s, "capacitor", 150, 500, 20, 10, "100nF");
        const u32 g2 = part_put(&s, "ground", 300, 450, 20, 20, "");
        wire_put(&s, rail, 0, r, 0); wire_put(&s, r, 1, led, 0); wire_put(&s, led, 1, g, 0);
        wire_put(&s, rail, 0, cap, 0); wire_put(&s, cap, 1, g2, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(c.blocks == 1u);   // (the resistor's and the LED's node alone: the rail's held)
        CHECK(run_for(&c, 0.2) && fabs(c.time - 0.2) < 1e-9);
        const f64 i_r = cpart(&c, r)->pin_i[0], i_rail = cpart(&c, rail)->pin_i[0], i_cap = cpart(&c, cap)->pin_i[0];
        printf("  held rail: %.3f mA through the LED, the rail's %.3f mA; the capacitor across it %.1e A\n", i_r * 1e3, i_rail * 1e3, i_cap);
        CHECK(fabs(fabs(i_r) - fabs(i_rail)) < 1e-9 && fabs(i_r) > 2e-3 && fabs(i_r) < 4e-3 && cpart(&c, led)->shown > 0.2);
        CHECK(fabs(i_cap) < 1e-9 && kirchhoff_worst(&c) < 1e-9);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    {
        // (a switch and an LED on a rail; apart, an RC charging for ever — τ 10 s — so the steps go on)
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 rail = part_put(&s, "supply rail", 0, 300, 30, 10, "5V");
        const u32 sw = part_put(&s, "SPST switch", 150, 250, 30, 20, "off");
        const u32 r = part_put(&s, "resistor", 300, 250, 30, 10, "1k");
        const u32 led = part_put(&s, "LED", 450, 200, 30, 20, "red");
        const u32 g = part_put(&s, "ground", 600, 100, 20, 20, "");
        wire_put(&s, rail, 0, sw, 0); wire_put(&s, sw, 1, r, 0); wire_put(&s, r, 1, led, 0); wire_put(&s, led, 1, g, 0);
        const u32 rail2 = part_put(&s, "supply rail", 0, -300, 30, 10, "5V");
        const u32 big = part_put(&s, "resistor", 150, -350, 30, 10, "1M");
        const u32 slow = part_put(&s, "capacitor", 300, -400, 20, 10, "10uF");
        const u32 g2 = part_put(&s, "ground", 450, -500, 20, 20, "");
        wire_put(&s, rail2, 0, big, 0); wire_put(&s, big, 1, slow, 0); wire_put(&s, slow, 1, g2, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.5));
        printf("  at rest: %u blocks, %u left quiet; the LED %.3f\n", c.blocks, c.skipped, cpart(&c, led)->shown);
        CHECK(c.blocks >= 2u && c.skipped >= 1u && cpart(&c, led)->shown < 0.01);
        c8 say[32];
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw), -1, say, sizeof say));
        CHECK(fude_zoom_circuit_run(&c, 0.01, 400u));
        const fude_zoom_circuit_part* sc = cpart(&c, slow);
        const f64 vc = fabs(fude_zoom_circuit_volts(&c, sc->node[0]) - fude_zoom_circuit_volts(&c, sc->node[1]));
        printf("  switched on: the LED %.3f; the RC %.4f V (%.4f V wanted)\n", cpart(&c, led)->shown, vc, 5.0 * (1.0 - exp(-c.time / 10.0)));
        CHECK(cpart(&c, led)->shown > 0.2 && fabs(vc - 5.0 * (1.0 - exp(-c.time / 10.0))) < 0.005);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    {
        // (two dividers on a rail, an oscilloscope's CH1 on one's middle and its GND on the other's)
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 rail = part_put(&s, "supply rail", 0, 300, 30, 10, "5V");
        u32 top[2], bottom[2], gnd[2];
        for(u32 k = 0; k < 2u; k++) {
            top[k] = part_put(&s, "resistor", 150, 300 - 300.0 * k, 30, 10, "10k");
            bottom[k] = part_put(&s, "resistor", 350, 300 - 300.0 * k, 30, 10, k == 0u ? "10k" : "30k");
            gnd[k] = part_put(&s, "ground", 500, 250 - 300.0 * k, 20, 20, "");
            wire_put(&s, rail, 0, top[k], 0); wire_put(&s, top[k], 1, bottom[k], 0); wire_put(&s, bottom[k], 1, gnd[k], 0);
        }
        const u32 sp = part_put(&s, "oscilloscope", 800, 100, 110, 80, "5ms 2V");
        wire_put(&s, top[0], 1, sp, 0); wire_put(&s, top[1], 1, sp, 2);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        const u32 blocks_before = c.blocks;
        CHECK(run_for(&c, 0.05));
        printf("  an oscilloscope's ground off ground: %u blocks, then %u (%u joins found); Kirchhoff off by %.1e A\n", blocks_before, c.blocks,
               (u32)rde_arr_length(&c.couplings), kirchhoff_worst(&c));
        CHECK(blocks_before == 2u && c.blocks == 1u && rde_arr_length(&c.couplings) >= 1u && kirchhoff_worst(&c) < 1e-9);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    {
        fude_zoom_scene s; fude_zoom_circuit c;
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_TRAFFIC);
        fude_zoom_circuit_sound(&c, 48000.0);
        CHECK(run_for(&c, 4.5));   // (step 6: WALK lit, the beeper on)
        u32 most = 0, skipped = 0, frames = 0;
        f64 peak = 0.0;
        for(u32 f = 0; f < 60u; f++) {
            const f64 end = c.time + 1.0 / 60.0;
            u32 steps = 0;
            while(c.time < end - 1e-12 && steps < 2000u) { CHECK(fude_zoom_circuit_advance(&c, end - c.time, 1u, false)); steps++; skipped += c.skipped; }
            most = steps > most ? steps : most;
            frames++;
            const f32* a = (const f32*)c.audio.memory;
            for(u32 i = 0; i < (u32)rde_arr_length(&c.audio); i++) peak = fmax(peak, fabs((f64)a[i]));
            rde_arr_clear(&c.audio);
        }
        printf("  crossroads beeping: %u blocks, at most %u steps a frame, %.1f blocks quiet a step; its sound's peak %.3f\n", c.blocks, most,
               (f64)skipped / (f64)(most * frames), peak);
        CHECK(c.blocks >= 4u && most <= 400u && skipped > most * frames && peak > 0.02);
        example_close(&s, &c);
    }
}


// AN OSCILLOSCOPE'S KNOBS (display.h): each channel's volts a division from its text (CH2's its second volts, else
// CH1's); every knob found anywhere in its cell — its right half turning it clockwise, its left back —, none on its
// screen or off its panel, none overlapping; volts and time stepping 1, 2, 5 (bigger the wave clockwise) and stopping
// at their ends, off-step values to the next step; positions half a division at a time, 8 either way; each knob further
// round clockwise; what each says. Its face's words on its panel, not its screen. In a circuit: CH1's volts turned,
// nothing else changed; its time turned from 5 ms to 2 ms a division — its sweep started again, its steps finer, a 50
// Hz cycle 400 samples long instead of 160; a step past its end said, unchanged.
static void test_scope_knobs(void) {
    printf("oscilloscope knobs\n");
    fude_zoom_scope sc = fude_zoom_display_scope("1ms 2V 0.5V");
    CHECK(fabs(sc.volt_div - 2.0) < 1e-12 && fabs(sc.volt_div2 - 0.5) < 1e-12 && sc.pos1 == 0.0 && sc.pos2 == 0.0);
    sc = fude_zoom_display_scope("1ms 2V");
    CHECK(fabs(sc.volt_div2 - 2.0) < 1e-12);
    sc = fude_zoom_display_scope("1ms trig 1V 2V 0.2V");
    CHECK(fabs(sc.level - 1.0) < 1e-12 && fabs(sc.volt_div - 2.0) < 1e-12 && fabs(sc.volt_div2 - 0.2) < 1e-12);
    f64 su0, sv0, su1, sv1;
    fude_zoom_scope_screen(&su0, &sv0, &su1, &sv1);
    CHECK(fabs((su1 - su0) * 150.0 / ((sv1 - sv0) * 80.0) - FUDE_ZOOM_SCOPE_DIVS_X / FUDE_ZOOM_SCOPE_DIVS_Y) < 0.01);   // (its divisions square, as it comes)
    u32 found = 0, ways = 0;
    for(u32 k = 0; k < FUDE_ZOOM_SCOPE_KNOBS; k++) {
        f64 u, v, r;
        fude_zoom_scope_knob(k, &u, &v, &r);
        i32 way = 0;
        found += fude_zoom_scope_knob_at(u + 0.03, v, &way) == k ? 1u : 0u;
        ways  += way == 1 ? 1u : 0u;
        found += fude_zoom_scope_knob_at(u - 0.03, v - 0.1, &way) == k ? 1u : 0u;
        ways  += way == -1 ? 1u : 0u;
        CHECK(u - r * 80.0 / 150.0 > su1 && u + r * 80.0 / 150.0 < 1.0 && v + r < 1.0 && v - r > -1.0);   // (on its panel)
    }
    CHECK(found == 2u * FUDE_ZOOM_SCOPE_KNOBS && ways == 2u * FUDE_ZOOM_SCOPE_KNOBS);
    // (none on its screen; every point of its panel some knob's — the one whose middle is in the same row and column —,
    // each knob's ring wholly its own)
    u32 on_screen = 0, panel = 0, panel_found = 0, ring_off = 0;
    for(f64 u = -1.0; u <= 1.0; u += 0.01) {
        for(f64 v = -1.0; v <= 1.0; v += 0.01) {
            const u32 k = fude_zoom_scope_knob_at(u, v, NULL);
            on_screen += k != FUDE_ZOOM_NONE && u <= su1 ? 1u : 0u;
            if(u >= 0.5 && u <= 0.98) {
                panel++;
                panel_found += k != FUDE_ZOOM_NONE ? 1u : 0u;
            }
        }
    }
    for(u32 k = 0; k < FUDE_ZOOM_SCOPE_KNOBS; k++) {
        f64 u, v, r;
        fude_zoom_scope_knob(k, &u, &v, &r);
        for(u32 a = 0; a < 16u; a++) {
            const f64 an = 2.0 * 3.14159265358979 * (f64)a / 16.0;
            ring_off += fude_zoom_scope_knob_at(u + 1.2 * r * 80.0 / 150.0 * cos(an), v + 1.2 * r * sin(an), NULL) != k ? 1u : 0u;
        }
    }
    CHECK(on_screen == 0u && panel_found == panel && ring_off == 0u);
    // (volts: 2 down to 1 mV clockwise, a step at a time, then no further; back up to 50 V)
    sc = fude_zoom_display_scope("1ms 2V");
    u32 steps = 0;
    f64 was_round = fude_zoom_scope_knob_round(&sc, FUDE_ZOOM_SCOPE_KNOB_VOLTS1);
    b8 rounder = true;
    while(fude_zoom_scope_turn(&sc, FUDE_ZOOM_SCOPE_KNOB_VOLTS1, 1) && steps < 50u) {
        steps++;
        const f64 r = fude_zoom_scope_knob_round(&sc, FUDE_ZOOM_SCOPE_KNOB_VOLTS1);
        rounder = rounder && r > was_round;
        was_round = r;
    }
    CHECK(steps == 10u && fabs(sc.volt_div - 1e-3) < 1e-15 && rounder && fabs(was_round - 1.0) < 1e-9);   // (1, 0.5, 0.2 ... 0.001)
    steps = 0;
    while(fude_zoom_scope_turn(&sc, FUDE_ZOOM_SCOPE_KNOB_VOLTS1, -1) && steps < 50u) steps++;
    CHECK(steps == 14u && fabs(sc.volt_div - 50.0) < 1e-12 && fude_zoom_scope_knob_round(&sc, FUDE_ZOOM_SCOPE_KNOB_VOLTS1) < 1e-9);
    sc = fude_zoom_display_scope("1ms 3V");
    fude_zoom_scope s2 = sc;
    CHECK(fude_zoom_scope_turn(&sc, FUDE_ZOOM_SCOPE_KNOB_VOLTS1, 1) && fabs(sc.volt_div - 2.0) < 1e-12);
    CHECK(fude_zoom_scope_turn(&s2, FUDE_ZOOM_SCOPE_KNOB_VOLTS1, -1) && fabs(s2.volt_div - 5.0) < 1e-12);
    CHECK(fude_zoom_scope_turn(&sc, FUDE_ZOOM_SCOPE_KNOB_VOLTS2, 1) && fabs(sc.volt_div2 - 2.0) < 1e-12 && fabs(sc.volt_div - 2.0) < 1e-12);
    CHECK(fude_zoom_scope_turn(&sc, FUDE_ZOOM_SCOPE_KNOB_TIME, 1) && fabs(sc.time_div - 5e-4) < 1e-15);
    // (positions: half a division at a time, to 8 and no further)
    steps = 0;
    while(fude_zoom_scope_turn(&sc, FUDE_ZOOM_SCOPE_KNOB_POS1, 1) && steps < 50u) steps++;
    CHECK(steps == 16u && fabs(sc.pos1 - FUDE_ZOOM_SCOPE_POS_MOST) < 1e-12 && sc.pos2 == 0.0);
    CHECK(fude_zoom_scope_turn(&sc, FUDE_ZOOM_SCOPE_KNOB_POS2, -1) && fabs(sc.pos2 + 0.5) < 1e-12);
    c8 say[32];
    static const struct { u32 knob; const c8* want; } said[5] = { { FUDE_ZOOM_SCOPE_KNOB_VOLTS1, "2 V" }, { FUDE_ZOOM_SCOPE_KNOB_VOLTS2, "2 V" },
                                                                   { FUDE_ZOOM_SCOPE_KNOB_TIME, "500 \xC2\xB5s" }, { FUDE_ZOOM_SCOPE_KNOB_POS1, "+8" },
                                                                   { FUDE_ZOOM_SCOPE_KNOB_POS2, "-0.5" } };
    u32 right = 0;
    for(u32 i = 0; i < 5u; i++) {
        fude_zoom_scope_knob_say(&sc, said[i].knob, say, sizeof(say));
        if(strcmp(say, said[i].want) != 0) printf("  knob %u says '%s', not '%s'\n", said[i].knob, say, said[i].want);
        right += strcmp(say, said[i].want) == 0 ? 1u : 0u;
    }
    CHECK(right == 5u);
    sc.volt_div = 0.05; sc.time_div = 2.0; sc.pos1 = 0.0;
    fude_zoom_scope_knob_say(&sc, FUDE_ZOOM_SCOPE_KNOB_VOLTS1, say, sizeof(say)); CHECK(strcmp(say, "50 mV") == 0);
    fude_zoom_scope_knob_say(&sc, FUDE_ZOOM_SCOPE_KNOB_TIME, say, sizeof(say));   CHECK(strcmp(say, "2 s") == 0);
    fude_zoom_scope_knob_say(&sc, FUDE_ZOOM_SCOPE_KNOB_POS1, say, sizeof(say));   CHECK(strcmp(say, "0") == 0);
    // (its face's words: on its panel, right of its screen; a resistor's: none)
    fude_zoom_part_label words[16];
    const u32 nw = fude_zoom_part_labels(fude_zoom_part_find("oscilloscope"), words, 16u);
    u32 off = 0;
    for(u32 i = 0; i < nw; i++) off += words[i].u <= su1 + 0.03 || fabsf(words[i].v) > 1.0f ? 1u : 0u;
    CHECK(nw == 7u && off == 0u && fude_zoom_part_labels(fude_zoom_part_find("resistor"), words, 16u) == 0u);
    // In a circuit: 50 Hz into CH1, 5 ms a division.
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 ac = part_put(&s, "AC source", -300, 0, 20, 30, "5V 50Hz");
    const u32 sp = part_put(&s, "oscilloscope", 0, 0, 150, 80, "5ms 2V");
    wire_put(&s, ac, 0, sp, 0);
    wire_put(&s, ac, 1, sp, 2);
    const u32 g = part_put(&s, "ground", -300, -150, 20, 20, "");
    wire_put(&s, ac, 1, g, 0);
    fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
    CHECK(run_for(&c, 0.31));
    const u32 at = cindex(&c, sp);
    CHECK(fude_zoom_circuit_scope_turn(&c, at, FUDE_ZOOM_SCOPE_KNOB_VOLTS1, 1, say, sizeof(say)) && strcmp(say, "CH1 VOLTS/DIV 1 V") == 0);
    const fude_zoom_scope now = fude_zoom_display_scope_of(cpart(&c, sp));
    CHECK(fabs(now.volt_div - 1.0) < 1e-12 && fabs(now.volt_div2 - 2.0) < 1e-12 && fabs(now.time_div - 5e-3) < 1e-15);
    const f64 step_was = c.step;
    CHECK(fude_zoom_circuit_scope_turn(&c, at, FUDE_ZOOM_SCOPE_KNOB_TIME, 1, say, sizeof(say)) && strcmp(say, "TIME/DIV 2 ms") == 0);
    CHECK(cpart(&c, sp)->state[1] == -1.0 && c.step < step_was && c.step <= 0.02 / 50.0 + 1e-12);
    CHECK(run_for(&c, 0.2));
    const f32* smp = (const f32*)fude_zoom_circuit_store(&c, cpart(&c, sp));
    u32 rises = 0;
    for(u32 i = 1; smp != NULL && i < FUDE_ZOOM_SCOPE_SAMPLES; i++) rises += smp[i - 1u] < 0.0f && smp[i] >= 0.0f ? 1u : 0u;
    CHECK(rises <= 1u);   // (a sweep a cycle: triggered at its start, no other rise through 0 in it)
    for(u32 k = 0; k < 20u; k++) fude_zoom_circuit_scope_turn(&c, at, FUDE_ZOOM_SCOPE_KNOB_TIME, -1, say, sizeof(say));
    CHECK(strcmp(say, "TIME/DIV 5 s") == 0 && fabs(fude_zoom_display_scope_of(cpart(&c, sp)).time_div - 5.0) < 1e-12);
    CHECK(!fude_zoom_circuit_scope_turn(&c, cindex(&c, ac), FUDE_ZOOM_SCOPE_KNOB_TIME, 1, say, sizeof(say)));
    fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
}

// A character LCD (an HD44780): its controller alone — 8 bits, then 4 — and on its pins, written by logic inputs.
static void lcd_send(fude_zoom_lcd* l, b8 rs, u8 b) { fude_zoom_lcd_strobe(l, rs, b); }
static void lcd_send4(fude_zoom_lcd* l, b8 rs, u8 b) { fude_zoom_lcd_strobe(l, rs, (u8)(b & 0xF0u)); fude_zoom_lcd_strobe(l, rs, (u8)(b << 4)); }
static void test_lcd(void) {
    printf("character LCD\n");
    fude_zoom_lcd l;
    fude_zoom_lcd_power_on(&l);
    CHECK((l.function & FUDE_ZOOM_LCD_DL) && !(l.control & FUDE_ZOOM_LCD_D) && fude_zoom_lcd_shown(&l, 16, 2, 0, 0, NULL) == ' ' && l.fresh);
    lcd_send(&l, 0, 0x38);
    CHECK(!l.fresh);   // (told something: its dark blocks gone)
    lcd_send(&l, 0, 0x0E); lcd_send(&l, 0, 0x01); lcd_send(&l, 0, 0x06);
    lcd_send(&l, 1, 'H'); lcd_send(&l, 1, 'i');
    lcd_send(&l, 0, 0xC0); lcd_send(&l, 1, '!');
    b8 cur = false;
    CHECK(fude_zoom_lcd_shown(&l, 16, 2, 0, 0, NULL) == 'H' && fude_zoom_lcd_shown(&l, 16, 2, 1, 0, NULL) == 'i' && fude_zoom_lcd_shown(&l, 16, 2, 0, 1, NULL) == '!');
    CHECK(fude_zoom_lcd_shown(&l, 16, 2, 1, 1, &cur) == ' ' && cur && (l.control & FUDE_ZOOM_LCD_C));
    // Shifted left a character (the display, not the cursor): 'i' at the left; back right: as it was.
    lcd_send(&l, 0, 0x18);
    CHECK(fude_zoom_lcd_shown(&l, 16, 2, 0, 0, NULL) == 'i');
    lcd_send(&l, 0, 0x1C);
    CHECK(fude_zoom_lcd_shown(&l, 16, 2, 0, 0, NULL) == 'H');
    // Counting down (entry mode, I/D 0): written leftward from column 5.
    lcd_send(&l, 0, 0x04); lcd_send(&l, 0, 0x85); lcd_send(&l, 1, 'a'); lcd_send(&l, 1, 'b');
    CHECK(fude_zoom_lcd_shown(&l, 16, 2, 5, 0, NULL) == 'a' && fude_zoom_lcd_shown(&l, 16, 2, 4, 0, NULL) == 'b');
    // Its own character 0: a box written to CGRAM, shown where 0 is written.
    lcd_send(&l, 0, 0x06); lcd_send(&l, 0, 0x40);
    const u8 box[8] = { 0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F, 0x00 };
    for(u32 r = 0; r < 8u; r++) lcd_send(&l, 1, box[r]);
    lcd_send(&l, 0, 0x8F); lcd_send(&l, 1, 0x00);
    u8 rows[8];
    fude_zoom_lcd_glyph(&l, fude_zoom_lcd_shown(&l, 16, 2, 15, 0, NULL), rows);
    CHECK(memcmp(rows, box, 8) == 0);
    // Its ROM: 'A', '¥' for the backslash, '°'.
    fude_zoom_lcd_glyph(&l, 'A', rows);
    CHECK(rows[0] == 0x0E && rows[1] == 0x11 && rows[3] == 0x11 && rows[4] == 0x1F && rows[6] == 0x11 && rows[7] == 0);
    fude_zoom_lcd_glyph(&l, '\\', rows);
    CHECK(rows[0] == 0x11 && rows[1] == 0x0A && rows[2] == 0x1F && rows[4] == 0x1F);
    fude_zoom_lcd_glyph(&l, 0xDF, rows);
    CHECK(rows[0] == 0x1C && rows[3] == 0);
    // One line on two rows: the second blank; a 20 × 4 on two lines: its third row on from its first (0x14).
    lcd_send(&l, 0, 0x30);
    CHECK(fude_zoom_lcd_shown(&l, 16, 2, 0, 1, NULL) == ' ');
    lcd_send(&l, 0, 0x38); lcd_send(&l, 0, 0x01); lcd_send(&l, 0, 0x94); lcd_send(&l, 1, 'X');
    CHECK(fude_zoom_lcd_shown(&l, 20, 4, 0, 2, NULL) == 'X' && fude_zoom_lcd_shown(&l, 20, 4, 0, 0, NULL) == ' ');
    // 4 bits: as a microcontroller starts one — 3, 3, 3, 2 on D7–D4 (8 bits till then), then each byte in two halves.
    fude_zoom_lcd_power_on(&l);
    lcd_send(&l, 0, 0x30); lcd_send(&l, 0, 0x30); lcd_send(&l, 0, 0x30); lcd_send(&l, 0, 0x20);
    CHECK(!(l.function & FUDE_ZOOM_LCD_DL));
    lcd_send4(&l, 0, 0x28); lcd_send4(&l, 0, 0x0C); lcd_send4(&l, 0, 0x01); lcd_send4(&l, 1, 'O'); lcd_send4(&l, 1, 'K');
    CHECK((l.function & FUDE_ZOOM_LCD_N) && (l.control & FUDE_ZOOM_LCD_D) && fude_zoom_lcd_shown(&l, 16, 2, 0, 0, NULL) == 'O' && fude_zoom_lcd_shown(&l, 16, 2, 1, 0, NULL) == 'K');
    // On its pins: VSS, RW, K grounded; VDD, A on 5 V; RS, E, D0–D7 logic inputs — 0x38, 0x0C, 0x01,
    // "Hi" written with E.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 d = part_put(&s, "character LCD", 0, 0, 170, 65, "16x2");
        tie(&s, d, 0, 0.0, 30.0 + 40.0, NULL);   // (a ground over it: its pin its top)
        tie(&s, d, 1, 0.0, 30.0, "5V");
        tie(&s, d, 4, 0.0, 30.0 + 40.0, NULL);
        tie(&s, d, 14, 0.0, 30.0, "5V");
        tie(&s, d, 15, 0.0, 30.0 + 40.0, NULL);
        u32 in[16] = { 0 };
        const u32 pins[10] = { 3, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
        for(u32 k = 0; k < 10u; k++) {
            fude_zoom_v2 at;
            CHECK(fude_zoom_part_pin_at(&s, d, pins[k], &at));
            in[pins[k]] = part_put(&s, "logic input", at.x - 30.0, at.y + 30.0, 30, 10, "0");
            const fude_zoom_v2 r[2] = { at, { at.x, at.y + 30.0 } };
            fude_zoom_wire_add(&s, s.root, r, 2u, d, (i32)pins[k], in[pins[k]], 0, (rde_color){ 1, 1, 1, 255 }, 0.1f);
        }
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.01));
        const u8 bytes[5] = { 0x38, 0x0C, 0x01, 'H', 'i' };
        for(u32 k = 0; k < 5u; k++) {
            set_in(&c, in[3], k >= 3u ? 1u : 0u);
            for(u32 b = 0; b < 8u; b++) set_in(&c, in[6u + b], (bytes[k] >> b) & 1u);
            CHECK(step(&c));
            set_in(&c, in[5], 1);
            CHECK(step(&c));
            set_in(&c, in[5], 0);
            CHECK(step(&c));
        }
        const fude_zoom_lcd* lc = (const fude_zoom_lcd*)fude_zoom_circuit_store(&c, cpart(&c, d));
        CHECK(lc != NULL && lc->powered && (lc->control & FUDE_ZOOM_LCD_D));
        CHECK(lc != NULL && fude_zoom_lcd_shown(lc, 16, 2, 0, 0, NULL) == 'H' && fude_zoom_lcd_shown(lc, 16, 2, 1, 0, NULL) == 'i');
        CHECK(cpart(&c, d)->state[0] > 0.99 && cpart(&c, d)->shown > 1.0);   // (full contrast, V0 left open; its backlight on)
        CHECK(!cpart(&c, d)->burnt);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
}

// LIMITS (limits.h): what each part takes at most, measured as it steps, and what goes past them burns.
static void test_limits(void) {
    printf("limits\n");
    // An LED straight on a 9 V battery: far past its 30 mA — burnt within a few hundredths of a second, dark from then
    // on, said once (the battery not: past its current only the moment it took).
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 bat = part_put(&s, "battery", 0, 0, 20, 30, "9V");
        const u32 led = part_put(&s, "LED", 200, 0, 30, 20, "red");
        wire_put(&s, bat, 0, led, 0); wire_put(&s, led, 1, bat, 1);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(cpart(&c, led)->limits.most[FUDE_ZOOM_LIMIT_CURRENT] > 0.02 && cpart(&c, led)->limits.most[FUDE_ZOOM_LIMIT_CURRENT] < 0.05);
        CHECK(run_for(&c, 0.05));
        CHECK(cpart(&c, led)->burnt);
        CHECK(events_of(&c, FUDE_ZOOM_CIRCUIT_BURNT, led) == 1u);
        CHECK(events_of(&c, FUDE_ZOOM_CIRCUIT_OVER, bat) == 0u);
        const fude_zoom_circuit_event* e = (const fude_zoom_circuit_event*)c.events.memory;
        for(u32 i = 0; i < (u32)rde_arr_length(&c.events); i++) {
            if(e[i].kind == FUDE_ZOOM_CIRCUIT_BURNT) CHECK(e[i].limit == FUDE_ZOOM_LIMIT_CURRENT && e[i].measure > 0.1 && fabs(e[i].most - 0.03) < 1e-9);
        }
        rde_arr_clear(&c.events);
        CHECK(run_for(&c, 0.2));
        CHECK(cpart(&c, led)->shown < 0.01);   // (burnt: open)
        CHECK(rde_arr_length(&c.events) == 0u);   // (said once: the battery within its current again, the LED gone)
        // Burnt it stays as the canvas changes (a rebuild); the circuit started again, a new one.
        fude_zoom_circuit_build(&c, &s);
        CHECK(cpart(&c, led)->burnt);
        fude_zoom_circuit_reset(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(!cpart(&c, led)->burnt && cpart(&c, led)->heat == 0.0);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // A wire straight across a battery: a dead short, said at once — 18 A (its 0.5 Ω), 1 A at most.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 bat = part_put(&s, "battery", 0, 0, 20, 30, "9V");
        wire_put(&s, bat, 0, bat, 1);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 0.01));
        CHECK(events_of(&c, FUDE_ZOOM_CIRCUIT_OVER, bat) == 1u);
        CHECK(rde_arr_length(&c.events) >= 1u && fabs(((const fude_zoom_circuit_event*)c.events.memory)[0].measure - 18.0) < 1e-9);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // The same LED behind 330 Ω on 5 V (13 mA): for good — never warmer than it was.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 rail = part_put(&s, "supply rail", 0, 100, 30, 10, "5V");
        const u32 r = part_put(&s, "resistor", 100, 50, 30, 10, "330");
        const u32 led = part_put(&s, "LED", 200, 50, 30, 20, "green");
        const u32 g = part_put(&s, "ground", 300, 0, 20, 20, "");
        wire_put(&s, rail, 0, r, 0); wire_put(&s, r, 1, led, 0); wire_put(&s, led, 1, g, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 3.0));
        CHECK(!cpart(&c, led)->burnt && cpart(&c, led)->heat == 0.0 && cpart(&c, led)->stress < 0.6 && cpart(&c, led)->shown > 0.4);
        CHECK(!cpart(&c, r)->burnt && cpart(&c, r)->stress < 0.5);   // (its 0.25 W: 0.03 W in it)
        CHECK(fabs(cpart(&c, r)->measure[FUDE_ZOOM_LIMIT_POWER] - cpart(&c, r)->measure[FUDE_ZOOM_LIMIT_CURRENT] * cpart(&c, r)->measure[FUDE_ZOOM_LIMIT_CURRENT] * 330.0) < 1e-6);
        CHECK(rde_arr_length(&c.events) == 0u);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // 10 Ω across 5 V: 2.5 W in a 0.25 W resistor — burnt in a fraction of a second; given 5 W on its card, never.
    for(u32 rated = 0; rated < 2u; rated++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 rail = part_put(&s, "supply rail", 0, 100, 30, 10, "5V");
        const u32 r = part_put(&s, "resistor", 100, 50, 30, 10, "10");
        const u32 g = part_put(&s, "ground", 200, 0, 20, 20, "");
        wire_put(&s, rail, 0, r, 0); wire_put(&s, r, 1, g, 0);
        if(rated) {
            const f64 most[4] = { 5.0, 0.0, 0.0, 0.0 };
            fude_zoom_props_add_limits(&s, r, most, 5);
        }
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(fabs(cpart(&c, r)->limits.most[FUDE_ZOOM_LIMIT_POWER] - (rated ? 5.0 : 0.25)) < 1e-12);
        CHECK(run_for(&c, rated ? 5.0 : 0.1));
        CHECK(cpart(&c, r)->burnt == !rated);
        CHECK(fabs(cpart(&c, r)->measure[FUDE_ZOOM_LIMIT_POWER] - (rated ? 2.5 : 0.0)) < 0.01);   // (burnt: nothing in it)
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // An electrolytic the wrong way round across 5 V: past its 1 V backwards — burnt; the right way round, not.
    for(u32 right = 0; right < 2u; right++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 rail = part_put(&s, "supply rail", 0, 100, 30, 10, "5V");
        const u32 cap = part_put(&s, "electrolytic", 100, 50, 30, 10, "100uF");
        const u32 g = part_put(&s, "ground", 200, 0, 20, 20, "");
        wire_put(&s, rail, 0, cap, right ? 0u : 1u); wire_put(&s, cap, right ? 1u : 0u, g, 0);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 1.0));
        CHECK(cpart(&c, cap)->burnt == !right);
        if(!right) CHECK(events_of(&c, FUDE_ZOOM_CIRCUIT_BURNT, cap) == 1u && ((const fude_zoom_circuit_event*)c.events.memory)[0].limit == FUDE_ZOOM_LIMIT_REVERSE);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // A pin joined to nothing puts no voltage across its part: an LED with its cathode on 9 V and its anode on nothing,
    // an electrolytic with its minus on 9 V and its plus on nothing, are neither reversed nor burnt (their open pin
    // read as 0 V burnt both once). Measured alone: an open pin's NaN volts measure nothing across it.
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 rail = part_put(&s, "supply rail", 0, 100, 30, 10, "9V");
        const u32 led = part_put(&s, "LED", 100, 50, 30, 10, "red");
        const u32 cap = part_put(&s, "electrolytic", 100, -50, 30, 10, "100uF");
        wire_put(&s, rail, 0, led, 1u); wire_put(&s, rail, 0, cap, 1u);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        CHECK(run_for(&c, 1.0));
        CHECK(!cpart(&c, led)->burnt && !cpart(&c, cap)->burnt && events_of(&c, FUDE_ZOOM_CIRCUIT_BURNT, led) == 0u && events_of(&c, FUDE_ZOOM_CIRCUIT_BURNT, cap) == 0u);
        CHECK(cpart(&c, led)->measure[FUDE_ZOOM_LIMIT_REVERSE] == 0.0 && cpart(&c, cap)->measure[FUDE_ZOOM_LIMIT_REVERSE] == 0.0);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
        f64 m[FUDE_ZOOM_LIMIT_COUNT];
        const f64 open_v[2] = { NAN, 9.0 }, wired_v[2] = { 0.0, 9.0 }, none_i[2] = { 0.0, 0.0 };
        fude_zoom_limits_measure(fude_zoom_part_find("LED"), open_v, none_i, 2u, m);
        CHECK(m[FUDE_ZOOM_LIMIT_REVERSE] == 0.0 && m[FUDE_ZOOM_LIMIT_VOLTAGE] == 0.0 && m[FUDE_ZOOM_LIMIT_POWER] == 0.0);
        fude_zoom_limits_measure(fude_zoom_part_find("LED"), wired_v, none_i, 2u, m);
        CHECK(m[FUDE_ZOOM_LIMIT_REVERSE] == 9.0 && m[FUDE_ZOOM_LIMIT_VOLTAGE] == 9.0);
    }
    // Real parts by their text: a 1N4007 its 1 A and 1000 V, a BC547 its 100 mA, the series 74HC its chips'; a typical
    // part's when it names none; those whose limits follow from their values.
    {
        const fude_zoom_part* d = fude_zoom_part_find("diode");
        const fude_zoom_part* q = fude_zoom_part_find("NPN");
        fude_zoom_limits l = fude_zoom_limits_typical(d, "D1 1N4007", NULL);
        CHECK(l.most[FUDE_ZOOM_LIMIT_CURRENT] == 1.0 && l.most[FUDE_ZOOM_LIMIT_REVERSE] == 1000.0);
        l = fude_zoom_limits_typical(d, "1n4148", NULL);
        CHECK(l.most[FUDE_ZOOM_LIMIT_CURRENT] == 0.3);
        l = fude_zoom_limits_typical(q, "BC547B", NULL);
        CHECK(l.most[FUDE_ZOOM_LIMIT_CURRENT] == 0.1 && l.most[FUDE_ZOOM_LIMIT_VOLTAGE] == 45.0);
        l = fude_zoom_limits_typical(q, "Q1", NULL);
        CHECK(l.most[FUDE_ZOOM_LIMIT_CURRENT] == 0.6);   // (its kind's own: the 2N2222 it is drawn as)
        CHECK(fude_zoom_limits_named(q, "X2N2222") < 0);   // (inside a word: not it)
        const f64 lamp[2] = { 12.0, 5.0 };
        l = fude_zoom_limits_typical(fude_zoom_part_find("lamp"), "12V 5W", lamp);
        CHECK(l.most[FUDE_ZOOM_LIMIT_POWER] == 7.5);
        CHECK(fude_zoom_limits_kinds(fude_zoom_part_find("ground")) == 0u && fude_zoom_limits_kinds(fude_zoom_part_find("N-MOSFET")) == 15u);
        CHECK(fude_zoom_limits_gate(fude_zoom_part_find("N-MOSFET")) && !fude_zoom_limits_gate(q));
        // Measured: an NPN's collector current and C–E voltage; a chip's supply and its worst output.
        f64 m[FUDE_ZOOM_LIMIT_COUNT];
        const f64 v3[3] = { 0.7, 5.0, 0.0 }, i3[3] = { 0.001, 0.1, -0.101 };
        fude_zoom_limits_measure(q, v3, i3, 3u, m);
        CHECK(fabs(m[FUDE_ZOOM_LIMIT_CURRENT] - 0.1) < 1e-12 && fabs(m[FUDE_ZOOM_LIMIT_VOLTAGE] - 5.0) < 1e-12 && fabs(m[FUDE_ZOOM_LIMIT_POWER] - 0.5007) < 1e-9);
        const fude_zoom_part* chip = fude_zoom_part_find("L293D");
        f64 v16[16], i16[16];
        for(u32 k = 0; k < 16u; k++) { v16[k] = 0.0; i16[k] = 0.0; }
        v16[7] = 12.0; v16[15] = 5.0; i16[2] = -0.4; i16[13] = 0.7; i16[7] = 1.1;   // (VCC2, VCC1; 1Y, 4Y; the supply's own)
        fude_zoom_limits_measure(chip, v16, i16, 16u, m);
        CHECK(fabs(m[FUDE_ZOOM_LIMIT_VOLTAGE] - 12.0) < 1e-12 && fabs(m[FUDE_ZOOM_LIMIT_CURRENT] - 0.7) < 1e-12);
        // Heat: twice its limit, burnt in a third of its kind's time; just under it, never; cooling below it.
        fude_zoom_limits lim = { { 0.0, 0.03, 0.0, 5.0 } };
        const fude_zoom_part* led = fude_zoom_part_find("LED");
        f64 stress, heat = 0.0, t = 0.0;
        u8 worst;
        const f64 twice[4] = { 0.0, 0.06, 2.0, 0.0 };
        while(!fude_zoom_limits_step(led, &lim, twice, 0.001, &stress, &worst, &heat) && t < 1.0) t += 0.001;
        CHECK(fabs(t - fude_zoom_limits_tau(led) / 3.0) < 0.002 && worst == FUDE_ZOOM_LIMIT_CURRENT && fabs(stress - 2.0) < 1e-12);
        heat = 0.5;
        const f64 under[4] = { 0.0, 0.029, 2.0, 4.9 };
        for(u32 k = 0; k < 1000u; k++) CHECK(!fude_zoom_limits_step(led, &lim, under, 0.001, &stress, &worst, &heat));
        CHECK(heat < 0.5 && heat > 0.0);
        // A source: past its current, never burnt.
        const fude_zoom_limits sl = { { 0.0, 1.0, 0.0, 0.0 } };
        const f64 shorted[4] = { 0.0, 18.0, 0.0, 0.0 };
        heat = 0.0;
        for(u32 k = 0; k < 1000u; k++) CHECK(!fude_zoom_limits_step(fude_zoom_part_find("battery"), &sl, shorted, 0.01, &stress, &worst, &heat));
        CHECK(stress == 18.0 && fabs(heat - 10.0) < 1e-9);   // (its heat: the seconds it has been past it)
        const f64 within[4] = { 0.0, 0.5, 0.0, 0.0 };
        CHECK(!fude_zoom_limits_step(fude_zoom_part_find("battery"), &sl, within, 0.01, &stress, &worst, &heat) && heat == 0.0);
    }
    // Every example (electronics, and those with a mechanism) within its parts' limits: nothing burnt, no source past
    // its current, three seconds on.
    for(u32 e = 0; e < FUDE_ZOOM_EXAMPLE_COUNT; e++) {
        if(fude_zoom_example_group(e) != FUDE_ZOOM_EXAMPLES_ELECTRONICS || fude_zoom_example_goes_wrong(e)) continue;   // (those: below)
        fude_zoom_scene s; fude_zoom_circuit c;
        example_open(&s, &c, e);
        run_for(&c, 3.0);
        const u32 n = (u32)rde_arr_length(&c.events);
        if(n != 0u) {
            const fude_zoom_circuit_event* ev = (const fude_zoom_circuit_event*)c.events.memory;
            const fude_zoom_circuit_part* p = (const fude_zoom_circuit_part*)c.parts.memory;
            for(u32 i = 0; i < n; i++) printf("  example %u: %s %s: %g of %g (limit %u)\n", e, p[ev[i].part].part->id, ev[i].kind == FUDE_ZOOM_CIRCUIT_BURNT ? "burnt" : "past", ev[i].measure, ev[i].most, ev[i].limit);
        }
        CHECK(n == 0u);
        example_close(&s, &c);
    }
}

// The examples' folders: each example's set under its own group; every set something in it; a group's sets together, in
// the groups' order (electronics', mechanisms', both's).
static void test_example_sets(void) {
    printf("example folders\n");
    u32 in_set[FUDE_ZOOM_EXAMPLE_SETS] = { 0 }, wrong_group = 0;
    for(u32 e = 0; e < FUDE_ZOOM_EXAMPLE_COUNT; e++) {
        const u8 set = fude_zoom_example_set(e);
        CHECK(set < FUDE_ZOOM_EXAMPLE_SETS);
        if(set >= FUDE_ZOOM_EXAMPLE_SETS) continue;
        in_set[set]++;
        if(fude_zoom_example_set_group(set) != fude_zoom_example_group(e)) {
            printf("  example %u in a folder of another group\n", e);
            wrong_group++;
        }
    }
    u32 empty = 0, out_of_order = 0;
    for(u8 set = 0; set < FUDE_ZOOM_EXAMPLE_SETS; set++) {
        empty += in_set[set] == 0u ? 1u : 0u;
        out_of_order += set > 0u && fude_zoom_example_set_group(set) < fude_zoom_example_set_group((u8)(set - 1u)) ? 1u : 0u;
    }
    CHECK(wrong_group == 0u && empty == 0u && out_of_order == 0u);
}

// Every electronics example works as it says: the torch lit; every gate's truth for A 1, B 0; the flasher on and off;
// 1 + 1 + 0, 5 + 3, 6 + 7 summed; the counter counting its clock; the chaser lighting one LED at a time, the next each
// tick. Every wire joined at both ends.
static void test_examples_electronics(void) {
    fude_zoom_scene s; fude_zoom_circuit c;
    for(u32 e = 0; e < FUDE_ZOOM_EXAMPLE_COUNT; e++) {
        if(fude_zoom_example_group(e) != FUDE_ZOOM_EXAMPLES_ELECTRONICS) continue;
        example_open(&s, &c, e);
        const fude_zoom_circuit_wire* w = (const fude_zoom_circuit_wire*)c.wires.memory;
        u32 loose = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&c.wires); i++) loose += (w[i].loose[0] || w[i].loose[1]) ? 1u : 0u;
        if(loose != 0u) printf("example %u: %u wires loose\n", e, loose);
        CHECK(loose == 0u);
        example_close(&s, &c);
    }
    example_open(&s, &c, FUDE_ZOOM_EXAMPLE_TORCH);
    CHECK(fude_zoom_circuit_run(&c, 0.05, 400u) && cpart(&c, example_part(&s, "LED", NULL))->shown > 0.3);
    example_close(&s, &c);
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_GATES);
        static const c8* const names[7] = { "A AND B", "A OR B", "A XOR B", "A NAND B", "A NOR B", "A XNOR B", "NOT A" };
        static const u32 want[7] = { 0, 1, 1, 1, 0, 0, 0 };
        CHECK(step(&c) && step(&c));
        u32 right = 0;
        for(u32 i = 0; i < 7u; i++) right += probe(&c, example_part(&s, "logic probe", names[i])) == want[i] ? 1u : 0u;
        CHECK(right == 7u);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_FLASHER);
        const u32 led = example_part(&s, "LED", NULL);
        u32 on = 0, off = 0;
        for(u32 k = 0; k < 120u; k++) {
            CHECK(fude_zoom_circuit_run(&c, 0.025, 400u));
            if(cpart(&c, led)->shown > 0.3) on++; else off++;
        }
        CHECK(on > 5u && off > 5u);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_FULL_ADDER);
        CHECK(step(&c) && step(&c));
        CHECK(probe(&c, example_part(&s, "logic probe", "S")) == 0u && probe(&c, example_part(&s, "logic probe", "CO")) == 1u);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_ADDER_4);
        CHECK(fude_zoom_circuit_run(&c, 0.01, 100u));
        static const c8* const sum[5] = { "S0", "S1", "S2", "S3", "C4" };
        u32 v = 0;
        for(u32 i = 0; i < 5u; i++) v |= probe(&c, example_part(&s, "logic probe", sum[i])) << i;
        CHECK(v == 8u);
        example_close(&s, &c);
    }
    {
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_ADDER_CHIP);
        CHECK(fude_zoom_circuit_run(&c, 0.01, 100u));
        static const c8* const sum[5] = { "S1", "S2", "S3", "S4", "C4" };
        u32 v = 0;
        for(u32 i = 0; i < 5u; i++) v |= probe(&c, example_part(&s, "logic probe", sum[i])) << i;
        CHECK(v == 13u);
        example_close(&s, &c);
    }
    {
        // (2 Hz: a rising edge a half second, its LEDs QA to QD as drawn top down)
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_COUNTER);
        u32 leds[4];
        CHECK(example_parts(&s, "LED", NULL, leds, 4u) == 4u);
        u32 counted = 0;
        CHECK(fude_zoom_circuit_run(&c, 0.25, 2000u));   // (between its edges: they are at every half second)
        for(u32 n = 1; n <= 10u; n++) {
            CHECK(fude_zoom_circuit_run(&c, 0.5, 4000u));
            u32 lit = 0;
            for(u32 i = 0; i < 4u; i++) lit |= (cpart(&c, leds[i])->shown > 0.3 ? 1u : 0u) << i;
            counted += lit == (n & 15u) ? 1u : 0u;
        }
        CHECK(counted == 10u);
        example_close(&s, &c);
    }
    {
        // (4 Hz: one of its eight lit, the next each quarter second)
        example_open(&s, &c, FUDE_ZOOM_EXAMPLE_CHASER);
        u32 leds[8];
        CHECK(example_parts(&s, "LED", NULL, leds, 8u) == 8u);
        CHECK(fude_zoom_circuit_run(&c, 0.125, 2000u));   // (between its edges)
        u32 one = 0, onward = 0, was = 99u;
        for(u32 k = 0; k < 20u; k++) {
            CHECK(fude_zoom_circuit_run(&c, 0.25, 4000u));
            u32 lit = 0, at = 99u;
            for(u32 i = 0; i < 8u; i++) if(cpart(&c, leds[i])->shown > 0.3) { lit++; at = i; }
            one += lit == 1u ? 1u : 0u;
            onward += was != 99u && at == (was + 1u) % 8u ? 1u : 0u;
            was = at;
        }
        CHECK(one == 20u && onward == 19u);
        example_close(&s, &c);
    }
}

// The lit colour of the circuit's part on object o (none: alpha 0).
static rde_color lit_of(const fude_zoom_circuit* c, u32 o) {
    const fude_zoom_circuit_part* p = (const fude_zoom_circuit_part*)c->parts.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&c->parts); i++) if(p[i].object == o) return p[i].lit;
    return (rde_color){ 0, 0, 0, 0 };
}
static b8 same_color(rde_color a, rde_color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

// A logic probe's light (props.h): kept, read by the circuit, carried by copies (a body's properties too).
static void test_probe_light(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 in  = part_put(&s, "logic input", -100, 0, 20, 15, "1");
    const u32 pr  = part_put(&s, "logic probe", 100, 0, 15, 15, "S");
    const u32 pr2 = part_put(&s, "logic probe", 100, -100, 15, 15, "T");
    wire_put(&s, in, 0, pr, 0);
    wire_put(&s, in, 0, pr2, 0);
    const rde_color green = { 40, 230, 70, 255 }, blue = { 60, 130, 255, 255 };
    const u32 li = fude_zoom_props_add_light(&s, pr, green);
    rde_color got = { 0, 0, 0, 0 };
    fude_zoom_body_props bp;
    CHECK(fude_zoom_props_kind(&s, li) == FUDE_ZOOM_PROPS_LIGHT && fude_zoom_props_target(&s, li) == pr);
    CHECK(fude_zoom_props_light(&s, li, &got) && same_color(got, green));
    CHECK(fude_zoom_props_find(&s, pr, FUDE_ZOOM_PROPS_LIGHT) == li && fude_zoom_props_find(&s, pr2, FUDE_ZOOM_PROPS_LIGHT) == FUDE_ZOOM_NONE);
    CHECK(!fude_zoom_props_body(&s, li, &bp) && fude_zoom_props_find(&s, pr, FUDE_ZOOM_PROPS_BODY) == FUDE_ZOOM_NONE);
    // The circuit: that probe lit green, the other red as ever.
    {
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        CHECK(same_color(lit_of(&c, pr), green) && same_color(lit_of(&c, pr2), FUDE_ZOOM_PROBE_LIT));
        fude_zoom_circuit_destroy(&c);
    }
    // Chosen again (the old one let go): the new one's colour.
    fude_zoom_scene_set_alive(&s, li, false);
    const u32 li2 = fude_zoom_props_add_light(&s, pr, blue);
    CHECK(fude_zoom_props_find(&s, pr, FUDE_ZOOM_PROPS_LIGHT) == li2 && fude_zoom_props_light(&s, li2, &got) && same_color(got, blue));
    {
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        CHECK(same_color(lit_of(&c, pr), blue));
        fude_zoom_circuit_destroy(&c);
    }
    // Copied: the probe's light with it; the other probe alone, nothing more.
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    rde_arr clips = rde_arr_new(sizeof(fude_zoom_clip), heap), lone = rde_arr_new(sizeof(fude_zoom_clip), heap);
    fude_zoom_select_clips_of(&s, &pr, 1u, &clips);
    fude_zoom_select_clips_of(&s, &pr2, 1u, &lone);
    CHECK(rde_arr_length(&clips) == 2u && rde_arr_length(&lone) == 1u);
    // Put once (Duplicate's, Paste's way): the copy held, its light on it (not held), the original's where it was.
    fude_zoom_selection sel; fude_zoom_select_init(&sel);
    const fude_zoom_sim down = { 1, 0, 0, -200 };
    CHECK(fude_zoom_select_put(&sel, &s, (const fude_zoom_clip*)clips.memory, 2u, &down, 1u, NULL, NULL, 0u) == 2u);
    CHECK(rde_arr_length(&sel.picks) == 1u);
    const u32 copy = ((const fude_zoom_pick*)sel.picks.memory)[0].object;
    const u32 cl = fude_zoom_props_find(&s, copy, FUDE_ZOOM_PROPS_LIGHT);
    CHECK(copy != pr && fude_zoom_part_of(&s, copy) != NULL && fude_zoom_part_of(&s, copy)->model == FUDE_ZOOM_MODEL_LOGIC_OUT);
    CHECK(cl != FUDE_ZOOM_NONE && cl != li2 && fude_zoom_props_light(&s, cl, &got) && same_color(got, blue));
    CHECK(fude_zoom_props_find(&s, pr, FUDE_ZOOM_PROPS_LIGHT) == li2);
    CHECK(fude_zoom_history_undo(&s) && !alive(&s, copy) && !alive(&s, cl) && alive(&s, pr) && alive(&s, li2));
    // Repeated three times: three lights, each on its own copy.
    {
        fude_zoom_select_clear(&sel);
        const fude_zoom_sim moves[3] = { { 1, 0, 0, -150 }, { 1, 0, 0, -300 }, { 1, 0, 0, -450 } };
        CHECK(fude_zoom_select_put(&sel, &s, (const fude_zoom_clip*)clips.memory, 2u, moves, 3u, NULL, NULL, 0u) == 6u);
        CHECK(rde_arr_length(&sel.picks) == 3u);
        u32 lights[3], right = 0;
        for(u32 i = 0; i < 3u; i++) {
            const u32 o = ((const fude_zoom_pick*)sel.picks.memory)[i].object;
            lights[i] = fude_zoom_props_find(&s, o, FUDE_ZOOM_PROPS_LIGHT);
            right += lights[i] != FUDE_ZOOM_NONE && fude_zoom_props_target(&s, lights[i]) == o && fude_zoom_props_light(&s, lights[i], &got) && same_color(got, blue);
        }
        CHECK(right == 3u && lights[0] != lights[1] && lights[1] != lights[2] && lights[0] != lights[2]);
        CHECK(fude_zoom_history_undo(&s));
    }
    // Turned over in place (the original let go): the light on the mirrored copy.
    {
        fude_zoom_select_clear(&sel);
        const fude_zoom_select_mirror m = { { 0, 0 }, 3.14159265358979323846 * 0.5 };
        CHECK(fude_zoom_select_put(&sel, &s, (const fude_zoom_clip*)clips.memory, 2u, NULL, 0u, &m, &pr, 1u) == 2u);
        const u32 o = ((const fude_zoom_pick*)sel.picks.memory)[0].object;
        const u32 l = fude_zoom_props_find(&s, o, FUDE_ZOOM_PROPS_LIGHT);
        CHECK(!alive(&s, pr) && l != FUDE_ZOOM_NONE && fude_zoom_props_light(&s, l, &got) && same_color(got, blue));
        CHECK(fude_zoom_history_undo(&s) && alive(&s, pr));
    }
    // A light in the clips whose probe is not: not put (the copy of nothing's).
    {
        fude_zoom_select_clear(&sel);
        const u32 before = fude_zoom_scene_object_count(&s);
        CHECK(fude_zoom_select_put(&sel, &s, (const fude_zoom_clip*)clips.memory + 1, 1u, &down, 1u, NULL, NULL, 0u) == 0u);
        CHECK(fude_zoom_scene_object_count(&s) == before);
    }
    fude_zoom_select_clips_free(&clips);
    fude_zoom_select_clips_free(&lone);
    // A drawing made a body: duplicated, the copy a body as it was (its material, fixed, its mass).
    {
        const f64 tri[6] = { 0, 0, 40, 0, 0, 60 };
        const u32 poly = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 300, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_POLYGON, tri, 6u, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
        const fude_zoom_body_props was = { 2u, true, 3.0, -1.0, -1.0 };
        fude_zoom_props_add_body(&s, poly, &was);
        fude_zoom_select_clips_of(&s, &poly, 1u, &clips);
        CHECK(rde_arr_length(&clips) == 2u);
        fude_zoom_select_clear(&sel);
        CHECK(fude_zoom_select_put(&sel, &s, (const fude_zoom_clip*)clips.memory, 2u, &down, 1u, NULL, NULL, 0u) == 2u);
        const u32 o = ((const fude_zoom_pick*)sel.picks.memory)[0].object;
        const u32 b = fude_zoom_props_find(&s, o, FUDE_ZOOM_PROPS_BODY);
        CHECK(o != poly && b != FUDE_ZOOM_NONE && fude_zoom_props_body(&s, b, &bp) && bp.material == 2u && bp.fixed && fabs(bp.mass - 3.0) < 1e-12);
        fude_zoom_select_clips_free(&clips);
    }
    rde_arr_free(&clips);
    rde_arr_free(&lone);
    fude_zoom_select_destroy(&sel);
    fude_zoom_scene_destroy(&s);
}

// --- far from 0, nested past reason, damaged numbers ----------------------------------------------

// Views far from 0 against their own size: a graph's ticks are counted by index (a step too fine to move its numbers
// looped for ever: x a few units wide at 1e16, y given round 1e15, y round 1e8 as its function goes), a dashed line
// deep in is walked from where it enters the screen (a position 1e18 along it did not move by a dash), papers far out
// count their lines by index (f64 counters stop past 2^53), and so does a hatch. A clip leaves an end it did not cut
// exactly as it was (what plot.c's curves go by).
static void test_far_views(void) {
    printf("far views\n");
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), heap), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), heap), labels = rde_arr_new(sizeof(fude_zoom_plot_label), heap);
    static const c8* const far[3] = { "y = x\nx 1e16 10000000000000002", "y = x\ny 1e15 1000000000000000.25", "y = 1e8 + x/1e9" };
    for(u32 i = 0; i < 3u; i++) {
        rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
        CHECK(fude_zoom_plot_lines("graph", far[i], 160.0, 110.0, &pts, &parts, &labels) == 1u);
        const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
        b8 inside = true;
        for(u32 k = 0; k < (u32)rde_arr_length(&pts); k++) inside = inside && fabs(p[k].x) <= 160.0 + 1e-6 && fabs(p[k].y) <= 110.0 + 1e-6;
        CHECK(inside && rde_arr_length(&labels) < 100u);
    }
    rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
    fude_zoom_plot_lines("number line", "1e16 10000000000000002", 160.0, 25.0, &pts, &parts, &labels);
    CHECK(rde_arr_length(&labels) == 0u);   // (its ticks' numbers could not be told apart: none)
    rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
    fude_zoom_plot_lines("number line", "-5 5", 160.0, 25.0, &pts, &parts, &labels);
    CHECK(rde_arr_length(&labels) == 11u);   // (near 0, every one)
    rde_arr_free(&pts); rde_arr_free(&parts); rde_arr_free(&labels);
    // A dashed line 2e18 long through a screen 200 across: its dashes on the screen only, a handful.
    rde_arr dashes = rde_arr_new(sizeof(fude_zoom_v2), heap);
    const fude_zoom_v2 longline[2] = { { -1e18, 0.0 }, { 1e18, 0.0 } };
    const u32 pieces = fude_zoom_line_dashes(longline, 2u, false, FUDE_ZOOM_LINE_DASHED, 2.0, (fude_zoom_box){ -100.0, -100.0, 100.0, 100.0 }, &dashes);
    const fude_zoom_v2* d = (const fude_zoom_v2*)dashes.memory;
    b8 near = true;
    for(u32 i = 0; i < 2u * pieces; i++) near = near && fabs(d[i].x) < 400.0 && d[i].y == 0.0;
    printf("  a dashed line 2e18 long: %u dashes on the screen\n", pieces);
    CHECK(pieces >= 5u && pieces <= 40u && near);
    // ... and a short one as it always was: dashes 14 long, 7 apart (a width of 2), from its start.
    const fude_zoom_v2 shortline[2] = { { 0.0, 0.0 }, { 210.0, 0.0 } };
    CHECK(fude_zoom_line_dashes(shortline, 2u, false, FUDE_ZOOM_LINE_DASHED, 2.0, (fude_zoom_box){ -500.0, -500.0, 500.0, 500.0 }, &dashes) == 10u);
    d = (const fude_zoom_v2*)dashes.memory;
    CHECK(d[0].x == 0.0 && d[1].x == 14.0 && d[2].x == 21.0 && d[19].x == 203.0);
    rde_arr_free(&dashes);
    // Papers seen at 1e17: each gives up (or draws what it can) and returns.
    rde_arr lines = rde_arr_new(sizeof(fude_zoom_paper_line), heap);
    const fude_zoom_box out_there = { 1e17, 1e17, 1e17 + 100.0, 1e17 + 80.0 };
    const u8 kinds[5] = { FUDE_PAPER_ISOMETRIC, FUDE_PAPER_HEX, FUDE_PAPER_POLAR, FUDE_PAPER_ENGINEERING, FUDE_PAPER_MUSIC };
    for(u32 k = 0; k < 5u; k++) {
        rde_arr_clear(&lines);
        fude_zoom_paper_lines(kinds[k], out_there, 20.0, 0.5, 2.0, &lines);
        CHECK(rde_arr_length(&lines) <= FUDE_ZOOM_PAPER_MOST);
    }
    rde_arr_clear(&lines);
    CHECK(fude_zoom_paper_page(FUDE_PAPER_CORNELL, (fude_zoom_box){ 0.0, 1e17, 210.0, 1e17 + 297.0 }, 1e17 + 280.0, 8.0, &lines) <= 4004u);
    rde_arr_clear(&lines);
    fude_zoom_paper_sheet(FUDE_PAPER_DOTS, (fude_zoom_box){ 1e17, 1e17, 1e17 + 210.0, 1e17 + 297.0 }, 20.0, 8.0, 5.0, 2.0, &lines);
    CHECK(rde_arr_length(&lines) <= FUDE_ZOOM_PAPER_MOST);
    rde_arr_free(&lines);
    // A hatch 1e-8 apart over a band at y 1e9: its lines' numbers past 2^53 steps.
    rde_arr hatch = rde_arr_new(sizeof(fude_zoom_v2), heap);
    const f64 y0 = 1e9, y1 = 1e9 + 1e-6;
    const fude_zoom_v2 band[4] = { { 0.0, y0 }, { 1.0, y0 }, { 1.0, y1 }, { 0.0, y1 } };
    fude_zoom_hatch_lines(band, 4u, 0.0, 1e-8, (fude_zoom_box){ -1.0, y0 - 1.0, 2.0, y1 + 1.0 }, &hatch);
    CHECK(rde_arr_length(&hatch) <= 2u * 20001u);
    rde_arr_free(&hatch);
    // A clip: the end it cuts moved onto the box, the one inside left exactly as it was; both inside, both kept.
    fude_zoom_v2 a = { 0.1, 0.2 }, b = { 0.7, 5.3 };
    CHECK(fude_zoom_clip_line(&a, &b, (fude_zoom_box){ -1.0, -1.0, 1.0, 1.0 }) && a.x == 0.1 && a.y == 0.2 && fabs(b.y - 1.0) < 1e-12);
    fude_zoom_v2 c = { 0.3, 0.1 }, e = { 0.9, -0.7 };
    CHECK(fude_zoom_clip_line(&c, &e, (fude_zoom_box){ -1.0, -1.0, 1.0, 1.0 }) && c.x == 0.3 && c.y == 0.1 && e.x == 0.9 && e.y == -0.7);
}

// A board cut by a grid of 48 saw cuts each way (96 bands): 9604 corners, more than the vertices' map held when it was
// sized from the edges (the crossings grow as their square, and a full map searched for ever). 49 × 49 pieces are
// left, their area what the bands leave.
static void test_cut_crossings(void) {
    printf("cut crossings\n");
    const fude_zoom_v2 board[4] = { { 0.0, 0.0 }, { 1000.0, 0.0 }, { 1000.0, 1000.0 }, { 0.0, 1000.0 } };
    fude_zoom_v2 cut[2u * 48u * 4u];
    u32 cut_rings[2u * 48u * 4u];
    u32 n = 0;
    for(u32 i = 0; i < 48u; i++) {
        const f64 m = (f64)(i + 1u) * 1000.0 / 49.0;
        const fude_zoom_v2 across[4] = { { -10.0, m - 1.0 }, { 1010.0, m - 1.0 }, { 1010.0, m + 1.0 }, { -10.0, m + 1.0 } };
        const fude_zoom_v2 down[4]   = { { m - 1.0, -10.0 }, { m + 1.0, -10.0 }, { m + 1.0, 1010.0 }, { m - 1.0, 1010.0 } };
        for(u32 k = 0; k < 4u; k++) { cut[n] = across[k]; cut_rings[n++] = 2u * i; }
        for(u32 k = 0; k < 4u; k++) { cut[n] = down[k]; cut_rings[n++] = 2u * i + 1u; }
    }
    rde_arr out = rde_arr_new(sizeof(fude_zoom_v2), NULL), out_rings = rde_arr_new(sizeof(u32), NULL), out_piece = rde_arr_new(sizeof(u32), NULL);
    const u32 pieces = fude_zoom_cut_region(board, NULL, 4u, cut, cut_rings, n, &out, &out_rings, &out_piece);
    const fude_zoom_v2* p = (const fude_zoom_v2*)out.memory;
    const u32* rg = (const u32*)out_rings.memory;
    const u32* pc = (const u32*)out_piece.memory;
    f64 area = 0.0;
    for(u32 from = 0; from < (u32)rde_arr_length(&out);) {
        u32 to = from;
        while(to < (u32)rde_arr_length(&out) && pc[to] == pc[from] && rg[to] == rg[from]) to++;
        area += fude_zoom_cut_area(&p[from], to - from);
        from = to;
    }
    const f64 left = 1000.0 * 1000.0 - 2.0 * 48.0 * 2.0 * 1000.0 + 48.0 * 48.0 * 4.0;
    printf("  %u pieces, %.3f of %.3f left\n", pieces, area, left);
    CHECK(pieces == 49u * 49u && fabs(area - left) < 1e-6 * left);
    rde_arr_free(&out); rde_arr_free(&out_rings); rde_arr_free(&out_piece);
}

// LaTeX nested past reason (anything can be pasted): \sqrt\sqrt… and \frac\frac… a hundred thousand deep are refused
// by texcalc, and \sqrt… and \hat… cut short by tex, before the stack runs out. The sizes (\bigl…) are dropped, but
// \bigcup and \bigstar are not taken for one: texcalc refuses them, tex shows \bigstar as written. \middle grows with
// its \left and \right; anywhere else it is reported.
static void test_deep_latex(void) {
    printf("deep latex\n");
    const u32 deep = 100000u;
    c8* s = (c8*)malloc(5u * deep + 8u);
    c8 out[256];
    usize n = 0;
    for(u32 i = 0; i < deep; i++) { memcpy(s + n, "\\sqrt", 5u); n += 5u; }
    s[n++] = '2';
    CHECK(!fude_zoom_texcalc(s, (u32)n, out, sizeof out));
    n = 0;
    for(u32 i = 0; i < deep; i++) { memcpy(s + n, "\\frac", 5u); n += 5u; }
    s[n++] = '1'; s[n++] = '2';
    CHECK(!fude_zoom_texcalc(s, (u32)n, out, sizeof out));
    const c8* ok = "\\sqrt\\sqrt{16} =";
    CHECK(fude_zoom_texcalc_answer(ok, (u32)strlen(ok), out, sizeof out) && strcmp(out, "2") == 0);
    // Sizes dropped; other names starting big refused.
    const c8* sized = "\\bigl( 1 + 2 \\bigr)", *biggest = "\\Biggl[ 3 \\Biggr]";
    CHECK(fude_zoom_texcalc(sized, (u32)strlen(sized), out, sizeof out) && strcmp(out, "( 1 + 2 )") == 0);
    CHECK(fude_zoom_texcalc(biggest, (u32)strlen(biggest), out, sizeof out) && strcmp(out, "( 3 )") == 0);
    static const c8* const not_sizes[3] = { "x \\bigcup y", "\\bigoplus x", "\\bigstar 2 =" };
    CHECK(!fude_zoom_texcalc(not_sizes[0], (u32)strlen(not_sizes[0]), out, sizeof out) && !fude_zoom_texcalc(not_sizes[1], (u32)strlen(not_sizes[1]), out, sizeof out));
    CHECK(!fude_zoom_texcalc_answer(not_sizes[2], (u32)strlen(not_sizes[2]), out, sizeof out));
    f64 ems = 0.0;
    c8 side = 'x';
    CHECK(fude_zoom_tex_sizing("Biggm", &ems, &side) && ems == 3.0 && side == 'm' && fude_zoom_tex_sizing("big", &ems, &side) && ems == 1.2 && side == 0);
    CHECK(!fude_zoom_tex_sizing("bigcup", NULL, NULL) && !fude_zoom_tex_sizing("biggest", NULL, NULL) && !fude_zoom_tex_sizing("Bigx", NULL, NULL));
    u32 size = 0;
    u8* bytes = maths_font_bytes(&size);
    CHECK(bytes != NULL);
    if(bytes != NULL) {
        fude_zoom_mfont f;
        fude_zoom_mfont_load(&f, bytes, size);
        fude_zoom_tex_box b; fude_zoom_tex_box_init(&b);
        static const c8* const nests[2] = { "\\sqrt", "\\hat" };
        for(u32 k = 0; k < 2u; k++) {
            const usize l = strlen(nests[k]);
            n = 0;
            for(u32 i = 0; i < deep; i++) { memcpy(s + n, nests[k], l); n += l; }
            s[n++] = 'x';
            fude_zoom_tex_set(&f, s, (u32)n, true, &b);
            CHECK(b.unknown >= 1u && isfinite(b.width) && isfinite(b.height) && isfinite(b.depth));
        }
        tex(&f, "\\bigstar x", true, &b);
        CHECK(b.unknown == 1u && tex_count(&b) >= 9u);   // (\bigstar as written, then x)
        tex(&f, "\\bigl( x \\bigr)", true, &b);
        CHECK(b.unknown == 0u && tex_count(&b) >= 3u);
        // \middle between \left. and \right.: its bar as tall as the fraction it stands by (not one line's height).
        tex(&f, "\\left. \\frac{a}{b} \\middle| c \\right.", true, &b);
        {
            const fude_zoom_tex_glyph* g = tex_glyphs(&b);
            const u32 letters[3] = { fude_zoom_mfont_glyph(&f, 0x1D44E), fude_zoom_mfont_glyph(&f, 0x1D44F), fude_zoom_mfont_glyph(&f, 0x1D450) };
            const f64 em = (f64)f.units_per_em;
            f64 top = -1e9, bottom = 1e9;
            for(u32 i = 0; i < tex_count(&b); i++) {
                if(g[i].glyph == letters[0] || g[i].glyph == letters[1] || g[i].glyph == letters[2]) continue;
                i32 box[4];
                fude_zoom_mfont_box(&f, g[i].glyph, box);
                top    = fmax(top, g[i].y + box[3] / em * g[i].size);
                bottom = fmin(bottom, g[i].y + box[1] / em * g[i].size);
            }
            printf("  \\middle's bar %.3f tall, the formula %.3f\n", top - bottom, b.height + b.depth);
            CHECK(b.unknown == 0u && top - bottom > 0.75 * (b.height + b.depth));
        }
        tex(&f, "a \\middle| b", true, &b);
        CHECK(b.unknown == 1u && tex_count(&b) == 3u);
        fude_zoom_tex_box_free(&b);
        free(bytes);
    }
    free(s);
}

// Make part with ports named alike: two inputs of the same 24 letters, after a third already named those and "_3"
// (the name the second's place gives it), each gets a name of its own (the renaming once went round for ever); a name
// of two-byte letters too long for a port is cut before a whole letter.
static void test_port_names(void) {
    printf("port names\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const c8* letters = "ABCDEFGHIJKLMNOPQRSTUVWX";
    part_put(&s, "logic input", 0, 200, 30, 20, "ABCDEFGHIJKLMNOPQRSTUVWX_3");
    part_put(&s, "logic input", 0, 100, 30, 20, letters);
    part_put(&s, "logic input", 0, 0, 30, 20, letters);
    part_put(&s, "logic input", 0, -100, 30, 20, "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9");   // (é × 18)
    part_put(&s, "AND gate", 200, 100, 40, 30, "");
    fude_zoom_circuit c; fude_zoom_circuit_init(&c);
    fude_zoom_circuit_build(&c, &s);
    u32 problem = 9;
    fude_sim_def* def = fude_zoom_logic_make(&c, &s, "user/names", "Names", &problem);
    CHECK(def != NULL && problem == FUDE_ZOOM_MAKE_OK);
    if(def != NULL) {
        const fude_sim_port* ports = (const fude_sim_port*)def->ports.memory;
        const u32 np = (u32)rde_arr_length(&def->ports);
        b8 apart = true, whole = true;
        for(u32 i = 0; i < np; i++) {
            for(u32 j = 0; j < i; j++) apart = apart && strcmp(ports[i].name, ports[j].name) != 0;
            if((u8)ports[i].name[0] == 0xC3u) {
                const usize l = strlen(ports[i].name);
                whole = whole && l % 2u == 0u && l + 2u >= sizeof(ports[i].name) - 1u;
                for(usize k = 0; k + 1u < l; k += 2u) whole = whole && (u8)ports[i].name[k] == 0xC3u && (u8)ports[i].name[k + 1u] == 0xA9u;
            }
            printf("  port %u: %s\n", i, ports[i].name);
        }
        CHECK(np == 4u && apart && whole);
        fude_sim_def_free(def);
    }
    fude_zoom_circuit_destroy(&c);
    fude_zoom_scene_destroy(&s);
}

// Numbers that are not what they should be: an offset of an outline with no extent (its grid divided by 0), or of one
// far smaller than the offset (a grid past what a u32 holds), and a sweep far longer than its reach (the same); an
// attribute's kind, material, part number and id, a connector's point count and heads and a symbol's kind, read from a
// file with numbers no u8, u32 or i32 holds (each cast was undefined); hex typed for a number ("0x10" is 0·x·10, not
// 16); a pie slice's name too long for its label keeping its share whole; a part that does not fit given a place all
// the same (it was left unwritten).
static void test_bad_numbers(void) {
    printf("bad numbers\n");
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    rde_arr out = rde_arr_new(sizeof(fude_zoom_v2), heap);
    const fude_zoom_v2 dot[3] = { { 5.0, 5.0 }, { 5.0, 5.0 }, { 5.0, 5.0 } }, speck[3] = { { 0.0, 0.0 }, { 1e-9, 0.0 }, { 0.0, 1e-9 } };
    const fude_zoom_v2 square[4] = { { 0.0, 0.0 }, { 10.0, 0.0 }, { 10.0, 10.0 }, { 0.0, 10.0 } };
    CHECK(fude_zoom_fill_offset(dot, 3u, 2.0, &out) == 0u && fude_zoom_fill_offset(speck, 3u, 10.0, &out) == 0u);
    CHECK(fude_zoom_fill_offset(square, 4u, 1.0, &out) > 8u);
    // A sweep far longer than its reach (its grid's width past what a u32 holds), and one through a point no number
    // holds: nothing traced, and no count made u32 that would not convert.
    const fude_zoom_v2 far_sweep[2] = { { 0.0, 0.0 }, { 1e13, 0.0 } }, inf_sweep[2] = { { 0.0, 0.0 }, { INFINITY, 0.0 } };
    CHECK(fude_zoom_fill_sweep(far_sweep, 2u, 1.0, &out) == 0u && fude_zoom_fill_sweep(inf_sweep, 2u, 1.0, &out) == 0u);
    CHECK(fude_zoom_fill_sweep(square, 2u, 1.0, &out) > 3u);   // (a short one as it always was)
    // A connector's point count and heads, and a symbol's kind, read from a file with numbers no u32 holds (each cast
    // was undefined): no points, no heads, no kind.
    {
        const f64 bad[3] = { NAN, 1e20, -3.0 };
        for(u32 i = 0; i < 3u; i++) {
            f64 arrow[10] = { bad[i], 0.0, 0.0, 10.0, 0.0, bad[i], 0.0, 0.0, 0.0, 0.0 };
            b8 closed = true;
            fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_ARROW, arrow, 10u, 16u, &out, &closed);
            CHECK(rde_arr_length(&out) == 0u && fude_zoom_arrow_points(arrow, 10u) == 0u && fude_zoom_arrow_heads(arrow, 10u, 2u) == 0u);
            fude_zoom_shape_mirror(FUDE_ZOOM_SHAPE_ARROW, arrow, 10u);
            const f64 sym[4] = { bad[i], 10.0, 10.0, 4.0 };
            fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_SYMBOL, sym, 4u, 16u, &out, &closed);
            CHECK(rde_arr_length(&out) == 0u && fude_zoom_symbol_kind_of(sym, 4u) == FUDE_ZOOM_NONE);
        }
        const f64 good[6] = { 2.0, 0.0, 0.0, 10.0, 0.0, (f64)FUDE_ZOOM_ARROW_END };
        CHECK(fude_zoom_arrow_points(good, 6u) == 2u && fude_zoom_arrow_heads(good, 6u, 2u) == FUDE_ZOOM_ARROW_END);
        // ...and a symbol of such a kind on a canvas: no part, no mechanism, no grid (each read its kind by a cast);
        // a cut board's counts (its rings, a ring's points) the same: no rings.
        fude_zoom_scene bs; fude_zoom_scene_init(&bs, 7);
        rde_arr rings = rde_arr_new(sizeof(u32), heap);
        for(u32 i = 0; i < 3u; i++) {
            const f64 sym[4] = { bad[i], 10.0, 10.0, 4.0 };
            CHECK(fude_zoom_part_of_numbers(sym, 4u) == NULL);
            const u32 o = fude_zoom_scene_add_shape(&bs, bs.root, (fude_zoom_place){ { 0.0, 0.0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL, sym, 4u, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
            CHECK(fude_zoom_part_of(&bs, o) == NULL && fude_zoom_mech_of(&bs, o) == NULL && fude_zoom_part_unit(&bs, o) == 0.0);
            f64 board[FUDE_ZOOM_BOARD_RINGS_AT + 8u];
            memset(board, 0, sizeof(board));
            board[0] = 10.0;
            board[1] = 5.0;
            board[FUDE_ZOOM_BOARD_RINGS_AT]      = 1.0;      // (one ring, of so many points)
            board[FUDE_ZOOM_BOARD_RINGS_AT + 1u] = bad[i];
            CHECK(fude_zoom_board_rings(board, FUDE_ZOOM_BOARD_RINGS_AT + 8u, &out, &rings) == 0u);
            board[FUDE_ZOOM_BOARD_RINGS_AT]      = 1e20;     // (so many rings, the first of three points)
            board[FUDE_ZOOM_BOARD_RINGS_AT + 1u] = 3.0;
            CHECK(fude_zoom_board_rings(board, FUDE_ZOOM_BOARD_RINGS_AT + 8u, &out, &rings) == 0u);
        }
        rde_arr_free(&rings);
        fude_zoom_scene_destroy(&bs);
    }
    rde_arr_free(&out);
    // Attributes with numbers past their types.
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const f64 box[3] = { 10.0, 10.0, 0.0 };
    const u32 thing = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0.0, 0.0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, box, 3u, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    f64 num[FUDE_ZOOM_PROPS_NUMBERS] = { 1e10, 0.0, 0.0, 1e20, 0.0, 0.0, 0.0, 1e20 };
    fude_zoom_id_put(&num[1], fude_zoom_scene_object(&s, thing)->id);
    const u32 bad = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0.0, 0.0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_PROPS, num, FUDE_ZOOM_PROPS_NUMBERS, (rde_color){ 1, 1, 1, 255 }, 0.0f, 0u, 0);
    fude_zoom_body_props bp;
    rde_color light;
    f64 most[4];
    i32 preset = 7;
    CHECK(fude_zoom_props_kind(&s, bad) == 0u && fude_zoom_props_find(&s, thing, FUDE_ZOOM_PROPS_BODY) == FUDE_ZOOM_NONE);
    CHECK(!fude_zoom_props_body(&s, bad, &bp) && !fude_zoom_props_light(&s, bad, &light) && !fude_zoom_props_limits(&s, bad, most, &preset));
    num[0] = NAN;
    const u32 not_a_kind = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0.0, 0.0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_PROPS, num, FUDE_ZOOM_PROPS_NUMBERS, (rde_color){ 1, 1, 1, 255 }, 0.0f, 0u, 0);
    CHECK(fude_zoom_props_kind(&s, not_a_kind) == 0u && fude_zoom_props_find(&s, thing, FUDE_ZOOM_PROPS_LIGHT) == FUDE_ZOOM_NONE);
    num[0] = (f64)FUDE_ZOOM_PROPS_BODY;
    const u32 body = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0.0, 0.0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_PROPS, num, FUDE_ZOOM_PROPS_NUMBERS, (rde_color){ 1, 1, 1, 255 }, 0.0f, 0u, 0);
    CHECK(fude_zoom_props_body(&s, body, &bp) && bp.material == 0u);
    num[0] = (f64)FUDE_ZOOM_PROPS_LIMITS;
    const u32 limits = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0.0, 0.0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_PROPS, num, FUDE_ZOOM_PROPS_NUMBERS, (rde_color){ 1, 1, 1, 255 }, 0.0f, 0u, 0);
    CHECK(fude_zoom_props_limits(&s, limits, most, &preset) && preset == -1);
    const f64 halves[2] = { 1e20, 3.0 }, minus[2] = { -1.0, 3.0 };
    CHECK(fude_zoom_id_get(halves) == 0u && fude_zoom_id_get(minus) == 0u);
    fude_zoom_scene_destroy(&s);
    // Typed numbers: decimal only.
    fude_zoom_calc fc;
    f64 v = 0.0;
    CHECK(!fude_zoom_calc_number("0x1p3", &v) && !(fude_zoom_calc_parse(&fc, "0x10") && fabs(fude_zoom_calc_at(&fc, 1.0) - 16.0) < 1e-9));
    CHECK(fude_zoom_calc_number("1e3", &v) && v == 1000.0 && fude_zoom_calc_number(".5", &v) && v == 0.5 && fude_zoom_calc_number("2.5e-1", &v) && v == 0.25);
    CHECK(fude_zoom_calc_parse(&fc, "2e") && fabs(fude_zoom_calc_at(&fc, 0.0) - 2.0 * exp(1.0)) < 1e-12);
    CHECK(fude_zoom_calc_decimal("0x2", false, &v) == 1u && v == 0.0 && fude_zoom_calc_decimal("-1.5e2x", true, &v) == 6u && v == -150.0);
    CHECK(fude_zoom_calc_decimal("inf", true, &v) == 0u && fude_zoom_calc_decimal("nan", true, &v) == 0u && fude_zoom_calc_decimal(".", true, &v) == 0u);
    CHECK(fude_zoom_calc_decimal("12.", false, &v) == 3u && v == 12.0 && fude_zoom_calc_decimal("-3", false, &v) == 0u);
    CHECK(fude_zoom_calc_decimal("-0x2", true, &v) == 2u && v == 0.0);
    // However long, read whole (one of 64 bytes or more was taken for no number): 1 and 69 noughts; a third to 80
    // places; 100 nines and an exponent, signed, an x after it.
    c8 longnum[128];
    memset(longnum, '0', sizeof(longnum));
    longnum[0] = '1';
    longnum[70] = 0;
    CHECK(fude_zoom_calc_decimal(longnum, false, &v) == 70u && v == 1e69);
    memcpy(longnum, "0.", 2u);
    memset(longnum + 2, '3', 80u);
    longnum[82] = 0;
    CHECK(fude_zoom_calc_decimal(longnum, false, &v) == 82u && fabs(v - 1.0 / 3.0) < 1e-15);
    longnum[0] = '-';
    memset(longnum + 1, '9', 100u);
    memcpy(longnum + 101, "e-105x", 7u);
    CHECK(fude_zoom_calc_decimal(longnum, true, &v) == 106u && fabs(v + 1e-5) < 1e-15);
    CHECK(fude_zoom_calc_number("1000000000000000000000000000000000000000000000000000000000000000000000/1e69", &v) && fabs(v - 1.0) < 1e-12);
    // A pie slice's label: its name cut (before a whole letter) so its share fits whole.
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), heap), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), heap), labels = rde_arr_new(sizeof(fude_zoom_plot_label), heap);
    fude_zoom_chart_lines("pie chart", "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrs 1\nB 3", 140.0, 120.0, &pts, &parts, &labels);
    const fude_zoom_plot_label* lb = (const fude_zoom_plot_label*)labels.memory;
    b8 share = false;
    for(u32 i = 0; i < (u32)rde_arr_length(&labels); i++) {
        const usize l = strlen(lb[i].text);
        if(l > 4u && strncmp(lb[i].text, "abc", 3u) == 0) share = strcmp(lb[i].text + l - 4u, " 25%") == 0;
    }
    CHECK(share);
    c8 text[96];
    usize tn = 0;
    for(u32 i = 0; i < 15u; i++) { memcpy(text + tn, "\xE6\x97\xA5", 3u); tn += 3u; }   // (日 × 15)
    memcpy(text + tn, " 1\nB 3", 7u);
    rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
    fude_zoom_chart_lines("pie chart", text, 140.0, 120.0, &pts, &parts, &labels);
    lb = (const fude_zoom_plot_label*)labels.memory;
    share = false;
    for(u32 i = 0; i < (u32)rde_arr_length(&labels); i++) {
        const usize l = strlen(lb[i].text);
        if(l > 4u && (u8)lb[i].text[0] == 0xE6u) {
            share = strcmp(lb[i].text + l - 4u, " 25%") == 0 && (l - 4u) % 3u == 0u;
            for(usize k = 0; share && k + 4u < l; k += 3u) share = memcmp(lb[i].text + k, "\xE6\x97\xA5", 3u) == 0;
        }
    }
    CHECK(share);
    // A value however long (one of 48 bytes or more was taken for none): B's 3, written to 50 places, is still 3.
    c8 longval[96];
    memcpy(longval, "A 1\nB 3.", 8u);
    memset(longval + 8, '0', 50u);
    longval[58] = 0;
    rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
    fude_zoom_chart_lines("pie chart", longval, 140.0, 120.0, &pts, &parts, &labels);
    lb = (const fude_zoom_plot_label*)labels.memory;
    share = false;
    for(u32 i = 0; i < (u32)rde_arr_length(&labels); i++) share = share || strcmp(lb[i].text, "A 25%") == 0;
    CHECK(share);
    rde_arr_free(&pts); rde_arr_free(&parts); rde_arr_free(&labels);
    // Fitting parts: one that does not fit is at 0, 0, its size as given, not turned.
    const fude_zoom_nest_part np[2] = { { 200.0, 50.0, true }, { 10.0, 10.0, false } };
    fude_zoom_nest_place pl[2];
    memset(pl, 0x5A, sizeof pl);
    CHECK(fude_zoom_nest(100.0, 100.0, 0.0, np, 2u, pl) == 1u && !pl[0].placed && pl[1].placed);
    CHECK(pl[0].x == 0.0 && pl[0].y == 0.0 && pl[0].w == 200.0 && pl[0].h == 50.0 && !pl[0].turned);
}

// A circle's size (RADIAL) keeps its circle's id as the scene keeps ids, in two halves: it follows its circle moved, is
// measured again when its circle is made again, and is joined to the copy when copied with it. An older one (four
// numbers, its id one number's bits) is still read, and written the new way when made again or copied.
static void test_radial_ids(void) {
    printf("radial ids\n");
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    s.camera = (fude_zoom_camera){ s.root, { 0, 0 }, 1.0 };
    const rde_color ink = { 1, 1, 1, 255 };
    const u32 ci = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, (const f64[2]){ 20, 20 }, 2u, ink, 1.0f, 0u, 0);
    f64 rn[FUDE_ZOOM_RADIAL_NUMBERS] = { 20.0, 0.0, 0.0, 0.0, 0.0 };
    fude_zoom_id_put(&rn[3], fude_zoom_scene_object(&s, ci)->id);
    CHECK(fude_zoom_connect_radial_id(rn, FUDE_ZOOM_RADIAL_NUMBERS) == fude_zoom_scene_object(&s, ci)->id);
    const u32 ra = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RADIAL, rn, FUDE_ZOOM_RADIAL_NUMBERS, ink, 0.5f, 0u, 0);
    // Its circle moved 100 across: it goes along.
    fude_zoom_selection sel; fude_zoom_select_init(&sel);
    rde_arr_add(&sel.picks, &(fude_zoom_pick){ ci });
    fude_zoom_select_update(&sel, &s);
    fude_zoom_select_begin(&sel, &s);
    fude_zoom_select_end(&sel, &s, (fude_zoom_sim){ 1.0, 0.0, 100.0, 0.0 });
    CHECK(fabs(fude_zoom_scene_object(&s, ra)->t.x - 100.0) < 1e-9);
    // Its circle made again, 30 round: measured again, its id the new circle's.
    const u32 c2 = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 100, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, (const f64[2]){ 30, 30 }, 2u, ink, 1.0f, 0u, 0);
    fude_zoom_scene_set_alive(&s, ci, false);
    const fude_zoom_id oid = fude_zoom_scene_object(&s, ci)->id, nid = fude_zoom_scene_object(&s, c2)->id;
    const u32 ra2 = fude_zoom_connect_dim_remap(&s, ra, &oid, &nid, 1u);
    f64 n[FUDE_ZOOM_SHAPE_NUMBERS];
    CHECK(ra2 != FUDE_ZOOM_NONE && fude_zoom_scene_shape_numbers(&s, ra2, n, FUDE_ZOOM_SHAPE_NUMBERS) == FUDE_ZOOM_RADIAL_NUMBERS);
    CHECK(fude_zoom_id_get(&n[3]) == nid && fabs(n[0] - 30.0) < 1e-9);
    // An older one on it: read, and made again the new way.
    f64 old4[4] = { 30.0, 0.0, 0.0, 0.0 };
    memcpy(&old4[3], &nid, sizeof nid);
    CHECK(fude_zoom_connect_radial_id(old4, 4u) == nid && fude_zoom_connect_radial_id(old4, 3u) == 0u);
    const u32 ra3 = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 100, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RADIAL, old4, 4u, ink, 0.5f, 0u, 0);
    const u32 c3 = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 100, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, (const f64[2]){ 40, 40 }, 2u, ink, 1.0f, 0u, 0);
    fude_zoom_scene_set_alive(&s, c2, false);
    const fude_zoom_id c3id = fude_zoom_scene_object(&s, c3)->id;
    const u32 ra4 = fude_zoom_connect_dim_remap(&s, ra3, &nid, &c3id, 1u);
    CHECK(ra4 != FUDE_ZOOM_NONE && fude_zoom_scene_shape_numbers(&s, ra4, n, FUDE_ZOOM_SHAPE_NUMBERS) == FUDE_ZOOM_RADIAL_NUMBERS);
    CHECK(fude_zoom_id_get(&n[3]) == c3id && fabs(n[0] - 40.0) < 1e-9);
    // Copied with its circle (a new one, and an older one): each copy on the circle's copy, its id in two halves.
    f64 old5[4] = { 40.0, 1.0, 0.0, 0.0 };
    memcpy(&old5[3], &c3id, sizeof c3id);
    const u32 ra5 = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 100, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RADIAL, old5, 4u, ink, 0.5f, 0u, 0);
    const u32 copies[2][2] = { { ra4, c3 }, { ra5, c3 } };
    rde_arr clips = rde_arr_new(sizeof(fude_zoom_clip), heap);
    const fude_zoom_sim up = { 1, 0, 0, 200 };
    for(u32 k = 0; k < 2u; k++) {
        fude_zoom_select_clips_of(&s, copies[k], 2u, &clips);
        fude_zoom_select_clear(&sel);
        const u32 base = fude_zoom_scene_object_count(&s);
        CHECK(fude_zoom_select_put(&sel, &s, (const fude_zoom_clip*)clips.memory, 2u, &up, 1u, NULL, NULL, 0u) == 2u);
        CHECK(fude_zoom_scene_object(&s, base)->channels == FUDE_ZOOM_SHAPE_ELLIPSE && fude_zoom_scene_object(&s, base + 1u)->channels == FUDE_ZOOM_SHAPE_RADIAL);
        CHECK(fude_zoom_scene_shape_numbers(&s, base + 1u, n, FUDE_ZOOM_SHAPE_NUMBERS) == FUDE_ZOOM_RADIAL_NUMBERS && fude_zoom_id_get(&n[3]) == fude_zoom_scene_object(&s, base)->id);
        fude_zoom_select_clips_free(&clips);
    }
    rde_arr_free(&clips);
    fude_zoom_select_destroy(&sel);
    fude_zoom_scene_destroy(&s);
}

// Mirrored shapes: a wall's, a guide's and a radius mark's way flips, an angle's from and sweep turn
// round, a wire stays.
static void mirror_kinds(void) {
    f64 wall[3] = { 30.0, 40.0, 2.0 }, guide[3] = { 1.0, 1.0, 500.0 }, radial[4] = { 20.0, 0.5, 0.0, 0.0 }, angle[3] = { 10.0, 0.25, 1.0 };
    fude_zoom_shape_mirror(FUDE_ZOOM_SHAPE_WALL, wall, 3u);
    fude_zoom_shape_mirror(FUDE_ZOOM_SHAPE_GUIDE, guide, 3u);
    fude_zoom_shape_mirror(FUDE_ZOOM_SHAPE_RADIAL, radial, 4u);
    fude_zoom_shape_mirror(FUDE_ZOOM_SHAPE_ANGLE, angle, 3u);
    CHECK(wall[0] == 30.0 && wall[1] == -40.0 && wall[2] == 2.0 && guide[1] == -1.0 && guide[2] == 500.0);
    CHECK(radial[0] == 20.0 && radial[1] == -0.5 && angle[0] == 10.0 && angle[1] == -0.25 && angle[2] == -1.0);
    f64 wire[11] = { 2.0, 0.0, 0.0, 10.0, 5.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0 };
    fude_zoom_shape_mirror(FUDE_ZOOM_SHAPE_WIRE, wire, 11u);
    CHECK(wire[4] == 5.0);
}

// Words in parts' texts (circuit.h's fude_zoom_circuit_word): whole words only. The reader itself (Latin words whole,
// accented letters counting as letters, Japanese anywhere, the longest where it begins); then switches, logic inputs
// and changeovers as their texts start them (a switch named "SW1" or "contact" open, a logic input "A1" or "Button"
// low, a changeover "Blink" not thrown); then LED colours in the apps' languages ("infrared" not red, "Bluetooth" not
// blue), the same table the page draws their light from.
static void test_part_words(void) {
    printf("words in parts' texts\n");
    static const fude_zoom_word w[] = {
        { "on", 1u }, { "1", 2u }, { "red", 3u }, { "cw", 4u }, { "ccw", 5u },
        { "\xE6\x99\x82\xE8\xA8\x88\xE5\x9B\x9E\xE3\x82\x8A", 4u },               // 時計回り
        { "\xE5\x8F\x8D\xE6\x99\x82\xE8\xA8\x88\xE5\x9B\x9E\xE3\x82\x8A", 5u },   // 反時計回り
    };
    const u32 nw = (u32)(sizeof w / sizeof w[0]);
    CHECK(fude_zoom_circuit_word("contact", w, nw, 0u) == 0u && fude_zoom_circuit_word("S1 on", w, nw, 0u) == 1u && fude_zoom_circuit_word("ON", w, nw, 0u) == 1u);
    CHECK(fude_zoom_circuit_word("SW1", w, nw, 0u) == 0u && fude_zoom_circuit_word("A1", w, nw, 0u) == 0u && fude_zoom_circuit_word("1", w, nw, 0u) == 2u);
    CHECK(fude_zoom_circuit_word("infrared", w, nw, 0u) == 0u && fude_zoom_circuit_word("red, 5mm", w, nw, 0u) == 3u);
    CHECK(fude_zoom_circuit_word("30 rpm ccw", w, nw, 0u) == 5u && fude_zoom_circuit_word("30 rpm cw", w, nw, 0u) == 4u && fude_zoom_circuit_word("CCW", w, nw, 0u) == 5u);
    CHECK(fude_zoom_circuit_word("\xE5\x8F\x8D\xE6\x99\x82\xE8\xA8\x88\xE5\x9B\x9E\xE3\x82\x8A", w, nw, 0u) == 5u);
    CHECK(fude_zoom_circuit_word("30rpm\xE6\x99\x82\xE8\xA8\x88\xE5\x9B\x9E\xE3\x82\x8A", w, nw, 0u) == 4u);   // (Japanese: no spaces needed)
    CHECK(fude_zoom_circuit_word("on\xC3\xA9", w, nw, 0u) == 0u && fude_zoom_circuit_word("\xC3\xA9on", w, nw, 0u) == 0u);    // (é is a letter)
    CHECK(fude_zoom_circuit_word("x-on", w, nw, 0u) == 1u && fude_zoom_circuit_word("(on)", w, nw, 0u) == 1u);
    CHECK(fude_zoom_circuit_word(NULL, w, nw, 9u) == 9u && fude_zoom_circuit_word("", w, nw, 9u) == 9u);
    // How switches, logic inputs and changeovers start.
    static const struct { const c8* id; const c8* text; u32 on; } sw[] = {
        { "SPST switch", "SW1", 0u }, { "SPST switch", "contact", 0u }, { "SPST switch", "S1 on", 1u }, { "SPST switch", "closed", 1u },
        { "SPST switch", "1", 1u }, { "SPST switch", "off", 0u }, { "SPST switch", "ON", 1u },
        { "logic input", "A1", 0u }, { "logic input", "Button", 0u }, { "logic input", "1", 1u }, { "logic input", "HIGH", 1u },
        { "SPDT switch", "SW B", 1u }, { "SPDT switch", "Blink", 0u }, { "SPDT switch", "A", 0u }, { "SPDT switch", "B", 1u },
        // (in the apps' other languages too, as an LED's colour is: the first level word says it)
        { "SPST switch", "encendido", 1u }, { "SPST switch", "ligado", 1u }, { "SPST switch", "ferm\xC3\xA9", 1u },
        { "SPST switch", "apagado", 0u }, { "SPST switch", "\xE3\x82\xAA\xE3\x83\xB3", 1u },   // オン
        { "logic input", "alto", 1u }, { "logic input", "haut", 1u }, { "logic input", "bajo 1", 0u }, { "logic input", "Altitude", 0u },
    };
    for(u32 i = 0; i < (u32)(sizeof sw / sizeof sw[0]); i++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 o = part_put(&s, sw[i].id, 0, 0, 30, 20, sw[i].text);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        const fude_zoom_circuit_part* p = cpart(&c, o);
        if(p == NULL || (p->switch_on & 1u) != sw[i].on) printf("  %s \"%s\": not %s\n", sw[i].id, sw[i].text, sw[i].on ? "on" : "off");
        CHECK(p != NULL && (p->switch_on & 1u) == sw[i].on);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // A level word whole is a level (a logic input's text so is no name for Make part's port); in a
    // name, or blinking, not.
    CHECK(fude_zoom_circuit_level_word("ON") && fude_zoom_circuit_level_word("1") && fude_zoom_circuit_level_word("bajo"));
    CHECK(fude_zoom_circuit_level_word("\xE3\x82\xAA\xE3\x83\x95") && fude_zoom_circuit_level_word("ferm\xC3\xA9"));   // オフ
    CHECK(!fude_zoom_circuit_level_word("ON_1") && !fude_zoom_circuit_level_word("EN") && !fude_zoom_circuit_level_word("blink"));
    CHECK(!fude_zoom_circuit_level_word("") && !fude_zoom_circuit_level_word(NULL));
    CHECK(fude_zoom_circuit_level("D13 parpadeo", 9u) == FUDE_ZOOM_LEVEL_BLINK && fude_zoom_circuit_level("SW1", 9u) == 9u);
    // A stepper's steps and a lit sensor's light: the number by its word (before it, or after it), whole; a number of
    // its name is not it ("28BYJ-48 stepper": 28 is no steps), nor a word inside another ("flux" holds no lux).
    static const struct { const c8* id; const c8* text; f64 want; } by[] = {
        { "stepper motor", "28BYJ-48 stepper", 2048.0 }, { "stepper motor", "NEMA17 200 steps", 200.0 }, { "stepper motor", "steps: 400", 400.0 },
        { "stepper motor", "48 pasos", 48.0 }, { "stepper motor", "28BYJ-48", 2048.0 },
        { "stepper motor", "100 \xE3\x82\xB9\xE3\x83\x86\xE3\x83\x83\xE3\x83\x97", 100.0 },   // 100 ステップ
        { "LDR", "300 \xE3\x83\xAB\xE3\x82\xAF\xE3\x82\xB9", 300.0 },                          // 300 ルクス
        { "LDR", "500 lux", 500.0 }, { "LDR", "1k lux", 1000.0 }, { "LDR", "flux 5", 100.0 }, { "LDR", "GL5528", 100.0 },
        { "photodiode", "BPW34, lux 20", 20.0 },
    };
    for(u32 i = 0; i < (u32)(sizeof by / sizeof by[0]); i++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 o = part_put(&s, by[i].id, 0, 0, 30, 20, by[i].text);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        const fude_zoom_circuit_part* p = cpart(&c, o);
        const f64 got = p == NULL ? -1.0 : (p->part->model == FUDE_ZOOM_MODEL_STEPPER ? p->value[0] : p->state[0]);
        if(fabs(got - by[i].want) > 1e-9) printf("  %s \"%s\": %g, not %g\n", by[i].id, by[i].text, got, by[i].want);
        CHECK(fabs(got - by[i].want) < 1e-9);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // A board's pins as its lines say, in any of the languages: blinking ("parpadeo"), high ("alto"), low ("bas").
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 o = part_put(&s, "Arduino Uno", 0, 0, 80, 120, "Arduino Uno\nD13 parpadeo\nD12 alto\nD11 bas");
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        const fude_zoom_circuit_part* p = cpart(&c, o);
        u32 pin[3] = { 64u, 64u, 64u };
        static const c8* const named[3] = { "D13", "D12", "D11" };
        for(u32 k = 0; p != NULL && k < p->part->pin_count; k++) {
            for(u32 j = 0; j < 3u; j++) if(p->part->pins[k].name != NULL && strcmp(p->part->pins[k].name, named[j]) == 0) pin[j] = k;
        }
        CHECK(p != NULL && pin[0] < 64u && pin[1] < 64u && pin[2] < 64u);
        if(p != NULL && pin[0] < 64u && pin[1] < 64u && pin[2] < 64u) {
            CHECK((p->drive >> pin[0] & 1u) && (p->drive_blink >> pin[0] & 1u));
            CHECK((p->drive >> pin[1] & 1u) && (p->drive_high >> pin[1] & 1u) && !(p->drive_blink >> pin[1] & 1u));
            CHECK((p->drive >> pin[2] & 1u) && !(p->drive_high >> pin[2] & 1u) && !(p->drive_blink >> pin[2] & 1u));
        }
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // LED colours.
    static const struct { const c8* text; const c8* color; f64 volts; } leds[] = {
        { "infrared", "infrared", 1.2 }, { "IR 940nm", "infrared", 1.2 }, { "rojo", "red", 1.8 }, { "vermelho", "red", 1.8 },
        { "bleu", "blue", 3.0 }, { "Blue", "blue", 3.0 }, { "verde 5mm", "green", 2.2 }, { "jaune", "yellow", 2.1 }, { "rose", "pink", 3.0 },
        { "\xE8\xB5\xA4", "red", 1.8 },                                  // 赤
        { "\xE8\xB5\xA4\xE5\xA4\x96\xE7\xB7\x9A", "infrared", 1.2 },     // 赤外線 (not the 赤 in it)
        { "\xE7\xB7\x91\xE8\x89\xB2", "green", 2.2 },                    // 緑色
        { "Bluetooth", "red", 1.8 }, { "Greenhouse", "red", 1.8 }, { "LED1", "red", 1.8 },
    };
    for(u32 i = 0; i < (u32)(sizeof leds / sizeof leds[0]); i++) {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 o = part_put(&s, "LED", 0, 0, 30, 20, leds[i].text);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
        const fude_zoom_circuit_part* p = cpart(&c, o);
        if(p == NULL || strcmp(p->color, leds[i].color) != 0) printf("  LED \"%s\": %s, not %s\n", leds[i].text, p != NULL ? p->color : "?", leds[i].color);
        CHECK(p != NULL && strcmp(p->color, leds[i].color) == 0 && fabs(p->value[0] - leds[i].volts) < 1e-12);
        fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
    }
    // The one table: a colour's English name reads back as itself, and its light is the one drawn.
    for(u8 k = 0; k < FUDE_ZOOM_LED_COLORS; k++) CHECK(fude_zoom_led_color_of(fude_zoom_led_color_get(k)->name) == k);
    const rde_color pink = fude_zoom_led_color_get(fude_zoom_led_color_of("rosa"))->light;
    CHECK(pink.r == 255 && pink.g == 110 && pink.b == 200 && fude_zoom_led_color_of("nothing at all") == FUDE_ZOOM_LED_RED);
}

// What a tap says, in no language, and the word the page says with it (fude_zoom_circuit_tap_word): a switch says
// nothing but its word (on, off); a light sensor its lux as a number alone; a board's pin its name, then high, low,
// blinking and an input again; a logic input its level, with no word.
static void test_tap_words(void) {
    printf("tap words\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 sw  = part_put(&s, "SPST switch", 0, 0, 30, 20, "off");
    const u32 ldr = part_put(&s, "LDR", 0, 100, 30, 20, "GL5528");
    const u32 li  = part_put(&s, "logic input", 0, 200, 30, 20, "0");
    const u32 uno = part_put(&s, "Arduino Uno", 300, 0, 80, 120, "Arduino Uno");
    fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
    c8 say[64];
    CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw), -1, say, sizeof say) && say[0] == 0 && fude_zoom_circuit_tap_word(cpart(&c, sw), -1) == FUDE_ZOOM_TAP_ON);
    CHECK(fude_zoom_circuit_tap(&c, cindex(&c, sw), -1, say, sizeof say) && say[0] == 0 && fude_zoom_circuit_tap_word(cpart(&c, sw), -1) == FUDE_ZOOM_TAP_OFF);
    CHECK(fude_zoom_circuit_tap(&c, cindex(&c, ldr), -1, say, sizeof say) && say[0] != 0 && strspn(say, "0123456789") == strlen(say));
    CHECK(fude_zoom_circuit_tap_word(cpart(&c, ldr), -1) == FUDE_ZOOM_TAP_LUX);
    CHECK(fude_zoom_circuit_tap(&c, cindex(&c, li), -1, say, sizeof say) && strcmp(say, "1") == 0 && fude_zoom_circuit_tap_word(cpart(&c, li), -1) == FUDE_ZOOM_TAP_PLAIN);
    const fude_zoom_circuit_part* b = cpart(&c, uno);
    i32 d13 = -1;
    for(u32 k = 0; b != NULL && k < b->part->pin_count; k++) if(b->part->pins[k].name != NULL && strcmp(b->part->pins[k].name, "D13") == 0) d13 = (i32)k;
    CHECK(d13 >= 0);
    static const u8 order[4] = { FUDE_ZOOM_TAP_HIGH, FUDE_ZOOM_TAP_LOW, FUDE_ZOOM_TAP_BLINK, FUDE_ZOOM_TAP_INPUT };
    for(u32 k = 0; k < 4u && d13 >= 0; k++) {
        CHECK(fude_zoom_circuit_tap(&c, cindex(&c, uno), d13, say, sizeof say) && strcmp(say, "D13") == 0 && fude_zoom_circuit_tap_word(cpart(&c, uno), d13) == order[k]);
    }
    fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
}

// A panel meter's unit read as a whole token (display.h): "AC 20V" in volts (its A in a word), "2 A" in amperes.
static void test_meter_units(void) {
    const fude_zoom_meter ac = fude_zoom_display_meter("AC 20V"), back = fude_zoom_display_meter("20 V AC"), amps = fude_zoom_display_meter("2 A");
    const fude_zoom_meter ma = fude_zoom_display_meter("DC 200mA"), mv = fude_zoom_display_meter("200 mV"), named = fude_zoom_display_meter("Ammeter 2A");
    CHECK(!ac.amps && ac.full == 20.0 && ac.times == 1.0);
    CHECK(!back.amps && back.full == 20.0);
    CHECK(amps.amps && amps.full == 2.0 && amps.times == 1.0);
    CHECK(ma.amps && ma.times == 1000.0 && fabs(ma.full - 0.2) < 1e-12);
    CHECK(!mv.amps && mv.times == 1000.0);
    CHECK(named.amps && named.full == 2.0);
    CHECK(!fude_zoom_display_meter("Volts").amps && !fude_zoom_display_meter("").amps && fude_zoom_display_meter("").full == 20.0);
}

// A bar graph's limits measured LED by LED from its pins (limits.h): LED 3's current through its own pins, LED 7 the
// wrong way round by 5 V — not pin 0's current and no reverse voltage, as a two-pin part would be measured.
static void test_display_limits(void) {
    const fude_zoom_part* bar = fude_zoom_part_find("LED bar graph");
    CHECK(bar != NULL && bar->pin_count >= 20u);
    if(bar == NULL || bar->pin_count < 20u) return;
    f64 v[20], a[20], m[FUDE_ZOOM_LIMIT_COUNT];
    for(u32 k = 0; k < 20u; k++) { v[k] = 0.0; a[k] = 0.0; }
    a[3] = 0.05; a[13] = -0.05;   // (in at A4, out at K4)
    v[17] = 5.0;                   // (K8 five volts over A8)
    fude_zoom_limits_measure(bar, v, a, 20u, m);
    CHECK(fabs(m[FUDE_ZOOM_LIMIT_CURRENT] - 0.05) < 1e-12 && fabs(m[FUDE_ZOOM_LIMIT_REVERSE] - 5.0) < 1e-12);
}

// Wires left on nothing, a long chain of them (circuit.h's fude_zoom_wire_orphans): 1500 branches, each from the middle
// of the one before, made last first, so that each goes only once the one it is on has gone; the first at a
// resistor's pin. The resistor deleted: every one of them goes, and one elsewhere stays — in a moment (each end looked
// at once, not every wire against every wire round after round: that took minutes).
static void test_orphans_chain(void) {
    printf("wires left on nothing, a chain\n");
    const u32 n = 1500u;
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 r = part_put(&s, "resistor", 0, 0, 30, 6, "1k");
    fude_zoom_v2 p;
    CHECK(fude_zoom_part_pin_at(&s, r, 1, &p));
    // Wire i from a[i] to b[i]: the first from the pin, along x; each after from the one before's middle, across it.
    rde_arr ends = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    rde_arr_resize(&ends, 2u * (n + 1u));
    fude_zoom_v2* e = (fude_zoom_v2*)ends.memory;
    e[0] = p;
    e[1] = (fude_zoom_v2){ p.x + 100.0, p.y };
    for(u32 i = 1; i <= n; i++) {
        const fude_zoom_v2 mid = { (e[2u * (i - 1u)].x + e[2u * (i - 1u) + 1u].x) * 0.5, (e[2u * (i - 1u)].y + e[2u * (i - 1u) + 1u].y) * 0.5 };
        e[2u * i] = mid;
        e[2u * i + 1u] = (i % 2u) == 1u ? (fude_zoom_v2){ mid.x, mid.y + 100.0 } : (fude_zoom_v2){ mid.x + 100.0, mid.y };
    }
    rde_arr wires = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    for(u32 j = 0; j <= n; j++) {
        const u32 i = n - j;   // (the last first)
        const u32 w = fude_zoom_wire_add(&s, s.root, &e[2u * i], 2u, i == 0u ? r : FUDE_ZOOM_NONE, i == 0u ? 1 : -1, FUDE_ZOOM_NONE, -1, (rde_color){ 1, 1, 1, 255 }, 0.1f);
        rde_arr_add(&wires, (any)&w);
    }
    const fude_zoom_v2 far[2] = { { -5000.0, -5000.0 }, { -4900.0, -5000.0 } };
    const u32 other = fude_zoom_wire_add(&s, s.root, far, 2u, FUDE_ZOOM_NONE, -1, FUDE_ZOOM_NONE, -1, (rde_color){ 1, 1, 1, 255 }, 0.1f);
    rde_arr died = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    // Nothing died: nothing goes.
    CHECK(fude_zoom_wire_orphans(&s, &died) == 0u && rde_arr_length(&died) == 0u);
    fude_zoom_scene_set_alive(&s, r, false);
    rde_arr_add(&died, (any)&r);
    const clock_t t0 = clock();
    const u32 made = fude_zoom_wire_orphans(&s, &died);
    const f64 secs = (f64)(clock() - t0) / (f64)CLOCKS_PER_SEC;
    printf("  %u wires let go in %.3f s\n", made, secs);
    CHECK(made == n + 1u && rde_arr_length(&died) == n + 2u);
    u32 alive = 0;
    for(u32 j = 0; j <= n; j++) alive += (fude_zoom_scene_object(&s, ((const u32*)wires.memory)[j])->flags & FUDE_ZOOM_FLAG_ALIVE) ? 1u : 0u;
    CHECK(alive == 0u && (fude_zoom_scene_object(&s, other)->flags & FUDE_ZOOM_FLAG_ALIVE));
    CHECK(secs < 5.0);
    rde_arr_free(&died);
    rde_arr_free(&wires);
    rde_arr_free(&ends);
    fude_zoom_scene_destroy(&s);
}

// A recording's links (audio.h's fude_zoom_audio_begun): the stroke the pen drew linked to when the pen went down; a
// text or a shape made after it to its own time, not the stroke's; a pen-down before the recording, or too long ago,
// not at all.
static void test_audio_begun(void) {
    const f64 started = 100.0;
    CHECK(fude_zoom_audio_begun(110.0, started - 1.0, started, 112.0) == 110.0);   // (the stroke: when the pen went down)
    CHECK(fude_zoom_audio_begun(110.0, 110.0, started, 115.0) == 115.0);           // (a text after it: its own time)
    CHECK(fude_zoom_audio_begun(120.0, 110.0, started, 121.0) == 120.0);           // (the next stroke: its own pen-down)
    CHECK(fude_zoom_audio_begun(90.0, started - 1.0, started, 112.0) == 112.0);    // (the pen went down before recording)
    CHECK(fude_zoom_audio_begun(110.0, started - 1.0, started, 110.0 + FUDE_ZOOM_AUDIO_DOWN_MOST + 1.0) == 110.0 + FUDE_ZOOM_AUDIO_DOWN_MOST + 1.0);
}

// Lines drawn through the engine (rde_rendering_2d_draw_line_1; engine.c's double draws nothing): counted, and whether
// every one was near the screen, for the lasso's box drawn far in (test_far_lasso).
static u32 g_lines = 0;
static b8  g_lines_near = true;
void rde_rendering_2d_draw_line_1(const rde_vec_2F _init, rde_vec_2F _end, rde_color _color, f32 _thickness) {
    (void)_color; (void)_thickness;
    g_lines++;
    g_lines_near = g_lines_near && fabsf(_init.x) < 1e4f && fabsf(_init.y) < 1e4f && fabsf(_end.x) < 1e4f && fabsf(_end.y) < 1e4f;
}

// The lasso's box round a rectangle 2e18 across, its sides dashed on the screen: a dash's place far along a side, a
// period added to it, did not move (an f64 past 2^53 periods), and the drawing never ended. Its dashes now: those on
// the screen alone, near it. A box on the screen as it was: from each corner, a dash each 10 points.
static void test_far_lasso(void) {
    printf("the lasso's box far in\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    s.camera = (fude_zoom_camera){ s.root, { 0, 0 }, 1.0 };
    const fude_zoom_v2 half = { 400, 300 };
    const f64 huge[3] = { 1e18, 20, 0 }, small[3] = { 50, 20, 0 };
    const u32 big = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, huge, 3u, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    const u32 box = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 100 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, small, 3u, (rde_color){ 1, 1, 1, 255 }, 1.0f, 0u, 0);
    fude_zoom_selection sel; fude_zoom_select_init(&sel);
    fude_zoom_renderer rr; fude_zoom_render_init(&rr);   // (the suite draws through the real renderer: its glow)
    sel.half = half;
    rde_arr_add(&sel.picks, &(fude_zoom_pick){ big });
    fude_zoom_select_update(&sel, &s);
    fude_zoom_box b;
    CHECK(fude_zoom_select_box(&sel, &b) && b.max_x - b.min_x >= 2e18);
    g_lines = 0; g_lines_near = true;
    fude_zoom_select_render(&sel, &rr, &s, half);
    printf("  %u lines drawn\n", g_lines);
    CHECK(g_lines > 100u && g_lines < 400u && g_lines_near);   // (two sides across the screen, 82 dashes each; its knob)
    // On the screen: 100 by 40, drawn padded (FUDE_ZOOM_SELECT_PAD, 8 each way) to 116 by 56: 12 dashes along and 6 up,
    // each way round, and its knob's line.
    fude_zoom_select_clear(&sel);
    rde_arr_add(&sel.picks, &(fude_zoom_pick){ box });
    fude_zoom_select_update(&sel, &s);
    g_lines = 0; g_lines_near = true;
    fude_zoom_select_render(&sel, &rr, &s, half);
    CHECK(g_lines == 37u && g_lines_near);
    fude_zoom_render_destroy(&rr);
    fude_zoom_select_destroy(&sel);
    fude_zoom_scene_destroy(&s);
}

// A polar paper's rings, cut to the view: a chord inside it ends exactly where the next begins. The cut (zoom.h's
// fude_zoom_clip_line) leaves an end it does not cut as it was; paper.c's own worked out a + (b - a) for it, which
// rounds off b about once in fifty.
static void test_paper_joins(void) {
    printf("paper's rings joined exactly\n");
    rde_arr lines = rde_arr_new(sizeof(fude_zoom_paper_line), rde_memory_allocator_get_default_std());
    CHECK(fude_zoom_paper_lines(FUDE_PAPER_POLAR, (fude_zoom_box){ -173.0, -131.0, 157.0, 149.0 }, 20.0, 0.5, 3.7, &lines) > 0u);
    const fude_zoom_paper_line* l = (const fude_zoom_paper_line*)lines.memory;
    u32 joins = 0, exact = 0;
    for(u32 i = 0; i + 1u < (u32)rde_arr_length(&lines); i++) {
        const f64 gap = fabs(l[i + 1u].a.x - l[i].b.x) + fabs(l[i + 1u].a.y - l[i].b.y);
        if(gap <= 1e-6 * (fabs(l[i].b.x) + fabs(l[i].b.y) + 1.0)) {
            joins++;
            exact += gap == 0.0 ? 1u : 0u;
        }
    }
    printf("  %u joins, %u exact\n", joins, exact);
    CHECK(joins > 500u && exact == joins);
    // The paper's lines counted by index (paper.h's fude_zoom_paper_count): none 2^53 steps from 0, none too many.
    CHECK(fude_zoom_paper_count(-2.0, 3.0, 10.0) == 6u && fude_zoom_paper_count(3.0, 2.0, 10.0) == 0u && fude_zoom_paper_count(0.0, 20.0, 10.0) == 0u);
    CHECK(fude_zoom_paper_count(1e16, 1e16 + 2.0, 10.0) == 0u && fude_zoom_paper_count(NAN, 1.0, 10.0) == 0u);
    rde_arr_free(&lines);
}

// Writes a big-endian u16 and u32 into a font being made.
static void be16(u8* p, u32 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static void be32(u8* p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }

// A damaged font: glyph 0 a composite of 64 parts, each glyph 0 itself. Read to its depth of 8, that is 64^8 glyphs (a
// hang); its outline is now given up once it has cost FZMF_OUTLINE_MOST: nothing, at once. A real composite (glyph 2:
// glyph 1's triangle three times) is read as ever.
static void test_font_loops(void) {
    printf("a font's composite glyph made of itself\n");
    enum { HEAD = 128, HHEA = 192, MAXP = 240, CMAP = 256, HMTX = 264, LOCA = 272, GLYF = 288, G0 = 10 + 64 * 8, G1 = 30, G2 = 10 + 3 * 8, SIZE = GLYF + G0 + G1 + G2 };
    static u8 font[SIZE];
    memset(font, 0, sizeof font);
    be32(font, 0x00010000u);
    be16(font + 4, 7u);
    const u32 tags[7] = { 0x68656164u, 0x68686561u, 0x6D617870u, 0x636D6170u, 0x686D7478u, 0x6C6F6361u, 0x676C7966u };   // head hhea maxp cmap hmtx loca glyf
    const u32 offs[7] = { HEAD, HHEA, MAXP, CMAP, HMTX, LOCA, GLYF };
    for(u32 i = 0; i < 7u; i++) { be32(font + 12 + 16 * i, tags[i]); be32(font + 12 + 16 * i + 8, offs[i]); }
    be16(font + HEAD + 18, 1000u);   // units per em
    be16(font + HEAD + 50, 1u);      // loca's offsets 32-bit
    be16(font + HHEA + 34, 1u);      // one long metric
    be16(font + MAXP + 4, 3u);       // three glyphs
    be32(font + LOCA, 0u); be32(font + LOCA + 4, G0); be32(font + LOCA + 8, G0 + G1); be32(font + LOCA + 12, G0 + G1 + G2);
    // Glyph 0: 64 parts (offsets in words, more after each), every one glyph 0.
    u8* g = font + GLYF;
    be16(g, 0xFFFFu);
    for(u32 k = 0; k < 64u; k++) { be16(g + 10 + 8 * k, 0x0021u); be16(g + 12 + 8 * k, 0u); }
    // Glyph 1: a triangle, its three points on the curve, their moves in words.
    g = font + GLYF + G0;
    be16(g, 1u); be16(g + 10, 2u); be16(g + 12, 0u);
    g[14] = g[15] = g[16] = 0x01u;
    be16(g + 17, 0u); be16(g + 19, 100u); be16(g + 21, (u32)(u16)(i16)-50);
    be16(g + 23, 0u); be16(g + 25, 0u); be16(g + 27, 100u);
    // Glyph 2: glyph 1 three times, 200 apart (its moves as offsets: flag 2).
    g = font + GLYF + G0 + G1;
    be16(g, 0xFFFFu);
    for(u32 k = 0; k < 3u; k++) { be16(g + 10 + 8 * k, k < 2u ? 0x0023u : 0x0003u); be16(g + 12 + 8 * k, 1u); be16(g + 14 + 8 * k, 200u * k); be16(g + 16 + 8 * k, 0u); }
    fude_zoom_mfont f;
    CHECK(fude_zoom_mfont_load(&f, font, SIZE) && f.glyphs == 3u);
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    rde_arr pts = rde_arr_new(sizeof(f64), heap), ends = rde_arr_new(sizeof(u32), heap);
    CHECK(fude_zoom_mfont_outline(&f, 0u, 2.0, &pts, &ends) == 0u && rde_arr_length(&pts) == 0u);
    CHECK(fude_zoom_mfont_outline(&f, 2u, 2.0, &pts, &ends) == 3u);
    const f64* p = (const f64*)pts.memory;
    f64 most = 0.0;
    for(u32 i = 0; i < (u32)rde_arr_length(&pts); i += 2u) most = fmax(most, p[i]);
    CHECK(most == 500.0);   // (the third triangle's right corner: 400 + 100)
    rde_arr_free(&pts); rde_arr_free(&ends);
}

// A custom part of 70 ports on the canvas: a pin for each, named as its port (there were 64, the rest left off), the
// inputs down its left, the outputs down its right; made once; as a package too.
static void test_wide_part_pins(void) {
    printf("a custom part of 70 ports, its pins\n");
    fude_sim_def* d = fude_sim_def_new("user/pins70", "seventy");
    for(u32 k = 0; k < 70u; k++) {
        c8 n[8];
        snprintf(n, sizeof n, "%c%u", k < 35u ? 'I' : 'O', k % 35u);
        fude_sim_def_port(d, n, FUDE_SIM_LOGIC, k < 35u ? FUDE_SIM_IN : FUDE_SIM_OUT, 1u);
    }
    const fude_zoom_part* t = fude_zoom_part_find("custom part");
    CHECK(t != NULL);
    const fude_zoom_part* p = t != NULL ? fude_zoom_logic_part(t, d, false) : NULL;
    CHECK(p != NULL && p->pin_count == 70u);
    if(p != NULL && p->pin_count == 70u) {
        CHECK(strcmp(p->pins[0].name, "I0") == 0 && strcmp(p->pins[69].name, "O34") == 0);
        CHECK(p->pins[0].side == FUDE_ZOOM_PIN_LEFT && p->pins[69].side == FUDE_ZOOM_PIN_RIGHT);
        CHECK(fude_zoom_logic_part(t, d, false) == p);
        const fude_zoom_part* q = fude_zoom_logic_part(t, d, true);
        CHECK(q != NULL && q != p && q->pin_count == 70u && strcmp(q->pins[69].name, "O34") == 0);
    }
    fude_sim_def_free(d);
    // Every one of its pins joined in a circuit (it joined the first 64 once; the rest were drawn and joined to
    // nothing): each output the NOT of its input, its first pair and its last (pins 34 and 69) wired to logic inputs
    // and probes, as they say; and a part made of it (Make part) keeps all 70 of its joins.
    {
        fude_sim_def* w = fude_sim_def_new("user/notseventy", "seventy nots");
        for(u32 k = 0; k < 70u; k++) {
            c8 n[8];
            snprintf(n, sizeof n, "%c%u", k < 35u ? 'I' : 'O', k % 35u);
            fude_sim_def_port(w, n, FUDE_SIM_LOGIC, k < 35u ? FUDE_SIM_IN : FUDE_SIM_OUT, 1u);
        }
        for(u32 k = 0; k < 35u; k++) {
            c8 i[8], o[8], r[8];
            snprintf(i, sizeof i, "I%u", k); snprintf(o, sizeof o, "O%u", k); snprintf(r, sizeof r, "N%u", k);
            const c8* nets[2] = { i, o };
            fude_sim_def_inst(w, "not", r, "", nets, 2u);
        }
        rde_arr text = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
        fude_sim_def_write(w, &text);
        fude_sim_def_free(w);
        c8 err[160];
        CHECK(fude_zoom_logic_learn((const c8*)text.memory, (u32)rde_arr_length(&text), err, sizeof err) != NULL);
        rde_arr_free(&text);
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 wide = part_put(&s, "custom part", 0, 0, 60, 900, "seventy nots");
        CHECK(fude_zoom_part_of(&s, wide) != NULL && fude_zoom_part_of(&s, wide)->pin_count == 70u);
        const c8* const ins[2] = { "I0", "I34" };
        const c8* const outs[2] = { "O0", "O34" };
        u32 in[2], out[2];
        for(u32 k = 0; k < 2u; k++) {
            in[k] = part_put(&s, "logic input", -400, 300 - (f64)k * 600, 30, 20, "0");
            wire_put(&s, in[k], 0, wide, pin_named(&s, wide, ins[k]));
            out[k] = part_put(&s, "logic probe", 400, 300 - (f64)k * 600, 20, 20, "");
            wire_put(&s, wide, pin_named(&s, wide, outs[k]), out[k], 0);
        }
        CHECK(pin_named(&s, wide, "O34") == 69u);
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        fude_zoom_circuit_build(&c, &s);
        CHECK(c.logic->on && cpart(&c, wide) != NULL && cpart(&c, wide)->node[69] != FUDE_ZOOM_NONE && cpart(&c, wide)->pin_room >= 70u);
        u32 right = 0;
        for(u32 v = 0; v < 4u; v++) {
            set_in(&c, in[0], v & 1u); set_in(&c, in[1], v >> 1);
            CHECK(step(&c));
            right += probe(&c, out[0]) == 1u - (v & 1u) && probe(&c, out[1]) == 1u - (v >> 1) ? 1u : 0u;
        }
        CHECK(right == 4u);
        u32 problem = FUDE_ZOOM_MAKE_OK;
        fude_sim_def* made = fude_zoom_logic_make(&c, &s, "user/wrapseventy", "wrap seventy", &problem);
        CHECK(made != NULL && problem == FUDE_ZOOM_MAKE_OK);
        u32 joins = 0;
        for(u32 k = 0; made != NULL && k < (u32)rde_arr_length(&made->insts); k++) {
            const fude_sim_inst* in70 = &((const fude_sim_inst*)made->insts.memory)[k];
            if(strcmp(in70->def, "user/notseventy") == 0) joins = in70->count;
        }
        CHECK(joins == 70u);
        fude_sim_def_free(made);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_scene_destroy(&s);
    }
}

// DIMENSION ids (shape.h): each end's thing's id in two halves (numbers 4-5 and 7-8, its keys 6 and 9), as RADIAL's
// are, rather than one number's own bits; an older file's (eight numbers, the bits) still read and followed, and made
// again or copied the new way; a key no i32 holds read as none.
static void test_dim_ids(void) {
    printf("dimension ids\n");
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    s.camera = (fude_zoom_camera){ s.root, { 0, 0 }, 1.0 };
    const rde_color ink = { 1, 1, 1, 255 };
    const f64 box[3] = { 50, 20, 0 };
    const u32 r = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, box, 3u, ink, 1.0f, 0u, 0);
    const fude_zoom_id rid = fude_zoom_scene_object(&s, r)->id;
    // Made now: ten numbers, the ids in halves.
    const u32 d = fude_zoom_connect_dimension(&s, s.root, (fude_zoom_v2){ -50, -20 }, (fude_zoom_v2){ 50, -20 }, -15.0, r, 0, r, 1, ink, 0.5f);
    f64 n[FUDE_ZOOM_SHAPE_NUMBERS];
    CHECK(fude_zoom_scene_shape_numbers(&s, d, n, FUDE_ZOOM_SHAPE_NUMBERS) == 10u && n[0] == 100.0 && n[2] == -15.0 && n[3] == 0.0);
    CHECK(n[4] == (f64)(u32)(rid >> 32) && n[5] == (f64)(u32)rid && n[6] == 0.0 && fude_zoom_id_get(&n[7]) == rid && n[9] == 1.0);
    fude_zoom_id ids[2];
    i32 keys[2];
    CHECK(fude_zoom_connect_dimension_ends(n, 10u, ids, keys) && ids[0] == rid && ids[1] == rid && keys[0] == 0 && keys[1] == 1);
    // An older file's: eight numbers, each id its bits. Read; three numbers are on nothing; a damaged key is none.
    f64 old[8] = { 100.0, 0.0, -15.0, 0.0, 0.0, 0.0, 0.0, 1.0 };
    memcpy(&old[4], &rid, sizeof rid);
    memcpy(&old[6], &rid, sizeof rid);
    CHECK(fude_zoom_connect_dimension_ends(old, 8u, ids, keys) && ids[0] == rid && ids[1] == rid && keys[0] == 0 && keys[1] == 1);
    CHECK(!fude_zoom_connect_dimension_ends(old, 3u, ids, keys) && ids[0] == 0u && ids[1] == 0u);
    f64 bad[10];
    memcpy(bad, n, sizeof bad);
    bad[6] = 1e300; bad[9] = NAN;
    CHECK(fude_zoom_connect_dimension_ends(bad, 10u, ids, keys) && keys[0] == FUDE_ZOOM_SNAP_NO_KEY && keys[1] == FUDE_ZOOM_SNAP_NO_KEY);
    const u32 od = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { -50, -20 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_DIMENSION, old, 8u, ink, 0.5f, 0u, 0);
    // The rectangle moved 30 across: both follow, the old one too.
    fude_zoom_selection sel; fude_zoom_select_init(&sel);
    rde_arr_add(&sel.picks, &(fude_zoom_pick){ r });
    fude_zoom_select_update(&sel, &s);
    fude_zoom_select_begin(&sel, &s);
    fude_zoom_select_end(&sel, &s, (fude_zoom_sim){ 1.0, 0.0, 30.0, 0.0 });
    CHECK(fabs(fude_zoom_scene_object(&s, d)->t.x + 20.0) < 1e-9 && fabs(fude_zoom_scene_object(&s, od)->t.x + 20.0) < 1e-9);
    // The rectangle made again, 70 a side: the old one measured again, its ids now in halves.
    const u32 r2 = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 30, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, (const f64[3]){ 70, 20, 0 }, 3u, ink, 1.0f, 0u, 0);
    fude_zoom_scene_set_alive(&s, r, false);
    const fude_zoom_id r2id = fude_zoom_scene_object(&s, r2)->id;
    const u32 od2 = fude_zoom_connect_dim_remap(&s, od, &rid, &r2id, 1u);
    CHECK(od2 != FUDE_ZOOM_NONE && fude_zoom_scene_shape_numbers(&s, od2, n, FUDE_ZOOM_SHAPE_NUMBERS) == 10u);
    CHECK(fabs(n[0] - 140.0) < 1e-9 && n[2] == -15.0 && fude_zoom_id_get(&n[4]) == r2id && fude_zoom_id_get(&n[7]) == r2id && n[6] == 0.0 && n[9] == 1.0);
    // An older one copied with its rectangle: the copy on the rectangle's copy, the new way.
    memcpy(&old[4], &r2id, sizeof r2id);
    memcpy(&old[6], &r2id, sizeof r2id);
    old[0] = 140.0;
    const u32 od3 = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { -40, -20 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_DIMENSION, old, 8u, ink, 0.5f, 0u, 0);
    rde_arr clips = rde_arr_new(sizeof(fude_zoom_clip), heap);
    const u32 both[2] = { od3, r2 };
    fude_zoom_select_clips_of(&s, both, 2u, &clips);
    fude_zoom_select_clear(&sel);
    const fude_zoom_sim up = { 1, 0, 0, 200 };
    const u32 base = fude_zoom_scene_object_count(&s);
    CHECK(fude_zoom_select_put(&sel, &s, (const fude_zoom_clip*)clips.memory, 2u, &up, 1u, NULL, NULL, 0u) == 2u);
    CHECK(fude_zoom_scene_object(&s, base)->channels == FUDE_ZOOM_SHAPE_RECT && fude_zoom_scene_object(&s, base + 1u)->channels == FUDE_ZOOM_SHAPE_DIMENSION);
    const fude_zoom_id copy = fude_zoom_scene_object(&s, base)->id;
    CHECK(fude_zoom_scene_shape_numbers(&s, base + 1u, n, FUDE_ZOOM_SHAPE_NUMBERS) == 10u && fude_zoom_id_get(&n[4]) == copy && fude_zoom_id_get(&n[7]) == copy);
    CHECK(n[6] == 0.0 && n[9] == 1.0);
    fude_zoom_select_clips_free(&clips);
    rde_arr_free(&clips);
    fude_zoom_select_destroy(&sel);
    fude_zoom_scene_destroy(&s);
}

// The kind-to-part map the topics share (symbol.h: fude_zoom_symbol_topic_part), which took the place of a cache in
// each of circuit_catalogue.c, mech.c and plan.c: every kind's part in a topic is the one its id finds in that topic's
// table (the same pointer, asked again: kept), and it is no other topic's; the topic is the kind's family's. A plan's
// doors, windows and openings are openings in a wall (a room's bounds take them), as their ids said before.
static void test_kind_parts(void) {
    const u32 n = fude_zoom_symbol_count();
    u32 circuits = 0, mechs = 0, plans = 0, openings = 0;
    for(u32 k = 0; k < n + 3u; k++) {   // (and kinds past the last: none)
        const fude_zoom_symbol_info* info = fude_zoom_symbol_info_of(k);
        const fude_zoom_part*        c    = fude_zoom_part_of_kind(k);
        const fude_zoom_mech_part*   m    = fude_zoom_mech_of_kind(k);
        const fude_zoom_plan_part*   p    = fude_zoom_plan_of_kind(k);
        const u8                     t    = fude_zoom_symbol_topic(k);
        if(info == NULL) {
            CHECK(c == NULL && m == NULL && p == NULL && t == FUDE_ZOOM_SYMBOL_TOPIC_NONE);
            continue;
        }
        CHECK(c == fude_zoom_part_find(info->id) && m == fude_zoom_mech_find(info->id) && p == fude_zoom_plan_find(info->id));
        CHECK(fude_zoom_part_of_kind(k) == c && fude_zoom_mech_of_kind(k) == m && fude_zoom_plan_of_kind(k) == p);
        CHECK((c != NULL) == (t == FUDE_ZOOM_SYMBOL_TOPIC_CIRCUIT) && (m != NULL) == (t == FUDE_ZOOM_SYMBOL_TOPIC_MECH) &&
              (p != NULL) == (t == FUDE_ZOOM_SYMBOL_TOPIC_PLAN));
        CHECK(fude_zoom_symbol_topic_part(k, FUDE_ZOOM_SYMBOL_TOPIC_NONE) == NULL);
        const b8 was_opening = strncmp(info->id, "door", 4) == 0 || strncmp(info->id, "window", 6) == 0 || strncmp(info->id, "opening", 7) == 0 ||
                               strcmp(info->id, "double door") == 0 || strcmp(info->id, "sliding door") == 0;
        CHECK(fude_zoom_plan_opening(p) == was_opening);
        circuits += c != NULL ? 1u : 0u;
        mechs    += m != NULL ? 1u : 0u;
        plans    += p != NULL ? 1u : 0u;
        openings += was_opening ? 1u : 0u;
    }
    CHECK(circuits > 100u && mechs > 30u && plans > 40u && openings == 11u && !fude_zoom_plan_opening(NULL));
    CHECK(fude_zoom_symbol_topic(fude_zoom_symbol_find("resistor")) == FUDE_ZOOM_SYMBOL_TOPIC_CIRCUIT);
    CHECK(fude_zoom_symbol_topic(fude_zoom_symbol_find("process")) == FUDE_ZOOM_SYMBOL_TOPIC_NONE);
    CHECK(fude_zoom_mech_of_kind(fude_zoom_symbol_find("gear 20T")) == fude_zoom_mech_find("gear 20T"));
    CHECK(fude_zoom_plan_of_kind(fude_zoom_symbol_find("sofa"))->true_size);
}

// --- the renderer's caches (render.c and its modules)
// ------------------------------------------------------------------

// What the renderer hands the engine while rt_on is set: each triangle as drawn (its f32 corners) and its colour, how
// many rectangles (a backdrop) and the last one's colour and how many triangles came before it, how many pictures.
// These take the place of the engine's (and of tests/support/engine.c's stand-ins), and draw nothing while rt_on is not
// set.
typedef struct { f64 x[3], y[3]; rde_color c; } rt_tri;
static b8        rt_on = false;
static rde_arr   rt_tris;
static u32       rt_rects = 0, rt_pictures = 0, rt_rect_after = 0;
static rde_color rt_rect;

static void rt_begin(void) {
    if(!rde_arr_is_inited(&rt_tris)) rt_tris = rde_arr_new(sizeof(rt_tri), NULL);
    rde_arr_clear(&rt_tris);
    rt_rects = 0;
    rt_on = true;
}
// The triangles recorded since rt_begin, copied into a new array (rde_arr_free after).
static rde_arr rt_take(void) {
    rde_arr a = rde_arr_new(sizeof(rt_tri), NULL);
    if(rde_arr_length(&rt_tris) > 0) memcpy(rde_arr_add_n(&a, rde_arr_length(&rt_tris)), rt_tris.memory, rde_arr_length(&rt_tris) * sizeof(rt_tri));
    return a;
}
void rde_rendering_2d_draw_triangle(const rde_vec_2F a, const rde_vec_2F b, const rde_vec_2F c, const rde_color k, rde_shader* sh) {
    (void)sh;
    if(!rt_on) return;
    const rt_tri t = { { a.x, b.x, c.x }, { a.y, b.y, c.y }, k };
    rde_arr_add(&rt_tris, (any)&t);
}
// (a convex polygon as the engine draws it, a fan: a tile's triangles come this way)
void rde_rendering_2d_draw_polygon(const rde_vec_2F* p, u32 n, const rde_color c, rde_shader* sh) {
    for(u32 i = 1; i + 1u < n; i++) rde_rendering_2d_draw_triangle(p[0], p[i], p[i + 1u], c, sh);
}
void rde_rendering_2d_draw_rectangle(const rde_vec_2F _center, const rde_vec_2F _size, const rde_color _color) {
    (void)_center; (void)_size;
    if(rt_on) { rt_rects++; rt_rect = _color; rt_rect_after = (u32)rde_arr_length(&rt_tris); }
}
void rde_rendering_2d_draw_polygon_border(const rde_vec_2F* _points, u32 _count, f32 _thickness, const rde_color _color, rde_shader* _shader) {
    (void)_points; (void)_count; (void)_thickness; (void)_color; (void)_shader;
}
void rde_rendering_2d_draw_texture_partial_2(const rde_texture* _texture, rde_vec_3F _position, rde_vec_2F _scale, f32 _rotation_deg, rde_color _color, rde_vec_2F _bl, rde_vec_2F _tr) {
    (void)_texture; (void)_position; (void)_scale; (void)_rotation_deg; (void)_color; (void)_bl; (void)_tr;
    rt_pictures++;
}

// Pictures: a picture's bytes say its size (two u16s first); its texture is one of these, counted as made and let go.
typedef struct { u32 w, h; b8 live; } rt_texture;
static rt_texture rt_texture_pool[64];
static u32 rt_loads = 0, rt_unloads = 0;
rde_texture* rde_texture_load_from_memory(const u8* _bytes, usize _bytes_size, const c8* _dbg_name, const rde_texture_parameters* _params, rde_memory_allocator* _allocator) {
    (void)_dbg_name; (void)_params; (void)_allocator;
    if(_bytes_size < 4u || rt_loads >= 64u) return NULL;
    rt_texture* t = &rt_texture_pool[rt_loads++];
    u16 w, h;
    memcpy(&w, _bytes, 2); memcpy(&h, _bytes + 2, 2);
    *t = (rt_texture){ w, h, true };
    return (rde_texture*)(any)t;
}
rde_vec_2UI rde_texture_get_size(const rde_texture* _texture) {
    const rt_texture* t = (const rt_texture*)(const any)_texture;
    return (rde_vec_2UI){ t->w, t->h };
}
void rde_texture_unload(rde_texture* _texture) { ((rt_texture*)(any)_texture)->live = false; rt_unloads++; }

// How many triangles of t (rt_tri) in colour k cover (x, y), each strictly inside.
static u32 rt_cover(const rde_arr* t, rde_color k, f64 x, f64 y) {
    const rt_tri* v = (const rt_tri*)t->memory;
    u32 n = 0;
    for(u32 i = 0; i < (u32)rde_arr_length(t); i++) {
        if(memcmp(&v[i].c, &k, sizeof k) != 0) continue;
        f64 e[3];
        for(u32 j = 0; j < 3u; j++) {
            const u32 q = (j + 1u) % 3u;
            e[j] = (v[i].x[q] - v[i].x[j]) * (y - v[i].y[j]) - (v[i].y[q] - v[i].y[j]) * (x - v[i].x[j]);
        }
        n += (e[0] > 0 && e[1] > 0 && e[2] > 0) || (e[0] < 0 && e[1] < 0 && e[2] < 0) ? 1u : 0u;
    }
    return n;
}

// The distance from (x, y) to the nearest edge of polygon p (n points in rings: each point's ring in rings, NULL when
// all are one ring), each ring closed.
static f64 rt_edge_distance(const fude_zoom_v2* p, const u32* rings, u32 n, f64 x, f64 y) {
    f64 best = 1e300;
    for(u32 from = 0; from < n;) {
        u32 to = from + 1u;
        while(to < n && rings != NULL && rings[to] == rings[from]) to++;
        if(rings == NULL) to = n;
        for(u32 i = from; i < to; i++) {
            const fude_zoom_v2 a = p[i], b = p[i + 1u < to ? i + 1u : from];
            best = fmin(best, rde_math_point_segment_distance_2d_f64((fude_zoom_v2){ x, y }, a, b));
        }
        from = to;
    }
    return best;
}

// Over a grid of the screen's points, those off every edge of polygon p (by more than 0.01): how many the triangles
// in a and in b (colour k) cover a different number of times (*differ), and how many a covers (*inside).
static void rt_compare(const rde_arr* a, const rde_arr* b, rde_color k, const fude_zoom_v2* p, const u32* rings, u32 n, fude_zoom_v2 half, u32* differ, u32* inside) {
    *differ = 0; *inside = 0;
    for(u32 j = 0; j < 60u; j++) {
        for(u32 i = 0; i < 80u; i++) {
            const f64 x = -half.x + ((f64)i + 0.4137) * (2.0 * half.x / 80.0), y = -half.y + ((f64)j + 0.6271) * (2.0 * half.y / 60.0);
            if(rt_edge_distance(p, rings, n, x, y) < 0.01) continue;
            const u32 ca = rt_cover(a, k, x, y), cb = rt_cover(b, k, x, y);
            *differ += ca != cb || ca > 1u ? 1u : 0u;
            *inside += ca;
        }
    }
}

// Shape o's outline on the screen through to, as it was drawn before its geometry was kept: the scene's outline at the
// detail drawn, placed (out: rde_arr of fude_zoom_v2).
static void rt_outline_on_screen(const fude_zoom_scene* s, u32 o, fude_zoom_sim to, rde_arr* out) {
    const fude_zoom_object* ob = fude_zoom_scene_object(s, o);
    const f64 size = fmax(ob->box.max_x - ob->box.min_x, ob->box.max_y - ob->box.min_y) * fude_zoom_sim_scale(to);
    b8 closed;
    fude_zoom_scene_shape_outline(s, o, fude_zoom_shape_segments(size), out, &closed);
    fude_zoom_v2* p = (fude_zoom_v2*)out->memory;
    for(u32 i = 0; i < (u32)rde_arr_length(out); i++) p[i] = fude_zoom_sim_apply(to, p[i]);
}

// Shape o drawn by the renderer, its inside in colour k, against the old way's: its outline on the screen cut to the
// box round it and triangulated there (fude_zoom_render_polygon_fill). Returns how many sample points differ; *inside,
// how many are inside.
static u32 rt_shape_against_old(fude_zoom_renderer* r, const fude_zoom_scene* s, u32 o, fude_zoom_sim to, fude_zoom_v2 half, rde_color k, u32* inside) {
    rt_begin();
    fude_zoom_render_shape(r, s, o, to, half, NULL, 0.0f);
    rde_arr kept = rt_take();
    rde_arr p = rde_arr_new(sizeof(fude_zoom_v2), NULL);
    rt_outline_on_screen(s, o, to, &p);
    rt_begin();
    fude_zoom_render_polygon_fill(r, (const fude_zoom_v2*)p.memory, (u32)rde_arr_length(&p), (fude_zoom_v2){ half.x * 2.0, half.y * 2.0 }, k);
    rt_on = false;
    u32 differ = 0;
    rt_compare(&kept, &rt_tris, k, (const fude_zoom_v2*)p.memory, NULL, (u32)rde_arr_length(&p), half, &differ, inside);
    rde_arr_free(&kept);
    rde_arr_free(&p);
    return differ;
}

// A star of n points round (0, 0), its points out to big and in to small (polygon numbers: x y each).
static void rt_star(f64* n, u32 points, f64 big, f64 small, f64 turn) {
    for(u32 i = 0; i < 2u * points; i++) {
        const f64 a = turn + 3.14159265358979 * (f64)i / (f64)points, l = i % 2u ? small : big;
        n[2u * i] = l * cos(a); n[2u * i + 1u] = l * sin(a);
    }
}

// Shapes' and cut boards' insides are triangulated once, in their own units, and kept (render.c's kept geometry),
// then drawn through where they are: they cover every sampled point of the screen (off their edges) exactly as the
// polygon on the screen, cut and triangulated there each frame as before, covers it — a concave polygon, an ellipse
// filled in its own colour, a hatched rectangle seen too small for its lines (its tint) and a cut board with a hole,
// each turned and scaled. Drawn again, nothing is made again; a shape whose payload's slot is given to another (an
// edit) is drawn as the new one; one far bigger than the screen is still cut to it. Past the bound (2048 kept), what
// one draw uses is never let go: a shape drawn every draw keeps its geometry while 3,000 others come and go, and those
// past the bound in a single draw are made each time, right still. Timed: a frame of 500 filled shapes, made each frame
// as before against kept.
static void test_render_kept_shapes(void) {
    printf("shapes' geometry kept by the renderer\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_renderer r; fude_zoom_render_init(&r);
    const fude_zoom_v2  half = { 400, 300 };
    const fude_zoom_sim to   = { 1.7 * cos(0.5), 1.7 * sin(0.5), 13.25, -7.5 };   // (turned and scaled, off centre)
    const rde_color ink = { 200, 30, 40, 255 }, own = { 20, 160, 60, 255 }, hatch_ink = { 40, 40, 220, 255 }, wood = { 150, 100, 50, 255 };
    f64 star[20];
    rt_star(star, 5u, 110.0, 40.0, 0.1);
    const u32 poly = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { -60, 20 }, 0.3, 1.0 }, FUDE_ZOOM_SHAPE_POLYGON, star, 20u, ink, 1.0f, FUDE_ZOOM_FLAG_FILLED, 0);
    const f64 oval[2] = { 90, 50 };
    const u32 ell = fude_zoom_scene_add_shape_fill(&s, s.root, (fude_zoom_place){ { 80, -40 }, -0.2, 1.3 }, FUDE_ZOOM_SHAPE_ELLIPSE, oval, 2u, ink, 1.0f,
                                                   FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN, own, 0);
    s.style = fude_zoom_q_of(0, FUDE_ZOOM_HATCH_DIAGONAL);
    const f64 rect[3] = { 70, 40, 0 };
    const u32 hat = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { -20, -90 }, 0.7, 1.0 }, FUDE_ZOOM_SHAPE_RECT, rect, 3u, hatch_ink, 0.05f, 0u, 0);
    s.style = 0;
    rde_color tint = hatch_ink;
    tint.a = (u8)(hatch_ink.a / 4u);
    u32 inside = 0;
    CHECK(rt_shape_against_old(&r, &s, poly, to, half, ink, &inside) == 0u && inside > 100u);
    CHECK(rt_shape_against_old(&r, &s, ell, to, half, own, &inside) == 0u && inside > 100u);
    CHECK(rt_shape_against_old(&r, &s, hat, to, half, tint, &inside) == 0u && inside > 100u);
    // A cut board with a hole: its rings, triangulated on the screen as before, against its kept triangles.
    {
        const fude_zoom_v2 ring_p[8] = { { -100, -60 }, { 100, -60 }, { 100, 60 }, { -100, 60 }, { -20, -20 }, { 20, -20 }, { 20, 20 }, { -20, 20 } };
        const u32 ring_of[8] = { 0, 0, 0, 0, 1, 1, 1, 1 };
        rde_arr nums = rde_arr_new(sizeof(f64), NULL);
        const u32 count = fude_zoom_board_cut_numbers(&nums, 100, 60, 18, 0, "", ring_p, ring_of, 8u);
        const u32 board = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 150, 120 }, 1.1, 0.8 }, FUDE_ZOOM_SHAPE_BOARD, (const f64*)nums.memory, count, wood, 1.0f,
                                                    FUDE_ZOOM_FLAG_FILLED, 0);
        rt_begin();
        fude_zoom_render_shape(&r, &s, board, to, half, NULL, 0.0f);
        rde_arr kept = rt_take();
        rde_arr p = rde_arr_new(sizeof(fude_zoom_v2), NULL), rings = rde_arr_new(sizeof(u32), NULL), old = rde_arr_new(sizeof(fude_zoom_v2), NULL);
        const u32 n = fude_zoom_board_rings((const f64*)nums.memory, count, &p, &rings);
        const fude_zoom_sim all = fude_zoom_sim_compose(to, fude_zoom_object_sim(fude_zoom_scene_object(&s, board)));
        for(u32 i = 0; i < n; i++) ((fude_zoom_v2*)p.memory)[i] = fude_zoom_sim_apply(all, ((fude_zoom_v2*)p.memory)[i]);
        rt_begin();
        const u32 t = fude_zoom_fill_triangulate_rings((const fude_zoom_v2*)p.memory, (const u32*)rings.memory, n, &old);
        fude_zoom_render_triangles((const fude_zoom_v2*)old.memory, t, wood);
        rt_on = false;
        u32 differ = 0;
        rt_compare(&kept, &rt_tris, wood, (const fude_zoom_v2*)p.memory, (const u32*)rings.memory, n, half, &differ, &inside);
        CHECK(n == 8u && differ == 0u && inside > 100u);
        // (the hole left out: its middle is not covered)
        const fude_zoom_v2 mid = fude_zoom_sim_apply(all, (fude_zoom_v2){ 0.31, 0.17 });
        CHECK(rt_cover(&kept, wood, mid.x, mid.y) == 0u);
        rde_arr_free(&kept); rde_arr_free(&p); rde_arr_free(&rings); rde_arr_free(&old); rde_arr_free(&nums);
    }
    // Drawn again: nothing made again (the same geometry, its triangles where they were).
    const fude_zoom_object* po = fude_zoom_scene_object(&s, poly);
    const u32 seg = fude_zoom_shape_segments(fmax(po->box.max_x - po->box.min_x, po->box.max_y - po->box.min_y) * fude_zoom_sim_scale(to));
    fude_zoom_geo* g = fude_zoom_render_shape_geo(&r, &s, poly, seg, FUDE_ZOOM_SHAPE_NUMBERS);
    const any tris_were = g->tris.memory;
    const u32 kept_were = rde_lru_length(&r.geo_lru);
    r.draws++;
    CHECK(rt_shape_against_old(&r, &s, poly, to, half, ink, &inside) == 0u);
    CHECK(fude_zoom_render_shape_geo(&r, &s, poly, seg, FUDE_ZOOM_SHAPE_NUMBERS) == g && g->triangulated && g->tris.memory == tris_were &&
          rde_lru_length(&r.geo_lru) == kept_were);
    // An edit: the star goes, a triangle takes its payload's slot (a new serial): drawn as the triangle. (Both small on
    // a small screen, so both are made at the same detail: only the serial tells them apart.)
    {
        const fude_zoom_v2 near_half = { 40, 30 };
        const f64 sa = 0.15 * cos(0.5), sb = 0.15 * sin(0.5);
        const fude_zoom_sim small = { sa, sb, -(sa * -60.0 - sb * 20.0), -(sb * -60.0 + sa * 20.0) };   // (its place, (-60, 20), in the middle)
        CHECK(rt_shape_against_old(&r, &s, poly, small, near_half, ink, &inside) == 0u && inside > 100u);
        const u32 blob = po->blob, serial = fude_zoom_blob_serial(&s, blob);
        fude_zoom_scene_discard(&s, poly);
        const f64 tri[6] = { -80, -60, 90, -50, 0, 100 };
        const u32 poly2 = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { -60, 20 }, 0.3, 1.0 }, FUDE_ZOOM_SHAPE_POLYGON, tri, 6u, ink, 1.0f, FUDE_ZOOM_FLAG_FILLED, 0);
        CHECK(fude_zoom_scene_object(&s, poly2)->blob == blob && fude_zoom_blob_serial(&s, blob) != serial);
        CHECK(rt_shape_against_old(&r, &s, poly2, small, near_half, ink, &inside) == 0u && inside > 100u);
        CHECK(rt_shape_against_old(&r, &s, poly2, to, half, ink, &inside) == 0u && inside > 100u);
    }
    // Far bigger than the screen: cut to it first, every corner near it.
    {
        // (the ellipse's middle, (80, -40), a little off the screen's: the screen deep inside it)
        const f64 fa = 4000.0 * cos(0.2), fb = 4000.0 * sin(0.2);
        const fude_zoom_sim far = { fa, fb, -(fa * 80.0 + fb * 40.0) + 30.0, -(fb * 80.0 - fa * 40.0) - 20.0 };
        CHECK(rt_shape_against_old(&r, &s, ell, far, half, own, &inside) == 0u && inside > 1000u);
        rt_begin();
        fude_zoom_render_shape(&r, &s, ell, far, half, NULL, 0.0f);
        rt_on = false;
        b8 near = rde_arr_length(&rt_tris) > 0;
        const rt_tri* v = (const rt_tri*)rt_tris.memory;
        for(u32 i = 0; i < (u32)rde_arr_length(&rt_tris); i++) {
            for(u32 j = 0; j < 3u; j++) near = near && fabs(v[i].x[j]) <= half.x * 2.0 + 1e-3 && fabs(v[i].y[j]) <= half.y * 2.0 + 1e-3;
        }
        CHECK(near);
    }

    // Past the bound: 3,000 polygons, 300 new ones a draw, and one drawn every draw.
    rde_arr others = rde_arr_new(sizeof(u32), NULL);
    for(u32 i = 0; i < 3000u; i++) {
        f64 n[12];
        rt_star(n, 3u, 30.0 + (f64)(i % 50u), 12.0 + (f64)(i / 50u) * 0.25, 0.01 * (f64)i);
        const u32 o = fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { (f64)(i % 60u) * 9.0 - 270.0, (f64)(i / 60u) * 9.0 - 220.0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_POLYGON,
                                                n, 12u, ink, 1.0f, FUDE_ZOOM_FLAG_FILLED, 0);
        rde_arr_add(&others, (any)&o);
    }
    const fude_zoom_object* eo = fude_zoom_scene_object(&s, ell);
    const u32 eseg = fude_zoom_shape_segments(fmax(eo->box.max_x - eo->box.min_x, eo->box.max_y - eo->box.min_y) * fude_zoom_sim_scale(to));
    const fude_zoom_geo* hot = fude_zoom_render_shape_geo(&r, &s, ell, eseg, FUDE_ZOOM_SHAPE_NUMBERS);
    const any hot_tris = hot->tris.memory;
    b8 hot_kept = hot->triangulated;
    for(u32 d = 0; d < 10u; d++) {
        r.draws++;
        fude_zoom_render_shape(&r, &s, ell, to, half, NULL, 0.0f);
        for(u32 i = 0; i < 300u; i++) fude_zoom_render_shape(&r, &s, ((const u32*)others.memory)[d * 300u + i], to, half, NULL, 0.0f);
        hot_kept = hot_kept && fude_zoom_render_shape_geo(&r, &s, ell, eseg, FUDE_ZOOM_SHAPE_NUMBERS) == hot && hot->tris.memory == hot_tris;
    }
    CHECK(hot_kept && rde_lru_length(&r.geo_lru) <= FUDE_ZOOM_RENDER_GEOS && r.geo_points <= FUDE_ZOOM_RENDER_GEO_POINTS);
    // ...and 2,100 in one draw: 2,048 kept, those past them made into the spare, each outline what its numbers make.
    r.draws++;
    u32 spare = 0, wrong = 0;
    rde_arr expect = rde_arr_new(sizeof(fude_zoom_v2), NULL);
    for(u32 i = 0; i < 2100u; i++) {
        const u32 o = ((const u32*)others.memory)[i];
        const fude_zoom_geo* got = fude_zoom_render_shape_geo(&r, &s, o, 24u, FUDE_ZOOM_SHAPE_NUMBERS);
        spare += got == &((const fude_zoom_geo*)r.geos.memory)[FUDE_ZOOM_RENDER_GEOS] ? 1u : 0u;
        f64 n[FUDE_ZOOM_SHAPE_NUMBERS];
        b8 closed;
        fude_zoom_shape_outline(FUDE_ZOOM_SHAPE_POLYGON, n, fude_zoom_scene_shape_numbers(&s, o, n, FUDE_ZOOM_SHAPE_NUMBERS), 24u, &expect, &closed);
        wrong += rde_arr_length(&expect) != rde_arr_length(&got->points) || got->closed != closed ||
                 memcmp(expect.memory, got->points.memory, rde_arr_length(&expect) * sizeof(fude_zoom_v2)) != 0 ? 1u : 0u;
    }
    printf("  2100 in one draw: %u made each time past the 2048 kept, %u wrong\n", spare, wrong);
    CHECK(spare == 2100u - FUDE_ZOOM_RENDER_GEOS && wrong == 0u && rde_lru_length(&r.geo_lru) == FUDE_ZOOM_RENDER_GEOS);
    rde_arr_free(&expect);

    // Timed: 500 of them filled, a frame made as before (outline, placed, cut and triangulated) against kept.
    {
        rde_arr p = rde_arr_new(sizeof(fude_zoom_v2), NULL);
        r.draws++;
        for(u32 i = 0; i < 500u; i++) fude_zoom_render_shape(&r, &s, ((const u32*)others.memory)[2500u + i], to, half, NULL, 0.0f);
        clock_t c0 = clock();
        for(u32 f = 0; f < 20u; f++) {
            for(u32 i = 0; i < 500u; i++) {
                const u32 o = ((const u32*)others.memory)[2500u + i];
                rt_outline_on_screen(&s, o, to, &p);
                fude_zoom_render_polygon_fill(&r, (const fude_zoom_v2*)p.memory, (u32)rde_arr_length(&p), (fude_zoom_v2){ half.x * 2.0, half.y * 2.0 }, ink);
            }
        }
        const f64 old_ms = (f64)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC / 20.0;
        c0 = clock();
        for(u32 f = 0; f < 20u; f++) {
            r.draws++;
            for(u32 i = 0; i < 500u; i++) fude_zoom_render_shape(&r, &s, ((const u32*)others.memory)[2500u + i], to, half, NULL, 0.0f);
        }
        const f64 new_ms = (f64)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC / 20.0;
        printf("  500 filled shapes: %.2f ms a frame made each frame (as before), %.2f ms kept (their lines too)\n", old_ms, new_ms);
        rde_arr_free(&p);
    }
    rde_arr_free(&others);
    fude_zoom_render_destroy(&r);
    fude_zoom_scene_destroy(&s);
}

// The Library's tiles draw their symbols from the renderer's kept geometry (fude_zoom_render_symbol_geo) instead of
// making each one every frame: for every symbol at a tile's size, its points and parts are what symbol.h makes, and
// each filled part's triangles, moved to the tile's middle, cover every sampled point the old way's cover (its points
// moved first, then triangulated with its holes); asked again, it is the same geometry. Timed: every tile, made as
// before against kept.
static void test_render_tiles(void) {
    printf("the Library's tiles from kept symbols\n");
    fude_zoom_renderer r; fude_zoom_render_init(&r);
    const fude_zoom_v2 c = { 523.25, 311.5 };
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), NULL), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), NULL);
    rde_arr rings = rde_arr_new(sizeof(u32), NULL), ring_pts = rde_arr_new(sizeof(fude_zoom_v2), NULL), old = rde_arr_new(sizeof(fude_zoom_v2), NULL);
    rde_arr ta = rde_arr_new(sizeof(rt_tri), NULL), tb = rde_arr_new(sizeof(rt_tri), NULL);
    const rde_color k = { 1, 2, 3, 255 };
    u32 symbols = 0, same = 0, sampled = 0, differ = 0, again = 0;
    for(u32 kind = 0; kind < fude_zoom_symbol_count(); kind++) {
        const fude_zoom_symbol_info* info = fude_zoom_symbol_info_of(kind);
        if(info == NULL || !(info->w > 0.0f) || !(info->h > 0.0f)) continue;
        // (a tile's symbol box is 68 by 52: page_diagrams.c)
        const f64 kk = fmin(68.0 / (f64)info->w, 52.0 / (f64)info->h);
        const f64 hw = fmax((f64)info->w * 0.5 * kk, 1.0), hh = fmax((f64)info->h * 0.5 * kk, 1.0);
        const fude_zoom_geo* g = fude_zoom_render_symbol_geo(&r, kind, hw, hh, 48u);
        const u32 np = fude_zoom_symbol_parts(kind, hw, hh, 48u, &pts, &parts);
        symbols++;
        b8 ok = g->parts_n == np && rde_arr_length(&g->points) == rde_arr_length(&pts) && rde_arr_length(&g->parts) == rde_arr_length(&parts) &&
                (rde_arr_length(&pts) == 0 || memcmp(g->points.memory, pts.memory, rde_arr_length(&pts) * sizeof(fude_zoom_v2)) == 0);
        const fude_zoom_symbol_part* pa = (const fude_zoom_symbol_part*)parts.memory;
        const fude_zoom_symbol_part* ga = (const fude_zoom_symbol_part*)g->parts.memory;
        for(u32 i = 0; ok && i < (u32)rde_arr_length(&parts); i++) ok = pa[i].first == ga[i].first && pa[i].count == ga[i].count && pa[i].flags == ga[i].flags;
        same += ok ? 1u : 0u;
        if(!ok) continue;
        fude_zoom_v2* p = (fude_zoom_v2*)pts.memory;
        for(u32 i = 0; i < (u32)rde_arr_length(&pts); i++) p[i] = (fude_zoom_v2){ p[i].x + c.x, p[i].y + c.y };
        const u32* range = (const u32*)g->tri_range.memory;
        const fude_zoom_v2* gt = (const fude_zoom_v2*)g->tris.memory;
        for(u32 i = 0; i < np; i++) {
            const u8 fl = pa[i].flags;
            if(!((fl & (FUDE_ZOOM_SYMBOL_FILLED | FUDE_ZOOM_SYMBOL_SOLID | FUDE_ZOOM_SYMBOL_TINTED)) && pa[i].count >= 3u && !(fl & FUDE_ZOOM_SYMBOL_JOIN))) continue;
            // The old way: its rings moved to the tile, then triangulated.
            rde_arr_clear(&rings); rde_arr_clear(&ring_pts); rde_arr_clear(&old);
            u32 ring = 0;
            for(u32 j = i; j < np && (j == i || (pa[j].flags & FUDE_ZOOM_SYMBOL_JOIN)); j++, ring++) {
                for(u32 t = 0; t < pa[j].count; t++) { rde_arr_add(&rings, (any)&ring); rde_arr_add(&ring_pts, (any)&p[pa[j].first + t]); }
            }
            const u32 nt = fude_zoom_fill_triangulate_rings((const fude_zoom_v2*)ring_pts.memory, (const u32*)rings.memory, (u32)rde_arr_length(&ring_pts), &old);
            rde_arr_clear(&ta); rde_arr_clear(&tb);
            for(u32 t = 0; t < nt; t++) {
                const fude_zoom_v2* o3 = &((const fude_zoom_v2*)old.memory)[3u * t];
                const rt_tri a = { { (f32)o3[0].x, (f32)o3[1].x, (f32)o3[2].x }, { (f32)o3[0].y, (f32)o3[1].y, (f32)o3[2].y }, k };
                rde_arr_add(&ta, (any)&a);
            }
            for(u32 t = 0; t < range[2u * i + 1u]; t++) {
                const fude_zoom_v2* g3 = &gt[3u * (range[2u * i] + t)];
                const rt_tri b = { { (f32)(g3[0].x + c.x), (f32)(g3[1].x + c.x), (f32)(g3[2].x + c.x) }, { (f32)(g3[0].y + c.y), (f32)(g3[1].y + c.y), (f32)(g3[2].y + c.y) }, k };
                rde_arr_add(&tb, (any)&b);
            }
            // (its box sampled, off its rings' edges)
            fude_zoom_box box = fude_zoom_box_empty();
            const fude_zoom_v2* rp = (const fude_zoom_v2*)ring_pts.memory;
            for(u32 t = 0; t < (u32)rde_arr_length(&ring_pts); t++) box = fude_zoom_box_union(box, (fude_zoom_box){ rp[t].x, rp[t].y, rp[t].x, rp[t].y });
            for(u32 sy = 0; sy < 12u; sy++) {
                for(u32 sx = 0; sx < 12u; sx++) {
                    const f64 x = box.min_x + ((f64)sx + 0.4137) * (box.max_x - box.min_x) / 12.0, y = box.min_y + ((f64)sy + 0.6271) * (box.max_y - box.min_y) / 12.0;
                    if(rt_edge_distance(rp, (const u32*)rings.memory, (u32)rde_arr_length(&ring_pts), x, y) < 0.01) continue;
                    sampled++;
                    differ += rt_cover(&ta, k, x, y) != rt_cover(&tb, k, x, y) ? 1u : 0u;
                }
            }
        }
        again += fude_zoom_render_symbol_geo(&r, kind, hw, hh, 48u) == g ? 1u : 0u;
    }
    printf("  %u symbols: %u as symbol.h makes them, %u points sampled in their fills, %u covered differently\n", symbols, same, sampled, differ);
    CHECK(symbols > 200u && same == symbols && again == symbols && sampled > 10000u && differ == 0u);
    // Timed: every tile's symbol a frame, made as the tiles did (its parts, then its fills triangulated) against kept.
    {
        clock_t c0 = clock();
        for(u32 kind = 0; kind < fude_zoom_symbol_count(); kind++) {
            const fude_zoom_symbol_info* info = fude_zoom_symbol_info_of(kind);
            if(info == NULL || !(info->w > 0.0f) || !(info->h > 0.0f)) continue;
            const f64 kk = fmin(68.0 / (f64)info->w, 52.0 / (f64)info->h);
            const u32 np = fude_zoom_symbol_parts(kind, fmax((f64)info->w * 0.5 * kk, 1.0), fmax((f64)info->h * 0.5 * kk, 1.0), 48u, &pts, &parts);
            const fude_zoom_symbol_part* pa = (const fude_zoom_symbol_part*)parts.memory;
            for(u32 i = 0; i < np; i++) {
                if(!((pa[i].flags & (FUDE_ZOOM_SYMBOL_FILLED | FUDE_ZOOM_SYMBOL_SOLID | FUDE_ZOOM_SYMBOL_TINTED)) && pa[i].count >= 3u && !(pa[i].flags & FUDE_ZOOM_SYMBOL_JOIN))) continue;
                rde_arr_clear(&old);
                fude_zoom_fill_triangulate((const fude_zoom_v2*)pts.memory + pa[i].first, pa[i].count, &old);
            }
        }
        const f64 old_ms = (f64)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
        c0 = clock();
        for(u32 kind = 0; kind < fude_zoom_symbol_count(); kind++) {
            const fude_zoom_symbol_info* info = fude_zoom_symbol_info_of(kind);
            if(info == NULL || !(info->w > 0.0f) || !(info->h > 0.0f)) continue;
            const f64 kk = fmin(68.0 / (f64)info->w, 52.0 / (f64)info->h);
            fude_zoom_render_symbol_geo(&r, kind, fmax((f64)info->w * 0.5 * kk, 1.0), fmax((f64)info->h * 0.5 * kk, 1.0), 48u);
        }
        const f64 new_ms = (f64)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
        printf("  every tile's symbol: %.2f ms made (as each frame did), %.3f ms found kept\n", old_ms, new_ms);
    }
    rde_arr_free(&pts); rde_arr_free(&parts); rde_arr_free(&rings); rde_arr_free(&ring_pts); rde_arr_free(&old); rde_arr_free(&ta); rde_arr_free(&tb);
    fude_zoom_render_destroy(&r);
}

// Whether two layouts are alike in all they draw: their runs, marks and size.
static b8 rt_same_layout(const fude_zoom_rich* a, const fude_zoom_rich* b) {
    if(a == NULL || b == NULL || rde_arr_length(&a->runs) != rde_arr_length(&b->runs) || rde_arr_length(&a->marks) != rde_arr_length(&b->marks) || a->w != b->w || a->h != b->h) return false;
    const fude_zoom_rich_run* ra = (const fude_zoom_rich_run*)a->runs.memory, *rb = (const fude_zoom_rich_run*)b->runs.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&a->runs); i++) {
        if(ra[i].x != rb[i].x || ra[i].y != rb[i].y || ra[i].w != rb[i].w || ra[i].px != rb[i].px || ra[i].from != rb[i].from || ra[i].len != rb[i].len || ra[i].style != rb[i].style) return false;
    }
    const fude_zoom_rich_mark* ma = (const fude_zoom_rich_mark*)a->marks.memory, *mb = (const fude_zoom_rich_mark*)b->marks.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&a->marks); i++) {
        if(ma[i].x0 != mb[i].x0 || ma[i].y0 != mb[i].y0 || ma[i].x1 != mb[i].x1 || ma[i].y1 != mb[i].y1 || ma[i].kind != mb[i].kind || ma[i].soft != mb[i].soft) return false;
    }
    return true;
}

// Notes' layouts (render_text.c) are kept by all they are made from, in an rde_lru of 256: 300 notes drawn over 50
// draws are each laid out as a fresh layout is; the 256 kept stay kept, not laid out again (a mark put in one survives
// the next draw), while the 44 past the bound are laid out each draw (all 300 were: every layout was let go past the
// bound); a word changed, another size or another font is a layout of its own. Timed against laying all out each draw.
static void test_render_kept_notes(void) {
    printf("notes' layouts kept by the renderer\n");
    fude_zoom_renderer r; fude_zoom_render_init(&r);
    r.font = (rde_font*)(any)&r;   // (any font: the suite's text widths are by the letter)
    r.font_px = 14.0f;
    static c8 texts[300][96];
    for(u32 i = 0; i < 300u; i++) snprintf(texts[i], sizeof texts[i], "# Note %u\n- [ ] a task **%u** to do\n- an item *%u* and more words", i, i * 7u, i * 13u);
    fude_zoom_rich fresh; fude_zoom_rich_init(&fresh);
    const fude_zoom_rich* first[300];
    u32 wrong = 0, moved = 0;
    for(u32 d = 0; d < 50u; d++) {
        r.draws++;
        if(d == 49u) { ((fude_zoom_rich*)first[3])->w = -7.0f; ((fude_zoom_rich*)first[250])->w = -7.0f; }   // (marks: laid out again, they would go)
        for(u32 i = 0; i < 300u; i++) {
            const u32 len = (u32)strlen(texts[i]);
            const fude_zoom_rich* l = fude_zoom_render_rich(&r, texts[i], len, 12.0, 220.0);
            if(d == 0u) first[i] = l;
            moved += i < 256u && l != first[i] ? 1u : 0u;
            if((d == 0u || d == 49u) && i != 3u && i != 250u) {
                fude_zoom_rich_lay_aligned(&fresh, texts[i], len, 220.0f, 12.0f, 1.3f, 0u, fude_zoom_render_measure, &r);
                wrong += rt_same_layout(l, &fresh) ? 0u : 1u;
            }
        }
    }
    CHECK(wrong == 0u && moved == 0u && first[3]->w == -7.0f && first[250]->w == -7.0f);
    // Another word, another size, another font: each its own layout.
    {
        r.draws++;
        c8 changed[96];
        snprintf(changed, sizeof changed, "%s", texts[5]);
        changed[3] = 'X';
        const fude_zoom_rich* l = fude_zoom_render_rich(&r, changed, (u32)strlen(changed), 12.0, 220.0);
        fude_zoom_rich_lay_aligned(&fresh, changed, (u32)strlen(changed), 220.0f, 12.0f, 1.3f, 0u, fude_zoom_render_measure, &r);
        CHECK(l != first[5] && rt_same_layout(l, &fresh));
        l = fude_zoom_render_rich(&r, texts[6], (u32)strlen(texts[6]), 13.0, 220.0);
        fude_zoom_rich_lay_aligned(&fresh, texts[6], (u32)strlen(texts[6]), 220.0f, 13.0f, 1.3f, 0u, fude_zoom_render_measure, &r);
        CHECK(l != first[6] && rt_same_layout(l, &fresh));
        r.font_bold = (rde_font*)(any)&fresh;
        CHECK(fude_zoom_render_rich(&r, texts[7], (u32)strlen(texts[7]), 12.0, 220.0) != first[7]);
        r.font_bold = NULL;
    }
    // Timed: 300 notes, 50 draws, all laid out each draw (as past the bound before) against kept.
    {
        clock_t c0 = clock();
        for(u32 d = 0; d < 50u; d++) {
            for(u32 i = 0; i < 300u; i++) fude_zoom_rich_lay_aligned(&fresh, texts[i], (u32)strlen(texts[i]), 220.0f, 12.0f, 1.3f, 0u, fude_zoom_render_measure, &r);
        }
        const f64 old_ms = (f64)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
        c0 = clock();
        for(u32 d = 0; d < 50u; d++) {
            r.draws++;
            for(u32 i = 0; i < 300u; i++) fude_zoom_render_rich(&r, texts[i], (u32)strlen(texts[i]), 12.0, 220.0);
        }
        const f64 new_ms = (f64)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
        printf("  300 notes over 50 draws: %.1f ms laid out each draw, %.1f ms kept (44 a draw past the bound)\n", old_ms, new_ms);
    }
    fude_zoom_rich_free(&fresh);
    fude_zoom_render_destroy(&r);
}

// Plain texts' lines (render_text.c): draw.c lays them out at the text's size and width on the screen. Here the
// layout is the suite's own while rt_wrap is set (a line every so many letters for the width: zoomdraw.c's stand-ins,
// one whole line, otherwise), each layout counted, and each line drawn summed with its place and size.
static b8  rt_wrap = false;
static u32 rt_layouts = 0, rt_lines = 0;
static u64 rt_lines_sum = 0;
static u32 rt_wrap_letters(f32 px, f32 width) { const u32 n = (u32)(width / (px * 0.5f)); return n > 0u ? n : 1u; }
u32 fude_draw_text_wrap_lines(rde_font* f, f32 fpx, const c8* t, f32 px, f32 width) {
    (void)f; (void)fpx;
    if(!rt_wrap) return 1u;
    rt_layouts++;
    const u32 len = (u32)strlen(t), per = rt_wrap_letters(px, width);
    return (len + per - 1u) / per;
}
u32 fude_draw_text_wrap_spans(rde_font* f, f32 fpx, const c8* t, f32 px, f32 width, u32* from, u32* to, u32 max) {
    (void)f; (void)fpx;
    if(!rt_wrap) {
        if(max == 0u) return 0u;
        from[0] = 0u; to[0] = (u32)strlen(t);
        return 1u;
    }
    const u32 len = (u32)strlen(t), per = rt_wrap_letters(px, width);
    u32 n = 0;
    for(u32 at = 0; at < len && n < max; at += per, n++) { from[n] = at; to[n] = at + per < len ? at + per : len; }
    return n;
}
void fude_draw_text(rde_font* f, f32 fpx, const c8* t, f32 x, f32 y, f32 px, rde_color c) {
    (void)f; (void)fpx; (void)c;
    if(!rt_wrap) return;
    rt_lines++;
    u32 bits[3];
    memcpy(&bits[0], &x, 4); memcpy(&bits[1], &y, 4); memcpy(&bits[2], &px, 4);
    rt_lines_sum += rde_hash_64(t, strlen(t)) ^ ((u64)bits[0] * 0x9E3779B97F4A7C15ull) ^ ((u64)bits[1] * 0xC2B2AE3D27D4EB4Full) ^ ((u64)bits[2] << 7);
}

// Plain texts on the canvas keep their lines (render_text.c's fude_zoom_render_plain_lines, in their slot of the texts'
// layouts): a still view lays nothing out again, and what is drawn is what laying out each draw drew (the first draw's,
// and a renderer with nothing kept). Another size on the screen (a zoom), another width or other words: laid out
// again, right. Past the bound (256), what one draw uses is kept: 300 texts over 50 still draws lay out 300 the first
// and 44 each after; past draw.c's 64 layouts, all 300 were laid out each draw. Over a zoom, each is laid out each
// frame, as before: its lines are draw.c's at its size on the screen, kept so for the same pixels.
static void test_render_plain_texts(void) {
    printf("plain texts' lines kept by the renderer\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_renderer r; fude_zoom_render_init(&r);
    r.font = (rde_font*)(any)&r;
    r.font_px = 14.0f;
    static c8 words[300][64];
    u32 texts[300];
    for(u32 i = 0; i < 300u; i++) {
        snprintf(words[i], sizeof words[i], "plain text %u with some words to wrap, %u", i, i * 7u);
        texts[i] = fude_zoom_scene_add_text(&s, s.root, (fude_zoom_place){ { (f64)(i % 20u) * 40.0, (f64)(i / 20u) * 30.0 }, 0.0, 1.0 }, 6.0, 36.0, 24.0,
                                            FUDE_ZOOM_TEXT_PLAIN, (rde_color){ 1, 2, 3, 255 }, (rde_color){ 0, 0, 0, 0 }, words[i], (u32)strlen(words[i]), 0);
    }
    const fude_zoom_sim view = { 1.5, 0.0, -400.0, 200.0 };
    rt_wrap = true;
    u32 made[50];
    u64 sums[50];
    for(u32 d = 0; d < 50u; d++) {
        r.draws++;
        rt_layouts = rt_lines = 0; rt_lines_sum = 0;
        for(u32 i = 0; i < 300u; i++) fude_zoom_render_text(&r, &s, texts[i], view, NULL);
        made[d] = rt_layouts; sums[d] = rt_lines_sum;
    }
    b8 still = true;
    for(u32 d = 1; d < 50u; d++) still = still && made[d] == 44u && sums[d] == sums[0];
    CHECK(made[0] == 300u && still && rt_lines > 600u);
    // Nothing kept (a renderer of its own): what laying each out draws.
    fude_zoom_renderer fresh; fude_zoom_render_init(&fresh);
    fresh.font = r.font; fresh.font_px = r.font_px;
    fresh.draws++;
    rt_layouts = 0; rt_lines_sum = 0;
    for(u32 i = 0; i < 300u; i++) fude_zoom_render_text(&fresh, &s, texts[i], view, NULL);
    CHECK(rt_layouts == 300u && rt_lines_sum == sums[0]);
    // Another size on the screen: each laid out again, and drawn as a renderer laying it out afresh draws it.
    const fude_zoom_sim nearer = { 1.6, 0.0, -400.0, 200.0 };
    r.draws++; fresh.draws++;
    rt_layouts = 0; rt_lines_sum = 0;
    for(u32 i = 0; i < 10u; i++) fude_zoom_render_text(&r, &s, texts[i], nearer, NULL);
    const u64 kept_sum = rt_lines_sum;
    CHECK(rt_layouts == 10u);
    fude_zoom_render_trim(&fresh);
    rt_lines_sum = 0;
    for(u32 i = 0; i < 10u; i++) fude_zoom_render_text(&fresh, &s, texts[i], nearer, NULL);
    CHECK(rt_lines_sum == kept_sum && kept_sum != 0u);
    // Other words (an edit is a new text: the slot's key), the same size: their own lines.
    const c8* other = "plain text 0 changed";
    const u32 edited = fude_zoom_scene_add_text(&s, s.root, (fude_zoom_place){ { 0.0, 0.0 }, 0.0, 1.0 }, 6.0, 36.0, 24.0, FUDE_ZOOM_TEXT_PLAIN,
                                                (rde_color){ 1, 2, 3, 255 }, (rde_color){ 0, 0, 0, 0 }, other, (u32)strlen(other), 0);
    r.draws++;
    rt_layouts = 0; rt_lines = 0;
    fude_zoom_render_text(&r, &s, edited, view, NULL);
    CHECK(rt_layouts == 1u && rt_lines == (u32)((strlen(other) + rt_wrap_letters(9.0f, 54.0f) - 1u) / rt_wrap_letters(9.0f, 54.0f)));
    // Over a zoom of 30 frames: each laid out each frame (a size of its own each), as before.
    u32 zoomed = 0;
    for(u32 f = 0; f < 30u; f++) {
        const fude_zoom_sim z = { 1.5 * pow(1.03, (f64)(f + 1u)), 0.0, -400.0, 200.0 };
        r.draws++;
        rt_layouts = 0;
        for(u32 i = 0; i < 100u; i++) fude_zoom_render_text(&r, &s, texts[i], z, NULL);
        zoomed += rt_layouts;
    }
    CHECK(zoomed == 3000u);
    printf("  300 plain texts over 50 still draws: %u laid out (300, then 44 a draw past the bound), 15000 before (all, past draw.c's 64)\n",
           made[0] + 49u * 44u);
    printf("  100 plain texts over a zoom of 30 frames: %u laid out (each frame: their size on the screen), as before\n", zoomed);
    rt_wrap = false;
    fude_zoom_render_destroy(&fresh);
    fude_zoom_render_destroy(&r);
    fude_zoom_scene_destroy(&s);
}

// Whether kept lines m are those made in pts, parts and labels: every point, each part's run and look, each label's
// place and words.
static b8 rt_same_plot(const fude_zoom_plot_made* m, const rde_arr* pts, const rde_arr* parts, const rde_arr* labels) {
    if(rde_arr_length(&m->points) != rde_arr_length(pts) || rde_arr_length(&m->parts) != rde_arr_length(parts) || rde_arr_length(&m->labels) != rde_arr_length(labels)) return false;
    if(rde_arr_length(pts) > 0 && memcmp(m->points.memory, pts->memory, rde_arr_length(pts) * sizeof(fude_zoom_v2)) != 0) return false;
    const fude_zoom_symbol_part* a = (const fude_zoom_symbol_part*)m->parts.memory, *b = (const fude_zoom_symbol_part*)parts->memory;
    for(u32 i = 0; i < (u32)rde_arr_length(parts); i++) {
        if(a[i].first != b[i].first || a[i].count != b[i].count || a[i].flags != b[i].flags || memcmp(&a[i].fill, &b[i].fill, sizeof a[i].fill) != 0 ||
           memcmp(&a[i].line, &b[i].line, sizeof a[i].line) != 0 || a[i].width != b[i].width) return false;
    }
    const fude_zoom_plot_label* la = (const fude_zoom_plot_label*)m->labels.memory, *lb = (const fude_zoom_plot_label*)labels->memory;
    for(u32 i = 0; i < (u32)rde_arr_length(labels); i++) {
        if(la[i].at.x != lb[i].at.x || la[i].at.y != lb[i].at.y || strcmp(la[i].text, lb[i].text) != 0 || la[i].align != lb[i].align || la[i].curve != lb[i].curve ||
           strcmp(la[i].latex, lb[i].latex) != 0) return false;
    }
    return true;
}

// Graphs' lines (render.c's fude_zoom_render_plot_made) are kept by their kind, text and size, 24 of them and a spare:
// each is what plot.h makes, kept ones found where they were each draw, and past the bound what one draw uses is never
// made into: 40 graphs over 30 draws make 40 the first and 16 each after (all 40 each draw before, every one pushing
// out the next). Another text or size: lines of its own. Timed against making all of them each draw.
static void test_render_kept_graphs(void) {
    printf("graphs' lines kept by the renderer\n");
    fude_zoom_renderer r; fude_zoom_render_init(&r);
    rde_memory_allocator* heap = rde_memory_allocator_get_default_std();
    static c8 texts[40][64];
    for(u32 i = 0; i < 40u; i++) snprintf(texts[i], sizeof texts[i], "y = sin(%u x) + x^2 / %u\ny -4 4", i + 1u, i + 2u);
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), heap), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), heap), labels = rde_arr_new(sizeof(fude_zoom_plot_label), heap);
    const fude_zoom_plot_made* first[40];
    u32 wrong = 0, moved = 0;
    for(u32 d = 0; d < 30u; d++) {
        r.draws++;
        for(u32 i = 0; i < 40u; i++) {
            const fude_zoom_plot_made* m = fude_zoom_render_plot_made(&r, "graph", texts[i], 160.0, 110.0);
            if(d == 0u) first[i] = m;
            moved += i < 24u && m != first[i] ? 1u : 0u;
            if(d == 0u || d == 29u) {
                rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
                fude_zoom_plot_lines_play("graph", texts[i], 160.0, 110.0, NULL, &pts, &parts, &labels);
                wrong += rt_same_plot(m, &pts, &parts, &labels) ? 0u : 1u;
            }
        }
    }
    CHECK(wrong == 0u && moved == 0u && rde_arr_length(&first[0]->points) > 0u);
    // Another text, another size: their own lines, not a kept graph's.
    r.draws++;
    const fude_zoom_plot_made* other = fude_zoom_render_plot_made(&r, "graph", "y = x\ny -4 4", 160.0, 110.0);
    rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
    fude_zoom_plot_lines_play("graph", "y = x\ny -4 4", 160.0, 110.0, NULL, &pts, &parts, &labels);
    CHECK(rt_same_plot(other, &pts, &parts, &labels));
    const fude_zoom_plot_made* bigger = fude_zoom_render_plot_made(&r, "graph", texts[2], 200.0, 110.0);
    rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
    fude_zoom_plot_lines_play("graph", texts[2], 200.0, 110.0, NULL, &pts, &parts, &labels);
    CHECK(rt_same_plot(bigger, &pts, &parts, &labels));
    rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
    fude_zoom_plot_lines_play("graph", texts[2], 160.0, 110.0, NULL, &pts, &parts, &labels);
    CHECK(!rt_same_plot(bigger, &pts, &parts, &labels));
    // Timed: 40 graphs over 30 draws, all made each draw (as past the bound before) against kept.
    {
        clock_t c0 = clock();
        for(u32 d = 0; d < 30u; d++) {
            for(u32 i = 0; i < 40u; i++) {
                rde_arr_clear(&pts); rde_arr_clear(&parts); rde_arr_clear(&labels);
                fude_zoom_plot_lines_play("graph", texts[i], 160.0, 110.0, NULL, &pts, &parts, &labels);
            }
        }
        const f64 old_ms = (f64)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
        fude_zoom_render_trim(&r);
        c0 = clock();
        for(u32 d = 0; d < 30u; d++) {
            r.draws++;
            for(u32 i = 0; i < 40u; i++) fude_zoom_render_plot_made(&r, "graph", texts[i], 160.0, 110.0);
        }
        const f64 new_ms = (f64)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
        printf("  40 graphs over 30 draws: %.1f ms made each draw, %.1f ms kept (16 a draw past the bound)\n", old_ms, new_ms);
    }
    rde_arr_free(&pts); rde_arr_free(&parts); rde_arr_free(&labels);
    fude_zoom_render_destroy(&r);
}

// Pictures on the GPU (render.c) are found through a map by their payload's handle and serial: a picture asked again,
// or a copy sharing its payload, is the texture made once; a picture put in the slot of one let go (a new serial) gets
// its own; past the budget (256 MB) the least recently drawn are let go, the others still found where they now are.
static void test_render_pictures(void) {
    printf("pictures found by their payload\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_renderer r; fude_zoom_render_init(&r);
    rt_loads = rt_unloads = 0;
    u8 bytes[8][16];
    const u16 size[8][2] = { { 100, 50 }, { 60, 30 }, { 4096, 4000 }, { 4096, 4000 }, { 4096, 4000 }, { 4096, 4000 }, { 4096, 4000 }, { 8, 8 } };
    for(u32 i = 0; i < 8u; i++) { memset(bytes[i], (int)i, sizeof bytes[i]); memcpy(bytes[i], size[i], 4); }
    const fude_zoom_place at = { { 0, 0 }, 0.0, 1.0 };
    const u32 a = fude_zoom_scene_add_image(&s, s.root, at, 50, 25, bytes[0], sizeof bytes[0], 0);
    const u32 a2 = fude_zoom_scene_copy_stroke(&s, a, s.root, (fude_zoom_place){ { 300, 0 }, 0.5, 2.0 });
    CHECK(fude_zoom_scene_object(&s, a2)->blob == fude_zoom_scene_object(&s, a)->blob);
    r.draws++;
    rde_vec_2UI px = { 0, 0 };
    rde_texture* ta = fude_zoom_render_picture(&r, &s, a, &px);
    CHECK(ta != NULL && px.x == 100u && px.y == 50u);
    CHECK(fude_zoom_render_picture(&r, &s, a, NULL) == ta && fude_zoom_render_picture(&r, &s, a2, &px) == ta && px.x == 100u && rt_loads == 1u);
    // Both let go for good: their payload's slot is free, and the next picture takes it.
    const u32 blob = fude_zoom_scene_object(&s, a)->blob;
    fude_zoom_scene_discard(&s, a);
    fude_zoom_scene_discard(&s, a2);
    const u32 b = fude_zoom_scene_add_image(&s, s.root, at, 30, 15, bytes[1], sizeof bytes[1], 0);
    CHECK(fude_zoom_scene_object(&s, b)->blob == blob);
    r.draws++;
    rde_texture* tb = fude_zoom_render_picture(&r, &s, b, &px);
    CHECK(tb != NULL && tb != ta && px.x == 60u && px.y == 30u && rt_loads == 2u);
    // Five of 62.5 MB, each in a draw of its own: past 256 MB the least recently drawn go (a's,
    // then the first of them).
    u32 big[5];
    rde_texture* tbig[5];
    for(u32 i = 0; i < 5u; i++) {
        big[i] = fude_zoom_scene_add_image(&s, s.root, at, 100, 100, bytes[2u + i], sizeof bytes[2u + i], 0);
        r.draws++;
        tbig[i] = fude_zoom_render_picture(&r, &s, big[i], NULL);
        fude_zoom_render_picture(&r, &s, b, NULL);   // (b drawn every draw)
    }
    CHECK(!((const rt_texture*)(const any)ta)->live && ((const rt_texture*)(const any)tb)->live && !((const rt_texture*)(const any)tbig[0])->live);
    CHECK(r.picture_bytes <= FUDE_ZOOM_RENDER_TEXTURES && rt_unloads == 2u);
    // Every one left found where it is now (the map kept right as the last moved into each gap), none made again.
    const u32 loads = rt_loads;
    b8 found = rde_hash_map_length(&r.picture_of) == rde_arr_length(&r.pictures);
    const fude_zoom_picture* pic = (const fude_zoom_picture*)r.pictures.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&r.pictures); i++) {
        const u64 key = ((u64)pic[i].blob << 32) | pic[i].serial;
        const u32* at_i = (const u32*)rde_hash_map_find(&r.picture_of, &key);
        found = found && at_i != NULL && *at_i == i;
    }
    for(u32 i = 1; i < 5u; i++) found = found && fude_zoom_render_picture(&r, &s, big[i], NULL) == tbig[i];
    CHECK(found && fude_zoom_render_picture(&r, &s, b, NULL) == tb && rt_loads == loads);
    // The one let go is made again when drawn again.
    CHECK(fude_zoom_render_picture(&r, &s, big[0], &px) != NULL && px.x == 4096u && rt_loads == loads + 1u);
    fude_zoom_render_destroy(&r);
    fude_zoom_scene_destroy(&s);
}

// Strokes' covering of point p (root units), decoded the old way: each point placed, its segment as wide as its wider
// end (fude_zoom_scene_point_at, fude_zoom_scene_radius_at).
static b8 rt_old_covers(const fude_zoom_scene* s, u32 o, fude_zoom_v2 p) {
    const fude_zoom_object* ob = fude_zoom_scene_object(s, o);
    rde_arr qa; fude_zoom_qpoint* q = table(&qa, sizeof(*q), ob->count);
    b8 covers = false;
    if(fude_zoom_scene_points(s, o, q)) {
        for(u32 k = 0; k < ob->count && !covers; k++) {
            const fude_zoom_v2 a = fude_zoom_scene_point_at(ob, &q[k]), b = k + 1u < ob->count ? fude_zoom_scene_point_at(ob, &q[k + 1u]) : a;
            const f64 rr = fmax(fude_zoom_scene_radius_at(ob, &q[k]), k + 1u < ob->count ? fude_zoom_scene_radius_at(ob, &q[k + 1u]) : 0.0);
            covers = rde_math_point_segment_distance_sq_2d_f64(p, a, b) <= rr * rr;
        }
    }
    rde_arr_free(&qa);
    return covers;
}

// Frames are drawn from the renderer's stack of items (nothing allocated or freed a draw): a frame's objects in their
// draw order, a frame inside drawn in its place among them, the same each draw, the stack empty after and grown no
// more. The backdrop: zoomed eight levels into a stroke, the screen is its colour exactly where the stroke's own
// points, decoded the old way, say it covers the camera's point (inside, outside, and either side of its edge), the
// stroke decoded once and kept.
static void test_render_frames(void) {
    printf("frames drawn from the renderer's stack, and the backdrop\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_renderer r; fude_zoom_render_init(&r);
    const fude_zoom_v2 half = { 400, 300 };
    const rde_color paper = { 250, 250, 245, 255 }, c1 = { 10, 0, 0, 255 }, c2 = { 0, 20, 0, 255 }, c3 = { 0, 0, 30, 255 };
    f64 sq[8] = { -40, -40, 40, -40, 40, 40, -40, 40 };
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { -100, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_POLYGON, sq, 8u, c1, 1.0f, FUDE_ZOOM_FLAG_FILLED, 0);
    const u32 f = fude_zoom_scene_new_frame(&s, s.root, (fude_zoom_xform){ 0, 0, 0.5, 0.2 }, (fude_zoom_box){ -60, -60, 60, 60 });
    fude_zoom_scene_add_shape(&s, f, (fude_zoom_place){ { 0, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_POLYGON, sq, 8u, c2, 1.0f, FUDE_ZOOM_FLAG_FILLED, 0);
    fude_zoom_scene_add_shape(&s, s.root, (fude_zoom_place){ { 100, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_POLYGON, sq, 8u, c3, 1.0f, FUDE_ZOOM_FLAG_FILLED, 0);
    s.camera = (fude_zoom_camera){ s.root, { 0, 0 }, 1.0 };
    rde_arr draws[3];
    usize room = 0;
    for(u32 d = 0; d < 3u; d++) {
        rt_begin();
        fude_zoom_render(&r, &s, half, paper);
        rt_on = false;
        draws[d] = rt_take();
        if(d == 1u) room = r.items.capacity;
    }
    // In order: the first square's, the frame's, the last square's, each a run.
    u32 runs = 0, order[8];
    const rt_tri* t = (const rt_tri*)draws[0].memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&draws[0]); i++) {
        const u32 col = t[i].c.r == 10 ? 1u : (t[i].c.g == 20 ? 2u : 3u);
        if(runs == 0u || order[runs - 1u] != col) { if(runs < 8u) order[runs] = col; runs++; }
    }
    CHECK(runs == 3u && order[0] == 1u && order[1] == 2u && order[2] == 3u);
    CHECK(rde_arr_length(&draws[1]) == rde_arr_length(&draws[0]) && memcmp(draws[1].memory, draws[0].memory, rde_arr_length(&draws[0]) * sizeof(rt_tri)) == 0);
    CHECK(rde_arr_length(&r.items) == 0u && r.items.capacity == room);
    for(u32 d = 0; d < 3u; d++) rde_arr_free(&draws[d]);
    fude_zoom_render_destroy(&r);
    fude_zoom_scene_destroy(&s);

    // The backdrop, eight levels into a stroke 100 long, about 11.7 wide each side (its pressure's share of 20).
    fude_zoom_scene b; fude_zoom_scene_init(&b, 7);
    fude_zoom_renderer rb; fude_zoom_render_init(&rb);
    const u32 st = line(&b, -50, 0, 50, 0, 2.0, 20.0f);
    const f64 ys[6] = { 0.0, 5.0, 11.70, 11.76, 30.0, -11.72 };
    u32 agree = 0;
    for(u32 i = 0; i < 6u; i++) {
        fude_zoom_camera_look_at(&b, b.root, (fude_zoom_v2){ 3.0, ys[i] }, 1.0);
        for(u32 k = 0; k < 80u; k++) { fude_zoom_camera_zoom_at(&b, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&b, half); }
        const fude_zoom_v2 p = fude_zoom_sim_apply(fude_zoom_scene_sim(&b, b.camera.frame, b.root), b.camera.at);
        const b8 covers = rt_old_covers(&b, st, p);
        rt_begin();
        fude_zoom_render(&rb, &b, half, paper);
        rt_on = false;
        const b8 shown = rt_rects > 0u && rt_rect.r == 10 && rt_rect.g == 20 && rt_rect.b == 30;
        agree += shown == covers && fude_zoom_scene_depth(&b, b.camera.frame) >= 8u ? 1u : 0u;
        if(shown != covers) printf("  at y %.2f: covered %d, drawn %d\n", ys[i], covers, shown);
    }
    CHECK(agree == 6u && st < (u32)rde_arr_length(&rb.slot_of) && ((const u32*)rb.slot_of.memory)[st] != 0u);
    fude_zoom_render_destroy(&rb);
    fude_zoom_scene_destroy(&b);
}

// The backdrop and the frame a new stroke goes in, with layers. What a hidden layer holds covers nothing: zoomed eight
// levels into a hidden stroke the screen is the paper, not its colour, and a later stroke on a hidden layer over the
// camera's frame makes no new frame on top. Under and over are by draw key, as the frames are drawn: a stroke on a
// layer ranked above layer 0 (a frame's rank) is drawn over the frames in its frame, though it was drawn before they
// were made, so its colour goes over everything the frames draw, and no new frame is made for it (it would be under
// that stroke too); a later one on layer 0 still makes one, as before.
static void test_cover_layers(void) {
    printf("the backdrop and later on top, with layers\n");
    const fude_zoom_v2 half = { 400, 300 };
    const rde_color paper = { 250, 250, 245, 255 }, deep_ink = { 0, 200, 0, 255 };
    // The backdrop: a stroke on layer 1, the camera eight levels into it, a filled square there filling the screen.
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_renderer r; fude_zoom_render_init(&r);
    s.layer = 1;
    line(&s, -50, 0, 50, 0, 2.0, 20.0f);
    s.layer = 0;
    fude_zoom_camera_look_at(&s, s.root, (fude_zoom_v2){ 3.0, 5.0 }, 1.0);
    for(u32 k = 0; k < 80u; k++) { fude_zoom_camera_zoom_at(&s, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&s, half); }
    CHECK(fude_zoom_scene_depth(&s, s.camera.frame) >= 8u);
    const f64 e = 1000.0 / s.camera.z;
    const f64 sq[8] = { -e, -e, e, -e, e, e, -e, e };
    fude_zoom_scene_add_shape(&s, s.camera.frame, (fude_zoom_place){ s.camera.at, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_POLYGON, sq, 8u, deep_ink, 0.0f, FUDE_ZOOM_FLAG_FILLED, 0);
    rt_begin();
    fude_zoom_render(&r, &s, half, paper);
    rt_on = false;
    const u32 drawn = (u32)rde_arr_length(&rt_tris);
    // Layer 1 is ranked over layer 0, so over the frame on the camera's way: its colour last, over the square.
    CHECK(drawn > 0u && rt_rects == 1u && rt_rect.r == 10 && rt_rect.g == 20 && rt_rect.b == 30 && rt_rect_after == drawn);
    // Moved under layer 0: under the frame, so its colour first, the square over it.
    fude_zoom_scene_add_layer(&s, s.root, 1, fude_zoom_layer_with_rank(0u, 0u), "");
    fude_zoom_scene_add_layer(&s, s.root, 0, fude_zoom_layer_with_rank(0u, 1u), "");
    fude_zoom_scene_layers_refresh(&s);
    rt_begin();
    fude_zoom_render(&r, &s, half, paper);
    rt_on = false;
    CHECK(rt_rects == 1u && rt_rect.r == 10 && rt_rect_after == 0u && (u32)rde_arr_length(&rt_tris) == drawn);
    // Hidden: not drawn, so not the screen's colour either.
    fude_zoom_scene_add_layer(&s, s.root, 1, fude_zoom_layer_with_rank(FUDE_ZOOM_LAYER_HIDDEN, 0u), "");
    fude_zoom_scene_layers_refresh(&s);
    rt_begin();
    fude_zoom_render(&r, &s, half, paper);
    rt_on = false;
    CHECK(rt_rects == 0u && (u32)rde_arr_length(&rt_tris) == drawn);
    fude_zoom_render_destroy(&r);
    fude_zoom_scene_destroy(&s);

    // Later on top: a line in a frame zoomed into, then a stroke over it from its parent on another layer.
    fude_zoom_scene t; fude_zoom_scene_init(&t, 7);
    line(&t, -200, 0, 200, 0, 1.0, 1.0f);   // (something in the root, so the camera goes out to it)
    fude_zoom_camera_look_at(&t, t.root, (fude_zoom_v2){ 5000, 5000 }, 1.0);
    for(u32 k = 0; k < 7; k++) { fude_zoom_camera_zoom_at(&t, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&t, half); }
    const u32 detail = t.camera.frame;
    CHECK(detail != t.root);
    line(&t, t.camera.at.x - 10, t.camera.at.y, t.camera.at.x + 10, t.camera.at.y, 0.5, 1.0f);
    const u32 over_layers[3] = { 1, 2, 0 };   // hidden, ranked above, layer 0's own
    const u8  over_flags[3]  = { FUDE_ZOOM_LAYER_HIDDEN, 0u, 0u };
    b8 fresh[3];
    for(u32 i = 0; i < 3u; i++) {
        for(u32 k = 0; k < 7; k++) { fude_zoom_camera_zoom_at(&t, (fude_zoom_v2){ 0, 0 }, 0.5); fude_zoom_camera_settle(&t, half); }
        CHECK(t.camera.frame == t.root);
        if(over_flags[i] != 0u) {
            fude_zoom_scene_add_layer(&t, t.root, (u16)over_layers[i], over_flags[i], "");
            fude_zoom_scene_layers_refresh(&t);
        }
        t.layer = (u16)over_layers[i];
        line(&t, t.camera.at.x - 50, t.camera.at.y, t.camera.at.x + 50, t.camera.at.y, 1.0, 20.0f);   // over the detail
        t.layer = 0;
        for(u32 k = 0; k < 7; k++) { fude_zoom_camera_zoom_at(&t, (fude_zoom_v2){ 0, 0 }, 2.0); fude_zoom_camera_settle(&t, half); }
        CHECK(t.camera.frame == detail);
        fresh[i] = fude_zoom_camera_drawing_frame(&t, (fude_zoom_box){ t.camera.at.x - 1, t.camera.at.y - 1, t.camera.at.x + 1, t.camera.at.y + 1 }) != detail;
    }
    CHECK(!fresh[0] && !fresh[1] && fresh[2]);
    fude_zoom_scene_destroy(&t);
}

// --- strokes' meshes kept (render_stroke.c) ----------------------------------------------------------------------

// Strokes' meshes as the renderer draws them (rde_rendering_2d_draw_stroke_mesh; engine.c's double draws nothing).
// rs_mode 1: each triangle's corners recorded where the engine puts them (the mesh's vertex moved by the translation)
// with the colour it gives them (its alpha the colour's times the vertex's share), into rs_corners; rs_mode 2: the
// vertices and indices copied into a batch as the engine's append does (for the timings), nothing kept. rs_meshes
// counts the meshes drawn. rs_pixel is the pixel's size the renderer is told (the engine's, in a drawing).
typedef struct { f32 x, y; rde_color c; } rs_corner;
static rde_arr rs_corners, rs_batch_v, rs_batch_i;
static u32     rs_mode = 0, rs_meshes = 0;
static f32     rs_pixel = 1.0f;
void rde_rendering_2d_draw_stroke_mesh(const rde_stroke_mesh* m, rde_vec_2F t, rde_color c) {
    if(rs_mode == 0u || m == NULL || rde_arr_length(&m->vertices) == 0 || c.a == 0) return;
    rs_meshes++;
    const rde_stroke_vertex* v = (const rde_stroke_vertex*)m->vertices.memory;
    const u32* idx = (const u32*)m->indices.memory;
    const u32 nv = (u32)rde_arr_length(&m->vertices), ni = (u32)rde_arr_length(&m->indices);
    if(rs_mode == 1u) {
        rs_corner* o = (rs_corner*)rde_arr_add_n(&rs_corners, ni);
        for(u32 i = 0; i < ni; i++) {
            const rde_stroke_vertex* p = &v[idx[i]];
            o[i] = (rs_corner){ p->position.x + t.x, p->position.y + t.y, { c.r, c.g, c.b, (u8)((f32)c.a * p->alpha) } };
        }
        return;
    }
    const u32 first = (u32)rde_arr_length(&rs_batch_v) / 4u;
    f32* bv = (f32*)rde_arr_add_n(&rs_batch_v, (usize)nv * 4u);
    for(u32 i = 0; i < nv; i++) {
        bv[i * 4u] = v[i].position.x + t.x + 600.0f; bv[i * 4u + 1u] = 400.0f - (v[i].position.y + t.y); bv[i * 4u + 2u] = 0.0f;
        bv[i * 4u + 3u] = (f32)(u32)((f32)c.a * v[i].alpha);
    }
    u32* bi = (u32*)rde_arr_add_n(&rs_batch_i, ni);
    for(u32 i = 0; i < ni; i++) bi[i] = first + idx[i];
}
f32 rde_rendering_2d_get_pixel_size(void) { return rs_pixel; }

// The renderer drawn with rs_mode 1: the corners it drew, in a new array (rde_arr_free after).
static rde_arr rs_draw(fude_zoom_renderer* r, const fude_zoom_scene* s, fude_zoom_v2 half) {
    if(!rde_arr_is_inited(&rs_corners)) rs_corners = rde_arr_new(sizeof(rs_corner), NULL);
    rde_arr_clear(&rs_corners);
    rs_mode = 1u;
    fude_zoom_render(r, s, half, (rde_color){ 250, 250, 245, 255 });
    rs_mode = 0u;
    rde_arr a = rde_arr_new(sizeof(rs_corner), NULL);
    if(rde_arr_length(&rs_corners) > 0) memcpy(rde_arr_add_n(&a, rde_arr_length(&rs_corners)), rs_corners.memory, rde_arr_length(&rs_corners) * sizeof(rs_corner));
    return a;
}
static b8 rs_same(const rde_arr* a, const rde_arr* b) {
    return rde_arr_length(a) == rde_arr_length(b) && (rde_arr_length(a) == 0 || memcmp(a->memory, b->memory, rde_arr_length(a) * sizeof(rs_corner)) == 0);
}
// Whether b is a moved by (dx, dy), each corner to within the rounding of its f32 sum, in the same colour.
static b8 rs_moved(const rde_arr* a, const rde_arr* b, f32 dx, f32 dy) {
    if(rde_arr_length(a) != rde_arr_length(b)) return false;
    const rs_corner* p = (const rs_corner*)a->memory; const rs_corner* q = (const rs_corner*)b->memory;
    for(u32 i = 0; i < (u32)rde_arr_length(a); i++) {
        if(fabs((f64)p[i].x + dx - q[i].x) > 1e-3 || fabs((f64)p[i].y + dy - q[i].y) > 1e-3 || memcmp(&p[i].c, &q[i].c, sizeof(rde_color)) != 0) return false;
    }
    return true;
}
// The sum of the decoded entries' meshes' bytes: what the renderer's count must be.
static u64 rs_mesh_bytes(const fude_zoom_renderer* r) {
    u64 n = 0;
    const fude_zoom_decoded* d = (const fude_zoom_decoded*)r->cache.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&r->cache); i++) n += d[i].mesh_bytes;
    return n;
}

// The old way, kept here as the reference: stroke o tessellated where it is on the screen (its points moved there in
// f64 and thinned there, as render_stroke.c did before its meshes were kept), its corners appended to out.
static void rs_old_stroke(const fude_zoom_scene* s, u32 o, fude_zoom_sim to, rde_stroke_mesh* m, rde_arr* out) {
    const fude_zoom_object* ob = fude_zoom_scene_object(s, o);
    rde_arr qa; fude_zoom_qpoint* q = table(&qa, sizeof(*q), ob->count);
    fude_zoom_scene_points(s, o, q);
    const fude_zoom_sim all = fude_zoom_sim_compose(to, fude_zoom_object_sim(ob));
    const f64 ws = fude_zoom_sim_scale(to) * ob->scale;
    rde_arr pa = rde_arr_new(sizeof(rde_vec_2F), NULL), ra = rde_arr_new(sizeof(f32), NULL);
    u32 n = 0;
    for(u32 i = 0; i < ob->count; i++) {
        const f64 x = (f32)ldexp((f64)q[i].x, ob->q), y = (f32)ldexp((f64)q[i].y, ob->q);
        const f32 sx = (f32)(all.tx + all.a * x - all.b * y), sy = (f32)(all.ty + all.b * x + all.a * y);
        f32 sr = (f32)((f64)fude_zoom_scene_local_radius_at(ob, &q[i]) * ws);
        sr = sr < FUDE_ZOOM_RENDER_MIN_RADIUS ? FUDE_ZOOM_RENDER_MIN_RADIUS : sr;
        if(n > 0 && i + 1u < ob->count) {
            const rde_vec_2F l = ((const rde_vec_2F*)pa.memory)[n - 1u];
            const f32 st = (ob->flags & FUDE_ZOOM_FLAG_MARKER) ? sr * 0.75f : 0.6f;
            if((sx - l.x) * (sx - l.x) + (sy - l.y) * (sy - l.y) < st * st) continue;
        }
        const rde_vec_2F p = { sx, sy };
        rde_arr_add(&pa, (any)&p); rde_arr_add(&ra, (any)&sr); n++;
    }
    rde_rendering_2d_tessellate_stroke((const rde_vec_2F*)pa.memory, (const f32*)ra.memory, n, rs_pixel, m);
    const rde_stroke_vertex* v = (const rde_stroke_vertex*)m->vertices.memory;
    const u32* idx = (const u32*)m->indices.memory;
    const rde_color c = fude_theme_resolve(ob->color);
    for(u32 i = 0; i < (u32)rde_arr_length(&m->indices); i++) {
        const rs_corner k = { v[idx[i]].position.x, v[idx[i]].position.y, { c.r, c.g, c.b, (u8)((f32)c.a * v[idx[i]].alpha) } };
        rde_arr_add(out, (any)&k);
    }
    rde_arr_free(&pa); rde_arr_free(&ra); rde_arr_free(&qa);
}

// A flower of 400 points round c, as the gate draws them (look.c's look_demo_curve), half-width w,
// in the camera's frame.
static u32 rs_flower(fude_zoom_scene* s, fude_zoom_v2 c, f64 rad, f64 phase, u32 petals, rde_color k, f64 w) {
    const i8 q = fude_zoom_quantum_for(s->camera.z);
    const f64 g = ldexp(1.0, q);
    fude_zoom_qpoint pts[400];
    fude_zoom_v2 first = { 0, 0 };
    for(u32 i = 0; i < 400u; i++) {
        const f64 a = 2.0 * 3.14159265358979 * (f64)i / 399.0, kk = rad * (0.55 + 0.45 * cos((f64)petals * a + phase));
        const fude_zoom_v2 p = { c.x + cos(a) * kk, c.y + sin(a) * kk };
        if(i == 0) first = p;
        pts[i] = (fude_zoom_qpoint){ (i32)llround((p.x - first.x) / g), (i32)llround((p.y - first.y) / g), (u16)(700 + 300 * sin(a * 3.0)), i * 4u };
    }
    return fude_zoom_scene_add_stroke(s, s->camera.frame, first, q, pts, 400u, FUDE_ZOOM_CHANNEL_PRESSURE | FUDE_ZOOM_CHANNEL_TIME, k, (f32)(w / s->camera.z), FUDE_ZOOM_FLAG_PRESSURE, 0, 0);
}

// Strokes are drawn from meshes kept with their decoded points (render_stroke.c). A kept mesh draws exactly what one
// made in the same draw does, and what a renderer that keeps none draws; against the old way (each stroke tessellated
// where it is on the screen) the same triangles in the same colours, each corner within a hundredth of a pixel. Kept
// while the view pans (nothing made; every corner moved by the pan), while a stroke is moved without being turned or
// scaled, through a change of theme, a faint stroke and a glow (the colour is the draw's, not the mesh's), and through
// a hidden layer, and through an erase and its undo; made again on a zoom, for a stroke turned or scaled, at another
// pixel size, and once for a new stroke. The bound: never more held than the budget, the renderer's count the entries'
// sum, also when the decoded strokes' own budget lets entries go; past it, what one draw keeps stays kept, the rest are
// made each draw and drawn the same; a trim keeps nothing.
static void test_render_kept_strokes(void) {
    printf("strokes' meshes kept by the renderer\n");
    const fude_zoom_v2 half = { 400, 300 };
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_camera_look_at(&s, s.root, (fude_zoom_v2){ 0, 0 }, 1.0);
    u32 strokes[12];
    for(u32 i = 0; i < 8u; i++) {
        strokes[i] = rs_flower(&s, (fude_zoom_v2){ -300.0 + 85.0 * i, (i % 2u) ? 120.0 : -90.0 }, 30.0 + 4.0 * i, 0.3 * i, 3u + i % 4u, (rde_color){ (u8)(30 * i), 60, 200, 255 }, 0.3 + 0.9 * i);
    }
    strokes[8] = line(&s, -350, -250, 350, 240, 3.0, 6.0f);
    const fude_zoom_v2 zig[5] = { { -200, 200 }, { -150, 260 }, { -100, 200 }, { -50, 262 }, { 0, 200 } };
    strokes[9] = poly_stroke(&s, zig, 5u, false);
    const fude_zoom_qpoint dot = { 0, 0, 900, 0 };
    strokes[10] = fude_zoom_scene_add_stroke(&s, s.root, (fude_zoom_v2){ 250, -200 }, 0, &dot, 1u, FUDE_ZOOM_CHANNEL_PRESSURE, FUDE_THEME_INK, 9.0f, FUDE_ZOOM_FLAG_PRESSURE, 0, 0);
    strokes[11] = rs_flower(&s, (fude_zoom_v2){ 150, -40 }, 60.0, 1.0, 5u, (rde_color){ 250, 220, 0, 110 }, 12.0);   // a see-through marker
    ((fude_zoom_object*)fude_zoom_scene_object(&s, strokes[11]))->flags |= FUDE_ZOOM_FLAG_MARKER;
    fude_zoom_renderer r; fude_zoom_render_init(&r);

    // Made, then kept: the same corners, bit for bit, and those of a renderer that keeps none.
    rde_arr first = rs_draw(&r, &s, half);
    const u32 made_first = r.meshes_made, drawn = r.strokes_drawn;
    rde_arr kept = rs_draw(&r, &s, half);
    CHECK(made_first == 12u && drawn == 12u && r.meshes_made == 0u && rde_arr_length(&first) > 0 && rs_same(&first, &kept));
    CHECK(r.mesh_bytes > 0u && r.mesh_bytes == rs_mesh_bytes(&r));
    fude_zoom_renderer fresh; fude_zoom_render_init(&fresh);
    fresh.mesh_budget = 0u;   // (it keeps none: every stroke is made in each draw)
    rde_arr none = rs_draw(&fresh, &s, half);
    CHECK(rs_same(&first, &none) && fresh.meshes_made == 12u && fresh.mesh_bytes == 0u);
    rde_arr_free(&none);

    // Against the old way: the same triangles in the same colours, each corner within a hundredth of a pixel.
    {
        rde_stroke_mesh m = rde_stroke_mesh_new(NULL);
        rde_arr old = rde_arr_new(sizeof(rs_corner), NULL);
        const fude_zoom_sim to = fude_zoom_camera_sim(&s.camera);
        for(u32 i = 0; i < 12u; i++) rs_old_stroke(&s, strokes[i], to, &m, &old);   // (made in order: drawn in order)
        f64 most = 0.0;
        b8 colours = rde_arr_length(&old) == rde_arr_length(&first);
        const rs_corner* a = (const rs_corner*)old.memory; const rs_corner* b = (const rs_corner*)first.memory;
        for(u32 i = 0; colours && i < (u32)rde_arr_length(&old); i++) {
            most = fmax(most, fmax(fabs((f64)a[i].x - b[i].x), fabs((f64)a[i].y - b[i].y)));
            colours = memcmp(&a[i].c, &b[i].c, sizeof(rde_color)) == 0;
        }
        printf("  against tessellating where they are on the screen: %u corners, each within %.2g of a pixel\n", (u32)rde_arr_length(&old), most);
        CHECK(colours && most < 0.01);
        rde_arr_free(&old); rde_stroke_mesh_free(&m);
    }

    // A pan: nothing made, every corner moved by it; and what a renderer that keeps none draws there.
    s.camera.at.x += 37.25; s.camera.at.y -= 11.5;
    rde_arr panned = rs_draw(&r, &s, half);
    CHECK(r.meshes_made == 0u && rs_moved(&first, &panned, -37.25f, 11.5f));
    rde_arr there = rs_draw(&fresh, &s, half);
    CHECK(rs_same(&panned, &there));
    rde_arr_free(&there);
    // A zoom: every one made again, as one that keeps none makes it; zoomed back, made again, as at first.
    s.camera.z *= 1.25;
    rde_arr zoomed = rs_draw(&r, &s, half);
    there = rs_draw(&fresh, &s, half);
    CHECK(r.meshes_made == r.strokes_drawn && r.strokes_drawn > 0u && rs_same(&zoomed, &there));
    rde_arr_free(&there); rde_arr_free(&zoomed);
    s.camera.z /= 1.25; s.camera.at.x -= 37.25; s.camera.at.y += 11.5;
    rde_arr back = rs_draw(&r, &s, half);
    CHECK(r.meshes_made == 12u && rs_same(&first, &back));
    rde_arr_free(&back);
    // Another pixel size (a picture drawn off the screen, a window resized): made again; back, made again as at first.
    rs_pixel = 0.5f;
    rde_arr finer = rs_draw(&r, &s, half);
    CHECK(r.meshes_made == 12u && !rs_same(&first, &finer));
    rde_arr_free(&finer);
    rs_pixel = 1.0f;
    back = rs_draw(&r, &s, half);
    CHECK(r.meshes_made == 12u && rs_same(&first, &back));
    rde_arr_free(&back);

    // The colour is the draw's: a theme's ink, a faint stroke, a glow; none of them makes the stroke's mesh again.
    fude_theme_set(FUDE_THEME_NIGHT);
    rde_arr night = rs_draw(&r, &s, half);
    fude_theme_set(FUDE_THEME_PAPER);
    CHECK(r.meshes_made == 0u && rde_arr_length(&night) == rde_arr_length(&first) && !rs_same(&night, &first));
    rde_arr_free(&night);
    rde_arr faint = rde_arr_new(sizeof(u8), NULL);
    rde_arr_resize(&faint, fude_zoom_scene_object_count(&s));
    ((u8*)faint.memory)[strokes[3]] = 1u;
    r.faint = &faint;
    rde_arr faded = rs_draw(&r, &s, half);
    r.faint = NULL;
    CHECK(r.meshes_made == 0u && rde_arr_length(&faded) == rde_arr_length(&first) && !rs_same(&faded, &first));
    rde_arr_free(&faded); rde_arr_free(&faint);
    r.meshes_made = 0u;
    fude_zoom_render_glow(&r, &s, strokes[3], fude_zoom_camera_sim(&s.camera), half, 3.0f, (rde_color){ 0, 120, 255, 90 });
    const u32 glow_made = r.meshes_made;
    rde_arr after_glow = rs_draw(&r, &s, half);
    CHECK(glow_made == 1u && r.meshes_made == 0u && rs_same(&first, &after_glow));
    rde_arr_free(&after_glow);

    // Moved alone, a stroke keeps its mesh; turned and scaled, it is made again; put back, made again as it was.
    const fude_zoom_place was = fude_zoom_scene_place_of(&s, strokes[2]);
    fude_zoom_scene_set_place(&s, strokes[2], (fude_zoom_place){ { was.t.x + 5.0, was.t.y }, was.rotation, was.scale });
    rde_arr moved = rs_draw(&r, &s, half);
    CHECK(r.meshes_made == 0u && rde_arr_length(&moved) == rde_arr_length(&first) && !rs_same(&moved, &first));
    rde_arr_free(&moved);
    fude_zoom_scene_set_place(&s, strokes[2], (fude_zoom_place){ was.t, was.rotation + 0.3, was.scale * 1.5 });
    rde_arr turned = rs_draw(&r, &s, half);
    CHECK(r.meshes_made == 1u);
    rde_arr_free(&turned);
    fude_zoom_scene_set_place(&s, strokes[2], was);
    back = rs_draw(&r, &s, half);
    CHECK(r.meshes_made == 1u && rs_same(&first, &back));
    rde_arr_free(&back);
    // A hidden layer's stroke is not drawn, and keeps its mesh for when it shows again.
    s.layer = 1;
    const u32 on_layer = line(&s, -100, 0, 100, 10, 2.0, 3.0f);
    s.layer = 0;
    rde_arr with_layer = rs_draw(&r, &s, half);
    CHECK(r.meshes_made == 1u);
    fude_zoom_scene_add_layer(&s, s.root, 1, FUDE_ZOOM_LAYER_HIDDEN, "");
    fude_zoom_scene_layers_refresh(&s);
    rde_arr hidden = rs_draw(&r, &s, half);
    CHECK(r.meshes_made == 0u && r.strokes_drawn == 12u && rs_same(&first, &hidden));
    fude_zoom_scene_add_layer(&s, s.root, 1, 0u, "");
    fude_zoom_scene_layers_refresh(&s);
    rde_arr shown = rs_draw(&r, &s, half);
    CHECK(r.meshes_made == 0u && rs_same(&with_layer, &shown));
    rde_arr_free(&hidden); rde_arr_free(&with_layer);

    // Edits. The stroke eraser takes the long line and the layer's line whole: not drawn, nothing made. Undone, they
    // come back drawn from the meshes kept with them (an object's points never change). A new stroke is made once,
    // then kept, and drawn as a renderer that keeps none draws it.
    fude_zoom_eraser e; fude_zoom_eraser_init(&e);
    fude_zoom_erase_begin(&e, FUDE_ZOOM_ERASE_STROKE);
    fude_zoom_erase_step(&s, &e, s.root, (fude_zoom_v2){ -10, -40 }, (fude_zoom_v2){ 10, 40 }, 6.0, 0.5);
    fude_zoom_erase_end(&s, &e, s.root, fude_zoom_box_empty());
    fude_zoom_eraser_destroy(&e);
    CHECK(!alive(&s, strokes[8]) && !alive(&s, on_layer));
    rde_arr erased = rs_draw(&r, &s, half);
    CHECK(r.meshes_made == 0u && r.strokes_drawn == 11u && rde_arr_length(&erased) < rde_arr_length(&first));
    CHECK(fude_zoom_history_undo(&s) && alive(&s, strokes[8]) && alive(&s, on_layer));
    rde_arr undone = rs_draw(&r, &s, half);
    CHECK(r.meshes_made == 0u && rs_same(&undone, &shown));
    const u32 added = line(&s, -380, 280, 380, 280, 2.0, 2.5f);
    rde_arr edited = rs_draw(&r, &s, half);
    const u32 made_new = r.meshes_made;
    rde_arr again = rs_draw(&r, &s, half);
    there = rs_draw(&fresh, &s, half);
    CHECK(alive(&s, added) && made_new == 1u && r.meshes_made == 0u && r.strokes_drawn == 14u && rs_same(&edited, &again) && rs_same(&edited, &there));
    rde_arr_free(&erased); rde_arr_free(&undone); rde_arr_free(&shown);
    rde_arr_free(&edited); rde_arr_free(&again); rde_arr_free(&there);

    // The bound: room for about half of them. Never over it, the entries' sum the count; what one draw keeps stays
    // kept (each draw makes the same few again, not all of them), and it draws what a renderer with room draws.
    fude_zoom_renderer small; fude_zoom_render_init(&small);
    small.mesh_budget = r.mesh_bytes / 2u;
    rde_arr roomy = rs_draw(&r, &s, half);
    u32 made[4];
    b8 same = true, under = true;
    for(u32 d = 0; d < 4u; d++) {
        rde_arr a = rs_draw(&small, &s, half);
        made[d] = small.meshes_made;
        same = same && rs_same(&a, &roomy);
        under = under && small.mesh_bytes <= small.mesh_budget && small.mesh_bytes == rs_mesh_bytes(&small);
        rde_arr_free(&a);
    }
    CHECK(same && under && made[0] == small.strokes_drawn && made[1] > 0u && made[1] < made[0] && made[2] == made[1] && made[3] == made[1]);
    // The decoded strokes' budget lets their entries go, meshes and all: the count follows.
    small.cache_budget = 1u;
    small.mesh_budget  = FUDE_ZOOM_RENDER_MESHES;
    for(u32 d = 0; d < 2u; d++) {
        rde_arr tight = rs_draw(&small, &s, half);
        CHECK(rs_same(&tight, &roomy) && small.mesh_bytes == rs_mesh_bytes(&small));
        rde_arr_free(&tight);
    }
    // Trimmed: nothing kept, all made again at the next draw.
    fude_zoom_render_trim(&r);
    CHECK(r.mesh_bytes == 0u);
    rde_arr trimmed = rs_draw(&r, &s, half);
    CHECK(r.meshes_made == r.strokes_drawn && rs_same(&trimmed, &roomy));
    rde_arr_free(&trimmed); rde_arr_free(&roomy);

    rde_arr_free(&first); rde_arr_free(&kept); rde_arr_free(&panned);
    rde_arr_free(&rs_corners);
    fude_zoom_render_destroy(&small);
    fude_zoom_render_destroy(&fresh);
    fude_zoom_render_destroy(&r);
    fude_zoom_scene_destroy(&s);
}

// Timed: a screen of the gate's flowers (--gate: 625 strokes of 400 points on the screen at once, each its own colour),
// handed over as the engine takes them (rs_mode 2: every vertex and index copied into a batch). A frame tessellating
// every stroke (a renderer that keeps no mesh, as before) against one drawn from the kept meshes, still and panning;
// and a zoom, which makes every mesh each frame (no worse than before).
static void test_render_kept_strokes_timed(void) {
    printf("strokes' meshes kept: timed\n");
    const fude_zoom_v2 half = { 640, 400 };
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    fude_zoom_camera_look_at(&s, s.root, (fude_zoom_v2){ 0, 0 }, 1.0);
    u32 seed = 7u;
    for(u32 i = 0; i < 625u; i++) {
        seed = seed * 1664525u + 1013904223u;
        const f64 x = ((f64)(seed >> 8 & 0xFFFF) / 65535.0 - 0.5) * 1200.0;
        seed = seed * 1664525u + 1013904223u;
        const f64 y = ((f64)(seed >> 8 & 0xFFFF) / 65535.0 - 0.5) * 760.0;
        rs_flower(&s, (fude_zoom_v2){ x, y }, 40.0, (f64)(seed & 255u), 3u + seed % 5u, (rde_color){ (u8)(seed >> 3), (u8)(seed >> 11), (u8)(seed >> 19), 255 }, 2.0);
    }
    rs_batch_v = rde_arr_new(sizeof(f32), NULL);
    rs_batch_i = rde_arr_new(sizeof(u32), NULL);
    fude_zoom_renderer old; fude_zoom_render_init(&old);
    old.mesh_budget = 0u;
    fude_zoom_renderer r; fude_zoom_render_init(&r);
    const u32 frames = 30u;
    f64 ms[4];
    for(u32 way = 0; way < 4u; way++) {   // tessellating every frame; kept, still; kept, panning; zooming
        fude_zoom_renderer* rr = way == 0u ? &old : &r;
        fude_zoom_camera_look_at(&s, s.root, (fude_zoom_v2){ 0, 0 }, 1.0);
        rs_mode = 2u;
        fude_zoom_render(rr, &s, half, (rde_color){ 250, 250, 245, 255 });   // (decoded, and kept where it keeps)
        const clock_t c0 = clock();
        for(u32 f = 0; f < frames; f++) {
            if(way == 2u) s.camera.at.x += 3.0;
            if(way == 3u) s.camera.z *= 1.01;
            rde_arr_clear(&rs_batch_v); rde_arr_clear(&rs_batch_i);
            fude_zoom_render(rr, &s, half, (rde_color){ 250, 250, 245, 255 });
        }
        ms[way] = (f64)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC / frames;
        rs_mode = 0u;
        if(way == 1u) {
            printf("  %u strokes, %u points drawn: %u vertices and %u indices a frame; their meshes hold %.1f MB\n", r.strokes_drawn, r.points_drawn,
                   (u32)rde_arr_length(&rs_batch_v) / 4u, (u32)rde_arr_length(&rs_batch_i), (f64)r.mesh_bytes / 1048576.0);
            CHECK(r.meshes_made == 0u && r.strokes_drawn == 625u);
        }
        if(way == 3u) CHECK(r.meshes_made == r.strokes_drawn);
    }
    printf("  a frame: tessellating every stroke %.2f ms; from kept meshes %.2f ms still, %.2f ms panning; zooming %.2f ms\n", ms[0], ms[1], ms[2], ms[3]);
    CHECK(ms[1] < ms[0] && ms[2] < ms[0]);
    rde_arr_free(&rs_batch_v); rde_arr_free(&rs_batch_i);
    fude_zoom_render_destroy(&old);
    fude_zoom_render_destroy(&r);
    fude_zoom_scene_destroy(&s);
}

int main(void) {
    mirror_kinds();
    test_part_words();
    test_tap_words();
    test_meter_units();
    test_display_limits();
    test_orphans_chain();
    test_audio_begun();
    test_codec();
    test_sim_and_index();
    test_frames();
    test_erasers();
    test_instruments();
    test_texts();
    test_layers();
    test_clear();
    test_connectors();
    test_layer_moves();
    test_select_abort();
    test_driving();
    test_boards();
    test_paths();
    test_arcs();
    test_video_yuv();
    test_hand_lines();
    test_symbols();
    test_brackets_shapes();
    test_rich();
    test_papers();
    test_charts();
    test_map();
    test_sheets();
    test_pieces();
    test_sculpt();
    test_cuts();
    test_snap_cross();
    test_snap_from();
    test_guides();
    test_trim();
    test_combine();
    test_constraints();
    test_follow_many();
    test_circuits();
    test_canvas_logic();
    test_play_tags();
    test_probe_light();
    test_led_colours();
    test_big_circuits();
    test_limits();
    test_servo_alone();
    test_display_sizes();
    test_display_leds();
    test_cd4511();
    test_lm3914();
    test_panel_meter();
    test_lcd();
    test_scope();
    test_scope_knobs();
    test_sensors_switches();
    test_mech_parts_alone();
    test_batch5_parts();
    test_circuit_jumps();
    test_batch5_examples();
    test_big_examples();
    test_quiet_blocks();
    test_logic_nodes();
    test_attributes_gathered();
    test_wire_scratch();
    test_products();
    test_maths_font();
    test_tex();
    test_formulas();
    test_texcalc();
    test_handmath();
    test_mathread();
    test_pages();
    test_history_tags();
    test_transcript();
    test_words();
    test_home_list();
    test_inktest();
    test_audio();
    test_voice_lost();
    test_pdf_pictures_once();
    test_display_examples();
    test_examples_go_wrong();
    test_logic_draw();
    test_logic_draw_order();
    test_circuit_nodes_order();
    test_custom();
    test_example_sets();
    test_examples_electronics();
    test_mechanisms();
    test_calc();
    test_drawn_bodies();
    test_plot();
    test_plot_curves();
    test_repeat();
    test_dims();
    test_select_frames();
    test_stl();
    test_nest();
    test_bucket();
    test_offsets();
    test_history();
    test_moves();
    test_shapes();
    test_pictures();
    test_smoothing();
    test_navigation();
    test_filling();
    test_fill_edges();
    test_export();
    test_svg_alpha();
    test_file();
    test_file_failed_write();
    test_crc();
    test_index_clean();
    test_load_whole();
    test_child_at();
    test_load_big();
    test_far_views();
    test_cut_crossings();
    test_deep_latex();
    test_port_names();
    test_bad_numbers();
    test_radial_ids();
    test_dim_ids();
    test_piece_rename_failed();
    test_far_lasso();
    test_paper_joins();
    test_font_loops();
    test_wide_part_pins();
    test_kind_parts();
    test_render_kept_shapes();
    test_render_tiles();
    test_render_kept_notes();
    test_render_plain_texts();
    test_render_kept_graphs();
    test_render_pictures();
    test_render_frames();
    test_cover_layers();
    test_render_kept_strokes();
    test_render_kept_strokes_timed();
    if(fails == 0) printf("ALL PASSED\n"); else printf("%d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
