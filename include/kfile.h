#ifndef KANA_KFILE
#define KANA_KFILE

#include "rde.h"

// ===========================================================================
// The KANA file format — one tagged binary layout for everything Kana writes:
// the page and settings saves (save.h) and the baked character data (bake.h,
// kanji.h).
//
//   header   "KANA"  u32 version  u32 kind                        (12 bytes)
//   chunk*   u32 tag  u32 size  payload[size]
//
// Every number is little-endian and written byte by byte, so the format does not
// depend on the machine; floats as their IEEE-754 bits. Readers SKIP chunk tags
// they do not know and read only the fields a chunk has, so new data goes in new
// chunks or at the END of a record, and old and new builds keep reading each
// other's files. What each kind holds is documented by its owner.
//
// Writes are ATOMIC (kana_bytes_write_and_free): <name>.tmp, then the previous
// file renamed to <name>.bak, then .tmp renamed into place. A kill at any point
// leaves the old file or the new one, never half of one.
// ===========================================================================

#define KANA_TAG(_a, _b, _c, _d) ((u32)(u8)(_a) | ((u32)(u8)(_b) << 8) | ((u32)(u8)(_c) << 16) | ((u32)(u8)(_d) << 24))

#define KANA_FILE_HEADER_SIZE 12u

// --- writing -------------------------------------------------------------------
//
// A file is built in memory (an rde_arr of bytes) and written in one go.

typedef rde_arr TYPE(u8) kana_bytes;

// On the standard heap: some of these (a page) are as big as the user makes them.
kana_bytes kana_bytes_new(u32 _capacity);
u32        kana_bytes_size(const kana_bytes* _b);

void       kana_put_u8(kana_bytes* _b, u8 _v);
void       kana_put_u16(kana_bytes* _b, u16 _v);
void       kana_put_i16(kana_bytes* _b, i16 _v);
void       kana_put_u32(kana_bytes* _b, u32 _v);
void       kana_put_f32(kana_bytes* _b, f32 _v);
void       kana_put_color(kana_bytes* _b, rde_color _c);
void       kana_put_data(kana_bytes* _b, const void* _data, u32 _size);
void       kana_put_header(kana_bytes* _b, u32 _version, u32 _kind);

// Returns where the chunk's size goes; kana_chunk_end fills it in.
u32        kana_chunk_begin(kana_bytes* _b, u32 _tag);
void       kana_chunk_end(kana_bytes* _b, u32 _at);

// Writes the bytes to _path atomically (see the header) and frees them.
// _out_bytes (may be NULL) receives the size.
b8         kana_bytes_write_and_free(kana_bytes* _b, const c8* _path, u32* _out_bytes);

// --- reading -------------------------------------------------------------------

typedef struct {
    const u8* data;
    u32       size;
    u32       pos;
    b8        ok;     // false once anything read past the end
} kana_reader;

kana_reader kana_reader_make(const u8* _data, u32 _size);
b8          kana_reader_has(kana_reader* _r, u32 _n);
u8          kana_get_u8(kana_reader* _r);
u16         kana_get_u16(kana_reader* _r);
i16         kana_get_i16(kana_reader* _r);
u32         kana_get_u32(kana_reader* _r);
f32         kana_get_f32(kana_reader* _r);
rde_color   kana_get_color(kana_reader* _r);

// "KANA", a version from 1 to _max_version, and the expected kind.
b8          kana_read_header(kana_reader* _r, u32 _max_version, u32 _kind);

// The next chunk after the header: its tag, and a reader over ONLY its payload,
// so a short or long chunk can never run into the next. False at the end, or
// when a size runs past the file (then _file->ok is false).
b8          kana_next_chunk(kana_reader* _file, u32* _tag, kana_reader* _chunk);

// The whole file, or NULL when it cannot be read. Free with kana_file_free.
u8*         kana_file_read(const c8* _path, u32* _size);
void        kana_file_free(u8* _data);

// Keeps a file that did not parse as <name>.bad, so nothing overwrites it.
void        kana_file_set_aside(const c8* _path);

#endif
