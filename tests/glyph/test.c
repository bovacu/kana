// The glyph suite: whole characters kept on the GPU (glyph.h, "Kept characters"). A character drawn again is not made
// again, wherever and in whatever colour it is drawn; a new size or pixel size makes another; the mesh is the strokes
// drawn by stroke, placed from the box's top-left; never more than FUDE_GLYPH_KEPT are kept, and none drawn in the
// current frame is let go; fude_glyph_forget lets every one go.
#include "study/chars/glyph.h"
#include "drawing/base/draw.h"
#include <stdio.h>
static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// RDE's kept stroke meshes as the renderer holds them, counted: made, let go, drawn kept, and drawn from a mesh (the
// characters over the bound). The last mesh kept is copied, to compare with drawing by stroke. (engine.c's doubles do
// nothing; these win.)
static u32             made = 0, released = 0, drawn_kept = 0, drawn_mesh = 0;
static rde_vec_2F      drawn_at;
static rde_stroke_mesh last_kept;
static f32             pixel = 1.0f;

f32 rde_rendering_2d_get_pixel_size(void) { return pixel; }
rde_kept_stroke_mesh rde_rendering_2d_keep_stroke_mesh(const rde_stroke_mesh* _mesh) {
    if(_mesh->vertices.count == 0) {
        return (rde_kept_stroke_mesh){ 0 };
    }
    made++;
    rde_arr_clear(&last_kept.vertices);
    rde_arr_clear(&last_kept.indices);
    rde_memcpy(rde_arr_add_n(&last_kept.vertices, _mesh->vertices.count), _mesh->vertices.memory, _mesh->vertices.count * sizeof(rde_stroke_vertex));
    rde_memcpy(rde_arr_add_n(&last_kept.indices, _mesh->indices.count), _mesh->indices.memory, _mesh->indices.count * sizeof(u32));
    return (rde_kept_stroke_mesh){ made, 1u };
}
void rde_rendering_2d_release_kept_stroke_mesh(rde_kept_stroke_mesh* _kept) {
    if(_kept != NULL && _kept->generation != 0u) {
        released++;
        *_kept = (rde_kept_stroke_mesh){ 0 };
    }
}
b8 rde_rendering_2d_kept_stroke_mesh_is_valid(rde_kept_stroke_mesh _kept) { return _kept.generation != 0u; }
void rde_rendering_2d_draw_kept_stroke_mesh(rde_kept_stroke_mesh _kept, rde_vec_2F _at, rde_color _color) {
    (void)_color;
    if(_kept.generation != 0u) {
        drawn_kept++;
        drawn_at = _at;
    }
}
void rde_rendering_2d_draw_stroke_mesh(const rde_stroke_mesh* _mesh, rde_vec_2F _at, rde_color _color) {
    (void)_mesh; (void)_at; (void)_color;
    drawn_mesh++;
}

static void counts_reset(void) { made = released = drawn_kept = drawn_mesh = 0; }

// The mesh kept for _info at _size is the strokes as fude_glyph_stroke drew them whole (each tessellated from its
// points on screen), moved to the box's top-left: every vertex within a thousandth of a unit, the same alphas, the
// same triangles.
static void test_same_as_by_stroke(const fude_kanji_db* _db, const fude_kanji_info* _info, f32 _size) {
    const f32        _scale  = _size / FUDE_KANJI_BOX;
    const f32        _radius = rde_math_max_f32(0.8f, _scale * FUDE_GLYPH_WIDTH * 0.5f);
    const rde_vec_2F _origin = { 37.25f, 140.5f };
    rde_stroke_mesh  _one    = rde_stroke_mesh_new(NULL);
    u32 _at = 0, _base = 0, _index_at = 0;
    b8  _same = true;
    for(u32 _s = 0; _s < _info->strokes; _s++) {
        fude_kanji_stroke _stroke;
        CHECK(fude_kanji_stroke_at(_db, _info, _s, &_stroke));
        rde_vec_2F _pts[FUDE_GLYPH_MAX_POINTS];
        f32        _radii[FUDE_GLYPH_MAX_POINTS];
        const u32  _n = fude_kanji_stroke_points(&_stroke, rde_math_max_f32(FUDE_GLYPH_TOLERANCE, FUDE_GLYPH_SCREEN_TOLERANCE / _scale), _pts, FUDE_GLYPH_MAX_POINTS);
        for(u32 _i = 0; _i < _n; _i++) {
            _pts[_i]   = (rde_vec_2F){ _origin.x + _pts[_i].x * _scale, _origin.y - _pts[_i].y * _scale };   // (on screen)
            _radii[_i] = _radius;
        }
        rde_rendering_2d_tessellate_stroke(_pts, _radii, _n, pixel, &_one);
        for(u32 _v = 0; _v < _one.vertices.count && _at < last_kept.vertices.count; _v++, _at++) {
            const rde_stroke_vertex* _a = (const rde_stroke_vertex*)rde_arr_at(&_one.vertices, _v);
            const rde_stroke_vertex* _b = (const rde_stroke_vertex*)rde_arr_at(&last_kept.vertices, _at);
            _same = _same && rde_math_abs_f32(_a->position.x - (_b->position.x + _origin.x)) < 1e-3f &&
                    rde_math_abs_f32(_a->position.y - (_b->position.y + _origin.y)) < 1e-3f && rde_math_abs_f32(_a->alpha - _b->alpha) < 1e-4f;
        }
        for(u32 _i = 0; _i < _one.indices.count && _index_at < last_kept.indices.count; _i++, _index_at++) {
            _same = _same && *(const u32*)rde_arr_at(&_one.indices, _i) + _base == *(const u32*)rde_arr_at(&last_kept.indices, _index_at);
        }
        _base += (u32)_one.vertices.count;
    }
    CHECK(_same);
    CHECK(_at == last_kept.vertices.count && _index_at == last_kept.indices.count);
    rde_stroke_mesh_free(&_one);
}

// Kept once, drawn anywhere: a second draw, at another place and in another colour, makes nothing; a new size, or a
// new pixel size, makes another. The ghost under a character writing itself is the same character.
static void test_kept_once(fude_glyph* _glyph, const fude_kanji_db* _db) {
    counts_reset();
    const rde_color _ink = { 20, 20, 20, 255 };
    CHECK(fude_glyph_character(_glyph, 0x6728u, (rde_vec_2F){ 37.25f, 140.5f }, 80.0f, _ink));   // 木
    CHECK(made == 1u && drawn_kept == 1u && drawn_mesh == 0u);
    CHECK(drawn_at.x == 37.25f && drawn_at.y == 140.5f);   // (placed from the box's top-left)
    fude_kanji_info _info;
    CHECK(fude_kanji_find(_db, 0x6728u, &_info));
    test_same_as_by_stroke(_db, &_info, 80.0f);

    CHECK(fude_glyph_character(_glyph, 0x6728u, (rde_vec_2F){ -300.0f, 12.0f }, 80.0f, (rde_color){ 200, 0, 0, 128 }));
    CHECK(made == 1u && drawn_kept == 2u && drawn_at.x == -300.0f && drawn_at.y == 12.0f);
    CHECK(fude_glyph_writing(_glyph, &_info, (rde_vec_2F){ 0.0f, 0.0f }, 80.0f, 0.0, NULL, 14.0f, 0.0f) == false);
    CHECK(made == 1u && drawn_kept == 3u);   // (its ghost: 木 at 80, kept)

    CHECK(fude_glyph_character(_glyph, 0x6728u, (rde_vec_2F){ 37.25f, 140.5f }, 81.0f, _ink));
    CHECK(made == 2u);   // a new size
    pixel = 0.5f;        // (a display of twice the density)
    CHECK(fude_glyph_character(_glyph, 0x6728u, (rde_vec_2F){ 37.25f, 140.5f }, 80.0f, _ink));
    CHECK(made == 3u);
    test_same_as_by_stroke(_db, &_info, 80.0f);
    pixel = 1.0f;
    CHECK(fude_glyph_character(_glyph, 0x6728u, (rde_vec_2F){ 0.0f, 0.0f }, 80.0f, _ink));
    CHECK(made == 3u && released == 0u);   // (the first is still kept)
    CHECK(!fude_glyph_character(_glyph, 0x1F600u, (rde_vec_2F){ 0.0f, 0.0f }, 80.0f, _ink));   // (no strokes: nothing)
    CHECK(made == 3u);
}

// The bound: FUDE_GLYPH_KEPT kept at most. A frame drawing more draws the rest from a mesh made each time and lets none
// of its own go; in the next frame, new characters take the slots drawn longest ago. fude_glyph_forget lets all go.
static void test_bound(fude_glyph* _glyph, const fude_kanji_db* _db) {
    fude_glyph_forget();
    counts_reset();
    const rde_color _ink   = { 20, 20, 20, 255 };
    const u32       _extra = 8u;
    u32 _codepoints[FUDE_GLYPH_KEPT + 16u];
    u32 _count = 0;
    for(u32 _i = 0; _i < _db->count && _count < FUDE_GLYPH_KEPT + 16u; _i++) {
        fude_kanji_info _info;
        if(fude_kanji_at(_db, _i, &_info) && _info.strokes > 0u) {
            _codepoints[_count++] = _info.codepoint;
        }
    }
    CHECK(_count == FUDE_GLYPH_KEPT + 16u);

    fude_glyph_frame();
    for(u32 _i = 0; _i < FUDE_GLYPH_KEPT + _extra; _i++) {
        fude_glyph_character(_glyph, _codepoints[_i], (rde_vec_2F){ 0.0f, 0.0f }, 30.0f, _ink);
    }
    CHECK(made == FUDE_GLYPH_KEPT && released == 0u);
    CHECK(drawn_kept == FUDE_GLYPH_KEPT && drawn_mesh == _extra);   // (the ones over the bound, from their mesh)

    fude_glyph_frame();
    for(u32 _i = FUDE_GLYPH_KEPT + _extra; _i < FUDE_GLYPH_KEPT + 2u * _extra; _i++) {
        fude_glyph_character(_glyph, _codepoints[_i], (rde_vec_2F){ 0.0f, 0.0f }, 30.0f, _ink);
    }
    CHECK(made == FUDE_GLYPH_KEPT + _extra && released == _extra);   // the oldest let go: still FUDE_GLYPH_KEPT
    CHECK(made - released == FUDE_GLYPH_KEPT);
    fude_glyph_character(_glyph, _codepoints[0], (rde_vec_2F){ 0.0f, 0.0f }, 30.0f, _ink);   // (one of those let go)
    CHECK(made == FUDE_GLYPH_KEPT + _extra + 1u && made - released == FUDE_GLYPH_KEPT);
    fude_glyph_character(_glyph, _codepoints[FUDE_GLYPH_KEPT - 1u], (rde_vec_2F){ 0.0f, 0.0f }, 30.0f, _ink);   // (still kept)
    CHECK(made == FUDE_GLYPH_KEPT + _extra + 1u);

    fude_glyph_forget();
    CHECK(made == released);   // every one let go
    fude_glyph_character(_glyph, _codepoints[FUDE_GLYPH_KEPT - 1u], (rde_vec_2F){ 0.0f, 0.0f }, 30.0f, _ink);
    CHECK(made == FUDE_GLYPH_KEPT + _extra + 2u);   // made again
    fude_glyph_forget();
}

int main(int _argc, char** _argv) {
    (void)_argc;
    fude_kanji_db _db;
    CHECK(fude_kanji_load(&_db, _argv[1]));
    last_kept = rde_stroke_mesh_new(NULL);
    fude_glyph _glyph;
    fude_glyph_init(&_glyph, &_db);
    test_kept_once(&_glyph, &_db);
    test_bound(&_glyph, &_db);
    fude_glyph_destroy(&_glyph);
    rde_stroke_mesh_free(&last_kept);
    fude_kanji_unload(&_db);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
