// RDE's 2D physics (its Box2D wrapper) built into a suite of the tests'. RDE compiles it within its engine's one
// translation unit; here, with what of the engine it uses — its allocator, its thread pool (never asked for: one
// worker) — given as the rest of tests/support gives them.

#include "rde.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

any rde_aligned_alloc(usize _bytes_count, usize _align) {
    void* _p = NULL;
    return posix_memalign(&_p, _align < sizeof(void*) ? sizeof(void*) : _align, _bytes_count > 0u ? _bytes_count : 1u) == 0 ? _p : NULL;
}
void rde_aligned_free(any _memory) { free(_memory); }
void rde_memset(any _dst, i32 _value, usize _dst_len) { memset(_dst, _value, _dst_len); }
void (rde_assert_fatal)(b8 _condition, const c8* _fmt, ...) {
    if(_condition) {
        return;
    }
    va_list _args;
    va_start(_args, _fmt);
    vfprintf(stderr, _fmt, _args);
    va_end(_args);
    fputc('\n', stderr);
    abort();
}
rde_thread_pool* rde_thread_pool_create(u32 _thread_count, usize _tasks_size) { RDE_UNUSED(_thread_count); RDE_UNUSED(_tasks_size); return NULL; }
b8 rde_thread_pool_run(rde_thread_pool* _pool, rde_thread_task_fn _fn, any _args) { RDE_UNUSED(_pool); RDE_UNUSED(_fn); RDE_UNUSED(_args); return false; }
b8 rde_thread_pool_destroy(rde_thread_pool* _pool) { RDE_UNUSED(_pool); return true; }

RDE_INTERNAL rde_memory_allocator* _rde_memory_get_valid_allocator(rde_memory_allocator* _allocator) {
    return _allocator != NULL ? _allocator : rde_memory_allocator_get_default_std();
}

// (the job system's: never used with one worker)
typedef struct SDL_Semaphore SDL_Semaphore;
static SDL_Semaphore* SDL_CreateSemaphore(u32 _value) { RDE_UNUSED(_value); return NULL; }
static void SDL_DestroySemaphore(SDL_Semaphore* _s) { RDE_UNUSED(_s); }
static void SDL_SignalSemaphore(SDL_Semaphore* _s) { RDE_UNUSED(_s); }
static void SDL_WaitSemaphore(SDL_Semaphore* _s) { RDE_UNUSED(_s); }

#include "rde_physics_jobs.c"
#include "rde_physics_2d.c"
