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
#include "zoom/props.h"
#include "sim/body.h"
#include "zoom/calc.h"
#include "zoom/plot.h"
#include "zoom/symbol.h"
#include "zoom/display.h"
#include "zoom/select.h"
#include "zoom/render.h"
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
    // A thin one (a true-size ruler zoomed out: its band 12 points): held on it and a little round it, drawn along past that.
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
    line(&m, 2900, 10, 3000, 10, 2.0, 2.0f);
    fude_zoom_nav_mark marks[FUDE_ZOOM_NAV_MARKS];
    u32 n = fude_zoom_nav_marks(&m, half, 30.0f, marks);
    CHECK(n == 2);
    b8 left = false, right = false;
    for(u32 i = 0; i < n; i++) {
        left  = left  || (!marks[i].ring && fabsf(fabsf(marks[i].angle) - 3.14159f) < 0.05f && fabsf(marks[i].at.x + 470.0f) < 1.0f);
        right = right || (!marks[i].ring && fabsf(marks[i].angle) < 0.05f && fabsf(marks[i].at.x - 470.0f) < 1.0f);
    }
    CHECK(left && right);
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
static void walked_stroke(void* self, const fude_zoom_v2* p, const f64* r, u32 n, rde_color color) {
    (void)self; (void)p; (void)r; (void)n;
    if(walked_n < 16u) walked[walked_n++] = color.r;
}
static void walked_shape(void* self, const fude_zoom_v2* p, u32 n, b8 closed, f64 radius, rde_color line_color, rde_color fill) {
    (void)self; (void)p; (void)n; (void)closed; (void)radius; (void)fill;
    if(walked_n < 16u) walked[walked_n++] = line_color.r;
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
    CHECK(fude_zoom_export_tiles(&s, half, 0.1, 210.0, 297.0, (fude_zoom_v2){ 0.0, 0.0 }, &tb, &pages) && pages == 1u);
    rde_arr_add(&tb, &(u8){ 0 });
    CHECK(strstr((const c8*)tb.memory, "/MediaBox [0 0 595.2756 841.8898]") != NULL);   // A4 exactly
    CHECK(strstr((const c8*)tb.memory, "1.020408 0 0 0.980392") == NULL);
    rde_arr_free(&tb);
    // ...and for a printer that prints 2% short across and 2% long down: drawn that much longer across, shorter down.
    tb = fude_bytes_new(4096);
    CHECK(fude_zoom_export_tiles(&s, half, 0.1, 210.0, 297.0, (fude_zoom_v2){ 0.98, 1.02 }, &tb, &pages) && pages == 1u);
    rde_arr_add(&tb, &(u8){ 0 });
    CHECK(strstr((const c8*)tb.memory, "1.020408 0 0 0.980392") != NULL);
    rde_arr_free(&tb);
    // The printer's check: one page, its two lines and their lengths.
    tb = fude_bytes_new(4096);
    CHECK(fude_zoom_export_print_check(210.0, 297.0, &tb));
    rde_arr_add(&tb, &(u8){ 0 });
    const c8* chk = (const c8*)tb.memory;
    CHECK(strstr(chk, "/Count 1") != NULL && strstr(chk, "Printer check") != NULL && strstr(chk, "X  150 mm") != NULL && strstr(chk, "Y  200 mm") != NULL);
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
    const fude_zoom_export_sink rec = { NULL, walked_stroke, walked_shape, NULL, NULL, NULL };
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
    CHECK(fude_zoom_scene_shape_numbers(&s, c, n, FUDE_ZOOM_SHAPE_NUMBERS) == 8u);
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
        CHECK(fude_zoom_scene_shape_numbers(&s, i, n, FUDE_ZOOM_SHAPE_NUMBERS) == 8u);
        fude_zoom_id from, to;
        memcpy(&from, &n[6], sizeof from); memcpy(&to, &n[7], sizeof to);
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
    CHECK(fude_zoom_export_tiles(&s, (fude_zoom_v2){ 500, 400 }, 0.1, 215.9, 279.4, (fude_zoom_v2){ 0, 0 }, &letter, &pages) && pages == 1u);
    rde_arr_add(&letter, &(u8){ 0 });
    CHECK(strstr((const c8*)letter.memory, "/MediaBox [0 0 612 792]") != NULL);
    rde_arr_free(&letter);
    // More than 26 rows (they go by letter): not made, even within 64 pages.
    letter = fude_bytes_new(256);
    CHECK(!fude_zoom_export_tiles(&s, (fude_zoom_v2){ 50, 3610 }, 1.0, 210.0, 297.0, (fude_zoom_v2){ 0, 0 }, &letter, &pages));
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
    CHECK(fude_zoom_export_pdf_pages(&s, &view, &half3, 1u, (fude_zoom_v2){ 0.0, 0.0 }, &pdf));
    rde_arr_add(&pdf, &(u8){ 0 });
    CHECK(strstr((const c8*)pdf.memory, "/MediaBox [0 0 1190.55") != NULL && strstr((const c8*)pdf.memory, " 841.88") != NULL);
    CHECK(memcmp(&s.camera, &was, sizeof(was)) == 0);
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
    fude_zoom_pieces_rename(&again, dir, 1u, "Uno");
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
    // A board sawn across (kerf 3): two plain boards, its name on both, in its place and order; the cut list counts them as one part twice.
    fude_zoom_scene sc; fude_zoom_scene_init(&sc, 7);
    const rde_color ink = { 1, 1, 1, 255 };
    f64 bn[FUDE_ZOOM_SHAPE_NUMBERS];
    const u32 bc = fude_zoom_board_numbers(bn, 300, 150, 18, (u8)(FUDE_ZOOM_MATERIAL_OAK << 1), "Side");
    const u32 bd = fude_zoom_scene_add_shape_fill(&sc, sc.root, (fude_zoom_place){ { 1000, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_BOARD, bn, bc, ink, 0.5f,
                                                  FUDE_ZOOM_FLAG_FILLED | FUDE_ZOOM_FLAG_FILL_OWN, (rde_color){ 226, 202, 160, 255 }, 0);
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
    rde_arr outline = rde_arr_new(sizeof(fude_zoom_v2), NULL);
    b8 oc = false;
    fude_zoom_scene_shape_outline(&sc, notched, 64, &outline, &oc);
    CHECK(oc && rde_arr_length(&outline) == 6u);
    rde_arr_free(&outline); rde_arr_free(&all);
    // The eraser across a board: two pieces; undone, the board back.
    fude_zoom_scene_set_alive(&sc, left, false);
    fude_zoom_history_push(&sc, sc.root, (fude_zoom_box){ 0, 0, 1, 1 }, &left, 1, &notched, 1);
    const u32 other = fude_zoom_scene_add_shape_fill(&sc, sc.root, (fude_zoom_place){ { 3000, 0 }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_BOARD, bn, bc, ink, 0.5f, FUDE_ZOOM_FLAG_FILLED, ink, 0);
    fude_zoom_history_push(&sc, sc.root, (fude_zoom_box){ 0, 0, 1, 1 }, NULL, 0, &other, 1);
    const u32 before = fude_zoom_scene_object_count(&sc);
    fude_zoom_eraser er; fude_zoom_eraser_init(&er);
    fude_zoom_erase_begin(&er, FUDE_ZOOM_ERASE_PARTIAL);
    CHECK(fude_zoom_erase_step(&sc, &er, sc.root, (fude_zoom_v2){ 3000, -300 }, (fude_zoom_v2){ 3000, 300 }, 2.0, 0.5) >= 1u);
    fude_zoom_erase_end(&sc, &er, sc.root, (fude_zoom_box){ 2600, -300, 3400, 300 });
    u32 boards_after = 0;
    for(u32 i = before; i < fude_zoom_scene_object_count(&sc); i++) {
        const fude_zoom_object* o = fude_zoom_scene_object(&sc, i);
        boards_after += (o->flags & FUDE_ZOOM_FLAG_ALIVE) && o->kind == FUDE_ZOOM_KIND_SHAPE && o->channels == FUDE_ZOOM_SHAPE_BOARD ? 1u : 0u;
    }
    CHECK(!alive(&sc, other) && boards_after == 2u);
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
    fude_zoom_id joined;
    memcpy(&joined, &n[4], sizeof(joined));
    CHECK(joined == fude_zoom_scene_object(&s, base)->id);
    fude_zoom_select_clips_free(&clips);
    fude_zoom_select_clips_of(&s, &d2, 1u, &clips);
    fude_zoom_select_put(&sel, &s, (const fude_zoom_clip*)clips.memory, 1u, &up, 1u, NULL, NULL, 0u);
    fude_zoom_scene_shape_numbers(&s, base + 2u, n, FUDE_ZOOM_SHAPE_NUMBERS);
    memcpy(&joined, &n[4], sizeof(joined));
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
    CHECK(fude_zoom_export_stl(&s, (fude_zoom_v2){ 400, 300 }, 1.0, 3.0, &stl, &said));
    CHECK(said.solids == 2u && rde_arr_length(&stl) == 84u + 50u * said.triangles);
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
    fude_zoom_v2 p0, p1, r[6];
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
    // Into a breadboard's hole: straight in, never past it and back (a hole has no side: FUDE_ZOOM_PIN_ANY).
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 bb = part_put(&s, "breadboard", 0, 0, 200, 120, "");
        const u32 r  = part_put(&s, "resistor", -400, -300, 30, 10, "1k");
        fude_zoom_v2 hole, pin, way[6];
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
        fude_zoom_v2 h1, h7, b1, b7;
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
    // Variables: y = a·x with a from 1 to 3; not playing, a is 1; a second and a half in, 2 (half way); a slider's, its.
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

// Mechanisms: what hinges where, a motor's speed, gears that touch, a spring's ends; gear trains' speeds.
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
    CHECK(fabs(m[0].ratio - 0.5) < 1e-12);
    // The 20T turning at 2: the 40T at -1.
    const u32 nb = (u32)rde_arr_length(&p.bodies);
    f64 omega[16] = { 0 };
    b8 driven[16] = { 0 };
    omega[m[0].a] = 2.0;
    driven[m[0].a] = true;
    fude_zoom_mech_gears(&p, omega, driven);
    CHECK(nb <= 16u && fabs(omega[m[0].b] + 1.0) < 1e-12);
    // A spring from the pivot's hole to a weight's: held by the ground at one end, the weight at the other.
    fude_zoom_scene t; fude_zoom_scene_init(&t, 7);
    part_put(&t, "fixed pivot", 0, -10, 20, 20, "");
    part_put(&t, "spring", 0 + 45, 0, 50, 12, "");        // (its ends at 0 and 90)
    part_put(&t, "weight", 90, 0, 20, 20, "2 kg");
    fude_zoom_mech_plan_build(&p, &t, NULL);
    CHECK(rde_arr_length(&p.springs) == 1u);
    const fude_zoom_mech_spring* sp = (const fude_zoom_mech_spring*)p.springs.memory;
    CHECK(sp[0].a == FUDE_ZOOM_NONE && sp[0].b != FUDE_ZOOM_NONE && fabs(sp[0].length - 90.0) < 1e-4);
    CHECK(fabs(((const fude_zoom_mech_body*)p.bodies.memory)[sp[0].b].value - 2.0) < 1e-12);
    // A pulley (its rope's groove 39 round), a rope down each side of it: a 1 kg weight on one, a 2 kg crate on the other.
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
    // Run as an Atwood machine (point masses, gravity 9.81, a 240th of a second a step): the crate goes down at g/3.
    fude_zoom_v2 at[2] = { { -39.0, -195.0 }, { 39.0, -195.0 } }, v[2] = { { 0.0, 0.0 }, { 0.0, 0.0 } };
    const fude_zoom_v2 rim[2] = { { -39.0, -5.0 }, { 39.0, -5.0 } };
    const f64 mass[2] = { 1.0, 2.0 }, dt = 1.0 / 240.0, total = 380.0;
    for(u32 step = 0; step < 240u; step++) {
        fude_zoom_mech_pull pull[2];
        f64 len = 0.0;
        for(u32 k = 0; k < 2u; k++) {
            v[k].y -= 9.81 * dt;
            at[k].x += v[k].x * dt;
            at[k].y += v[k].y * dt;
            const f64 d = hypot(at[k].x - rim[k].x, at[k].y - rim[k].y);
            len += d;
            pull[k] = (fude_zoom_mech_pull){ v[k], { (at[k].x - rim[k].x) / d, (at[k].y - rim[k].y) / d }, 1.0 / mass[k], { 0, 0 }, { 0, 0 } };
        }
        fude_zoom_mech_rope_keep(pull, 2u, len - total);
        for(u32 k = 0; k < 2u; k++) {
            v[k].x += pull[k].dv.x; v[k].y += pull[k].dv.y;
            at[k].x += pull[k].dp.x; at[k].y += pull[k].dp.y;
        }
    }
    CHECK(fabs(v[1].y + 9.81 / 3.0) < 0.05 && fabs(v[0].y - 9.81 / 3.0) < 0.05);   // (after a second: g/3 down, g/3 up)
    CHECK(fabs(hypot(at[0].x - rim[0].x, at[0].y - rim[0].y) + hypot(at[1].x - rim[1].x, at[1].y - rim[1].y) - total) < 1e-6);
    // A slider on a rail: along the rail, as far as its ends; a 20-tooth gear (pitch radius 50) over a rack: meshed, the
    // rack going on 50 a radian the gear turns anticlockwise (its turn − travel / 50 kept).
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
    // A slack rope does nothing.
    fude_zoom_mech_pull one = { { 0.0, -1.0 }, { 0.0, -1.0 }, 1.0, { 0, 0 }, { 0, 0 } };
    fude_zoom_mech_rope_keep(&one, 1u, -5.0);
    CHECK(one.dv.y == 0.0 && one.dp.y == 0.0);
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
    // Random shapes drawn (stars of 3 to 60 corners, anywhere, any size): one body each, its pieces convex, at most eight
    // corners, as many as a body holds, their area within 3 % of the drawing's, its centre near the drawing's.
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
    // Make part, as a person does it: a full adder drawn of gates (inputs and probes named A, B, CI, S, CO) made a part;
    // four of it placed and joined into a 4-bit adder, drawn; that made a part too (a part of parts, made on the
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
    // Borja's tablet circuit: A through a NOT into an AND; A and B into an OR (A's wire branching off its wire to the NOT,
    // a junction); the OR into the AND (branching off the OR's wire on) and into a last OR with the AND: A or B.
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
        // (from the OR's first input up to a point along A's wire; from the AND's second input down to the OR's wire on)
        fude_zoom_v2 pa[FUDE_ZOOM_WIRE_POINTS], po[FUDE_ZOOM_WIRE_POINTS], q0, q1;
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
        fude_zoom_v2 pin, q0;
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
        // (a branch off w2 ending on it — a junction — and a branch off that branch; a branch off w3 that also ends on w4)
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

// --- the examples (Insert → Examples): each drawn as the app draws it, run as Play runs it ------------------------------

// The parts of example e's drawing that are id (in the order drawn; with text: only those saying it): into out. How many.
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
    const fude_sim_def* old_def = fude_zoom_logic_learn(old_text, strlen(old_text), err, sizeof err);
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
    CHECK(fude_zoom_logic_find("Guardian") == old_def && fude_zoom_logic_find("Keeper") == old_def && fude_zoom_logic_find("user/keeper") == old_def);
    c8 slug[40];
    fude_zoom_logic_slug("  Half adder #2 (v3) ", slug, sizeof slug);
    CHECK(strcmp(slug, "half-adder-2-v3") == 0);
    fude_zoom_logic_slug("¿?", slug, sizeof slug);
    CHECK(strcmp(slug, "part") == 0);
    // Forgotten: found by neither; a chip is not a person's to forget.
    CHECK(fude_zoom_logic_forget_part("user/keeper") && !fude_zoom_logic_forget_part("user/keeper"));
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
// voltage exactly where it should be —, a 30 × 30 grid of them, a battery lighting an LED with no ground of its own on a
// canvas whose ground is elsewhere (its minus held at 0 V). Solved sparse, quickly, Kirchhoff kept at every node.
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
        CHECK(c.blocks == 2u);
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

// The examples made to show what goes wrong do just that, and nothing else: the LED with no resistor burnt (its current),
// the one behind 470 Ω lit; the ¼ W resistor burnt after a moment, the 1 W one not; the lamps lit until a switch shorts
// them — behind the fuse it blows (nothing said: it took the short), with none the battery says so and the switch burns;
// the backwards electrolytic and the 6.3 V one popped (the wrong way round, past its volts), the 16 V one well; the BC547
// burnt, the TIP120's lamp lit. And the LC rings: twice its supply and back at 1.6 Hz, still swinging seconds later.
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

// DISPLAYS (display.h). A one-pin part (a rail of _volts; NULL: a ground) beside pin _pa of _a, _dx, _dy from it, a wire
// straight between them.
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

// The display examples (examples.h), played: the digit counting 0–9 then blank; the matrix's rows their numbers; the bar
// six high; the panel meters' readings; the LCD on and an H written by its button.
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
}

static void test_display_sizes(void) {
    printf("display sizes\n");
    const u32 panel = fude_zoom_symbol_find("7-segment panel"), matrix = fude_zoom_symbol_find("LED matrix"), lcd = fude_zoom_symbol_find("character LCD");
    CHECK(panel != FUDE_ZOOM_NONE && matrix != FUDE_ZOOM_NONE && lcd != FUDE_ZOOM_NONE);
    // A panel: its digits as its text says (none: 4; past 8: 8), its commons anodes when it says "CA"; its segments' pins
    // down its left, a common under each digit, 20 apart.
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
    // A matrix: columns × rows ("5x7": 5 across, 7 down; up to 32 each), its rows' pins down its left, its columns' along
    // its bottom; its rows anodes, cathodes when it says "CC".
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
    // A 4-digit panel (common cathodes): segments b and c from 5 V through 330 Ω each, digit 2's common grounded — a 1 on
    // it, nothing on the others.
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
    // Its limits LED by LED: segment a through 82 Ω with all four commons grounded — 40 mA shared, 10 mA each: well; with
    // one common, all 40 mA in one LED: burnt.
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

// Panel meters: a 20 V one on 9 V shows 9.00; a 200 mA one in a row with 100 Ω on 5 V, 49.5 (its shunt 1 Ω); a 2 V one on
// 9 V, a 1 alone. What it shows of a reading, its 3½ digits.
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
    // On its pins: VSS, RW, K grounded; VDD, A on 5 V; RS, E, D0–D7 logic inputs — 0x38, 0x0C, 0x01, "Hi" written with E.
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

int main(void) {
    test_codec();
    test_sim_and_index();
    test_frames();
    test_erasers();
    test_instruments();
    test_texts();
    test_layers();
    test_connectors();
    test_layer_moves();
    test_driving();
    test_boards();
    test_paths();
    test_arcs();
    test_video_yuv();
    test_hand_lines();
    test_symbols();
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
    test_display_examples();
    test_examples_go_wrong();
    test_logic_draw();
    test_custom();
    test_examples_electronics();
    test_mechanisms();
    test_calc();
    test_drawn_bodies();
    test_plot();
    test_repeat();
    test_dims();
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
    test_file();
    if(fails == 0) printf("ALL PASSED\n"); else printf("%d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
