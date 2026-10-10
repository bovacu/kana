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
        if(m[k].belt) continue;   // (a belt's: no teeth meeting)
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

// Whether the mechanism works object o's circuit part (coupling.h: not a hand's to tap).
static b8 worked_by_mechanism(played* x, u32 o) {
    const fude_zoom_circuit_part* p = part_of_object(x, o);
    return p != NULL && fude_zoom_coupling_works(&x->k, (u32)(p - (const fude_zoom_circuit_part*)x->c.parts.memory));
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
// A SERVO (coupling.h) with a link on its shaft, its signal a pulse source's at 50 Hz: the link turned to where the
// pulses say (its horn at 90° as it began, the link along it then: 1 ms 45° — the link 45° clockwise —, 2 ms 135°), as
// fast as it turns (60° in 0.1 s at 4.8 V); held still by a fixed pivot, it strains and draws its stall current.
static void test_servo(void) {
    for(u32 k = 0; k < 3u; k++) {
        const c8* const pulse[3] = { "50Hz 5V 1ms", "50Hz 5V 2ms", "50Hz 5V 1.5ms" };
        const double turn[3] = { -PI / 4.0, PI / 4.0, 0.0 };
        fude_zoom_scene sc; fude_zoom_scene_init(&sc, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_placer pl = fude_zoom_placer_make(&sc, sc.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, &born);
        const u32 sv = fude_zoom_placer_part(&pl, "servo", 0, 0, 0, 0, 0, "SG90 servo");
        const u32 arm = fude_zoom_placer_part(&pl, "link", 63, 0, 0, 150, 24, "");
        const u32 rail = fude_zoom_placer_part(&pl, "supply rail", -300, 200, 0, 0, 0, "5V");
        const u32 gnd = fude_zoom_placer_part(&pl, "ground", -300, -200, 0, 0, 0, "");
        const u32 clk = fude_zoom_placer_part(&pl, "clock", -500, 0, 0, 0, 0, pulse[k]);
        fude_zoom_placer_wire(&pl, rail, 0, sv, 1, false);
        fude_zoom_placer_wire(&pl, sv, 0, gnd, 0, false);
        fude_zoom_placer_wire(&pl, clk, 0, sv, 2, false);
        fude_zoom_placer_wire(&pl, clk, 1, gnd, 0, false);
        played x;
        play_begin(&x, &sc);
        CHECK(x.coupled >= 1u && fude_zoom_mech_world_shaft(&x.w, sv) != FUDE_ZOOM_NONE);
        const u32 b = body_of(&x.w.plan, arm);
        const fude_zoom_sim m0 = fude_zoom_mech_world_move(&x.w, b);
        const double a0 = atan2(m0.b, m0.a);
        play_frames(&x, 6u);   // (its first pulses: on its way, not there — 45° takes 75 ms)
        const fude_zoom_sim m1 = fude_zoom_mech_world_move(&x.w, b);
        if(k < 2u) CHECK(fabs(atan2(m1.b, m1.a) - a0) > 0.05 && fabs(atan2(m1.b, m1.a) - a0) < fabs(turn[k]) - 0.05);
        play_frames(&x, 60u);
        const fude_zoom_sim m = fude_zoom_mech_world_move(&x.w, b);
        char what[96];
        snprintf(what, sizeof what, "servo, %s: the link's turn off (rad)", pulse[k]);
        WITHIN(what, fabs(atan2(m.b, m.a) - a0 - turn[k]), 0.03);
        const fude_zoom_circuit_part* sp = part_of_object(&x, sv);
        snprintf(what, sizeof what, "servo, %s: its horn's angle off (deg)", pulse[k]);
        WITHIN(what, fabs(sp->shown - (90.0 + turn[k] * 180.0 / PI)), 2.0);
        snprintf(what, sizeof what, "servo, %s: at rest, what it draws (A)", pulse[k]);
        WITHIN(what, fabs(sp->pin_i[1]), 0.05);
        CHECK(x.ok);
        play_end(&x);
        rde_arr_free(&born);
        fude_zoom_scene_destroy(&sc);
    }
    // The servo tester (examples.h): its 555's pulses as long as R1 (5.6k and the pot's 10k) makes them — 1.37 ms, 78° —;
    // the pot tapped a quarter on (15k: 1.81 ms, 118°), the arm turned with it.
    {
        fude_zoom_scene sc; fude_zoom_scene_init(&sc, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_example_build(&sc, sc.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, FUDE_ZOOM_EXAMPLE_SERVO_TESTER, NULL, &born);
        played x;
        play_begin(&x, &sc);
        CHECK(x.coupled >= 1u);
        play_frames(&x, 60u);
        u32 sv = FUDE_ZOOM_NONE, pot = FUDE_ZOOM_NONE;
        const fude_zoom_circuit_part* cp = (const fude_zoom_circuit_part*)x.c.parts.memory;
        for(u32 i = 0; i < (u32)rde_arr_length(&x.c.parts); i++) {
            if(cp[i].part->model == FUDE_ZOOM_MODEL_SERVO) sv = i;
            if(cp[i].part->model == FUDE_ZOOM_MODEL_POT) pot = i;
        }
        CHECK(sv != FUDE_ZOOM_NONE && pot != FUDE_ZOOM_NONE);
        if(sv != FUDE_ZOOM_NONE && pot != FUDE_ZOOM_NONE) {
            WITHIN("servo tester, the pot halfway: its angle off 78° (deg)", fabs(cp[sv].shown - 78.4), 5.0);
            c8 say[32];
            CHECK(fude_zoom_circuit_tap(&x.c, pot, -1, say, sizeof say));
            play_frames(&x, 60u);
            cp = (const fude_zoom_circuit_part*)x.c.parts.memory;
            WITHIN("servo tester, the pot at 75%: its angle off 118° (deg)", fabs(cp[sv].shown - 118.0), 5.0);
            CHECK(!cp[sv].burnt && x.ok);
        }
        play_end(&x);
        rde_arr_free(&born);
        fude_zoom_scene_destroy(&sc);
    }
    // Held still: the link's other end on a fixed pivot.
    {
        fude_zoom_scene sc; fude_zoom_scene_init(&sc, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_placer pl = fude_zoom_placer_make(&sc, sc.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, &born);
        const u32 sv = fude_zoom_placer_part(&pl, "servo", 0, 0, 0, 0, 0, "SG90 servo");
        const u32 arm = fude_zoom_placer_part(&pl, "link", 63, 0, 0, 150, 24, "");
        fude_zoom_placer_part(&pl, "fixed pivot", 126, -10, 0, 0, 0, "");
        const u32 rail = fude_zoom_placer_part(&pl, "supply rail", -300, 200, 0, 0, 0, "5V");
        const u32 gnd = fude_zoom_placer_part(&pl, "ground", -300, -200, 0, 0, 0, "");
        const u32 clk = fude_zoom_placer_part(&pl, "clock", -500, 0, 0, 0, 0, "50Hz 5V 1ms");
        fude_zoom_placer_wire(&pl, rail, 0, sv, 1, false);
        fude_zoom_placer_wire(&pl, sv, 0, gnd, 0, false);
        fude_zoom_placer_wire(&pl, clk, 0, sv, 2, false);
        fude_zoom_placer_wire(&pl, clk, 1, gnd, 0, false);
        played x;
        play_begin(&x, &sc);
        play_frames(&x, 60u);
        WITHIN("servo held still: its link's turn (rad/s)", fabs(spin_over(&x, body_of(&x.w.plan, arm), 30u)), 0.02);
        const fude_zoom_circuit_part* sp = part_of_object(&x, sv);
        WITHIN("servo held still: what it draws off its stall's (A)", fabs(fabs(sp->pin_i[1]) - 0.66), 0.05);
        play_end(&x);
        rde_arr_free(&born);
        fude_zoom_scene_destroy(&sc);
    }
}

// STRENGTH (mechrun.h): each joint against what it takes — a pin's shear (its parts' material over a pin six tenths of
// their half width across: about 980 N in plastic for a 24-wide link), a rope's 2 kN. A link from a pivot holding a
// weight: 1 kg for good; 5 tonnes, its pin shears — said, and the link and weight fall. A rope pendulum of 300 kg snaps.
// A drive motor (10 N·m) turning an arm pinned to the ground besides: jammed — said once —, nothing broken.
typedef struct { fude_zoom_scene s; rde_arr born; fude_zoom_placer pl; fude_zoom_mech_plan p; fude_zoom_mech_world w; } strength_rig;

static void strength_begin(strength_rig* r) {
    fude_zoom_scene_init(&r->s, 7);
    r->born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    r->pl = fude_zoom_placer_make(&r->s, r->s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, &r->born);
}

static void strength_run(strength_rig* r, u32 frames) {
    fude_zoom_mech_plan_init(&r->p);
    fude_zoom_mech_world_init(&r->w);
    CHECK(fude_zoom_mech_plan_build(&r->p, &r->s, NULL) > 0u && fude_zoom_mech_world_start(&r->w, &r->p));
    for(u32 f = 0; f < frames; f++) fude_zoom_mech_world_step(&r->w, 1.0 / 60.0);
}

static u32 strength_events(const strength_rig* r, u8 kind) {
    u32 n = 0;
    const fude_zoom_mech_event* ev = (const fude_zoom_mech_event*)r->w.events.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&r->w.events); i++) n += ev[i].kind == kind ? 1u : 0u;
    return n;
}

static void strength_end(strength_rig* r) {
    fude_zoom_mech_world_destroy(&r->w);
    fude_zoom_mech_plan_destroy(&r->p);
    rde_arr_free(&r->born);
    fude_zoom_scene_destroy(&r->s);
}

static void test_strength(void) {
    for(u32 heavy = 0; heavy < 2u; heavy++) {
        strength_rig r;
        strength_begin(&r);
        fude_zoom_placer_part(&r.pl, "fixed pivot", 0, 140, 0, 0, 0, "");   // (its hole 10 over its middle: at 150)
        const u32 link = fude_zoom_placer_part(&r.pl, "link", 0, 150 - 48, -90, 120, 24, "");
        const u32 wt = fude_zoom_placer_part(&r.pl, "weight", 0, 150 - 96, 0, 0, 0, heavy ? "5000 kg" : "1 kg");
        strength_run(&r, 120u);
        const fude_zoom_mech_joint* j = (const fude_zoom_mech_joint*)r.w.joints.memory;
        u32 pins = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&r.w.joints); i++) {
            if(j[i].kind != FUDE_ZOOM_MECH_JOINT_PIN) continue;
            pins++;
            WITHIN("a pin's strength off 980 N (plastic, 7.2 mm)", fabs(j[i].strength - 977.2) / 977.2, 0.01);
        }
        CHECK(pins == 2u);
        const fude_zoom_v2 at = fude_zoom_mech_world_point(&r.w, body_of(&r.w.plan, wt), (fude_zoom_v2){ 0, 150 - 96 });
        if(heavy) {
            CHECK(strength_events(&r, FUDE_ZOOM_MECH_BROKE) >= 1u);
            CHECK(at.y < 150 - 96 - 200);   // (fallen)
        } else {
            CHECK(rde_arr_length(&r.w.events) == 0u);
            CHECK(fabs(at.y - (150 - 96)) < 5.0);   // (hanging where it was)
            for(u32 i = 0; i < (u32)rde_arr_length(&r.w.joints); i++) CHECK(j[i].joint != NULL && j[i].load < 0.05);
        }
        RDE_UNUSED(link);
        strength_end(&r);
    }
    // A rope pendulum: 300 kg on a 2 kN rope, snapped.
    {
        strength_rig r;
        strength_begin(&r);
        fude_zoom_placer_part(&r.pl, "fixed pivot", 0, 100, 0, 0, 0, "");   // (its hole at 110: the rope from it down to the weight)
        fude_zoom_placer_part(&r.pl, "rope", 0, 55, -90, 110.0 / 0.95, 12, "");
        const u32 wt = fude_zoom_placer_part(&r.pl, "weight", 0, 0, 0, 0, 0, "300 kg");
        strength_run(&r, 120u);
        CHECK(strength_events(&r, FUDE_ZOOM_MECH_BROKE) == 1u);
        const fude_zoom_mech_event* ev = (const fude_zoom_mech_event*)r.w.events.memory;
        CHECK(rde_arr_length(&r.w.events) >= 1u && ((const fude_zoom_mech_joint*)r.w.joints.memory)[ev[0].joint].kind == FUDE_ZOOM_MECH_JOINT_ROPE && ev[0].strength == 2000.0);
        CHECK(fude_zoom_mech_world_point(&r.w, body_of(&r.w.plan, wt), (fude_zoom_v2){ 0, 0 }).y < -200.0);
        strength_end(&r);
    }
    // A drive motor's arm pinned to the ground at its other end: jammed, said once; nothing broken (79 N on the pin).
    {
        strength_rig r;
        strength_begin(&r);
        fude_zoom_placer_part(&r.pl, "drive motor", 0, 0, 0, 0, 0, "30 rpm");
        fude_zoom_placer_part(&r.pl, "link", 63, 0, 0, 150, 24, "");
        fude_zoom_placer_part(&r.pl, "fixed pivot", 126, -10, 0, 0, 0, "");
        strength_run(&r, 120u);
        CHECK(strength_events(&r, FUDE_ZOOM_MECH_JAMMED) == 1u);
        CHECK(strength_events(&r, FUDE_ZOOM_MECH_BROKE) == 0u);
        strength_end(&r);
    }
}

// A TRACER (mechrun.h): on the end of a motor's arm (60 rpm, the arm 123 from its axle), its path a circle 123 round
// the axle, the whole of it drawn in a turn and a half, its points a little apart; one on nothing that moves, a point.
static void test_tracer(void) {
    printf("tracer\n");
    strength_rig r;
    strength_begin(&r);
    fude_zoom_placer_part(&r.pl, "drive motor", 0, 0, 0, 0, 0, "60 rpm");
    fude_zoom_placer_part(&r.pl, "link", 63, 0, 0, 150, 24, "");
    const u32 on = fude_zoom_placer_part(&r.pl, "tracer", 123, 0, 0, 0, 0, "");
    const u32 off = fude_zoom_placer_part(&r.pl, "tracer", 300, 300, 0, 0, 0, "");
    strength_run(&r, 90u);
    CHECK(rde_arr_length(&r.w.plan.tracers) == 2u);
    const fude_zoom_mech_tracer* t = (const fude_zoom_mech_tracer*)r.w.plan.tracers.memory;
    u32 ton = 0, toff = 1;
    if(t[0].body != body_of(&r.w.plan, on)) { ton = 1; toff = 0; }
    CHECK(t[ton].body == body_of(&r.w.plan, on) && t[ton].on != FUDE_ZOOM_NONE && t[toff].on == FUDE_ZOOM_NONE);
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    const u32 n = fude_zoom_mech_world_trail(&r.w, ton, &pts);
    const fude_zoom_v2* q = (const fude_zoom_v2*)pts.memory;
    f64 worst = 0.0, turned = 0.0, closest = 1e300;
    for(u32 i = 0; i < n; i++) {
        worst = fmax(worst, fabs(hypot(q[i].x, q[i].y) - 123.0));
        if(i > 0u) {
            f64 d = atan2(q[i].y, q[i].x) - atan2(q[i - 1u].y, q[i - 1u].x);
            d -= 2.0 * 3.14159265358979323846 * floor((d + 3.14159265358979323846) / (2.0 * 3.14159265358979323846));
            turned += d;
            closest = fmin(closest, hypot(q[i].x - q[i - 1u].x, q[i].y - q[i - 1u].y));
        }
    }
    printf("  on the arm: %u points, %.2f turns, %.2f off its circle at worst, %.2f apart at least\n", n, fabs(turned) / (2.0 * 3.14159265358979323846), worst, closest);
    CHECK(n > 40u && worst < 2.0 && fabs(turned) > 2.0 * 3.14159265358979323846 && closest >= 0.1 * r.w.plan.unit - 1e-9);
    CHECK(fude_zoom_mech_world_trail(&r.w, toff, &pts) == 1u);   // (on nothing: where it is, once)
    rde_arr_free(&pts);
    strength_end(&r);
    // The example: its crank's end round a circle 40 about the motor (-55, 0); its coupler's middle a closed curve, well
    // inside the linkage's reach.
    {
        fude_zoom_scene sc; fude_zoom_scene_init(&sc, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        CHECK(fude_zoom_example_build(&sc, sc.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, FUDE_ZOOM_EXAMPLE_COUPLER_CURVE, NULL, &born) > 3u);
        fude_zoom_mech_plan p; fude_zoom_mech_plan_init(&p);
        fude_zoom_mech_world w; fude_zoom_mech_world_init(&w);
        CHECK(fude_zoom_mech_plan_build(&p, &sc, NULL) > 0u && fude_zoom_mech_world_start(&w, &p) && rde_arr_length(&p.tracers) == 2u);
        for(u32 f = 0; f < 60u * 7u; f++) fude_zoom_mech_world_step(&w, 1.0 / 60.0);   // (20 rpm: two turns and more)
        rde_arr q = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
        const fude_zoom_mech_tracer* tr = (const fude_zoom_mech_tracer*)w.plan.tracers.memory;
        for(u32 k = 0; k < 2u; k++) {
            const u32 n = fude_zoom_mech_world_trail(&w, k, &q);
            const fude_zoom_v2* v = (const fude_zoom_v2*)q.memory;
            f64 worst = 0.0, x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
            for(u32 i = 0; i < n; i++) {
                worst = fmax(worst, fabs(hypot(v[i].x + 55.0, v[i].y) - 40.0));
                x0 = fmin(x0, v[i].x); x1 = fmax(x1, v[i].x); y0 = fmin(y0, v[i].y); y1 = fmax(y1, v[i].y);
            }
            const b8 crank = fabs(tr[k].at.x + 15.0) < 1e-6;
            printf("  %s: %u points, x %.0f to %.0f, y %.0f to %.0f%s\n", crank ? "crank's end" : "coupler's middle", n, x0, x1, y0, y1, crank ? "" : " (its curve)");
            CHECK(n > 30u);
            if(crank) {
                CHECK(worst < 1.5);
            } else {
                CHECK(x1 - x0 > 20.0 && y1 - y0 > 10.0 && x0 > -80.0 && x1 < 80.0 && y0 > -20.0 && y1 < 120.0);
            }
        }
        rde_arr_free(&q);
        fude_zoom_mech_world_destroy(&w); fude_zoom_mech_plan_destroy(&p);
        rde_arr_free(&born); fude_zoom_scene_destroy(&sc);
    }
}

// BY HAND (mechrun.h). A weight on a link from a pivot, taken hold of and dragged out to the side: there (the link swung
// out with it) within a second; let go, it swings back down. Nothing taken on empty space or on a wall.
static void test_grab(void) {
    printf("grab\n");
    strength_rig r;
    strength_begin(&r);
    fude_zoom_placer_part(&r.pl, "fixed pivot", 0, -10, 0, 0, 0, "");   // (its hole 10 over its middle: at 0, 0)
    fude_zoom_placer_part(&r.pl, "link", 0, -48, -90, 120, 24, "");
    const u32 wt = fude_zoom_placer_part(&r.pl, "weight", 0, -96, 0, 0, 0, "1 kg");
    fude_zoom_placer_part(&r.pl, "wall", 300, -300, 0, 100, 20, "");
    strength_run(&r, 30u);
    CHECK(!fude_zoom_mech_world_grab(&r.w, (fude_zoom_v2){ 200, 200 }));   // (nothing there)
    CHECK(!fude_zoom_mech_world_grab(&r.w, (fude_zoom_v2){ 300, -300 }));  // (a wall: it does not move)
    const fude_zoom_v2 at = fude_zoom_mech_world_point(&r.w, body_of(&r.w.plan, wt), (fude_zoom_v2){ 0, -96 });
    CHECK(fude_zoom_mech_world_grab(&r.w, at));
    // (out to 45°, on the weight's circle round the pivot: 106 from it)
    const f64 rad = hypot(at.x, at.y);
    const fude_zoom_v2 to = { rad * sin(0.785398), -rad * cos(0.785398) };
    fude_zoom_mech_world_drag(&r.w, to);
    for(u32 f = 0; f < 60u; f++) fude_zoom_mech_world_step(&r.w, 1.0 / 60.0);
    fude_zoom_v2 held;
    CHECK(fude_zoom_mech_world_grabbed(&r.w, &held));
    printf("  held out to 45 degrees: %.1f off where it is dragged\n", hypot(held.x - to.x, held.y - to.y));
    CHECK(hypot(held.x - to.x, held.y - to.y) < 6.0);
    fude_zoom_mech_world_let_go(&r.w);
    CHECK(!fude_zoom_mech_world_grabbed(&r.w, &held));
    for(u32 f = 0; f < 20u; f++) fude_zoom_mech_world_step(&r.w, 1.0 / 60.0);
    const fude_zoom_v2 now = fude_zoom_mech_world_point(&r.w, body_of(&r.w.plan, wt), (fude_zoom_v2){ 0, -96 });
    CHECK(now.x < to.x - 10.0);   // (swinging back down)
    strength_end(&r);
}

// A HAND CRANK: on its own axle; held and turned half a turn, then two turns, it follows the hand (a tracer on its handle
// round a circle 21 about its axle); let go, it stays (balanced on its axle). Found where its disc is, not elsewhere.
static void test_crank(void) {
    printf("hand crank\n");
    strength_rig r;
    strength_begin(&r);
    const u32 cr = fude_zoom_placer_part(&r.pl, "hand crank", 0, 0, 0, 60, 60, "");
    fude_zoom_placer_part(&r.pl, "tracer", 21, 0, 0, 0, 0, "");
    strength_run(&r, 10u);
    CHECK(rde_arr_length(&r.w.cranks) == 1u && ((const fude_zoom_mech_crank*)r.w.cranks.memory)[0].body == body_of(&r.w.plan, cr));
    CHECK(fude_zoom_mech_world_crank_at(&r.w, (fude_zoom_v2){ 5, 5 }) == 0u && fude_zoom_mech_world_crank_at(&r.w, (fude_zoom_v2){ 100, 0 }) == FUDE_ZOOM_NONE);
    const f64 pi = 3.14159265358979323846;
    fude_zoom_mech_world_crank_hold(&r.w, 0u, pi);
    for(u32 f = 0; f < 45u; f++) fude_zoom_mech_world_step(&r.w, 1.0 / 60.0);
    const f64 half = fude_zoom_mech_world_crank_angle(&r.w, 0u);
    fude_zoom_mech_world_crank_hold(&r.w, 0u, 4.0 * pi);
    for(u32 f = 0; f < 90u; f++) fude_zoom_mech_world_step(&r.w, 1.0 / 60.0);
    const f64 two = fude_zoom_mech_world_crank_angle(&r.w, 0u);
    fude_zoom_mech_world_crank_let_go(&r.w, 0u);
    for(u32 f = 0; f < 60u; f++) fude_zoom_mech_world_step(&r.w, 1.0 / 60.0);
    const f64 after = fude_zoom_mech_world_crank_angle(&r.w, 0u);
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    const u32 n = fude_zoom_mech_world_trail(&r.w, 0u, &pts);
    f64 worst = 0.0;
    for(u32 i = 0; i < n; i++) worst = fmax(worst, fabs(hypot(((const fude_zoom_v2*)pts.memory)[i].x, ((const fude_zoom_v2*)pts.memory)[i].y) - 21.0));
    rde_arr_free(&pts);
    printf("  turned to %.3f (half a turn), %.3f (two turns), %.3f a second after it was let go; its handle %.2f off its circle\n", half / pi, two / (2.0 * pi), after / (2.0 * pi), worst);
    CHECK(fabs(half - pi) < 0.05 && fabs(two - 4.0 * pi) < 0.05 && fabs(after - two) < 0.3 && worst < 0.5 && n > 20u);
    // Let go while turning (its axle's friction stopping it, and it staying): turned a quarter in a sixth of a second —
    // on a few degrees —; flicked round as fast as a hand turns it — on under a turn.
    f64 on[2], more[2];
    for(u32 k = 0; k < 2u; k++) {
        const f64 from = fude_zoom_mech_world_crank_angle(&r.w, 0u);
        for(u32 f = 0; f < 10u; f++) {
            fude_zoom_mech_world_crank_hold(&r.w, 0u, k == 0u ? from - 0.5 * pi * (f64)(f + 1u) / 10.0 : from + 40.0 * pi);
            fude_zoom_mech_world_step(&r.w, 1.0 / 60.0);
        }
        const f64 at = fude_zoom_mech_world_crank_angle(&r.w, 0u);
        fude_zoom_mech_world_crank_let_go(&r.w, 0u);
        for(u32 f = 0; f < 30u; f++) fude_zoom_mech_world_step(&r.w, 1.0 / 60.0);
        const f64 stopped = fude_zoom_mech_world_crank_angle(&r.w, 0u);
        for(u32 f = 0; f < 30u; f++) fude_zoom_mech_world_step(&r.w, 1.0 / 60.0);
        on[k] = stopped - at;
        more[k] = fude_zoom_mech_world_crank_angle(&r.w, 0u) - stopped;
        if(k == 1u) CHECK(at - from > 2.0 * pi);   // (flicked: more than a turn in a sixth of a second)
    }
    printf("  let go turning: a quarter turn on %.1f degrees, flicked on %.2f turns; then %.4f, %.4f rad more\n", on[0] * 180.0 / pi, on[1] / (2.0 * pi), more[0], more[1]);
    CHECK(fabs(on[0]) < 10.0 * pi / 180.0 && fabs(on[1]) < 2.0 * pi && fabs(more[0]) < 1e-3 && fabs(more[1]) < 1e-3);
    strength_end(&r);
    // The example: its crank turned by hand two turns, its rocker rocking (its coupler's tracer drawing a closed curve);
    // its pendulum's weight taken hold of.
    {
        fude_zoom_scene sc; fude_zoom_scene_init(&sc, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        CHECK(fude_zoom_example_build(&sc, sc.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, FUDE_ZOOM_EXAMPLE_BY_HAND, NULL, &born) > 3u);
        fude_zoom_mech_plan p; fude_zoom_mech_plan_init(&p);
        fude_zoom_mech_world w; fude_zoom_mech_world_init(&w);
        CHECK(fude_zoom_mech_plan_build(&p, &sc, NULL) > 0u && fude_zoom_mech_world_start(&w, &p));
        CHECK(rde_arr_length(&w.cranks) == 1u && rde_arr_length(&w.plan.tracers) == 1u);
        for(u32 f = 0; f < 30u; f++) fude_zoom_mech_world_step(&w, 1.0 / 60.0);
        for(u32 f = 0; f < 240u; f++) {
            fude_zoom_mech_world_crank_hold(&w, 0u, 4.0 * pi * (f64)(f + 1u) / 240.0);   // (two turns in 4 s, the hand going round)
            fude_zoom_mech_world_step(&w, 1.0 / 60.0);
        }
        for(u32 f = 0; f < 30u; f++) fude_zoom_mech_world_step(&w, 1.0 / 60.0);
        rde_arr q = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
        const u32 n = fude_zoom_mech_world_trail(&w, 0u, &q);
        const fude_zoom_v2* v = (const fude_zoom_v2*)q.memory;
        f64 x0 = 1e300, x1 = -1e300;
        for(u32 i = 0; i < n; i++) { x0 = fmin(x0, v[i].x); x1 = fmax(x1, v[i].x); }
        printf("  by hand: the crank %.2f turns, its coupler's curve %u points %.0f wide\n", fude_zoom_mech_world_crank_angle(&w, 0u) / (2.0 * pi), n, x1 - x0);
        CHECK(fabs(fude_zoom_mech_world_crank_angle(&w, 0u) - 4.0 * pi) < 0.1 && n > 40u && x1 - x0 > 15.0);
        CHECK(fude_zoom_mech_world_grab(&w, (fude_zoom_v2){ 250, -20 }));   // (its weight)
        rde_arr_free(&q);
        fude_zoom_mech_world_destroy(&w); fude_zoom_mech_plan_destroy(&p);
        rde_arr_free(&born); fude_zoom_scene_destroy(&sc);
    }
}

// The mechanisms' examples made to show what their parts take: materials (the ice block furthest down its ramp, steel
// and wood after it, rubber where it was; the rubber ball bouncing highest), too heavy (only the 500 kg pin sheared, only
// the 300 kg rope snapped), a motor strong enough (the 2 N·m one jammed, the 30 N·m one round and round), servo angles
// (45°, 90°, 135°; the one on 12 V burnt).
static void test_examples_go_wrong_mech(void) {
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_example_build(&s, s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, FUDE_ZOOM_EXAMPLE_MATERIALS, NULL, &born);
        fude_zoom_mech_plan p; fude_zoom_mech_plan_init(&p);
        fude_zoom_mech_world w; fude_zoom_mech_world_init(&w);
        CHECK(fude_zoom_mech_plan_build(&p, &s, NULL) > 0u && fude_zoom_mech_world_start(&w, &p));
        // (the moving drawn bodies in the order drawn: the four blocks, then the three balls)
        u32 moving[7], nm = 0;
        const fude_zoom_mech_body* b = (const fude_zoom_mech_body*)w.plan.bodies.memory;
        for(u32 i = 0; i < (u32)rde_arr_length(&w.plan.bodies) && nm < 7u; i++) if(!b[i].fixed) moving[nm++] = i;
        CHECK(nm == 7u);
        f64 top[3] = { -1e9, -1e9, -1e9 };
        for(u32 f = 0; f < 240u && nm == 7u; f++) {
            fude_zoom_mech_world_step(&w, 1.0 / 60.0);
            for(u32 k = 0; k < 3u; k++) {
                const fude_zoom_v2 at = fude_zoom_mech_world_point(&w, moving[4u + k], b[moving[4u + k]].at);
                if(f > 40u) top[k] = fmax(top[k], at.y);   // (after the first bounce: how high it comes back up)
            }
        }
        if(nm == 7u) {
            f64 went[4];
            for(u32 k = 0; k < 4u; k++) {
                const fude_zoom_v2 at = fude_zoom_mech_world_point(&w, moving[k], b[moving[k]].at);
                went[k] = hypot(at.x - b[moving[k]].at.x, at.y - b[moving[k]].at.y);
            }
            printf("  materials: ice %.0f, wood %.0f, rubber %.0f, steel %.0f mm down; balls back up to %.0f, %.0f, %.0f\n", went[0], went[1], went[2], went[3], top[0], top[1], top[2]);
            CHECK(went[0] > went[3] && went[3] > went[2] && went[1] > went[2] && went[2] < 10.0 && went[0] > 300.0);
            CHECK(top[0] > top[1] + 30.0 && top[0] > top[2] + 30.0);
        }
        CHECK(rde_arr_length(&w.events) == 0u);
        fude_zoom_mech_world_destroy(&w); fude_zoom_mech_plan_destroy(&p);
        rde_arr_free(&born); fude_zoom_scene_destroy(&s);
    }
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_example_build(&s, s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, FUDE_ZOOM_EXAMPLE_TOO_HEAVY, NULL, &born);
        fude_zoom_mech_plan p; fude_zoom_mech_plan_init(&p);
        fude_zoom_mech_world w; fude_zoom_mech_world_init(&w);
        CHECK(fude_zoom_mech_plan_build(&p, &s, NULL) > 0u && fude_zoom_mech_world_start(&w, &p));
        for(u32 f = 0; f < 120u; f++) fude_zoom_mech_world_step(&w, 1.0 / 60.0);
        const fude_zoom_mech_joint* j = (const fude_zoom_mech_joint*)w.joints.memory;
        const fude_zoom_mech_event* ev = (const fude_zoom_mech_event*)w.events.memory;
        u32 pins = 0, ropes = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&w.events); i++) {
            CHECK(ev[i].kind == FUDE_ZOOM_MECH_BROKE);
            const fude_zoom_mech_joint* r = &j[ev[i].joint];
            pins  += r->kind == FUDE_ZOOM_MECH_JOINT_PIN && fabs(r->at.x - (-420.0 + 320.0)) < 30.0 ? 1u : 0u;   // (the third link's: 500 kg)
            ropes += r->kind == FUDE_ZOOM_MECH_JOINT_ROPE && r->at.x > 200.0 ? 1u : 0u;                          // (the second rope's: 300 kg)
        }
        printf("  too heavy: %u broken (a pin %u, a rope %u)\n", (u32)rde_arr_length(&w.events), pins, ropes);
        CHECK(rde_arr_length(&w.events) == 3u && pins == 2u && ropes == 1u);   // (both the 500 kg link's pins: each past its 980 N)
        fude_zoom_mech_world_destroy(&w); fude_zoom_mech_plan_destroy(&p);
        rde_arr_free(&born); fude_zoom_scene_destroy(&s);
    }
    {
        fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_example_build(&s, s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, FUDE_ZOOM_EXAMPLE_MOTOR_TORQUE, NULL, &born);
        fude_zoom_mech_plan p; fude_zoom_mech_plan_init(&p);
        fude_zoom_mech_world w; fude_zoom_mech_world_init(&w);
        CHECK(fude_zoom_mech_plan_build(&p, &s, NULL) > 0u && fude_zoom_mech_world_start(&w, &p));
        const fude_zoom_mech_motor* mo = (const fude_zoom_mech_motor*)w.motors.memory;
        CHECK(rde_arr_length(&w.motors) == 2u);
        f64 turn0 = 0.0, turn1 = 0.0;
        for(u32 f = 0; f < 300u && rde_arr_length(&w.motors) == 2u; f++) {
            fude_zoom_mech_world_step(&w, 1.0 / 60.0);
            turn0 = fabs(mo[0].turned);
            turn1 = fabs(mo[1].turned);
        }
        u32 jams = 0;
        const fude_zoom_mech_event* ev = (const fude_zoom_mech_event*)w.events.memory;
        for(u32 i = 0; i < (u32)rde_arr_length(&w.events); i++) jams += ev[i].kind == FUDE_ZOOM_MECH_JAMMED && ev[i].joint == mo[0].pin ? 1u : 0u;
        printf("  motor strong enough: weak %.2f turns (jammed %u), strong %.2f turns in 5 s\n", turn0 / (2.0 * PI), jams, turn1 / (2.0 * PI));
        CHECK(jams == 1u && turn0 < 0.5 * 2.0 * PI && turn1 > 1.5 * 2.0 * PI && rde_arr_length(&w.events) == 1u);
        fude_zoom_mech_world_destroy(&w); fude_zoom_mech_plan_destroy(&p);
        rde_arr_free(&born); fude_zoom_scene_destroy(&s);
    }
    {
        fude_zoom_scene sc; fude_zoom_scene_init(&sc, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_example_build(&sc, sc.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, FUDE_ZOOM_EXAMPLE_SERVO_ANGLES, NULL, &born);
        played x;
        play_begin(&x, &sc);
        CHECK(x.coupled == 4u);
        play_frames(&x, 90u);
        const fude_zoom_circuit_part* cp = (const fude_zoom_circuit_part*)x.c.parts.memory;
        f64 angles[4];
        u32 ns = 0, burnt = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&x.c.parts); i++) {
            if(cp[i].part->model != FUDE_ZOOM_MODEL_SERVO) continue;
            if(ns < 4u) angles[ns] = cp[i].shown;
            burnt += cp[i].burnt ? 1u : 0u;
            ns++;
        }
        CHECK(ns == 4u && burnt == 1u);
        if(ns == 4u) {
            printf("  servo angles: %.1f, %.1f, %.1f (and %.1f, burnt)\n", angles[0], angles[1], angles[2], angles[3]);
            CHECK(fabs(angles[0] - 45.0) < 2.5 && fabs(angles[1] - 90.0) < 2.5 && fabs(angles[2] - 135.0) < 2.5 && cp != NULL);
        }
        play_end(&x);
        rde_arr_free(&born);
        fude_zoom_scene_destroy(&sc);
    }
}

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

// A part placed in a rig of its own (its scene, its placer).
typedef struct { fude_zoom_scene s; rde_arr born; fude_zoom_placer pl; } rig;

static void rig_begin(rig* r) {
    fude_zoom_scene_init(&r->s, 7);
    r->born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    r->pl = fude_zoom_placer_make(&r->s, r->s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, &r->born);
}

static void rig_end(rig* r) {
    rde_arr_free(&r->born);
    fude_zoom_scene_destroy(&r->s);
}

static u32 rig_part(rig* r, const c8* id, f64 x, f64 y, f64 turn, f64 w, f64 h, const c8* text) {
    return fude_zoom_placer_part(&r->pl, id, x, y, turn, w, h, text);
}

static void rig_wire(rig* r, u32 a, u32 pa, u32 b, u32 pb) { fude_zoom_placer_wire(&r->pl, a, pa, b, pb, false); }

static u32 circuit_index(played* x, u32 o) {
    const fude_zoom_circuit_part* p = (const fude_zoom_circuit_part*)x->c.parts.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&x->c.parts); i++) if(p[i].object == o) return i;
    return FUDE_ZOOM_NONE;
}

// A pin of a part pulled up to 5 V through 10k (its rail placed by it), or tied to ground.
static void rig_pull_up(rig* r, u32 part, u32 pin, f64 dx, f64 dy) {
    const u32 res  = rig_part(r, "resistor", dx, dy, 0, 0, 0, "10k");
    const u32 rail = rig_part(r, "supply rail", dx - 80.0, dy + 40.0, 0, 0, 0, "5V");
    rig_wire(r, part, pin, res, 1);
    rig_wire(r, res, 0, rail, 0);
}

static void rig_ground(rig* r, u32 part, u32 pin, f64 x, f64 y) {
    const u32 g = rig_part(r, "ground", x, y, 0, 0, 0, "");
    rig_wire(r, part, pin, g, 0);
}

// A SOLENOID (coupling.h): its plunger a body of the mechanism, along its coil — its stroke 0.3 of its half width in,
// its end's hole a link's hinge. 12 V through a switch: in, all the way, in a fraction of a second, carrying the link
// pinned on its end 15 along with it; off, its spring pushes it out again. A push button at its end: pressed while it is
// out, let go while it is in (a limit switch).
static void test_solenoid(void) {
    rig r; rig_begin(&r);
    const u32 so  = rig_part(&r, "solenoid", 0, 0, 0, 0, 0, "12V");
    const u32 src = rig_part(&r, "DC source", -250, 0, 0, 0, 0, "12V");
    const u32 sw  = rig_part(&r, "SPST switch", -150, 80, 0, 0, 0, "on");
    rig_wire(&r, src, 0, sw, 0);
    rig_wire(&r, sw, 1, so, 0);
    rig_wire(&r, so, 1, src, 1);
    rig_ground(&r, src, 1, -250, -120);
    const u32 arm = rig_part(&r, "link", 42.5 + 63.0, 0, 0, 150, 24, "");   // (its left hole on the plunger's end)
    const u32 btn = rig_part(&r, "push button", 46, -150, 0, 0, 0, "");
    {
        // (the button's middle where the plunger's end is out — moved there after its wires: its own circuit's)
        rig_pull_up(&r, btn, 0, -60, -150);
        const u32 pd = rig_part(&r, "resistor", 160, -150, 0, 0, 0, "1k");
        rig_wire(&r, btn, 1, pd, 0);
        rig_ground(&r, pd, 1, 220, -200);
    }
    played x;
    play_begin(&x, &r.s);
    const u32 pb = body_of(&x.w.plan, so);
    CHECK(pb != FUDE_ZOOM_NONE && ((const fude_zoom_mech_body*)x.w.plan.bodies.memory)[pb].part->kind == FUDE_ZOOM_MECH_PLUNGER);
    {
        const fude_zoom_mech_slide* sl = (const fude_zoom_mech_slide*)x.w.plan.slides.memory;
        b8 found = false;
        for(u32 i = 0; i < (u32)rde_arr_length(&x.w.plan.slides); i++) {
            if(sl[i].body == pb) found = fabs(sl[i].lower + 15.0) < 1e-9 && sl[i].upper == 0.0 && fabs(sl[i].axis.x - 1.0) < 1e-12;
        }
        CHECK(found);
        const fude_zoom_mech_hinge* h = (const fude_zoom_mech_hinge*)x.w.plan.hinges.memory;
        b8 pinned = false;
        for(u32 i = 0; i < (u32)rde_arr_length(&x.w.plan.hinges); i++) {
            pinned = pinned || ((h[i].a == pb || h[i].b == pb) && (h[i].a == body_of(&x.w.plan, arm) || h[i].b == body_of(&x.w.plan, arm)) && fabs(h[i].at.x - 42.5) < 1.0);
        }
        CHECK(pinned);
    }
    const u32 pl = fude_zoom_mech_world_plunger(&x.w, so);
    CHECK(pl != FUDE_ZOOM_NONE && part_of_object(&x, so)->state[3] == 1.0);
    // (the button: moved under the plunger's end, its circuit's wires left where they were — joined by its pins)
    CHECK(x.coupled >= 2u);
    const fude_zoom_v2 tip0 = fude_zoom_mech_world_point(&x.w, body_of(&x.w.plan, arm), (fude_zoom_v2){ 42.5, 0.0 });
    play_frames(&x, 30u);
    const double in = fude_zoom_mech_world_plunger_in(&x.w, pl);
    const fude_zoom_v2 tip1 = fude_zoom_mech_world_point(&x.w, body_of(&x.w.plan, arm), (fude_zoom_v2){ 42.5, 0.0 });
    printf("  solenoid on: its plunger %.3f in, the link's end moved %.2f along (%.2f across)\n", in, tip1.x - tip0.x, tip1.y - tip0.y);
    WITHIN("solenoid on: its plunger short of all the way in", 1.0 - in, 0.02);
    WITHIN("solenoid on: its circuit's plunger off the world's", fabs(part_of_object(&x, so)->state[2] - in), 1e-9);
    WITHIN("solenoid on: the link's end off 15 in", fabs(tip1.x - tip0.x + 15.0), 0.5);
    WITHIN("solenoid on: its current off 0.5 A", fabs(part_of_object(&x, so)->state[0] - 0.5), 0.01);
    c8 say[16];
    CHECK(fude_zoom_circuit_tap(&x.c, circuit_index(&x, sw), -1, say, sizeof say));
    play_frames(&x, 60u);
    const double out = fude_zoom_mech_world_plunger_in(&x.w, pl);
    printf("  solenoid off: its plunger %.3f in\n", out);
    WITHIN("solenoid off: its plunger still in", out, 0.02);
    CHECK(x.ok);
    play_end(&x);
    rig_end(&r);
    // A limit switch at its end: a button whose middle is where the plunger's end is out (45 along).
    {
        rig q; rig_begin(&q);
        const u32 so2  = rig_part(&q, "solenoid", 0, 0, 0, 0, 0, "12V");
        const u32 src2 = rig_part(&q, "DC source", -250, 0, 0, 0, 0, "12V");
        const u32 sw2  = rig_part(&q, "SPST switch", -150, 80, 0, 0, 0, "off");
        rig_wire(&q, src2, 0, sw2, 0);
        rig_wire(&q, sw2, 1, so2, 0);
        rig_wire(&q, so2, 1, src2, 1);
        rig_ground(&q, src2, 1, -250, -120);
        const u32 b2 = rig_part(&q, "push button", 45, 0, 90, 0, 0, "");   // (upright: its pins over and under the rod)
        rig_pull_up(&q, b2, 1, 200, 120);
        const u32 pd = rig_part(&q, "resistor", 120, -120, 0, 0, 0, "1k");
        rig_wire(&q, b2, 0, pd, 0);
        rig_ground(&q, pd, 1, 200, -170);
        played y;
        play_begin(&y, &q.s);
        play_frames(&y, 10u);
        CHECK(part_of_object(&y, b2)->pushed);
        CHECK(fude_zoom_circuit_tap(&y.c, circuit_index(&y, sw2), -1, say, sizeof say));
        play_frames(&y, 30u);
        CHECK(!part_of_object(&y, b2)->pushed);
        CHECK(fude_zoom_circuit_tap(&y.c, circuit_index(&y, sw2), -1, say, sizeof say));
        play_frames(&y, 60u);
        CHECK(part_of_object(&y, b2)->pushed && y.ok && rde_arr_length(&y.w.events) == 0u);   // (slammed in and out: nothing broken)
        play_end(&y);
        rig_end(&q);
    }
}

// A STEPPER (coupling.h, "200 steps": 1.8° a step) with a gear on its shaft, COM on 5 V, each coil to ground through a
// switch: A, B, C, D, A on in turn, the gear a step on each time — four steps, 7.2° —, its circuit's steps its shaft's;
// back one (D): a step back.
static void test_stepper(void) {
    rig r; rig_begin(&r);
    const u32 st = rig_part(&r, "stepper motor", 0, 0, 0, 0, 0, "200 steps");
    const u32 gear = rig_part(&r, "gear 20T", 0, 0, 0, 0, 0, "");
    const u32 rail = rig_part(&r, "supply rail", -200, 120, 0, 0, 0, "5V");
    rig_wire(&r, rail, 0, st, 4);
    u32 sw[4];
    for(u32 k = 0; k < 4u; k++) {
        sw[k] = rig_part(&r, "SPST switch", -260.0 - 80.0 * (f64)k, 80.0 - 40.0 * (f64)k, 0, 0, 0, "off");
        rig_wire(&r, st, k, sw[k], 1);
        rig_ground(&r, sw[k], 0, -320.0 - 80.0 * (f64)k, 20.0 - 40.0 * (f64)k);
    }
    played x;
    play_begin(&x, &r.s);
    CHECK(fude_zoom_mech_world_shaft(&x.w, st) != FUDE_ZOOM_NONE && part_of_object(&x, st)->state[7] == 0.0);
    runner rn = runner_begin(&x.w, body_of(&x.w.plan, gear));
    const u32 order[6] = { 0, 1, 2, 3, 0, 3 };
    const double want[6] = { 0, 1, 2, 3, 4, 3 };
    u32 on = FUDE_ZOOM_NONE;
    c8 say[16];
    for(u32 k = 0; k < 6u; k++) {
        if(on != FUDE_ZOOM_NONE) CHECK(fude_zoom_circuit_tap(&x.c, circuit_index(&x, sw[on]), -1, say, sizeof say));
        CHECK(fude_zoom_circuit_tap(&x.c, circuit_index(&x, sw[order[k]]), -1, say, sizeof say));
        on = order[k];
        for(u32 f = 0; f < 15u; f++) { play_frames(&x, 1u); runner_track(&x.w, &rn); }
        const double steps = part_of_object(&x, st)->state[2];
        printf("  stepper, coil %c: the gear at %.3f°, %.3f steps\n", "ABCD"[order[k]], rn.turned * 180.0 / PI, steps);
        WITHIN("stepper: the gear's turn off its steps' (deg)", fabs(rn.turned * 180.0 / PI - want[k] * 1.8), 0.1);
        WITHIN("stepper: its steps off", fabs(steps - want[k]), 0.05);
    }
    CHECK(x.ok);
    play_end(&x);
    rig_end(&r);
}

// Sensors on shafts (coupling.h), a drive motor (30 rpm) turning a gear on each: a ROTARY ENCODER's position its shaft's
// turn's — 20 detents a turn, clockwise on —, its A pulsing as often; a POT's wiper as far round (270° end to end) and
// held at its end past it. A SLOTTED SENSOR an arm sweeps through (60 rpm): blocked each turn, its C high while it is.
static void test_shaft_sensors(void) {
    {
        rig r; rig_begin(&r);
        rig_part(&r, "drive motor", 0, 0, 0, 0, 0, "30 rpm");
        rig_part(&r, "gear 20T", 0, 0, 0, 0, 0, "");
        const u32 en = rig_part(&r, "rotary encoder", 0, 0, 0, 0, 0, "20");
        rig_pull_up(&r, en, 0, -200, 80);
        rig_pull_up(&r, en, 2, -200, -80);
        rig_ground(&r, en, 1, -120, -40);
        played x;
        play_begin(&x, &r.s);
        CHECK(rde_arr_length(&x.k.encoders) == 1u && part_of_object(&x, en)->state[7] == 0.0);
        CHECK(worked_by_mechanism(&x, en));   // (a touch on it goes to what is on its shaft)
        u32 falls = 0;
        b8 was = true;
        for(u32 f = 0; f < 120u; f++) {
            play_frames(&x, 1u);
            const fude_zoom_circuit_part* p = part_of_object(&x, en);
            const b8 high = fude_zoom_circuit_volts(&x.c, p->node[0]) > 2.5;
            falls += was && !high ? 1u : 0u;
            was = high;
        }
        const fude_zoom_coupled_shaft* sh = (const fude_zoom_coupled_shaft*)x.k.encoders.memory;
        const double pos = part_of_object(&x, en)->state[0];
        printf("  encoder: its shaft %.3f turns, at %.3f detents, A fell %u times\n", sh[0].turned / (2.0 * PI), pos, falls);
        WITHIN("encoder: its position off its shaft's turn", fabs(pos + sh[0].turned * 20.0 / (2.0 * PI)), 1e-9);
        CHECK(fabs(sh[0].turned) > 1.5 * PI);
        WITHIN("encoder: A's pulses off its detents", fabs((double)falls - floor(fabs(pos))), 1.0);
        CHECK(x.ok);
        play_end(&x);
        rig_end(&r);
    }
    {
        rig r; rig_begin(&r);
        rig_part(&r, "drive motor", 0, 0, 0, 0, 0, "30 rpm");
        rig_part(&r, "gear 20T", 0, 0, 0, 0, 0, "");
        const u32 pot = rig_part(&r, "potentiometer", 0, 0, 0, 0, 0, "10k 50%");
        const u32 rail = rig_part(&r, "supply rail", -200, 60, 0, 0, 0, "5V");
        rig_wire(&r, rail, 0, pot, 0);
        rig_ground(&r, pot, 1, 150, -60);
        const u32 free_pot = rig_part(&r, "potentiometer", 400, 300, 0, 0, 0, "10k 50%");   // (on no shaft: tapped)
        played x;
        play_begin(&x, &r.s);
        CHECK(rde_arr_length(&x.k.pots) == 1u);
        CHECK(worked_by_mechanism(&x, pot) && !worked_by_mechanism(&x, free_pot) && !worked_by_mechanism(&x, rail));
        double worst = 0.0;
        for(u32 f = 0; f < 90u; f++) {
            play_frames(&x, 1u);
            const fude_zoom_coupled_shaft* sh = (const fude_zoom_coupled_shaft*)x.k.pots.memory;
            const double want = fmin(fmax(0.5 - sh[0].turned / (1.5 * PI), 0.0), 1.0);
            worst = fmax(worst, fabs(part_of_object(&x, pot)->state[0] - want));
        }
        const double end = part_of_object(&x, pot)->state[0];
        printf("  pot on a shaft: its wiper at %.3f after 1.5 s\n", end);
        WITHIN("pot on a shaft: its wiper off its shaft's turn", worst, 1e-9);
        CHECK(end == 0.0 || end == 1.0);   // (past its end: held there)
        CHECK(x.ok);
        play_end(&x);
        rig_end(&r);
    }
    {
        rig r; rig_begin(&r);
        rig_part(&r, "drive motor", 0, 0, 0, 0, 0, "60 rpm");
        rig_part(&r, "link", 63, 0, 0, 150, 24, "");
        const u32 sl = rig_part(&r, "slotted sensor", 60, -9, 0, 0, 0, "ITR9608");   // (its beam 0.3 of its half height over its middle)
        const u32 led_r = rig_part(&r, "resistor", -100, 160, 0, 0, 0, "330");
        const u32 rail = rig_part(&r, "supply rail", -180, 200, 0, 0, 0, "5V");
        rig_wire(&r, rail, 0, led_r, 0);
        rig_wire(&r, led_r, 1, sl, 0);
        rig_ground(&r, sl, 1, -40, -120);
        rig_pull_up(&r, sl, 2, 260, 80);
        rig_ground(&r, sl, 3, 200, -120);
        played x;
        play_begin(&x, &r.s);
        CHECK(rde_arr_length(&x.k.slots) == 1u && worked_by_mechanism(&x, sl) && !worked_by_mechanism(&x, led_r));
        play_frames(&x, 1u);
        CHECK(part_of_object(&x, sl)->state[0] == 1.0);   // (the arm in it as it starts)
        u32 blocks = 0, wrong = 0;
        double was = 1.0;
        for(u32 f = 0; f < 150u; f++) {
            play_frames(&x, 1u);
            const fude_zoom_circuit_part* p = part_of_object(&x, sl);
            blocks += p->state[0] > was ? 1u : 0u;
            was = p->state[0];
            const double vc = fude_zoom_circuit_volts(&x.c, p->node[2]);
            wrong += (p->state[0] > 0.5) != (vc > 2.5) ? 1u : 0u;
        }
        printf("  slotted sensor: blocked again %u times in 2.5 s (60 rpm), its C wrong %u frames\n", blocks, wrong);
        CHECK(blocks >= 1u && blocks <= 3u && wrong == 0u);
        CHECK(x.ok);
        play_end(&x);
        rig_end(&r);
    }
}

// The examples of batch 0.1.73 (examples.h), played. SOLENOID: each latch's button held — its plunger in, its bolt
// (the slider) drawn back 15 —, let go: back out; the one with a diode across its coil quiet, the one without sparking.
// STEPPER: its clock's ticks its steps (200 a second: about 400 in 2 s), the arm on its shaft as far round. SHAFT
// SENSORS: the encoder's crank turned a turn counter-clockwise by hand — 20 detents back, its two LEDs lit in turn —,
// the pot's a quarter turn clockwise — its wiper a third on (270° end to end), the meter as much less —, the arm's turns through
// the slot toggling the T flip-flop's probe.
static u32 example_parts(played* x, u8 model, u32* out, u32 most) {
    u32 n = 0;
    const fude_zoom_circuit_part* p = (const fude_zoom_circuit_part*)x->c.parts.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&x->c.parts); i++) if(p[i].part->model == model && n < most) out[n++] = i;
    return n;
}

static u32 events_of(played* x, u8 kind) {
    u32 n = 0;
    for(u32 i = 0; i < (u32)rde_arr_length(&x->c.events); i++) n += ((const fude_zoom_circuit_event*)x->c.events.memory)[i].kind == kind ? 1u : 0u;
    return n;
}

static void test_new_examples(void) {
    c8 say[16];
    {
        fude_zoom_scene sc; fude_zoom_scene_init(&sc, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_example_build(&sc, sc.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, FUDE_ZOOM_EXAMPLE_SOLENOID, NULL, &born);
        played x;
        play_begin(&x, &sc);
        u32 so[2], btn[2];
        CHECK(example_parts(&x, FUDE_ZOOM_MODEL_SOLENOID, so, 2u) == 2u && example_parts(&x, FUDE_ZOOM_MODEL_BUTTON, btn, 2u) == 2u);
        CHECK(rde_arr_length(&x.k.solenoids) == 2u);
        play_frames(&x, 10u);
        for(u32 k = 0; k < 2u; k++) {
            const fude_zoom_circuit_part* sp = &((const fude_zoom_circuit_part*)x.c.parts.memory)[so[k]];
            // (its bolt: the slider over its rail, 168.5 right of its solenoid)
            u32 bolt = FUDE_ZOOM_NONE;
            const fude_zoom_mech_body* b = (const fude_zoom_mech_body*)x.w.plan.bodies.memory;
            const fude_zoom_v2 at = b[body_of(&x.w.plan, sp->object)].at;
            for(u32 i = 0; i < (u32)rde_arr_length(&x.w.plan.bodies); i++) {
                if(b[i].part->kind == FUDE_ZOOM_MECH_SLIDER && fabs(b[i].at.x - at.x - 168.5) < 1.0 && fabs(b[i].at.y - at.y) < 1.0) bolt = i;
            }
            CHECK(bolt != FUDE_ZOOM_NONE);
            rde_arr_clear(&x.c.events);
            CHECK(fude_zoom_circuit_tap(&x.c, btn[k], -1, say, sizeof say));   // (held)
            play_frames(&x, 30u);
            const double in = ((const fude_zoom_circuit_part*)x.c.parts.memory)[so[k]].state[2];
            const fude_zoom_sim m = bolt != FUDE_ZOOM_NONE ? fude_zoom_mech_world_move(&x.w, bolt) : fude_zoom_sim_identity();
            CHECK(fude_zoom_circuit_tap(&x.c, btn[k], -1, say, sizeof say));   // (let go)
            play_frames(&x, 40u);
            const double out = ((const fude_zoom_circuit_part*)x.c.parts.memory)[so[k]].state[2];
            printf("  solenoid example, the latch %s a diode: held, %.2f in, its bolt back %.2f; let go, %.2f in, %u sparks\n", k == 0u ? "with" : "without", in,
                   -fude_zoom_sim_apply(m, b[bolt != FUDE_ZOOM_NONE ? bolt : 0u].at).x + b[bolt != FUDE_ZOOM_NONE ? bolt : 0u].at.x, out, events_of(&x, FUDE_ZOOM_CIRCUIT_ARC));
            CHECK(in > 0.98 && out < 0.02);
            WITHIN("solenoid example: its bolt off 15 back", fabs(fude_zoom_sim_apply(m, b[bolt != FUDE_ZOOM_NONE ? bolt : 0u].at).x - b[bolt != FUDE_ZOOM_NONE ? bolt : 0u].at.x + 15.0), 0.6);
            CHECK(events_of(&x, FUDE_ZOOM_CIRCUIT_ARC) == (k == 0u ? 0u : 1u) && events_of(&x, FUDE_ZOOM_CIRCUIT_BURNT) == 0u);
        }
        CHECK(x.ok && rde_arr_length(&x.w.events) == 0u);
        play_end(&x);
        rde_arr_free(&born);
        fude_zoom_scene_destroy(&sc);
    }
    {
        fude_zoom_scene sc; fude_zoom_scene_init(&sc, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_example_build(&sc, sc.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, FUDE_ZOOM_EXAMPLE_STEPPER, NULL, &born);
        played x;
        play_begin(&x, &sc);
        u32 st;
        CHECK(example_parts(&x, FUDE_ZOOM_MODEL_STEPPER, &st, 1u) == 1u && rde_arr_length(&x.k.steppers) == 1u);
        const fude_zoom_coupled_shaft* sh = (const fude_zoom_coupled_shaft*)x.k.steppers.memory;
        runner rn = runner_begin(&x.w, ((const fude_zoom_mech_shaft*)x.w.shafts.memory)[sh[0].shaft].body);
        for(u32 f = 0; f < 120u; f++) { play_frames(&x, 1u); runner_track(&x.w, &rn); }
        const fude_zoom_circuit_part* p = &((const fude_zoom_circuit_part*)x.c.parts.memory)[st];
        printf("  stepper example: %.1f steps in 2 s, its arm %.2f° round (its steps' %.2f°)\n", p->state[2], rn.turned * 180.0 / PI, p->state[1]);
        CHECK(p->state[2] > 390.0 && p->state[2] < 402.0);
        WITHIN("stepper example: its arm's turn off its rotor's (rad)", fabs(rn.turned - (p->state[0] - sh[0].from) * 4.0 / 2048.0), 0.01);
        CHECK(x.ok && !p->burnt);
        play_end(&x);
        rde_arr_free(&born);
        fude_zoom_scene_destroy(&sc);
    }
    {
        fude_zoom_scene sc; fude_zoom_scene_init(&sc, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_example_build(&sc, sc.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, FUDE_ZOOM_EXAMPLE_SHAFT_SENSORS, NULL, &born);
        played x;
        play_begin(&x, &sc);
        CHECK(rde_arr_length(&x.k.encoders) == 1u && rde_arr_length(&x.k.pots) == 1u && rde_arr_length(&x.k.slots) == 1u && rde_arr_length(&x.w.cranks) == 2u);
        u32 en, pot, led[2], probe, meter;
        CHECK(example_parts(&x, FUDE_ZOOM_MODEL_ENCODER, &en, 1u) == 1u && example_parts(&x, FUDE_ZOOM_MODEL_POT, &pot, 1u) == 1u);
        CHECK(example_parts(&x, FUDE_ZOOM_MODEL_LED, led, 2u) == 2u && example_parts(&x, FUDE_ZOOM_MODEL_LOGIC_OUT, &probe, 1u) == 1u);
        CHECK(example_parts(&x, FUDE_ZOOM_MODEL_PANEL_METER, &meter, 1u) == 1u);
        const u32 ce = fude_zoom_mech_world_crank_at(&x.w, (fude_zoom_v2){ 5, 5 }), cp = fude_zoom_mech_world_crank_at(&x.w, (fude_zoom_v2){ 455, 5 });
        CHECK(ce != FUDE_ZOOM_NONE && cp != FUDE_ZOOM_NONE && ce != cp);
        play_frames(&x, 10u);
        const fude_zoom_circuit_part* P = (const fude_zoom_circuit_part*)x.c.parts.memory;
        const double pos0 = P[en].state[0], wiper0 = P[pot].state[0];
        u32 seen[64], n = 0, last = 99u, flips = 0;
        double last_probe = P[probe].shown;
        for(u32 f = 0; f < 240u; f++) {
            if(ce != FUDE_ZOOM_NONE && f < 120u) fude_zoom_mech_world_crank_hold(&x.w, ce, 2.0 * PI * (double)(f + 1u) / 120.0);   // (a turn, counter-clockwise, in 2 s)
            if(cp != FUDE_ZOOM_NONE && f < 60u) fude_zoom_mech_world_crank_hold(&x.w, cp, -0.5 * PI * (double)(f + 1u) / 60.0);   // (a quarter, clockwise)
            play_frames(&x, 1u);
            P = (const fude_zoom_circuit_part*)x.c.parts.memory;
            const u32 now = (P[led[0]].shown > 0.3 ? 1u : 0u) | (P[led[1]].shown > 0.3 ? 2u : 0u);
            if(now != last && n < 64u) seen[n++] = now;
            last = now;
            flips += (P[probe].shown > 0.5) != (last_probe > 0.5) ? 1u : 0u;
            last_probe = P[probe].shown;
        }
        if(ce != FUDE_ZOOM_NONE) fude_zoom_mech_world_crank_let_go(&x.w, ce);
        if(cp != FUDE_ZOOM_NONE) fude_zoom_mech_world_crank_let_go(&x.w, cp);
        P = (const fude_zoom_circuit_part*)x.c.parts.memory;
        // (the LEDs, counter-clockwise: each detent B's closing first — neither, B, both, A, neither…)
        u32 bad = 0;
        for(u32 i = 1; i < n; i++) {
            const u32 a = seen[i - 1u], b = seen[i];
            const b8 ok = (a == 0u && b == 2u) || (a == 2u && b == 3u) || (a == 3u && b == 1u) || (a == 1u && b == 0u);
            bad += ok ? 0u : 1u;
        }
        printf("  shaft sensors: the encoder %.2f -> %.2f detents (%u changes of its LEDs, %u out of turn); the pot %.3f -> %.3f, its meter %.2f V; "
               "the probe toggled %u times\n", pos0, P[en].state[0], n, bad, wiper0, P[pot].state[0], P[meter].shown, flips);
        WITHIN("shaft sensors: the encoder off 20 detents back", fabs(P[en].state[0] - pos0 + 20.0), 0.3);
        CHECK(n >= 60u && bad == 0u);
        WITHIN("shaft sensors: the pot's wiper off a third on", fabs(P[pot].state[0] - wiper0 - 1.0 / 3.0), 0.02);
        WITHIN("shaft sensors: the meter off its wiper's volts", fabs(P[meter].shown - 5.0 * (1.0 - P[pot].state[0])), 0.02);
        CHECK(flips >= 1u && x.ok);
        play_end(&x);
        rde_arr_free(&born);
        fude_zoom_scene_destroy(&sc);
    }
}

// Batch 0.1.74 (mech.h): each example played as Play runs a mechanism alone.
typedef struct { fude_zoom_scene s; rde_arr born; fude_zoom_mech_plan p; fude_zoom_mech_world w; } mech_example;

static void mech_example_begin(mech_example* x, u32 e) {
    fude_zoom_scene_init(&x->s, 7);
    x->born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    fude_zoom_example_build(&x->s, x->s.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, e, NULL, &x->born);
    fude_zoom_mech_plan_init(&x->p);
    fude_zoom_mech_world_init(&x->w);
    CHECK(fude_zoom_mech_plan_build(&x->p, &x->s, NULL) > 0u && fude_zoom_mech_world_start(&x->w, &x->p));
}

static void mech_example_end(mech_example* x) {
    fude_zoom_mech_world_destroy(&x->w);
    fude_zoom_mech_plan_destroy(&x->p);
    rde_arr_free(&x->born);
    fude_zoom_scene_destroy(&x->s);
}

static void mech_run(mech_example* x, u32 frames) {
    for(u32 f = 0; f < frames; f++) fude_zoom_mech_world_step(&x->w, 1.0 / 60.0);
}

// The plan's bodies of kind _kind, in the order drawn.
static u32 bodies_of(const fude_zoom_mech_plan* p, u8 kind, u32* out, u32 most) {
    u32 n = 0;
    const fude_zoom_mech_body* b = (const fude_zoom_mech_body*)p->bodies.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&p->bodies); i++) if(b[i].part->kind == kind && n < most) out[n++] = i;
    return n;
}

// BELTS AND CHAINS: the chain's sprocket twice as big turning half as fast, the same way; the crossed belt's as fast,
// the other way. A belt with an end on nothing turns nothing.
static void test_belts(void) {
    printf("belts and chains\n");
    {
        // (each new part found by its id, a symbol of its own, drawn)
        static const char* const ids[9] = { "belt", "chain", "sprocket", "cam", "follower", "ratchet", "pawl", "damper", "worm" };
        rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std()), parts = rde_arr_new(sizeof(fude_zoom_symbol_part), rde_memory_allocator_get_default_std());
        for(u32 i = 0; i < 9u; i++) {
            const fude_zoom_mech_part* mp = fude_zoom_mech_find(ids[i]);
            CHECK(mp != NULL && fude_zoom_symbol_find(ids[i]) != FUDE_ZOOM_NONE && fude_zoom_mech_draw(mp, 60.0, 20.0, 32u, &pts, &parts) >= 2u);
        }
        rde_arr_free(&pts);
        rde_arr_free(&parts);
    }
    mech_example x;
    mech_example_begin(&x, FUDE_ZOOM_EXAMPLE_BELTS);
    CHECK(rde_arr_length(&x.w.plan.belts) == 2u);
    const fude_zoom_mech_belt* bt = (const fude_zoom_mech_belt*)x.w.plan.belts.memory;
    const fude_zoom_mech_mesh* m = (const fude_zoom_mech_mesh*)x.w.plan.meshes.memory;
    u32 belts = 0;
    for(u32 k = 0; k < (u32)rde_arr_length(&x.w.plan.meshes); k++) {
        if(!m[k].belt) continue;
        belts++;
        WITHIN("belts: a mesh's ratio off its radii's", fabs(fabs(m[k].ratio) - bt[belts - 1u].ra / bt[belts - 1u].rb), 1e-12);
        CHECK((m[k].ratio > 0.0) == bt[belts - 1u].crossed);
    }
    CHECK(belts == 2u && bt[0].chain && !bt[0].crossed && !bt[1].chain && bt[1].crossed);
    mech_run(&x, 60u);
    const double a0[2] = { fude_zoom_mech_world_turned(&x.w, bt[0].a), fude_zoom_mech_world_turned(&x.w, bt[1].a) };
    const double b0[2] = { fude_zoom_mech_world_turned(&x.w, bt[0].b), fude_zoom_mech_world_turned(&x.w, bt[1].b) };
    mech_run(&x, 120u);
    for(u32 k = 0; k < 2u; k++) {
        const double da = fude_zoom_mech_world_turned(&x.w, bt[k].a) - a0[k], db = fude_zoom_mech_world_turned(&x.w, bt[k].b) - b0[k];
        const double want = (k == 0u ? 0.5 : -1.0) * da;
        printf("  %s: the motor's sprocket %.3f turns, the other %.3f (%.3f wanted)\n", k == 0u ? "chain" : "crossed belt", da / (2.0 * PI), db / (2.0 * PI), want / (2.0 * PI));
        CHECK(fabs(da) > 0.9 * 2.0 * PI);   // (30 rpm: a turn in 2 s)
        WITHIN("belts: the driven wheel's turn off its ratio's (rad)", fabs(db - want), 0.02 * fabs(want) + 0.01);
    }
    mech_example_end(&x);
    {
        strength_rig r;
        strength_begin(&r);
        fude_zoom_placer_part(&r.pl, "sprocket", 0, 0, 0, 60, 60, "");
        fude_zoom_placer_part(&r.pl, "belt", 125, 0, 0, 290, 40, "");   // (its far end on nothing)
        strength_run(&r, 1u);
        CHECK(rde_arr_length(&r.w.plan.belts) == 0u);
        strength_end(&r);
    }
}

// CAM AND FOLLOWER: the follower's roller kept on the cam as it turns (its height the cam's surface's over its axle line,
// a roller's radius out), rising and falling twice the cam's eccentricity.
static void test_cam(void) {
    printf("cam and follower\n");
    mech_example x;
    mech_example_begin(&x, FUDE_ZOOM_EXAMPLE_CAM);
    u32 cam, fol;
    CHECK(bodies_of(&x.w.plan, FUDE_ZOOM_MECH_CAM, &cam, 1u) == 1u && bodies_of(&x.w.plan, FUDE_ZOOM_MECH_FOLLOWER, &fol, 1u) == 1u);
    const fude_zoom_mech_body* b = (const fude_zoom_mech_body*)x.w.plan.bodies.memory;
    const double R = 0.95 * 40.0, r = 0.6 * 15.0, e = FUDE_ZOOM_MECH_CAM_ECCENTRIC * R;
    mech_run(&x, 30u);
    double lo = 1e300, hi = -1e300, worst = 0.0;
    double base = NAN;
    for(u32 f = 0; f < 360u; f++) {
        mech_run(&x, 1u);
        const double th = fude_zoom_mech_world_turned(&x.w, cam);
        const double xc = e * cos(th), yc = e * sin(th);
        const double roller = yc + sqrt((R + r) * (R + r) - xc * xc);   // (where its roller's middle should be, over the axle)
        const fude_zoom_v2 at = fude_zoom_sim_apply(fude_zoom_mech_world_move(&x.w, fol), b[fol].at);
        const double rm = at.y - 45.0;   // (its roller 45 down its length from its middle)
        if(isnan(base)) base = rm - roller;
        worst = fmax(worst, fabs(rm - roller - base));
        lo = fmin(lo, rm); hi = fmax(hi, rm);
    }
    printf("  the follower's roller %.2f to %.2f (%.2f, its cam's %.2f), off the cam's surface %.2f at worst\n", lo, hi, hi - lo, 2.0 * e, worst);
    WITHIN("cam: the follower's rise off twice its eccentricity", fabs(hi - lo - 2.0 * e), 1.0);
    WITHIN("cam: the follower off the cam's surface", worst, 1.0);
    WITHIN("cam: the roller's middle off its radius from the cam's (pressed in a little)", fabs(base), 2.0);
    mech_example_end(&x);
}

// RATCHET AND PAWL: the weights pulling both wheels clockwise — the one with a pawl held within a tooth, the other run
// back; the held one dragged a quarter turn counter-clockwise, its pawl clicking over its teeth (lifted and dropped), and
// let go, kept there within a tooth.
static void test_ratchet(void) {
    printf("ratchet and pawl\n");
    mech_example x;
    mech_example_begin(&x, FUDE_ZOOM_EXAMPLE_RATCHET);
    u32 rt[2], pw;
    CHECK(bodies_of(&x.w.plan, FUDE_ZOOM_MECH_RATCHET, rt, 2u) == 2u && bodies_of(&x.w.plan, FUDE_ZOOM_MECH_PAWL, &pw, 1u) == 1u);
    CHECK(rde_arr_length(&x.w.plan.pawls) == 1u && ((const fude_zoom_mech_pawl*)x.w.plan.pawls.memory)[0].ratchet == rt[0]);
    const double tooth = 2.0 * PI / (double)FUDE_ZOOM_MECH_RATCHET_TEETH;
    mech_run(&x, 120u);
    const double held = fude_zoom_mech_world_turned(&x.w, rt[0]), ran = fude_zoom_mech_world_turned(&x.w, rt[1]);
    printf("  pulled clockwise 2 s: the one with a pawl %.3f rad, the one without %.3f rad\n", held, ran);
    CHECK(held > -tooth - 0.05 && ran < -0.5);
    // (dragged by its bottom round to its right: counter-clockwise, a quarter turn in 1.5 s)
    CHECK(fude_zoom_mech_world_grab(&x.w, (fude_zoom_v2){ -150.0, -25.0 }));
    double pawl_lo = 1e300, pawl_hi = -1e300;
    for(u32 f = 0; f < 90u; f++) {
        const double a = -0.5 * PI + 0.5 * PI * (double)(f + 1u) / 90.0;
        fude_zoom_mech_world_drag(&x.w, (fude_zoom_v2){ -150.0 + 25.0 * cos(a), 25.0 * sin(a) });
        mech_run(&x, 1u);
        const double pa = fude_zoom_mech_world_turned(&x.w, pw);
        pawl_lo = fmin(pawl_lo, pa); pawl_hi = fmax(pawl_hi, pa);
    }
    const double wound = fude_zoom_mech_world_turned(&x.w, rt[0]);
    fude_zoom_mech_world_let_go(&x.w);
    mech_run(&x, 90u);
    const double after = fude_zoom_mech_world_turned(&x.w, rt[0]);
    printf("  dragged round: %.3f rad (its pawl swinging %.3f rad); let go, %.3f rad\n", wound, pawl_hi - pawl_lo, after);
    CHECK(wound - held > 0.25 * PI && pawl_hi - pawl_lo > 0.05);
    CHECK(after > wound - tooth - 0.05);
    CHECK(rde_arr_length(&x.w.events) == 0u);
    mech_example_end(&x);
}

// A DAMPER: two weights on springs bouncing from where they were drawn — between half a second and a second on, the one
// with a damper under it within a tenth of the other's bounce.
static void test_damper(void) {
    printf("damper\n");
    mech_example x;
    mech_example_begin(&x, FUDE_ZOOM_EXAMPLE_DAMPER);
    u32 wt[2];
    CHECK(bodies_of(&x.w.plan, FUDE_ZOOM_MECH_WEIGHT, wt, 2u) == 2u && rde_arr_length(&x.w.plan.dampers) == 1u);
    const fude_zoom_mech_damper* d = (const fude_zoom_mech_damper*)x.w.plan.dampers.memory;
    CHECK(d[0].a == wt[1] || d[0].b == wt[1]);
    CHECK(d[0].a == FUDE_ZOOM_NONE || d[0].b == FUDE_ZOOM_NONE);   // (its other end on a pivot: the ground)
    const fude_zoom_mech_body* b = (const fude_zoom_mech_body*)x.w.plan.bodies.memory;
    mech_run(&x, 30u);
    double lo[2] = { 1e300, 1e300 }, hi[2] = { -1e300, -1e300 };
    for(u32 f = 0; f < 60u; f++) {
        mech_run(&x, 1u);
        for(u32 k = 0; k < 2u; k++) {
            const double y = fude_zoom_sim_apply(fude_zoom_mech_world_move(&x.w, wt[k]), b[wt[k]].at).y;
            lo[k] = fmin(lo[k], y); hi[k] = fmax(hi[k], y);
        }
    }
    printf("  bouncing from 0.5 s to 1 s: %.2f with no damper, %.2f with one\n", hi[0] - lo[0], hi[1] - lo[1]);
    CHECK(hi[0] - lo[0] > 2.0 && hi[1] - lo[1] < 0.1 * (hi[0] - lo[0]));   // (a spring damps itself a little: the free one too, slowly)
    mech_example_end(&x);
}

// A WORM: its gear a tooth a turn of it (60 rpm on a 30-tooth gear: 2 rpm, counter-clockwise over it), the 10-tooth gear
// beside it three times as fast the other way; held hard the other way (a torque on the gear), no faster, no slower: a
// gear cannot turn a worm.
static void test_worm(void) {
    printf("worm gear\n");
    mech_example x;
    mech_example_begin(&x, FUDE_ZOOM_EXAMPLE_WORM);
    u32 g[2];
    CHECK(bodies_of(&x.w.plan, FUDE_ZOOM_MECH_GEAR, g, 2u) == 2u && rde_arr_length(&x.w.plan.worms) == 1u);
    const fude_zoom_mech_worm* wm = (const fude_zoom_mech_worm*)x.w.plan.worms.memory;
    CHECK(wm[0].gear == g[0] && wm[0].side == 1.0 && fabs(wm[0].speed - 1.0) < 1e-12);
    mech_run(&x, 60u);
    for(u32 k = 0; k < 2u; k++) {
        const double big0 = fude_zoom_mech_world_turned(&x.w, g[0]), small0 = fude_zoom_mech_world_turned(&x.w, g[1]);
        for(u32 f = 0; f < 180u; f++) {
            if(k == 1u) {
                rde_physics_2d_body* gb = ((rde_physics_2d_body**)x.w.bodies.memory)[g[0]];
                rde_physics_2d_body_apply_torque(gb, (f32)(-200.0 * fude_zoom_mech_world_torque_unit(&x.w)));   // (hard the other way)
            }
            mech_run(&x, 1u);
        }
        const double big = fude_zoom_mech_world_turned(&x.w, g[0]) - big0, small = fude_zoom_mech_world_turned(&x.w, g[1]) - small0;
        const double want = 2.0 * PI / 30.0 * 3.0;   // (2 rpm for 3 s)
        printf("  %s: the 30-tooth gear %.4f rad (%.4f wanted), the 10-tooth %.4f\n", k == 0u ? "free" : "held back hard", big, want, small);
        WITHIN("worm: its gear's turn off a tooth a turn", fabs(big - want), 0.03 * want);
        WITHIN("worm: the small gear's turn off three times the big one's, back", fabs(small + 3.0 * big), 0.03 * want * 3.0);
    }
    mech_example_end(&x);
}

// ONE PIN THROUGH THEM ALL: three links on a fixed pivot — each pinned to the ground there, none to another —; three on
// one hole of a moving part — two hinges, not three. KEYED: a sprocket on a gear on a motor turns with it (a belt from
// it, not from the gear under it: the topmost), a belt to a sprocket twice its size, a cam on that one turning with it —
// half the motor's speed, the same way. A TRACER weighs nothing and does not change the mechanism's scale.
static void test_pins_and_keys(void) {
    printf("pins, keys, tracers\n");
    {
        rig r; rig_begin(&r);
        rig_part(&r, "fixed pivot", 0, -10, 0, 0, 0, "");
        for(u32 k = 0; k < 3u; k++) {
            const f64 an = 2.0 * PI * (f64)k / 3.0;
            rig_part(&r, "link", 60.0 * cos(an), 60.0 * sin(an), an / PI * 180.0, 144, 24, "");
        }
        // (three more on one hole, away from the pivot: a link's end at (300, 0), two others' there)
        rig_part(&r, "fixed pivot", 180, -10, 0, 0, 0, "");
        rig_part(&r, "link", 240, 0, 0, 144, 24, "");
        rig_part(&r, "link", 360, 0, 0, 144, 24, "");
        rig_part(&r, "link", 300, 60, 90, 144, 24, "");
        fude_zoom_mech_plan p; fude_zoom_mech_plan_init(&p);
        CHECK(fude_zoom_mech_plan_build(&p, &r.s, NULL) > 0u);
        const fude_zoom_mech_hinge* h = (const fude_zoom_mech_hinge*)p.hinges.memory;
        u32 at_pivot = 0, between = 0, at_hole = 0;
        for(u32 i = 0; i < (u32)rde_arr_length(&p.hinges); i++) {
            if(hypot(h[i].at.x, h[i].at.y) < 1.0) { at_pivot++; between += h[i].b != FUDE_ZOOM_NONE ? 1u : 0u; }
            if(hypot(h[i].at.x - 300.0, h[i].at.y) < 1.0) at_hole++;
        }
        printf("  three on a pivot: %u hinges (%u between them); three on a hole: %u\n", at_pivot, between, at_hole);
        CHECK(at_pivot == 3u && between == 0u && at_hole == 2u);
        fude_zoom_mech_plan_destroy(&p);
        rig_end(&r);
    }
    for(u32 k = 0; k < 2u; k++) {
        rig r; rig_begin(&r);
        rig_part(&r, "drive motor", 0, 0, 0, 0, 0, "30 rpm");
        rig_part(&r, "gear 30T", 0, 0, 0, 0, 0, "");
        rig_part(&r, "sprocket", 0, 0, 0, 60, 60, "");
        rig_part(&r, "sprocket", 300, 0, 0, 120, 120, "");
        rig_part(&r, "belt", 150, 0, 0, 340, 40, "");
        const u32 cam = rig_part(&r, "cam", 300 + 0.285 * 40.0, 0, 0, 80, 80, "");
        if(k == 1u) rig_part(&r, "tracer", 150, 200, 0, 0, 0, "");   // (on nothing: its scale the same)
        mech_example x;
        x.s = r.s; x.born = r.born;
        fude_zoom_mech_plan_init(&x.p);
        fude_zoom_mech_world_init(&x.w);
        CHECK(fude_zoom_mech_plan_build(&x.p, &x.s, NULL) > 0u && fude_zoom_mech_world_start(&x.w, &x.p));
        const fude_zoom_mech_belt* bt = (const fude_zoom_mech_belt*)x.p.belts.memory;
        const fude_zoom_mech_body* b = (const fude_zoom_mech_body*)x.p.bodies.memory;
        CHECK(rde_arr_length(&x.p.belts) == 1u && b[bt[0].a].part->kind == FUDE_ZOOM_MECH_SPROCKET && b[bt[0].b].part->kind == FUDE_ZOOM_MECH_SPROCKET);
        static f64 unit_alone = 0.0;
        if(k == 0u) unit_alone = x.p.unit; else CHECK(x.p.unit == unit_alone);
        const u32 cb = body_of(&x.p, cam), motor_gear = 1u;
        mech_run(&x, 120u);
        const f64 gear_turn = fude_zoom_mech_world_turned(&x.w, motor_gear), cam_turn = fude_zoom_mech_world_turned(&x.w, cb);
        printf("  keyed%s: the motor's gear %.3f turns, the cam %.3f (scale %.2f)\n", k == 1u ? " (a tracer by it)" : "", gear_turn / (2.0 * PI), cam_turn / (2.0 * PI), x.p.unit);
        WITHIN("keyed: the cam off half the motor's turn", fabs(cam_turn - 0.5 * gear_turn), 0.02 * fabs(gear_turn));
        CHECK(fabs(gear_turn) > PI);
        fude_zoom_mech_world_destroy(&x.w);
        fude_zoom_mech_plan_destroy(&x.p);
        rig_end(&r);
    }
}

// THE BIG MECHANISMS. The Strandbeest's legs: eight seconds, nothing broken; each foot's path Jansen's at four times his
// size (about 271 wide, 88 high), level along its bottom (a fifth of its points within 4 of its lowest), the two
// mirrored. The four-stroke engine: its camshafts at half the crank's speed, the other way; the intake valve open the
// most within the first downstroke, the exhaust within the last upstroke. The solenoid engine: it starts by itself (rocking
// a few times) and runs — over 10 turns in 8 s —, its 74HC161 counting each time its sensor is blocked; switched off, its
// coil let go, it slows.
static void test_big_mechanisms(void) {
    printf("big mechanisms\n");
    {
        mech_example x;
        mech_example_begin(&x, FUDE_ZOOM_EXAMPLE_STRANDBEEST);
        mech_run(&x, 480u);
        CHECK(rde_arr_length(&x.w.events) == 0u && rde_arr_length(&x.w.plan.tracers) == 2u);
        f64 x0[2], x1[2], y0[2], y1[2], flat[2];
        for(u32 t = 0; t < 2u; t++) {
            rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
            const u32 n = fude_zoom_mech_world_trail(&x.w, t, &pts);
            const fude_zoom_v2* q = (const fude_zoom_v2*)pts.memory;
            x0[t] = y0[t] = 1e9; x1[t] = y1[t] = -1e9;
            for(u32 i = n / 4u; i < n; i++) { x0[t] = fmin(x0[t], q[i].x); x1[t] = fmax(x1[t], q[i].x); y0[t] = fmin(y0[t], q[i].y); y1[t] = fmax(y1[t], q[i].y); }
            u32 low = 0;
            for(u32 i = n / 4u; i < n; i++) low += q[i].y < y0[t] + 4.0 ? 1u : 0u;
            flat[t] = (f64)low / (f64)(n - n / 4u);
            rde_arr_free(&pts);
        }
        printf("  Strandbeest: feet %.0f..%.0f and %.0f..%.0f across, %.0f and %.0f high, %.0f%% and %.0f%% level\n", x0[0], x1[0], x0[1], x1[1], y1[0] - y0[0],
               y1[1] - y0[1], flat[0] * 100.0, flat[1] * 100.0);
        for(u32 t = 0; t < 2u; t++) {
            CHECK(fabs(x1[t] - x0[t] - 271.0) < 15.0 && fabs(y1[t] - y0[t] - 88.0) < 10.0 && flat[t] > 0.2);
        }
        CHECK(fabs(x0[0] + x1[1]) < 5.0 && fabs(x1[0] + x0[1]) < 5.0 && fabs(y0[0] - y0[1]) < 5.0);
        mech_example_end(&x);
    }
    {
        mech_example x;
        mech_example_begin(&x, FUDE_ZOOM_EXAMPLE_ENGINE);
        u32 cams[2], fol[2], links[2], sl[1];
        CHECK(bodies_of(&x.p, FUDE_ZOOM_MECH_CAM, cams, 2u) == 2u && bodies_of(&x.p, FUDE_ZOOM_MECH_FOLLOWER, fol, 2u) == 2u &&
              bodies_of(&x.p, FUDE_ZOOM_MECH_LINK, links, 2u) == 2u && bodies_of(&x.p, FUDE_ZOOM_MECH_SLIDER, sl, 1u) == 1u);
        const fude_zoom_mech_body* b = (const fude_zoom_mech_body*)x.p.bodies.memory;
        f64 best[2] = { -1e9, -1e9 }, at[2] = { 0.0, 0.0 }, lo = 1e9, hi = -1e9;
        for(u32 f = 0; f < 480u; f++) {
            mech_run(&x, 1u);
            const f64 crank = fude_zoom_mech_world_turned(&x.w, links[0]) * 180.0 / PI;
            for(u32 k = 0; k < 2u; k++) {
                // (its lift: how far it has gone toward the head, along its rod)
                const u32 v = b[fol[0]].at.x < 0.0 ? fol[k] : fol[1u - k];
                const fude_zoom_v2 q = fude_zoom_mech_world_point(&x.w, v, b[v].at);
                const f64 lift = -((q.x - b[v].at.x) * cos(b[v].angle) + (q.y - b[v].at.y) * sin(b[v].angle));
                if(lift > best[k] && crank < 720.0) { best[k] = lift; at[k] = crank; }
            }
            const fude_zoom_v2 pp = fude_zoom_mech_world_point(&x.w, sl[0], b[sl[0]].at);
            lo = fmin(lo, pp.y); hi = fmax(hi, pp.y);
        }
        const f64 crank = fude_zoom_mech_world_turned(&x.w, links[0]), cam = fude_zoom_mech_world_turned(&x.w, cams[0]);
        printf("  engine: the crank %.2f turns, its camshafts %.3f of it; the piston %.0f..%.0f; intake open most at %.0f°, exhaust at %.0f°\n",
               crank / (2.0 * PI), cam / crank, lo, hi, at[0], at[1]);
        WITHIN("engine: its camshafts off half its crank's speed, the other way", fabs(cam / crank + 0.5), 0.01);
        CHECK(at[0] > 40.0 && at[0] < 160.0 && at[1] > 560.0 && at[1] < 700.0 && fabs(lo - 120.0) < 3.0 && fabs(hi - 220.0) < 3.0);
        CHECK(rde_arr_length(&x.w.events) == 0u);
        mech_example_end(&x);
    }
    {
        fude_zoom_scene sc; fude_zoom_scene_init(&sc, 7);
        rde_arr born = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        fude_zoom_example_build(&sc, sc.root, fude_zoom_sim_identity(), (rde_color){ 1, 1, 1, 255 }, FUDE_ZOOM_EXAMPLE_SOLENOID_ENGINE, NULL, &born);
        played x;
        play_begin(&x, &sc);
        CHECK(rde_arr_length(&x.w.cranks) == 1u && rde_arr_length(&x.k.slots) == 1u && rde_arr_length(&x.w.plungers) == 1u);
        const fude_zoom_circuit_part* cp = (const fude_zoom_circuit_part*)x.c.parts.memory;
        u32 sol = FUDE_ZOOM_NONE, slot = FUDE_ZOOM_NONE, cnt = FUDE_ZOOM_NONE, sw = FUDE_ZOOM_NONE;
        for(u32 i = 0; i < (u32)rde_arr_length(&x.c.parts); i++) {
            if(cp[i].part->model == FUDE_ZOOM_MODEL_SOLENOID) sol = i;
            if(cp[i].part->model == FUDE_ZOOM_MODEL_SLOT) slot = i;
            if(cp[i].part->model == FUDE_ZOOM_MODEL_SWITCH) sw = i;
            if(strcmp(cp[i].part->id, "74HC161") == 0) cnt = i;
        }
        CHECK(sol != FUDE_ZOOM_NONE && slot != FUDE_ZOOM_NONE && cnt != FUDE_ZOOM_NONE && sw != FUDE_ZOOM_NONE);
        // (by itself: its rod's weight lets its pin down, the cam blocks the sensor, the coil pulls — it starts)
        const f64 from = fude_zoom_mech_world_crank_angle(&x.w, 0u);
        u32 blocked = 0;
        b8 was = false;
        for(u32 f = 0; f < 480u; f++) {
            play_frames(&x, 1u);
            cp = (const fude_zoom_circuit_part*)x.c.parts.memory;
            const b8 b = cp[slot].state[0] > 0.5;
            blocked += b && !was ? 1u : 0u;
            was = b;
        }
        cp = (const fude_zoom_circuit_part*)x.c.parts.memory;
        const f64 turns = fabs(fude_zoom_mech_world_crank_angle(&x.w, 0u) - from) / (2.0 * PI);
        u32 count = 0;
        static const u32 qpin[4] = { 13u, 12u, 11u, 10u };   // (QA, QB, QC, QD)
        for(u32 q = 0; q < 4u; q++) count |= (fude_zoom_circuit_volts(&x.c, cp[cnt].node[qpin[q]]) > 2.5 ? 1u : 0u) << q;
        printf("  solenoid engine by itself: %.1f turns in 8 s, its sensor blocked %u times, its counter at %u\n", turns, blocked, count);
        // (it rocks a few times as it starts; the last time it was blocked may not be counted yet — its smoothing holds
        // the counter's clock back some 4 frames)
        CHECK(turns > 10.0 && (count == blocked % 16u || count == (blocked + 15u) % 16u) && (f64)blocked >= turns - 1.0 && (f64)blocked <= turns + 4.0);
        // (switched off: its coil let go, it slows)
        const f64 a0 = fude_zoom_mech_world_crank_angle(&x.w, 0u);
        play_frames(&x, 60u);
        const f64 before = fabs(fude_zoom_mech_world_crank_angle(&x.w, 0u) - a0);
        c8 say[16];
        CHECK(fude_zoom_circuit_tap(&x.c, sw, -1, say, sizeof say));
        play_frames(&x, 240u);
        const f64 a1 = fude_zoom_mech_world_crank_angle(&x.w, 0u);
        play_frames(&x, 60u);
        const f64 after = fabs(fude_zoom_mech_world_crank_angle(&x.w, 0u) - a1);
        cp = (const fude_zoom_circuit_part*)x.c.parts.memory;
        printf("  switched off: %.2f turns a second before, %.2f four seconds after; its coil %.3f A (events %u, ran %d)\n", before / (2.0 * PI), after / (2.0 * PI),
               cp[sol].state[0], (u32)rde_arr_length(&x.w.events), (int)x.ok);
        CHECK(after < 0.97 * before && fabs(cp[sol].state[0]) < 1e-3 && rde_arr_length(&x.w.events) == 0u && x.ok);   // (its cam a flywheel: slowly)
        // (nothing of its circuit burnt nor past its limits — its dynamo's LED the right way round)
        const fude_zoom_circuit_event* cev = (const fude_zoom_circuit_event*)x.c.events.memory;
        for(u32 i = 0; i < (u32)rde_arr_length(&x.c.events); i++) printf("  solenoid engine's circuit: %s %s\n", ((const fude_zoom_circuit_part*)x.c.parts.memory)[cev[i].part].part->id, cev[i].kind == FUDE_ZOOM_CIRCUIT_BURNT ? "burnt" : "past");
        CHECK(rde_arr_length(&x.c.events) == 0u);
        play_end(&x);
        rde_arr_free(&born);
        fude_zoom_scene_destroy(&sc);
    }
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
        u32 happened = 0;   // (its mechanism's events: none — nothing broken, jammed, nor drawn not to fit)
        for(int f = 0; f < 360; f++) {
            const clock_t t0 = clock();
            if(coupled) {
                ran = ran && fude_zoom_coupling_step(&k, &c, &w, 1.0 / 60.0);
            } else {
                fude_zoom_mech_world_step(&w, 1.0 / 60.0);
                if(circuit) ran = ran && fude_zoom_circuit_run(&c, 1.0 / 60.0, 400u);
            }
            spent += (double)(clock() - t0) / CLOCKS_PER_SEC;
            if(rde_arr_length(&w.events) > 0u) {
                const fude_zoom_mech_event* ev = (const fude_zoom_mech_event*)w.events.memory;
                for(u32 i = 0; i < (u32)rde_arr_length(&w.events); i++) printf("  example %u: event %u on joint %u: %g of %g\n", e, ev[i].kind, ev[i].joint, ev[i].force, ev[i].strength);
                happened += (u32)rde_arr_length(&w.events);
                rde_arr_clear(&w.events);
            }
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
        const b8 wrong = fude_zoom_example_goes_wrong(e);   // (made to break, jam, burn: their own test below)
        CHECK(wrong || happened == 0u);
        snprintf(what, sizeof(what), "example %u: finite", e);
        if(!finite) printf("FAIL %s\n", what), fails++;
        snprintf(what, sizeof(what), "example %u: teeth off gaps (tooth)", e);
        WITHIN(what, worst_mesh, 0.05);
        snprintf(what, sizeof(what), "example %u: furthest a body went (points)", e);
        WITHIN(what, far, 2000.0);
        snprintf(what, sizeof(what), "example %u: bodies not moving of", e);
        const b8 by_hand = e == FUDE_ZOOM_EXAMPLE_BY_HAND || e == FUDE_ZOOM_EXAMPLE_SOLENOID || e == FUDE_ZOOM_EXAMPLE_SHAFT_SENSORS ||
                           e == FUDE_ZOOM_EXAMPLE_RATCHET || e == FUDE_ZOOM_EXAMPLE_SOLENOID_ENGINE;   // (still until a hand works it, held by a pawl: their own tests)
        if(!wrong && !by_hand) WITHIN(what, (double)(moving - moved), (double)moving * 0.25);   // (a few may rest where they are: a weight on a wall)
        if(circuit) {
            const b8 shows = lit || e == FUDE_ZOOM_EXAMPLE_FORWARD_BACK || e == FUDE_ZOOM_EXAMPLE_SERVO_TESTER || e == FUDE_ZOOM_EXAMPLE_SERVO_ANGLES ||
                             e == FUDE_ZOOM_EXAMPLE_SOLENOID || e == FUDE_ZOOM_EXAMPLE_STEPPER || e == FUDE_ZOOM_EXAMPLE_SHAFT_SENSORS ||
                             e == FUDE_ZOOM_EXAMPLE_SOLENOID_ENGINE;   // (its motor, its servos, its solenoids, its arm what it shows)
            if(!(ran && shows)) printf("  example %u: ran %d, an LED lit %d\n", e, (int)ran, (int)lit);
            CHECK(ran && shows);
        }
        // (those where the two work on each other: coupled)
        CHECK(coupled == ((e >= FUDE_ZOOM_EXAMPLE_MOTOR_GEARS && e <= FUDE_ZOOM_EXAMPLE_SHAFT_SENSORS) || e == FUDE_ZOOM_EXAMPLE_EVERYTHING ||
                          e == FUDE_ZOOM_EXAMPLE_SOLENOID_ENGINE));
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
    test_servo();
    test_strength();
    test_examples_go_wrong_mech();
    test_dynamo();
    test_dynamo_holds();
    test_shuttle();
    test_turn_counter();
    test_examples();
    test_tracer();
    test_grab();
    test_crank();
    test_solenoid();
    test_stepper();
    test_shaft_sensors();
    test_new_examples();
    test_belts();
    test_cam();
    test_ratchet();
    test_damper();
    test_worm();
    test_pins_and_keys();
    test_big_mechanisms();
    if(fails == 0) printf("ALL PASSED\n"); else printf("%d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
