/**
 * @file syscall_mem.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-10-24
 *
 * @copyright Copyright (c) 2023
 *
 */
#include <FreeRTOS.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include "SEGGER_RTT.h"

#ifdef __CC_ARM
/* avoid the heap and heap-using library functions supplied by arm */
#pragma import(__use_no_heap)
#endif /* __CC_ARM */

void* malloc(size_t n)
{
    return pvPortMalloc(n);
}

void* realloc(void* rmem, size_t n)
{
    return pvPortRealloc(rmem, n);
}

void* calloc(size_t nelem, size_t elsize)
{
    void* p;

    /* allocate 'count' objects of size 'size' */
    p = pvPortMalloc(nelem * elsize);
    /* zero the memory */
    if (p) {
        memset(p, 0, nelem * elsize);
    }
    return p;
}

void free(void* rmem)
{
    vPortFree(rmem);
}
