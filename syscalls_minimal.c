/*
 * Minimal _sbrk implementation for STM32F103 with ChibiOS
 *
 * Provides just enough heap management for newlib without bloat.
 * This is used ONLY if something internally tries to allocate memory.
 *
 * Also stubs out newlib's timezone machinery to prevent ~20KB of bloat
 * when hal_rtc.h includes <time.h>.
 */

#include <sys/types.h>
#include <errno.h>

/* STM32F103xB RAM Configuration */
#define RAM_START   0x20000000  /* SRAM start address */
#define RAM_SIZE    0x5000      /* 20KB SRAM */
#define RAM_END     (RAM_START + RAM_SIZE)

/* Conservative heap start */
#define HEAP_START  0x20001000

static char *heap_end = 0;

/*
 * _sbrk - Allocate memory from heap
 * incr: bytes to allocate
 * Returns: previous heap end on success, (char*)-1 on failure
 */
char *_sbrk(int incr) {
    char *prev_heap_end;

    if (heap_end == 0) {
        heap_end = (char *)HEAP_START;
    }

    prev_heap_end = heap_end;

    if ((heap_end + incr) > ((char *)RAM_END - 512)) {
        errno = ENOMEM;
        return (char *)-1;
    }

    heap_end += incr;
    return prev_heap_end;
}

/*
 * Stub out newlib's timezone machinery
 *
 * When hal_rtc.h includes <time.h>, newlib links in:
 *   - lcltime_r.o
 *   - mktime.o
 *   - tzset.o
 *   - tzvars.o
 *   - tzlock.o
 *   - month_lengths.o
 *   - tzcalc_limits.o
 *
 * These are only called if you use localtime(), mktime(), or tzset().
 * By providing empty stubs, we prevent linking the entire timezone database.
 */

/* Weak references - will be overridden if user code calls these */
extern void __tzset_unlocked(void);
extern void _tzset_unlocked_r(struct _reent *);
extern void __tz_lock(void);
extern void __tz_unlock(void);

void __tzset_unlocked(void) {
    /* Stub: no timezone support needed */
}

void _tzset_unlocked_r(struct _reent *r) {
    (void)r;
    /* Stub: no timezone support needed */
}

void __tz_lock(void) {
    /* Stub: no timezone support needed */
}

void __tz_unlock(void) {
    /* Stub: no timezone support needed */
}
