// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/circuit_parts.h"

// ===========================================================================
// See circuit.h. Circuits, the unknowns: the rails' nodes held (known, at their rails' volts), the nodes only logic is
// on (the logic's: no unknowns), the pins that only sense, and the unknowns in blocks, each solved on its own
// (fzc_blocks), with a reference held at 0 V in each island ground does not reach (docs/infinite_canvas_design.md §46
// and §59).
//
// The file is in order: which side of a part a pin is on, the blocks' systems let go, the pins that sense, the rails
// held, and the blocks: the nodes in none (fzc_outside), fzc_blocks and its steps (fzc_blocks_join, _number, _parts,
// _quiet, _reference).
//
// Part of the circuit: circuit_parts.h holds what the modules share and says which module holds what. Prefix fzc_ and
// FZC_. Main thread only.
// ===========================================================================

// Which of a part's sides pin _k is on, nothing conducting between them: an optocoupler's or a slot sensor's LED and
// its transistor, a transformer's windings, a relay module's coil side and its contacts (0: one side, all of it).
RDE_INTERNAL u32 fzc_side(u8 _model, u32 _k) {
    switch(_model) {
    case FUDE_ZOOM_MODEL_OPTO:
    case FUDE_ZOOM_MODEL_SLOT:
    case FUDE_ZOOM_MODEL_TRANSFORMER: return _k >= 2u ? 1u : 0u;
    case FUDE_ZOOM_MODEL_RELAY:       return _k >= 3u ? 1u : 0u;
    default:                          return 0u;
    }
}

// Lets its blocks' sparse systems go, and empties their list.
void fzc_systems_free(fude_zoom_circuit* _c) {
    fude_sim_sparse* _sys = (fude_sim_sparse*)_c->block_system.memory;
    for(u32 _b = 0; _b < (u32)rde_arr_length(&_c->block_system); _b++) {
        fude_sim_sparse_free(&_sys[_b]);
    }
    rde_arr_clear(&_c->block_system);
}

// The pins of part _i that only SENSE, as bits: what they are on is read once solved (a 555's trigger, threshold and
// reset; a logic input; an oscilloscope's probes) and their stamps are a leak alone — its blocks not joined through
// them, what is on them changing nothing it stamps.
RDE_INTERNAL u64 fzc_sense_pins(const fude_zoom_circuit* _c, u32 _i) {
    const fude_zoom_circuit_part* _p = &((const fude_zoom_circuit_part*)_c->parts.memory)[_i];
    switch(_p->part->model) {
    case FUDE_ZOOM_MODEL_TIMER555: return (1ull << 1) | (1ull << 3) | (1ull << 5);   // TRIG, RESET, THRES.
    case FUDE_ZOOM_MODEL_SCOPE:    return 3ull;                                        // CH1, CH2.
    case FUDE_ZOOM_MODEL_GATE:
    case FUDE_ZOOM_MODEL_DFF:
    case FUDE_ZOOM_MODEL_TFF:
    case FUDE_ZOOM_MODEL_JKFF:
    case FUDE_ZOOM_MODEL_SRLATCH:
    case FUDE_ZOOM_MODEL_LOGIC_IN:
    case FUDE_ZOOM_MODEL_LOGIC_OUT:
    case FUDE_ZOOM_MODEL_SIM: {
        // Its bridges' inputs, but not a pin one drives too.
        if(_c->logic == NULL) {
            return 0ull;
        }
        u32 _first;
        const u32 _n = fude_zoom_logic_bridges_of(_c->logic, _i, &_first);
        const fude_zoom_bridge* _b = &((const fude_zoom_bridge*)_c->logic->bridges.memory)[_first];
        u64 _in = 0ull, _out = 0ull;
        for(u32 _k = 0; _k < _n; _k++) {
            if(_b[_k].pin < FZC_PIN_BITS) {   // (past 64: not one that only senses, fzc_senses)
                if(_b[_k].way == FUDE_ZOOM_BRIDGE_IN) {
                    _in |= 1ull << _b[_k].pin;
                } else {
                    _out |= 1ull << _b[_k].pin;
                }
            }
        }
        return _in & ~_out;
    }
    default: return 0ull;
    }
}

// The rails' nodes held (fude_zoom_circuit's): each node rails of one voltage are on — known, at their volts (a rail
// never burns: nothing to let it go); each other pin on one, for what its rail gives. Each part's pins that sense.
void fzc_held(fude_zoom_circuit* _c) {
    const u32 _n = _c->nodes > 0u ? _c->nodes : 1u;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr_resize(&_c->node_held, _n);
    u32* _held = (u32*)_c->node_held.memory;
    rde_arr TYPE(u8) _clash_arr = rde_arr_new(sizeof(u8), _heap);
    rde_arr_resize(&_clash_arr, _n);
    u8* _clash = (u8*)_clash_arr.memory;
    for(u32 _k = 0; _k < _n; _k++) {
        _held[_k]  = FUDE_ZOOM_NONE;
        _clash[_k] = 0u;
    }
    fude_zoom_circuit_part* _p = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts);
    for(u32 _i = 0; _i < _np; _i++) {
        const u32 _node = _p[_i].node[0];
        if(_p[_i].part->model != FUDE_ZOOM_MODEL_RAIL || _node == FUDE_ZOOM_NONE || _node == 0u || _node >= _n) {
            continue;
        }
        if(_held[_node] == FUDE_ZOOM_NONE) {
            _held[_node] = _i;
        } else if(_p[_held[_node]].value[0] != _p[_i].value[0]) {
            // Two rails of two voltages joined: a short, solved as it was, so that it is told.
            _clash[_node] = 1u;
        }
    }
    f64* _v = (f64*)_c->v.memory;
    for(u32 _k = 1; _k < _n && _k < (u32)rde_arr_length(&_c->v); _k++) {
        _held[_k] = _clash[_k] ? FUDE_ZOOM_NONE : _held[_k];
        _v[_k]    = _held[_k] != FUDE_ZOOM_NONE ? _p[_held[_k]].value[0] : _v[_k];
    }
    rde_arr_clear(&_c->held_pins);
    for(u32 _i = 0; _i < _np; _i++) {
        for(u32 _k = 0; _k < _p[_i].part->pin_count; _k++) {
            const u32 _node = _p[_i].node[_k];
            if(_node != FUDE_ZOOM_NONE && _node != 0u && _node < _n && _held[_node] != FUDE_ZOOM_NONE && !(_held[_node] == _i && _k == 0u)) {
                const u64 _at = ((u64)_node << 32) | (u64)(_p[_i].pin_at + _k);   // (the pin's place in pin_amps)
                rde_arr_add(&_c->held_pins, (any)&_at);
            }
        }
        _p[_i].sense = fzc_sense_pins(_c, _i);
    }
    rde_arr_free(&_clash_arr);
}

// Returns whether node _k is in no block: held by a rail (_held: known, at its volts), or one only logic pins are on,
// which is the logic's own net (logic.h): nothing analog stamps it, so as an unknown it was only pulled to 0 V by
// GMIN and back to its level by fzc_at, damping its block's first Newton steps; fzc_at gives it its level's volts.
RDE_INTERNAL b8 fzc_outside(const fude_zoom_circuit* _c, const u32* _held, u32 _k) {
    return _held[_k] != FUDE_ZOOM_NONE || fude_zoom_logic_node(_c->logic, _k) != 0xFFu;
}

// Joins, in the disjoint set _up, the nodes each part's pins are on (ground not, nor a node in no block (fzc_outside),
// nor through a pin that only senses), and the pairs of nodes its stamps were found to write across (the couplings).
RDE_INTERNAL void fzc_blocks_join(const fude_zoom_circuit* _c, u32 _n, rde_disjoint_set* _up, const u32* _held) {
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_c->parts.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_c->parts); _i++) {
        u32 _first = FUDE_ZOOM_NONE;
        for(u32 _k = 0; _k < _p[_i].part->pin_count; _k++) {
            const u32 _node = _p[_i].node[_k];
            if(_node == FUDE_ZOOM_NONE || _node == 0u || _node >= _n || fzc_outside(_c, _held, _node) || fzc_senses(&_p[_i], _k)) {
                continue;
            }
            if(_first == FUDE_ZOOM_NONE) {
                _first = _node;
            } else {
                rde_disjoint_set_union(_up, _first, _node);
            }
        }
    }
    for(u32 _j = 0; _j < (u32)rde_arr_length(&_c->couplings); _j++) {
        const u64 _pair = ((const u64*)_c->couplings.memory)[_j];
        const u32 _a = (u32)(_pair >> 32), _b = (u32)(_pair & 0xFFFFFFFFull);
        if(_a < _n && _b < _n) {
            rde_disjoint_set_union(_up, _a, _b);
        }
    }
}

// Numbers the blocks, each as its first node comes (_id: each set's block, FUDE_ZOOM_NONE until it has one), and
// each node's slot: its block's first and its place in it (ground and a node in no block, fzc_outside: none). Each
// block's system made anew, to be shaped before it is first solved.
RDE_INTERNAL void fzc_blocks_number(fude_zoom_circuit* _c, u32 _n, rde_disjoint_set* _up, u32* _id, const u32* _held) {
    rde_arr_resize(&_c->node_block, _n);
    rde_arr_resize(&_c->node_slot, _n);
    rde_arr_clear(&_c->block_size);
    u32* _block = (u32*)_c->node_block.memory;
    u32* _slot  = (u32*)_c->node_slot.memory;
    _block[0] = _slot[0] = FUDE_ZOOM_NONE;   // Ground: no unknown.
    u32 _blocks = 0;
    for(u32 _k = 1; _k < _n; _k++) {
        if(fzc_outside(_c, _held, _k)) {
            _block[_k] = _slot[_k] = FUDE_ZOOM_NONE;   // Held, or only logic's: known.
            continue;
        }
        const u32 _r = rde_disjoint_set_find(_up, _k);
        if(_id[_r] == FUDE_ZOOM_NONE) {
            _id[_r] = _blocks++;
            const u32 _zero = 0u;
            rde_arr_add(&_c->block_size, (any)&_zero);
        }
        _block[_k] = _id[_r];
        _slot[_k]  = ((u32*)_c->block_size.memory)[_id[_r]]++;   // Its place in its block, for now.
    }
    _c->blocks = _blocks;
    rde_arr_resize(&_c->block_first, _blocks);
    rde_arr_resize(&_c->block_done, _blocks);
    const u32* _size = (const u32*)_c->block_size.memory;
    u32* _first = (u32*)_c->block_first.memory;
    u32 _slots = 0;
    for(u32 _b = 0; _b < _blocks; _b++) {
        _first[_b] = _slots;
        _slots    += _size[_b];
    }
    // Each block's system, shaped before it is first solved (fzc_shape: with the joins found before, nodes still).
    fzc_systems_free(_c);
    rde_arr_resize(&_c->block_system, _blocks);
    for(u32 _b = 0; _b < _blocks; _b++) {
        fude_sim_sparse_init(&((fude_sim_sparse*)_c->block_system.memory)[_b]);
    }
    _c->reshape = true;
    for(u32 _k = 1; _k < _n; _k++) {
        _slot[_k] += _block[_k] != FUDE_ZOOM_NONE ? _first[_block[_k]] : 0u;
    }
}

// Each part's own block: its pins' but those that sense, else a sensing one's; and whether one of its pins is in
// another block (spans).
RDE_INTERNAL void fzc_blocks_parts(fude_zoom_circuit* _c, u32 _n) {
    u32* _block = (u32*)_c->node_block.memory;
    fude_zoom_circuit_part* _q = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts);
    for(u32 _i = 0; _i < _np; _i++) {
        // Its own block: its pins' but those that sense; else a sensing one's.
        _q[_i].block = FUDE_ZOOM_NONE;
        _q[_i].spans = false;
        for(u32 _pass = 0; _pass < 2u && _q[_i].block == FUDE_ZOOM_NONE; _pass++) {
            for(u32 _k = 0; _k < _q[_i].part->pin_count && _q[_i].block == FUDE_ZOOM_NONE; _k++) {
                const u32 _node = _q[_i].node[_k];
                if(_node != FUDE_ZOOM_NONE && _node != 0u && _node < _n && (_pass == 1u || !fzc_senses(&_q[_i], _k))) {
                    _q[_i].block = _block[_node];
                }
            }
        }
        for(u32 _k = 0; _k < _q[_i].part->pin_count; _k++) {
            const u32 _node = _q[_i].node[_k];
            _q[_i].spans = _q[_i].spans || (_node != FUDE_ZOOM_NONE && _node != 0u && _node < _n && _block[_node] != FUDE_ZOOM_NONE &&
                                            _block[_node] != _q[_i].block);
        }
    }
}

// The blocks' watch for quiet (none quiet or still yet; live, a block with a source that changes by itself: an AC
// source, a clock, a board), and where each part's key starts (fzc_key).
RDE_INTERNAL void fzc_blocks_quiet(fude_zoom_circuit* _c, u32 _n) {
    u32* _block = (u32*)_c->node_block.memory;
    fude_zoom_circuit_part* _q = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts);
    const u32 _blocks = _c->blocks;
    // Quiet: none yet (each solved and watched first); live, a block with what changes by itself in it.
    rde_arr_resize(&_c->block_still, _blocks);
    rde_arr_resize(&_c->block_live, _blocks);
    rde_arr_resize(&_c->block_quiet, _blocks);
    rde_memset(_c->block_still.memory, 0, _blocks);
    rde_memset(_c->block_live.memory, 0, _blocks);
    rde_memset(_c->block_quiet.memory, 0, _blocks);
    for(u32 _i = 0; _i < _np; _i++) {
        const u8 _m = _q[_i].part->model;
        if(_m == FUDE_ZOOM_MODEL_ACSOURCE || _m == FUDE_ZOOM_MODEL_CLOCK || _m == FUDE_ZOOM_MODEL_BOARD) {
            for(u32 _k = 0; _k < _q[_i].part->pin_count; _k++) {
                const u32 _node = _q[_i].node[_k];
                if(_node != FUDE_ZOOM_NONE && _node != 0u && _node < _n && _block[_node] != FUDE_ZOOM_NONE) {
                    ((u8*)_c->block_live.memory)[_block[_node]] = 1u;
                }
            }
        }
    }
    rde_arr_resize(&_c->key_at, _np + 1u);
    u32* _key_at = (u32*)_c->key_at.memory;
    u32 _keys = 0, _most_pins = 0;
    for(u32 _i = 0; _i < _np; _i++) {
        _key_at[_i] = _keys;
        _keys += FZC_KEY_FIXED + _q[_i].part->pin_count;
        _most_pins = _q[_i].part->pin_count > _most_pins ? _q[_i].part->pin_count : _most_pins;
    }
    _key_at[_np] = _keys;
    rde_arr_resize(&_c->part_key, _keys > 0u ? _keys : 1u);
    // (one part's scratch: its key, or its pins' volts and currents, for the part with the most pins)
    rde_arr_resize(&_c->part_scratch, FZC_KEY_FIXED + 2u * (usize)_most_pins);
}

// Each island ground does not reach held at 0 V at one node (its reference), as the note inside says; _up and _id
// are worked in again (what they held is not needed after the blocks are numbered).
RDE_INTERNAL void fzc_blocks_reference(fude_zoom_circuit* _c, u32 _n, rde_disjoint_set* _up, u32* _id, const u32* _held) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    u32* _block = (u32*)_c->node_block.memory;
    fude_zoom_circuit_part* _q = (fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts);
    // What ground reaches nowhere (a battery and what it lights, on a canvas whose ground is elsewhere; the side of an
    // optocoupler, a transformer or a relay ground is not on) has its level from nothing — a system all but singular.
    // An ISLAND (what conducts: a part's pins joined, but not across its sides — fzc_side —, ground among them) is held
    // at 0 V at one node, as a canvas without ground holds its first source's minus: a board's GND, else a source's
    // minus, else its first node. (Nothing flows that way: nothing else of it reaches ground.) One in a group of what a
    // part's pins join ground reaches, that holds a part of one pin (a supply rail, a logic input: what drives from
    // ground), is not: it is reached so — nor one a rail holds a node of.
    rde_arr_clear(&_c->block_reference);
    // _up again: what a part's pins join, every pin, and which of those ground is in.
    rde_disjoint_set_reset(_up, _n);
    for(u32 _i = 0; _i < _np; _i++) {
        u32 _f0 = FUDE_ZOOM_NONE;
        for(u32 _k = 0; _k < _q[_i].part->pin_count; _k++) {
            const u32 _node = _q[_i].node[_k];
            if(_node == FUDE_ZOOM_NONE || _node == 0u || _node >= _n) {
                continue;
            }
            if(_f0 == FUDE_ZOOM_NONE) {
                _f0 = _node;
            } else {
                rde_disjoint_set_union(_up, _f0, _node);
            }
        }
    }
    rde_arr TYPE(u8) _grounded_arr = rde_arr_new(sizeof(u8), _heap);
    rde_arr_resize(&_grounded_arr, _n);
    u8* _grounded = (u8*)_grounded_arr.memory;
    rde_memset(_grounded, 0, _n);
    for(u32 _i = 0; _i < _np; _i++) {
        u32 _f0 = FUDE_ZOOM_NONE;
        b8  _g0 = false;
        for(u32 _k = 0; _k < _q[_i].part->pin_count; _k++) {
            const u32 _node = _q[_i].node[_k];
            _g0 = _g0 || _node == 0u;
            _f0 = _f0 == FUDE_ZOOM_NONE && _node != FUDE_ZOOM_NONE && _node != 0u && _node < _n ? _node : _f0;
        }
        if(_f0 != FUDE_ZOOM_NONE && _g0) {
            _grounded[rde_disjoint_set_find(_up, _f0)] = 1u;
        }
    }
    rde_arr TYPE(u32) _group_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_group_arr, _n);
    u32* _group = (u32*)_group_arr.memory;
    for(u32 _k = 0; _k < _n; _k++) {
        _group[_k] = rde_disjoint_set_find(_up, _k);
    }
    // _up again: the islands, ground's among them; _id: each island's reference, FUDE_ZOOM_NONE none yet.
    rde_arr TYPE(u8) _single_arr = rde_arr_new(sizeof(u8), _heap);
    rde_arr_resize(&_single_arr, _n);
    u8* _single = (u8*)_single_arr.memory;
    rde_disjoint_set_reset(_up, _n);
    for(u32 _k = 0; _k < _n; _k++) {
        _id[_k]     = FUDE_ZOOM_NONE;
        _single[_k] = 0u;
    }
    for(u32 _i = 0; _i < _np; _i++) {
        u32 _f[2] = { FUDE_ZOOM_NONE, FUDE_ZOOM_NONE };
        for(u32 _k = 0; _k < _q[_i].part->pin_count; _k++) {
            const u32 _node = _q[_i].node[_k], _sd = fzc_side(_q[_i].part->model, _k);
            if(_node == FUDE_ZOOM_NONE || _node >= _n) {
                continue;
            }
            if(_f[_sd] == FUDE_ZOOM_NONE) {
                _f[_sd] = _node;
            } else {
                rde_disjoint_set_union(_up, _f[_sd], _node);
            }
        }
    }
    for(u32 _i = 0; _i < _np; _i++) {
        const u32 _node = _q[_i].part->pin_count == 1u ? _q[_i].node[0] : FUDE_ZOOM_NONE;
        if(_node != FUDE_ZOOM_NONE && _node < _n) {
            _single[rde_disjoint_set_find(_up, _node)] = 1u;
        }
    }
    for(u32 _k = 1; _k < _n; _k++) {
        if(_held[_k] != FUDE_ZOOM_NONE) {
            _id[rde_disjoint_set_find(_up, _k)] = _k;   // A rail holds it: there it is.
        }
    }
    const u32 _earth = rde_disjoint_set_find(_up, 0u);
    for(u32 _pass = 0; _pass < 3u; _pass++) {
        const u32 _count = _pass < 2u ? _np : _n;
        for(u32 _i = 0; _i < _count; _i++) {
            u32 _node = FUDE_ZOOM_NONE;
            if(_pass == 2u) {
                _node = _i;   // Its first node.
            } else if(_pass == 0u && _q[_i].part->model == FUDE_ZOOM_MODEL_BOARD) {
                for(u32 _k = 0; _k < _q[_i].part->pin_count && _node == FUDE_ZOOM_NONE; _k++) {
                    _node = fzc_same_name(_q[_i].part->pins[_k].name, "GND") ? _q[_i].node[_k] : FUDE_ZOOM_NONE;
                }
            } else if(_pass == 1u) {
                const u8 _m = _q[_i].part->model;
                const b8 _source = _m == FUDE_ZOOM_MODEL_VSOURCE || _m == FUDE_ZOOM_MODEL_BATTERY || _m == FUDE_ZOOM_MODEL_ACSOURCE ||
                                   _m == FUDE_ZOOM_MODEL_CLOCK || _m == FUDE_ZOOM_MODEL_ISOURCE;
                _node = _source ? _q[_i].node[1] : FUDE_ZOOM_NONE;   // Its minus.
            }
            if(_node == FUDE_ZOOM_NONE || _node == 0u || _node >= _n || _block[_node] == FUDE_ZOOM_NONE) {
                continue;
            }
            const u32 _isle = rde_disjoint_set_find(_up, _node);
            if(_isle != _earth && _id[_isle] == FUDE_ZOOM_NONE && !(_grounded[_group[_node]] && _single[_isle])) {
                _id[_isle] = _node;
                rde_arr_add(&_c->block_reference, (any)&_node);
            }
        }
    }
    rde_arr_free(&_group_arr);
    rde_arr_free(&_single_arr);
    rde_arr_free(&_grounded_arr);
}

// Its unknowns in blocks (fude_zoom_circuit's): the nodes each part's pins are on joined (ground not, nor a node in no
// block — held, or only logic's: fzc_outside —, nor through a pin that only senses: what is joined only so is apart),
// and what its stamps were found to write across; each block numbered as its first node comes; each node's slot its
// block's first and its place in it; each block's matrix after the one before's. Each part's block: its pins' but
// those that sense (spans: one of those elsewhere).
void fzc_blocks(fude_zoom_circuit* _c) {
    const u32 _n = _c->nodes > 0u ? _c->nodes : 1u;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    // The nodes in groups, each named by its smallest node (rde_disjoint_set's rule, as the blocks are numbered by
    // walking the nodes in order: block numbers that do not depend on the order the nodes were joined in).
    rde_disjoint_set _up = rde_disjoint_set_new(_n, _heap);
    rde_arr _id_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_id_arr, _n);
    u32* _id = (u32*)_id_arr.memory;
    for(u32 _k = 0; _k < _n; _k++) {
        _id[_k] = FUDE_ZOOM_NONE;
    }
    if(rde_arr_length(&_c->node_held) < _n) {
        rde_arr_resize(&_c->node_held, _n);
        for(u32 _k = 0; _k < _n; _k++) {
            ((u32*)_c->node_held.memory)[_k] = FUDE_ZOOM_NONE;
        }
    }
    const u32* _held = (const u32*)_c->node_held.memory;
    fzc_blocks_join(_c, _n, &_up, _held);
    fzc_blocks_number(_c, _n, &_up, _id, _held);
    fzc_blocks_parts(_c, _n);
    fzc_blocks_quiet(_c, _n);
    fzc_blocks_reference(_c, _n, &_up, _id, _held);
    rde_disjoint_set_free(&_up);
    rde_arr_free(&_id_arr);
}
