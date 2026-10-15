// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/pdf.h"
#include "zoom/mathfont.h"
#include "drawing/base/utf8.h"

#include <stdarg.h>

// ===========================================================================
// See pdf.h. The pages' content streams and the images' samples are kept as
// they come, each in one buffer; the objects are only numbered and written at
// the end, when everything they refer to is known:
//
//   1 the catalog   2 the page tree   3 the resources (one dictionary every
//   page shares: all the alpha states, images and fonts)   then Helvetica's
//   faces used, each embedded font (five objects: the Type 0 font, its
//   CIDFont, its descriptor, its program and its ToUnicode map), the alpha
//   states, the images (each followed by its mask) and, last, each page with
//   its content stream.
//
// Text in the fonts set is written as each font's codes (Identity-H: two bytes,
// the CID, which is the glyph id but in a CID-keyed CFF), and each document
// keeps, for each font, the first character each glyph was written for: what
// is embedded at the end (the glyphs used) and its ToUnicode map come from it.
// The resources name Helvetica's faces /F1 to /F4 and the embedded fonts /E1
// on, by their place in the font set.
// ===========================================================================

#define FUDE_ZOOM_PDF_LIMIT  0x7FFFFFFFu   // content and samples together, bytes: the file must fit a fude_bytes
#define FUDE_ZOOM_PDF_DASHES 16u           // lengths in a dash pattern, at most
#define FUDE_ZOOM_PDF_SIDE   3.0           // the smallest page side PDF allows, points
#define FUDE_ZOOM_PDF_PLACES 4u            // decimals for coordinates and lengths
#define FUDE_ZOOM_PDF_SCALE  6u            // decimals for a matrix's scale and turn (small ones must not round away)
#define FUDE_ZOOM_PDF_COLOR  3u            // decimals for a colour channel: enough to give back its byte
#define FUDE_ZOOM_PDF_GLYPH  2u            // decimals for a glyph's width, in thousandths of an em

// The font files a font set holds at most: every face's chain all different files (most are the same files).
#define FUDE_ZOOM_PDF_FONTS  (FUDE_ZOOM_PDF_FACES * FUDE_ZOOM_PDF_CHAIN)
#define FUDE_ZOOM_PDF_NONE   0xFFFFFFFFu   // no font: a text's run not begun yet
#define FUDE_ZOOM_PDF_NOTDEF 0xFFFDu       // what the missing-glyph box copies as: U+FFFD, the replacement character
#define FUDE_ZOOM_PDF_PARTS  64u           // a composite glyph's parts read, at most (mathfont.c reads no more)
#define FUDE_ZOOM_PDF_HEAD   54u           // a TrueType head table's length: a font with a shorter one is not embedded
#define FUDE_ZOOM_PDF_SHORT  0x1FFFEu      // a glyf this long at most has a short loca (offsets halved into 16 bits)
#define FUDE_ZOOM_PDF_SUM    0xB1B0AFBAu   // what a TrueType file's words add up to, its head's adjustment included
#define FUDE_ZOOM_PDF_BFCHAR 100u          // a ToUnicode CMap's bfchar block, at most (PostScript's limit)

// OpenType table tags, as the table directory has them (four characters, big-endian).
#define FUDE_ZOOM_PDF_TAG_CFF  0x43464620u   // 'CFF '
#define FUDE_ZOOM_PDF_TAG_HEAD 0x68656164u   // 'head'
#define FUDE_ZOOM_PDF_TAG_GLYF 0x676C7966u   // 'glyf'
#define FUDE_ZOOM_PDF_TAG_LOCA 0x6C6F6361u   // 'loca'
#define FUDE_ZOOM_PDF_TAG_POST 0x706F7374u   // 'post'
#define FUDE_ZOOM_PDF_TAG_OS2  0x4F532F32u   // 'OS/2'

// The tables a cut-down TrueType font keeps, in the order a font file lists them (by tag): what a reader draws its
// glyphs with (glyf, head, hhea, hmtx, loca, maxp, and the hinting: cvt, fpgm, prep) and what some readers look for
// besides (OS/2, cmap, name, post). glyf and loca are rebuilt, head copied with its loca format and adjustment set,
// the rest copied as they are; any other table (the shaping ones, GSUB and GPOS, the biggest) is left out.
static const u32 fude_zoom_pdf_kept[13] = {
    0x4F532F32u, 0x636D6170u, 0x63767420u, 0x6670676Du, 0x676C7966u, 0x68656164u, 0x68686561u,   // OS/2 cmap cvt fpgm glyf head hhea
    0x686D7478u, 0x6C6F6361u, 0x6D617870u, 0x6E616D65u, 0x706F7374u, 0x70726570u,                // hmtx loca maxp name post prep
};

// A page until the file is written: its size and where its content stream is.
typedef struct {
    f64 width;   // points
    f64 height;
    u32 at;      // its content stream, in the document's content
    u32 size;    // its length, bytes
} fude_zoom_pdf_page;

// An image until the file is written: its size, its colours and where its bytes are.
typedef struct {
    u32 width;       // pixels
    u32 height;
    u32 at;          // its samples (or the JPEG's bytes), in the document's data
    u32 size;        // their length, bytes
    u32 mask_at;     // its alpha; size 0: opaque, no mask
    u32 mask_size;
    u8  components;  // 1 gray, 3 RGB, 4 CMYK
    b8  jpeg;        // its bytes a JPEG's, written as they are (DCTDecode); else RGB samples
    b8  inverted;    // an Adobe CMYK JPEG: its ink stored the other way round
} fude_zoom_pdf_picture;

// What the page's stream has set so far, so nothing is said twice.
typedef struct {
    rde_color fill;     // the fill's colour and alpha
    rde_color stroke;   // the stroke's
    f64       width;    // the line width
} fude_zoom_pdf_state;

// A font file of a font set: its path, and once it is read, its bytes and tables.
typedef struct {
    rde_arr TYPE(c8)  path;    // NUL-ended
    u8*               bytes;   // fude_file_read's (the std heap); NULL until read, or when it could not be
    fude_zoom_mfont   font;    // its tables (size 0 until read, or when it is left out)
    rde_arr TYPE(u16) cids;    // a CID-keyed CFF's CID for each glyph (mathfont.h); empty when a code is the glyph id
    b8                tried;   // reading it was tried
} fude_zoom_pdf_font;

// The fonts set (fude_zoom_pdf_fonts): the module's while they are set, and each document's made with them, freed when
// neither holds them any more. Its fixed arrays are bounded by the design: FUDE_ZOOM_PDF_FACES chains of
// FUDE_ZOOM_PDF_CHAIN files at most.
typedef struct {
    fude_zoom_pdf_font fonts[FUDE_ZOOM_PDF_FONTS];   // the distinct files, in the order the chains first name them
    u32                count;
    u8                 chains[FUDE_ZOOM_PDF_FACES][FUDE_ZOOM_PDF_CHAIN];   // each face's files, as index + 1; 0 ends
    u32                docs;      // documents made with these fonts and not yet freed
    b8                 current;   // still the fonts set (else freed with the last of its documents)
} fude_zoom_pdf_fontset;

// A glyph of an embedded font, as the file names it: its code (its CID), its glyph id, and the character it copies as.
typedef struct {
    u32 code;
    u32 glyph;
    u32 unicode;
} fude_zoom_pdf_glyph;

// An embedded font while the file is finished: its glyphs used (by code), its program and its ToUnicode map.
typedef struct {
    rde_arr TYPE(fude_zoom_pdf_glyph) glyphs;
    fude_bytes                        program;   // a TrueType font cut down, or a CFF table as it is
    fude_bytes                        unicode;   // the ToUnicode CMap
    c8                                name[80];  // its /BaseFont: a subset tag, '+', the file's name (a PDF name: no delimiters)
} fude_zoom_pdf_embed;

// The document, as pdf.h's drawing builds it.
struct fude_zoom_pdf {
    fude_bytes                          content;   // every page's content stream, one after another
    fude_bytes                          data;      // every image's bytes, one after another
    rde_arr TYPE(fude_zoom_pdf_page)    pages;
    rde_arr TYPE(fude_zoom_pdf_picture) images;
    rde_arr TYPE(fude_zoom_pdf_state)   saved;     // the states each open q put away
    fude_zoom_pdf_state                 state;     // what the open page's stream has set now
    b8                                  open;      // inside a page
    b8                                  text;      // Helvetica is used
    b8                                  faces[4];  // which of its faces are used (fude_zoom_pdf_text_face)
    b8                                  full;      // outgrew the limit: finish fails
    b8                                  fill_alpha[256];     // the alphas fills have used (each an ExtGState)
    b8                                  stroke_alpha[256];   // the alphas strokes have used
    fude_zoom_pdf_fontset*              fonts;     // the fonts set when it was made (NULL: none, Helvetica)
    // For each font of its set: each glyph's first character written with it (0: not used; the missing glyph's
    // FUDE_ZOOM_PDF_NOTDEF), a u32 a glyph. Made when the font is first written in: the fonts embedded are those made.
    rde_arr TYPE(u32)                   used[FUDE_ZOOM_PDF_FONTS];
};

// The fonts set now (fude_zoom_pdf_fonts), NULL when none: what a new document takes. Main thread only.
RDE_INTERNAL fude_zoom_pdf_fontset* fude_zoom_pdf_fonts_now = NULL;

// Helvetica's advance widths (thousandths of the size) by WinAnsi code, from
// its standard AFM; 0 where WinAnsi has no character (never written).
static const u16 fude_zoom_pdf_helvetica[256] = {
       0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     278,  278,  355,  556,  556,  889,  667,  191,  333,  333,  389,  584,  278,  333,  278,  278,
     556,  556,  556,  556,  556,  556,  556,  556,  556,  556,  278,  278,  584,  584,  584,  556,
    1015,  667,  667,  722,  722,  667,  611,  778,  722,  278,  500,  667,  556,  833,  722,  778,
     667,  778,  722,  667,  611,  722,  667,  944,  667,  667,  611,  278,  278,  278,  469,  556,
     333,  556,  556,  500,  556,  556,  278,  556,  556,  222,  222,  500,  222,  833,  556,  556,
     556,  556,  333,  500,  278,  556,  500,  722,  500,  500,  500,  334,  260,  334,  584,    0,
     556,    0,  222,  556,  333, 1000,  556,  556,  333, 1000,  667,  333, 1000,    0,  611,    0,
       0,  222,  222,  333,  333,  350,  556, 1000,  333, 1000,  500,  333,  944,    0,  500,  667,
     278,  333,  556,  556,  556,  556,  260,  556,  333,  737,  370,  556,  584,  333,  737,  333,
     400,  584,  333,  333,  333,  556,  537,  278,  333,  333,  365,  556,  834,  834,  834,  611,
     667,  667,  667,  667,  667,  667, 1000,  722,  667,  667,  667,  667,  278,  278,  278,  278,
     722,  722,  778,  778,  778,  778,  778,  584,  778,  722,  722,  722,  722,  667,  667,  611,
     556,  556,  556,  556,  556,  556,  889,  500,  556,  556,  556,  556,  278,  278,  278,  278,
     556,  556,  556,  556,  556,  556,  556,  584,  611,  556,  556,  556,  556,  500,  556,  500,
};

// What WinAnsi puts at 0x80 to 0x9F, in place of Latin-1's control codes.
static const u16 fude_zoom_pdf_winansi_extra[32] = {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
    0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178,
};

// PDF's state at the start of every page: black, opaque, line width 1.
static const fude_zoom_pdf_state fude_zoom_pdf_default = { { 0, 0, 0, 255 }, { 0, 0, 0, 255 }, 1.0 };

// --- numbers and bytes -----------------------------------------------------------------------------

// A real as PDF writes it: at most _decimals places, its trailing zeros gone,
// never an exponent (PDF has none), -0 as 0, NaN and the infinities as 0, kept
// within a billion either way. Into _out (24 bytes at least); its length.
RDE_INTERNAL u32 fude_zoom_pdf_real(f64 _v, u32 _decimals, c8* _out) {
    static const f64 _scales[] = { 1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0 };
    if(_decimals > 6u) {
        _decimals = 6u;
    }
    if(!rde_math_is_finite_f64(_v)) {
        _v = 0.0;
    }
    _v = rde_math_min_f64(rde_math_max_f64(_v, -1e9), 1e9);
    const i64 _n = rde_math_round_to_i64_f64(_v * _scales[_decimals]);
    u64       _m = (u64)(_n < 0 ? -_n : _n);
    // Its digits, the last first, at least one before the point.
    c8  _digits[24];
    u32 _k = 0;
    do {
        _digits[_k++] = (c8)('0' + (int)(_m % 10u));
        _m /= 10u;
    } while(_m > 0u || _k <= _decimals);
    u32 _zeros = 0;
    while(_zeros < _decimals && _digits[_zeros] == '0') {
        _zeros++;
    }
    u32 _len = 0;
    if(_n < 0) {
        _out[_len++] = '-';
    }
    for(u32 _i = _k; _i > _decimals; _i--) {
        _out[_len++] = _digits[_i - 1u];
    }
    if(_zeros < _decimals) {
        _out[_len++] = '.';
        for(u32 _i = _decimals; _i > _zeros; _i--) {
            _out[_len++] = _digits[_i - 1u];
        }
    }
    return _len;
}

// Can _n more bytes be kept? Once not, the document is spoilt (finish says so).
RDE_INTERNAL b8 fude_zoom_pdf_room(fude_zoom_pdf* _pdf, u64 _n) {
    if(_pdf->full || (u64)fude_bytes_size(&_pdf->content) + (u64)fude_bytes_size(&_pdf->data) + _n > FUDE_ZOOM_PDF_LIMIT) {
        _pdf->full = true;
        return false;
    }
    return true;
}

// Bytes onto the open page's content stream.
RDE_INTERNAL void fude_zoom_pdf_emit(fude_zoom_pdf* _pdf, const c8* _s, u32 _n) {
    if(_n > 0u && fude_zoom_pdf_room(_pdf, _n)) {
        fude_put_data(&_pdf->content, _s, _n);
    }
}

// A C string onto the open page's content stream.
RDE_INTERNAL void fude_zoom_pdf_emit_str(fude_zoom_pdf* _pdf, const c8* _s) {
    fude_zoom_pdf_emit(_pdf, _s, (u32)rde_strlen(_s));
}

// _n numbers (six at most), then the operator, one to a line.
RDE_INTERNAL void fude_zoom_pdf_op(fude_zoom_pdf* _pdf, const f64* _v, u32 _n, u32 _decimals, const c8* _op) {
    c8  _buf[192];
    u32 _len = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        _len += fude_zoom_pdf_real(_v[_i], _decimals, _buf + _len);
        _buf[_len++] = ' ';
    }
    const u32 _k = (u32)rde_strlen(_op);
    rde_memcpy(_buf + _len, _op, _k);
    _len += _k;
    _buf[_len++] = '\n';
    fude_zoom_pdf_emit(_pdf, _buf, _len);
}

// Formatted text into the file being finished (names and integers only: no
// reals through printf, whose decimal point follows the locale).
RDE_INTERNAL void fude_zoom_pdf_put(fude_bytes* _out, const c8* _fmt, ...) {
    c8      _buf[256];
    va_list _args;
    va_start(_args, _fmt);
    const int _n = rde_vsprintf(_buf, sizeof _buf, _fmt, _args);
    va_end(_args);
    if(_n > 0) {
        fude_put_data(_out, _buf, (u32)((usize)_n < sizeof _buf ? (usize)_n : sizeof _buf - 1u));
    }
}

// A real onto the file being finished, as fude_zoom_pdf_real writes it.
RDE_INTERNAL void fude_zoom_pdf_put_real(fude_bytes* _out, f64 _v, u32 _decimals) {
    c8 _buf[24];
    fude_put_data(_out, _buf, fude_zoom_pdf_real(_v, _decimals, _buf));
}

// --- the fonts -------------------------------------------------------------------------------------

// Lets go of every file set _s has read, keeping their paths: each is read again when next asked for.
RDE_INTERNAL void fude_zoom_pdf_fonts_unload(fude_zoom_pdf_fontset* _s) {
    for(u32 _k = 0; _k < _s->count; _k++) {
        fude_zoom_pdf_font* _f = &_s->fonts[_k];
        fude_file_free(_f->bytes);
        _f->bytes = NULL;
        rde_memset(&_f->font, 0, sizeof(_f->font));
        rde_arr_clear(&_f->cids);
        _f->tried = false;
    }
}

// Frees set _s: the files read, their paths, and the set (rde_calloc's).
RDE_INTERNAL void fude_zoom_pdf_fonts_free(fude_zoom_pdf_fontset* _s) {
    fude_zoom_pdf_fonts_unload(_s);
    for(u32 _k = 0; _k < _s->count; _k++) {
        rde_arr_free(&_s->fonts[_k].path);
        rde_arr_free(&_s->fonts[_k].cids);
    }
    rde_free(_s);
}

// See the header. The set before is freed now unless a document still holds it (then with the last of them); the new
// one holds each distinct path once, its chains naming them by place.
void fude_zoom_pdf_fonts(const fude_zoom_pdf_font_files* _files) {
    fude_zoom_pdf_fontset* _was = fude_zoom_pdf_fonts_now;
    fude_zoom_pdf_fonts_now = NULL;
    if(_was != NULL) {
        _was->current = false;
        if(_was->docs == 0u) {
            fude_zoom_pdf_fonts_free(_was);
        }
    }
    if(_files == NULL) {
        return;
    }
    fude_zoom_pdf_fontset* _s = (fude_zoom_pdf_fontset*)rde_calloc(1u, sizeof(fude_zoom_pdf_fontset));
    if(_s == NULL) {
        rde_log_level(RDE_LOG_LEVEL_WARNING, "PDF: no memory for the fonts: text is written in Helvetica");
        return;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _s->current = true;
    for(u32 _face = 0; _face < FUDE_ZOOM_PDF_FACES; _face++) {
        for(u32 _i = 0; _i < FUDE_ZOOM_PDF_CHAIN; _i++) {
            const c8* _path = _files->chains[_face][_i];
            if(_path == NULL || _path[0] == 0) {
                break;
            }
            u32 _k = 0;
            while(_k < _s->count && rde_strcmp((const c8*)_s->fonts[_k].path.memory, _path) != 0) {
                _k++;
            }
            if(_k == _s->count) {   // (a new file: at most FUDE_ZOOM_PDF_FONTS, one a slot)
                fude_zoom_pdf_font* _f   = &_s->fonts[_s->count++];
                const u32           _len = (u32)rde_strlen(_path);
                _f->path = rde_arr_new(sizeof(c8), _heap);
                rde_arr_resize(&_f->path, (usize)_len + 1u);   // (zeroed: NUL-ended)
                rde_memcpy(_f->path.memory, _path, _len);
                _f->cids = rde_arr_new(sizeof(u16), _heap);
            }
            _s->chains[_face][_i] = (u8)(_k + 1u);
        }
    }
    fude_zoom_pdf_fonts_now = _s;
}

// Font _k of set _s, read the first time it is asked for: its tables, or NULL when it is left out (it cannot be read,
// is not OpenType with TrueType or CFF outlines, has a head too short to set, or is a CFF too big to embed whole).
RDE_INTERNAL const fude_zoom_mfont* fude_zoom_pdf_font_get(fude_zoom_pdf_fontset* _s, u32 _k) {
    fude_zoom_pdf_font* _f = &_s->fonts[_k];
    if(!_f->tried) {
        _f->tried = true;
        u32 _size = 0, _table = 0;
        _f->bytes = fude_file_read((const c8*)_f->path.memory, &_size);
        b8 _ok = _f->bytes != NULL && fude_zoom_mfont_load(&_f->font, _f->bytes, _size);
        if(_ok && _f->font.cff != 0u) {
            _ok = fude_zoom_mfont_table(&_f->font, FUDE_ZOOM_PDF_TAG_CFF, &_table) != 0u && _table <= FUDE_ZOOM_PDF_CFF_MOST;
            if(_ok) {
                fude_zoom_mfont_cids(&_f->font, &_f->cids);   // (false: not CID-keyed, its glyphs named by their ids)
            }
        } else if(_ok) {
            _ok = fude_zoom_mfont_table(&_f->font, FUDE_ZOOM_PDF_TAG_HEAD, &_table) != 0u && _table >= FUDE_ZOOM_PDF_HEAD;
        }
        if(!_ok) {
            rde_log_level(RDE_LOG_LEVEL_WARNING, "PDF: the font %s is left out (%s): its characters go to the next font", (const c8*)_f->path.memory,
                          _f->bytes == NULL ? "it cannot be read" : "it is not a font this writer embeds");
            fude_file_free(_f->bytes);
            _f->bytes = NULL;
            rde_memset(&_f->font, 0, sizeof(_f->font));
            rde_arr_clear(&_f->cids);
        }
    }
    return _f->font.size != 0u ? &_f->font : NULL;
}

// The chain face _face is written in with set _s (its fonts as their place + 1, a 0 ending them), or NULL for
// Helvetica: the face's own when its first font is there, else the regular face's when that one's is.
RDE_INTERNAL const u8* fude_zoom_pdf_chain(fude_zoom_pdf_fontset* _s, u8 _face) {
    if(_s == NULL) {
        return NULL;
    }
    const u8 _tries[2] = { _face < FUDE_ZOOM_PDF_FACES ? _face : 0u, 0u };
    for(u32 _i = 0; _i < 2u; _i++) {
        const u8* _chain = _s->chains[_tries[_i]];
        if(_chain[0] != 0u && fude_zoom_pdf_font_get(_s, _chain[0] - 1u) != NULL) {
            return _chain;
        }
    }
    return NULL;
}

// Whether code point _cp is one the screen draws as nothing: Unicode's default-ignorable code points (format controls
// such as a zero-width space or joiner, variation selectors, fillers, tags), which HarfBuzz hides.
RDE_INTERNAL b8 fude_zoom_pdf_ignorable(u32 _cp) {
    return _cp == 0x00ADu || _cp == 0x034Fu || _cp == 0x061Cu || (_cp >= 0x115Fu && _cp <= 0x1160u) || (_cp >= 0x17B4u && _cp <= 0x17B5u) ||
           (_cp >= 0x180Bu && _cp <= 0x180Fu) || (_cp >= 0x200Bu && _cp <= 0x200Fu) || (_cp >= 0x202Au && _cp <= 0x202Eu) ||
           (_cp >= 0x2060u && _cp <= 0x206Fu) || _cp == 0x3164u || (_cp >= 0xFE00u && _cp <= 0xFE0Fu) || _cp == 0xFEFFu || _cp == 0xFFA0u ||
           (_cp >= 0xFFF0u && _cp <= 0xFFF8u) || (_cp >= 0x1BCA0u && _cp <= 0x1BCA3u) || (_cp >= 0x1D173u && _cp <= 0x1D17Au) ||
           (_cp >= 0xE0000u && _cp <= 0xE0FFFu);
}

// Finds code point _cp in chain _chain of set _s: the first of its fonts with a glyph for it (the font's place into
// *_font, the glyph into *_glyph). When none has one: the chain's first font and its glyph 0, the missing-glyph box the
// screen draws then. A glyph id past the font's glyphs (a damaged character map) counts as none.
RDE_INTERNAL void fude_zoom_pdf_find(fude_zoom_pdf_fontset* _s, const u8* _chain, u32 _cp, u32* _font, u32* _glyph) {
    *_font  = _chain[0] - 1u;
    *_glyph = 0u;
    for(u32 _i = 0; _i < FUDE_ZOOM_PDF_CHAIN && _chain[_i] != 0u; _i++) {
        const fude_zoom_mfont* _f = fude_zoom_pdf_font_get(_s, _chain[_i] - 1u);
        const u32              _g = _f != NULL ? fude_zoom_mfont_glyph(_f, _cp) : 0u;
        if(_f != NULL && _g != 0u && _g < _f->glyphs) {
            *_font  = _chain[_i] - 1u;
            *_glyph = _g;
            return;
        }
    }
}

// The code font _f's glyph _g is written as: its CID in a CID-keyed CFF, else its glyph id.
RDE_INTERNAL u32 fude_zoom_pdf_code(const fude_zoom_pdf_font* _f, u32 _g) {
    return _g < (u32)rde_arr_length(&_f->cids) ? (u32)((const u16*)_f->cids.memory)[_g] : _g;
}

// --- the document ----------------------------------------------------------------------------------

// See the header. It takes the fonts set now, if any.
fude_zoom_pdf* fude_zoom_pdf_new(void) {
    fude_zoom_pdf* _pdf = (fude_zoom_pdf*)rde_calloc(1u, sizeof(fude_zoom_pdf));
    if(_pdf == NULL) {
        return NULL;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _pdf->content = fude_bytes_new(4096u);
    _pdf->data    = fude_bytes_new(16u);
    _pdf->pages   = rde_arr_new(sizeof(fude_zoom_pdf_page), _heap);
    _pdf->images  = rde_arr_new(sizeof(fude_zoom_pdf_picture), _heap);
    _pdf->saved   = rde_arr_new(sizeof(fude_zoom_pdf_state), _heap);
    _pdf->state   = fude_zoom_pdf_default;
    _pdf->fonts   = fude_zoom_pdf_fonts_now;
    if(_pdf->fonts != NULL) {
        _pdf->fonts->docs++;
    }
    return _pdf;
}

// See the header. Its fonts are let go when it was the last document holding them: freed when they are no longer the
// fonts set, else their files unloaded (read again when next needed).
void fude_zoom_pdf_free(fude_zoom_pdf* _pdf) {
    if(_pdf == NULL) {
        return;
    }
    rde_arr_free(&_pdf->content);
    rde_arr_free(&_pdf->data);
    rde_arr_free(&_pdf->pages);
    rde_arr_free(&_pdf->images);
    rde_arr_free(&_pdf->saved);
    for(u32 _k = 0; _k < FUDE_ZOOM_PDF_FONTS; _k++) {
        if(rde_arr_is_inited(&_pdf->used[_k])) {
            rde_arr_free(&_pdf->used[_k]);
        }
    }
    fude_zoom_pdf_fontset* _s = _pdf->fonts;
    if(_s != NULL && --_s->docs == 0u) {
        if(_s->current) {
            fude_zoom_pdf_fonts_unload(_s);
        } else {
            fude_zoom_pdf_fonts_free(_s);
        }
    }
    rde_free(_pdf);
}

// --- pages -----------------------------------------------------------------------------------------

// See the header. A page's content stream goes on in the one content buffer from where the last page's ended, and
// it starts from PDF's default state.
void fude_zoom_pdf_page_begin(fude_zoom_pdf* _pdf, f64 _width_pt, f64 _height_pt) {
    if(_pdf == NULL) {
        return;
    }
    if(_pdf->open) {
        fude_zoom_pdf_page_end(_pdf);
    }
    fude_zoom_pdf_page _page = {
        .width  = rde_math_is_finite_f64(_width_pt) && _width_pt >= FUDE_ZOOM_PDF_SIDE ? _width_pt : FUDE_ZOOM_PDF_SIDE,
        .height = rde_math_is_finite_f64(_height_pt) && _height_pt >= FUDE_ZOOM_PDF_SIDE ? _height_pt : FUDE_ZOOM_PDF_SIDE,
        .at     = fude_bytes_size(&_pdf->content),
    };
    rde_arr_add(&_pdf->pages, &_page);
    rde_arr_clear(&_pdf->saved);
    _pdf->state = fude_zoom_pdf_default;
    _pdf->open  = true;
}

// See the header.
void fude_zoom_pdf_page_end(fude_zoom_pdf* _pdf) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    // Every stream balanced on its own: some readers refuse a q left open.
    while(rde_arr_length(&_pdf->saved) > 0u) {
        fude_zoom_pdf_restore(_pdf);
    }
    fude_zoom_pdf_page* _page = (fude_zoom_pdf_page*)rde_arr_at(&_pdf->pages, rde_arr_length(&_pdf->pages) - 1u);
    _page->size = fude_bytes_size(&_pdf->content) - _page->at;
    _pdf->open  = false;
}

// --- graphics state --------------------------------------------------------------------------------

// See the header: q, and the state put away for the restore that matches it.
void fude_zoom_pdf_save(fude_zoom_pdf* _pdf) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    fude_zoom_pdf_emit_str(_pdf, "q\n");
    rde_arr_add(&_pdf->saved, &_pdf->state);
}

// See the header: Q, and the state saved taken back.
void fude_zoom_pdf_restore(fude_zoom_pdf* _pdf) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    const u32 _depth = (u32)rde_arr_length(&_pdf->saved);
    if(_depth == 0u) {
        return;
    }
    fude_zoom_pdf_emit_str(_pdf, "Q\n");
    _pdf->state = *(const fude_zoom_pdf_state*)rde_arr_at(&_pdf->saved, _depth - 1u);
    rde_arr_remove(&_pdf->saved, _depth - 1u);
}

// See the header: its scale and turn written with FUDE_ZOOM_PDF_SCALE decimals, its shift with FUDE_ZOOM_PDF_PLACES.
void fude_zoom_pdf_transform(fude_zoom_pdf* _pdf, f64 _a, f64 _b, f64 _c, f64 _d, f64 _e, f64 _f) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    const f64 _turn[4]  = { _a, _b, _c, _d };
    const f64 _shift[2] = { _e, _f };
    c8  _buf[192];
    u32 _len = 0;
    for(u32 _i = 0; _i < 4u; _i++) {
        _len += fude_zoom_pdf_real(_turn[_i], FUDE_ZOOM_PDF_SCALE, _buf + _len);
        _buf[_len++] = ' ';
    }
    for(u32 _i = 0; _i < 2u; _i++) {
        _len += fude_zoom_pdf_real(_shift[_i], FUDE_ZOOM_PDF_PLACES, _buf + _len);
        _buf[_len++] = ' ';
    }
    rde_memcpy(_buf + _len, "cm\n", 3u);
    fude_zoom_pdf_emit(_pdf, _buf, _len + 3u);
}

// A colour for fills or strokes: its RGB when it changes, and its alpha by the
// shared ExtGState for that alpha when that changes.
RDE_INTERNAL void fude_zoom_pdf_color(fude_zoom_pdf* _pdf, rde_color _c, b8 _stroke) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    rde_color* _now = _stroke ? &_pdf->state.stroke : &_pdf->state.fill;
    if(_c.r != _now->r || _c.g != _now->g || _c.b != _now->b) {
        const f64 _rgb[3] = { _c.r / 255.0, _c.g / 255.0, _c.b / 255.0 };
        fude_zoom_pdf_op(_pdf, _rgb, 3u, FUDE_ZOOM_PDF_COLOR, _stroke ? "RG" : "rg");
    }
    if(_c.a != _now->a) {
        (_stroke ? _pdf->stroke_alpha : _pdf->fill_alpha)[_c.a] = true;
        c8 _buf[32];
        const int _n = rde_snprintf(_buf, sizeof _buf, "/%s%u gs\n", _stroke ? "as" : "af", (unsigned)_c.a);
        fude_zoom_pdf_emit(_pdf, _buf, (u32)_n);
    }
    *_now = _c;
}

// See the header.
void fude_zoom_pdf_fill_color(fude_zoom_pdf* _pdf, rde_color _c) {
    fude_zoom_pdf_color(_pdf, _c, false);
}

// See the header.
void fude_zoom_pdf_stroke_color(fude_zoom_pdf* _pdf, rde_color _c) {
    fude_zoom_pdf_color(_pdf, _c, true);
}

// See the header. A negative width, or not a number, is 0; nothing is written when the width is set already.
void fude_zoom_pdf_line_width(fude_zoom_pdf* _pdf, f64 _width) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    if(!rde_math_is_finite_f64(_width) || _width < 0.0) {
        _width = 0.0;   // PDF's thinnest line a device can draw
    }
    if(_width == _pdf->state.width) {
        return;
    }
    fude_zoom_pdf_op(_pdf, &_width, 1u, FUDE_ZOOM_PDF_PLACES, "w");
    _pdf->state.width = _width;
}

// See the header. Written every time: the state does not follow caps and joins.
void fude_zoom_pdf_line_style(fude_zoom_pdf* _pdf, u8 _cap, u8 _join) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    c8 _buf[16];
    const int _n = rde_snprintf(_buf, sizeof _buf, "%u J %u j\n", (unsigned)(_cap > 2u ? 2u : _cap), (unsigned)(_join > 2u ? 2u : _join));
    fude_zoom_pdf_emit(_pdf, _buf, (u32)_n);
}

// See the header. A length under 0.00005, which writes as 0, counts as zero.
void fude_zoom_pdf_dash(fude_zoom_pdf* _pdf, const f64* _pattern, u32 _count, f64 _phase) {
    if(_pdf == NULL || !_pdf->open) {
        return;
    }
    if(_pattern == NULL || _count > FUDE_ZOOM_PDF_DASHES) {
        _count = _pattern == NULL ? 0u : FUDE_ZOOM_PDF_DASHES;
    }
    // PDF calls a negative length, or a pattern of only zeros, an error: solid.
    b8 _any = false;
    for(u32 _i = 0; _i < _count; _i++) {
        if(!rde_math_is_finite_f64(_pattern[_i]) || _pattern[_i] < 0.0) {
            _count = 0u;
            break;
        }
        if(_pattern[_i] >= 0.00005) {
            _any = true;
        }
    }
    if(!_any) {
        _count = 0u;
    }
    c8  _buf[FUDE_ZOOM_PDF_DASHES * 24u + 40u];
    u32 _len = 0;
    _buf[_len++] = '[';
    for(u32 _i = 0; _i < _count; _i++) {
        if(_i > 0u) {
            _buf[_len++] = ' ';
        }
        _len += fude_zoom_pdf_real(_pattern[_i], FUDE_ZOOM_PDF_PLACES, _buf + _len);
    }
    _buf[_len++] = ']';
    _buf[_len++] = ' ';
    _len += fude_zoom_pdf_real(_count > 0u ? _phase : 0.0, FUDE_ZOOM_PDF_PLACES, _buf + _len);
    rde_memcpy(_buf + _len, " d\n", 3u);
    fude_zoom_pdf_emit(_pdf, _buf, _len + 3u);
}

// --- paths -----------------------------------------------------------------------------------------

// See the header.
void fude_zoom_pdf_move(fude_zoom_pdf* _pdf, f64 _x, f64 _y) {
    if(_pdf != NULL && _pdf->open) {
        const f64 _v[2] = { _x, _y };
        fude_zoom_pdf_op(_pdf, _v, 2u, FUDE_ZOOM_PDF_PLACES, "m");
    }
}

// See the header.
void fude_zoom_pdf_line(fude_zoom_pdf* _pdf, f64 _x, f64 _y) {
    if(_pdf != NULL && _pdf->open) {
        const f64 _v[2] = { _x, _y };
        fude_zoom_pdf_op(_pdf, _v, 2u, FUDE_ZOOM_PDF_PLACES, "l");
    }
}

// See the header.
void fude_zoom_pdf_curve(fude_zoom_pdf* _pdf, f64 _x1, f64 _y1, f64 _x2, f64 _y2, f64 _x3, f64 _y3) {
    if(_pdf != NULL && _pdf->open) {
        const f64 _v[6] = { _x1, _y1, _x2, _y2, _x3, _y3 };
        fude_zoom_pdf_op(_pdf, _v, 6u, FUDE_ZOOM_PDF_PLACES, "c");
    }
}

// See the header.
void fude_zoom_pdf_close(fude_zoom_pdf* _pdf) {
    if(_pdf != NULL && _pdf->open) {
        fude_zoom_pdf_emit_str(_pdf, "h\n");
    }
}

// See the header.
void fude_zoom_pdf_rect(fude_zoom_pdf* _pdf, f64 _x, f64 _y, f64 _w, f64 _h) {
    if(_pdf != NULL && _pdf->open) {
        const f64 _v[4] = { _x, _y, _w, _h };
        fude_zoom_pdf_op(_pdf, _v, 4u, FUDE_ZOOM_PDF_PLACES, "re");
    }
}

// See the header.
void fude_zoom_pdf_polyline(fude_zoom_pdf* _pdf, const f64* _xy, u32 _count, b8 _closed) {
    if(_pdf == NULL || !_pdf->open || _xy == NULL || _count == 0u) {
        return;
    }
    // In blocks, straight into a buffer: no call per number.
    c8  _buf[4096];
    u32 _len = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        if(_len > sizeof _buf - 64u) {
            fude_zoom_pdf_emit(_pdf, _buf, _len);
            _len = 0;
        }
        _len += fude_zoom_pdf_real(_xy[2u * _i], FUDE_ZOOM_PDF_PLACES, _buf + _len);
        _buf[_len++] = ' ';
        _len += fude_zoom_pdf_real(_xy[2u * _i + 1u], FUDE_ZOOM_PDF_PLACES, _buf + _len);
        _buf[_len++] = ' ';
        _buf[_len++] = _i == 0u ? 'm' : 'l';
        _buf[_len++] = '\n';
    }
    if(_closed) {
        _buf[_len++] = 'h';
        _buf[_len++] = '\n';
    }
    fude_zoom_pdf_emit(_pdf, _buf, _len);
}

// See the header.
void fude_zoom_pdf_fill(fude_zoom_pdf* _pdf, b8 _even_odd) {
    if(_pdf != NULL && _pdf->open) {
        fude_zoom_pdf_emit_str(_pdf, _even_odd ? "f*\n" : "f\n");
    }
}

// See the header.
void fude_zoom_pdf_stroke(fude_zoom_pdf* _pdf) {
    if(_pdf != NULL && _pdf->open) {
        fude_zoom_pdf_emit_str(_pdf, "S\n");
    }
}

// See the header.
void fude_zoom_pdf_fill_stroke(fude_zoom_pdf* _pdf, b8 _even_odd) {
    if(_pdf != NULL && _pdf->open) {
        fude_zoom_pdf_emit_str(_pdf, _even_odd ? "B*\n" : "B\n");
    }
}

// See the header.
void fude_zoom_pdf_clip(fude_zoom_pdf* _pdf, b8 _even_odd) {
    if(_pdf != NULL && _pdf->open) {
        fude_zoom_pdf_emit_str(_pdf, _even_odd ? "W* n\n" : "W n\n");
    }
}

// --- images ----------------------------------------------------------------------------------------

// A JPEG's size and colours from its frame header, and whether Adobe's marker
// (APP14) is there, which means a CMYK one is stored inverted. Only the frames
// DCTDecode reads (SOF0 baseline, SOF1 extended, SOF2 progressive; 8 bits).
RDE_INTERNAL b8 fude_zoom_pdf_jpeg_info(const u8* _b, u32 _size, fude_zoom_pdf_picture* _p) {
    if(_size < 4u || _b[0] != 0xFFu || _b[1] != 0xD8u) {
        return false;
    }
    u32 _i = 2u;
    while(_i < _size) {
        if(_b[_i] != 0xFFu) {
            return false;
        }
        while(_i < _size && _b[_i] == 0xFFu) {   // a marker may be padded with 0xFF
            _i++;
        }
        if(_i >= _size) {
            return false;
        }
        const u8 _m = _b[_i++];
        if(_m == 0x01u || (_m >= 0xD0u && _m <= 0xD8u)) {
            continue;   // markers with nothing after them
        }
        if(_m == 0xD9u || _m == 0xDAu || _i + 2u > _size) {
            return false;   // the end, or a scan, before any frame header
        }
        const u32 _len = ((u32)_b[_i] << 8) | (u32)_b[_i + 1u];
        if(_len < 2u || _len > _size - _i) {
            return false;
        }
        const u8* _seg = _b + _i + 2u;   // the segment after its length
        if(_m == 0xC0u || _m == 0xC1u || _m == 0xC2u) {
            if(_len < 8u) {
                return false;
            }
            _p->height     = ((u32)_seg[1] << 8) | (u32)_seg[2];
            _p->width      = ((u32)_seg[3] << 8) | (u32)_seg[4];
            _p->components = _seg[5];
            return _seg[0] == 8u && _p->width > 0u && _p->height > 0u
                && (_p->components == 1u || _p->components == 3u || _p->components == 4u)
                && _len >= 8u + 3u * (u32)_p->components;
        }
        if(_m >= 0xC3u && _m <= 0xCFu && _m != 0xC4u && _m != 0xC8u && _m != 0xCCu) {
            return false;   // lossless or arithmetic-coded: DCTDecode reads neither
        }
        if(_m == 0xEEu && _len >= 7u && rde_memcmp(_seg, "Adobe", 5u) == 0) {
            _p->inverted = true;
        }
        _i += _len;
    }
    return false;
}

// See the header. An image's id is its place in the list, from 1: the file names it /Im<id>.
u32 fude_zoom_pdf_jpeg(fude_zoom_pdf* _pdf, const u8* _bytes, u32 _size) {
    if(_pdf == NULL || _bytes == NULL) {
        return 0u;
    }
    fude_zoom_pdf_picture _p;
    rde_memset(&_p, 0, sizeof _p);
    if(!fude_zoom_pdf_jpeg_info(_bytes, _size, &_p)) {
        return 0u;
    }
    if((u64)fude_bytes_size(&_pdf->content) + (u64)fude_bytes_size(&_pdf->data) + _size > FUDE_ZOOM_PDF_LIMIT) {
        return 0u;
    }
    _p.jpeg     = true;
    _p.inverted = _p.inverted && _p.components == 4u;
    _p.at       = fude_bytes_size(&_pdf->data);
    _p.size     = _size;
    fude_put_data(&_pdf->data, _bytes, _size);
    rde_arr_add(&_pdf->images, &_p);
    return (u32)rde_arr_length(&_pdf->images);
}

// See the header. Its RGB samples and, unless every pixel is opaque, its alpha as a grey image of its own (the soft
// mask) go into the data buffer.
u32 fude_zoom_pdf_rgba(fude_zoom_pdf* _pdf, const u8* _rgba, u32 _width, u32 _height) {
    if(_pdf == NULL || _rgba == NULL || _width == 0u || _height == 0u) {
        return 0u;
    }
    const u64 _n = (u64)_width * (u64)_height;
    if(_n > FUDE_ZOOM_PDF_LIMIT) {
        return 0u;
    }
    b8 _opaque = true;
    for(u64 _i = 0; _i < _n && _opaque; _i++) {
        _opaque = _rgba[_i * 4u + 3u] == 255u;
    }
    if((u64)fude_bytes_size(&_pdf->content) + (u64)fude_bytes_size(&_pdf->data) + _n * (_opaque ? 3u : 4u) > FUDE_ZOOM_PDF_LIMIT) {
        return 0u;
    }
    fude_zoom_pdf_picture _p;
    rde_memset(&_p, 0, sizeof _p);
    _p.width      = _width;
    _p.height     = _height;
    _p.components = 3u;
    _p.at         = fude_bytes_size(&_pdf->data);
    _p.size       = (u32)(_n * 3u);
    u8* _rgb = (u8*)rde_arr_add_n(&_pdf->data, (usize)_p.size);
    for(u64 _i = 0; _i < _n; _i++) {
        _rgb[_i * 3u]      = _rgba[_i * 4u];
        _rgb[_i * 3u + 1u] = _rgba[_i * 4u + 1u];
        _rgb[_i * 3u + 2u] = _rgba[_i * 4u + 2u];
    }
    if(!_opaque) {
        _p.mask_at   = fude_bytes_size(&_pdf->data);
        _p.mask_size = (u32)_n;
        u8* _alpha = (u8*)rde_arr_add_n(&_pdf->data, (usize)_n);
        for(u64 _i = 0; _i < _n; _i++) {
            _alpha[_i] = _rgba[_i * 4u + 3u];
        }
    }
    rde_arr_add(&_pdf->images, &_p);
    return (u32)rde_arr_length(&_pdf->images);
}

// See the header: drawn inside a save, its matrix applied, by its /Im<id> name.
void fude_zoom_pdf_image(fude_zoom_pdf* _pdf, u32 _id, f64 _a, f64 _b, f64 _c, f64 _d, f64 _e, f64 _f) {
    if(_pdf == NULL || !_pdf->open || _id == 0u || _id > (u32)rde_arr_length(&_pdf->images)) {
        return;
    }
    fude_zoom_pdf_save(_pdf);
    fude_zoom_pdf_transform(_pdf, _a, _b, _c, _d, _e, _f);
    c8 _buf[32];
    const int _n = rde_snprintf(_buf, sizeof _buf, "/Im%u Do\n", (unsigned)_id);
    fude_zoom_pdf_emit(_pdf, _buf, (u32)_n);
    fude_zoom_pdf_restore(_pdf);
}

// --- text ------------------------------------------------------------------------------------------

// A code point as WinAnsiEncoding has it; '?' for one it has not.
RDE_INTERNAL u8 fude_zoom_pdf_winansi(u32 _cp) {
    if((_cp >= 0x20u && _cp <= 0x7Eu) || (_cp >= 0xA0u && _cp <= 0xFFu)) {
        return (u8)_cp;
    }
    for(u32 _i = 0; _i < 32u; _i++) {
        if(fude_zoom_pdf_winansi_extra[_i] != 0u && fude_zoom_pdf_winansi_extra[_i] == _cp) {
            return (u8)(0x80u + _i);
        }
    }
    return (u8)'?';
}

// See the header: face 0.
void fude_zoom_pdf_text(fude_zoom_pdf* _pdf, f64 _x, f64 _y, f64 _size, rde_color _c, const c8* _utf8) {
    fude_zoom_pdf_text_face(_pdf, _x, _y, _size, _c, _utf8, 0u);
}

// See the header. In the fonts set, each character found in the regular face's chain (glyph 0 meaning none); in
// Helvetica, each as fude_zoom_pdf_winansi finds it, a '?' that is not a '?' itself meaning none.
b8 fude_zoom_pdf_text_writes(const c8* _utf8) {
    if(_utf8 == NULL) {
        return false;
    }
    const u8* _chain = fude_zoom_pdf_chain(fude_zoom_pdf_fonts_now, 0u);
    const c8* _s     = _utf8;
    for(u32 _cp = fude_utf8_next(&_s); _cp != 0u; _cp = fude_utf8_next(&_s)) {
        if(_chain != NULL) {
            u32 _k, _g;
            fude_zoom_pdf_find(fude_zoom_pdf_fonts_now, _chain, _cp, &_k, &_g);
            if(_g == 0u && !fude_zoom_pdf_ignorable(_cp)) {
                return false;
            }
        } else if(_cp != (u32)'?' && fude_zoom_pdf_winansi(_cp) == (u8)'?') {
            return false;
        }
    }
    return true;
}

// Marks glyph _g of font _k (of the document's set) used, written for character _cp: the first character it is
// written for is what it copies as (glyph 0, the missing glyph, copies as U+FFFD, and so does a character UTF-16
// cannot hold: a surrogate, or one past U+10FFFF).
RDE_INTERNAL void fude_zoom_pdf_use(fude_zoom_pdf* _pdf, u32 _k, u32 _g, u32 _cp) {
    rde_arr* _used = &_pdf->used[_k];
    if(!rde_arr_is_inited(_used)) {
        *_used = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
        rde_arr_resize(_used, _pdf->fonts->fonts[_k].font.glyphs);
    }
    u32* _first = (u32*)_used->memory;
    if(_g < (u32)rde_arr_length(_used) && _first[_g] == 0u) {
        const b8 _plain = _g != 0u && _cp <= 0x10FFFFu && (_cp < 0xD800u || _cp > 0xDFFFu);
        _first[_g] = _plain ? _cp : FUDE_ZOOM_PDF_NOTDEF;
    }
}

// Writes _utf8 in chain _chain of the document's fonts, as fude_zoom_pdf_text_face says: inside a save, in runs of
// one font each (/E<n>: the font's place in the set + 1), each character its code as four hex digits in a hex string
// (Identity-H), and the characters the screen hides left out. The buffer is sent on before a character could leave less
// room than a run's change (at most "> Tj\n", the font and its size, the place and "<": under 80 bytes), the
// character's four digits and the closing "> Tj\nET\n" take.
RDE_INTERNAL void fude_zoom_pdf_text_fonts(fude_zoom_pdf* _pdf, const u8* _chain, f64 _x, f64 _y, f64 _size, rde_color _c, const c8* _utf8) {
    static const c8 _hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    fude_zoom_pdf_save(_pdf);
    fude_zoom_pdf_fill_color(_pdf, _c);
    c8  _buf[512];
    u32 _len = 3u;
    u32 _run = FUDE_ZOOM_PDF_NONE;   // the font being written in
    rde_memcpy(_buf, "BT\n", 3u);
    const c8* _s = _utf8;
    for(u32 _cp = fude_utf8_next(&_s); _cp != 0u; _cp = fude_utf8_next(&_s)) {
        if(fude_zoom_pdf_ignorable(_cp)) {
            continue;
        }
        u32 _k, _g;
        fude_zoom_pdf_find(_pdf->fonts, _chain, _cp, &_k, &_g);
        if(_len + 128u > sizeof _buf) {
            fude_zoom_pdf_emit(_pdf, _buf, _len);
            _len = 0;
        }
        if(_k != _run) {
            if(_run != FUDE_ZOOM_PDF_NONE) {
                rde_memcpy(_buf + _len, "> Tj\n", 5u);
                _len += 5u;
            }
            _len += (u32)rde_snprintf(_buf + _len, sizeof _buf - _len, "/E%u ", (unsigned)(_k + 1u));
            _len += fude_zoom_pdf_real(_size, FUDE_ZOOM_PDF_PLACES, _buf + _len);
            rde_memcpy(_buf + _len, " Tf\n", 4u);
            _len += 4u;
            if(_run == FUDE_ZOOM_PDF_NONE) {
                _len += fude_zoom_pdf_real(_x, FUDE_ZOOM_PDF_PLACES, _buf + _len);
                _buf[_len++] = ' ';
                _len += fude_zoom_pdf_real(_y, FUDE_ZOOM_PDF_PLACES, _buf + _len);
                rde_memcpy(_buf + _len, " Td\n", 4u);
                _len += 4u;
            }
            _buf[_len++] = '<';
            _run = _k;
        }
        const u32 _code = fude_zoom_pdf_code(&_pdf->fonts->fonts[_k], _g);
        for(u32 _d = 0; _d < 4u; _d++) {
            _buf[_len++] = _hex[(_code >> (12u - 4u * _d)) & 0x0Fu];
        }
        fude_zoom_pdf_use(_pdf, _k, _g, _cp);
    }
    if(_run != FUDE_ZOOM_PDF_NONE) {
        rde_memcpy(_buf + _len, "> Tj\n", 5u);
        _len += 5u;
    }
    rde_memcpy(_buf + _len, "ET\n", 3u);
    fude_zoom_pdf_emit(_pdf, _buf, _len + 3u);
    fude_zoom_pdf_restore(_pdf);
}

// See the header. In the document's fonts when its chain for the face is there (fude_zoom_pdf_text_fonts); else in
// Helvetica, written inside a save, so the fill colour is put back after, each character as its WinAnsi byte in a
// literal string.
void fude_zoom_pdf_text_face(fude_zoom_pdf* _pdf, f64 _x, f64 _y, f64 _size, rde_color _c, const c8* _utf8, u8 _face) {
    if(_pdf == NULL || !_pdf->open || _utf8 == NULL || _utf8[0] == '\0' || !rde_math_is_finite_f64(_size) || _size <= 0.0) {
        return;
    }
    _face = _face < 4u ? _face : 0u;
    const u8* _chain = fude_zoom_pdf_chain(_pdf->fonts, _face);
    if(_chain != NULL) {
        fude_zoom_pdf_text_fonts(_pdf, _chain, _x, _y, _size, _c, _utf8);
        return;
    }
    _pdf->text         = true;
    _pdf->faces[_face] = true;
    fude_zoom_pdf_save(_pdf);
    fude_zoom_pdf_fill_color(_pdf, _c);
    c8  _buf[512];
    u32 _len = 0;
    rde_memcpy(_buf, "BT\n/F1 ", 7u);
    _buf[5] = (c8)('1' + _face);
    _len = 7u;
    _len += fude_zoom_pdf_real(_size, FUDE_ZOOM_PDF_PLACES, _buf + _len);
    rde_memcpy(_buf + _len, " Tf\n", 4u);
    _len += 4u;
    const f64 _at[2] = { _x, _y };
    for(u32 _i = 0; _i < 2u; _i++) {
        _len += fude_zoom_pdf_real(_at[_i], FUDE_ZOOM_PDF_PLACES, _buf + _len);
        _buf[_len++] = ' ';
    }
    rde_memcpy(_buf + _len, "Td\n(", 4u);
    _len += 4u;
    // The bytes as they are (a literal string may hold any), but for the three
    // the string itself is made of. The buffer is sent on before a character
    // could leave less room than the closing ") Tj\nET\n" takes: a character is
    // at most 2 bytes (escaped), the closing 8.
    const c8* _s = _utf8;
    for(u32 _cp = fude_utf8_next(&_s); _cp != 0u; _cp = fude_utf8_next(&_s)) {
        if(_len + 2u + 8u > sizeof _buf) {
            fude_zoom_pdf_emit(_pdf, _buf, _len);
            _len = 0;
        }
        const u8 _byte = fude_zoom_pdf_winansi(_cp);
        if(_byte == '(' || _byte == ')' || _byte == '\\') {
            _buf[_len++] = '\\';
        }
        _buf[_len++] = (c8)_byte;
    }
    rde_memcpy(_buf + _len, ") Tj\nET\n", 8u);
    fude_zoom_pdf_emit(_pdf, _buf, _len + 8u);
    fude_zoom_pdf_restore(_pdf);
}

// See the header: the regular face's.
f64 fude_zoom_pdf_text_width(f64 _size, const c8* _utf8) {
    return fude_zoom_pdf_text_width_face(_size, _utf8, 0u);
}

// See the header. In the fonts set now, each character's glyph's advance in ems (as fude_zoom_pdf_text_fonts finds the
// glyph, a hidden character none); in Helvetica, each WinAnsi byte's width.
f64 fude_zoom_pdf_text_width_face(f64 _size, const c8* _utf8, u8 _face) {
    if(_utf8 == NULL || !rde_math_is_finite_f64(_size)) {
        return 0.0;
    }
    fude_zoom_pdf_fontset* _set   = fude_zoom_pdf_fonts_now;
    const u8*              _chain = fude_zoom_pdf_chain(_set, _face);
    const c8*              _s     = _utf8;
    if(_chain == NULL) {
        u64 _units = 0;
        for(u32 _cp = fude_utf8_next(&_s); _cp != 0u; _cp = fude_utf8_next(&_s)) {
            _units += fude_zoom_pdf_helvetica[fude_zoom_pdf_winansi(_cp)];
        }
        return (f64)_units * _size / 1000.0;
    }
    f64 _ems = 0.0;
    for(u32 _cp = fude_utf8_next(&_s); _cp != 0u; _cp = fude_utf8_next(&_s)) {
        if(fude_zoom_pdf_ignorable(_cp)) {
            continue;
        }
        u32 _k, _g;
        fude_zoom_pdf_find(_set, _chain, _cp, &_k, &_g);
        const fude_zoom_mfont* _f = &_set->fonts[_k].font;   // (read: the chain's first font, or the one that had it)
        _ems += (f64)fude_zoom_mfont_advance(_f, _g) / (f64)_f->units_per_em;
    }
    return _ems * _size;
}

// --- the file --------------------------------------------------------------------------------------

// Where object _n starts (from the file's first byte), and its first line.
RDE_INTERNAL void fude_zoom_pdf_object(fude_bytes* _out, u32 _base, u32* _offsets, u32 _n) {
    _offsets[_n] = fude_bytes_size(_out) - _base;
    fude_zoom_pdf_put(_out, "%u 0 obj\n", (unsigned)_n);
}

// Ends a stream object: its length, its bytes, endstream and endobj. Its dictionary is opened before by the caller,
// which may have put entries in it.
RDE_INTERNAL void fude_zoom_pdf_stream(fude_bytes* _out, const u8* _data, u32 _size) {
    fude_zoom_pdf_put(_out, "/Length %u >>\nstream\n", (unsigned)_size);
    fude_put_data(_out, _data, _size);
    fude_zoom_pdf_put(_out, "\nendstream\nendobj\n");
}

// --- the fonts embedded ----------------------------------------------------------------------------

// Appends _v to _out as its last _n bytes, big-endian, as a font file's numbers are.
RDE_INTERNAL void fude_zoom_pdf_be(fude_bytes* _out, u32 _v, u32 _n) {
    for(u32 _i = _n; _i > 0u; _i--) {
        fude_put_u8(_out, (u8)(_v >> (8u * (_i - 1u))));
    }
}

// Writes _v big-endian over the 4 bytes at _at of _out (which holds them already).
RDE_INTERNAL void fude_zoom_pdf_be_at(fude_bytes* _out, u32 _at, u32 _v) {
    u8* _p = _out->memory + _at;
    _p[0] = (u8)(_v >> 24);
    _p[1] = (u8)(_v >> 16);
    _p[2] = (u8)(_v >> 8);
    _p[3] = (u8)_v;
}

// A TrueType check sum: _n bytes at _p added up as big-endian u32 words (wrapping), the last word padded with zeros.
RDE_INTERNAL u32 fude_zoom_pdf_sum(const u8* _p, u32 _n) {
    u32 _sum = 0;
    for(u32 _i = 0; _i < _n; _i += 4u) {
        u32 _word = 0;
        for(u32 _k = 0; _k < 4u; _k++) {
            _word = (_word << 8) | (_i + _k < _n ? _p[_i + _k] : 0u);
        }
        _sum += _word;
    }
    return _sum;
}

// Which glyphs a font cut down keeps, into _keep (an rde_arr of u8, resized to the font's glyphs; 1 kept): glyph 0
// (the missing glyph), every glyph _used marks (fude_zoom_pdf.used's), and the parts of every composite kept, and
// theirs, each taken once.
RDE_INTERNAL void fude_zoom_pdf_keep(const fude_zoom_mfont* _f, const rde_arr* _used, rde_arr* _keep) {
    const u32  _n     = _f->glyphs;
    const u32* _first = (const u32*)_used->memory;
    rde_arr_resize(_keep, _n);
    u8* _k = (u8*)_keep->memory;
    rde_arr _todo = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    for(u32 _g = 0; _g < _n; _g++) {
        if(_g == 0u || (_g < (u32)rde_arr_length(_used) && _first[_g] != 0u)) {
            _k[_g] = 1u;
            rde_arr_add(&_todo, &_g);
        }
    }
    while(rde_arr_length(&_todo) > 0u) {
        const u32 _g = ((const u32*)_todo.memory)[rde_arr_length(&_todo) - 1u];
        rde_arr_resize(&_todo, rde_arr_length(&_todo) - 1u);
        u32       _parts[FUDE_ZOOM_PDF_PARTS];
        const u32 _count = fude_zoom_mfont_parts(_f, _g, _parts, FUDE_ZOOM_PDF_PARTS);
        for(u32 _i = 0; _i < _count; _i++) {
            if(_parts[_i] < _n && _k[_parts[_i]] == 0u) {
                _k[_parts[_i]] = 1u;
                rde_arr_add(&_todo, &_parts[_i]);
            }
        }
    }
    rde_arr_free(&_todo);
}

// Appends TrueType font _f cut down to the glyphs _used marks to _out, as a TrueType file (see pdf.h's design): every
// glyph id kept, the glyphs fude_zoom_pdf_keep leaves out emptied; glyf rebuilt with the rest (each on 4 bytes) and
// loca with where each starts (short, halved, when glyf is small enough); head copied with its loca format and its
// check sum adjustment set; the other tables of fude_zoom_pdf_kept the font has copied as they are. Each table starts
// on 4 bytes, and its check sum is in the table directory.
RDE_INTERNAL void fude_zoom_pdf_subset(const fude_zoom_mfont* _f, const rde_arr* _used, fude_bytes* _out) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    const u32 _n = _f->glyphs;
    rde_arr _keep = rde_arr_new(sizeof(u8), _heap), _starts = rde_arr_new(sizeof(u32), _heap);
    fude_zoom_pdf_keep(_f, _used, &_keep);
    rde_arr_resize(&_starts, (usize)_n + 1u);
    u32*       _start = (u32*)_starts.memory;
    fude_bytes _glyf  = fude_bytes_new(4096u), _loca = fude_bytes_new(4096u);
    for(u32 _g = 0; _g < _n; _g++) {
        _start[_g] = fude_bytes_size(&_glyf);
        u32 _at, _len;
        if(((const u8*)_keep.memory)[_g] != 0u && fude_zoom_mfont_glyph_data(_f, _g, &_at, &_len)) {
            fude_put_data(&_glyf, _f->data + _at, _len);
            while(fude_bytes_size(&_glyf) % 4u != 0u) {
                fude_put_u8(&_glyf, 0u);
            }
        }
    }
    _start[_n] = fude_bytes_size(&_glyf);
    const b8 _short = _start[_n] <= FUDE_ZOOM_PDF_SHORT;
    for(u32 _g = 0; _g <= _n; _g++) {
        fude_zoom_pdf_be(&_loca, _short ? _start[_g] / 2u : _start[_g], _short ? 2u : 4u);
    }
    // (the tables the font has, in fude_zoom_pdf_kept's order: their bytes and lengths)
    const u8* _bytes[13];
    u32       _size[13], _tag[13], _count = 0;
    for(u32 _i = 0; _i < 13u; _i++) {
        const u32 _t = fude_zoom_pdf_kept[_i];
        if(_t == FUDE_ZOOM_PDF_TAG_GLYF || _t == FUDE_ZOOM_PDF_TAG_LOCA) {
            const fude_bytes* _made = _t == FUDE_ZOOM_PDF_TAG_GLYF ? &_glyf : &_loca;
            _bytes[_count] = _made->memory;
            _size[_count]  = fude_bytes_size(_made);
        } else {
            u32       _len = 0;
            const u32 _at  = fude_zoom_mfont_table(_f, _t, &_len);
            if(_at == 0u) {
                continue;   // (one it has not: cvt, fpgm and prep in a font with no hinting, say)
            }
            _bytes[_count] = _f->data + _at;
            _size[_count]  = _len;
        }
        _tag[_count++] = _t;
    }
    // (the directory: its search numbers from the greatest power of two of tables, 2^_log, then a record a table)
    u32 _log = 0;
    while((2u << _log) <= _count) {
        _log++;
    }
    const u32 _base = fude_bytes_size(_out), _range = 16u << _log;
    fude_zoom_pdf_be(_out, 0x00010000u, 4u);   // (TrueType outlines)
    fude_zoom_pdf_be(_out, _count, 2u);
    fude_zoom_pdf_be(_out, _range, 2u);
    fude_zoom_pdf_be(_out, _log, 2u);
    fude_zoom_pdf_be(_out, _count * 16u - _range, 2u);
    const u32 _dir = fude_bytes_size(_out);
    rde_arr_resize(_out, (usize)_dir + 16u * (usize)_count);
    u32 _head = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        const u32 _at = fude_bytes_size(_out);
        fude_put_data(_out, _bytes[_i], _size[_i]);
        if(_tag[_i] == FUDE_ZOOM_PDF_TAG_HEAD) {   // (at least FUDE_ZOOM_PDF_HEAD long: fude_zoom_pdf_font_get)
            _head = _at;
            fude_zoom_pdf_be_at(_out, _at + 8u, 0u);   // its checkSumAdjustment: 0 while the sums are made
            _out->memory[_at + 50u] = 0u;              // its indexToLocFormat: 0 short, 1 long
            _out->memory[_at + 51u] = _short ? 0u : 1u;
        }
        while((fude_bytes_size(_out) - _base) % 4u != 0u) {
            fude_put_u8(_out, 0u);
        }
        const u32 _rec = _dir + 16u * _i;
        fude_zoom_pdf_be_at(_out, _rec, _tag[_i]);
        fude_zoom_pdf_be_at(_out, _rec + 4u, fude_zoom_pdf_sum(_out->memory + _at, _size[_i]));
        fude_zoom_pdf_be_at(_out, _rec + 8u, _at - _base);
        fude_zoom_pdf_be_at(_out, _rec + 12u, _size[_i]);
    }
    fude_zoom_pdf_be_at(_out, _head + 8u, FUDE_ZOOM_PDF_SUM - fude_zoom_pdf_sum(_out->memory + _base, fude_bytes_size(_out) - _base));
    rde_arr_free(&_glyf);
    rde_arr_free(&_loca);
    rde_arr_free(&_keep);
    rde_arr_free(&_starts);
}

// rde_arr_sort's order for an embedded font's glyphs: by code, as the W array and the ToUnicode map list them.
RDE_INTERNAL int fude_zoom_pdf_by_code(const any _a, const any _b) {
    const u32 _p = ((const fude_zoom_pdf_glyph*)_a)->code, _q = ((const fude_zoom_pdf_glyph*)_b)->code;
    return _p < _q ? -1 : (_p > _q ? 1 : 0);
}

// Appends the ToUnicode CMap of glyphs _glyphs (by code) to _out: each code to its character in UTF-16 (two units, a
// surrogate pair, past U+FFFF), in bfchar blocks of FUDE_ZOOM_PDF_BFCHAR.
RDE_INTERNAL void fude_zoom_pdf_tounicode(const rde_arr* _glyphs, fude_bytes* _out) {
    const fude_zoom_pdf_glyph* _g = (const fude_zoom_pdf_glyph*)_glyphs->memory;
    const u32                  _n = (u32)rde_arr_length(_glyphs);
    fude_zoom_pdf_put(_out, "/CIDInit /ProcSet findresource begin\n12 dict begin\nbegincmap\n");
    fude_zoom_pdf_put(_out, "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n");
    fude_zoom_pdf_put(_out, "/CMapName /Adobe-Identity-UCS def\n/CMapType 2 def\n1 begincodespacerange\n<0000> <FFFF>\nendcodespacerange\n");
    for(u32 _i = 0; _i < _n; _i += FUDE_ZOOM_PDF_BFCHAR) {
        const u32 _m = _n - _i < FUDE_ZOOM_PDF_BFCHAR ? _n - _i : FUDE_ZOOM_PDF_BFCHAR;
        fude_zoom_pdf_put(_out, "%u beginbfchar\n", (unsigned)_m);
        for(u32 _k = _i; _k < _i + _m; _k++) {
            const u32 _u = _g[_k].unicode;
            if(_u >= 0x10000u) {
                fude_zoom_pdf_put(_out, "<%04X> <%04X%04X>\n", (unsigned)_g[_k].code, (unsigned)(0xD800u + ((_u - 0x10000u) >> 10)),
                                  (unsigned)(0xDC00u + ((_u - 0x10000u) & 0x3FFu)));
            } else {
                fude_zoom_pdf_put(_out, "<%04X> <%04X>\n", (unsigned)_g[_k].code, (unsigned)_u);
            }
        }
        fude_zoom_pdf_put(_out, "endbfchar\n");
    }
    fude_zoom_pdf_put(_out, "endcmap\nCMapName currentdict /CMap defineresource pop\nend\nend\n");
}

// Names embedded font _k (_font) as its /BaseFont into _e->name: a subset tag of six capitals from a hash of its place
// and its codes (two subsets told apart, and the same document named the same each time it is finished), '+', and its
// file's name without its folder or extension, kept to what a PDF name holds as it is (ASCII letters, digits, '-' and
// '_'; "Font" when nothing is left), cut to fit.
RDE_INTERNAL void fude_zoom_pdf_embed_name(const fude_zoom_pdf_font* _font, u32 _k, fude_zoom_pdf_embed* _e) {
    const fude_zoom_pdf_glyph* _g    = (const fude_zoom_pdf_glyph*)_e->glyphs.memory;
    u32                        _hash = _k;   // (RDE's hash over its codes in turn, seeded with its place)
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_e->glyphs); _i++) {
        _hash = rde_hash_32_seeded(&_g[_i].code, sizeof(_g[_i].code), _hash);
    }
    u32 _len = 0;
    for(u32 _i = 0; _i < 6u; _i++) {
        _e->name[_len++] = (c8)('A' + (c8)(_hash % 26u));
        _hash /= 26u;
    }
    _e->name[_len++] = '+';
    const c8* _path = (const c8*)_font->path.memory;
    const c8* _from = _path;
    for(const c8* _p = _path; *_p != 0; _p++) {
        _from = *_p == '/' || *_p == '\\' ? _p + 1 : _from;
    }
    const c8* _dot = rde_strrchr(_from, '.');
    const c8* _to  = _dot != NULL ? _dot : _from + rde_strlen(_from);
    const u32 _named = _len;
    for(const c8* _p = _from; _p < _to && _len + 1u < sizeof(_e->name); _p++) {
        const c8 _c = *_p;
        if((_c >= 'A' && _c <= 'Z') || (_c >= 'a' && _c <= 'z') || (_c >= '0' && _c <= '9') || _c == '-' || _c == '_') {
            _e->name[_len++] = _c;
        }
    }
    if(_len == _named) {
        rde_memcpy(_e->name + _len, "Font", 4u);
        _len += 4u;
    }
    _e->name[_len] = 0;
}

// Makes what font _k of the document is embedded from (into _e, which the caller zeroed): its glyphs used by code, its
// program (cut down, or its CFF table whole), its ToUnicode map and its name. False, _e left empty, when the document
// wrote nothing in it. Free it with fude_zoom_pdf_embed_free.
RDE_INTERNAL b8 fude_zoom_pdf_embed_make(const fude_zoom_pdf* _pdf, u32 _k, fude_zoom_pdf_embed* _e) {
    const rde_arr* _used = &_pdf->used[_k];
    if(_pdf->fonts == NULL || !rde_arr_is_inited(_used)) {
        return false;
    }
    const fude_zoom_pdf_font* _font  = &_pdf->fonts->fonts[_k];
    const fude_zoom_mfont*    _f     = &_font->font;
    const u32*                _first = (const u32*)_used->memory;
    _e->glyphs = rde_arr_new(sizeof(fude_zoom_pdf_glyph), rde_memory_allocator_get_default_std());
    for(u32 _g = 0; _g < (u32)rde_arr_length(_used); _g++) {
        if(_first[_g] != 0u) {
            const fude_zoom_pdf_glyph _glyph = { fude_zoom_pdf_code(_font, _g), _g, _first[_g] };
            rde_arr_add(&_e->glyphs, &_glyph);
        }
    }
    rde_arr_sort(&_e->glyphs, fude_zoom_pdf_by_code);
    _e->program = fude_bytes_new(16384u);
    if(_f->cff != 0u) {
        u32       _len = 0;
        const u32 _at  = fude_zoom_mfont_table(_f, FUDE_ZOOM_PDF_TAG_CFF, &_len);
        fude_put_data(&_e->program, _f->data + _at, _len);
    } else {
        fude_zoom_pdf_subset(_f, _used, &_e->program);
    }
    _e->unicode = fude_bytes_new(1024u);
    fude_zoom_pdf_tounicode(&_e->glyphs, &_e->unicode);
    fude_zoom_pdf_embed_name(_font, _k, _e);
    return true;
}

// Frees what fude_zoom_pdf_embed_make made (nothing when it made nothing).
RDE_INTERNAL void fude_zoom_pdf_embed_free(fude_zoom_pdf_embed* _e) {
    if(rde_arr_is_inited(&_e->glyphs)) {
        rde_arr_free(&_e->glyphs);
        rde_arr_free(&_e->program);
        rde_arr_free(&_e->unicode);
    }
}

// Writes embedded font _e (font _k of the document's set) as objects _obj to _obj + 4: the Type 0 font; its CIDFont,
// with each glyph's width (W, in runs of consecutive codes: "first [w w w]"); its descriptor, from head's box and
// style, post's slant, OS/2's cap height, weight and style (its stem guessed from the weight, as PDF writers do:
// 50 + (weight / 65)²) and hhea's ascender and descender; its program (FontFile2 for TrueType, with its length as
// Length1; FontFile3 CIDFontType0C for a CFF); and its ToUnicode map. Every number of the font in thousandths of an em.
RDE_INTERNAL void fude_zoom_pdf_embed_write(const fude_zoom_pdf* _pdf, u32 _k, const fude_zoom_pdf_embed* _e, fude_bytes* _out, u32 _base, u32* _offsets, u32 _obj) {
    const fude_zoom_mfont* _f   = &_pdf->fonts->fonts[_k].font;
    const b8               _cff = _f->cff != 0u;
    const f64              _em  = 1000.0 / (f64)_f->units_per_em;
    fude_zoom_pdf_object(_out, _base, _offsets, _obj);
    fude_zoom_pdf_put(_out, "<< /Type /Font /Subtype /Type0 /BaseFont /%s /Encoding /Identity-H", _e->name);
    fude_zoom_pdf_put(_out, " /DescendantFonts [%u 0 R] /ToUnicode %u 0 R >>\nendobj\n", (unsigned)(_obj + 1u), (unsigned)(_obj + 4u));

    fude_zoom_pdf_object(_out, _base, _offsets, _obj + 1u);
    fude_zoom_pdf_put(_out, "<< /Type /Font /Subtype /%s /BaseFont /%s", _cff ? "CIDFontType0" : "CIDFontType2", _e->name);
    fude_zoom_pdf_put(_out, " /CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> /FontDescriptor %u 0 R\n/W [", (unsigned)(_obj + 2u));
    const fude_zoom_pdf_glyph* _g = (const fude_zoom_pdf_glyph*)_e->glyphs.memory;
    const u32                  _n = (u32)rde_arr_length(&_e->glyphs);
    for(u32 _i = 0; _i < _n;) {
        u32 _j = _i + 1u;
        while(_j < _n && _g[_j].code == _g[_j - 1u].code + 1u) {
            _j++;
        }
        fude_zoom_pdf_put(_out, _i == 0u ? "%u [" : " %u [", (unsigned)_g[_i].code);
        for(u32 _m = _i; _m < _j; _m++) {
            if(_m > _i) {
                fude_zoom_pdf_put(_out, " ");
            }
            fude_zoom_pdf_put_real(_out, (f64)fude_zoom_mfont_advance(_f, _g[_m].glyph) * _em, FUDE_ZOOM_PDF_GLYPH);
        }
        fude_zoom_pdf_put(_out, "]");
        _i = _j;
    }
    fude_zoom_pdf_put(_out, "]%s >>\nendobj\n", _cff ? "" : " /CIDToGIDMap /Identity");

    u32       _post_len = 0, _os2_len = 0;
    const u32 _head = fude_zoom_mfont_table(_f, FUDE_ZOOM_PDF_TAG_HEAD, NULL);
    const u32 _post = fude_zoom_mfont_table(_f, FUDE_ZOOM_PDF_TAG_POST, &_post_len);
    const u32 _os2  = fude_zoom_mfont_table(_f, FUDE_ZOOM_PDF_TAG_OS2, &_os2_len);
    const f64 _slant  = _post != 0u && _post_len >= 8u ? (f64)(i32)fude_zoom_mfont_u32(_f, _post + 4u) / 65536.0 : 0.0;   // (16.16)
    const u32 _weight = _os2 != 0u && fude_zoom_mfont_u16(_f, _os2 + 4u) != 0u ? fude_zoom_mfont_u16(_f, _os2 + 4u) : 400u;
    // (italic: slanted, or so marked in head's macStyle (bit 1) or OS/2's fsSelection (bit 0), as Roboto Italic is with
    // a post slant of 0)
    const b8  _italic = _slant != 0.0 || (fude_zoom_mfont_u16(_f, _head + 44u) & 2u) != 0u ||
                       (_os2 != 0u && _os2_len >= 64u && (fude_zoom_mfont_u16(_f, _os2 + 62u) & 1u) != 0u);
    const i32 _cap    = _os2 != 0u && _os2_len >= 90u && fude_zoom_mfont_u16(_f, _os2) >= 2u ? (i16)fude_zoom_mfont_u16(_f, _os2 + 88u) : _f->ascender;
    i32 _v[7];   // its box (x min, y min, x max, y max), ascent, descent and cap height
    for(u32 _i = 0; _i < 4u; _i++) {
        _v[_i] = (i32)rde_math_round_to_i64_f64((f64)(i16)fude_zoom_mfont_u16(_f, _head + 36u + 2u * _i) * _em);
    }
    _v[4] = (i32)rde_math_round_to_i64_f64((f64)_f->ascender * _em);
    _v[5] = (i32)rde_math_round_to_i64_f64((f64)_f->descender * _em);
    _v[6] = (i32)rde_math_round_to_i64_f64((f64)_cap * _em);
    const f64 _stem = 50.0 + ((f64)_weight / 65.0) * ((f64)_weight / 65.0);
    fude_zoom_pdf_object(_out, _base, _offsets, _obj + 2u);
    fude_zoom_pdf_put(_out, "<< /Type /FontDescriptor /FontName /%s /Flags %u", _e->name, (unsigned)(_italic ? 4u + 64u : 4u));   // (symbolic; italic)
    fude_zoom_pdf_put(_out, " /FontBBox [%d %d %d %d] /ItalicAngle ", (int)_v[0], (int)_v[1], (int)_v[2], (int)_v[3]);
    fude_zoom_pdf_put_real(_out, _slant, FUDE_ZOOM_PDF_GLYPH);
    fude_zoom_pdf_put(_out, " /Ascent %d /Descent %d /CapHeight %d /StemV %d /%s %u 0 R >>\nendobj\n", (int)_v[4], (int)_v[5], (int)_v[6],
                      (int)rde_math_round_to_i64_f64(_stem), _cff ? "FontFile3" : "FontFile2", (unsigned)(_obj + 3u));

    fude_zoom_pdf_object(_out, _base, _offsets, _obj + 3u);
    if(_cff) {
        fude_zoom_pdf_put(_out, "<< /Subtype /CIDFontType0C ");
    } else {
        fude_zoom_pdf_put(_out, "<< /Length1 %u ", (unsigned)fude_bytes_size(&_e->program));
    }
    fude_zoom_pdf_stream(_out, _e->program.memory, fude_bytes_size(&_e->program));
    fude_zoom_pdf_object(_out, _base, _offsets, _obj + 4u);
    fude_zoom_pdf_put(_out, "<< ");
    fude_zoom_pdf_stream(_out, _e->unicode.memory, fude_bytes_size(&_e->unicode));
}

// Writes the resources' /Font dictionary: Helvetica's faces used as /F1 to /F4 (objects _face_obj), and each font
// embedded as /E<k + 1> (objects _font_obj, FUDE_ZOOM_PDF_FONTS of them); an object 0 is a font not used.
RDE_INTERNAL void fude_zoom_pdf_fonts_named(fude_bytes* _out, const u32* _face_obj, const u32* _font_obj) {
    fude_zoom_pdf_put(_out, "\n/Font <<");
    for(u32 _i = 0; _i < 4u; _i++) {
        if(_face_obj[_i] != 0u) {
            fude_zoom_pdf_put(_out, " /F%u %u 0 R", (unsigned)(_i + 1u), (unsigned)_face_obj[_i]);
        }
    }
    for(u32 _k = 0; _k < FUDE_ZOOM_PDF_FONTS; _k++) {
        if(_font_obj[_k] != 0u) {
            fude_zoom_pdf_put(_out, " /E%u %u 0 R", (unsigned)(_k + 1u), (unsigned)_font_obj[_k]);
        }
    }
    fude_zoom_pdf_put(_out, " >>");
}

// Writes the fonts' objects, as fude_zoom_pdf_fonts_named names them: Helvetica's faces used, then each font embedded
// from what _embed holds for it (fude_zoom_pdf_embed_make's), which is freed then.
RDE_INTERNAL void fude_zoom_pdf_fonts_write(const fude_zoom_pdf* _pdf, const u32* _face_obj, const u32* _font_obj, fude_zoom_pdf_embed* _embed, fude_bytes* _out, u32 _base,
                                            u32* _offsets) {
    static const c8* const _faces[4] = { "Helvetica", "Helvetica-Bold", "Helvetica-Oblique", "Helvetica-BoldOblique" };
    for(u32 _i = 0; _i < 4u; _i++) {
        if(_face_obj[_i] != 0u) {
            fude_zoom_pdf_object(_out, _base, _offsets, _face_obj[_i]);
            fude_zoom_pdf_put(_out, "<< /Type /Font /Subtype /Type1 /BaseFont /%s /Encoding /WinAnsiEncoding >>\nendobj\n", _faces[_i]);
        }
    }
    for(u32 _k = 0; _k < FUDE_ZOOM_PDF_FONTS; _k++) {
        if(_font_obj[_k] != 0u) {
            fude_zoom_pdf_embed_write(_pdf, _k, &_embed[_k], _out, _base, _offsets, _font_obj[_k]);
        }
        fude_zoom_pdf_embed_free(&_embed[_k]);
    }
}

// Writes the document's images (pictures _pic, _images of them) as their objects, _image_obj[i] each and its mask the
// object after it when it has one.
RDE_INTERNAL void fude_zoom_pdf_images_write(const fude_zoom_pdf* _pdf, const fude_zoom_pdf_picture* _pic, u32 _images, const u32* _image_obj, fude_bytes* _out, u32 _base,
                                             u32* _offsets) {
    static const c8* const _spaces[5] = { "", "/DeviceGray", "", "/DeviceRGB", "/DeviceCMYK" };
    for(u32 _i = 0; _i < _images; _i++) {
        const fude_zoom_pdf_picture* _p = &_pic[_i];
        fude_zoom_pdf_object(_out, _base, _offsets, _image_obj[_i]);
        fude_zoom_pdf_put(_out, "<< /Type /XObject /Subtype /Image /Width %u /Height %u /ColorSpace %s /BitsPerComponent 8",
                          (unsigned)_p->width, (unsigned)_p->height, _spaces[_p->components]);
        if(_p->jpeg) {
            fude_zoom_pdf_put(_out, " /Filter /DCTDecode");
        }
        if(_p->inverted) {
            fude_zoom_pdf_put(_out, " /Decode [1 0 1 0 1 0 1 0]");
        }
        if(_p->mask_size > 0u) {
            fude_zoom_pdf_put(_out, " /SMask %u 0 R", (unsigned)(_image_obj[_i] + 1u));
        }
        fude_zoom_pdf_put(_out, " ");
        fude_zoom_pdf_stream(_out, _pdf->data.memory + _p->at, _p->size);
        if(_p->mask_size > 0u) {
            fude_zoom_pdf_object(_out, _base, _offsets, _image_obj[_i] + 1u);
            fude_zoom_pdf_put(_out, "<< /Type /XObject /Subtype /Image /Width %u /Height %u /ColorSpace /DeviceGray /BitsPerComponent 8 ",
                              (unsigned)_p->width, (unsigned)_p->height);
            fude_zoom_pdf_stream(_out, _pdf->data.memory + _p->mask_at, _p->mask_size);
        }
    }
}

// See the header. The objects are numbered first (in the order at the top of this file; each font embedded is made
// then, as it is only then known whole), then written in that order, then the cross-reference table of where each
// starts, and the trailer. Nothing is written when _out could grow past 4 GB. The document is left as it was, so
// finishing it again writes the same file.
b8 fude_zoom_pdf_finish(fude_zoom_pdf* _pdf, fude_bytes* _out) {
    if(_pdf == NULL || _out == NULL) {
        return false;
    }
    if(_pdf->open) {
        fude_zoom_pdf_page_end(_pdf);
    }
    const u32 _pages  = (u32)rde_arr_length(&_pdf->pages);
    const u32 _images = (u32)rde_arr_length(&_pdf->images);
    if(_pages == 0u || _pdf->full) {
        return false;
    }
    const fude_zoom_pdf_page*    _page = (const fude_zoom_pdf_page*)_pdf->pages.memory;
    const fude_zoom_pdf_picture* _pic  = (const fude_zoom_pdf_picture*)_pdf->images.memory;
    u32 _states = 0, _masks = 0;
    for(u32 _i = 0; _i < 256u; _i++) {
        _states += (u32)_pdf->fill_alpha[_i] + (u32)_pdf->stroke_alpha[_i];
    }
    for(u32 _i = 0; _i < _images; _i++) {
        _masks += _pic[_i].mask_size > 0u ? 1u : 0u;
    }
    // Numbering: 1 catalog, 2 pages, 3 resources, then what they refer to.
    u32       _next  = 4u;
    // Each face of Helvetica used, an object (F1 the plain one: fude_zoom_pdf_text's).
    u32 _face_obj[4] = { 0u, 0u, 0u, 0u };
    for(u32 _i = 0; _i < 4u; _i++) {
        _face_obj[_i] = _pdf->faces[_i] ? _next++ : 0u;
    }
    // Each font of the set written in, five objects, made first: its program and maps are streams of their own.
    fude_zoom_pdf_embed _embed[FUDE_ZOOM_PDF_FONTS];
    u32                 _font_obj[FUDE_ZOOM_PDF_FONTS];
    u64                 _font_bytes = 0;
    b8                  _fonts      = _pdf->text;   // (a /Font dictionary in the resources)
    rde_memset(_embed, 0, sizeof(_embed));
    for(u32 _k = 0; _k < FUDE_ZOOM_PDF_FONTS; _k++) {
        _font_obj[_k] = fude_zoom_pdf_embed_make(_pdf, _k, &_embed[_k]) ? _next : 0u;
        if(_font_obj[_k] != 0u) {
            _next += 5u;
            _fonts = true;
            _font_bytes += (u64)fude_bytes_size(&_embed[_k].program) + (u64)fude_bytes_size(&_embed[_k].unicode) + 16u * rde_arr_length(&_embed[_k].glyphs);
        }
    }
    const u32 _state = _next;
    _next += _states;
    const u32 _image = _next;
    _next += _images + _masks;
    const u32 _first = _next;   // each page, then its content
    _next += 2u * _pages;
    const u32 _count = _next;
    // Everything but the streams (and the widths, 16 bytes a glyph at most) is small: a generous few hundred bytes an
    // object.
    const u64 _room = (u64)fude_bytes_size(&_pdf->content) + (u64)fude_bytes_size(&_pdf->data) + _font_bytes + (u64)_count * 400u + 1024u;
    if((u64)fude_bytes_size(_out) + _room > 0xFFFFFFFFu) {
        for(u32 _k = 0; _k < FUDE_ZOOM_PDF_FONTS; _k++) {
            fude_zoom_pdf_embed_free(&_embed[_k]);
        }
        return false;
    }
    rde_arr _offsets_arr = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_offsets_arr, (usize)_count + (usize)_images + 1u);
    u32* _offsets = (u32*)_offsets_arr.memory;   // (sized once: it stays put)
    u32* _image_obj = _offsets + _count;   // each image's object (its mask the next)
    for(u32 _i = 0, _n = _image; _i < _images; _i++) {
        _image_obj[_i] = _n;
        _n += _pic[_i].mask_size > 0u ? 2u : 1u;
    }
    const u32 _base = fude_bytes_size(_out);
    // The second line's high bytes tell transfer tools the file is binary.
    fude_put_data(_out, "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n", 15u);

    fude_zoom_pdf_object(_out, _base, _offsets, 1u);
    fude_zoom_pdf_put(_out, "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    fude_zoom_pdf_object(_out, _base, _offsets, 2u);
    fude_zoom_pdf_put(_out, "<< /Type /Pages /Kids [");
    for(u32 _i = 0; _i < _pages; _i++) {
        fude_zoom_pdf_put(_out, _i == 0u ? "%u 0 R" : " %u 0 R", (unsigned)(_first + 2u * _i));
    }
    fude_zoom_pdf_put(_out, "] /Count %u >>\nendobj\n", (unsigned)_pages);

    fude_zoom_pdf_object(_out, _base, _offsets, 3u);
    fude_zoom_pdf_put(_out, "<< /ProcSet [/PDF /Text /ImageB /ImageC /ImageI]");
    if(_states > 0u) {
        fude_zoom_pdf_put(_out, "\n/ExtGState <<");
        u32 _n = _state;
        for(u32 _k = 0; _k < 2u; _k++) {
            const b8* _used = _k == 0u ? _pdf->fill_alpha : _pdf->stroke_alpha;
            for(u32 _i = 0; _i < 256u; _i++) {
                if(_used[_i]) {
                    fude_zoom_pdf_put(_out, " /%s%u %u 0 R", _k == 0u ? "af" : "as", (unsigned)_i, (unsigned)_n++);
                }
            }
        }
        fude_zoom_pdf_put(_out, " >>");
    }
    if(_images > 0u) {
        fude_zoom_pdf_put(_out, "\n/XObject <<");
        for(u32 _i = 0; _i < _images; _i++) {
            fude_zoom_pdf_put(_out, " /Im%u %u 0 R", (unsigned)(_i + 1u), (unsigned)_image_obj[_i]);
        }
        fude_zoom_pdf_put(_out, " >>");
    }
    if(_fonts) {
        fude_zoom_pdf_fonts_named(_out, _face_obj, _font_obj);
    }
    fude_zoom_pdf_put(_out, " >>\nendobj\n");
    fude_zoom_pdf_fonts_write(_pdf, _face_obj, _font_obj, _embed, _out, _base, _offsets);

    u32 _n = _state;
    for(u32 _k = 0; _k < 2u; _k++) {
        const b8* _used = _k == 0u ? _pdf->fill_alpha : _pdf->stroke_alpha;
        for(u32 _i = 0; _i < 256u; _i++) {
            if(_used[_i]) {
                fude_zoom_pdf_object(_out, _base, _offsets, _n++);
                fude_zoom_pdf_put(_out, "<< /Type /ExtGState /%s ", _k == 0u ? "ca" : "CA");
                fude_zoom_pdf_put_real(_out, _i / 255.0, FUDE_ZOOM_PDF_COLOR);
                fude_zoom_pdf_put(_out, " >>\nendobj\n");
            }
        }
    }

    fude_zoom_pdf_images_write(_pdf, _pic, _images, _image_obj, _out, _base, _offsets);

    for(u32 _i = 0; _i < _pages; _i++) {
        const u32 _obj = _first + 2u * _i;
        fude_zoom_pdf_object(_out, _base, _offsets, _obj);
        fude_zoom_pdf_put(_out, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ");
        fude_zoom_pdf_put_real(_out, _page[_i].width, FUDE_ZOOM_PDF_PLACES);
        fude_zoom_pdf_put(_out, " ");
        fude_zoom_pdf_put_real(_out, _page[_i].height, FUDE_ZOOM_PDF_PLACES);
        fude_zoom_pdf_put(_out, "] /Resources 3 0 R /Contents %u 0 R >>\nendobj\n", (unsigned)(_obj + 1u));
        fude_zoom_pdf_object(_out, _base, _offsets, _obj + 1u);
        fude_zoom_pdf_put(_out, "<< ");
        fude_zoom_pdf_stream(_out, _pdf->content.memory + _page[_i].at, _page[_i].size);
    }

    // Each entry exactly 20 bytes, its end of line two of them.
    const u32 _xref = fude_bytes_size(_out) - _base;
    fude_zoom_pdf_put(_out, "xref\n0 %u\n0000000000 65535 f\r\n", (unsigned)_count);
    for(u32 _i = 1u; _i < _count; _i++) {
        fude_zoom_pdf_put(_out, "%010u 00000 n\r\n", (unsigned)_offsets[_i]);
    }
    fude_zoom_pdf_put(_out, "trailer\n<< /Size %u /Root 1 0 R >>\nstartxref\n%u\n%%%%EOF\n", (unsigned)_count, (unsigned)_xref);
    rde_arr_free(&_offsets_arr);
    return true;
}
