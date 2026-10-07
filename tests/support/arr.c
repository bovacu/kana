// Stand-in for the engine's rde_arr + allocators, SAME semantics and asserts as
// engine/src/rde_stl.c (growth when count + n >= capacity, doubling; at/length/
// clear/free assert an inited array; at asserts the index).
#include "rde.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <execinfo.h>
#define STUB_ASSERT(c) do { if(!(c)) { fprintf(stderr, "STUB ASSERT %s:%d %s\n", __FILE__, __LINE__, #c); void* _bt[16]; backtrace_symbols_fd(_bt, backtrace(_bt, 16), 2); abort(); } } while(0)
static any s_malloc(any a, usize n) { (void)a; return malloc(n); }
static any s_calloc(any a, usize c, usize s) { (void)a; return calloc(c, s); }
static any s_realloc(any a, any p, usize n) { (void)a; return realloc(p, n); }
static void s_free(any a, any p) { (void)a; free(p); }
static rde_memory_allocator g_std = { .malloc = s_malloc, .calloc = s_calloc, .realloc = s_realloc, .free = s_free };
rde_memory_allocator* rde_memory_allocator_get_default_std(void) { return &g_std; }
rde_memory_allocator* rde_memory_allocator_get_default(void) { return &g_std; }
any rde_malloc(usize n) { any p = malloc(n); STUB_ASSERT(p); return p; }
any rde_realloc(any m, usize n) { any r = realloc(m, n); STUB_ASSERT(r); return r; }
void rde_free(any m) { free(m); }
f32 rde_math_clamp_f32(f32 v, f32 lo, f32 hi) { return v < lo ? lo : (v > hi ? hi : v); }
rde_arr rde_arr_new_with_capacity(usize t, usize c, rde_memory_allocator* a) {
    STUB_ASSERT(t > 0 && c > 0); if(!a) a = &g_std;
    return (rde_arr){ .memory = a->calloc(a->allocator, c, t), .capacity = (u32)c, .count = 0, .data_type_size = t, .allocator = a };
}
rde_arr rde_arr_new(usize t, rde_memory_allocator* a) { return rde_arr_new_with_capacity(t, 16, a); }
b8 rde_arr_is_inited(const rde_arr* a) { return a != NULL && a->memory != NULL && a->data_type_size > 0; }
static void grow(rde_arr* a, usize need) {
    if(need < a->capacity) return;
    usize cap = a->capacity > 0 ? a->capacity : 1; while(need >= cap) cap *= 2;
    a->memory = a->allocator->realloc(a->allocator->allocator, a->memory, cap * a->data_type_size); STUB_ASSERT(a->memory); a->capacity = (u32)cap;
}
any rde_arr_add(rde_arr* a, const any item) {
    STUB_ASSERT(rde_arr_is_inited(a)); grow(a, a->count + 1);
    u8* d = a->memory + a->count * a->data_type_size;
    if(item) memcpy(d, item, a->data_type_size); else memset(d, 0, a->data_type_size);
    a->count++; return d;
}
any rde_arr_add_n(rde_arr* a, usize n) {
    STUB_ASSERT(rde_arr_is_inited(a) && n > 0); grow(a, a->count + n);
    u8* d = a->memory + a->count * a->data_type_size; a->count += (u32)n; return d;
}
void rde_arr_resize(rde_arr* a, usize n) {
    STUB_ASSERT(rde_arr_is_inited(a));
    if(n <= a->count) { a->count = (u32)n; return; }
    const usize was = a->count; u8* d = rde_arr_add_n(a, n - was); memset(d, 0, (n - was) * a->data_type_size);
}
any rde_arr_at(const rde_arr* a, usize i) { STUB_ASSERT(rde_arr_is_inited(a) && a->count > i); return &a->memory[i * a->data_type_size]; }
usize rde_arr_length(const rde_arr* a) { STUB_ASSERT(rde_arr_is_inited(a)); return a->count; }
void rde_arr_clear(rde_arr* a) { STUB_ASSERT(rde_arr_is_inited(a)); a->count = 0; }
void rde_arr_free(rde_arr* a) { STUB_ASSERT(rde_arr_is_inited(a)); a->allocator->free(a->allocator->allocator, a->memory); memset(a, 0, sizeof(*a)); }
any rde_arr_insert(rde_arr* a, usize i, const any item) {
    STUB_ASSERT(rde_arr_is_inited(a) && a->count >= i); rde_arr_add(a, item);
    memmove(a->memory + (i + 1) * a->data_type_size, a->memory + i * a->data_type_size, (a->count - 1 - i) * a->data_type_size);
    u8* d = a->memory + i * a->data_type_size; if(item) memcpy(d, item, a->data_type_size); else memset(d, 0, a->data_type_size); return d;
}
void rde_arr_remove(rde_arr* a, usize i) {
    STUB_ASSERT(rde_arr_is_inited(a) && a->count > i);
    memmove(a->memory + i * a->data_type_size, a->memory + (i + 1) * a->data_type_size, (a->count - i - 1) * a->data_type_size); a->count--;
}

// rde_str: the part fude uses, as engine/src/rde_stl.c does it (NUL-ended, capacity a power of two, 16 at least).
rde_str rde_str_new(const c8* t, rde_memory_allocator* a) {
    if(!a) a = &g_std;
    const usize n = t ? strlen(t) : 0; usize cap = 16; while(cap < n + 1) cap *= 2;
    rde_str r; memset(&r, 0, sizeof r); r.allocator = a; r.capacity = (u32)cap; r.length = (u32)n;
    r.str = a->calloc(a->allocator, cap, 1); STUB_ASSERT(r.str); if(n) memcpy(r.str, t, n); return r;
}
usize rde_str_length(const rde_str* s) { STUB_ASSERT(s && s->str); return s->length; }
const c8* rde_str_to_char_ptr(const rde_str* s) { STUB_ASSERT(s && s->str); return s->str; }
void rde_str_append_str(rde_str* s, const c8* t) {
    STUB_ASSERT(s && s->str && t); const usize n = strlen(t); usize cap = s->capacity;
    while(cap < s->length + n + 1) cap *= 2;
    if(cap != s->capacity) { s->str = s->allocator->realloc(s->allocator->allocator, s->str, cap); STUB_ASSERT(s->str); s->capacity = (u32)cap; }
    memcpy(s->str + s->length, t, n); s->length += (u32)n; s->str[s->length] = 0; s->dirty = true;
}
void rde_str_clear(rde_str* s) { STUB_ASSERT(s && s->str); memset(s->str, 0, s->capacity); s->length = 0; s->hash = 0; s->dirty = false; }
void rde_str_free(rde_str* s) { STUB_ASSERT(s && s->str); s->allocator->free(s->allocator->allocator, s->str); memset(s, 0, sizeof(*s)); }

// rde_hash_map: open addressing with linear probing, as engine/src/rde_stl.c does it (add refuses a key that is
// there, find gives a pointer to the value or NULL, update_entry overwrites one, a NULL value is zeroes; the table
// starts at RDE_HASH_MAP_DEFAULT_CAPACITY and goes to the next power of two past count + count / 3). Each entry: a
// used flag, the key, the value, each 8-aligned (the u64 compare reads keys in place). No remove: nothing uses it.
#define HM_AL(n) (((usize)(n) + 7u) & ~(usize)7u)
static usize hm_stride(const rde_hash_map* m) { return 8u + HM_AL(m->key_bytes_size) + HM_AL(m->value_bytes_size); }
static usize hm_index(const rde_hash_map* m, const any k) {
    u64 h = m->hash_fn(k); h ^= h >> 33; h *= 0xff51afd7ed558ccdull; h ^= h >> 33; return (usize)h & (m->capacity - 1);
}
static void hm_copy(u8* d, const any s, usize n) { if(s) memcpy(d, s, n); else memset(d, 0, n); }
static void hm_put(rde_hash_map* m, const any k, const any v) {
    usize i = hm_index(m, k); u8* e; while((e = m->memory + i * hm_stride(m))[0]) i = (i + 1) & (m->capacity - 1);
    e[0] = 1; memcpy(e + 8, k, m->key_bytes_size); hm_copy(e + 8 + HM_AL(m->key_bytes_size), v, m->value_bytes_size); m->count++;
}
u64 rde_hash_map_fn_u64_hash(const any k) { return *(const u64*)k; }
i32 rde_hash_map_fn_u64_cmp(const any a, const any b) { return *(const u64*)a == *(const u64*)b ? 0 : -1; }
rde_hash_map rde_hash_map_new(usize ks, usize vs, rde_hash_map_hash_fn h, rde_hash_map_cmp_fn c, rde_hash_map_key_to_str_fn s, rde_memory_allocator* a) {
    STUB_ASSERT(ks > 0 && vs > 0 && h && c); if(!a) a = &g_std;
    rde_hash_map m; memset(&m, 0, sizeof m); m.initial_capacity = m.capacity = RDE_HASH_MAP_DEFAULT_CAPACITY;
    m.key_bytes_size = ks; m.value_bytes_size = vs; m.hash_fn = h; m.cmp_fn = c; m.key_str_fn = s; m.allocator = a;
    m.memory = a->calloc(a->allocator, m.capacity, hm_stride(&m)); STUB_ASSERT(m.memory); return m;
}
any rde_hash_map_find(const rde_hash_map* m, const any k) {
    STUB_ASSERT(m && m->memory && k);
    for(usize i = hm_index(m, k), n = 0; n < m->capacity; i = (i + 1) & (m->capacity - 1), n++) {
        u8* e = m->memory + i * hm_stride(m); if(!e[0]) return NULL;
        if(m->cmp_fn(k, e + 8) == 0) return e + 8 + HM_AL(m->key_bytes_size);
    }
    return NULL;
}
b8 rde_hash_map_add(rde_hash_map* m, const any k, const any v) {
    STUB_ASSERT(m && k); if(rde_hash_map_find(m, k)) return false;
    const usize want = m->count + m->count / 3;
    if(want > m->capacity) {
        usize cap = m->capacity; while(cap < want) cap *= 2;
        u8* old = m->memory; const usize was = m->capacity, st = hm_stride(m);
        m->memory = m->allocator->calloc(m->allocator->allocator, cap, st); STUB_ASSERT(m->memory); m->capacity = (u32)cap; m->count = 0;
        for(usize i = 0; i < was; i++) { u8* e = old + i * st; if(e[0]) hm_put(m, e + 8, e + 8 + HM_AL(m->key_bytes_size)); }
        m->allocator->free(m->allocator->allocator, old);
    }
    hm_put(m, k, v); return true;
}
b8 rde_hash_map_update_entry(const rde_hash_map* m, const any k, const any v) {
    u8* d = rde_hash_map_find(m, k); if(!d) return false; hm_copy(d, v, m->value_bytes_size); return true;
}
usize rde_hash_map_length(const rde_hash_map* m) { STUB_ASSERT(m); return m->count; }
void rde_hash_map_clear(rde_hash_map* m) { STUB_ASSERT(m); if(m->memory) memset(m->memory, 0, m->capacity * hm_stride(m)); m->count = 0; }
void rde_hash_map_free(rde_hash_map* m) { STUB_ASSERT(m); if(m->memory) m->allocator->free(m->allocator->allocator, m->memory); memset(m, 0, sizeof(*m)); }
b8 rde_str_is_inited(const rde_str* s) { STUB_ASSERT(s); return s->str != NULL; }
void rde_arr_add_arr(rde_arr* a, const rde_arr* b) {
    STUB_ASSERT(rde_arr_is_inited(a) && rde_arr_is_inited(b) && a->data_type_size == b->data_type_size);
    if(b->count == 0) return;
    u8* d = rde_arr_add_n(a, b->count); memcpy(d, b->memory, (usize)b->count * a->data_type_size);
}
// rde_hash_set: a map of keys to a byte, as the engine's.
rde_hash_set rde_hash_set_new(usize ks, rde_hash_map_hash_fn h, rde_hash_map_cmp_fn c, rde_hash_map_key_to_str_fn s, rde_memory_allocator* a) {
    rde_hash_set r; r.map = rde_hash_map_new(ks, 1, h, c, s, a); return r;
}
b8 rde_hash_set_add(rde_hash_set* t, const any k) { const u8 one = 1; return rde_hash_map_add(&t->map, k, (any)&one); }
b8 rde_hash_set_contains(const rde_hash_set* t, const any k) { return rde_hash_map_find(&t->map, k) != NULL; }
usize rde_hash_set_length(const rde_hash_set* t) { return rde_hash_map_length(&t->map); }
void rde_hash_set_free(rde_hash_set* t) { rde_hash_map_free(&t->map); }
