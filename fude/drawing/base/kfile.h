// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_KFILE
#define FUDE_KFILE

#include "rde.h"

// ===========================================================================
// The KANA file format: one tagged binary layout for everything the apps write (the page and settings saves of
// save.h, the notes' index, a document's read text) and for the baked character data (bake.h, kanji.h). Also the
// helpers that read and write a whole file.
//
// Where it sits: drawing/base. Every module that saves a file builds it with fude_bytes and reads it back with
// fude_reader; save.h decides where files go and how a damaged one is recovered.
// Its main types and their lifecycle: fude_bytes is a file being built, made by fude_bytes_new and freed by
// fude_bytes_write_and_free. fude_reader is only a view over bytes the caller owns; it allocates nothing.
// Threads: main thread. The writers and the reader keep no state beyond the buffer they are handed.
// Files: the format itself:
//   header   "KANA"  u32 version  u32 kind                        (12 bytes)
//   chunk*   u32 tag  u32 size  payload[size]
// Every number is little-endian and written byte by byte, so the format does not depend on the machine; floats are
// written as their IEEE-754 bits, and 64-bit numbers as two 32-bit halves, the low one first. What each kind holds is
// documented by its owner.
// Design: readers SKIP chunk tags they do not know, and read only the fields a chunk has. So new data goes in new
// chunks or at the END of a record, and old and new builds keep reading each other's files.
// Writes are ATOMIC (fude_bytes_write_and_free): the bytes go to <name>.tmp, then the previous file is renamed to
// <name>.bak, then the .tmp is renamed into place. A kill at any point leaves the old file or the new one, never half
// of one.
// ===========================================================================

// A chunk tag or a file kind, from its four characters in the order they appear in the file.
#define FUDE_TAG(_a, _b, _c, _d) ((u32)(u8)(_a) | ((u32)(u8)(_b) << 8) | ((u32)(u8)(_c) << 16) | ((u32)(u8)(_d) << 24))

// The header's size in bytes: "KANA", the version and the kind.
#define FUDE_FILE_HEADER_SIZE 12u

// --- writing -------------------------------------------------------------------
//
// A file is built in memory (an rde_arr of bytes) and written in one go.

// A file being built: its bytes so far.
typedef rde_arr TYPE(u8) fude_bytes;

// Makes an empty file with room for _capacity bytes; it grows as needed. It is on the standard heap, because some
// files (a page) are as big as the user makes them. fude_bytes_size returns how many bytes it holds so far.
fude_bytes fude_bytes_new(u32 _capacity);
u32        fude_bytes_size(const fude_bytes* _b);

// Append one value to _b, as the format above says: little-endian, floats as their bits, 64-bit numbers as two
// halves, a colour as r, g, b, a. fude_put_data appends _size raw bytes; fude_put_header writes the 12-byte header
// ("KANA", _version, _kind).
void       fude_put_u8(fude_bytes* _b, u8 _v);
void       fude_put_u16(fude_bytes* _b, u16 _v);
void       fude_put_i16(fude_bytes* _b, i16 _v);
void       fude_put_u32(fude_bytes* _b, u32 _v);
void       fude_put_u64(fude_bytes* _b, u64 _v);
void       fude_put_f32(fude_bytes* _b, f32 _v);
void       fude_put_f64(fude_bytes* _b, f64 _v);
void       fude_put_color(fude_bytes* _b, rde_color _c);
void       fude_put_data(fude_bytes* _b, const any _data, u32 _size);
void       fude_put_header(fude_bytes* _b, u32 _version, u32 _kind);

// fude_chunk_begin writes a chunk's tag and room for its size, and returns where the size goes; fude_chunk_end fills
// the size in from what was written since.
u32        fude_chunk_begin(fude_bytes* _b, u32 _tag);
void       fude_chunk_end(fude_bytes* _b, u32 _at);

// Writes the bytes to _path atomically (see the top), then frees them whatever happened. _out_bytes (may be NULL)
// gets the size. Returns false, and logs it, when the file could not be written or put in place; the previous file
// is then still there.
b8         fude_bytes_write_and_free(fude_bytes* _b, const c8* _path, u32* _out_bytes);

// --- reading -------------------------------------------------------------------

// A reader over bytes the caller owns. Reading past the end gives zeros and turns ok false for good, so a parser can
// read a whole record and check ok once.
typedef struct {
    const u8* data;
    u32       size;
    // Where the next read starts.
    u32       pos;
    b8        ok;     // false once anything was read past the end
} fude_reader;

// fude_reader_make makes a reader over _size bytes of _data (a NULL _data gives one that is already not ok).
// fude_reader_has returns whether _n more bytes are there, and turns ok false when they are not. The fude_get_
// functions read one value back as fude_put_ wrote it; past the end they return 0 and turn ok false.
fude_reader fude_reader_make(const u8* _data, u32 _size);
b8          fude_reader_has(fude_reader* _r, u32 _n);
u8          fude_get_u8(fude_reader* _r);
u16         fude_get_u16(fude_reader* _r);
i16         fude_get_i16(fude_reader* _r);
u32         fude_get_u32(fude_reader* _r);
u64         fude_get_u64(fude_reader* _r);
f32         fude_get_f32(fude_reader* _r);
f64         fude_get_f64(fude_reader* _r);
rde_color   fude_get_color(fude_reader* _r);

// Reads the header at the start of the data. Returns true when it says "KANA", a version from 1 to _max_version, and
// the expected _kind.
b8          fude_read_header(fude_reader* _r, u32 _max_version, u32 _kind);

// Reads the next chunk after the header: its tag into *_tag, and into *_chunk a reader over ONLY its payload, so a
// chunk shorter or longer than expected can never run into the next. Returns false at the end of the data, or when
// a size runs past it (then _file->ok is false).
b8          fude_next_chunk(fude_reader* _file, u32* _tag, fude_reader* _chunk);

// Reads a whole file into new memory, *_size getting its size. Returns NULL, *_size 0, when it cannot be read or is
// 4 GiB or more. The memory is on the standard heap, because files are as big as the user made them (a page, the
// character data), more than the engine's fixed pool is sized for; free it with fude_file_free (NULL is fine).
u8*         fude_file_read(const c8* _path, u32* _size);
void        fude_file_free(u8* _data);
// Reads a whole file into _out, a byte array the caller made and keeps (it is cleared first). Returns true when the
// file was read whole, false when it could not be (then _out is empty). Fit for a file of any size: the memory is on
// whatever allocator _out was made with (the std one for large files, never the engine's pool).
b8          fude_file_read_into(const c8* _path, rde_arr* _out);
// Returns the file's size in bytes, or 0 when it does not exist or cannot be opened.
u64         fude_file_size(const c8* _path);
// Reads the file's first _size - 1 bytes into _out, NUL-terminated: a short text, such as a sample a developer's look
// reads. Returns how many bytes it read; 0, with _out empty, when the file cannot be read.
usize       fude_file_read_text(const c8* _path, c8* _out, usize _size);
// Writes _size bytes as the whole file at _path, replacing any. Returns true when it was written whole. On failure the
// partial file is deleted, so a reader never finds half a file. Unlike fude_bytes_write_and_free, it keeps no .bak.
b8          fude_file_write(const c8* _path, const any _data, usize _size);

// Renames a file that could not be parsed to <name>.bad (and logs it), so nothing overwrites it; when that is taken,
// <name>.2.bad and on, up to FUDE_FILE_ASIDE_MAX (past it, the last is replaced: a file damaged that often holds
// nothing new).
void        fude_file_set_aside(const c8* _path);
#define FUDE_FILE_ASIDE_MAX 100u   // The most numbered copies of one damaged file (fude_file_set_aside).

#endif
