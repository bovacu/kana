# Mechanisms

How the canvas's mechanisms are worked out and run today: the parts and their plan (`zoom/mech.c`), the running world
on RDE's 2D physics (`zoom/mechrun.c`), drawings made into bodies (`props.h`, `sim/body.c`), Play's drawing and the
hand (`page_mech.c`), and a mechanism played with a circuit (`zoom/coupling.c`). The plan for all the simulations is
`docs/simulation_architecture.md` (§7, and its status table in §10); the history of each decision is
`docs/infinite_canvas_design.md` (§28, §32–33, §40, §42, §49, §52–55, §57–58). Circuits are
`docs/design/circuits.md`. This note says how the code works now.

---

## What it is for, and where it sits

A person draws a mechanism with the library's parts (links, gears, pivots, motors, springs, ropes, pulleys, racks,
cams, belts...) or with drawings made into bodies, and plays it: the parts move as rigid bodies would. As with
circuits, the canvas is read once to build what runs and never written while it runs; the view draws where each part
has moved to.

| File | What it holds |
|---|---|
| `zoom/mech.h`, `mech.c` | the catalogue (`FZM_PARTS`), each part drawn (`fude_zoom_mech_draw`), and the **plan**: which bodies there are and what joins them (`fude_zoom_mech_plan_build`). Pure: no physics |
| `zoom/mechrun.h`, `mechrun.c` | the **world**: the plan made into RDE's 2D physics, stepped, watched for what breaks, handed out for drawing |
| `zoom/coupling.h`, `coupling.c` | a circuit and a mechanism stepped together |
| `zoom/props.h` | a drawing's BODY attribute: its material, fixed or not, its mass, friction and bounce |
| `zoom/bodyform.c` | the Make body card |
| `sim/body.h`, `body.c` | outlines simplified and cut into convex pieces; areas, centres; the materials |
| `page_mech.c` | Play (its build, its step), the hand on what moves, drawing what plays, Make body |

Underneath is RDE's 2D physics (`rde_physics_2d_*`, `~/RDE/engine/src/rde_physics_2d.c`): Box2D v3 with joints RDE
added to its solver (a gear joint and a pulley joint, Box2D v2's brought into v3's solver), ropes, sliders, compound
bodies and force queries.

## The main types and who owns them

- **`fude_zoom_mech_part`**: a catalogue entry (its symbol's id, kind, teeth, holes in its own u, v, starting text).
  Static.
- **`fude_zoom_mech_plan`**: bodies (`fude_zoom_mech_body`: where each part is in the home frame, its half sizes, its
  value from its text, a gear's starting `phase`, a rack's `shift`, a drawn body's pieces), hinges, springs, meshes,
  ropes, slides, tracers, belts, dampers, worms, pawls, and drawn bodies' convex pieces. The page owns one (`page.h`:
  `mech`), built when Play starts. `fude_zoom_mech_plan_init` and `_destroy`; every array on the std allocator.
- **`fude_zoom_mech_world`**: the running mechanism. The page owns one (`mech_run`). It keeps its own copy of the plan
  (`fude_zoom_mech_plan_copy`), its RDE world, a body per plan body (NULL for what has none), its joints as it watches
  them (`fude_zoom_mech_joint`), its motors, cranks, circuit shafts and plungers, each body's move from where it was
  drawn, and its tracers' trails.
- **`fude_zoom_coupling`**: pairs of circuit parts and world shafts or plungers, as indices. The page owns one
  (`coupling`); it must be built again whenever the circuit or the world is.

## The life of a mechanism

```
Play pressed   fude_zoom_page_play_build: fude_zoom_mech_plan_build(scope), fude_zoom_mech_world_start(plan)
               mech_lifted marked (what the world draws itself); coupling built if a circuit plays too
each frame     alone:   fude_zoom_mech_world_step(dt)       in 1/240 s ticks, the remainder kept
               coupled: fude_zoom_coupling_step(dt)         a tick at a time, the circuit stepped between
               events told (a break before a jam before a misfit)
drawn          fude_zoom_page_render_mech and fude_zoom_page_render_trails: parts where they have moved to, springs,
               ropes, belts, trails
Restart        built again from the canvas (edits to a mechanism are taken only then; a circuit's are taken at once)
Stop           fude_zoom_mech_world_stop: RDE's world destroyed, the arrays emptied (their memory kept)
```

## The plan: from the canvas

`fude_zoom_mech_plan_build` is pure and is tested on its own. Every joint is found by place: a hole within half the
plan's **unit** of another hole, or of a body's shape (`fzm_on`), because parts are placed by hand.

1. **Bodies** (`fzm_bodies`): each live, shown mechanism symbol in scope; and a circuit's motor, servo, stepper,
   encoder or pot becomes a **shaft** (a fixed "motor shaft" body: what is pinned on it is turned by the circuit or
   read by it), a solenoid its **plunger**. Pivots, motors, walls, pulleys, rails, shafts and worms are fixed. The text
   gives each its value: a drive motor's speed and way (`fude_zoom_mech_rpm`: "30 rpm cw" in any of the apps'
   languages) and stall torque ("30 rpm 5Nm", else 10 N·m), a weight's or a crate's mass ("500 g"), a spring's
   stiffness, a damper's strength, a worm's speed, a belt "crossed", a crank's axle friction. The plan's unit is the
   smallest part's size (a link's thickness); a shaft, a plunger and a tracer do not count, since a tracer is drawn
   small and made every body ten times heavier when it did (§58).
2. **Drawings made bodies**: each BODY attribute in scope (on the drawing, or the attribute itself) becomes one
   (`fzm_drawn`; below).
3. **Hinges and springs** (`fzm_hinges`): holes over holes are hinged; over a fixed part's, to the ground there; over a
   drive motor's, a motor hinge (its speed and torque); over a shaft's, a shaft hinge. Several parts on one pin are
   joined as a tree, not every pair (`fzm_pinned_there`): more fought each other apart (the Strandbeest's knees). A cam
   or a sprocket on a turning part's middle is **keyed** to it instead (`fzm_keyed`: a mesh of ratio −1 with no teeth).
   A spring's ends hold what their holes are on (fixed: the ground).
4. **Axles** (`fzm_axles`): a hand crank on its own axle; a gear, sprocket, cam, ratchet or pawl that nothing holds on
   an axle where its hole is, so it turns rather than falls.
5. **Tracers** (`fzm_tracers`): each on the topmost moving body under its point, else on nothing.
6. **Pins** (`fzm_pins`): the moving bodies under a pin hinged together there; one alone, or a fixed part under it too,
   to the ground (a nail).
7. **Ropes** (`fzm_ropes`): each end on a body (anywhere on it), else off a pulley's rim, else held where it is; two
   ropes off one pulley are paired.
8. **Slides** (`fzm_slides`): a slider along the rail it is on (between the rail's ends), a rack along its length
   (half its length each way), a follower in its guide, a plunger into its coil.
9. **Meshes** (`fzm_meshes`): a gear whose pitch circle lies on a rack's pitch line, and two gears whose pitch circles
   touch (within 0.6 of a module). Then **belts** (`fzm_belts`), **dampers**, **worms**, **pawls**.
10. **Racks' travel** (`fzm_rack_travel`) and **teeth in gaps** (`fzm_phases`), below.

**Drawings made bodies.** Make body (the lasso's row, `fude_zoom_page_body_selection`) opens the card (`bodyform.c`:
a material from `sim/body.h`, moving or fixed, the mass its area makes); Apply writes a BODY attribute on each drawing
(`fude_zoom_props_add_body`), one undo step. The attribute is a separate object naming the drawing by its id, so the
drawing stays a drawing, older files simply have none, and it follows the drawing when the drawing is made again
(`fude_zoom_props_remap`) or copied. At Play, `fzm_drawn` takes the drawing's outline in the home frame, simplifies it
(Douglas–Peucker, from half a percent of its size, coarser only until it cuts into at most 65 convex pieces of at most
8 corners: `fude_sim_outline_simplify`, `fude_sim_convex_pieces`), and falls back to its box when it crosses itself. Its
mass is its material's density times its area times 10 mm (`FUDE_SIM_BODY_THICKNESS`), unless given. An open line is
fixed ground: thin pieces along it (a ramp, a track). Its pieces are kept about its centroid in the plan's
`piece_points` and `piece_counts`.

## The world: RDE's 2D physics

`fude_zoom_mech_world_start` copies the plan and makes:

- **Scale and units.** World units are `k` per millimetre, `k = 1 / (4 · unit)`, so parts are about one world unit
  across, which suits Box2D. Gravity is 9.81 m/s² in millimetres (`9810 · k`). A world force is turned to newtons by
  `1e-3 / k`, a torque by its square; masses are taken as kilograms (a weight's, a crate's and a drawn body's are; a
  library part's is set from its area in world units, 2·hw·hh for a link, 3·r² for a gear). Sleeping is off: a
  machine at rest is not asleep, since a circuit may start it.
- **Bodies** (`fzw_start_body`, `fzw_shape_plain`, `fzw_shape_more`): a link a capsule, a plate its triangle, a gear
  its pitch circle, a weight or a wheel a ball, a crate a box, a drawn body its pieces as one compound body, a
  follower its rod and roller, a ratchet its hub and twelve sawteeth, a pawl its lever and point. Pivots, motors,
  springs, ropes, pulleys, rails, shafts, tracers, belts, dampers and worms have no body.
- **Collision layers**: 1, a linkage's parts, which pass over one another as a real linkage's do; 2, what is fixed
  (walls, fixed drawings); 4, what is free (weights, wheels, crates, drawings that move), which hits all of them; 8,
  what bears on another (cams, followers' rollers, ratchets' teeth, pawls' points).
- **Joints**, each noted with what it can take (`fzw_note`): hinges, as stiff as RDE lets them be; a motor hinge
  brought up to its speed over a quarter of a second, as a real motor is; a crank's axle a motor holding it still as
  hard as its friction; a shaft's hinge free until the circuit drives it (`fzw_start_hinges`). Slides from the ground
  with their travel (`fzw_start_slides`). Springs as Box2D's, their stiffness a frequency (1: about 2 Hz)
  (`fzw_start_springs`). Ropes and pulleys (`fzw_start_ropes`) and gears (`fzw_start_meshes`), below.

**A tick** (`fzw_tick`, one `FUDE_ZOOM_MECH_STEP`, 1/240 s, in 8 sub-steps): motors ramped while they spin up; the
hand (`fzw_hand`); plungers pulled; what presses by itself (`fzw_springs_of`: a follower onto its cam at three times
its weight, a pawl onto its ratchet at twice, dampers as explicit forces never more than stop half their motion in a
step, so they never throw anything back); RDE's step; then each joint against its strength (`fzw_strain`).
`fude_zoom_mech_world_step` takes the frame's time (at most 0.1 s) in whole ticks, keeps the rest, and hands out
what it is (`fzw_publish`: each body's move from where it was drawn, each body's turn with every whole turn counted, a
pulley's turn from its rope) and the tracers' points (`fzw_trace`) once, after the ticks. The 8 sub-steps (RDE's 4,
twice) are for trains of light and heavy gears (§40); the history gives no reason for 1/240 s beyond its being the
first mechanisms' step (§28), whatever the frame rate.

## Gears and racks

All gears share one tooth size (a 5-point module as they come), so any two mesh; a gear's pitch radius is its half
width times `t / (t + 2)` (`fude_zoom_mech_pitch`). Two meshed gears are an RDE gear joint on their two hinges, turn₁ +
ratio · turn₂ kept, the ratio their teeth's and its sign by which body each gear is in its hinge. A gear on a rack is a
gear joint on the gear's hinge and the rack's slide, its turn and the rack's travel over its pitch radius kept.

Box2D v3 has no gear joint. RDE's first one was a velocity pass after each step, outside the solver: a train's far
gears lagged more each gear along and a rack jammed at half its travel (§40). Now, where each joint holds a body to
the static world (a gear on its axle, a rack on its slide), it is Box2D v2's gear brought into v3's solver: solved with
motors, limits and contacts in every sub-step. A gear on a moving arm (three bodies) is still RDE's pass after each
step.

- **A rack's range**: a rack is held only while some gear is over its teeth (`rde_physics_2d_joint_set_gear_range`;
  the slide's travel is set to the same span by `fzm_rack_travel`). Past its last tooth it stops and its gear turns
  on; taken back, it meshes again a whole tooth from where it was (`rde_physics_2d_joint_set_gear_period`).
- **Teeth in gaps** (`fzm_phases`): before the world is made, each meshed gear is turned (its `phase`, less than half a
  tooth) so its teeth meet the gaps of what it meshes with, along each train from a gear something else holds (a link
  pinned to it, a spring's or a rope's end), which keeps its angle; a train held by nothing starts from its first gear.
  Then each rack is slid along itself (its `shift`) to its gear, so drawn teeth never start inside each other.
- **Worms** (`fzm_worms`): a gear whose pitch circle lies on a worm's pitch line has its axle made a motor turning it a
  tooth for each turn of the worm, as hard as `FZM_WORM_TORQUE` (200 N·m): the gear cannot turn the worm back.

## Belts, pulleys and ropes

- **Belts and chains** (`fzm_belts`): a belt's two end middles on two turning parts' axles (the topmost there,
  `fzm_belt_on`, so a sprocket on a gear's middle is taken, not the gear) make a mesh with no teeth (`belt` set):
  a gear joint of ratio −rₐ/r_b (crossed, +), left out of the teeth's phasing. Play draws the real loop
  (`fude_zoom_page_belt_loop`: the external tangents, or the internal ones crossed) with marks moving as far as its
  first wheel's rim has gone.
- **Ropes**: RDE's rope, a distance joint with only its longest kept, so it goes slack and hangs (drawn sagging from
  the length it has to spare). Two ropes over one pulley are one rope: RDE's pulley joint (Box2D v2's), the two sides'
  lengths together kept, either side slack. With one side tied off, the other is a rope from the rim as long as what is
  left. A pulley has no body; it is drawn turned by how far its first rope has come off it.

## Joints that break, motors that jam, parts that do not fit

Every joint has a strength (§49), from its parts' material (`sim/body.h`'s, in MPa; the library's parts plastic, 40;
the ground and a plunger steel, 400):

| Joint | Strength |
|---|---|
| pin | ⅗ of the weaker part's strength in shear over a pin 0.6 of the thinner part's half width across, 3 to 12 mm |
| slide | half its part's material over its part's own size (`fude_zoom_mech_body_size`, 2 mm at least) by 10 mm, so a slide is as strong wherever it is drawn, not as weak as the smallest part elsewhere on the canvas (the plan's unit, which it once used); a plunger's and a follower's never give |
| spring | stretched past its length by one and a half times it (2½ times as long as drawn) |
| rope | 2 kN (a 6 mm rope) |
| gear mesh | Lewis's: the weaker material × 10 mm face × module × 0.3, against its torque over its pitch radius |

`fzw_strain`, each tick: what pulls each joint (RDE's `rde_physics_2d_joint_get_force` and `_get_torque`, a spring's
stretch), over its strength. Past it for four ticks in a row (a 60th of a second: a knock is not a load), or three
times past it at once, after the first tenth of a second (drawn parts settle), it **breaks** (`fzw_break`): what rests
on it first (a mesh on its gears' pins or slide, `on[]`: RDE needs a gear destroyed before its joints), then itself;
the motor, shaft or crank that drove it is let go of it. A **pin whose parts pull apart** more than 1.5 mm for 0.3 s
is a misfit, told once until they fit again. A **drive motor** that, once up to speed, turns under a tenth of its
speed over half a second is **jammed** (judged by how far it got, every turn counted: it rocks against what holds it).
Each goes into the world's `events`; Play tells the latest of the most serious and draws strain: a ring warming past
six tenths of a joint's strength, a red cross where one broke, a pulsing ring at a misfit or a jammed motor
(`fude_zoom_page_render_strain`).

## Tracers and trails

A tracer is a pen on whatever moves under its point. Each tick `fzw_trace` keeps its point when it has moved a tenth of
the plan's unit, each tracer in an array of its own, at most `FUDE_ZOOM_MECH_TRAIL_MOST` (40 000) points together;
past that nothing more is kept. The page reads each path in place (`fude_zoom_mech_world_trail`) and draws it as one
stroke for each stretch near the screen (`fude_zoom_mech_trail_runs`: points under `FUDE_ZOOM_MECH_TRAIL_STEP` from the
last dropped, a stretch beginning and ending out of sight so its round caps are not seen), into the page's scratch
(`mech_trail`, `mech_trail_runs`, `mech_trail_radii`). Since 2026-10-15 it is drawn as the pen's lines are, smooth and
round-jointed; before, it was copied each frame and drawn as up to 40 000 separate lines (`quality_plan.md` 4.5).

## By hand

While it plays, the pen or a first finger on the mechanism (`fude_zoom_page_mech_hold`): on a **hand crank** (within
1.15 of its smaller half size), it turns as the hand goes round it, every turn counted
(`fude_zoom_page_mech_hold_moved`); `fzw_hand` drives its axle toward that angle at 30 times how far off it is (60
rad/s at most) and no harder than a hand (`FUDE_ZOOM_MECH_HAND_TORQUE`, 20 N·m); let go, its friction holds it. On
anything else that moves (`fude_zoom_mech_world_grab`, a point query), the point taken is pulled toward the hand by a
spring its own mass makes `FUDE_ZOOM_MECH_HAND_HZ` (12 Hz), critically damped, at most 30 times its weight (at 4 Hz
a pendulum's weight sagged 23 units off the hand, §53). A circuit's parts worked by hand take a touch first,
unless the mechanism works them (`fude_zoom_coupling_works`: an encoder or a pot on a shaft, a slot sensor's beam):
then it goes to the crank on them (§57).

## With a circuit

`fude_zoom_coupling_build` pairs, by object, each circuit motor, servo, stepper, encoder or pot with the world's
shaft at it, each solenoid with its plunger, and notes each push button's middle and each slotted sensor's beam.
`fude_zoom_coupling_step` then runs both, a tick at a time:

1. **The mechanism into the circuit**: each shafted motor's speed (`spin`, its back-EMF), a servo's horn where its
   part is, a button pressed while a moving part covers its middle (`fude_zoom_mech_world_covers`), a beam blocked,
   a stepper's rotor, an encoder's detents and a pot's wiper (270° end to end) from how far their shafts turned, a
   plunger's travel. A button or beam that changes is a jump (`fude_zoom_circuit_jump`).
2. **The circuit on by one tick** (`fude_zoom_circuit_advance`, at most 200 of its own steps; short of them it runs in
   slow motion beside the mechanism).
3. **The circuit into the mechanism**: motors (`fzcp_drive`), servos (`fzcp_servos`, toward the angle their pulses
   say at a 25th of a second's pace, no harder than their stall torque, their strain read back as current), steppers
   (`fzcp_steppers`, to their field's nearest rest within a tick, as hard as `FUDE_ZOOM_COUPLING_HOLD`: a 28BYJ-48's
   step is 0.003 rad, too fine for a proportional pace to keep up), solenoids' pulls less their springs
   (`FUDE_ZOOM_COUPLING_PULL`).
4. **The world ticks.** The wires' currents are worked out once, after the last tick, for drawing.

**A motor** is the delicate one. Its torque from its current at the last step would be explicit, and unstable when it
is stiff (a motor shorted as a brake). So `fude_zoom_circuit_motor_load` takes the circuit, as it loads the motor, as a
straight line over the tick: it solves the motor's block again with 2% more back-EMF and puts everything back,
giving a conductance (how much more current one volt less makes) and the back-EMF at which no current flows. The
shaft is driven toward the speed of that back-EMF, with no more torque than the current at its present speed makes:
open, free; shorted, held hard; never past (§42). When the circuit cannot be solved the world still takes the tick it
was on, and stepping stops there.

## Drawn as it plays

What the world draws itself is marked in `mech_lifted` (moving bodies, springs, ropes, pulleys, tracers, belts,
dampers), and the renderer leaves those objects out (`renderer.lifted`). `fude_zoom_page_render_mech` then draws strain,
springs as zigzags between their ends, ropes, dampers (a cylinder and a sliding rod), each moving part's own drawing
moved by the world (`fude_zoom_render_object` with `fude_zoom_mech_world_move` composed in, so what is drawn on a drawn
body moves with it), and belts. A solenoid's plunger, a follower and a worm's threads are their symbols'
`FUDE_ZOOM_SYMBOL_MOVES` parts, drawn as far along as they have gone (`fude_zoom_page_moving_parts`, the renderer's
`moved`). Not playing, each body has its mark (`fude_zoom_page_render_bodies`: ⊕ moving, the ground's hatch fixed).

## What is kept, threads, caches

- **Kept between frames**: the world (its plan copy, bodies, joints, moves) for the whole play; the time left over
  under a tick (`left`, in the world or the coupling); the trails, and their arrays' memory across stops; the page's
  scratch for trails and belts. Nothing is allocated a frame once they have grown.
- **Threads**: main thread only. The world is made with `rde_physics_2d_world_create`, which runs Box2D with no worker
  threads.
- **Caches**: the topics' kind-to-part table (`fude_zoom_mech_of_kind`, through `fude_zoom_symbol_topic_part`: a kind
  found once, never invalidated, the catalogue being static); the renderer's symbol geometry. A plan is not a cache:
  it is built afresh at each Play and each Restart.

## Invariants a change must keep

- **The world never reads the canvas after it starts**, and nothing writes the canvas while it plays.
- **Every joint the world makes goes through `fzw_note`** with a strength (0: never breaks), so strain, events and
  `fude_zoom_mech_world_broken` see it. A joint made on others (a gear on hinges) lists them in `on[]`, so breaking
  them breaks it first; any copy of a joint pointer (motors, shafts, cranks) is cleared in `fzw_break`.
- **The coupling holds indices**: it is built again whenever the circuit or the world is.
- **Units**: home units are millimetres, world units `k` a millimetre; turn forces and torques with `1e-3 / k`.
- **The plan's unit sets every tolerance**: a part drawn small on purpose (a tracer) must stay out of it.
- **Symbol ids are the catalogue's ids**, and the symbol table is append-only (`symbol.h`).

## How to change common things

### Add a mechanism part

1. `mech.h`: a `FUDE_ZOOM_MECH_` kind at the end, with a comment saying what it does.
2. `symbol.c`: an `FZS` entry at the end of the table, family MECHANISMS, drawn by `fzs_mech`, with its name's text id
   in `zoom/strings.py` in every language, that id appended to `page_diagrams.c`'s `FUDE_ZOOM_PAGE_SYMBOL_NAMES`, and
   `symbol.h`'s `FUDE_ZOOM_SYMBOL_KINDS` one more (both tables are checked against it as they compile).
3. `mech.c`: its `FZM_PARTS` entry (the symbol's id, kind, teeth, holes, starting text); its lines in
   `fzm_draw_turning`, `_linkage` or `_body`; in `fzm_bodies`, whether it is fixed, what its text sets, its size for
   the unit; what is "on" it (`fzm_on`); its joints, in `fzm_hinges` through its holes, or in a step of its own called
   from `fude_zoom_mech_plan_build` (a new plan array goes in `_init`, `_destroy`, `_copy` and the build's clearing).
4. `mechrun.c`: its shape, mass and layer (`fzw_shape_plain` or `fzw_shape_more`); its joints in `fzw_start_*`, each
   through `fzw_note` with a strength; a force of its own in `fzw_springs_of`.
5. `page_mech.c`: whether the world draws it (`mech_lifted` in `fude_zoom_page_play_build`) and how
   (`fude_zoom_page_render_mech`), or its MOVES parts (`fude_zoom_page_moving_parts`).
6. Tests in `tests/mech`, run as Play runs it, against its physics; an example if it needs showing.

## Where the tests are

- `tests/mech` (`tests/run.sh mech`; RDE's physics and Box2D built into it, with the sanitizers): gear pairs and
  trains of light and heavy gears from either end (every gear within 5 mrad, speeds within 1%), energy kept and turned
  back, a minute's run, racks at the rim, at their ends and falling, Atwood's machine at g/3 and a block and tackle;
  then the canvas's own: Borja's mechanism, parts placed zoomed, strength, tracers, the hand and cranks, belts, cams,
  ratchets, dampers, worms, pins and keys, trail runs, big mechanisms, every example; and coupled to circuits (motors
  at their speeds and loads, a dynamo, a shuttle between limit switches, a turn counter, servos, solenoids, steppers,
  shaft sensors).
- `tests/zoom`: `test_mechanisms` (the plan: hinges, motors, meshes, springs, ropes; a pin through twenty squares,
  every one joined) and `test_drawn_bodies`.
- `tests/sim`: `test_bodies` (materials, areas, centres and inertia against their formulas, simplifying, convex pieces
  of random stars).

## Open points

Found while writing this note and mending what it found, against the code of 2026-10-15; each is in that day's
reports.

- Building the plan tries every body against every other in most of its steps (`fzm_hinges`, `fzm_meshes`, `fzm_pins`,
  `fzm_ropes`...): quadratic, where the circuit's build uses a grid. Kept so on purpose, and `mech.c`'s
  `fude_zoom_mech_plan_build` says why: it runs once a Play over a few dozen parts, measured at 0.1 ms or less (the 55
  bodies of Everything at once 0.1 ms, the Gearbox's 12 under 0.01 ms in an optimised build), so a spatial lookup
  would buy nothing that shows. Worth looking at again only if canvases of hundreds of parts are played.
- One fixed cap is left: `fzm_pinned_there` follows at most 32 bodies on one pin (a pin holds few). `fzm_pins` joins
  every moving body under a pin, however many, and Make body takes every drawing the lasso holds
  (`fude_zoom_page_body_all`): both were capped (16 and 64) and silently left the rest out.

A spring's drawing keeps its own symbol's index in the plan (`fude_zoom_mech_spring.body`, as a rope's does), so
`fude_zoom_page_render_mech` reads its colour and width from it directly; it was found every frame by searching every
body.
