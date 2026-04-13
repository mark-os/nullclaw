/**
 * ARMv5TE atomic builtins shim.
 *
 * ARMv5TE lacks hardware atomic instructions (ldrex/strex require ARMv6+).
 * Linux provides kernel-space helpers at fixed user-accessible addresses:
 *
 *   __kuser_memory_barrier @ 0xffff0fa0  — full memory barrier
 *   __kuser_cmpxchg        @ 0xffff0fc0  — atomic compare-and-swap (32-bit)
 *
 * These are part of the Linux ARM ABI and available on all ARM kernels.
 * See: Documentation/arch/arm/kernel_user_helpers.rst
 *
 * This file implements the GCC __sync_* builtins that LLVM emits for
 * atomic operations but cannot lower to ARMv5 instructions.
 *
 * We use asm labels to export the correct symbol names without Clang
 * rejecting them as builtin redeclarations.
 */

typedef unsigned char      u8;
typedef unsigned int       u32;

/* Kernel user helpers — fixed addresses in every ARM Linux process. */
typedef void (*kuser_memory_barrier_t)(void);
typedef int  (*kuser_cmpxchg_t)(u32 oldval, u32 newval, volatile u32 *ptr);

#define __kuser_memory_barrier (*(kuser_memory_barrier_t)0xffff0fa0)
#define __kuser_cmpxchg        (*(kuser_cmpxchg_t)0xffff0fc0)
/* __kuser_cmpxchg returns 0 on success, non-zero on failure. */

/* ── Memory barrier ─────────────────────────────────────────────── */

void armv5_sync_synchronize(void) __asm__("__sync_synchronize");
void armv5_sync_synchronize(void) {
    __kuser_memory_barrier();
}

/* ── 32-bit atomics ─────────────────────────────────────────────── */

u32 armv5_sync_val_compare_and_swap_4(volatile u32 *ptr, u32 oldval, u32 newval)
    __asm__("__sync_val_compare_and_swap_4");
u32 armv5_sync_val_compare_and_swap_4(volatile u32 *ptr, u32 oldval, u32 newval) {
    u32 prev;
    do {
        prev = *ptr;
        if (prev != oldval) return prev;
    } while (__kuser_cmpxchg(prev, newval, ptr) != 0);
    return oldval;
}

u32 armv5_sync_lock_test_and_set_4(volatile u32 *ptr, u32 val)
    __asm__("__sync_lock_test_and_set_4");
u32 armv5_sync_lock_test_and_set_4(volatile u32 *ptr, u32 val) {
    u32 prev;
    do {
        prev = *ptr;
    } while (__kuser_cmpxchg(prev, val, ptr) != 0);
    return prev;
}

u32 armv5_sync_fetch_and_add_4(volatile u32 *ptr, u32 val)
    __asm__("__sync_fetch_and_add_4");
u32 armv5_sync_fetch_and_add_4(volatile u32 *ptr, u32 val) {
    u32 prev;
    do {
        prev = *ptr;
    } while (__kuser_cmpxchg(prev, prev + val, ptr) != 0);
    return prev;
}

/* ── 8-bit atomics (emulated via 32-bit CAS on containing word) ── */

static inline u8 byte_swap(volatile u8 *ptr, u8 newval) {
    volatile u32 *word_ptr = (volatile u32 *)((unsigned long)ptr & ~3UL);
    unsigned shift = ((unsigned long)ptr & 3) * 8;
    u32 mask = 0xFFu << shift;
    u32 prev_word, word_new;
    do {
        prev_word = *word_ptr;
        word_new = (prev_word & ~mask) | ((u32)newval << shift);
    } while (__kuser_cmpxchg(prev_word, word_new, word_ptr) != 0);
    return (u8)((prev_word >> shift) & 0xFF);
}

u8 armv5_sync_val_compare_and_swap_1(volatile u8 *ptr, u8 oldval, u8 newval)
    __asm__("__sync_val_compare_and_swap_1");
u8 armv5_sync_val_compare_and_swap_1(volatile u8 *ptr, u8 oldval, u8 newval) {
    volatile u32 *word_ptr = (volatile u32 *)((unsigned long)ptr & ~3UL);
    unsigned shift = ((unsigned long)ptr & 3) * 8;
    u32 mask = 0xFFu << shift;
    u32 prev_word, word_old, word_new;
    u8 prev;
    do {
        prev_word = *word_ptr;
        prev = (u8)((prev_word >> shift) & 0xFF);
        if (prev != oldval) return prev;
        word_old = prev_word;
        word_new = (prev_word & ~mask) | ((u32)newval << shift);
    } while (__kuser_cmpxchg(word_old, word_new, word_ptr) != 0);
    return oldval;
}

u8 armv5_sync_lock_test_and_set_1(volatile u8 *ptr, u8 val)
    __asm__("__sync_lock_test_and_set_1");
u8 armv5_sync_lock_test_and_set_1(volatile u8 *ptr, u8 val) {
    return byte_swap(ptr, val);
}
