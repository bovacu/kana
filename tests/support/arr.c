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
