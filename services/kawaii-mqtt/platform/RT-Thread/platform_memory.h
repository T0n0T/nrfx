/*
 * @Author: jiejie
 * @Github: https://github.com/jiejieTop
 * @Date: 2019-12-14 22:06:35
 * @LastEditTime: 2020-02-19 23:54:02
 * @Description: the code belongs to jiejie, please keep the author information and source code according to the license.
 */
#ifndef _PLATFORM_MEMORY_H_
#define _PLATFORM_MEMORY_H_
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include <rtthread.h>

#define platform_memory_alloc(size) memory_alloc(size)
// ((void)printf("memory_alloc sizeof %d, %s:%d %s\n", size, __FILE__, __LINE__, __FUNCTION__), memory_alloc(size))

#define platform_memory_calloc(num, size) memory_calloc(num, size)
// ((void)printf("memory_calloc sizeof %d, %s:%d %s\n", num * size, __FILE__, __LINE__, __FUNCTION__), memory_calloc(num, size))

#define platform_memory_free(ptr) memory_free(ptr)
// ((void)printf("memory_free addr 0x%02x, %s:%d %s\n", ptr, __FILE__, __LINE__, __FUNCTION__), memory_free(ptr))

void *memory_alloc(size_t size);
void *memory_calloc(size_t num, size_t size);
void memory_free(void *ptr);

#endif
