#include <pthread.h>

void pthread_create(pthread_t *thread,
                   void (*start_routine)(void*),
                   void *arg)
{
    *thread = sys_thread_create(start_routine, arg);
}

pthread_t pthread_join(pthread_t thread)
{
    return sys_thread_join(thread);
}
