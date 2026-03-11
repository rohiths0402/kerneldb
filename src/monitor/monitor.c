
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>
#include <time.h>
#include "monitor.h"
#include "reph.h"

static atomic_int g_stop = 0;

static void *monitor_thread(void *arg) {
    (void)arg;

    unsigned long uptime  = 0;
    int ticks   = 0;
    while (!atomic_load(&g_stop)) {
        sleep(1);
        uptime++;
        ticks++;

        if (ticks >= MONITOR_INTERVAL_SEC) {
            ticks = 0;
            printf("\n  \033[1;32m[monitor]\033[0m" " engine alive — uptime %lus\n"
                   REPL_PROMPT, uptime);
            fflush(stdout);
        }
    }

    return NULL;
}

void monitor_start(void) {
    pthread_t tid;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&tid, &attr, monitor_thread, NULL) != 0) {
        perror("pthread_create");
    }

    pthread_attr_destroy(&attr);
}

void monitor_stop(void) {
    atomic_store(&g_stop, 1);
}