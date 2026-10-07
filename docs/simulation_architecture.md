# Simulation architecture: circuits, logic, mechanisms, custom parts

Sketching's simulations — electronics (analog and digital), mechanisms, and whatever comes after (PCBs, thermal, optics) — built as one system, not one per topic. This document is the design every later piece follows. Status of each part is at the end.

## 1. What it must do

- **Analog circuits** as SPICE does them (DC, transient, AC; real device models), at interactive speed.
- **Digital logic**: gates, flip-flops, counters, buses, tri-state, microcontrollers running sketches; fast (events, not equations) and mixed with analog where they meet.
- **Custom parts**: anything built from other parts — a multiplexer from gates, a full adder, an ALU from adders, a motor driver from transistors — saved as a part with N inputs and M outputs (any ports, any domains), used like any other, nested to any depth, edited later with every use following.
- **Mechanisms**: rigid bodies, joints, motors, springs, ropes, gears; and **bodies drawn by hand**, each with its own physical properties.
- **Both together**: a circuit's motor turns a mechanism's wheel, a mechanism's cam presses a circuit's switch, a shaft turns a potentiometer.
- **Later, PCBs**: the same parts and nets become a board — footprints, references, a netlist, a layout, Gerbers.
- **Extensible** everywhere: a new kind of part is a new model, a new kind of physics is a new domain, without touching the rest.

## 2. Layers

```
  canvas (zoom/)            what is drawn: symbols, wires, drawings, ports, bodies
      │  compile  (canvas → netlist: pure, tested)
  netlist (sim/)            definitions, instances, ports, nets — hierarchical
      │  elaborate (flatten, check, bridge domains)
  flat circuit (sim/)       primitives with their nets, per domain
      │  run
  engines (sim/)            analog (MNA) · digital (events) · mechanical (RDE physics) · …
      │  publish
  state                     each net's value, each part's readings, each body's place
      │  read
  view (zoom/)              overlays, instruments, moving parts — never the reverse
```

- The **canvas** is never written by a simulation, and the engines never read the canvas: they read a netlist compiled from it (`circuit.c`'s build and `mech.c`'s plan today; `sim/` from now on).
- Everything below the canvas is **pure C with no canvas, no UI, no RDE rendering**, and is unit-tested on its own (`tests/sim/`). Only the mechanical engine uses RDE (its physics).
- The **view** draws from the published state only (as a game's view draws its world).

## 3. The data model

### 3.1 Domains and ports

A **domain** is a kind of connection, with what flows through it:

| domain | across (a net's value) | through (what flows) | engine |
|---|---|---|---|
| ELECTRIC | voltage | current | analog (MNA) |
| LOGIC | 0, 1, Z (released), X (unknown or conflict) | — (drivers) | digital (events) |
| ROTATION | angle, angular speed | torque | mechanical |
| TRANSLATION | position, speed | force | mechanical |
| SIGNAL | a number (a sensor's reading, a PWM duty) | — | whichever reads it |

More can be added (THERMAL: temperature / heat flow; OPTICAL: light) without changing the others: a domain is an id, its net values, and the engine that solves it.

A **port** is a part's connection: a name, a domain, a direction (IN, OUT, INOUT, PASSIVE — electrical pins are passive), a width (a LOGIC port may be a bus of 1–64 bits), and where it sits on the part's symbol (side, place along it). Pins, a shaft, a mounting point, a sensor's output are all ports.

### 3.2 Definitions

A **definition** is what a kind of part is. Every part on a canvas is an instance of one.

- **Primitive**: its behaviour is a **model** (C code registered by id: `"resistor"`, `"and"`, `"dff"`, `"dc motor"`, `"body"`). Its ports and parameters come from the model (some depend on parameters: an AND's input count).
- **Composite** (a custom part): its behaviour is the parts inside it — **instances** of other definitions (primitive or composite), the **nets** that join them, and which nets are its **ports**. A multiplexer made of gates, a 4-bit adder made of full adders made of gates.
- **Behavioural** (later): a truth table, boolean expressions per output, a state machine, or a script — for parts easier to say than to draw.

Every definition has:
- an **id** (unique; a user's are `user/<name>`), a **name** and description (its language's), a **version** (bumped on each edit), an **origin** (built in, the user's, a shared library's);
- its **ports** in order, and its **parameters** (name, unit, default, limits; a composite's can be used inside it: `R = 2 * Rbase`, read with `calc.h`);
- its **symbol**: a generated box (inputs left, outputs right, power top/bottom, names by the pins) or SVG art (`symbolart.h`);
- for a composite, the **drawing** it was made from (its canvas objects, as My pieces keeps them), to show and edit it again;
- later, its **footprints** (PCB): which packages it can have, and its pins' pads in each — and whether a composite goes onto a board as its parts (a *sheet*) or as one thing (a *module*: an Arduino, a relay board).

### 3.3 Instances and nets

An **instance** is a definition used: which definition (id and version), its **reference** (R1, U3: kept, for the parts list and the board), its parameters' values (as typed: `4.7k`, an expression), and, for each port, the **net** it is on.

A **net** is what is joined: a name (given, or `n12`), its domain (from its ports: a mix of LOGIC and ELECTRIC is allowed and bridged, others are errors), and its ports. On the canvas, nets come from wires, wire junctions, a breadboard's strips, pins touching, and net labels (same name: same net; power symbols are labels).

### 3.4 Elaboration

Elaborating a netlist **flattens** it: each composite instance replaced by its insides (their names prefixed by its path: `U2/U1`), its ports' nets joined to the nets outside, its parameters given values; nets merged (union-find). The result is a **flat circuit**: primitives, each with its model, its parameter values and its ports' nets.

Elaboration also **checks** (each problem said in words, with the part it is about): a definition missing or recursive (a part made of itself), a port's domain against its net's, an input driven by nothing (it reads Z, then X), outputs fighting, a source shorted, a node joined to nothing analog.

And it **bridges domains** where nets meet: a LOGIC output on an ELECTRIC net drives it through a source and a resistance (its high and low levels, its family's: 5 V TTL, 3.3 V CMOS); a LOGIC input on an ELECTRIC net reads it through thresholds with hysteresis. Pure logic nets never touch the analog solver.

## 4. Models

A model is a table of functions, each optional, one set per domain it works in:

```c
typedef struct fude_sim_model {
    const c8* id;
    // its ports and parameters (some depend on its parameters: an AND's inputs)
    u32  (*ports)(const f64* params, fude_sim_port* out, u32 max);
    const fude_sim_param_def* params; u32 param_count;
    u32  state;                               // numbers it keeps, each instance
    // LOGIC
    void (*start)(fude_sim_ctx*, u32 prim);   // its outputs at the start
    void (*logic)(fude_sim_ctx*, u32 prim);   // an input changed: its outputs, after its delay
    void (*wake)(fude_sim_ctx*, u32 prim);    // a time it asked to be woken at (a clock, a timer)
    // ELECTRIC (analog engine)
    void (*stamp)(fude_sim_ctx*, u32 prim, fude_sim_mna*);   // its share of the system (linearised: Newton)
    void (*accept)(fude_sim_ctx*, u32 prim);                // a step accepted: its state on (charges, temperatures)
    f64  (*next)(fude_sim_ctx*, u32 prim);                  // its next breakpoint (a square wave's edge)
    // ROTATION / TRANSLATION (mechanical engine)
    void (*mech)(fude_sim_ctx*, u32 prim);    // its forces and torques on its bodies; what it reads of them
    // what it shows (an LED's light, a meter's reading, a motor's speed)
    void (*probe)(const fude_sim_ctx*, u32 prim, fude_sim_probe* out);
} fude_sim_model;
```

Models reach the engines only through the context (`fude_sim_in`, `fude_sim_out(… value, delay)`, `fude_sim_wake`, `fude_sim_state`, the MNA's stamps, a body's speed and torque), so an engine can change inside without models changing. A new kind of part is one file of models; it is registered once and is in the library.

## 5. Engines and how they run together

- **Digital** (events): an event queue (time, port, value); a net's value resolved from its drivers (0/1 win over Z; 0 against 1 is X); when it changes, the models reading it are asked; delays (each model's, by default a few nanoseconds) order them. Clocks and timers wake themselves. Thousands of gates at interactive speed.
- **Analog** (MNA): the nodes' voltages and the sources' currents solved together; Newton for what is not linear, with SPICE's limiting; trapezoidal integration (backward Euler to start and after a jump); steps that grow and shrink with the circuit's error; breakpoints at sources' edges; a sparse LU. (Today's `circuit.c` is the start of it: §27 of the design notes; it moves here, model by model.)
- **Mechanical**: RDE's 2D physics (Box2D v3 with RDE's own pulley and gear joints, `mechrun.h`), stepped in 240ths of a second.

A **scheduler** runs them together in steps of the mechanical engine's (1/240 s; finer when analog needs it):
1. digital events up to the step's end (an analog net crossing a threshold makes one at its crossing);
2. analog solved to the step's end (logic outputs as sources at their values; motors' back-EMF from their shafts' speeds);
3. mechanical stepped (motors' torques from their currents, solenoids' forces, springs);
4. sensors read the bodies (a shaft's angle into a potentiometer, a body pressing a switch, an encoder's pulses) for the next step.

What each engine finds is **published** after each step for the view: each net's value, each part's probe, each body's place.

## 6. Custom parts (macros)

### On the canvas
- **Ports** are what a circuit is tried with: its **logic inputs** (switches) are the part's inputs, its **logic probes** its outputs, each named by its text (else IN1…, OUT1…). (Electric, rotation and translation ports — power pins, shafts — will be port symbols of their own.)
- **Make part** on the lasso's row: what the lasso holds (parts, wires, inputs and probes) becomes a composite definition — named by a typed text in the lasso (else "Part N"), its ports in order (inputs on the left sorted down, outputs on the right), drawn as a block with its pins, saved in **My parts** (`<saves>/parts/<name>.part`) — and one of it is put beside what it was made of. Made again with the same name: a newer version, every one of it following (0.1.51).
- **My parts** is a tab of the library: each custom part a tile, placed as any part is; its pins are wired as any pins.
- An instance tapped: **Look inside** (its drawing, read-only; on the deep-zoom canvas, zooming into the part shows its insides), **Edit part** (its definition opened: saved, its version goes up, and every instance follows — on this canvas now, on others when opened, asked), **Unpack** (its insides put on the canvas in its place).

### Stored
- A definition is a small text file (`<saves>/parts/<id>.part`): readable, diffable, shareable — its header (id, name, version, description), ports, parameters, instances with their port nets, and its drawing (as My pieces keeps drawings). Every canvas that uses a custom part keeps a copy of its definition, so a canvas opened elsewhere still works; a newer one in My parts is offered.
- Parameters pass down: an instance's values, then the definition's defaults; expressions inside may use them.
- Buses: a port of width N is N bits on one wire; splitters join and part them.

### Behavioural parts (later)
A truth table or one expression per output (`Y = S1 ? (S0 ? D3 : D2) : (S0 ? D1 : D0)`), a state machine, a script — a definition whose model is generated from what is typed.

## 7. Mechanisms

### Bodies and joints
- A **body** is a rigid shape: a library part (link, plate, gear, wheel, weight, crate, slider, rack…) or **a drawing made into a body** (below).
- **Joints**: a pin (hinge) where holes meet, a **pin** part joining whatever bodies are under it, a **weld** gluing them, a slider along a rail, springs, ropes, pulleys, gears meshing by touch, motors.
- Collision **groups**: parts of a linkage pass over each other; bodies, weights, walls collide.

### Drawings made into bodies
- Anything closed that is drawn — a pen loop, a shape, a path, a fill — lassoed, **Make body**: it becomes a body, keeping its drawing (what is drawn moves with it).
- Its **properties** (a card): moving or fixed; a material (wood, steel, aluminium, rubber, plastic, glass — each its density, friction and bounciness) or its own; its mass (from its area × density, or given); friction; bounciness; what it collides with; its starting speed and spin later.
- Its shape: its outline in the home frame, simplified, cut into convex pieces (at most eight corners each: the physics' polygons) — a concave shape is several pieces of one body.
- An **open** line made into a body is fixed ground: a ramp, a track, a funnel (its line as a chain of thin capsules).
- Pins and welds placed on it join it to the rest; holes are where pins go.
- Stored as an attribute on the drawing (a BODY object naming it by its id, as constraints and wires name theirs), so the drawing stays a drawing, and the body follows it when it is moved or re-made.

### With circuits
Ports in ROTATION and TRANSLATION are a body's: a **motor** symbol over a body's hole is its shaft (wired: its torque from its current, its back-EMF from its speed; not wired: an ideal speed, as today); a **servo** turns to the angle its PWM says; a **potentiometer** or **encoder** on a shaft reads its angle; a **limit switch** is closed while a body presses it; a **solenoid** pushes a plunger body; a motor turned by a mechanism is a generator.

## 8. PCBs (later; why the model is as it is)

- Every primitive gets footprints (a resistor's axial or 0805, an LED's 5 mm or 0603, DIP-14, a module's own); an instance keeps its chosen one.
- References (R1, U3) are kept from the start, so a board and a schematic agree.
- A netlist out (KiCad's, SPICE's) from the flat circuit; then a board canvas: its outline drawn (a drawing made into a board, as into a body), footprints placed from the netlist, the ratsnest, traces drawn with the wire pen on copper layers, design rules checked, Gerbers and drill files out.
- A composite goes on a board as its parts (a sheet) or as one module.

## 9. How it is tested

Each layer by itself, with answers known:
- netlist and elaboration: flattening, nested parts, port joins, recursion and domain errors, buses;
- digital: every gate's truth table, a multiplexer made of gates as a custom part (all its inputs), a 4-bit adder of full adders of gates (all 256 sums), counters and flip-flops on a clock, conflicts and tri-states;
- analog: an RC's time constant, an LC's frequency (no damping with the trapezoidal rule), a diode's drop, a transistor amplifier's gain, a 555's frequency, against their formulas;
- mechanical: pendulum periods, an Atwood machine, gear and rack ratios, a slider-crank's stroke;
- together: a motor's speed against its voltage and load;
- definitions written and read back the same.

## 10. Where it stands, and the order of work

| step | what | status |
|---|---|---|
| 0 | this design | written (0.1.51) |
| 1 | `sim/` core: definitions, ports, domains, library, hierarchical netlists, elaboration, the text format; the digital engine and its logic models (gates of any inputs, buffers, tri-states, flip-flops with set and reset, clocks, constants, a memory); the chips (74HC…) as part texts | 0.1.51 |
| 2 | drawings made into bodies (Mechanisms) | 0.1.51 |
| 3 | the canvas compiled into `sim/`'s netlist (every logic part: `zoom/logic.h`), bridges where logic meets the analog circuit, **Make part** (logic inputs and probes its ports), custom part symbols, My parts kept and learnt | 0.1.51 |
| 3b | Look inside, Edit part, My parts as a library tab, each canvas keeping a copy of the definitions it uses | next |
| 4 | the analog engine moved into `sim/` model by model, with the solver's core (§34 of the design notes: trapezoidal, adaptive steps, analytic Jacobians, sparse LU, limiting) | |
| 5 | mixed nets bridged; the scheduler running digital, analog and mechanical together | |
| 6 | instruments: oscilloscope, function generator, multimeter, ratings | |
| 7 | electromechanical parts: motors with back-EMF, servos, encoders, potentiometers on shafts, limit switches, solenoids | |
| 8 | models in depth (diodes, BJTs, MOSFETs, op-amps, transformers, relays, batteries, sensors) | |
| 9 | microcontroller boards running sketches; I²C/SPI parts | |
| 10 | behavioural parts (truth tables, expressions, state machines) | |
| 11 | analyses: operating point, sweeps, AC (Bode), Monte Carlo | |
| 12 | PCBs: footprints, netlist out, the board canvas, Gerbers | |
