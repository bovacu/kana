# The circuit simulator

How the canvas's circuits are worked out and run today: the analog solver in `fude/zoom` (`circuit.c` and its
`circuit_*.c` modules), the logic that runs on `fude/sim`'s digital engine (`zoom/logic.c`), parts' ratings
(`limits.c`), the displays (`display.c`), and Play's loop. The plan this is heading for, and the status of each of its
steps, is `docs/simulation_architecture.md` (§10): it is not repeated here. The history of each decision is in
`docs/infinite_canvas_design.md` (§27, §35, §42, §46–48, §51–59); this note says how the code works now. Mechanisms,
and how a circuit and a mechanism step together, are in `docs/design/mechanisms.md`.

---

## What it is for, and where it sits

A person draws a schematic: parts (symbols of the diagram library) joined by wires. Play, on what the lasso holds,
builds a circuit from that drawing and runs it in time: voltages, currents along the wires, LEDs lit, logic levels,
parts that burn. The canvas is read to build the circuit and never written while it runs; the view draws what the
circuit is now, as a game's view draws its world.

| File | What it holds |
|---|---|
| `zoom/circuit.h` | the public types: a part kind (`fude_zoom_part`), a circuit (`fude_zoom_circuit`), its parts and wires, the models (`FUDE_ZOOM_MODEL_`) |
| `zoom/circuit_parts.h` | what the modules share (prefix `fzc_`), and its map of which module holds what |
| `circuit.c` | the coordinator: init and destroy, build, reset, run in time (`fude_zoom_circuit_advance`), jumps |
| `circuit_catalogue.c` | the catalogue (`FZC_PARTS`): each kind's pins, model and look; a breadboard's holes |
| `circuit_read.c` | a part's text read into its numbers (`fzc_read`): "4.7k", "5V 50Hz", "red", "on" |
| `circuit_wires.c` | wires' numbers, routes, following moved parts, orphans; the grid of segments (`fzc_seg_grid`) |
| `circuit_build.c` | the circuit from the canvas (`fude_zoom_circuit_build_in`) |
| `circuit_blocks.c` | rails held, pins that only sense, the unknowns in blocks, each island's reference |
| `circuit_stamp.c` | each model's share of its block's system; the non-linear devices |
| `circuit_solve.c` | systems shaped and solved, Newton's method, quiet blocks, the wires' currents, one solve (`fzc_at`) |
| `circuit_step.c` | after a step: what parts show and keep, limits, the next step's size, breakpoints, sound |
| `circuit_play.c` | what Play asks: a tap, an oscilloscope's knob, a motor's load, the tags' size |
| `circuit_draw.c` | each part's lines, for `symbol.c` |
| `logic.c` | the canvas's logic compiled into one `sim/` netlist, bridged to the analog side; custom parts |
| `limits.c`, `limitsform.c` | each part's ratings, how it heats and burns; the Limits card |
| `display.c` | 7-segment panels, LED matrices, bar graphs, panel meters, character LCDs, the oscilloscope |
| `sim/` | `def.c` (definitions, the part text), `elab.c` (flattening), `digital.c` (the event engine), `logic.c` (its models), `chips.c` (74HC and CD4000 chips as part texts), `sparse.c` (the sparse LU) |

The analog engine is still in the canvas layer (`zoom`); only the digital engine and the sparse solver are in `sim`.
The page drives everything from `page_mech.c` (Play) and draws it from `page_circuit.c`.

## The main types and who owns them

- **`fude_zoom_part`**, a kind of part: static, in `FZC_PARTS`. Parts made from a text are made once and kept for the
  app's life, because circuits hold pointers to them: a chip or a custom part (`fude_zoom_logic_part`, list
  `fzlg_parts` in `logic.c`, keyed by template, package, definition id and version) and a sized display
  (`fude_zoom_display_part`, list `fzd_parts` in `display.c`). They are never freed.
- **`fude_zoom_circuit`** is the caller's struct. The page owns one (`page.h`: `circuit`); Make part and a template's
  keep build short-lived ones on the stack. `fude_zoom_circuit_init` puts every array on the std allocator;
  `fude_zoom_circuit_destroy` frees them. It owns its logic (`fude_zoom_logic`), which owns its netlist, its flat
  circuit and its run.
- **`fude_zoom_circuit_part`**: one part in the circuit. Its pins' nodes and currents live in the circuit's
  `pin_nodes` and `pin_amps` (`pin_room` places from `pin_at`, at least `FZC_PIN_ROOM`, 64), so a model that reads a
  pin past its part's own reads one joined to nothing. `value[8]` is what its text says, `state[8]` what it remembers;
  anything bigger (a display's glow, an LCD's controller, a scope's sweeps) is in the circuit's `store`.
- **`fzc_ctx`**: one solve's context (the step's length, its end time, trapezoidal or backward Euler), on the stack.
- **The logic library** (`fude_zoom_logic_library`): a static `fude_sim_lib` in `logic.c`, made on first use: the
  simulation's models, the chips, and My parts as they are learnt.

## The life of a circuit

```
Play pressed   fude_zoom_page_play_build: fude_zoom_circuit_build_in(scope), fude_zoom_circuit_reset,
               fude_zoom_circuit_build_in again (every part fresh from its text)
each frame     fude_zoom_page_play_step: canvas edited? fude_zoom_circuit_build_in again, states kept
               coupled with a mechanism? fude_zoom_coupling_step : fude_zoom_circuit_run(dt, 400 or 2000 steps)
               events told, pace measured, sound queued
drawn          fude_zoom_page_render_circuit: current's dots, volts tags, what each part shows
Stop           fude_zoom_page_play_stop: parts and wires cleared, sound closed
```

Play builds twice on purpose: the first build says whether anything plays, the reset lets its parts go, and the
second reads every part afresh (nothing charged, each switch as its text says). While it plays, a change to the canvas
(its `revision`) builds the circuit again with each part's state kept, so a value typed or a wire drawn takes effect
without starting over.

## From a canvas to a circuit

`fude_zoom_circuit_build_in` works in named steps (`fzc_build_*`) sharing a context (`fzc_build`). Everything is put
in the home frame's units first, so parts at different depths join. Nothing in it goes through everything for each
thing: a canvas may hold thousands of parts and wires.

1. **Parts** (`fzc_build_read`, `fzc_build_part`). A live, shown SYMBOL whose kind is a part's
   (`fude_zoom_part_of`, through the topics' one kind-to-part table, `fude_zoom_symbol_topic_part`). A custom part's
   or a sized display's pins come from its text (`fude_zoom_part_of_text`). The part's whole text is read
   (`fzc_text_of`, then `fzc_read`): numbers with metric prefixes, words in any of the apps' languages. A part still
   there from the last build (same object, same part pointer) keeps its state, switches, drives, heat and burnt flag;
   a new one starts from its text. Its limits are its LIMITS attribute's (`props.h`), else those of the real part its
   text names, else a typical one's (`fude_zoom_limits_typical`). Its grid unit (a tenth of a resistor's length at
   its size) is how near counts as touching.
2. **Wires in scope** (`fzc_build_scope`). Play's scope is the lasso's: its parts, the wires it holds, the wires
   joined to its parts, and the wires joined to those, found by a walk over a list of joins built once from a grid
   of segments (`fzc_seg_grid`) rather than every wire against every other.
3. **Terminals**: every pin (with its lead's inner end, 45% of the way in), then every wire end.
4. **Joins**, in two RDE disjoint sets: `net` (one node) and `touch` (one place, for the wires' currents). A wire's
   ends are one net; each end joins the pin it keeps by id (`fzc_build_kept`; a file older than 0.1.49 finds its part
   by the nearest pin, `fude_zoom_wire_part`); terminals within 0.3 of the smaller grid unit touch, never a part's own
   pins (`fzc_build_touching`, sorted along x); a free wire end along another wire is a junction, and that wire is cut
   there (`fzc_build_junctions`, `fzc_build_cuts`, at most `FUDE_ZOOM_WIRE_PIECES`); a free end on a pin's drawn lead
   is on the pin (`fzc_build_leads`), because a person who drops a wire on a gate's lead believes it is joined. Ends
   held by nothing are `loose`. Same-named pins inside a part (a board's GNDs, compared exactly so a CD4511's `a` is
   not its `A`) and a breadboard's strips are one net (`fzc_build_same_names`).
5. **Ground** (`fzc_build_ground`): every ground symbol, else a board's GND, else the first source's minus.
6. **Nodes** (`fzc_build_nodes`): each net that a pin of a part with a model is on, but a pin joined to nothing
   (a 555's excepted: its CTRL left open is its own divider's). Ground is 0. Sets are named by their smallest
   terminal and numbered by walking terminals in order, so the numbers never depend on the order joins were made
   (`test_circuit_nodes_order`). The voltages array keeps its old values, so Newton starts where the last build left
   it.
7. **Vertices** (`fzc_build_vertices`): the `touch` sets numbered, for the wires' currents.
8. Then **its logic** (`fude_zoom_logic_build`), **rails held and pins that sense** (`fzc_held`), **blocks**
   (`fzc_blocks`), a **jump** and its **longest step** (`fzc_step_most`).

**Rails held.** A node a supply rail is on (and no rail of another voltage) is known, as ground is: outside every
block, at the rail's volts. A stamp across it puts what it drives on the other node's right-hand side (`fzc_g`,
`fzc_nonlinear`), and the rail's current is what the rest on its node take (`fzc_held_currents`). Before rails were
held (§59), one 9 V rail made a whole circuit one block. Two rails of two voltages on one node are a short, solved as
before so that it is told.

**Pins that only sense** (`fzc_sense_pins`: a 555's TRIG, THRES and RESET, an oscilloscope's probes, a logic input
bridge) stamp only a leak and are read once solved, so they do not join their node to the part's block.

**Blocks** (`fzc_blocks`). The unknowns are grouped by what each part's pins join (not ground, not held nodes, not
nodes only logic is on, not through sensing pins), plus the pairs a stamp was found to write across (`couplings`).
Each block is solved on its own: the independent circuits on a canvas, and the corners of one circuit a rail feeds.
A node only logic pins are on is the logic's own net, at its level's volts, so it is in no block (`fzc_outside`): as
an unknown nothing analog stamped it, GMIN pulled it toward 0 V each solve and its level was put back after, which
damped its block's first Newton iterations (taking them out, Everything at once without its mechanism went from 1,619
to 1,160 ms, every answer within 3·10⁻⁹; `test_logic_nodes` checks they stay out, at their levels' volts).
**References** (`fzc_blocks_reference`): an island that ground does not reach (a battery and its LED elsewhere on the
canvas, an optocoupler's or a transformer's far side, `fzc_side`) is held at 0 V at one node, through 1 S
(`FZC_REFERENCE_G`): a board's GND, else a source's minus, else its first node. Without it such a block is held only
by GMIN, nearly singular, and the sparse solve wandered.

## Solving: stamps, the sparse LU, Newton

**Stamps** (nodal analysis, `circuit_stamp.c`). Each model writes its share through four primitives: a conductance
(`fzc_g`), a current (`fzc_i`), a voltage source behind a resistance (`fzc_vsrc`) and a non-linear element
(`fzc_nonlinear`). Every source is stamped as its Norton equivalent (a DC source behind 1 mΩ, a battery behind 0.5 Ω,
a logic output behind 25 Ω), so the unknowns are node voltages alone: no branch rows. That is what blocks, rails held
and the quiet keys are built on, though no note says it was chosen for them. A non-linear element is a function from
its pins' voltages to their currents (`fzc_diode_fn`, `fzc_bjt_fn`, `fzc_mos_fn`, `fzc_opamp_fn`...); its Jacobian is
found by nudging each voltage by a ten-millionth, not analytically. Exponentials are straightened past e^40, and a
diode's past a kiloampere (`fzc_exp`, `fzc_diode_fn`), so Newton's steps stay finite. Breakdowns (an opening contact's
arc at `FUDE_ZOOM_ARC`, 50 V; a transistor past its voltage rating) are straight lines of 1 S a volt, so the 2 V
damped steps reach them and stay. A burnt part is open but for such an arc between its pins. `fzc_stamp` asks each
family (`fzc_stamp_load`, `_source`, `_semiconductor`, `_analog`, `_display`, `_digital`) in turn.

**The systems** (`fzc_shape`). A block's sparse system needs to know where entries go before their values. So every
part is stamped once with `recording` set: `fzc_entry` notes each (row, column) it would write (`joins`) and writes
nowhere. The joins are dealt to their blocks and each `fude_sim_sparse` is shaped. A stamp that later writes where
its block has no place (a chip's output turned on) is noted, and the block is shaped again before it is solved (at
most `FZC_RESHAPES`, 8, a solve). A stamp across two blocks is noted as a coupling and the blocks are made again
(`reblock`, at most 8 times a solve), so a part that does more than its pins said costs time, never a wrong answer.

**The sparse LU** (`sim/sparse.c`). Shaped once: the unknowns ordered by minimum degree (an `rde_heap` of degrees),
the fill worked out, rows compressed. It factors without swapping rows, so the shape never changes; a pivot smaller
than `FSS_PIVOT_SMALL` times its row is reported, and `fzc_block_solve` then copies the block out dense
(`fude_sim_sparse_dense`) and solves it with row swaps (`fzc_solve`), if it has at most `FZC_DENSE_MOST` (400)
unknowns. A gain ahead of what it drives, or a source straight across another, needs that.

**Newton's loop** (`fzc_newton_once`). Each iteration stamps every part whose block is still being solved
(`fzc_part_done`); a part in no block (its pins all on ground, held nodes or nodes only logic is on) writes into no
system, and its currents come from volts that do not move, so it is stamped once a solve, at the first iteration. It
adds `FZC_GMIN` (10⁻¹² S, SPICE's) to every diagonal so a node joined to nothing still solves, solves each block, and
moves the voltages. **Damping**: a block's step is scaled down so that no node in it moves more
than 2 V an iteration; this stands in for SPICE's junction limiting, which the code does not have. A block is settled
when no node moved more than 10⁻⁷ V plus 10⁻⁶ of its value; settled blocks are left alone for the rest of the solve.
At most 150 iterations. Not settled: the parts on the nodes that moved most are marked `suspect` (`fzc_suspect`), and
Play marks them in red.

**One solve at a time** (`fzc_at`): the quiet blocks found (`fzc_quiet_begin`); Newton; then the logic and what is
stepped from voltages (`fzc_digital`: a 555's flip-flop, a 595, an L293D, a ULN2003, a relay, a fuse) are worked out,
and while either changes, solved again (at most 16 times), every block woken when the logic changed; a node only
logic is on, no unknown, given its level's volts (0 V when released, Z, or unknown, X), for drawing and for what
reads it; still blocks noted (`fzc_quiet_end`); then `fzc_after`; then
time moves on, but only when it was solved.

## Time steps

- **Integration.** Capacitors and inductors (and a solenoid's coil, a transformer's windings) are stamped as their
  trapezoidal companions (2C/h and the last current carried), which keep an oscillation's size; and by backward Euler
  (C/h) on the step after a jump, because the trapezoidal rule rings after one. A capacitor across two known nodes
  (decoupling, rail to ground) is always backward Euler and left out of the step's bound: the trapezoidal rule carried
  its charging spike on, turned round each step, for ever.
- **Jumps** (`fude_zoom_circuit_jump`): a tap, a part added, a button pressed by a mechanism or put up by hand, a
  build. It asks for backward Euler next (`be_next`) and cuts the step to an eighth (`FZC_STEP_FIRST`, 1 µs, at
  least). A jump inside a step does the same: the logic or a stepped part settling within it (`fzc_at`), or a part's
  own step after the solve calling `fude_zoom_circuit_jump` (a thyristor fired, an encoder's contact, an LM3914's
  outputs, in `fzc_after`); `fzc_at` clears `be_next` before `fzc_after` and adds its own jump after, so neither is
  lost. `fude_zoom_circuit_advance` then plans the next step at an eighth of the one the jump was in, but not on the
  step right after a jump (`be_last`): with an edge in every step (logic oscillating faster than any step) the steps
  would only shrink to the least and show nothing more (`test_circuit_jumps`: a latch of two NOR gates still steps at
  a millisecond).
- **Adaptive steps** (`fude_zoom_circuit_advance`). The first step is 1 µs. After each, `fzc_after` works out
  `step_ratio`: how far each capacitor's curve bent (its current's change as volts, `|Δi|·h/C`) against 0.05 V and 2%,
  how far it went against a looser 0.5 V and 10%, and each inductor's current change against 1 mA and 2%. The next
  step is `0.9/√ratio` times this one, between a quarter and four times (`FZC_STEP_GROW`), never under 0.1 µs
  (`FZC_STEP_LEAST`) nor over `step`. Bounding the bend, not the travel, lets an RC follow its curve in a few steps a
  time constant (§59).
- **The longest step** (`fzc_step_most`): 1 ms, a 40th of an AC source's or a clock's period, a 50th of an
  oscilloscope's sweep, never under 1 µs.
- **Breakpoints** (`fzc_breakpoint`): a step never passes a clock's next edge (landed just past it: `fzc_clock_phase`
  adds a whisker, so a sum of steps a rounding short still counts as past), an AC source's 40th of a period, or the
  time a 555's capacitor will reach its threshold as it goes now (`fzc_555_ahead`). The last step of an advance lands
  on its end, never leaving a sliver.
- **Quiet blocks** (§59). A block is left as it was for a step when nothing in it moved last step (no node, its
  capacitors' currents and inductors' volts next to none: `fzc_quiet_end`), nothing in it changes by itself (an AC
  source, a clock, a board: `block_live`), and each of its parts is as it was when last solved: its **key**
  (`fzc_key`), compared bit for bit: its values, the state its stamps read (`fzc_state_read`), its spin, pushed and
  switched flags, burnt, and its pins' volts but where it senses. So a big circuit whose fast corner alone moves (a
  555 beeping a speaker) is stepped as fast as that corner.

## Logic, bridged to the analog side

The canvas's logic parts (gates, flip-flops, logic inputs and probes, chips, custom parts: `fude_zoom_logic_is`) do
not run in the analog solver. `fude_zoom_logic_build` writes them all as one composite of `sim/`
(`fzlg_build_netlist`) and elaborates it (`fude_sim_elaborate`). Its names are a convention the code reads back:
`P<object>` a part, `n<node>` a node only logic is on (with a probe `r<node>` to read it), `b<bridge>` a bridge's net
with its input `i<bridge>` and probe `o<bridge>`, and `zero` a net tied low for ports no pin is on.

- A node **only logic pins** are on is one of the logic's nets, so a latch of two NANDs drawn on the canvas holds. It
  is no unknown of the analog solve (above: Blocks).
- Where a logic pin meets **the analog circuit** (ground always counts) there is a **bridge** (`fude_zoom_bridge`). An
  output drives the node as a source of its supply's high (its VCC over its GND; `FUDE_ZOOM_LOGIC_V`, 5 V, for a part
  with no supply pins) or 0 V, behind `FUDE_ZOOM_LOGIC_R` (25 Ω), and nothing while it is Z, X, or unpowered (under
  1 V) (`fzc_logic_stamp`). An input is a small leak to its ground and reads the node against half its supply; an
  unpowered input reads low, not X, because a flip-flop reading its reset as X while the circuit powers up would hold
  X for ever.
- **Stepping** (`fude_zoom_logic_step`, called by `fzc_at` after each solve): the run is taken on to the step's end,
  each logic input's switch and each input bridge's level set where it changed, the run settled for a microsecond,
  and each output bridge's drive read (an INOUT pin's without its own input: `fude_sim_run_net_without`). A changed
  drive returns true, and the circuit is solved again within the same step.
- **Rebuilt**: the new run takes what the old held (`fude_sim_run_keep`), by each primitive's path, model and ports,
  states and drives both, since a latch of gates holds its bit in its nets. A reset (`fude_zoom_logic_forget`) starts
  it from nothing.

**`fude/sim` underneath.** `def.c`: definitions (a primitive's model, or a composite of instances and nets), the
library (an array of pointers, searched in full by id; one added with an id already there replaces it, its version
one more), and the text format. `elab.c`: a composite flattened depth first (`fse_expand`), nets joined through ports
in a disjoint set and numbered by their smallest member; domains checked (logic and electric may share a net, anything
else is an error); nesting deeper than `FUDE_SIM_DEPTH` (32) taken as a part made of itself; primitives found by path
through a hash map. `digital.c`: events in a heap ordered by time then creation; each port's drive, each net resolved
from its drivers (Z yields, 0 against 1 is X); a newer change to a port cancels a waiting one, so delays are
inertial; a run that fires more than 200 000 events in a call is oscillating, and its waiting drives are dropped while
its wakes (clocks) go on. `logic.c`: the models (inputs, constants, probes, clocks, gates of 1–16 inputs, buffers,
tri-states, D/JK/T flip-flops, latches, a memory). `chips.c`: the 74HC series and the CD4511 and CD4017, each a part
text, its ports in pin order with VCC and GND as electric ports.

## Custom parts

A custom part is a `fude_sim_def` composite whose id is `user/<slug>`. **Make part** (`fude_zoom_page_make_part`)
builds a circuit of what the lasso holds and calls `fude_zoom_logic_make`: its logic inputs become its inputs and its
probes its outputs, named by their texts (else IN1…, OUT1…), sorted from the top; a pin on ground or a rail is tied
to a constant. If one of that id exists, `fude_zoom_logic_keep_order` puts the new ports in the old order by name, so
what is wired to its pins on every canvas stays on them. It is then written and learnt from its own text
(`fude_zoom_page_part_keep`), so what runs is exactly what a reload would read.

**My parts** are files in `<save folder>/parts/`: `<slug>.part` (the text), `<slug>.piece` (the drawing it was made
from) and `templates.txt` (`custom.h`: which canvas is each part's template). `fude_zoom_page_parts_load` learns every
`.part` when the canvas's UI is first built; a file that cannot be read or learnt is logged and skipped. A canvas keeps
no copies: a symbol names its part by its text (`fude_zoom_logic_find`: id, then name, then the slug of a first name,
so a renamed part is still found).

The text (`fude_sim_def_write`, read back by `fude_sim_def_read`):

```
fude-part 1
id user/half-adder
name Half adder
version 1
port A logic in 1 left
port B logic in 1 left
port S logic out 1 right
port C logic out 1 right
net A
net B
net S
net C
inst xor U1 "inputs=2" A B S
inst and U2 "inputs=2" A B C
end
```

A learnt part replaces the older one (which is freed: nothing may keep a pointer to a definition across a learn) and
bumps `fude_zoom_logic_revision`, which the page's caches compare. Its part on the canvas is made again for the new
version, so every use follows. Editing a template canvas and leaving it makes the part again from all of it
(`fude_zoom_page_template_keep`).

## After a step: what parts show, their limits, sound

`fzc_after` runs once a solved step: each part's `shown` (an LED's brightness, a meter's reading, a motor's turn),
what it remembers (a capacitor's voltage and current for the next companion), the step ratio, and each part's own
step: a 555's threshold ahead, a servo's pulses timed and its horn turned, contacts' sparks, a thyristor latched or
let go, a solenoid's plunger, a stepper's rotor, an encoder's contacts, an LM3914's comparators, an LCD's controller
strobed on E falling (`fude_zoom_lcd_strobe`), a scope's sweep sampled (`fude_zoom_scope_step`), a display's LEDs
eased toward their current over `FUDE_ZOOM_DISPLAY_GLOW` (20 ms, so a multiplexed digit looks steady).

**Limits** (`fzc_limits`, `limits.c`). Each stepped part is measured from its pins' volts and currents
(`fude_zoom_limits_measure`: power, current, voltage, the wrong way round), a display's LED by LED. Past a limit it
heats by `(stress² − 1)·dt/τ` and burns at 1 (`fude_zoom_limits_step`; τ from `fude_zoom_limits_tau`: 0.05 s for a
junction, 2 s for a resistor, 20 s for a winding); below, it cools four times more slowly. Burnt is kept over
rebuilds until a reset. A source (τ 0) never burns; past its current for 20 ms it is told once
(`FUDE_ZOOM_CIRCUIT_OVER`). What happened goes into `events`, which Play tells (a part burnt before a spark before a
source's surge) and clears.

**Sound.** While a speaker plays, `fzc_sound` samples the speakers' currents together at the stream's rate in the
circuit's time (0.1 A full scale, a 20 Hz high-pass), at most a second unplayed. The page pushes them to an RDE audio
stream each frame, only at ×1 (`fude_zoom_page_sound_feed`, at most `FUDE_ZOOM_PAGE_SOUND_AHEAD` queued).

## Play's loop per frame and its budget

`fude_zoom_page_play_step` (called once a frame by `page_frame.c`, timed into `perf_play`):

1. The frame's time, at most 0.1 s, times Play's speed (×1, ½, ¼, 0.1); paused, a Step is a 60th of a second at it.
2. The canvas edited since the build (`play_seen`): built again in its scope, states kept, and the coupling rebuilt
   (it holds indices into the circuit's parts).
3. A push button let go too quickly is put up only once the circuit has seen it down for 0.1 s of its time
   (`FUDE_ZOOM_PAGE_PRESS_LEAST`): a tap's down and up can fall in one frame. Put up, it is a jump, as its press was
   (`page_mech.c`: `fude_zoom_page_circuit_let_go`).
4. **Run.** Coupled with a mechanism: `fude_zoom_coupling_step`, a mechanism step (1/240 s) at a time, the circuit at
   most `FZCP_STEPS_MOST` (200) of its own steps in each. Alone: `fude_zoom_circuit_run` with at most 400 steps a
   frame, or `FUDE_ZOOM_PAGE_FINE_STEPS` (2000) while a speaker or an oscilloscope plays: a 555 at a few hundred hertz
   cuts its steps at every edge, and under 400 a frame its sound stutters.
5. **Over budget** the circuit goes less far than the clock: slow motion, never a skipped step.
   `fude_zoom_page_play_pace` compares the two each half second and says it once when under 0.95 (`play_speed`, shown
   on the panel).
6. Not solved: said once (where, when parts are `suspect`); the events told; the sound queued.

The wires' currents (`fzc_wire_currents`) are worked out once per advance, not per step: they are only drawn. Each
piece's current comes from the parts' pin currents pushed into the places they touch, carried along a breadth-first
tree of the wire pieces (a loop of wires leaves one piece carrying nothing).

## What is kept between frames, and why

| Kept | Why |
|---|---|
| `v`, the node voltages, across steps and builds | Newton starts from the last answer, not from 0 V |
| each part's state, switches, heat and burnt, across a build (same object, same part) | editing while it plays does not discharge capacitors or reset flip-flops |
| `store` (glow, LCD, scope sweeps), across a build | a display does not go dark when a wire is drawn |
| each wire's dots' `phase` | the current's dots keep moving instead of jumping when the circuit is rebuilt (the page writes them as it draws) |
| the logic's run (`fude_sim_run_keep`) | a latch keeps its bit across a rebuild |
| each block's sparse shape, `joins`, part keys, `block_still` | shaping and factoring are the costly part; quiet blocks need the last keys |
| scratch: `wire_inject` … `wire_fill`, `rhs`, `x`, `a`, `cols`, `part_scratch`, `v_keep`, `amps_keep` | no allocation a frame (`coding_rules.md` §8); the wire scratch alone took Everything at once's wires from 37.8 to 20.6 ms for 2000 calls (`test_wire_scratch`) |

The stamps' places are found again each iteration (a binary search along a row) rather than replayed: settled blocks
are skipped, so the order changes, and the search is under a tenth of a solve (`quality_plan.md` 4.5).

## Threads

Main thread only, the circuit, its logic and `sim/` alike. The one exception is sound: the page pushes samples into an
RDE audio stream from the main thread, and RDE's audio thread plays them.

## Caches

| Cache | Key | Bound | Invalidated by |
|---|---|---|---|
| kind → part (`fude_zoom_symbol_topic_part`) | symbol kind | `FUDE_ZOOM_SYMBOL_N` | never: the catalogue is static; custom parts and sized displays are resolved from their text on every call |
| made parts (`fzlg_parts`, `fzd_parts`) | template, id and version; size and way round | none: kept for the app's life | a new version makes a new entry; old ones stay, as circuits may hold them |
| block systems' shapes | where stamps write | one per block | a build, a reshape, a reblock |
| part keys, `block_still` | a part's numbers, state read, pins' volts | one per part, one per block | taken after each solved step |
| `fude_zoom_page_play_what` | the lasso's key, `fude_zoom_logic_revision` | one answer | the lasso, or a part learnt, renamed or forgotten |

## Invariants a change must keep

- **Pin order is the model's.** Stamps, limits and displays name pins by index, and wires keep pin numbers in files:
  never reorder or remove an existing kind's pins.
- **The symbol table is append-only** (`symbol.h`): a kind is its place in it, and canvases keep that number.
- **Anything a stamp reads must be in the key**: a new model whose stamps read some of its state must say which in
  `fzc_state_read` (the default, all, is safe but never quiet), or a quiet block will skip a change.
- **A source that changes by itself is live**: add it to `fzc_blocks_quiet`'s list, or its block will go quiet.
- **Write through `fzc_entry`**, never into a system directly: that is how new places and cross-block couplings are
  found. A pin listed as sensing (`fzc_sense_pins`) must stamp no more than a leak.
- **Energy and timing**: a model that stores energy feeds `step_ratio` in `fzc_after`; one with a moment to land on
  says it in `fzc_breakpoint`; one that changes abruptly after a step calls `fude_zoom_circuit_jump`.
- **Node and block numbers depend only on the drawing**, never on the order joins were made (smallest-member names).
- **Nothing frees a made part or a learnt definition another may hold** (circuits point at parts; a learn frees only
  the definition it replaces).
- The simulation never writes the canvas.

## How to change common things

### Add a part model

1. `circuit.h`: a `FUDE_ZOOM_MODEL_` before `FUDE_ZOOM_MODEL_COUNT`, with a comment saying its pins and state.
2. `symbol.c`: an `FZS` entry at the end of the table (its family, its size, `fzs_circuit`), with its name's text id
   in `zoom/strings.py` in every language (then `tools/strings/build.py`), that id appended to `page_diagrams.c`'s
   `FUDE_ZOOM_PAGE_SYMBOL_NAMES`, and `symbol.h`'s `FUDE_ZOOM_SYMBOL_KINDS` one more: both tables are checked against
   it as they compile, so one forgotten does not build.
3. `circuit_catalogue.c`: a pin table in the order its model reads them, and an `FZ` entry with the symbol's id, the
   model, a look and its starting text. A new look goes in `FZC_LOOK_` (`circuit_parts.h`) and is drawn in
   `circuit_draw.c`.
4. `circuit_read.c`: its numbers and defaults from its text (`fzc_read_component` or `fzc_read_module`), its state's
   starting values when fresh.
5. `circuit_stamp.c`: its elements in the family's function, through `fzc_g`, `fzc_i`, `fzc_vsrc`, `fzc_nonlinear`.
6. `circuit_step.c`: in `fzc_after`, what it shows and keeps, its share of `step_ratio`; `fzc_digital` if it switches
   on voltages; `fzc_breakpoint` if it has a moment to land on.
7. `circuit_solve.c` and `circuit_blocks.c`: `fzc_state_read`, `fzc_sense_pins`, `fzc_side`, `block_live` as needed.
8. `limits.c`: its presets, the limits it has (`fude_zoom_limits_kinds`), its τ, its measures.
9. Worked by hand: `fzc_tap_change` (`circuit_play.c`) and the list in `fude_zoom_page_circuit_press`; drawn as it
   plays: `page_circuit.c`'s parts.
10. Tests in `tests/zoom` (a circuit drawn and played, against its formula), and `tests/spice` if ngspice can model it.

### Add a chip

1. `sim/chips.c`: its text in `FSC_CHIPS`, built of the primitives; its ports in pin order (pin 1 first, down the left,
   up the right), VCC and GND (or VDD and VSS) as electric ports joined to nothing inside, `~` before an active-low
   name.
2. `circuit_catalogue.c`: `FZ_SIM("<id>", CHIP, "<id>")`; its pins come from the definition's ports round a package.
3. `symbol.c`: an `FZS` entry in the CHIPS family at the end of the table, its id the chip's, with its name's text id
   (and its name and `FUDE_ZOOM_SYMBOL_KINDS`, as for a part model).
4. Its limits are the logic family's (`FZLM_LOGIC`: its text "74HC…" or "CD40…" names the series).
5. Tests: the chip against a model of it in `tests/sim` (`test_chips`), and on a breadboard in `tests/zoom`.

## Where the tests are

- `tests/spice` (`tests/run.sh spice`): RC, RL, RLC, diodes, a BJT stage, a MOSFET, an inverting op-amp and a 555,
  each drawn on a canvas and played, and the same netlist run in ngspice with the same models; with no ngspice
  installed it says so and checks only the answers on paper.
- `tests/sim`: definitions and their text (fuzzed), elaboration and its errors, the digital engine (every gate's
  table, adders, sequences, buses, oscillation), the chips against models, runs kept across rebuilds, and the sparse
  LU against a dense solve on random circuit-like systems (`test_sparse`).
- `tests/zoom`: circuits as a person draws them (`test_circuits`, `test_canvas_logic`, `test_custom`,
  `test_big_circuits`, `test_quiet_blocks`, `test_circuit_jumps`, `test_logic_nodes`, `test_wire_scratch`,
  `test_limits`, `test_scope`, `test_lcd`, the display and part batches, the examples run as Play runs them).
- `tests/mech`: circuits coupled to mechanisms (motors, dynamos, servos, solenoids, steppers, shaft sensors).

## Open points

Found while writing this note and mending what it found, against the code of 2026-10-15; each is in that day's
reports.

- A stiff node still rings after backward Euler. Measured while the jumps were mended (not in a test): an encoder's
  contact into 100 nF, once the step after a jump has been taken by backward Euler and the trapezoidal rule takes
  over again, a node whose time constant is far under the step swings about ±40 µV from step to step, and the steps
  there come down to about 0.5 µs. Nothing drawn shows it; it costs steps. Not looked into further.
- Play solves no DC operating point first: it starts from 0 V with backward Euler; `fude_zoom_circuit_dc` is used by
  the tests alone.
