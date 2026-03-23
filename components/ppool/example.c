#include <stdio.h>
#include <unistd.h>
#include "ppool.h"

pid_t gettid(void);

void thread_entry(void *args)
{
    printf("[%d] args: %ld\n", gettid(), (long)args);
    sleep(1);
}

int main(void)
{
    pool_t *ppool;
    ppool = ppool_init(5);

    pool_task ptasks[5] = {
        {0, thread_entry, 1},
        {0, thread_entry, 2},
        {0, thread_entry, 3},
        {0, thread_entry, 4},
        {0, thread_entry, 5},
    };

    for (int i=0; i<30; i++) {
        printf("add the %d task to ppool\n", i+1);
        ppool_add(ppool, &ptasks[i%5]);
    }

    sleep(5);
    ppool_destroy(ppool);

    return 0;
}