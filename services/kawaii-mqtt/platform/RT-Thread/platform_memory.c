/*
 * @Author: jiejie
 * @Github: https://github.com/jiejieTop
 * @Date: 2019-12-14 22:02:07
 * @LastEditTime: 2020-02-19 23:53:50
 * @Description: the code belongs to jiejie, please keep the author information and source code according to the license.
 */
#include "platform_memory.h"
#include "string.h"

void *memory_alloc(size_t size)
{
    char *ptr;
    ptr = rt_malloc(size);
    memset(ptr, 0, size);
    return (void *)ptr;
}

void *memory_calloc(size_t num, size_t size)
{
    return rt_calloc(num, size);
}

void memory_free(void *ptr)
{
    rt_free(ptr);
}
