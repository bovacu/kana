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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static int fails = 0;
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

// A straight stroke from a to b in the camera frame, points every `step`, as the page draws one.
static u32 line(fude_zoom_scene* s, f64 ax, f64 ay, f64 bx, f64 by, f64 step, f32 radius) {
    const u32 frame = s->camera.frame;
    const i8  q     = fude_zoom_quantum_for(s->camera.z);
    const f64 g     = ldexp(1.0, q);
    const f64 len   = hypot(bx - ax, by - ay);
    const u32 n     = (u32)(len / step) + 1u;
    fude_zoom_qpoint* pts = calloc(n + 1, sizeof(*pts));
    for(u32 i = 0; i < n; i++) {
        const f64 t = n > 1 ? (f64)i / (f64)(n - 1) : 0.0;
        pts[i].x = (i32)llround((bx - ax) * t / g); pts[i].y = (i32)llround((by - ay) * t / g);
        pts[i].pressure = 600; pts[i].time = i * 4u;
    }
    const u32 o = fude_zoom_scene_add_stroke(s, frame, (fude_zoom_v2){ ax, ay }, q, pts, n, FUDE_ZOOM_CHANNEL_PRESSURE | FUDE_ZOOM_CHANNEL_TIME,
                                             (rde_color){ 10, 20, 30, 255 }, radius, FUDE_ZOOM_FLAG_FROM_PEN, 0, 0);
    free(pts);
    fude_zoom_history_push(s, frame, fude_zoom_scene_object(s, o)->box, NULL, 0, &o, 1);
    return o;
}

static b8 alive(const fude_zoom_scene* s, u32 o) { return (fude_zoom_scene_object(s, o)->flags & FUDE_ZOOM_FLAG_ALIVE) != 0; }

// The alive strokes' ends, frame units.
static void ends(const fude_zoom_scene* s, u32 o, fude_zoom_v2* a, fude_zoom_v2* b) {
    const fude_zoom_object* ob = fude_zoom_scene_object(s, o);
    fude_zoom_qpoint* q = calloc(ob->count, sizeof(*q));
    fude_zoom_scene_points(s, o, q);
    *a = fude_zoom_scene_point_at(ob, &q[0]); *b = fude_zoom_scene_point_at(ob, &q[ob->count - 1]);
    free(q);
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
        fude_zoom_qpoint* q = calloc(o->count, sizeof(*q)); fude_zoom_v2* p = calloc(o->count, sizeof(*p)); u32* r = calloc(o->count, sizeof(u32));
        fude_zoom_scene_points(s, i, q);
        for(u32 k = 0; k < o->count; k++) { p[k] = fude_zoom_scene_point_at(o, &q[k]); r[k] = q[k].time; }
        in = fude_zoom_fill_inside_rings(p, r, o->count, (fude_zoom_v2){ x, y });
        free(q); free(p); free(r);
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
        fude_zoom_qpoint* cq = calloc(co->count, sizeof(*cq)); fude_zoom_v2* cp = calloc(co->count, sizeof(*cp)); u32* cr = calloc(co->count, sizeof(u32));
        fude_zoom_scene_points(&s, cutf, cq);
        for(u32 i = 0; i < co->count; i++) { cp[i] = fude_zoom_scene_point_at(co, &cq[i]); cr[i] = cq[i].time; }
        rde_arr ct = rde_arr_new(sizeof(fude_zoom_v2), NULL);
        fude_zoom_fill_triangulate_rings(cp, cr, co->count, &ct);
        const f64 circle = 0.5 * 64 * 100 * 100 * sin(6.283185307 / 64);
        CHECK(fabs(tri_area(&ct) - (circle - 20.0 * 200.0)) < circle * 0.02);   // less a band 20 wide across it (a little more at its rims)
        CHECK(!fude_zoom_fill_inside_rings(cp, cr, co->count, (fude_zoom_v2){ 0, 0 }) && fude_zoom_fill_inside_rings(cp, cr, co->count, (fude_zoom_v2){ 0, 50 }));
        rde_arr_free(&ct); free(cq); free(cp); free(cr);
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
                fude_zoom_qpoint* q = calloc(o->count, sizeof(*q)); fude_zoom_v2* p = calloc(o->count, sizeof(*p)); u32* r = calloc(o->count, sizeof(u32));
                fude_zoom_scene_points(&ps, i, q);
                for(u32 j = 0; j < o->count; j++) { p[j] = fude_zoom_scene_point_at(o, &q[j]); r[j] = q[j].time; }
                rde_arr_clear(&tris);
                fude_zoom_fill_triangulate_rings(p, r, o->count, &tris);
                free(q); free(p); free(r);
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
            fude_zoom_qpoint* q = calloc(o->count, sizeof(*q)); fude_zoom_v2* p = calloc(o->count, sizeof(*p)); u32* r = calloc(o->count, sizeof(u32));
            fude_zoom_scene_points(&ts, tf, q);
            for(u32 i = 0; i < o->count; i++) { p[i] = fude_zoom_scene_point_at(o, &q[i]); r[i] = q[i].time; }
            CHECK(!fude_zoom_fill_inside_rings(p, r, o->count, (fude_zoom_v2){ 0, 18 }));    // the notch
            CHECK(fude_zoom_fill_inside_rings(p, r, o->count, (fude_zoom_v2){ 0, -15 }));    // its other side
            CHECK(fude_zoom_fill_inside_rings(p, r, o->count, (fude_zoom_v2){ 50, 18 }));    // and along
            CHECK(fude_zoom_fill_inside_rings(p, r, o->count, (fude_zoom_v2){ -110, 0 }) && !fude_zoom_fill_inside_rings(p, r, o->count, (fude_zoom_v2){ 0, 25 }));   // its round end; not past its side
            free(q); free(p); free(r);
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

static u8* read_all(const char* p, u32* n) {
    FILE* f = fopen(p, "rb"); if(!f) { *n = 0; return NULL; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    u8* d = malloc((size_t)sz + 1); *n = (u32)fread(d, 1, (size_t)sz, f); fclose(f); return d;
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
            fude_zoom_qpoint* qa = calloc(o->count, sizeof(*qa)); fude_zoom_qpoint* qb = calloc(o->count, sizeof(*qb));
            fude_zoom_scene_points(&s, i, qa); fude_zoom_scene_points(&t, j, qb);
            if(p->count != o->count || memcmp(qa, qb, o->count * sizeof(*qa)) || p->t.x != o->t.x || p->t.y != o->t.y || p->q != o->q) same = false;
            free(qa); free(qb);
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
    u32 total; u8* whole = read_all(path, &total);
    CHECK(total == f.size);
    printf("  file: %u bytes, every cut tried\n", total);
    const char* cut = "./saves/cut.zoom";
    b8 all_ok = true;
    for(u32 len = 0; len <= total; len++) {
        remove(cut); remove("./saves/cut.zoom.bak"); remove("./saves/cut.zoom.bad"); remove("./saves/cut.zoom.tmp");
        write_all(cut, whole, len);
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
        u8* bad = malloc(total); memcpy(bad, whole, total);
        bad[total - 6] ^= 0x5A;
        remove(cut); write_all(cut, bad, total);
        fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
        CHECK(fude_zoom_file_open(&g, cut, &t) == FUDE_LOAD_OK);
        i32 want = -1;
        for(u32 k = 0; k < nsaved; k++) if(saved[k].size < total) want = (i32)k;
        CHECK(want >= 0 && signature(&t) == saved[want].sig);
        fude_zoom_scene_destroy(&t);
        free(bad);
    }
    // Not a canvas at all: kept as .bad, nothing loaded.
    {
        remove(cut); remove("./saves/cut.zoom.bak"); write_all(cut, (const u8*)"KANA garbage that is not a canvas", 33);
        fude_zoom_scene t; fude_zoom_scene_init(&t, 7); fude_zoom_file g;
        CHECK(fude_zoom_file_open(&g, cut, &t) == FUDE_LOAD_CORRUPT);
        struct stat st; CHECK(stat("./saves/cut.zoom.bad", &st) == 0);
        fude_zoom_scene_destroy(&t);
    }
    free(whole);

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

int main(void) {
    test_codec();
    test_sim_and_index();
    test_frames();
    test_erasers();
    test_history();
    test_moves();
    test_shapes();
    test_pictures();
    test_smoothing();
    test_navigation();
    test_filling();
    test_export();
    test_file();
    if(fails == 0) printf("ALL PASSED\n"); else printf("%d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
