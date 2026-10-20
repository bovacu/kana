// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"
#include "drawing/base/redraw.h"

// ===========================================================================
// Page, export: the toolbar's Export (PNG, SVG, PDF, DXF, tiles, the cut list, STL...), a notebook's cover and
// what is drawn off screen, the zoom video (video.h), the Smoothing choices, the units, and the page's settings
// (gathered for Settings and applied from it).
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
//
// What is drawn off screen goes in stages, since a picture drawn into a render texture can only be read back the
// frame after: export_stage is 0 when nothing is under way, 1 when a PNG's view is to be drawn
// (fude_zoom_page_render_offscreen), 2 when it is drawn and waits to be read back and written
// (fude_zoom_page_export_finish, from page_frame.c), 10 when a video's next frame is to be drawn and 11 when it waits
// to be read back into the video (fude_zoom_page_video_step). A cover goes the same way on cover_stage. Every other
// format is written at once. Export's formats are numbered as page.h's fude_zoom_page_look_export lists them.
// ===========================================================================

// --- export (export.h) ------------------------------------------------------------------------------

#define FUDE_ZOOM_PAGE_EXPORT_PX    4096.0   // a PNG's longer side, at most (pixels)
#define FUDE_ZOOM_PAGE_EXPORT_SCALE 3.0      // and how many pixels a screen point, at most

// See page_parts.h. The names are in shape.h's FUDE_ZOOM_MATERIAL_ order.
void fude_zoom_page_material_words(const c8** _out, const c8* _none) {
    static const FUDE_TEXT_ _said[FUDE_ZOOM_MATERIAL_COUNT] = {
        FUDE_TEXT_ZOOM_MATERIAL_NONE, FUDE_TEXT_ZOOM_MATERIAL_PINE, FUDE_TEXT_ZOOM_MATERIAL_OAK, FUDE_TEXT_ZOOM_MATERIAL_WALNUT,
        FUDE_TEXT_ZOOM_MATERIAL_MAPLE, FUDE_TEXT_ZOOM_MATERIAL_BEECH, FUDE_TEXT_ZOOM_MATERIAL_CHERRY, FUDE_TEXT_ZOOM_MATERIAL_BIRCH_PLY,
        FUDE_TEXT_ZOOM_MATERIAL_MDF, FUDE_TEXT_ZOOM_MATERIAL_MELAMINE,
    };
    for(u32 _i = 0; _i < FUDE_ZOOM_MATERIAL_COUNT; _i++) {
        _out[_i] = _i == 0u ? _none : fude_text(_said[_i]);
    }
}

// Writes the cut list (export.h) in the app's language into _out: false when there is no board to list.
RDE_INTERNAL b8 fude_zoom_page_cutlist(const fude_zoom_page* _page, fude_bytes* _out) {
    const c8* const _header[7] = { fude_text(FUDE_TEXT_ZOOM_CUT_PART), fude_text(FUDE_TEXT_ZOOM_CUT_QTY), fude_text(FUDE_TEXT_ZOOM_CUT_LENGTH),
                                   fude_text(FUDE_TEXT_ZOOM_CUT_WIDTH), fude_text(FUDE_TEXT_ZOOM_CUT_THICKNESS), fude_text(FUDE_TEXT_ZOOM_MATERIAL),
                                   fude_text(FUDE_TEXT_ZOOM_CUT_GRAIN) };
    const c8* const _grain[2]  = { fude_text(FUDE_TEXT_ZOOM_CUT_ALONG), fude_text(FUDE_TEXT_ZOOM_CUT_ACROSS) };
    const c8* _materials[FUDE_ZOOM_MATERIAL_COUNT];
    fude_zoom_page_material_words(_materials, "");
    return fude_zoom_export_cutlist(&_page->scene, FUDE_ZOOM_PAGE_MM_PER_UNIT, _header, _grain, _materials, _out) > 0u;
}

RDE_INTERNAL void fude_zoom_page_video_begin(fude_zoom_page* _page, const c8* _path, b8 _share);

// Marks what the lasso holds, to export alone (into export_keep, a byte an object; export_hidden its opposite): what is
// held (a container with what it contains), everything inside a frame held, and the frames down to what is held (to be
// walked into). False: nothing held. A frame chain longer than 4096 is not followed further.
RDE_INTERNAL b8 fude_zoom_page_export_keep(fude_zoom_page* _page) {
    const u32 _held = (u32)rde_arr_length(&_page->selection.picks);
    if(_held == 0u) {
        return false;
    }
    const fude_zoom_scene* _s     = &_page->scene;
    const u32              _count = fude_zoom_scene_object_count(_s);
    rde_memory_allocator*  _heap  = rde_memory_allocator_get_default_std();
    if(!rde_arr_is_inited(&_page->export_keep)) {
        _page->export_keep   = rde_arr_new(sizeof(u8), _heap);
        _page->export_hidden = rde_arr_new(sizeof(u8), _heap);
    }
    rde_arr_clear(&_page->export_keep);
    rde_arr_clear(&_page->export_hidden);
    u8* _keep = rde_arr_add_n(&_page->export_keep, _count);
    u8* _hide = rde_arr_add_n(&_page->export_hidden, _count);
    rde_arr _held_by_arr = rde_arr_new(sizeof(u8), _heap);
    rde_arr_resize(&_held_by_arr, (usize)_count + 1u);
    u8* _held_by = (u8*)_held_by_arr.memory;   // (held itself)
    rde_memset(_keep, 0, _count);
    // (a container held: what is in it held with it — an area exported is the area)
    rde_arr _with = rde_arr_new(sizeof(u32), _heap);
    for(u32 _i = 0; _i < _held; _i++) {
        const u32 _o = ((const fude_zoom_pick*)_page->selection.picks.memory)[_i].object;
        rde_arr_add(&_with, (any)&_o);
        if(fude_zoom_page_container(_s, _o)) {
            fude_zoom_page_container_contents(_s, _o, &_with);
        }
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_with); _i++) {
        const u32 _o = ((const u32*)_with.memory)[_i];
        if(_o < _count) {
            _held_by[_o] = 1u;
            _keep[_o]    = 1u;
            // The frames down to it, from the top.
            for(u32 _f = fude_zoom_scene_object(_s, _o)->frame, _d = 0; _f != FUDE_ZOOM_NONE && _d < 4096u; _f = fude_zoom_scene_frame(_s, _f)->parent, _d++) {
                const u32 _fo = fude_zoom_scene_frame(_s, _f)->object;
                if(_fo != FUDE_ZOOM_NONE && _fo < _count) {
                    _keep[_fo] = 1u;
                }
            }
        }
    }
    // Inside a frame held: kept, at any depth.
    for(u32 _i = 0; _i < _count; _i++) {
        if(_keep[_i] != 0u) {
            continue;
        }
        for(u32 _f = fude_zoom_scene_object(_s, _i)->frame, _d = 0; _f != FUDE_ZOOM_NONE && _d < 4096u; _f = fude_zoom_scene_frame(_s, _f)->parent, _d++) {
            const u32 _fo = fude_zoom_scene_frame(_s, _f)->object;
            if(_fo != FUDE_ZOOM_NONE && _fo < _count && _held_by[_fo] != 0u) {
                _keep[_i] = 1u;
                break;
            }
        }
    }
    rde_arr_free(&_held_by_arr);
    rde_arr_free(&_with);
    for(u32 _i = 0; _i < _count; _i++) {
        _hide[_i] = _keep[_i] != 0u ? 0u : 1u;
    }
    return true;
}

// Returns the view an export of what the lasso holds is of: the selection's box framed (in the camera's frame), 90% of
// the screen; the camera as it is when there is no box.
RDE_INTERNAL fude_zoom_camera fude_zoom_page_export_framing(fude_zoom_page* _page) {
    fude_zoom_scene* _s = &_page->scene;
    fude_zoom_box    _b;
    if(!fude_zoom_select_box(&_page->selection, &_b)) {
        return _s->camera;
    }
    const fude_zoom_v2 _a = fude_zoom_camera_to_frame(&_s->camera, (fude_zoom_v2){ _b.min_x, _b.min_y });
    const fude_zoom_v2 _c = fude_zoom_camera_to_frame(&_s->camera, (fude_zoom_v2){ _b.max_x, _b.max_y });
    const fude_zoom_box _in = { rde_math_min_f64(_a.x, _c.x), rde_math_min_f64(_a.y, _c.y), rde_math_max_f64(_a.x, _c.x), rde_math_max_f64(_a.y, _c.y) };
    return fude_zoom_nav_framing(fude_zoom_page_half(_page), _s->camera.frame, _in, 0.9);
}

RDE_INTERNAL void fude_zoom_page_export_write(fude_zoom_page* _page, u8 _format, const c8* _path, b8 _share);

// Exports in format _format to _path (then to the share sheet when _share): of the view or, the lasso holding
// something, of that alone, framed. The printer's check, the cut list, the video, the areas, the sheets and the PDF a
// canvas is over are never a selection's. A PNG of what is held keeps its framing for the drawing the frame after;
// any other format is framed and limited to what is held only while it is written.
RDE_INTERNAL void fude_zoom_page_export_to(fude_zoom_page* _page, u8 _format, const c8* _path, b8 _share) {
    const b8 _over_pdf = _format == 2u && fude_zoom_pdfview_is_open(&_page->pdf);
    const b8 _held     = _format != 5u && _format != 6u && _format != 8u && _format != 9u && _format != 10u && !_over_pdf && fude_zoom_page_export_keep(_page);
    fude_zoom_export_units(FUDE_ZOOM_PAGE_MM_PER_UNIT, &_page->units);   // (a dimension's number, a sheet's rulers)
    _page->export_held = _held && _format == 0u;
    if(_held && _format == 0u) {
        _page->export_view = fude_zoom_page_export_framing(_page);   // (the PNG: drawn the frame after)
    }
    if(!_held || _format == 0u) {
        fude_zoom_page_export_write(_page, _format, _path, _share);
        return;
    }
    fude_zoom_scene*       _s   = &_page->scene;
    const fude_zoom_camera _was = _s->camera;
    _s->camera = fude_zoom_page_export_framing(_page);
    fude_zoom_export_only((const u8*)_page->export_keep.memory, (u32)rde_arr_length(&_page->export_keep));
    fude_zoom_page_export_write(_page, _format, _path, _share);
    fude_zoom_export_only(NULL, 0u);
    _s->camera = _was;
}

// Export's Areas: a PDF page each, in Present's order, each page the area's shape (its longer side 842 points, an
// A4's; its shorter 120 at least), the area framed in it. False: no areas.
RDE_INTERNAL b8 fude_zoom_page_areas_pdf(fude_zoom_page* _page, fude_bytes* _out) {
    fude_zoom_scene*   _s = &_page->scene;
    fude_zoom_map_area _areas[FUDE_ZOOM_MAP_AREAS];
    const u32 _n = fude_zoom_map_areas(_s, _areas, FUDE_ZOOM_MAP_AREAS);
    if(_n == 0u) {
        return false;
    }
    fude_zoom_map_areas_order(_areas, _n);
    fude_zoom_camera _views[FUDE_ZOOM_MAP_AREAS];
    fude_zoom_v2     _halves[FUDE_ZOOM_MAP_AREAS];
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _areas[_i].object);
        const f64 _w = rde_math_max_f64(_o->box.max_x - _o->box.min_x, 1e-300), _h = rde_math_max_f64(_o->box.max_y - _o->box.min_y, 1e-300);
        _halves[_i] = _w >= _h ? (fude_zoom_v2){ 421.0, rde_math_max_f64(421.0 * _h / _w, 60.0) } : (fude_zoom_v2){ rde_math_max_f64(421.0 * _w / _h, 60.0), 421.0 };
        _views[_i]  = fude_zoom_nav_framing(_halves[_i], _o->frame, _o->box, 0.96);
    }
    return fude_zoom_export_pdf_views(_s, fude_theme_active()->page, _views, _halves, _n, _out);
}

// A notebook as a PDF: a page each, at its true size (a millimetre a millimetre), what is drawn on it at any depth,
// and its paper under it; the last one, when it is kept empty to write on, is left out. False: nothing written.
RDE_INTERNAL b8 fude_zoom_page_pages_pdf(fude_zoom_page* _page, fude_bytes* _out) {
    const fude_zoom_pages* _p = &_page->pages;
    u32 _n = _p->count;
    if(_n > 1u && !fude_zoom_pages_used(_p, &_page->scene, _n - 1u)) {
        _n--;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr TYPE(fude_zoom_camera) _views_arr  = rde_arr_new(sizeof(fude_zoom_camera), _heap);
    rde_arr TYPE(fude_zoom_v2)     _halves_arr = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    rde_arr_resize(&_views_arr, _n);
    rde_arr_resize(&_halves_arr, _n);
    fude_zoom_camera* _views  = (fude_zoom_camera*)_views_arr.memory;
    fude_zoom_v2*     _halves = (fude_zoom_v2*)_halves_arr.memory;
    const f64 _pt = 72.0 / 25.4 / FUDE_ZOOM_PAGE_MM_PER_UNIT;   // (a home unit's points)
    for(u32 _i = 0; _i < _n; _i++) {
        const fude_zoom_box _b = fude_zoom_pages_box(_p, _i);
        _views[_i]  = (fude_zoom_camera){ _page->scene.home, { (_b.min_x + _b.max_x) * 0.5, (_b.min_y + _b.max_y) * 0.5 }, _pt };
        _halves[_i] = (fude_zoom_v2){ (_b.max_x - _b.min_x) * 0.5 * _pt, (_b.max_y - _b.min_y) * 0.5 * _pt };
    }
    // (each page's paper under it, as it is printed: at its true spacing, no finer lines between)
    rde_arr TYPE(rde_arr) _papers_arr = rde_arr_new(sizeof(rde_arr), _heap);
    rde_arr_resize(&_papers_arr, _n);
    rde_arr* _papers = (rde_arr*)_papers_arr.memory;
    for(u32 _i = 0; _i < _n; _i++) {
        _papers[_i] = rde_arr_new(sizeof(fude_zoom_paper_line), _heap);
        fude_zoom_paper_sheet(_page->app->canvas->page.paper, fude_zoom_pages_box(_p, _i), FUDE_ZOOM_PAGES_MARGIN, FUDE_ZOOM_PAGES_LINES, FUDE_ZOOM_PAGES_GRID,
                              FUDE_ZOOM_PAGE_STAFF, &_papers[_i]);
    }
    const b8 _made = _n > 0u && fude_zoom_export_pdf_pages(&_page->scene, _views, _halves, _n, (fude_zoom_v2){ 1.0, 1.0 }, _papers, _out);
    for(u32 _i = 0; _i < _n; _i++) {
        rde_arr_free(&_papers[_i]);
    }
    rde_arr_free(&_papers_arr);
    rde_arr_free(&_views_arr);
    rde_arr_free(&_halves_arr);
    return _made;
}

// Returns string _id, in the app's language, for printing in a PDF's own words (a template's test square, the
// printer's check), or NULL when the PDF's fonts cannot write it (pdf.h: a character none of them has would print as
// the missing-glyph box): the words are left out then, the square and the lines speaking for themselves.
RDE_INTERNAL const c8* fude_zoom_page_pdf_words(FUDE_TEXT_ _id) {
    const c8* _words = fude_text(_id);
    return fude_zoom_pdf_text_writes(_words) ? _words : NULL;
}

// Sets the PDF's fonts (pdf.h) to the ones the screen draws a canvas's text in, in the same order: each face's Roboto
// (the regular one ui.c's, the others page_render.c's, for a note's bold and italic words), then the app's script and
// Latin fonts (info.h), and last, behind the regular face only, Phosphor's icons, as ui.c chains them.
RDE_INTERNAL void fude_zoom_page_pdf_fonts(const fude_zoom_page* _page) {
    static const c8* const _faces[FUDE_ZOOM_PDF_FACES] = { FUDE_DRAW_FONT_REGULAR, FUDE_DRAW_FONT_BOLD, FUDE_DRAW_FONT_ITALIC, FUDE_DRAW_FONT_BOLD_ITALIC };
    const fude_app_info*     _info = _page->app->info;
    fude_zoom_pdf_font_files _files;
    rde_memset(&_files, 0, sizeof(_files));
    for(u32 _face = 0; _face < FUDE_ZOOM_PDF_FACES; _face++) {
        u32 _n = 0;
        _files.chains[_face][_n++] = _faces[_face];
        if(_info->script_font != NULL) {
            _files.chains[_face][_n++] = _info->script_font;
        }
        if(_info->latin_font != NULL) {
            _files.chains[_face][_n++] = _info->latin_font;
        }
        if(_face == 0u) {
            _files.chains[_face][_n++] = "assets/fonts/Phosphor-Regular.ttf";
        }
    }
    fude_zoom_pdf_fonts(&_files);
}

// Writes the view out to _path in format _format: the video begun (fude_zoom_page_video_begin); a PNG drawn in the next
// frame's render and written in the update after (fude_zoom_page_render_offscreen, fude_zoom_page_export_finish); any
// other format written now. _share: then to the system's share sheet. A notice says what came of it.
RDE_INTERNAL void fude_zoom_page_export_write(fude_zoom_page* _page, u8 _format, const c8* _path, b8 _share) {
    if(_format == 8u) {
        fude_zoom_page_video_begin(_page, _path, _share);   // (made over the frames to come)
        return;
    }
    fude_zoom_page_pdf_fonts(_page);   // (a PDF's text in the screen's fonts; read only when a PDF writes text)
    if(_format == 2u && fude_zoom_pdfview_is_open(&_page->pdf)) {
        // Over a PDF: the PDF itself, with what was drawn on it (pdfview.h).
        // (in the screen's faces: a note's bold and italic words, page_render.c's, measured and set in their own)
        rde_font* const _fonts[FUDE_PDF_FACES] = { _page->app->font, _page->renderer.font_bold, _page->renderer.font_italic, _page->renderer.font_bold_italic };
        if(!fude_zoom_pdfview_export(&_page->pdf, &_page->scene, _fonts, _page->app->font_px, &_page->formulas, _path) ||
           (_share && !rde_mobile_share_file(_path, "application/pdf", _page->app->info->name))) {
            fude_notice_show(fude_text(FUDE_TEXT_DOC_EXPORT_FAILED));
        } else if(!_share) {
            c8 _line[1200];
            FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_EXPORT_SAVED, FUDE_TS(_path));
            fude_notice_show(_line);
        }
        return;
    }
    if(_format == 2u && fude_zoom_page_paged(_page)) {
        // A notebook: its pages, each at its true size.
        fude_bytes _b = fude_bytes_new(1u << 16);
        const b8 _made  = fude_zoom_page_pages_pdf(_page, &_b);
        const b8 _wrote = _made && fude_bytes_write_and_free(&_b, _path, NULL);
        if(!_made) {
            rde_arr_free(&_b);
        }
        if(!_wrote || (_share && !rde_mobile_share_file(_path, "application/pdf", _page->app->info->name))) {
            fude_notice_show(fude_text(FUDE_TEXT_DOC_EXPORT_FAILED));
        } else if(!_share) {
            c8 _line[1200];
            FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_EXPORT_SAVED, FUDE_TS(_path));
            fude_notice_show(_line);
        }
        return;
    }
    if(_format != 0u) {
        // Written now: a drawing (SVG), a document (PDF), or a cutting file (DXF, in millimetres, its pre-flight told).
        static const c8* const _types[8] = { "image/png", "image/svg+xml", "application/pdf", "application/dxf", "application/pdf", "application/pdf", "text/csv", "application/pdf" };
        fude_bytes _b = fude_bytes_new(1u << 16);
        fude_zoom_export_check _check;
        rde_memset(&_check, 0, sizeof(_check));
        const fude_zoom_v2 _half = fude_zoom_page_half(_page);
        u32 _pages = 0;
        fude_zoom_stl_said _stl;
        rde_memset(&_stl, 0, sizeof(_stl));
        const b8 _made = _format == 1u ? fude_zoom_export_svg(&_page->scene, _half, fude_theme_active()->page, &_b) :
                         _format == 2u ? fude_zoom_export_pdf(&_page->scene, _half, fude_theme_active()->page, &_b) :
                         _format == 4u || _format == 7u ? fude_zoom_export_tiles(&_page->scene, _half, fude_zoom_page_mm_per_point(_page),
                                                                                 _format == 7u ? 215.9 : 210.0, _format == 7u ? 279.4 : 297.0,   // (Letter, or A4)
                                                                                 (fude_zoom_v2){ _page->print_x, _page->print_y },
                                                                                 fude_zoom_page_pdf_words(FUDE_TEXT_ZOOM_PRINT_SQUARE), &_b, &_pages) :
                         _format == 5u ? fude_zoom_export_print_check(210.0, 279.4, fude_zoom_page_pdf_words(FUDE_TEXT_ZOOM_PRINT_CHECK_TITLE),
                                                                      fude_zoom_page_pdf_words(FUDE_TEXT_ZOOM_PRINT_CHECK_HOW), &_b) :   // (whole on A4 and on Letter)
                         _format == 6u ? fude_zoom_page_cutlist(_page, &_b) :
                         _format == 9u ? fude_zoom_page_areas_pdf(_page, &_b) :
                         _format == 10u ? fude_zoom_page_sheets_pdf(_page, &_b) :
                         _format == 11u ? fude_zoom_export_stl(&_page->scene, _half, fude_zoom_page_mm_per_point(_page), _page->stl_mm > 0.0 ? _page->stl_mm : 3.0, _page->app->info->name, &_b, &_stl) :
                         _format == 12u ? fude_zoom_page_parts_list(_page, &_b) :
                                         fude_zoom_export_dxf(&_page->scene, _half, fude_zoom_page_mm_per_point(_page), &_b, &_check);
        if(!_made && (_format == 4u || _format == 6u || _format == 7u || _format == 9u || _format == 10u || _format == 11u || _format == 12u)) {
            rde_arr_free(&_b);
            fude_notice_show(fude_text(_format == 6u ? FUDE_TEXT_ZOOM_CUTLIST_NONE : _format == 9u ? FUDE_TEXT_ZOOM_EXPORT_AREAS_NONE : _format == 12u ? FUDE_TEXT_ZOOM_PARTS_NONE :
                                       _format == 10u ? FUDE_TEXT_ZOOM_EXPORT_SHEETS_NONE : _format == 11u ? FUDE_TEXT_ZOOM_EXPORT_STL_NONE : FUDE_TEXT_ZOOM_EXPORT_TOO_BIG));
            return;
        }
        const b8 _wrote = _made && fude_bytes_write_and_free(&_b, _path, NULL);
        if(!_made) {
            rde_arr_free(&_b);
        }
        if(!_wrote || (_share && !rde_mobile_share_file(_path, _format == 11u ? "model/stl" : (_format == 12u ? "text/csv" : _types[_format < 8u ? _format : 2u]),
                                                        _page->app->info->name))) {
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_EXPORT_FAILED));
        } else if(_format == 5u) {
            fude_zoom_page_print_check_ask(_page);   // printed: its two lines' lengths
        } else if(_format == 11u) {
            c8 _line[300];
            FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_EXPORT_STL_MADE, FUDE_TN(_stl.solids));
            fude_notice_show(_line);
        } else if(_format == 3u && (_check.open_paths > 0 || _check.duplicates > 0 || _check.tiny_segments > 0)) {
            c8 _line[400];
            FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_EXPORT_CHECK, FUDE_TN(_check.open_paths), FUDE_TN(_check.duplicates), FUDE_TN(_check.tiny_segments));
            fude_notice_show(_line);
        } else if(!_share) {
            c8 _line[1200];
            FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_EXPORT_SAVED, FUDE_TS(_path));
            fude_notice_show(_line);
        }
        return;
    }
    rde_snprintf(_page->export_path, sizeof(_page->export_path), "%s", _path);
    _page->export_share = _share;
    _page->export_stage = 1u;
}

// The format being exported, kept while a computer's save dialog is open.
RDE_INTERNAL u8 fude_zoom_page_export_format = 0;

#if !defined(RDE_PLATFORM_MOBILE)
// The save dialog's answer: the export written where chosen, the format's extension added when the name lacks it.
RDE_INTERNAL void fude_zoom_page_on_export_path(const c8* const* _paths, u32 _count, i32 _filter, any _user_data) {
    RDE_UNUSED(_filter);
    fude_zoom_page* _page = (fude_zoom_page*)_user_data;
    fude_redraw_request();   // (render on demand: a dialog's answer may come while the app sleeps, with no event)
    if(_paths == NULL || _count == 0 || _page == NULL) {
        return;   // cancelled
    }
    static const c8* const _ext[13] = { ".png", ".svg", ".pdf", ".dxf", ".pdf", ".pdf", ".csv", ".pdf", ".mp4", ".pdf", ".pdf", ".stl", ".csv" };
    static const c8* const _EXT[13] = { ".PNG", ".SVG", ".PDF", ".DXF", ".PDF", ".PDF", ".CSV", ".PDF", ".MP4", ".PDF", ".PDF", ".STL", ".CSV" };
    const u8 _f = fude_zoom_page_export_format < 13u ? fude_zoom_page_export_format : 0u;
    c8 _path[1024];
    fude_session_with_extension(_path, sizeof(_path), _paths[0], _ext[_f], _EXT[_f]);
    fude_zoom_page_export_to(_page, fude_zoom_page_export_format, _path, false);
}
#endif

// Exports format _f where the user says: on a phone or tablet, into the outbox and then shared; on a computer, saved
// where the save dialog says.
RDE_INTERNAL void fude_zoom_page_export_go(fude_zoom_page* _page, u8 _f) {
    fude_app* _app = _page->app;
    fude_zoom_page_export_format = _f;
#if defined(RDE_PLATFORM_MOBILE)
    static const c8* const _ext[13] = { "png", "svg", "pdf", "dxf", "pdf", "pdf", "csv", "pdf", "mp4", "pdf", "pdf", "stl", "csv" };
    c8 _path[1024];
    fude_session_outbox_path(_path, sizeof(_path), _app->info->name, _ext[_f < 13u ? _f : 0u]);
    fude_zoom_page_export_to(_page, _f, _path, true);
#else
    static const rde_dialog_filter _filters[13] = { { "PNG", "png" }, { "SVG", "svg" }, { "PDF", "pdf" }, { "DXF", "dxf" }, { "PDF", "pdf" }, { "PDF", "pdf" }, { "CSV", "csv" }, { "PDF", "pdf" },
                                                    { "MP4", "mp4" }, { "PDF", "pdf" }, { "PDF", "pdf" }, { "STL", "stl" }, { "CSV", "csv" } };
    rde_dialog_save_file(_app->window, &_filters[_f < 13u ? _f : 0u], 1, NULL, fude_zoom_page_on_export_path, _page);
#endif
}

// 3D's thickness typed (what is not a board stands this tall): on to where it goes.
RDE_INTERNAL void fude_zoom_page_stl_typed(any _self, const c8* _text, FUDE_ZOOM_UNIT_ _unit, f64 _mm) {
    RDE_UNUSED(_text); RDE_UNUSED(_unit);
    fude_zoom_page* _page = (fude_zoom_page*)_self;
    if(!(_mm > 0.0)) {
        return;
    }
    _page->stl_mm = _mm;
    fude_zoom_page_export_go(_page, 11u);
}

// See page_parts.h. Nothing while the page is closed or an export is under way. The 3D model first asks how tall what
// is not a board stands. Returns false always.
b8 fude_zoom_export_choose(fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open || _page->export_stage != 0u) {
        return false;
    }
    // (Its choices: PNG, SVG, PDF, DXF, the template on A4, on Letter, the cut list, the zoom video, the areas, the
    // sheets, the 3D model, the parts list. Formats: 4 A4, 7 Letter, 6 the cut list, 8 the video, 9 the areas, 10 the
    // sheets, 11 the 3D model, 12 the parts list; 5 the printer's check, from Settings, asked for as 99.)
    static const u8 _formats[12] = { 0u, 1u, 2u, 3u, 4u, 7u, 6u, 8u, 9u, 10u, 11u, 12u };
    const u8 _f = _index == 99u ? 5u : (_index < 12u ? _formats[_index] : 0u);   // (99: the printer's check, from Settings)
    if(_f == 11u) {
        // How tall what is not a board stands (a board: its own thickness).
        c8 _text[48];
        fude_zoom_page_length_text(_page, _page->stl_mm > 0.0 ? _page->stl_mm : 3.0, _text, sizeof(_text));
        fude_zoom_numpad_open(&_page->numpad, _text, _page->units.unit, false, NULL, fude_zoom_page_stl_typed, _page);
        fude_zoom_numpad_ask(&_page->numpad, fude_text(FUDE_TEXT_ZOOM_EXPORT_STL_ASK));
        return false;
    }
    fude_zoom_page_export_go(_page, _f);
    return false;
}

// See page_parts.h. In the order fude_zoom_export_choose's _formats maps to formats.
const fude_extension_choice FUDE_ZOOM_EXPORT_CHOICES[] = {
    { FUDE_TEXT_ZOOM_EXPORT_PNG, FUDE_ICON_IMAGE,   fude_zoom_export_choose },
    { FUDE_TEXT_ZOOM_EXPORT_SVG, FUDE_ICON_PEN,     fude_zoom_export_choose },
    { FUDE_TEXT_ZOOM_EXPORT_PDF, FUDE_ICON_PDF,     fude_zoom_export_choose },
    { FUDE_TEXT_ZOOM_EXPORT_DXF, FUDE_ICON_MEASURE, fude_zoom_export_choose },
    { FUDE_TEXT_ZOOM_EXPORT_TILES, FUDE_ICON_PRINT, fude_zoom_export_choose },
    { FUDE_TEXT_ZOOM_EXPORT_TILES_LETTER, FUDE_ICON_PRINT, fude_zoom_export_choose },
    { FUDE_TEXT_ZOOM_EXPORT_CUTLIST, FUDE_ICON_LIST_CHECKS, fude_zoom_export_choose },
    { FUDE_TEXT_ZOOM_EXPORT_VIDEO, FUDE_ICON_VIDEO, fude_zoom_export_choose },
    { FUDE_TEXT_ZOOM_EXPORT_AREAS, FUDE_ICON_AREA,  fude_zoom_export_choose },   // (a page each, in Present's order: map.h)
    { FUDE_TEXT_ZOOM_EXPORT_SHEETS, FUDE_ICON_PAPER_SQUARES, fude_zoom_export_choose },   // (a page each, at its scale: sheet.h)
    { FUDE_TEXT_ZOOM_EXPORT_STL, FUDE_ICON_STACK, fude_zoom_export_choose },   // (closed outlines as solids, for a printer: stl.h)
    { FUDE_TEXT_ZOOM_EXPORT_PARTS, FUDE_ICON_LIST_CHECKS, fude_zoom_export_choose },   // (the parts counted: electronics', a plan's)
};
// A compile-time check that the table has its FUDE_ZOOM_OUT_COUNT rows (the size is -1, which does
// not compile, when not).
typedef char fude_zoom_export_choices_counted[(sizeof(FUDE_ZOOM_EXPORT_CHOICES) / sizeof(FUDE_ZOOM_EXPORT_CHOICES[0]) == FUDE_ZOOM_OUT_COUNT) ? 1 : -1];

RDE_INTERNAL void fude_zoom_page_video_draw(fude_zoom_page* _page, rde_window* _window, rde_camera* _camera);

// See page_parts.h: a notebook page's paper drawn into a picture of it (its cover, the strip's), its lines at their
// true spacing, as the scene's camera puts them (set to the picture's).
void fude_zoom_page_draw_sheet(fude_zoom_page* _page, u32 _i) {
    rde_arr* _lines = fude_zoom_page_paper_lines(_page);
    rde_arr_clear(_lines);
    fude_zoom_paper_sheet((u8)_page->app->canvas->page.paper, fude_zoom_pages_box(&_page->pages, _i), FUDE_ZOOM_PAGES_MARGIN, FUDE_ZOOM_PAGES_LINES,
                          FUDE_ZOOM_PAGES_GRID, FUDE_ZOOM_PAGE_STAFF, _lines);
    // (dots and the margin's line as the export draws them: a dot a short stroke, the margin a faint red)
    fude_zoom_paper_line* _l = (fude_zoom_paper_line*)_lines->memory;
    for(u32 _k = 0; _k < (u32)rde_arr_length(_lines); _k++) {
        if(_l[_k].kind == FUDE_ZOOM_PAPER_DOT) {
            _l[_k].b.x += 0.35;
        }
    }
    fude_zoom_page_draw_paper_lines(_lines, fude_zoom_page_frame_sim(_page, _page->scene.home), fude_theme_active()->page_dots);
}

// --- its cover (0.1.92) ----------------------------------------------------------------------------------

#define FUDE_ZOOM_PAGE_COVER_W    320.0   // a cover's pixels across (its height as its page's or the screen's shape)
#define FUDE_ZOOM_PAGE_COVER_WAIT 4.0     // seconds after a change, nothing drawn since, before it is drawn again

// See page.h.
void fude_zoom_page_cover_path(u32 _canvas, c8* _out, usize _size) {
    c8 _kana[RDE_MAX_PATH];
    fude_notes_canvas_path(_canvas, _kana, sizeof(_kana));
    c8* _dot = rde_strrchr(_kana, '.');
    if(_dot != NULL) {
        *_dot = 0;
    }
    rde_snprintf(_out, _size, "%s.cover.png", _kana);
}

// See page.h.
void fude_zoom_page_cover_now(fude_zoom_page* _page) {
    _page->cover_now = true;
}

// See page.h. Kept as _size + 1, so 0 means none (fude_zoom_page_open reads it).
void fude_zoom_page_notebook_next(fude_zoom_page* _page, u8 _size) {
    _page->notebook_next = (u8)(_size + 1u);
}

// The canvas's cover: drawn into a picture when asked for (fude_zoom_page_cover_now), or when the canvas has none or
// changed, once nothing has been drawn, rubbed out or flown for FUDE_ZOOM_PAGE_COVER_WAIT seconds; read back and
// written as <id>.cover.png the frame after. A notebook's cover is its first page, any other canvas's the view as it
// is. Never while an export is under way.
RDE_INTERNAL void fude_zoom_page_cover_draw(fude_zoom_page* _page, rde_window* _window, rde_camera* _camera) {
    if(!_page->open || _page->canvas == 0u || _page->export_stage != 0u) {
        return;
    }
    fude_zoom_scene* _s   = &_page->scene;
    const f64        _now = rde_engine_get_time_now();
    if(_page->cover_stage == 1u && _page->cover_target != NULL) {
        // (drawn the frame before: read back, kept)
        _page->cover_stage = 0u;
        u32 _w = 0, _h = 0;
        rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
        u8* _px = rde_render_texture_read_pixels(_page->cover_target, &_w, &_h, _heap);
        usize _size = 0;
        u8*   _png  = _px != NULL ? rde_image_encode_png(_px, _w, _h, &_size, _heap) : NULL;
        if(_png != NULL) {
            c8 _path[RDE_MAX_PATH];
            fude_zoom_page_cover_path(_page->canvas, _path, sizeof(_path));
            fude_bytes _b = fude_bytes_new((u32)_size + 16u);
            fude_put_data(&_b, _png, (u32)_size);
            if(fude_bytes_write_and_free(&_b, _path, NULL)) {
                _page->cover_stamp++;
                _page->cover_note = _page->canvas;
            }
        }
        if(_px != NULL) {
            _heap->free(_heap->allocator, _px);
        }
        if(_png != NULL) {
            _heap->free(_heap->allocator, _png);
        }
        return;
    }
    if(_page->cover_canvas != _page->canvas) {
        // (another canvas: its cover drawn soon if it has none)
        c8 _path[RDE_MAX_PATH];
        fude_zoom_page_cover_path(_page->canvas, _path, sizeof(_path));
        _page->cover_canvas     = _page->canvas;
        _page->cover_rev        = _s->revision;
        _page->cover_due        = !rde_file_exists(_path);
        _page->cover_changed_at = _now;
    }
    if(_s->revision != _page->cover_rev) {
        _page->cover_rev        = _s->revision;
        _page->cover_due        = true;
        _page->cover_changed_at = _now;
    }
    const b8 _busy = _page->drawing || _page->erasing || _page->flight.active;
    if(_page->cover_due && !_busy) {
        fude_redraw_after(FUDE_ZOOM_PAGE_COVER_WAIT - (_now - _page->cover_changed_at));   // (render on demand: drawn then, in a render)
    }
    if(!_page->cover_now && !(_page->cover_due && !_busy && _now - _page->cover_changed_at >= FUDE_ZOOM_PAGE_COVER_WAIT)) {
        return;
    }
    // Its framing: a notebook's first page, else the view.
    const fude_zoom_v2 _half_screen = fude_zoom_page_half(_page);
    const b8  _book = fude_zoom_page_paged(_page);
    const f64 _shape = _book ? _page->pages.h / rde_math_max_f64(_page->pages.w, 1e-9) : _half_screen.y / rde_math_max_f64(_half_screen.x, 1e-9);
    const u32 _pw = (u32)FUDE_ZOOM_PAGE_COVER_W, _ph = (u32)rde_math_min_f64(rde_math_max_f64(rde_math_round_f64(FUDE_ZOOM_PAGE_COVER_W * _shape), 120.0), 520.0);
    if(_page->cover_target != NULL) {
        rde_render_texture_unload(_page->cover_target);   // (a size of its own each time: a page's shape, the screen's)
        _page->cover_target = NULL;
    }
    _page->cover_target = rde_render_texture_load(_pw, _ph, NULL);
    if(_page->cover_target == NULL) {
        return;
    }
    const fude_zoom_camera _was  = _s->camera;
    const fude_zoom_v2     _half = { (f64)_pw * 0.5, (f64)_ph * 0.5 };
    const rde_color        _bg   = fude_theme_active()->page;
    if(_book) {
        _s->camera = fude_zoom_nav_framing(_half, _s->home, fude_zoom_pages_box(&_page->pages, 0u), 1.0);
    } else {
        _s->camera.z = _was.z * (f64)_pw / (2.0 * _half_screen.x);   // (the view as it is, smaller)
    }
    rde_render_texture_enable(_page->cover_target);
    rde_rendering_2d_begin_drawing(_window, _camera);
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)_pw + 4.0f, (f32)_ph + 4.0f }, _bg);
    if(_book) {
        fude_zoom_page_draw_sheet(_page, 0u);   // (its paper, as a page)
    }
    fude_zoom_pdfview_render(&_page->pdf, fude_zoom_page_frame_sim(_page, _s->home), _half);   // (a PDF's pages, under the ink)
    _page->renderer.no_guides = true;
    fude_zoom_render(&_page->renderer, _s, _half, _bg);
    _page->renderer.no_guides = false;
    rde_rendering_2d_end_drawing();
    rde_render_texture_disable();
    _s->camera = _was;
    _page->cover_due   = false;
    _page->cover_now   = false;
    _page->cover_stage = 1u;
    fude_redraw_after(0.0);   // (render on demand: read back in the next frame's render)
}

// See page.h. A PNG export's view is drawn larger than the screen (up to FUDE_ZOOM_PAGE_EXPORT_SCALE pixels a point,
// its longer side FUDE_ZOOM_PAGE_EXPORT_PX at most) by scaling the camera's zoom for the drawing only. The arrows'
// previews wait while a PNG or a video is being drawn.
void fude_zoom_page_render_offscreen(fude_zoom_page* _page, rde_window* _window, rde_camera* _camera) {
    if(_page->export_stage != 0u) {
        fude_redraw_changed();   // (render on demand: an export goes on frame by frame, its update's and its render's)
    }
    fude_zoom_page_cover_draw(_page, _window, _camera);   // (its cover, a while after it changed)
    fude_zoom_page_map_draw(_page, _window, _camera);   // (the map's picture, when it is wanted again)
    fude_zoom_page_strip_draw(_page, _window, _camera);   // (a page's picture for the strip, when one is wanted)
    if(_page->export_stage == 10u && _page->open) {
        fude_zoom_page_video_draw(_page, _window, _camera);   // (a zoom video's next frame)
        return;
    }
    if(_page->export_stage != 1u || !_page->open) {
        fude_zoom_page_previews_draw(_page, _window, _camera);   // (an arrow's preview, when one is wanted)
        return;
    }
    // The screen, larger: up to three pixels a point, its longer side 4096 at most.
    const fude_zoom_v2 _half  = fude_zoom_page_half(_page);
    const f64          _scale = rde_math_min_f64(FUDE_ZOOM_PAGE_EXPORT_SCALE, FUDE_ZOOM_PAGE_EXPORT_PX / (2.0 * rde_math_max_f64(_half.x, _half.y)));
    const u32          _w     = (u32)rde_math_max_f64(1.0, rde_math_floor_f64(_half.x * 2.0 * _scale));
    const u32          _h     = (u32)rde_math_max_f64(1.0, rde_math_floor_f64(_half.y * 2.0 * _scale));
    _page->export_target = rde_render_texture_load(_w, _h, NULL);
    if(_page->export_target == NULL) {
        _page->export_stage = 0;
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_EXPORT_FAILED));
        return;
    }
    fude_zoom_scene*       _s    = &_page->scene;
    const fude_zoom_camera _view = _s->camera;
    if(_page->export_held) {
        _s->camera = _page->export_view;   // (what the lasso holds, framed: the rest hidden)
        _page->renderer.lifted = &_page->export_hidden;
    }
    const f64        _z  = _s->camera.z;
    const rde_color  _bg = fude_theme_active()->page;
    rde_render_texture_enable(_page->export_target);
    rde_rendering_2d_begin_drawing(_window, _camera);
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)_w + 4.0f, (f32)_h + 4.0f }, _bg);
    _s->camera.z = _z * _scale;   // (drawn only: never settled like this)
    if(!_page->export_held) {
        fude_zoom_pdfview_render(&_page->pdf, fude_zoom_page_frame_sim(_page, _s->home), (fude_zoom_v2){ (f64)_w * 0.5, (f64)_h * 0.5 });   // (a PDF's pages, under the ink)
    }
    _page->renderer.no_guides = true;   // (a picture of the page: no guides)
    fude_zoom_render(&_page->renderer, _s, (fude_zoom_v2){ (f64)_w * 0.5, (f64)_h * 0.5 }, _bg);
    _page->renderer.no_guides = false;
    _s->camera.z = _z;
    rde_rendering_2d_end_drawing();
    rde_render_texture_disable();
    if(_page->export_held) {
        _s->camera             = _view;
        _page->renderer.lifted = &_page->selection.lifted;
        _page->export_held     = false;
    }
    _page->export_stage = 2u;
}

// See page_parts.h: the PNG's view drawn the frame before, read back, encoded, written, shared.
void fude_zoom_page_export_finish(fude_zoom_page* _page) {
    u32 _w = 0, _h = 0;
    u8* _px = rde_render_texture_read_pixels(_page->export_target, &_w, &_h, rde_memory_allocator_get_default_std());
    usize _size = 0;
    u8*   _png  = _px != NULL ? rde_image_encode_png(_px, _w, _h, &_size, rde_memory_allocator_get_default_std()) : NULL;
    b8    _ok   = false;
    if(_png != NULL) {
        fude_bytes _b = fude_bytes_new((u32)_size + 16u);
        fude_put_data(&_b, _png, (u32)_size);
        _ok = fude_bytes_write_and_free(&_b, _page->export_path, NULL);
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    if(_px != NULL) {
        _heap->free(_heap->allocator, _px);
    }
    if(_png != NULL) {
        _heap->free(_heap->allocator, _png);
    }
    rde_render_texture_unload(_page->export_target);
    _page->export_target = NULL;
    _page->export_stage  = 0;
    if(!_ok || (_page->export_share && !rde_mobile_share_file(_page->export_path, "image/png", _page->app->info->name))) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_EXPORT_FAILED));
    } else if(!_page->export_share) {
        c8 _line[1200];
        FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_EXPORT_SAVED, FUDE_TS(_page->export_path));
        fude_notice_show(_line);
    }
}

// --- the zoom video (video.h) ----------------------------------------------------------------------

#define FUDE_ZOOM_PAGE_VIDEO_FPS   30u      // its frames a second
#define FUDE_ZOOM_PAGE_VIDEO_LONG  1280.0   // its longer side, pixels
#define FUDE_ZOOM_PAGE_VIDEO_START 0.6      // seconds still on the whole canvas first
#define FUDE_ZOOM_PAGE_VIDEO_END   1.2      // and on the view at the end

// Returns what the canvas's own frame holds, in its units: where the video starts from. Bookmarks, layers' records,
// guides and what hidden layers hide are left out. Over a PDF, the page the view is on (a long document whole is a
// thin strip).
RDE_INTERNAL fude_zoom_box fude_zoom_page_home_contents(const fude_zoom_page* _page) {
    const fude_zoom_scene* _s    = &_page->scene;
    const u32              _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    if(fude_zoom_pdfview_is_open(&_page->pdf) && _page->pdf.page_count > 0u) {
        const fude_zoom_v2 _at = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _s->camera.frame, _home), _s->camera.at);
        return fude_zoom_pdfview_pages(&_page->pdf)[fude_zoom_pdfview_page_at(&_page->pdf, _at)].box;
    }
    fude_zoom_box _all = fude_zoom_box_empty();
    for(u32 _i = 0; _i < fude_zoom_scene_object_count(_s); _i++) {
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _i);
        if((_o->flags & FUDE_ZOOM_FLAG_ALIVE) && _o->frame == _home && _o->kind != FUDE_ZOOM_KIND_MARK && _o->kind != FUDE_ZOOM_KIND_LAYER &&
           !fude_zoom_scene_hides(_s, _o) && !(_o->kind == FUDE_ZOOM_KIND_SHAPE && _o->channels == FUDE_ZOOM_SHAPE_GUIDE)) {
            _all = fude_zoom_box_union(_all, _o->box);
        }
    }
    return _all;
}

// Returns the camera for frame _i: still at the start, then flown (its clock, the frame's time), still at the end. The
// flight is stepped on a copy, with the scene's camera put back after.
RDE_INTERNAL fude_zoom_camera fude_zoom_page_video_camera(fude_zoom_page* _page, u32 _i) {
    const f64 _t = (f64)_i / (f64)FUDE_ZOOM_PAGE_VIDEO_FPS - FUDE_ZOOM_PAGE_VIDEO_START;
    if(_t <= 0.0) {
        return _page->video_from;
    }
    if(_t >= _page->video_flight.length) {
        return _page->video_to;
    }
    fude_zoom_scene*       _s     = &_page->scene;
    const fude_zoom_camera _was   = _s->camera;
    fude_zoom_flight       _f     = _page->video_flight;   // (a copy: a step may land it)
    fude_zoom_fly_step(&_f, _s, fude_zoom_page_half(_page), _t);
    const fude_zoom_camera _there = _s->camera;
    _s->camera = _was;
    return _there;
}

// See page_parts.h. The video is closed without being finished.
void fude_zoom_page_video_drop(fude_zoom_page* _page) {
    if(_page->video != NULL) {
        fude_zoom_video_close(_page->video, false);
        _page->video = NULL;
    }
    if(_page->export_target != NULL && _page->export_stage >= 10u) {
        rde_render_texture_unload(_page->export_target);
        _page->export_target = NULL;
    }
    if(_page->export_stage >= 10u) {
        _page->export_stage = 0;
    }
}

// Export's Zoom video: from the whole canvas (its own frame's contents, framed) down to the view on screen, flown as
// Back flies but slower (longer the deeper: 3 to 30 seconds), still a moment at each end; at 30 frames a second, its
// longer side 1280 pixels, each side a multiple of 16. Nothing when the platform cannot make videos or the view is the
// whole canvas already.
RDE_INTERNAL void fude_zoom_page_video_begin(fude_zoom_page* _page, const c8* _path, b8 _share) {
    if(!fude_zoom_video_available()) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_EXPORT_FAILED));
        return;
    }
    fude_zoom_scene*    _s    = &_page->scene;
    const fude_zoom_v2  _half = fude_zoom_page_half(_page);
    const u32           _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    const fude_zoom_box _all  = fude_zoom_page_home_contents(_page);
    _page->video_from = fude_zoom_box_is_empty(_all) ? (fude_zoom_camera){ _home, { 0.0, 0.0 }, 1.0 } : fude_zoom_nav_framing(_half, _home, _all, 0.9);
    _page->video_to   = _s->camera;
    const fude_zoom_camera _was = _s->camera;
    _s->camera = _page->video_from;
    const b8 _flies = fude_zoom_fly_begin(&_page->video_flight, _s, _page->video_to, _half, 0.0);
    _s->camera = _was;
    if(!_flies) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_VIDEO_HERE));   // (the view is the whole canvas: nothing to fly)
        return;
    }
    _page->video_flight.began  = 0.0;
    _page->video_flight.length = rde_math_min_f64(rde_math_max_f64(2.0 + 0.4 * rde_math_abs_f64(_page->video_flight.l[1] - _page->video_flight.l[0]), 3.0), 30.0);
    _page->video_frames = (u32)rde_math_ceil_f64((FUDE_ZOOM_PAGE_VIDEO_START + _page->video_flight.length + FUDE_ZOOM_PAGE_VIDEO_END) * (f64)FUDE_ZOOM_PAGE_VIDEO_FPS);
    _page->video_frame  = 0;
    // Its size: the screen's shape, each side a multiple of 16.
    _page->video_scale = FUDE_ZOOM_PAGE_VIDEO_LONG / (2.0 * rde_math_max_f64(_half.x, _half.y));
    _page->video_w     = (u32)rde_math_max_f64(16.0, rde_math_floor_f64(_half.x * 2.0 * _page->video_scale / 16.0) * 16.0);
    _page->video_h     = (u32)rde_math_max_f64(16.0, rde_math_floor_f64(_half.y * 2.0 * _page->video_scale / 16.0) * 16.0);
    _page->video       = fude_zoom_video_open(_path, _page->video_w, _page->video_h, FUDE_ZOOM_PAGE_VIDEO_FPS);
    if(_page->video == NULL) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_EXPORT_FAILED));
        return;
    }
    rde_snprintf(_page->export_path, sizeof(_page->export_path), "%s", _path);
    _page->export_share = _share;
    _page->export_stage = 10u;
    c8 _line[200];
    FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_VIDEO_MAKING, FUDE_TN(0u));
    fude_notice_show(_line);
}

// Draws the video's next frame off screen, to be read back the frame after (fude_zoom_page_video_step). Its picture is
// made on the first frame and kept to the end.
RDE_INTERNAL void fude_zoom_page_video_draw(fude_zoom_page* _page, rde_window* _window, rde_camera* _camera) {
    if(_page->export_target == NULL) {
        _page->export_target = rde_render_texture_load(_page->video_w, _page->video_h, NULL);
        if(_page->export_target == NULL) {
            fude_zoom_page_video_drop(_page);
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_EXPORT_FAILED));
            return;
        }
    }
    fude_zoom_scene*       _s   = &_page->scene;
    const fude_zoom_camera _was = _s->camera;
    const rde_color        _bg  = fude_theme_active()->page;
    const fude_zoom_v2     _px  = { (f64)_page->video_w * 0.5, (f64)_page->video_h * 0.5 };
    _s->camera    = fude_zoom_page_video_camera(_page, _page->video_frame);
    _s->camera.z *= _page->video_scale;   // (drawn only: never settled like this)
    rde_render_texture_enable(_page->export_target);
    rde_rendering_2d_begin_drawing(_window, _camera);
    rde_rendering_2d_draw_rectangle((rde_vec_2F){ 0.0f, 0.0f }, (rde_vec_2F){ (f32)_page->video_w + 4.0f, (f32)_page->video_h + 4.0f }, _bg);
    fude_zoom_pdfview_render(&_page->pdf, fude_zoom_page_frame_sim(_page, _s->home), _px);   // (a PDF's pages, under the ink)
    _page->renderer.no_guides = true;   // (a picture of the page: no guides)
    fude_zoom_render(&_page->renderer, _s, _px, _bg);
    _page->renderer.no_guides = false;
    rde_rendering_2d_end_drawing();
    rde_render_texture_disable();
    _s->camera = _was;
    _page->export_stage = 11u;
}

// See page_parts.h: the frame drawn last frame read back, into the video; after the last, the video finished, shared
// or saved. A frame of the wrong size, or one the encoder refuses, ends it as failed. Progress is said every 15
// frames.
void fude_zoom_page_video_step(fude_zoom_page* _page) {
    u32 _w = 0, _h = 0;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    u8* _px = rde_render_texture_read_pixels(_page->export_target, &_w, &_h, _heap);
    const b8 _took = _px != NULL && _w == _page->video_w && _h == _page->video_h && fude_zoom_video_add(_page->video, _px, _w * 4u);
    if(_px != NULL) {
        _heap->free(_heap->allocator, _px);
    }
    if(!_took) {
        fude_zoom_page_video_drop(_page);
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_EXPORT_FAILED));
        return;
    }
    _page->video_frame++;
    if(_page->video_frame < _page->video_frames) {
        if(_page->video_frame % 15u == 0u) {
            c8 _line[200];
            FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_VIDEO_MAKING, FUDE_TN(_page->video_frame * 100u / _page->video_frames));
            fude_notice_show(_line);
        }
        _page->export_stage = 10u;
        return;
    }
    const b8 _ok = fude_zoom_video_close(_page->video, true);
    _page->video = NULL;
    fude_zoom_page_video_drop(_page);
    if(!_ok || (_page->export_share && !rde_mobile_share_file(_page->export_path, "video/mp4", _page->app->info->name))) {
        fude_notice_show(fude_text(FUDE_TEXT_ZOOM_EXPORT_FAILED));
    } else if(!_page->export_share) {
        c8 _line[1200];
        FUDE_TEXTF(_line, FUDE_TEXT_ZOOM_EXPORT_SAVED, FUDE_TS(_page->export_path));
        fude_notice_show(_line);
    }
}

// See page.h.
void fude_zoom_page_look_export(fude_zoom_page* _page, u8 _format, const c8* _path) {
    fude_zoom_page_export_to(_page, _format, _path, false);
}

// The toolbar's Smoothing: the level new strokes are drawn with (smooth.h); after the levels, Sculpt and Nudge, each a
// switch that turns the other off. Returns false always.
RDE_INTERNAL b8 fude_zoom_smoothing_choose(fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    if(fude_zoom_page_the != NULL && _index < FUDE_ZOOM_SMOOTH_COUNT) {
        fude_zoom_page_the->smooth_level  = (u8)_index;
        fude_zoom_page_the->smooth_chosen = true;
    } else if(fude_zoom_page_the != NULL && _index == FUDE_ZOOM_SMOOTH_COUNT) {
        // Sculpt: on or off (a switch beside the levels; Nudge off with it on).
        fude_zoom_page_the->sculpt = !fude_zoom_page_the->sculpt;
        fude_zoom_page_the->nudge  = fude_zoom_page_the->nudge && !fude_zoom_page_the->sculpt;
        fude_notice_show(fude_text(fude_zoom_page_the->sculpt ? FUDE_TEXT_ZOOM_SCULPT_ON : FUDE_TEXT_ZOOM_SCULPT_OFF));
    } else if(fude_zoom_page_the != NULL && _index == FUDE_ZOOM_SMOOTH_COUNT + 1u) {
        fude_zoom_page_the->nudge  = !fude_zoom_page_the->nudge;
        fude_zoom_page_the->sculpt = fude_zoom_page_the->sculpt && !fude_zoom_page_the->nudge;
        fude_notice_show(fude_text(fude_zoom_page_the->nudge ? FUDE_TEXT_ZOOM_NUDGE_ON : FUDE_TEXT_ZOOM_NUDGE_OFF));
    }
    return false;
}

// See page_parts.h.
b8 fude_zoom_smoothing_chosen(const fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    return fude_zoom_page_the != NULL && (_index == FUDE_ZOOM_SMOOTH_COUNT ? fude_zoom_page_the->sculpt :
                                          _index == FUDE_ZOOM_SMOOTH_COUNT + 1u ? fude_zoom_page_the->nudge : fude_zoom_page_the->smooth_level == _index);
}

// See page_parts.h.
b8 fude_zoom_smoothing_selected(const fude_app* _app) {
    RDE_UNUSED(_app);
    return false;   // a setting, not a tool in hand
}

// See page_parts.h: in FUDE_ZOOM_SMOOTH_ order, then Sculpt and Nudge.
const fude_extension_choice FUDE_ZOOM_SMOOTHING_CHOICES[] = {
    { FUDE_TEXT_ZOOM_SMOOTH_OFF,    FUDE_ICON_SCRIBBLE,      fude_zoom_smoothing_choose },
    { FUDE_TEXT_ZOOM_SMOOTH_LOW,    FUDE_ICON_SIGNAL_LOW,    fude_zoom_smoothing_choose },
    { FUDE_TEXT_ZOOM_SMOOTH_MEDIUM, FUDE_ICON_SIGNAL_MEDIUM, fude_zoom_smoothing_choose },
    { FUDE_TEXT_ZOOM_SMOOTH_HIGH,   FUDE_ICON_SIGNAL_HIGH,   fude_zoom_smoothing_choose },
    { FUDE_TEXT_ZOOM_SMOOTH_ROPE,   FUDE_ICON_GUIDED,        fude_zoom_smoothing_choose },
    { FUDE_TEXT_ZOOM_SCULPT,        FUDE_ICON_PEN_NIB,       fude_zoom_smoothing_choose },
    { FUDE_TEXT_ZOOM_NUDGE,         FUDE_ICON_MAGNET,        fude_zoom_smoothing_choose },
};
// A compile-time check that the table has its FUDE_ZOOM_SMOOTHING_CHOICES_N rows (the size is -1, which does
// not compile, when not).
typedef char fude_zoom_smoothing_choices_counted[(sizeof(FUDE_ZOOM_SMOOTHING_CHOICES) / sizeof(FUDE_ZOOM_SMOOTHING_CHOICES[0]) == FUDE_ZOOM_SMOOTHING_CHOICES_N) ? 1 : -1];

// See page_parts.h. The units chosen, as units.h writes them: millimetres to a tenth, centimetres to a hundredth,
// inches and feet-inches to a sixteenth. An unknown choice is millimetres.
void fude_zoom_page_set_units(fude_zoom_page* _page, u8 _choice) {
    static const fude_zoom_units_style _styles[4] = {
        { FUDE_ZOOM_UNIT_MM, 1u, 16u, false }, { FUDE_ZOOM_UNIT_CM, 2u, 16u, false },
        { FUDE_ZOOM_UNIT_IN, 3u, 16u, false }, { FUDE_ZOOM_UNIT_IN, 3u, 16u, true },
    };
    _page->units_choice = _choice < 4u ? _choice : 0u;
    _page->units        = _styles[_page->units_choice];
}

// See page.h. The smoothing is stored as its level + 1. Nothing while there is no page.
void fude_zoom_page_settings_gather(const fude_app* _app, fude_settings* _settings) {
    RDE_UNUSED(_app);
    if(fude_zoom_page_the != NULL) {
        _settings->smoothing         = (u8)(fude_zoom_page_the->smooth_level + 1u);
        _settings->units             = fude_zoom_page_the->units_choice;
        _settings->true_mm_per_point = fude_zoom_page_the->true_mm_per_point;
        _settings->print_x           = fude_zoom_page_the->print_x;
        _settings->print_y           = fude_zoom_page_the->print_y;
        _settings->kerf_mm           = fude_zoom_page_the->kerf_mm;
        _settings->show_fps          = fude_zoom_page_the->show_fps;
        _settings->keep_grain        = fude_zoom_page_the->keep_grain;
        _settings->smooth_chosen     = fude_zoom_page_the->smooth_chosen;
    }
}

// See page.h. The frame rate shown is taken only in a developer's build.
void fude_zoom_page_settings_apply(fude_app* _app, const fude_settings* _settings) {
    RDE_UNUSED(_app);
    if(fude_zoom_page_the == NULL) {
        return;
    }
    // (the smoothing as chosen; never chosen by hand, it is the default, off, since a file from before stored the old
    // default all the same)
    fude_zoom_page_the->smooth_chosen = _settings->smooth_chosen;
    if(_settings->smooth_chosen && _settings->smoothing >= 1u && _settings->smoothing <= FUDE_ZOOM_SMOOTH_COUNT) {
        fude_zoom_page_the->smooth_level = (u8)(_settings->smoothing - 1u);
    } else {
        fude_zoom_page_the->smooth_level = FUDE_ZOOM_SMOOTH_DEFAULT;
    }
    fude_zoom_page_set_units(fude_zoom_page_the, _settings->units);
    fude_zoom_page_the->true_mm_per_point = _settings->true_mm_per_point;
    fude_zoom_page_the->print_x           = _settings->print_x;
    fude_zoom_page_the->print_y           = _settings->print_y;
    fude_zoom_page_the->kerf_mm           = _settings->kerf_mm;
    fude_zoom_page_the->show_fps          = _settings->show_fps;   // (Settings' Display)
    fude_zoom_page_the->keep_grain        = _settings->keep_grain;
}
