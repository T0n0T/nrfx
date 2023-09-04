/*
 * @Author: jiejie
 * @Github: https://github.com/jiejieTop
 * @Date: 2019-12-15 18:31:33
 * @LastEditTime : 2020-01-16 00:19:10
 * @Description: the code belongs to jiejie, please keep the author information and source code according to the license.
 */
#ifndef _PLATFORM_MUTEX_H_
#define _PLATFORM_MUTEX_H_

#include <rtthread.h>

typedef struct platform_mutex {
    rt_mutex_t mutex;
} platform_mutex_t;

#define platform_mutex_init(m) \
    (printf("mutex_init, %s:%d %s\n", __FILE__, __LINE__, __FUNCTION__), mutex_init(m))

#define platform_mutex_destroy(m) \
    (printf("mutex_destroy, %s:%d %s\n", __FILE__, __LINE__, __FUNCTION__), mutex_destroy(m))

int mutex_init(platform_mutex_t *m);
int platform_mutex_lock(platform_mutex_t *m);
int platform_mutex_trylock(platform_mutex_t *m);
int platform_mutex_unlock(platform_mutex_t *m);
int mutex_destroy(platform_mutex_t *m);

#endif
