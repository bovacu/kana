// Mechanisms' physics, hard: RDE's 2D physics (Box2D's, and RDE's own couplings over it) — gears in pairs and trains,
// a rack and its pinion, pulleys — run as Play runs them, against what they must do: exact ratios held over minutes,
// no drift, energy kept where nothing takes it, a rack that runs out of teeth letting its gear go on, a rope never
// longer than it is. Then Sketching's own mechanisms (mech.h's plan, mechrun.h's world) the same way.

#include "rde.h"
#include "zoom/mech.h"
#include "zoom/mechrun.h"
#include "zoom/examples.h"
#include "zoom/circuit.h"
#include "zoom/coupling.h"
#include "zoom/placer.h"
#include <time.h>
#include "zoom/scene.h"
#include "zoom/shape.h"
#include "zoom/symbol.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)
// (a measure and what it may be, said when it is not)
#define WITHIN(_what, _v, _most) do { const double _x = (double)(_v); if(!(_x <= (double)(_most))) { printf("FAIL line %d: %s = %g (at most %g)\n", __LINE__, _what, _x, (double)(_most)); fails++; } } while(0)

#define DT (1.0f / 240.0f)
#define PI 3.14159265358979323846

// --- a little world ---------------------------------------------------------------------------------------------

typedef struct {
    rde_physics_2d_body*  body;
    rde_physics_2d_joint* hinge;
    double                r, mass, turned;   // (turned: its angle, unwrapped)
    float                 last;
} gear;

static unsigned sub_steps = 8u;   // (Sketching's mechanisms': mechrun.c)

static rde_physics_2d_world* world_new(float g) {
    rde_physics_2d_world* w = rde_physics_2d_world_create((rde_vec_2F){ 0.0f, -g }, 256u, NULL);
    rde_physics_2d_world_set_sub_steps(w, sub_steps);
    return w;
}

static rde_physics_2d_body* ground_new(rde_physics_2d_world* w) {
    rde_physics_2d_shape_def dot;
    memset(&dot, 0, sizeof(dot));
    dot.type = RDE_PHYSICS_2D_SHAPE_CIRCLE;
    dot.circle.radius = 0.01f;
    dot.is_sensor = true;
    return rde_physics_2d_body_create(w, RDE_PHYSICS_2D_BODY_TYPE_STATIC, (rde_vec_2F){ 0.0f, 0.0f }, 0.0f, &dot, 0.0f);
}

// A gear: a disc of radius r (its pitch circle), mass m, on an axle to the ground at (x, y); it touches nothing.
static gear gear_new(rde_physics_2d_world* w, rde_physics_2d_body* ground, double x, double y, double r, double m) {
    rde_physics_2d_shape_def s;
    memset(&s, 0, sizeof(s));
    s.type = RDE_PHYSICS_2D_SHAPE_CIRCLE;
    s.circle.radius = (float)r;
    s.layer = 1u;
    s.layer_mask = 8u;   // (nothing of these tests is on layer 8: gears pass through each other, as meshed ones do)
    gear g = { 0 };
    g.body  = rde_physics_2d_body_create(w, RDE_PHYSICS_2D_BODY_TYPE_DYNAMIC, (rde_vec_2F){ (float)x, (float)y }, 0.0f, &s, (float)m);
    g.hinge = rde_physics_2d_joint_create_hinge(w, g.body, (rde_vec_2F){ 0.0f, 0.0f }, ground, (rde_vec_2F){ (float)x, (float)y });
    g.r = r;
    g.mass = m;
    return g;
}

static void track(gear* g) {
    const float a = rde_physics_2d_body_get_angle(g->body);
    double d = (double)a - (double)g->last;
    while(d > PI) d -= 2.0 * PI;
    while(d < -PI) d += 2.0 * PI;
    g->turned += d;
    g->last = a;
}

static void run(rde_physics_2d_world* w, gear* gs, unsigned n, double seconds) {
    const unsigned steps = (unsigned)lround(seconds / (double)DT);
    for(unsigned k = 0; k < steps; k++) {
        rde_physics_2d_world_step(w, DT);
        for(unsigned i = 0; i < n; i++) track(&gs[i]);
    }
}

// Gears i and i + 1 meshed, all along (each the other way round: θ₁ r₁ = −θ₂ r₂).
static void chain(rde_physics_2d_world* w, gear* gs, unsigned n) {
    for(unsigned i = 0; i + 1u < n; i++) {
        rde_physics_2d_joint_create_gear(w, gs[i].hinge, gs[i + 1u].hinge, (float)(gs[i + 1u].r / gs[i].r));
    }
}

// How far gear i is from where gear 0 says it must be (radians, the worst), and its speed's (a fraction, the worst).
static double chain_error(const gear* gs, unsigned n) {
    double worst = 0.0;
    for(unsigned i = 1; i < n; i++) {
        const double want = ((i & 1u) ? -1.0 : 1.0) * gs[0].r / gs[i].r * gs[0].turned;
        worst = fmax(worst, fabs(gs[i].turned - want));
    }
    return worst;
}
static double speed_error(const gear* gs, unsigned n) {
    const double w0 = (double)rde_physics_2d_body_get_angular_velocity(gs[0].body);
    double worst = 0.0;
    for(unsigned i = 1; i < n; i++) {
        const double want = ((i & 1u) ? -1.0 : 1.0) * gs[0].r / gs[i].r * w0;
        worst = fmax(worst, fabs((double)rde_physics_2d_body_get_angular_velocity(gs[i].body) - want) / fmax(fabs(want), 1e-9));
    }
    return worst;
}

static double inertia(const gear* g) { return 0.5 * g->mass * g->r * g->r; }

// --- gears -------------------------------------------------------------------------------------------------------

// Two gears, the small one driven: the big one at half its speed, the other way, from the first step to the last.
static void test_pair(void) {
    rde_physics_2d_world* w = world_new(9.81f);
    rde_physics_2d_body* ground = ground_new(w);
    gear g[2] = { gear_new(w, ground, 0, 0, 1.0, 1.0), gear_new(w, ground, 3, 0, 2.0, 4.0) };
    chain(w, g, 2u);
    rde_physics_2d_joint_enable_motor(g[0].hinge, 2.0f, 1e9f);
    double worst = 0.0;
    for(int s = 0; s < 50; s++) {
        run(w, g, 2u, 0.1);
        worst = fmax(worst, chain_error(g, 2u));
    }
    WITHIN("pair: angle error (rad)", worst, 2e-3);
    WITHIN("pair: speed error", speed_error(g, 2u), 5e-3);
    WITHIN("pair: driven turn off 10 rad", fabs(fabs(g[0].turned) - 10.0), 0.1);
    rde_physics_2d_world_destroy(w);
}

// A train of six, of four sizes, driven from one end (then the other), each with a load against it or not; at Box2D's
// 4 sub-steps and Sketching's 8: every gear where the first says, at its speed, all along. Light gears meshed with
// heavy ones (1:100): the same once the motor's start has gone through (at most a quarter of a 30-tooth gear's tooth
// at its start). 1:1000 (a light gear between heavy ones: an iterative solver's worst): settled within twenty seconds.
static void train_case(const char* name, int from_end, int loaded, double light, double heavy, double settle, double start_most, double most, double speed_most) {
    static const double radii[6] = { 1.0, 1.5, 0.5, 2.0, 1.0, 0.75 };
    rde_physics_2d_world* w = world_new(9.81f);
    rde_physics_2d_body* ground = ground_new(w);
    gear g[6];
    double x = 0.0;
    for(unsigned i = 0; i < 6u; i++) {
        const double m = light > 0.0 ? ((i & 1u) ? heavy : light) : 2.0 * radii[i] * radii[i];
        if(i > 0u) x += radii[i - 1u] + radii[i];
        g[i] = gear_new(w, ground, x, 0.0, radii[i], m);
    }
    chain(w, g, 6u);
    gear* driven = from_end ? &g[5] : &g[0];
    rde_physics_2d_joint_enable_motor(driven->hinge, 3.0f, 1e9f);
    double start = 0.0, worst = 0.0, worst_speed = 0.0;
    for(int s = 0; s < 200; s++) {
        if(loaded) {
            // (a brake on the far gear: a torque against its turning, as a load it drives)
            gear* far = from_end ? &g[0] : &g[5];
            for(int k = 0; k < 24; k++) {
                const float wv = rde_physics_2d_body_get_angular_velocity(far->body);
                rde_physics_2d_body_apply_torque(far->body, wv > 0.0f ? -50.0f : 50.0f);
                rde_physics_2d_world_step(w, DT);
                for(unsigned i = 0; i < 6u; i++) track(&g[i]);
            }
        } else {
            run(w, g, 6u, 0.1);
        }
        if(0.1 * (s + 1) <= settle) {
            start = fmax(start, chain_error(g, 6u));
        } else {
            worst = fmax(worst, chain_error(g, 6u));
            worst_speed = fmax(worst_speed, speed_error(g, 6u));
        }
    }
    char what[128];
    snprintf(what, sizeof(what), "%s: angle error at its start (rad)", name);
    WITHIN(what, start, start_most);
    snprintf(what, sizeof(what), "%s: angle error (rad)", name);
    WITHIN(what, worst, most);
    snprintf(what, sizeof(what), "%s: speed error", name);
    WITHIN(what, worst_speed, speed_most);
    snprintf(what, sizeof(what), "%s: the driven one's speed off 3 rad/s", name);
    WITHIN(what, fabs(fabs((double)rde_physics_2d_body_get_angular_velocity(driven->body)) - 3.0), 0.03);
    rde_physics_2d_world_destroy(w);
}

static void test_trains(void) {
    static const char* const names[4] = { "train", "train from its end", "train loaded", "train from its end, loaded" };
    const unsigned were = sub_steps;
    for(unsigned pass = 0; pass < 2u; pass++) {
        sub_steps = pass == 0u ? 4u : 8u;
        for(int c = 0; c < 4; c++) {
            char name[64];
            snprintf(name, sizeof(name), "%s (%u sub-steps)", names[c], sub_steps);
            train_case(name, c & 1, (c & 2) != 0, 0.0, 0.0, 1.0, 5e-3, 5e-3, 1e-2);
        }
    }
    sub_steps = were;
    train_case("train 1:100", 0, 0, 0.1, 10.0, 1.0, 0.05, 5e-3, 1e-2);
    train_case("train 1:100 from its end", 1, 0, 0.1, 10.0, 1.0, 0.05, 5e-3, 1e-2);
    train_case("train 1:1000", 0, 0, 0.1, 100.0, 15.0, 1.0, 5e-3, 2e-2);
    train_case("train 1:1000 from its end", 1, 0, 0.1, 100.0, 15.0, 1.0, 5e-3, 2e-2);
}

// Nothing driving, nothing braking: one gear set spinning, the train takes its share at once (meshing), then keeps the
// energy it has for ten seconds; and turned back by a motor the other way, back where it started.
static void test_energy_and_reverse(void) {
    {
        rde_physics_2d_world* w = world_new(0.0f);
        rde_physics_2d_body* ground = ground_new(w);
        gear g[4] = { gear_new(w, ground, 0, 0, 1.0, 1.0), gear_new(w, ground, 2.5, 0, 1.5, 3.0), gear_new(w, ground, 4.5, 0, 0.5, 0.5),
                      gear_new(w, ground, 6.5, 0, 1.5, 2.0) };
        chain(w, g, 4u);
        rde_physics_2d_body_set_angular_velocity(g[0].body, 5.0f);
        run(w, g, 4u, 0.2);
        double e0 = 0.0;
        for(unsigned i = 0; i < 4u; i++) {
            const double om = rde_physics_2d_body_get_angular_velocity(g[i].body);
            e0 += 0.5 * inertia(&g[i]) * om * om;
        }
        double worst = 0.0, angle = 0.0;
        for(int s = 0; s < 100; s++) {
            run(w, g, 4u, 0.1);
            double e = 0.0;
            for(unsigned i = 0; i < 4u; i++) {
                const double om = rde_physics_2d_body_get_angular_velocity(g[i].body);
                e += 0.5 * inertia(&g[i]) * om * om;
            }
            worst = fmax(worst, fabs(e - e0) / e0);
            angle = fmax(angle, chain_error(g, 4u));
        }
        CHECK(e0 > 0.1);
        WITHIN("free train: energy change", worst, 1e-2);
        WITHIN("free train: angle error (rad)", angle, 5e-3);
        rde_physics_2d_world_destroy(w);
    }
    {
        rde_physics_2d_world* w = world_new(9.81f);
        rde_physics_2d_body* ground = ground_new(w);
        gear g[3] = { gear_new(w, ground, 0, 0, 1.0, 1.0), gear_new(w, ground, 3, 0, 2.0, 4.0), gear_new(w, ground, 5.5, 0, 0.5, 0.25) };
        chain(w, g, 3u);
        rde_physics_2d_joint_enable_motor(g[0].hinge, 3.0f, 1e9f);
        run(w, g, 3u, 5.0);
        const double out = chain_error(g, 3u);
        rde_physics_2d_joint_set_motor_speed(g[0].hinge, -3.0f);
        run(w, g, 3u, 5.0);
        WITHIN("reversed: angle error out (rad)", out, 5e-3);
        WITHIN("reversed: angle error back (rad)", chain_error(g, 3u), 5e-3);
        WITHIN("reversed: not back where it started (rad)", fabs(g[0].turned), 0.1);
        rde_physics_2d_world_destroy(w);
    }
}

// A minute at 10 rad/s (some 95 turns): never more than 5 mrad off.
static void test_long_run(void) {
    rde_physics_2d_world* w = world_new(9.81f);
    rde_physics_2d_body* ground = ground_new(w);
    gear g[3] = { gear_new(w, ground, 0, 0, 1.0, 1.0), gear_new(w, ground, 2, 0, 1.0, 1.0), gear_new(w, ground, 4, 0, 1.0, 1.0) };
    chain(w, g, 3u);
    rde_physics_2d_joint_enable_motor(g[0].hinge, 10.0f, 1e9f);
    double worst = 0.0;
    for(int s = 0; s < 600; s++) {
        run(w, g, 3u, 0.1);
        worst = fmax(worst, chain_error(g, 3u));
    }
    WITHIN("a minute: angle error (rad)", worst, 5e-3);
    WITHIN("a minute: turned off 600 rad", fabs(fabs(g[0].turned) - 600.0), 6.0);
    rde_physics_2d_world_destroy(w);
}

// --- a rack and its pinion ----------------------------------------------------------------------------------------

typedef struct {
    rde_physics_2d_body*  body;
    rde_physics_2d_joint* slide;
} rack;

// A rack (a bar 2·half long) sliding along x (from x0, as far as lower..upper), its pitch line at y.
static rack rack_new(rde_physics_2d_world* w, rde_physics_2d_body* ground, double x0, double y, double half, double m, double lower, double upper, b8 upright) {
    rde_physics_2d_shape_def s;
    memset(&s, 0, sizeof(s));
    s.type = RDE_PHYSICS_2D_SHAPE_BOX;
    s.box.half_extents = upright ? (rde_vec_2F){ 0.2f, (float)half } : (rde_vec_2F){ (float)half, 0.2f };
    s.layer = 1u;
    s.layer_mask = 8u;
    rack r;
    const rde_vec_2F at = upright ? (rde_vec_2F){ (float)y, (float)x0 } : (rde_vec_2F){ (float)x0, (float)y };
    r.body  = rde_physics_2d_body_create(w, RDE_PHYSICS_2D_BODY_TYPE_DYNAMIC, at, 0.0f, &s, (float)m);
    r.slide = rde_physics_2d_joint_create_slider(w, ground, at, r.body, (rde_vec_2F){ 0.0f, 0.0f }, upright ? (rde_vec_2F){ 0.0f, 1.0f } : (rde_vec_2F){ 1.0f, 0.0f },
                                                (float)lower, (float)upper);
    return r;
}

// A gear over a rack: the rack goes on as far as the gear's rim does where they touch, all along. (A motor's speed is
// the ground's turn against its gear's, RDE's hinge's way: 1 turns the gear clockwise.)
static void test_rack(void) {
    rde_physics_2d_world* w = world_new(9.81f);
    rde_physics_2d_body* ground = ground_new(w);
    gear g = gear_new(w, ground, 0, 1.0, 1.0, 1.0);
    rack r = rack_new(w, ground, 0.0, 0.0, 10.0, 5.0, -8.0, 8.0, false);
    rde_physics_2d_joint_create_gear(w, g.hinge, r.slide, -1.0f);   // (θ − x / r kept: r = 1)
    rde_physics_2d_joint_enable_motor(g.hinge, 1.0f, 1e9f);
    double worst = 0.0;
    for(int s = 0; s < 30; s++) {
        run(w, &g, 1u, 0.1);
        worst = fmax(worst, fabs(rde_physics_2d_joint_get_coordinate(r.slide) - g.turned));
    }
    WITHIN("rack: travel error", worst, 2e-3);
    WITHIN("rack: its speed off its gear's rim's", fabs(rde_physics_2d_body_get_velocity(r.body).x - rde_physics_2d_body_get_angular_velocity(g.body)), 1e-2);
    WITHIN("rack: its speed off 1", fabs(fabs(rde_physics_2d_body_get_velocity(r.body).x) - 1.0), 1e-2);
    rde_physics_2d_world_destroy(w);
}

// The rack runs out of teeth (its travel at an end): it stops there, and its gear — the motor's — goes on as fast; the
// motor turned back, the rack is taken back in, meshed again a whole tooth from where it was (its teeth in its gaps).
static void test_rack_end(void) {
    rde_physics_2d_world* w = world_new(9.81f);
    rde_physics_2d_body* ground = ground_new(w);
    gear g[2] = { gear_new(w, ground, -2.0, 1.0, 1.0, 1.0), gear_new(w, ground, 0.0, 1.0, 1.0, 1.0) };
    chain(w, g, 2u);
    rack r = rack_new(w, ground, 0.0, 0.0, 3.0, 5.0, -2.0, 2.0, false);
    rde_physics_2d_joint* mesh = rde_physics_2d_joint_create_gear(w, g[1].hinge, r.slide, -1.0f);
    const double tooth = 2.0 * PI / 20.0;   // (a 20-tooth gear's)
    rde_physics_2d_joint_set_gear_range(mesh, -2.0f, 2.0f);
    rde_physics_2d_joint_set_gear_period(mesh, (float)tooth);
    rde_physics_2d_joint_enable_motor(g[0].hinge, 1.0f, 1e9f);   // (the first clockwise, the second anticlockwise: the rack goes on)
    run(w, g, 2u, 4.0);
    WITHIN("rack's end: not stopped at 2", fabs(rde_physics_2d_joint_get_coordinate(r.slide) - 2.0), 0.02);
    CHECK(!rde_physics_2d_joint_is_gear_engaged(mesh));
    WITHIN("rack's end: the motor's gear's speed off 1", fabs(fabs((double)rde_physics_2d_body_get_angular_velocity(g[0].body)) - 1.0), 1e-2);
    WITHIN("rack's end: the rack's gear's speed off 1", fabs(fabs((double)rde_physics_2d_body_get_angular_velocity(g[1].body)) - 1.0), 1e-2);
    WITHIN("rack's end: the gears' angle error (rad)", chain_error(g, 2u), 5e-3);
    WITHIN("rack's end: the rack's speed", fabs(rde_physics_2d_body_get_velocity(r.body).x), 1e-2);
    // Back: the rack taken back in at its gear's rim's speed, meshed again on a tooth.
    rde_physics_2d_joint_set_motor_speed(g[0].hinge, -1.0f);
    run(w, g, 2u, 1.0);
    CHECK(rde_physics_2d_joint_is_gear_engaged(mesh));
    WITHIN("rack back: its speed off its gear's rim's", fabs(rde_physics_2d_body_get_velocity(r.body).x - rde_physics_2d_body_get_angular_velocity(g[1].body)), 1e-2);
    WITHIN("rack back: off a whole tooth (rad)", fabs(rde_physics_2d_joint_get_gear_error(mesh)), 5e-3);
    CHECK(rde_physics_2d_joint_get_coordinate(r.slide) < 1.2);
    WITHIN("rack back: the gears' angle error (rad)", chain_error(g, 2u), 5e-3);
    rde_physics_2d_world_destroy(w);
}

// An upright rack let fall, turning two gears as it goes: what it loses in height they and it gain in motion.
static void test_rack_falling(void) {
    rde_physics_2d_world* w = world_new(9.81f);
    rde_physics_2d_body* ground = ground_new(w);
    gear g[2] = { gear_new(w, ground, 1.0, 0.0, 1.0, 2.0), gear_new(w, ground, 3.0, 0.0, 1.0, 2.0) };
    chain(w, g, 2u);
    rack r = rack_new(w, ground, 0.0, 0.0, 6.0, 3.0, -4.0, 4.0, true);   // (its line at x = 0, the first gear's rim at its left)
    rde_physics_2d_joint_create_gear(w, g[0].hinge, r.slide, 1.0f);    // (it falls, the gear turns anticlockwise: θ + y / r kept)
    run(w, g, 2u, 1.0);
    const double y = rde_physics_2d_joint_get_coordinate(r.slide), v = rde_physics_2d_body_get_velocity(r.body).y;
    const double kinetic = 0.5 * 3.0 * v * v + 0.5 * inertia(&g[0]) * pow(rde_physics_2d_body_get_angular_velocity(g[0].body), 2) +
                           0.5 * inertia(&g[1]) * pow(rde_physics_2d_body_get_angular_velocity(g[1].body), 2);
    const double lost = 3.0 * 9.81 * -y;
    CHECK(y < -1.0);
    WITHIN("falling rack: energy error", fabs(kinetic - lost) / lost, 2e-2);
    WITHIN("falling rack: travel error", fabs(y + g[0].turned), 2e-3);
    // (its fall's rate: m g / (m + I₁/r² + I₂/r²))
    const double a = 3.0 * 9.81 / (3.0 + inertia(&g[0]) + inertia(&g[1]));
    WITHIN("falling rack: fall off ½ a t²", fabs(-y - 0.5 * a) / (0.5 * a), 2e-2);
    rde_physics_2d_world_destroy(w);
}

// --- pulleys ------------------------------------------------------------------------------------------------------

static rde_physics_2d_body* weight_new(rde_physics_2d_world* w, double x, double y, double m) {
    rde_physics_2d_shape_def s;
    memset(&s, 0, sizeof(s));
    s.type = RDE_PHYSICS_2D_SHAPE_CIRCLE;
    s.circle.radius = 0.2f;
    s.layer = 1u;
    s.layer_mask = 8u;
    return rde_physics_2d_body_create(w, RDE_PHYSICS_2D_BODY_TYPE_DYNAMIC, (rde_vec_2F){ (float)x, (float)y }, 0.0f, &s, (float)m);
}

static double rope(rde_physics_2d_body* a, rde_vec_2F ga, rde_physics_2d_body* b, rde_vec_2F gb, double ratio) {
    const rde_vec_2F pa = rde_physics_2d_body_get_position(a), pb = rde_physics_2d_body_get_position(b);
    return hypot(pa.x - ga.x, pa.y - ga.y) + ratio * hypot(pb.x - gb.x, pb.y - gb.y);
}

// Atwood's machine (1 kg, 2 kg): they go at g / 3, the rope as long as ever; a block and tackle in balance stays; one
// thrown up slackens the rope, which is never longer than it is.
static void test_pulleys(void) {
    const rde_vec_2F ga = { -1.0f, 5.0f }, gb = { 1.0f, 5.0f };
    {
        rde_physics_2d_world* w = world_new(9.81f);
        rde_physics_2d_body* a = weight_new(w, -1.0, 0.0, 1.0), *b = weight_new(w, 1.0, 0.0, 2.0);
        rde_physics_2d_joint_create_pulley(w, a, (rde_vec_2F){ 0, 0 }, ga, b, (rde_vec_2F){ 0, 0 }, gb, 1.0f);
        double longest = 0.0, shortest = 1e9;
        for(int k = 0; k < 240; k++) {
            rde_physics_2d_world_step(w, DT);
            const double l = rope(a, ga, b, gb, 1.0);
            longest = fmax(longest, l);
            shortest = fmin(shortest, l);
        }
        const double fell = -rde_physics_2d_body_get_position(b).y, want = 0.5 * 9.81 / 3.0;
        WITHIN("Atwood: fall off g t² / 6", fabs(fell - want) / want, 2e-2);
        WITHIN("Atwood: rope longer than 10 by", longest - 10.0, 1e-2);
        WITHIN("Atwood: rope slack by", 10.0 - shortest, 1e-2);
        WITHIN("Atwood: a weight's sideways drift", fabs(rde_physics_2d_body_get_position(a).x + 1.0), 1e-2);
        rde_physics_2d_world_destroy(w);
    }
    {
        rde_physics_2d_world* w = world_new(9.81f);
        rde_physics_2d_body* a = weight_new(w, -1.0, 0.0, 1.0), *b = weight_new(w, 1.0, 0.0, 2.0);
        rde_physics_2d_joint_create_pulley(w, a, (rde_vec_2F){ 0, 0 }, ga, b, (rde_vec_2F){ 0, 0 }, gb, 2.0f);
        for(int k = 0; k < 480; k++) rde_physics_2d_world_step(w, DT);
        WITHIN("block and tackle: moved", fabs(rde_physics_2d_body_get_position(a).y) + fabs(rde_physics_2d_body_get_position(b).y), 1e-2);
        rde_physics_2d_world_destroy(w);
    }
    {
        rde_physics_2d_world* w = world_new(9.81f);
        rde_physics_2d_body* a = weight_new(w, -1.0, 0.0, 1.0), *b = weight_new(w, 1.0, 0.0, 1.0);
        rde_physics_2d_joint_create_pulley(w, a, (rde_vec_2F){ 0, 0 }, ga, b, (rde_vec_2F){ 0, 0 }, gb, 1.0f);
        rde_physics_2d_body_set_velocity(a, (rde_vec_2F){ 0.0f, 3.0f });
        double longest = 0.0, fastest = 0.0, slackest = 0.0;
        for(int k = 0; k < 720; k++) {
            rde_physics_2d_world_step(w, DT);
            const double l = rope(a, ga, b, gb, 1.0);
            longest = fmax(longest, l);
            slackest = fmax(slackest, 10.0 - l);
            fastest = fmax(fastest, hypot(rde_physics_2d_body_get_velocity(a).x, rde_physics_2d_body_get_velocity(a).y));
        }
        WITHIN("thrown: rope longer than 10 by", longest - 10.0, 1e-2);
        CHECK(slackest > 0.05);   // (it did go slack)
        WITHIN("thrown: fastest", fastest, 10.0);
        rde_physics_2d_world_destroy(w);
    }
}

// --- Sketching's mechanisms, as Play runs them --------------------------------------------------------------------

// (_scale: its place's — placed zoomed out, its own units more than the home frame's)
static u32 part_put_scaled(fude_zoom_scene* s, const c8* id, f64 x, f64 y, f64 hw, f64 hh, f64 turn, f64 scale, const c8* text) {
    f64 n[FUDE_ZOOM_SHAPE_NUMBERS + 200];
    const u32 kind = fude_zoom_symbol_find(id);
    CHECK(kind != FUDE_ZOOM_NONE);
    const u32 k = fude_zoom_symbol_numbers(n, kind, hw, hh, 2.0, text);
    return fude_zoom_scene_add_shape(s, s->root, (fude_zoom_place){ { x, y }, turn, scale }, FUDE_ZOOM_SHAPE_SYMBOL, n, k, (rde_color){ 1, 1, 1, 255 }, 0.2f, 0u, 0);
}

static u32 part_put(fude_zoom_scene* s, const c8* id, f64 x, f64 y, f64 hw, f64 hh, f64 turn, const c8* text) {
    return part_put_scaled(s, id, x, y, hw, hh, turn, 1.0, text);
}

// The plan's body drawn as object o.
static u32 body_of(const fude_zoom_mech_plan* p, u32 o) {
    const fude_zoom_mech_body* b = (const fude_zoom_mech_body*)p->bodies.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&p->bodies); i++) if(b[i].object == o) return i;
    return FUDE_ZOOM_NONE;
}

// A body as it runs: how far it has turned from as drawn (every turn counted), where its middle is.
typedef struct {
    u32    body;
    double turned, last;
} runner;

// (from as it starts: a gear turned to mesh not counted)
static runner runner_begin(const fude_zoom_mech_world* w, u32 body) {
    const fude_zoom_sim m = fude_zoom_mech_world_move(w, body);
    return (runner){ body, 0.0, atan2(m.b, m.a) };
}

static void runner_track(const fude_zoom_mech_world* w, runner* r) {
    const fude_zoom_sim m = fude_zoom_mech_world_move(w, r->body);
    const double a = atan2(m.b, m.a);
    double d = a - r->last;
    while(d > PI) d -= 2.0 * PI;
    while(d < -PI) d += 2.0 * PI;
    r->turned += d;
    r->last = a;
}

// How far a gear's teeth are from meeting another's gaps (a fraction of a tooth, the worst of the meshes): their
// places where they touch add to a half when they mesh; a gear's and a rack's, its less the rack's.
static double mesh_error(const fude_zoom_mech_world* w, const fude_zoom_mech_plan* p) {
    const fude_zoom_mech_body* b = (const fude_zoom_mech_body*)p->bodies.memory;
    const fude_zoom_mech_mesh* m = (const fude_zoom_mech_mesh*)p->meshes.memory;
    double worst = 0.0;
    for(u32 k = 0; k < (u32)rde_arr_length(&p->meshes); k++) {
        fude_zoom_mech_body ga = b[m[k].a];
        const fude_zoom_sim ma = fude_zoom_mech_world_move(w, m[k].a);
        ga.angle += atan2(ma.b, ma.a) - ga.phase;   // (as it stands now: its drawn angle and its whole turn since)
        double e;
        if(m[k].rack) {
            const fude_zoom_mech_body* rk = &b[m[k].b];
            const fude_zoom_sim mr = fude_zoom_mech_world_move(w, m[k].b);
            const fude_zoom_v2 c = fude_zoom_sim_apply(mr, rk->at);
            const fude_zoom_v2 u = { cos(rk->angle), sin(rk->angle) }, n = { -u.y, u.x };
            const double along = (ga.at.x - c.x) * u.x + (ga.at.y - c.y) * u.y, up = (ga.at.x - c.x) * n.x + (ga.at.y - c.y) * n.y;
            f64 first, pitch, line;
            const u32 teeth = fude_zoom_mech_rack_teeth(rk, &first, &pitch, &line);
            if(along > first + 0.5 * pitch || along < first - pitch * ((double)teeth - 0.5)) continue;   // (run off its teeth: let go)
            const double q = (along - first) / pitch - floor((along - first) / pitch);
            const double pg = fude_zoom_mech_tooth_at(&ga, up >= 0.0 ? atan2(-n.y, -n.x) : atan2(n.y, n.x));
            e = up >= 0.0 ? pg - q : pg + q;
        } else {
            fude_zoom_mech_body gb = b[m[k].b];
            const fude_zoom_sim mb = fude_zoom_mech_world_move(w, m[k].b);
            gb.angle += atan2(mb.b, mb.a) - gb.phase;
            const double toward = atan2(gb.at.y - ga.at.y, gb.at.x - ga.at.x);
            e = fude_zoom_mech_tooth_at(&ga, toward) + fude_zoom_mech_tooth_at(&gb, toward + PI);
        }
        e = e - 0.5;
        e -= floor(e + 0.5);   // (to the nearest whole tooth: −½ to ½)
#ifdef MECH_DEBUG
        if(fabs(e) > 0.05) printf("  mesh %u (%u-%u%s) off %.3f\n", k, m[k].a, m[k].b, m[k].rack ? " rack" : "", e);
#endif
        worst = fmax(worst, fabs(e));
    }
    return worst;
}

// Borja's mechanism: a motor at 30 rpm turning a gear, three more on (in an L), the last under a rack (the rack turned
// over, its teeth down on it). Every gear at the motor's speed, the other way from the one before; the rack at the
// last one's rim's, until that gear runs off its teeth — then the rack still there, the gears going on as fast; every
// gear's teeth in the gaps of those it meshes with, from the first step to the last; the motor up to speed smoothly.
static void test_borjas_mechanism(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    part_put(&s, "drive motor", 0, 0, 30, 30, 0.0, "30 rpm");
    const u32 go[4] = { part_put(&s, "gear 20T", 0, 0, 55, 55, 0.3, "20T"), part_put(&s, "gear 20T", 100, 0, 55, 55, 1.1, "20T"),
                        part_put(&s, "gear 20T", 200, 0, 55, 55, -0.4, "20T"), part_put(&s, "gear 20T", 200, 100, 55, 55, 2.0, "20T") };
    const u32 ro = part_put(&s, "rack", 260, 155, 200, 15, PI, "");   // (turned over: its pitch line at y 150, the top gear's rim)
    fude_zoom_mech_plan p; fude_zoom_mech_plan_init(&p);
    fude_zoom_mech_plan_build(&p, &s, NULL);
    CHECK(rde_arr_length(&p.meshes) == 4u);
    u32 gb[4];
    for(u32 i = 0; i < 4u; i++) { gb[i] = body_of(&p, go[i]); CHECK(gb[i] != FUDE_ZOOM_NONE); }
    const u32 rb = body_of(&p, ro);
    const fude_zoom_mech_body* pb = (const fude_zoom_mech_body*)p.bodies.memory;
    // (the motor's gear on it, not turned; the rest turned to mesh, each less than half a tooth)
    for(u32 i = 0; i < 4u; i++) WITHIN("Borja's: a gear turned to mesh (rad)", fabs(pb[gb[i]].phase), PI / 20.0 + 1e-9);
    fude_zoom_mech_world w; fude_zoom_mech_world_init(&w);
    CHECK(fude_zoom_mech_world_start(&w, &p));
    runner g[4];
    for(u32 i = 0; i < 4u; i++) g[i] = runner_begin(&w, gb[i]);
    WITHIN("Borja's: teeth off gaps at the start (tooth)", mesh_error(&w, &p), 1e-3);
    double lower = 0.0;   // (where its teeth end: as far as it goes)
    for(u32 k = 0; k < (u32)rde_arr_length(&p.meshes); k++) if(((const fude_zoom_mech_mesh*)p.meshes.memory)[k].rack) lower = ((const fude_zoom_mech_mesh*)p.meshes.memory)[k].lower;
    const fude_zoom_v2 rack0 = pb[rb].at;   // (as drawn: where it starts may be moved a little along itself, to mesh)
    const double d0 = -(fude_zoom_sim_apply(fude_zoom_mech_world_move(&w, rb), pb[rb].at).x - rack0.x);
    WITHIN("Borja's: the rack moved to mesh (points)", fabs(d0), 5.0 * PI * 0.5 + 1e-9);
    double worst_mesh = 0.0, worst_ratio = 0.0, worst_rack = 0.0, start = 0.0;
    double stopped_at = 0.0, rack_d = 0.0, worst_sag = 0.0;
    b8 stopped = false;
    for(int f = 0; f < 60 * 6; f++) {
        fude_zoom_mech_world_step(&w, 1.0 / 60.0);
        for(u32 i = 0; i < 4u; i++) runner_track(&w, &g[i]);
        for(u32 i = 1; i < 4u; i++) {
            const double e = fabs(g[i].turned + g[i - 1u].turned);   // (the same teeth: as far, the other way)
            if(f < 30) start = fmax(start, e); else worst_ratio = fmax(worst_ratio, e);
        }
        if(rde_arr_length(&p.meshes) > 0u) worst_mesh = fmax(worst_mesh, mesh_error(&w, &p));
        const fude_zoom_v2 c = fude_zoom_sim_apply(fude_zoom_mech_world_move(&w, rb), pb[rb].at);
        const double d = -(c.x - rack0.x);   // (along the rack's own way: it is turned over)
        worst_sag = fmax(worst_sag, fabs(c.y - rack0.y));
        if(!stopped && fabs(d - rack_d) < 1e-3 && f > 30) {
            stopped = true;
            stopped_at = d;
        }
        // (while it goes: as far as the top gear's rim, its pitch radius 50)
        if(!stopped && f > 30 && fabs(d - lower) > 5.0) worst_rack = fmax(worst_rack, fabs(d - d0 - 50.0 * g[3].turned));   // (clear of where it stops)
        rack_d = d;
    }
    WITHIN("Borja's: gears off each other at the start (rad)", start, 0.02);
    WITHIN("Borja's: gears off each other (rad)", worst_ratio, 5e-3);
    WITHIN("Borja's: teeth off gaps (tooth)", worst_mesh, 0.02);
    WITHIN("Borja's: the rack off its gear's rim", worst_rack, 0.5);
    CHECK(stopped);
    WITHIN("Borja's: the rack not stopped where its teeth end", fabs(stopped_at - lower), 1.0);
    WITHIN("Borja's: the rack off its line (sagging)", worst_sag, 0.01);
    for(u32 i = 0; i < 4u; i++) {
        const fude_zoom_sim mv = fude_zoom_mech_world_move(&w, g[i].body);
        RDE_UNUSED(mv);
    }
    // (the last second: every gear at 30 rpm, π a second)
    double was[4];
    for(u32 i = 0; i < 4u; i++) was[i] = g[i].turned;
    for(int f = 0; f < 60; f++) {
        fude_zoom_mech_world_step(&w, 1.0 / 60.0);
        for(u32 i = 0; i < 4u; i++) runner_track(&w, &g[i]);
    }
    for(u32 i = 0; i < 4u; i++) {
        char what[64];
        snprintf(what, sizeof(what), "Borja's: gear %u's speed off pi", i);
        WITHIN(what, fabs(fabs(g[i].turned - was[i]) - PI), 0.01);
    }
    fude_zoom_mech_world_destroy(&w);
    fude_zoom_mech_plan_destroy(&p);
    fude_zoom_scene_destroy(&s);
}

// A train of 10, 30 and 20 teeth on a motor: each at its teeth's share of the motor's speed, teeth in gaps all along;
// a gear something else is pinned to (a link on its axle) keeps its angle, the one meshing with it turns to it.
static void test_mixed_and_pinned(void) {
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        part_put(&s, "drive motor", 0, 0, 30, 30, 0.0, "60 rpm");
        const u32 go[3] = { part_put(&s, "gear 10T", 0, 0, 30, 30, 0.2, "10T"), part_put(&s, "gear 30T", 100, 0, 80, 80, 0.7, "30T"),
                            part_put(&s, "gear 20T", 100, 125, 55, 55, -1.0, "20T") };
        fude_zoom_mech_plan p; fude_zoom_mech_plan_init(&p);
        fude_zoom_mech_plan_build(&p, &s, NULL);
        CHECK(rde_arr_length(&p.meshes) == 2u);
        fude_zoom_mech_world w; fude_zoom_mech_world_init(&w);
        CHECK(fude_zoom_mech_world_start(&w, &p));
        runner g[3];
        for(u32 i = 0; i < 3u; i++) g[i] = runner_begin(&w, body_of(&p, go[i]));
        double worst_mesh = 0.0, worst = 0.0;
        for(int f = 0; f < 60 * 5; f++) {
            fude_zoom_mech_world_step(&w, 1.0 / 60.0);
            for(u32 i = 0; i < 3u; i++) runner_track(&w, &g[i]);
            worst_mesh = fmax(worst_mesh, mesh_error(&w, &p));
            worst = fmax(worst, fabs(g[1].turned + g[0].turned / 3.0) + fabs(g[2].turned + g[1].turned * 1.5));
        }
        WITHIN("mixed: teeth off gaps (tooth)", worst_mesh, 0.02);
        WITHIN("mixed: gears off their ratios (rad)", worst, 0.01);
        WITHIN("mixed: the 10T's 5 turns (rad)", fabs(fabs(g[0].turned) - 10.0 * PI + 2.0 * PI * 0.125), 0.1);   // (spinning up: an eighth of a turn less)
        fude_zoom_mech_world_destroy(&w);
        fude_zoom_mech_plan_destroy(&p);
        fude_zoom_scene_destroy(&s);
    }
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        const u32 a = part_put(&s, "gear 20T", 0, 0, 55, 55, 0.3, "20T");
        const u32 b = part_put(&s, "gear 20T", 0, 100, 55, 55, 1.1, "20T");
        part_put(&s, "link", 48, 0, 60, 12, 0.0, "");   // (its hole at 0: on the first gear's axle; at 96, nothing)
        fude_zoom_mech_plan p; fude_zoom_mech_plan_init(&p);
        fude_zoom_mech_plan_build(&p, &s, NULL);
        const fude_zoom_mech_body* pb = (const fude_zoom_mech_body*)p.bodies.memory;
        CHECK(pb[body_of(&p, a)].phase == 0.0);
        fude_zoom_mech_world w; fude_zoom_mech_world_init(&w);
        CHECK(fude_zoom_mech_world_start(&w, &p));
        WITHIN("pinned: teeth off gaps (tooth)", mesh_error(&w, &p), 1e-3);
        RDE_UNUSED(b);
        fude_zoom_mech_world_destroy(&w);
        fude_zoom_mech_plan_destroy(&p);
        fude_zoom_scene_destroy(&s);
    }
}

// A gear and a rack placed zoomed out (their own units half the home frame's, as the library sizes them for the screen):
// their teeth as big as each other's, meshed — the rack at the gear's rim, teeth in gaps.
static void test_zoomed(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    part_put_scaled(&s, "drive motor", 0, 0, 60, 60, 0.0, 0.5, "20 rpm");
    const u32 go = part_put_scaled(&s, "gear 20T", 0, 0, 110, 110, 0.4, 0.5, "20T");   // (its pitch circle 50 round, home)
    const u32 ro = part_put_scaled(&s, "rack", 0, -55, 600, 30, 0.0, 0.5, "");         // (its pitch line at -50, home)
    fude_zoom_mech_plan p; fude_zoom_mech_plan_init(&p);
    fude_zoom_mech_plan_build(&p, &s, NULL);
    CHECK(rde_arr_length(&p.meshes) == 1u);
    fude_zoom_mech_world w; fude_zoom_mech_world_init(&w);
    CHECK(fude_zoom_mech_world_start(&w, &p));
    const u32 rb = body_of(&p, ro);
    runner g = runner_begin(&w, body_of(&p, go));
    const fude_zoom_v2 at = fude_zoom_sim_apply(fude_zoom_mech_world_move(&w, rb), ((const fude_zoom_mech_body*)p.bodies.memory)[rb].at);   // (where it starts)
    double worst_mesh = 0.0, worst = 0.0;
    for(int f = 0; f < 120; f++) {
        fude_zoom_mech_world_step(&w, 1.0 / 60.0);
        runner_track(&w, &g);
        worst_mesh = fmax(worst_mesh, mesh_error(&w, &p));
        const fude_zoom_v2 c = fude_zoom_sim_apply(fude_zoom_mech_world_move(&w, rb), ((const fude_zoom_mech_body*)p.bodies.memory)[rb].at);
        worst = fmax(worst, fabs((c.x - at.x) - 50.0 * g.turned));   // (under it: the rack goes as the gear's bottom does)
    }
    CHECK(fabs(g.turned) > 1.0);
    WITHIN("zoomed: teeth off gaps (tooth)", worst_mesh, 0.02);
    WITHIN("zoomed: the rack off its gear's rim", worst, 0.5);
    fude_zoom_mech_world_destroy(&w);
    fude_zoom_mech_plan_destroy(&p);
    fude_zoom_scene_destroy(&s);
}

// Every mechanism example, and those of both, run six seconds as Play runs them: nothing goes to infinity or flies away,
// what is driven or let go moves, every gear's teeth in the gaps of those it meshes with all along; the circuits of
// those of both running with them (their LEDs lit at some time). What the biggest costs a step, said.
// --- a circuit and a mechanism together (coupling.h) ---------------------------------------------------------------

// What is in a scene played as Play plays it: its mechanism's world, its circuit, the two coupled.
typedef struct {
    fude_zoom_mech_plan p;
    fude_zoom_mech_world w;
    fude_zoom_circuit c;
    fude_zoom_coupling k;
    b8 ok;
    u32 coupled;
} played;

static void play_begin(played* x, fude_zoom_scene* s) {
    fude_zoom_mech_plan_init(&x->p);
    fude_zoom_mech_world_init(&x->w);
    fude_zoom_circuit_init(&x->c);
    fude_zoom_coupling_init(&x->k);
    CHECK(fude_zoom_mech_plan_build(&x->p, s, NULL) > 0u && fude_zoom_mech_world_start(&x->w, &x->p));
    fude_zoom_circuit_build(&x->c, s);
    fude_zoom_circuit_reset(&x->c);
    fude_zoom_circuit_build(&x->c, s);
    x->coupled = fude_zoom_coupling_build(&x->k, &x->c, &x->w, s);
    x->ok = true;
}

static void play_frames(played* x, u32 frames) {
    for(u32 f = 0; f < frames; f++) {
        x->ok = x->ok && fude_zoom_coupling_step(&x->k, &x->c, &x->w, 1.0 / 60.0);
    }
}

static void play_end(played* x) {
    fude_zoom_coupling_destroy(&x->k);
    fude_zoom_circuit_destroy(&x->c);
    fude_zoom_mech_world_destroy(&x->w);
    fude_zoom_mech_plan_destroy(&x->p);
}

static fude_zoom_circuit_part* part_of_object(played* x, u32 o) {
    fude_zoom_circuit_part* p = (fude_zoom_circuit_part*)x->c.parts.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&x->c.parts); i++) if(p[i].object == o) return &p[i];
    return NULL;
}

// How fast body _b (the world's) turns over _frames (radians a second, counter-clockwise), played meanwhile.
static double spin_over(played* x, u32 b, u32 frames) {
    runner r = runner_begin(&x->w, b);
    for(u32 f = 0; f < frames; f++) {
        play_frames(x, 1u);
        runner_track(&x->w, &r);
    }
    return r.turned / ((double)frames / 60.0);
}

// A battery of _volts (reversed: its minus to the motor's first pin), maybe a switch, a motor (6 V, 60 rpm) with a
// 10-tooth gear on its shaft and a 30-tooth one over it.
typedef struct { fude_zoom_scene s; u32 motor, small, big, sw; rde_arr born; } motor_rig;

static void motor_rig_make(motor_rig* m, double volts, b8 reversed, int sw) {
    fude_zoom_scene_init(&m->s, 7);
    m->born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_placer pl = fude_zoom_placer_make(&m->s, m->s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, &m->born);
    char v[16];
    snprintf(v, sizeof v, "%gV", volts);
    const u32 bat = fude_zoom_placer_part(&pl, "battery", -300, 0, 0, 0, 0, v);
    m->small = fude_zoom_placer_part(&pl, "gear 10T", 0, 0, 0, 0, 0, "");
    m->big   = fude_zoom_placer_part(&pl, "gear 30T", 0, 100, 0, 0, 0, "");
    m->motor = fude_zoom_placer_part(&pl, "motor", 0, 0, 0, 80, 80, "6V 60rpm");
    m->sw = FUDE_ZOOM_NONE;
    u32 from = bat, from_pin = reversed ? 1u : 0u;
    if(sw >= 0) {
        m->sw = fude_zoom_placer_part(&pl, "SPST switch", -150, 120, 0, 0, 0, sw ? "on" : "off");
        fude_zoom_placer_wire(&pl, from, from_pin, m->sw, 0, false);
        from = m->sw; from_pin = 1u;
    }
    fude_zoom_placer_wire(&pl, from, from_pin, m->motor, 0, false);
    fude_zoom_placer_wire(&pl, m->motor, 1, bat, reversed ? 0u : 1u, false);
}

static void motor_rig_free(motor_rig* m) {
    rde_arr_free(&m->born);
    fude_zoom_scene_destroy(&m->s);
}

// A motor comes up to the speed its volts make (its back-EMF balancing them: nothing loads it), turning the way its
// volts say; drawing much as it starts, little once at speed; switched off, nothing; on, up to speed.
static void test_motor_drives(void) {
    const double volts[3] = { 3.0, 6.0, 9.0 };
    for(u32 i = 0; i < 3u; i++) {
        motor_rig m;
        motor_rig_make(&m, volts[i], false, -1);
        played x;
        play_begin(&x, &m.s);
        CHECK(x.coupled == 1u && fude_zoom_mech_world_shaft(&x.w, m.motor) != FUDE_ZOOM_NONE);
        fude_zoom_circuit_part* mp = part_of_object(&x, m.motor);
        CHECK(mp != NULL && mp->shafted);
        play_frames(&x, 120u);
        const double want = volts[i] / 6.0 * 2.0 * PI;
        const double small = spin_over(&x, body_of(&x.w.plan, m.small), 60u);
        char what[96];
        snprintf(what, sizeof what, "motor at %gV: its speed off (of %.2f rad/s)", volts[i], want);
        WITHIN(what, fabs(small - want) / want, 0.02);
        const double big = spin_over(&x, body_of(&x.w.plan, m.big), 60u);
        snprintf(what, sizeof what, "motor at %gV: the gear on off a third, the other way", volts[i]);
        WITHIN(what, fabs(big + want / 3.0) / (want / 3.0), 0.02);
        snprintf(what, sizeof what, "motor at %gV: its current at speed (A)", volts[i]);
        WITHIN(what, fabs(mp->pin_i[0]), 0.03);
        CHECK(x.ok);
        play_end(&x);
        motor_rig_free(&m);
    }
    // Its battery the other way round: the other way, as fast.
    {
        motor_rig m;
        motor_rig_make(&m, 6.0, true, -1);
        played x;
        play_begin(&x, &m.s);
        play_frames(&x, 120u);
        WITHIN("motor reversed: its speed off", fabs(spin_over(&x, body_of(&x.w.plan, m.small), 60u) + 2.0 * PI) / (2.0 * PI), 0.02);
        play_end(&x);
        motor_rig_free(&m);
    }
    // Held still (an arm on its shaft pinned to the ground besides): it does not turn, all its battery's volts across its
    // winding — V / (R + the battery's ½ Ω) through it.
    {
        fude_zoom_scene sc; fude_zoom_scene_init(&sc, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_placer pl = fude_zoom_placer_make(&sc, sc.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, &born);
        const u32 bat = fude_zoom_placer_part(&pl, "battery", -300, 0, 0, 0, 0, "6V");
        const u32 arm = fude_zoom_placer_part(&pl, "link", 63, 0, 0, 150, 24, "");
        fude_zoom_placer_part(&pl, "fixed pivot", 126, -10, 0, 0, 0, "");   // (its hole, 10 over its middle, on the arm's other end)
        const u32 mo = fude_zoom_placer_part(&pl, "motor", 0, 0, 0, 80, 80, "6V 60rpm");
        fude_zoom_placer_wire(&pl, bat, 0, mo, 0, false);
        fude_zoom_placer_wire(&pl, mo, 1, bat, 1, false);
        played x;
        play_begin(&x, &sc);
        play_frames(&x, 60u);
        const fude_zoom_circuit_part* mp = part_of_object(&x, mo);
        WITHIN("motor held still: its arm's turn (rad/s)", fabs(spin_over(&x, body_of(&x.w.plan, arm), 60u)), 0.01);
        WITHIN("motor held still: its current off V / R", fabs(fabs(mp->pin_i[0]) - 6.0 / 8.5) / (6.0 / 8.5), 0.02);
        play_end(&x);
        rde_arr_free(&born);
        fude_zoom_scene_destroy(&sc);
    }
    // Switched off: still, nothing through it; on: up to speed.
    {
        motor_rig m;
        motor_rig_make(&m, 6.0, false, 0);
        played x;
        play_begin(&x, &m.s);
        WITHIN("motor off: its turn (rad/s)", fabs(spin_over(&x, body_of(&x.w.plan, m.small), 60u)), 1e-3);
        fude_zoom_circuit_part* sp = part_of_object(&x, m.sw);
        CHECK(sp != NULL);
        if(sp != NULL) sp->switch_on = 1u;
        CHECK(x.ok);
        play_frames(&x, 120u);
        CHECK(x.ok);
        WITHIN("motor switched on: its speed off", fabs(spin_over(&x, body_of(&x.w.plan, m.small), 60u) - 2.0 * PI) / (2.0 * PI), 0.02);
        play_end(&x);
        motor_rig_free(&m);
    }
}

// A dynamo: the example's drive motor turning it four times as fast as itself (30 rpm: 4π rad/s, the clock's way):
// about 12 V across it, its LED lit, its meter reading what is left of them past its winding.
static void test_dynamo(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_example_build(&s, s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, FUDE_ZOOM_EXAMPLE_DYNAMO, NULL, &born);
    played x;
    play_begin(&x, &s);
    CHECK(x.coupled == 1u);
    play_frames(&x, 120u);
    const fude_zoom_mech_shaft* sh = (const fude_zoom_mech_shaft*)x.w.shafts.memory;
    CHECK(rde_arr_length(&x.w.shafts) == 1u);
    const double spin = spin_over(&x, sh[0].body, 60u);
    WITHIN("dynamo: its speed off (of −4π)", fabs(spin + 4.0 * PI) / (4.0 * PI), 0.02);
    double led = 0.0, meter = 0.0, emf = 0.0;
    const fude_zoom_circuit_part* p = (const fude_zoom_circuit_part*)x.c.parts.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&x.c.parts); i++) {
        if(p[i].part->model == FUDE_ZOOM_MODEL_LED) led = p[i].shown;
        if(p[i].part->model == FUDE_ZOOM_MODEL_VOLTMETER) meter = p[i].shown;
        if(p[i].part->model == FUDE_ZOOM_MODEL_MOTOR) emf = -fude_zoom_motor_k(&p[i]) * p[i].spin;
    }
    WITHIN("dynamo: its back-EMF off 12 V", fabs(emf - 12.0), 0.3);
    CHECK(led > 0.9);
    WITHIN("dynamo: its meter off what is left past its winding", fabs(meter - (emf - 0.022 * 8.0)), 0.4);
    CHECK(x.ok);
    play_end(&x);
    rde_arr_free(&born);
    fude_zoom_scene_destroy(&s);
}

// A rack let fall, its teeth on a gear: on its own axle; on a motor's shaft with nothing joined to it (it falls as
// fast: a dynamo with nothing to light does not hold it back); shorted through an ohm (it hardly falls: the current it
// makes holds it).
static double rack_fall(int how) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_placer pl = fude_zoom_placer_make(&s, s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, &born);
    fude_zoom_placer_part(&pl, "gear 20T", 0, 0, 0, 0, 0, "");
    const u32 rack = fude_zoom_placer_part(&pl, "rack", 55, 0, 90, 300, 30, "");   // (upright, its teeth toward the gear)
    if(how > 0) {
        const u32 mo = fude_zoom_placer_part(&pl, "motor", 0, 0, 0, 80, 80, "6V 60rpm");
        if(how == 2) {
            const u32 r = fude_zoom_placer_part(&pl, "resistor", 0, -150, 0, 0, 0, "1");
            fude_zoom_placer_wire(&pl, mo, 0, r, 0, false);
            fude_zoom_placer_wire(&pl, r, 1, mo, 1, false);
        }
    }
    played x;
    play_begin(&x, &s);
    play_frames(&x, 6u);   // (a tenth of a second)
    const u32 b = body_of(&x.w.plan, rack);
    const fude_zoom_mech_body* rb = &((const fude_zoom_mech_body*)x.w.plan.bodies.memory)[b];
    const fude_zoom_v2 now = fude_zoom_mech_world_point(&x.w, b, rb->at);
    const double fell = rb->at.y - now.y;
    CHECK(x.ok);
    play_end(&x);
    rde_arr_free(&born);
    fude_zoom_scene_destroy(&s);
    return fell;
}

static void test_dynamo_holds(void) {
    const double free_fall = rack_fall(0), open = rack_fall(1), shorted = rack_fall(2);
    CHECK(free_fall > 5.0);
    WITHIN("a dynamo joined to nothing: its rack's fall off the free one's", fabs(open - free_fall) / free_fall, 0.02);
    WITHIN("a dynamo shorted: its rack's fall of the free one's", shorted / free_fall, 0.2);
}

// The shuttle: its rack back and forth between its buttons, the motor turned back at each — its teeth always on the
// pinion; its LEDs in turn.
static void test_shuttle(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_example_build(&s, s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, FUDE_ZOOM_EXAMPLE_SHUTTLE, NULL, &born);
    played x;
    play_begin(&x, &s);
    CHECK(x.coupled == 3u);   // (its motor, its two buttons)
    u32 rack = FUDE_ZOOM_NONE;
    const fude_zoom_mech_body* b = (const fude_zoom_mech_body*)x.w.plan.bodies.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&x.w.plan.bodies); i++) if(b[i].part->kind == FUDE_ZOOM_MECH_RACK) rack = i;
    CHECK(rack != FUDE_ZOOM_NONE);
    if(rack == FUDE_ZOOM_NONE) { play_end(&x); rde_arr_free(&born); fude_zoom_scene_destroy(&s); return; }
    double lo = 1e9, hi = -1e9, was = 0.0;
    int way = 0;
    u32 turns = 0;
    b8 green = false, red = false;
    for(u32 f = 0; f < 60u * 14u; f++) {
        play_frames(&x, 1u);
        const double at = fude_zoom_mech_world_point(&x.w, rack, b[rack].at).x - b[rack].at.x;
        if(f > 30u) { lo = fmin(lo, at); hi = fmax(hi, at); }
        const int now = at > was + 0.05 ? 1 : (at < was - 0.05 ? -1 : way);
        if(now != way && way != 0) turns++;
        way = now;
        was = at;
        const fude_zoom_circuit_part* p = (const fude_zoom_circuit_part*)x.c.parts.memory;
        for(u32 i = 0; i < (u32)rde_arr_length(&x.c.parts); i++) {
            if(p[i].part->model != FUDE_ZOOM_MODEL_LED || p[i].shown < 0.5) continue;
            green = green || strcmp(p[i].color, "green") == 0;
            red   = red || strcmp(p[i].color, "red") == 0;
        }
    }
    printf("  shuttle: %u turns back in 14 s, between %.0f and %.0f points from where it started\n", turns, lo, hi);
    CHECK(turns >= 6u);
    CHECK(green && red);
    // (it starts at −60 over the left button: between where its ends reach each button — its middle at −40 and 40)
    CHECK(lo > 0.0 && lo < 40.0 && hi > 80.0 && hi < 120.0);
    CHECK(x.ok);
    play_end(&x);
    rde_arr_free(&born);
    fude_zoom_scene_destroy(&s);
}

// The turn counter: its arm over its button once a turn; the counter's LEDs, in binary, the turns it has made past it.
static void test_turn_counter(void) {
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_example_build(&s, s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, FUDE_ZOOM_EXAMPLE_TURN_COUNTER, NULL, &born);
    played x;
    play_begin(&x, &s);
    CHECK(x.coupled == 2u);
    const fude_zoom_mech_shaft* sh = (const fude_zoom_mech_shaft*)x.w.shafts.memory;
    CHECK(rde_arr_length(&x.w.shafts) == 1u);
    runner r = runner_begin(&x.w, sh[0].body);
    u32 bad = 0, checked = 0;
    for(u32 f = 0; f < 60u * 12u; f++) {
        play_frames(&x, 1u);
        runner_track(&x.w, &r);
        // (pressed as it comes over the button, straight down: a quarter turn short of a whole one from where it starts)
        const double past = r.turned - 1.5 * PI + 0.2;
        const int want = past >= 0.0 ? (int)floor(past / (2.0 * PI)) + 1 : 0;
        const double near = fmod(fabs(past), 2.0 * PI);
        if(near < 0.45 || near > 2.0 * PI - 0.45) continue;   // (over the button, about: either)
        int got = 0, bit = 0;
        const fude_zoom_circuit_part* p = (const fude_zoom_circuit_part*)x.c.parts.memory;
        static const char* const order[4] = { "red", "yellow", "green", "blue" };
        for(u32 k = 0; k < 4u; k++) {
            bit = 0;
            for(u32 i = 0; i < (u32)rde_arr_length(&x.c.parts); i++) {
                if(p[i].part->model == FUDE_ZOOM_MODEL_LED && strcmp(p[i].color, order[k]) == 0) bit = p[i].shown > 0.5;
            }
            got |= bit << k;
        }
        checked++;
        if(got != (want & 15)) {
            if(bad < 3u) printf("  turn counter at %.2f s: %d on its LEDs, %d turns\n", (double)f / 60.0, got, want);
            bad++;
        }
    }
    printf("  turn counter: %.2f turns in 12 s\n", r.turned / (2.0 * PI));
    CHECK(checked > 500u && bad == 0u);
    WITHIN("turn counter: its turns off 6 in 12 s", fabs(r.turned / (2.0 * PI) - 6.0), 0.3);
    CHECK(x.ok);
    play_end(&x);
    rde_arr_free(&born);
    fude_zoom_scene_destroy(&s);
}

static void test_examples(void) {
    for(u32 e = 0; e < FUDE_ZOOM_EXAMPLE_COUNT; e++) {
        if(fude_zoom_example_group(e) == FUDE_ZOOM_EXAMPLES_ELECTRONICS) continue;
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        const u32 made = fude_zoom_example_build(&s, s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, e, NULL, &born);
        if(made < 3u) printf("FAIL example %u: %u made\n", e, made), fails++;
        rde_arr_free(&born);
        fude_zoom_mech_plan p; fude_zoom_mech_plan_init(&p);
        const u32 nb = fude_zoom_mech_plan_build(&p, &s, NULL);
        fude_zoom_mech_world w; fude_zoom_mech_world_init(&w);
        CHECK(nb > 0u && fude_zoom_mech_world_start(&w, &p));
        fude_zoom_circuit c; fude_zoom_circuit_init(&c);
        const b8 circuit = fude_zoom_circuit_build(&c, &s) > 0u;
        fude_zoom_coupling k; fude_zoom_coupling_init(&k);
        const b8 coupled = circuit && fude_zoom_coupling_build(&k, &c, &w, &s) > 0u;
        CHECK(circuit == (fude_zoom_example_group(e) == FUDE_ZOOM_EXAMPLES_BOTH));
        const fude_zoom_mech_body* b = (const fude_zoom_mech_body*)p.bodies.memory;
        double worst_mesh = 0.0, far = 0.0, spent = 0.0;
        rde_arr moved_arr = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
        rde_arr_resize(&moved_arr, nb);
        u8* went = (u8*)moved_arr.memory;   // (each body: it moved, at some time)
        b8 finite = true, lit = false, ran = true;
        for(int f = 0; f < 360; f++) {
            const clock_t t0 = clock();
            if(coupled) {
                ran = ran && fude_zoom_coupling_step(&k, &c, &w, 1.0 / 60.0);
            } else {
                fude_zoom_mech_world_step(&w, 1.0 / 60.0);
                if(circuit) ran = ran && fude_zoom_circuit_run(&c, 1.0 / 60.0, 400u);
            }
            spent += (double)(clock() - t0) / CLOCKS_PER_SEC;
            for(u32 i = 0; i < nb; i++) {
                const fude_zoom_sim m = fude_zoom_mech_world_move(&w, i);
                finite = finite && isfinite(m.a) && isfinite(m.b) && isfinite(m.tx) && isfinite(m.ty);
                const fude_zoom_v2 at = fude_zoom_sim_apply(m, b[i].at);
                far = fmax(far, hypot(at.x - b[i].at.x, at.y - b[i].at.y));
                went[i] = went[i] || fabs(atan2(m.b, m.a) - b[i].phase) > 0.1 || hypot(at.x - b[i].at.x, at.y - b[i].at.y) > 2.0;
            }
            worst_mesh = fmax(worst_mesh, mesh_error(&w, &p));
            if(circuit) {
                const fude_zoom_circuit_part* cp = (const fude_zoom_circuit_part*)c.parts.memory;
                for(u32 i = 0; i < (u32)rde_arr_length(&c.parts); i++) lit = lit || (cp[i].part->model == FUDE_ZOOM_MODEL_LED && cp[i].shown > 0.3);
            }
        }
        // (what moved: a body a tenth of a radian or 2 points from where it was drawn)
        u32 moving = 0, moved = 0;
        for(u32 i = 0; i < nb; i++) {
            if(b[i].fixed || ((rde_physics_2d_body* const*)w.bodies.memory)[i] == NULL) continue;
            moving++;
            moved += went[i] ? 1u : 0u;
        }
        rde_arr_free(&moved_arr);
        char what[96];
        snprintf(what, sizeof(what), "example %u: finite", e);
        if(!finite) printf("FAIL %s\n", what), fails++;
        snprintf(what, sizeof(what), "example %u: teeth off gaps (tooth)", e);
        WITHIN(what, worst_mesh, 0.05);
        snprintf(what, sizeof(what), "example %u: furthest a body went (points)", e);
        WITHIN(what, far, 2000.0);
        snprintf(what, sizeof(what), "example %u: bodies not moving of", e);
        WITHIN(what, (double)(moving - moved), (double)moving * 0.25);   // (a few may rest where they are: a weight on a wall)
        if(circuit) {
            CHECK(ran && (lit || e == FUDE_ZOOM_EXAMPLE_FORWARD_BACK));   // (its motor and rack what it shows)
        }
        // (those where the two work on each other: coupled)
        CHECK(coupled == ((e >= FUDE_ZOOM_EXAMPLE_MOTOR_GEARS && e <= FUDE_ZOOM_EXAMPLE_SHUTTLE) || e == FUDE_ZOOM_EXAMPLE_EVERYTHING));
        if(e == FUDE_ZOOM_EXAMPLE_EVERYTHING) {
            printf("  everything at once: %u bodies, %u meshes, %u circuit parts, %u wires: %.2f ms a frame (60 a second)\n", nb, (u32)rde_arr_length(&p.meshes),
                   (u32)rde_arr_length(&c.parts), (u32)rde_arr_length(&c.wires), spent / 360.0 * 1000.0);
        }
        fude_zoom_coupling_destroy(&k);
        fude_zoom_circuit_destroy(&c);
        fude_zoom_mech_world_destroy(&w);
        fude_zoom_mech_plan_destroy(&p);
        fude_zoom_scene_destroy(&s);
    }
}

int main(void) {
    test_pair();
    test_trains();
    test_energy_and_reverse();
    test_long_run();
    test_rack();
    test_rack_end();
    test_rack_falling();
    test_pulleys();
    test_borjas_mechanism();
    test_mixed_and_pinned();
    test_zoomed();
    test_motor_drives();
    test_dynamo();
    test_dynamo_holds();
    test_shuttle();
    test_turn_counter();
    test_examples();
    if(fails == 0) printf("ALL PASSED\n"); else printf("%d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
