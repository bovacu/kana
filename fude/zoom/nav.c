// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/nav.h"
#include "zoom/shape.h"

// ===========================================================================
// See nav.h.
//
// The file is in five parts: frames (walking up the tree), flying, the marks, the depth and the levels. A flight is
// worked out once, as it begins: both ends' zooms as log2 in the frame both are in, the dip that takes it out far
// enough, and a table of how far along the pan is by eased time; each step then reads the table. The marks search
// each frame's R-tree nearest first, so the first thing met in a direction is the nearest there. The file's own
// functions share the header's prefixes, fude_zoom_nav_ and fude_zoom_fly_.
// ===========================================================================

// --- frames ----------------------------------------------------------------------------------

// Returns frame _f's parent frame (FUDE_ZOOM_NONE for the top).
RDE_INTERNAL u32 fude_zoom_nav_parent(const fude_zoom_scene* _s, u32 _f) {
    return fude_zoom_scene_frame(_s, _f)->parent;
}

// Returns the nearest frame both are in (either may be it): both are walked up to the same depth, then up together.
// FUDE_ZOOM_NONE when they share no frame.
RDE_INTERNAL u32 fude_zoom_nav_common(const fude_zoom_scene* _s, u32 _a, u32 _b) {
    u32 _da = fude_zoom_scene_depth(_s, _a), _db = fude_zoom_scene_depth(_s, _b);
    while(_da > _db) { _a = fude_zoom_nav_parent(_s, _a); _da--; }
    while(_db > _da) { _b = fude_zoom_nav_parent(_s, _b); _db--; }
    while(_a != _b && _a != FUDE_ZOOM_NONE && _b != FUDE_ZOOM_NONE) {
        _a = fude_zoom_nav_parent(_s, _a);
        _b = fude_zoom_nav_parent(_s, _b);
    }
    return _a;
}

// Returns log2 of a unit of frame _f in the units of its ancestor _up: the sum of log2 of each frame's scale on the
// way.
RDE_INTERNAL f64 fude_zoom_nav_log_unit(const fude_zoom_scene* _s, u32 _f, u32 _up) {
    f64 _g = 0.0;
    while(_f != _up && _f != FUDE_ZOOM_NONE) {
        _g += rde_math_log2_f64(rde_math_abs_f64(fude_zoom_scene_frame(_s, _f)->xf.scale));
        _f  = fude_zoom_nav_parent(_s, _f);
    }
    return _g;
}

// --- flying ----------------------------------------------------------------------------------

// Eases time _t (held to 0 to 1) in and out: smoothstep, 3t^2 - 2t^3.
RDE_INTERNAL f64 fude_zoom_nav_ease(f64 _t) {
    _t = _t < 0.0 ? 0.0 : (_t > 1.0 ? 1.0 : _t);
    return _t * _t * (3.0 - 2.0 * _t);
}

// Returns the zoom (log2, the common frame's units) at eased time _e: from one end's to the other's in a straight line,
// less the dip, which is a parabola with all of l_dip at the middle and none at the ends.
RDE_INTERNAL f64 fude_zoom_fly_zoom(const fude_zoom_flight* _f, f64 _e) {
    return _f->l[0] + (_f->l[1] - _f->l[0]) * _e - _f->l_dip * 4.0 * _e * (1.0 - _e);
}

// Puts the camera exactly at the flight's far end and ends the flight.
RDE_INTERNAL void fude_zoom_fly_land(fude_zoom_flight* _f, fude_zoom_scene* _s, fude_zoom_v2 _half) {
    fude_zoom_camera_look_at(_s, _f->ends[1].frame, _f->ends[1].at, _f->ends[1].z);
    fude_zoom_camera_settle(_s, _half);
    _f->active = false;
}

// See the header. Works out the flight's ends in the frame both are in, the dip, the table of the way along and the
// flight's length. "Too many levels apart" is any end whose numbers are not finite there.
b8 fude_zoom_fly_begin(fude_zoom_flight* _f, fude_zoom_scene* _s, fude_zoom_camera _to, fude_zoom_v2 _half, f64 _now) {
    rde_memset(_f, 0, sizeof(*_f));
    _f->ends[0] = _s->camera;
    _f->ends[1] = _to;
    const u32 _common = fude_zoom_nav_common(_s, _f->ends[0].frame, _to.frame);
    for(u32 _k = 0; _k < 2u; _k++) {
        _f->g[_k] = fude_zoom_nav_log_unit(_s, _f->ends[_k].frame, _common);
        _f->l[_k] = rde_math_log2_f64(_f->ends[_k].z) - _f->g[_k];
    }
    _f->cross[0] = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _to.frame, _f->ends[0].frame), _to.at);
    _f->cross[1] = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _f->ends[0].frame, _to.frame), _f->ends[0].at);
    // How far apart, in the common frame (all that matters here is the scale).
    const fude_zoom_v2 _pa = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _f->ends[0].frame, _common), _f->ends[0].at);
    const fude_zoom_v2 _pb = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _to.frame, _common), _to.at);
    const f64 _d = rde_math_hypot_f64(_pb.x - _pa.x, _pb.y - _pa.y);
    const b8 _sane = _common != FUDE_ZOOM_NONE && rde_math_is_finite_f64(_f->l[0]) && rde_math_is_finite_f64(_f->l[1]) && rde_math_is_finite_f64(_d) &&
                     rde_math_is_finite_f64(_f->cross[0].x) && rde_math_is_finite_f64(_f->cross[0].y) && rde_math_is_finite_f64(_f->cross[1].x) && rde_math_is_finite_f64(_f->cross[1].y);
    // Already there (the same zoom, under half a point apart on screen), or past flying: put there.
    if(!_sane || (rde_math_abs_f64(_f->l[0] - _f->l[1]) < 0.01 && _d * rde_math_exp2_f64(rde_math_min_f64(_f->l[0], _f->l[1])) < 0.5)) {
        fude_zoom_fly_land(_f, _s, _half);
        return false;
    }
    // Out far enough in the middle that both ends are on screen together (their distance on screen 80% of the
    // screen's shorter side); no dip when either end is out that far already.
    const f64 _fit = rde_math_log2_f64(1.6 * rde_math_min_f64(_half.x, _half.y) / rde_math_max_f64(_d, 1e-300));
    _f->l_dip = _fit < rde_math_min_f64(_f->l[0], _f->l[1]) ? (_f->l[0] + _f->l[1]) * 0.5 - _fit : 0.0;
    // The way along, by eased time: by the view's width (2^-zoom), so the pan happens while far out and the camera
    // settles onto each end. The widths are taken relative to the furthest out, so none overflows. Each step's is the
    // trapezium rule's, and the table is then made to run from 0 to 1.
    f64 _ls[FUDE_ZOOM_FLY_STEPS + 1];
    f64 _out = 1e300;
    for(u32 _i = 0; _i <= FUDE_ZOOM_FLY_STEPS; _i++) {
        _ls[_i] = fude_zoom_fly_zoom(_f, (f64)_i / (f64)FUDE_ZOOM_FLY_STEPS);
        _out    = rde_math_min_f64(_out, _ls[_i]);
    }
    _f->way[0] = 0.0;
    for(u32 _i = 1; _i <= FUDE_ZOOM_FLY_STEPS; _i++) {
        _f->way[_i] = _f->way[_i - 1] + 0.5 * (rde_math_exp2_f64(_out - _ls[_i - 1]) + rde_math_exp2_f64(_out - _ls[_i]));
    }
    const f64 _total = _f->way[FUDE_ZOOM_FLY_STEPS];
    for(u32 _i = 0; _i <= FUDE_ZOOM_FLY_STEPS; _i++) {
        _f->way[_i] = _total > 0.0 ? _f->way[_i] / _total : (f64)_i / (f64)FUDE_ZOOM_FLY_STEPS;
    }
    // Longer the further it zooms, out and in together (log2): 0.45 seconds for a step aside and 0.03 more for each
    // doubling, at most 2.5 seconds (from about 68 doublings on).
    const f64 _zooms = (_f->l[0] - _out) + (_f->l[1] - _out);
    _f->length = rde_math_min_f64(0.45 + 0.03 * _zooms, 2.5);
    _f->began  = _now;
    _f->active = true;
    return true;
}

// See the header. The pan is read from the way's table between its two steps either side of the eased time.
b8 fude_zoom_fly_step(fude_zoom_flight* _f, fude_zoom_scene* _s, fude_zoom_v2 _half, f64 _now) {
    if(!_f->active) {
        return false;
    }
    const f64 _t = (_now - _f->began) / _f->length;
    if(_t >= 1.0) {
        fude_zoom_fly_land(_f, _s, _half);
        return false;
    }
    const f64 _e = fude_zoom_nav_ease(_t);
    const f64 _l = fude_zoom_fly_zoom(_f, _e);
    const f64 _k = _e * (f64)FUDE_ZOOM_FLY_STEPS;
    const u32 _i = (u32)_k < FUDE_ZOOM_FLY_STEPS ? (u32)_k : FUDE_ZOOM_FLY_STEPS - 1u;
    const f64 _u = _f->way[_i] + (_f->way[_i + 1u] - _f->way[_i]) * (_k - (f64)_i);
    // Worked out from the coarser end while the camera is out past it, else from the deeper: walking up from it then
    // reaches the camera's level.
    const u32 _coarse = _f->l[0] <= _f->l[1] ? 0u : 1u;
    const u32 _x      = _l <= _f->l[_coarse] ? _coarse : 1u - _coarse;
    const f64 _from_x = _x == 0u ? _u : 1.0 - _u;   // How much of the way from end x to the other.
    const fude_zoom_camera* _end = &_f->ends[_x];
    fude_zoom_v2 _p = { _end->at.x + _from_x * (_f->cross[_x].x - _end->at.x), _end->at.y + _from_x * (_f->cross[_x].y - _end->at.y) };
    // Its zoom in that end's frame's units, and up the frames until that zoom is at least FUDE_ZOOM_Z_MIN there.
    f64 _lz = _l + _f->g[_x];
    u32 _frame = _end->frame;
    const f64 _floor = rde_math_log2_f64(FUDE_ZOOM_Z_MIN);
    while(_lz < _floor && fude_zoom_nav_parent(_s, _frame) != FUDE_ZOOM_NONE) {
        const fude_zoom_frame* _fr = fude_zoom_scene_frame(_s, _frame);
        _p      = fude_zoom_sim_apply(fude_zoom_sim_from_xform(_fr->xf), _p);
        _lz    -= rde_math_log2_f64(rde_math_abs_f64(_fr->xf.scale));   // a parent's unit is 1/scale of its child's
        _frame  = _fr->parent;
    }
    fude_zoom_camera_look_at(_s, _frame, _p, rde_math_exp2_f64(_lz));
    fude_zoom_camera_settle(_s, _half);
    return true;
}

// See the header. The zoom fits the box's width and its height both, each held to at least a thousandth of the box's
// larger side.
fude_zoom_camera fude_zoom_nav_framing(fude_zoom_v2 _half, u32 _frame, fude_zoom_box _box, f64 _fill) {
    const f64 _w = _box.max_x - _box.min_x, _h = _box.max_y - _box.min_y;
    // A box with nothing in it, a point: its larger side taken as 1e-3, so it is framed as a millionth of the
    // frame's unit across (never a zoom without end). A line: as a thousandth of its length across. A box however
    // small: as it is.
    const f64 _big = rde_math_max_f64(_w, _h) > 0.0 ? rde_math_max_f64(_w, _h) : 1e-3;
    const f64 _z = rde_math_min_f64(2.0 * _half.x * _fill / rde_math_max_f64(_w, _big * 1e-3), 2.0 * _half.y * _fill / rde_math_max_f64(_h, _big * 1e-3));
    return (fude_zoom_camera){ _frame, { (_box.min_x + _box.max_x) * 0.5, (_box.min_y + _box.max_y) * 0.5 }, _z };
}

// --- marks -----------------------------------------------------------------------------------

// One frame's search for the marks (fude_zoom_nav_visit is handed it).
typedef struct {
    const fude_zoom_scene* s;
    u32                    skip;       // the FRAME object of the frame the camera is in (in its parent)
    fude_zoom_sim          to_screen;  // the searched frame's units → the screen
    fude_zoom_box          view;       // the screen, in those units
    f64                    z;          // those units → screen points
    u32                    frame;      // the frame searched
    u32                    seen;       // how many things it has been handed; it stops past 4096
    fude_zoom_nav_mark     best[FUDE_ZOOM_NAV_ARROWS];     // the nearest off screen in each eighth of a turn
    f64                    best_d[FUDE_ZOOM_NAV_ARROWS];   // screen points (1e300: none)
    u32                    filled;     // how many eighths have one
    fude_zoom_nav_mark     rings[FUDE_ZOOM_NAV_RINGS];   // what is on screen but too small to see
    u32                    ring_count;
    // A frame in it looked at: the deepest frame holding its drawing (FUDE_ZOOM_NONE: none)...
    u32                    as_frame;
    fude_zoom_box          as_box;     // ...and that drawing there: what a tap shows
    // The objects visited, made (on the std allocator, freed with the search) only while the frame has stale entries:
    // one moved away and back has two entries with its own box, and is visited once.
    rde_hash_set TYPE(u32) visited;
} fude_zoom_nav_search;

// Returns whether _o is something drawn, to be seen (not a frame: what is in it is). Not a bookmark, not a layer's
// record, not what a hidden layer hides; nor a guide (it reaches far past what it helps draw) or an attribute of other
// things (a constraint, a part's properties: shape.h), which goes with what it names.
RDE_INTERNAL b8 fude_zoom_nav_seen(const fude_zoom_scene* _s, const fude_zoom_object* _o) {
    return (_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->kind != FUDE_ZOOM_KIND_MARK && _o->kind != FUDE_ZOOM_KIND_LAYER && _o->kind != FUDE_ZOOM_KIND_FRAME &&
           !fude_zoom_scene_hides(_s, _o) &&
           !(_o->kind == FUDE_ZOOM_KIND_SHAPE && (_o->channels == FUDE_ZOOM_SHAPE_GUIDE || fude_zoom_shape_is_attribute(_o->channels)));
}

// Returns the box round what is drawn in frame _frame and deeper, in its units, leaving out its object _skip. A frame
// in it counts for what is drawn in it, not its own box: that is the view it was made for, often far wider (an arrow
// to it showed a speck in the middle of nothing). Past FUDE_ZOOM_NAV_DRAWN_DEPTH levels down, a frame in use counts
// for its own box. A removed frame holds nothing.
RDE_INTERNAL fude_zoom_box fude_zoom_nav_drawn_in(const fude_zoom_scene* _s, u32 _frame, u32 _skip, u32 _depth) {
    fude_zoom_box _all = fude_zoom_box_empty();
    const fude_zoom_frame* _f = fude_zoom_scene_frame(_s, _frame);
    if(_f->removed) {
        return _all;
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_f->order); _i++) {
        const u32               _k = ((const u32*)_f->order.memory)[_i];
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _k);
        if(_k == _skip) {
            continue;
        }
        if(_o->kind == FUDE_ZOOM_KIND_FRAME && (_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
            if(_depth >= FUDE_ZOOM_NAV_DRAWN_DEPTH) {
                _all = fude_zoom_scene_frame_used(_s, _o->child) ? fude_zoom_box_union(_all, _o->box) : _all;
                continue;
            }
            const fude_zoom_box _in = fude_zoom_nav_drawn_in(_s, _o->child, FUDE_ZOOM_NONE, _depth + 1u);
            if(!fude_zoom_box_is_empty(_in)) {
                _all = fude_zoom_box_union(_all, fude_zoom_sim_box(fude_zoom_sim_from_xform(fude_zoom_scene_frame(_s, _o->child)->xf), _in));
            }
            continue;
        }
        if(fude_zoom_nav_seen(_s, _o)) {
            _all = fude_zoom_box_union(_all, _o->box);
        }
    }
    return _all;
}

// See the header.
fude_zoom_box fude_zoom_nav_drawn(const fude_zoom_scene* _s, u32 _frame, u32 _skip) {
    return _frame == FUDE_ZOOM_NONE ? fude_zoom_box_empty() : fude_zoom_nav_drawn_in(_s, _frame, _skip, 0u);
}

// Finds the deepest frame that holds all that is drawn in frame *_f (its box *_box, its units): down into the one frame
// in it with anything drawn, while nothing else is, at most FUDE_ZOOM_NAV_DRAWN_DEPTH levels; *_f and *_box are then
// that frame's. Seen from there it is exact, however far out *_f is.
RDE_INTERNAL void fude_zoom_nav_deepest(const fude_zoom_scene* _s, u32* _f, fude_zoom_box* _box) {
    for(u32 _depth = 0; _depth < FUDE_ZOOM_NAV_DRAWN_DEPTH; _depth++) {
        const fude_zoom_frame* _fr   = fude_zoom_scene_frame(_s, *_f);
        u32                    _only = FUDE_ZOOM_NONE;
        b8                     _more = false;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_fr->order) && !_more; _i++) {
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, ((const u32*)_fr->order.memory)[_i]);
            if(_o->kind == FUDE_ZOOM_KIND_FRAME && (_o->flags & FUDE_ZOOM_FLAG_ALIVE)) {
                if(!fude_zoom_box_is_empty(fude_zoom_nav_drawn(_s, _o->child, FUDE_ZOOM_NONE))) {
                    _more = _only != FUDE_ZOOM_NONE;
                    _only = _o->child;
                }
            } else {
                _more = fude_zoom_nav_seen(_s, _o);
            }
        }
        if(_more || _only == FUDE_ZOOM_NONE) {
            return;
        }
        *_f   = _only;
        *_box = fude_zoom_nav_drawn(_s, _only, FUDE_ZOOM_NONE);
    }
}

// The marks' visitor for fude_zoom_index_nearest (and for each frame in the searched one, called by hand): what is on
// screen but under FUDE_ZOOM_NAV_RING points becomes a ring; what is off screen is kept when it is the nearest yet in
// its eighth of a turn. Returns false, to stop, after 4096 things, or once every eighth has its arrow and the rings
// are full.
RDE_INTERNAL b8 fude_zoom_nav_visit(any _user, u32 _value, fude_zoom_box _box, fude_zoom_box _leaf, f64 _d2) {
    fude_zoom_nav_search*   _q = (fude_zoom_nav_search*)_user;
    const fude_zoom_object* _o = fude_zoom_scene_object(_q->s, _value);
    if(++_q->seen > 4096u) {
        return false;
    }
    // Only what is drawn, as fude_zoom_nav_seen says (what fude_zoom_nav_drawn counts, so an arrow never leads to what
    // a tap's view leaves out): not the skipped frame, not a dead object, not a bookmark, not a layer's record
    // (nothing to see, nowhere: its box is a point at the origin, and an arrow to it flew there zoomed in without end),
    // not what a hidden layer hides, not a guide or an attribute. A frame in it comes with what is drawn in it for its
    // box, and frames let go of are left out, by fude_zoom_nav_marks.
    if(_value == _q->skip || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || (_o->kind != FUDE_ZOOM_KIND_FRAME && !fude_zoom_nav_seen(_q->s, _o))) {
        return true;
    }
    // Each object once, where it is now: the index keeps a moved object's old entries (stale, where it was: scene.h)
    // until it is built again, so an entry whose box is not the object's own is passed over, and one moved away and
    // back, two entries with its box, is visited at the first. (A frame in it is handed in by hand, with its drawing's
    // box, never from the index.)
    if(_o->kind != FUDE_ZOOM_KIND_FRAME) {
        const fude_zoom_box _own = _o->box;
        if(_box.min_x != _own.min_x || _box.min_y != _own.min_y || _box.max_x != _own.max_x || _box.max_y != _own.max_y) {
            return true;
        }
        if(rde_hash_set_is_inited(&_q->visited) && !rde_hash_set_add(&_q->visited, (const any)&_value)) {
            return true;
        }
    }
    // What a tap shows: it and its neighbours (its leaf), unless they spread far wider.
    const f64     _size = rde_math_max_f64(_box.max_x - _box.min_x, _box.max_y - _box.min_y);
    if(!(_size > 0.0)) {
        return true;   // Nothing there to show.
    }
    const f64     _wide = rde_math_max_f64(_leaf.max_x - _leaf.min_x, _leaf.max_y - _leaf.min_y);
    fude_zoom_box _show = _wide <= _size * 16.0 ? _leaf : _box;
    const u32     _in   = _q->as_frame != FUDE_ZOOM_NONE ? _q->as_frame : _q->frame;
    _show               = _q->as_frame != FUDE_ZOOM_NONE ? _q->as_box : _show;
    const fude_zoom_v2 _c = fude_zoom_sim_apply(_q->to_screen, (fude_zoom_v2){ (_box.min_x + _box.max_x) * 0.5, (_box.min_y + _box.max_y) * 0.5 });
    if(fude_zoom_box_overlaps(_box, _q->view)) {
        // On screen: a ring when it is too small to see (else it is drawn and seen).
        if(_size * _q->z < FUDE_ZOOM_NAV_RING && _q->ring_count < FUDE_ZOOM_NAV_RINGS) {
            _q->rings[_q->ring_count++] = (fude_zoom_nav_mark){ { (f32)_c.x, (f32)_c.y }, 0.0f, true, _in, _show };
        }
        return true;
    }
    // Off screen: which eighth of a turn it is in (the eighths centred on the eight ways), and how far on screen.
    const f64 _angle  = rde_math_atan2_f64(_c.y, _c.x);
    const u32 _sector = (u32)rde_math_floor_f64((_angle + RDE_PI) / (2.0 * RDE_PI) * (f64)FUDE_ZOOM_NAV_ARROWS + 0.5) % FUDE_ZOOM_NAV_ARROWS;
    const f64 _d      = rde_math_sqrt_f64(_d2) * _q->z;
    if(_d < _q->best_d[_sector]) {
        _q->filled        += _q->best_d[_sector] >= 1e300 ? 1u : 0u;
        _q->best_d[_sector] = _d;
        _q->best[_sector]   = (fude_zoom_nav_mark){ { 0.0f, 0.0f }, (f32)_angle, false, _in, _show };
    }
    return _q->filled < FUDE_ZOOM_NAV_ARROWS || _q->ring_count < FUDE_ZOOM_NAV_RINGS;
}

// See the header. Each frame searched (the camera's, then two above it, each skipping the frame it came up from) keeps
// its own nearest in each eighth; the nearest over all three is taken. Rings come after the arrows, the first found
// first.
u32 fude_zoom_nav_marks(const fude_zoom_scene* _s, fude_zoom_v2 _half, f32 _inset, fude_zoom_nav_mark* _out) {
    // Each searched frame: the camera's, then two above it; the nearest in each direction over all three.
    fude_zoom_nav_mark _best[FUDE_ZOOM_NAV_ARROWS];
    f64                _best_d[FUDE_ZOOM_NAV_ARROWS];
    fude_zoom_nav_mark _rings[FUDE_ZOOM_NAV_RINGS];
    u32                _ring_count = 0;
    for(u32 _i = 0; _i < FUDE_ZOOM_NAV_ARROWS; _i++) {
        _best_d[_i] = 1e300;
    }
    const fude_zoom_sim _cam_to_screen = fude_zoom_camera_sim(&_s->camera);
    u32 _frame = _s->camera.frame, _skip = FUDE_ZOOM_NONE, _from = _s->camera.frame;
    for(u32 _level = 0; _level < 3u && _frame != FUDE_ZOOM_NONE; _level++) {
        fude_zoom_nav_search _q;
        rde_memset(&_q, 0, sizeof _q);
        _q.s         = _s;
        _q.skip      = _skip;
        _q.frame     = _frame;
        _q.to_screen = fude_zoom_sim_compose(_cam_to_screen, fude_zoom_scene_sim(_s, _frame, _from));
        _q.view      = fude_zoom_sim_box(fude_zoom_sim_inverse(_q.to_screen), (fude_zoom_box){ -_half.x, -_half.y, _half.x, _half.y });
        _q.z         = fude_zoom_sim_scale(_q.to_screen);
        _q.as_frame  = FUDE_ZOOM_NONE;
        for(u32 _i = 0; _i < FUDE_ZOOM_NAV_ARROWS; _i++) {
            _q.best_d[_i] = 1e300;
        }
        const fude_zoom_v2 _mid = fude_zoom_sim_apply(fude_zoom_sim_inverse(_q.to_screen), (fude_zoom_v2){ 0.0, 0.0 });
        if(fude_zoom_scene_frame(_s, _frame)->stale > 0u) {
            _q.visited = rde_hash_set_new(sizeof(u32), rde_hash_map_fn_u32_hash, rde_hash_map_fn_u32_cmp, NULL, rde_memory_allocator_get_default_std());
        }
        fude_zoom_index_nearest(&fude_zoom_scene_frame(_s, _frame)->index, _mid, fude_zoom_nav_visit, &_q, 4096u);
        if(rde_hash_set_is_inited(&_q.visited)) {
            rde_hash_set_free(&_q.visited);
        }
        // The frames in it are not in its index: each looked at too, as what is drawn in it (a tap shows its drawing
        // from the deepest frame holding it). Their visits do not count towards the 4096.
        const fude_zoom_frame* _fr = fude_zoom_scene_frame(_s, _frame);
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_fr->kids); _i++) {
            const u32               _k = ((const u32*)_fr->kids.memory)[_i];
            const fude_zoom_object* _o = fude_zoom_scene_object(_s, _k);
            if(_k == _skip || !(_o->flags & FUDE_ZOOM_FLAG_ALIVE) || !fude_zoom_scene_frame_used(_s, _o->child)) {
                continue;
            }
            fude_zoom_box _b = fude_zoom_nav_drawn(_s, _o->child, FUDE_ZOOM_NONE);
            if(fude_zoom_box_is_empty(_b)) {
                continue;   // All of it rubbed out, or hidden: nothing to see at the end of an arrow.
            }
            _q.as_frame = _o->child;
            _q.as_box   = _b;
            fude_zoom_nav_deepest(_s, &_q.as_frame, &_q.as_box);
            _b = fude_zoom_sim_box(fude_zoom_sim_from_xform(fude_zoom_scene_frame(_s, _o->child)->xf), _b);
            const f64 _dx = _mid.x < _b.min_x ? _b.min_x - _mid.x : (_mid.x > _b.max_x ? _mid.x - _b.max_x : 0.0);
            const f64 _dy = _mid.y < _b.min_y ? _b.min_y - _mid.y : (_mid.y > _b.max_y ? _mid.y - _b.max_y : 0.0);
            const u32 _seen = _q.seen;
            fude_zoom_nav_visit(&_q, _k, _b, _b, _dx * _dx + _dy * _dy);
            _q.seen     = _seen;
            _q.as_frame = FUDE_ZOOM_NONE;
        }
        for(u32 _i = 0; _i < FUDE_ZOOM_NAV_ARROWS; _i++) {
            if(_q.best_d[_i] < _best_d[_i]) {
                _best_d[_i] = _q.best_d[_i];
                _best[_i]   = _q.best[_i];
            }
        }
        for(u32 _i = 0; _i < _q.ring_count && _ring_count < FUDE_ZOOM_NAV_RINGS; _i++) {
            _rings[_ring_count++] = _q.rings[_i];
        }
        _skip  = fude_zoom_scene_frame(_s, _frame)->object;
        _frame = fude_zoom_scene_frame(_s, _frame)->parent;
    }
    // The arrows nearest first, none within a third of a radian of a nearer one, each where its way out meets the
    // inset edge.
    u32 _n = 0;
    for(;;) {
        i32 _pick = -1;
        for(u32 _i = 0; _i < FUDE_ZOOM_NAV_ARROWS; _i++) {
            if(_best_d[_i] < 1e300 && (_pick < 0 || _best_d[_i] < _best_d[_pick])) {
                _pick = (i32)_i;
            }
        }
        if(_pick < 0) {
            break;
        }
        fude_zoom_nav_mark _m = _best[_pick];
        _best_d[_pick] = 1e300;
        b8 _close = false;
        for(u32 _j = 0; _j < _n && !_close; _j++) {
            f32 _da = rde_math_abs_f32(_out[_j].angle - _m.angle);
            _da     = _da > (f32)RDE_PI ? (f32)(2.0 * RDE_PI) - _da : _da;
            _close  = _da < 0.33f;
        }
        if(_close) {
            continue;
        }
        const f32 _cx = rde_math_cos_f32(_m.angle), _cy = rde_math_sin_f32(_m.angle);
        const f32 _hx = (f32)_half.x - _inset, _hy = (f32)_half.y - _inset;
        const f32 _t  = rde_math_min_f32(rde_math_abs_f32(_cx) > 1e-6f ? _hx / rde_math_abs_f32(_cx) : 1e30f, rde_math_abs_f32(_cy) > 1e-6f ? _hy / rde_math_abs_f32(_cy) : 1e30f);
        _m.at = (rde_vec_2F){ _cx * _t, _cy * _t };
        _out[_n++] = _m;
    }
    for(u32 _i = 0; _i < _ring_count && _n < FUDE_ZOOM_NAV_MARKS; _i++) {
        _out[_n++] = _rings[_i];
    }
    return _n;
}

// --- depth -----------------------------------------------------------------------------------

// Returns log10 of a unit of frame _f in the root's units.
RDE_INTERNAL f64 fude_zoom_nav_log10_unit(const fude_zoom_scene* _s, u32 _f) {
    f64 _g = 0.0;
    for(; _f != FUDE_ZOOM_NONE && fude_zoom_nav_parent(_s, _f) != FUDE_ZOOM_NONE; _f = fude_zoom_nav_parent(_s, _f)) {
        _g += rde_math_log10_f64(rde_math_abs_f64(fude_zoom_scene_frame(_s, _f)->xf.scale));
    }
    return _g;
}

// See the header.
f64 fude_zoom_nav_depth_of(const fude_zoom_scene* _s, fude_zoom_camera _c) {
    // The zoom in home's units: a unit of the camera's frame is 10^(g_cam − g_home) of home's.
    return rde_math_log10_f64(_c.z) - (fude_zoom_nav_log10_unit(_s, _c.frame) - fude_zoom_nav_log10_unit(_s, _s->home));
}

// See the header.
f64 fude_zoom_nav_depth(const fude_zoom_scene* _s) {
    return fude_zoom_nav_depth_of(_s, _s->camera);
}

// --- levels ----------------------------------------------------------------------------------

// Returns the view of all that is drawn in frame _f (its box _box, its units), from the deepest frame holding it,
// filling 80% of the screen.
RDE_INTERNAL fude_zoom_camera fude_zoom_nav_view_all(const fude_zoom_scene* _s, fude_zoom_v2 _half, u32 _f, fude_zoom_box _box) {
    fude_zoom_nav_deepest(_s, &_f, &_box);
    return fude_zoom_nav_framing(_half, _f, _box, 0.8);
}

// See the header. Walks up from the camera's frame, carrying what is drawn below in each parent's units. A level's view
// is "the view the camera is at" when its depth is within 0.15 (log10) of the camera's and its middle within a tenth
// of the screen's shorter half side of the camera's.
u32 fude_zoom_nav_levels(const fude_zoom_scene* _s, fude_zoom_v2 _half, fude_zoom_camera* _out, u32 _max) {
    if(_max == 0) {
        return 0;
    }
    const fude_zoom_camera* _cam  = &_s->camera;
    const f64               _here = fude_zoom_nav_depth(_s);
    u32           _n     = 0;
    fude_zoom_v2  _at    = _cam->at;
    u32           _f     = _cam->frame;
    fude_zoom_box _box   = fude_zoom_nav_drawn(_s, _f, FUDE_ZOOM_NONE);   // All drawn at this level, its units.
    fude_zoom_box _shown = fude_zoom_box_empty();                         // The last level given's, in these units.
    for(;;) {
        const fude_zoom_frame* _fr = fude_zoom_scene_frame(_s, _f);
        if(!fude_zoom_box_is_empty(_box)) {
            const f64 _size = rde_math_max_f64(_box.max_x - _box.min_x, _box.max_y - _box.min_y);
            const f64 _was  = fude_zoom_box_is_empty(_shown) ? 0.0 : rde_math_max_f64(_shown.max_x - _shown.min_x, _shown.max_y - _shown.min_y);
            if(fude_zoom_box_is_empty(_shown) || _size > _was * FUDE_ZOOM_NAV_LEVEL_GROWS) {
                const fude_zoom_camera _view = fude_zoom_nav_view_all(_s, _half, _f, _box);
                const fude_zoom_v2     _mid  = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _f, _cam->frame), _view.at);
                const b8 _same = rde_math_abs_f64(fude_zoom_nav_depth_of(_s, _view) - _here) < 0.15 &&
                                 rde_math_hypot_f64(_mid.x - _cam->at.x, _mid.y - _cam->at.y) * _cam->z < 0.1 * rde_math_min_f64(_half.x, _half.y);
                if(!_same) {
                    _out[_n < _max ? _n++ : _max - 1u] = _view;   // More than fit: the outermost in the last.
                }
                _shown = _box;
            }
        }
        if(_fr->parent == FUDE_ZOOM_NONE) {
            break;
        }
        // Up a level: this one's drawing in its parent's units, and the parent's own beside it.
        const fude_zoom_sim _up = fude_zoom_sim_from_xform(_fr->xf);
        _at    = fude_zoom_sim_apply(_up, _at);
        _box   = fude_zoom_box_is_empty(_box) ? _box : fude_zoom_sim_box(_up, _box);
        _shown = fude_zoom_box_is_empty(_shown) ? _shown : fude_zoom_sim_box(_up, _shown);
        _box   = fude_zoom_box_union(_box, fude_zoom_nav_drawn(_s, _fr->parent, _fr->object));
        _f     = _fr->parent;
    }
    // None: the whole of it is what is on screen (there it is anyway), or nothing is drawn (the top at its zoom 1).
    if(_n == 0) {
        _out[_n++] = fude_zoom_box_is_empty(_box) ? (fude_zoom_camera){ _f, _at, 1.0 } : fude_zoom_nav_view_all(_s, _half, _f, _box);
    }
    return _n;
}

// See the header. From ×0.0001 to under ×1000 the zoom is written out: whole from 9.95 up, one decimal from 0.995
// up, below that one figure that counts. Past that range it is a figure times a power of ten.
void fude_zoom_nav_say(f64 _log10, c8* _out, usize _size) {
    if(_size == 0) {
        return;
    }
    if(_log10 >= -4.0 && _log10 < 3.0) {
        const f64 _m = rde_math_pow_f64(10.0, _log10);
        if(_m >= 9.95) {
            rde_snprintf(_out, _size, "\xC3\x97%.0f", _m);
        } else if(_m >= 0.995) {
            rde_snprintf(_out, _size, "\xC3\x97%.1f", _m);
        } else {
            // One figure that counts: ×0.5, ×0.05, ×0.004.
            rde_snprintf(_out, _size, "\xC3\x97%.*f", (int)rde_math_ceil_f64(-_log10 - 1e-9), _m);
        }
        // "×2.0" says no more than "×2".
        const usize _n = rde_strlen(_out);
        if(_n > 2u && _out[_n - 1u] == '0' && _out[_n - 2u] == '.') {
            _out[_n - 2u] = 0;
        }
        return;
    }
    // Its first figure (left out when it is 1) times 10 to the power, the power in superscript digits: ×10⁶, ×3·10³,
    // ×4·10⁻⁶.
    static const c8* const SUPER[10] = { "\xE2\x81\xB0", "\xC2\xB9", "\xC2\xB2", "\xC2\xB3", "\xE2\x81\xB4", "\xE2\x81\xB5", "\xE2\x81\xB6", "\xE2\x81\xB7", "\xE2\x81\xB8", "\xE2\x81\xB9" };
    i32 _e    = (i32)rde_math_floor_f64(_log10);
    i32 _lead = (i32)rde_math_floor_f64(rde_math_pow_f64(10.0, _log10 - (f64)_e) + 0.5);
    if(_lead >= 10) {
        _lead = 1;
        _e++;
    }
    c8 _digits[16];
    rde_snprintf(_digits, sizeof _digits, "%d", _e < 0 ? -_e : _e);
    if(_lead > 1) {
        rde_snprintf(_out, _size, "\xC3\x97%d\xC2\xB7" "10%s", _lead, _e < 0 ? "\xE2\x81\xBB" : "");
    } else {
        rde_snprintf(_out, _size, "\xC3\x97" "10%s", _e < 0 ? "\xE2\x81\xBB" : "");
    }
    for(const c8* _d = _digits; *_d != 0; _d++) {
        const usize _n = rde_strlen(_out);
        rde_snprintf(_out + _n, _size - _n, "%s", SUPER[*_d - '0']);
    }
}
