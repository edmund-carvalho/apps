/**
 * Copyright (c) 2026 Edmund C
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "timer.h"

#define CLOCKID CLOCK_REALTIME
#define TIMER_SIG (SIGRTMIN + 1) // Avoid using the absolute minimum to reduce conflicts

typedef struct timer_node {
    timer_t id;
    timer_cb_t user_cb;
    void *user_data;
    struct timer_node *next;
} timer_node_t;

static timer_node_t *timer_list = NULL;
static pthread_mutex_t timer_lock = PTHREAD_MUTEX_INITIALIZER;

// Helper to convert MS to timespec
static void ms_to_timespec(long long ms, struct timespec *ts) {
    ts->tv_sec = ms / 1000;
    ts->tv_nsec = (ms % 1000) * 1000000;
}

// POSIX Signal Handler (Internal)
static void timer_signal_handler(int sig, siginfo_t *si, void *uc) {
    // Avoid heavy locking here if possible. 
    // We fetch the node directly passed via sival_ptr (Safe and fast!)
    timer_node_t *node = (timer_node_t *)si->si_value.sival_ptr;
    if (node && node->user_cb) {
        node->user_cb(node->user_data);
    }
}

int timer_queue_init(void) {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sa.sa_sigaction = timer_signal_handler;
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(TIMER_SIG, &sa, NULL) == -1) {
        perror("sigaction failed");
        return -1;
    }
    return 0;
}

int timer_start(timer_t *timerid, long long delay_ms, long long interval_ms, timer_cb_t callback, void *user_data) {
    if (!timerid || !callback) return -1;

    timer_node_t *node = malloc(sizeof(timer_node_t));
    if (!node) return -1;

    node->user_cb = callback;
    node->user_data = user_data;

    struct sigevent sev;
    memset(&sev, 0, sizeof(struct sigevent));
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = TIMER_SIG;
    // CRITICAL: Pass the node pointer itself so the signal handler doesn't have to loop through a list!
    sev.sigev_value.sival_ptr = node; 

    if (timer_create(CLOCKID, &sev, &node->id) == -1) {
        perror("timer_create");
        free(node);
        return -1;
    }

    struct itimerspec its;
    struct timespec start_ts, int_ts;
    ms_to_timespec(delay_ms, &start_ts);
    ms_to_timespec(interval_ms, &int_ts);
    its.it_value = start_ts;
    its.it_interval = int_ts;

    if (timer_settime(node->id, 0, &its, NULL) == -1) {
        perror("timer_settime");
        timer_delete(node->id);
        free(node);
        return -1;
    }

    // Copy ID back to user
    *timerid = node->id;

    // Insert into tracking list safely
    pthread_mutex_lock(&timer_lock);
    node->next = timer_list;
    timer_list = node;
    pthread_mutex_unlock(&timer_lock);

    return 0;
}

int timer_reload(timer_t *timerid, long long delay_ms, long long interval_ms) {
    if (!timerid) return -1;

    struct itimerspec its;
    struct timespec start_ts, int_ts;
    ms_to_timespec(delay_ms, &start_ts);
    ms_to_timespec(interval_ms, &int_ts);
    its.it_value = start_ts;
    its.it_interval = int_ts;

    if (timer_settime(*timerid, 0, &its, NULL) == -1) {
        perror("timer_settime reload");
        return -1;
    }
    return 0;
}

int timer_stop(timer_t *timerid) {
    if (!timerid || *timerid == 0) return -1;

    pthread_mutex_lock(&timer_lock);
    
    timer_node_t *cur = timer_list;
    timer_node_t *prev = NULL;

    while (cur && cur->id != *timerid) {
        prev = cur;
        cur = cur->next;
    }

    if (!cur) {
        pthread_mutex_unlock(&timer_lock);
        return -1; // Timer not found
    }

    // Fix the Head-Deletion Bug
    if (!prev) {
        timer_list = cur->next;
    } else {
        prev->next = cur->next;
    }
    pthread_mutex_unlock(&timer_lock);

    // Clean up OS timer and memory
    timer_delete(cur->id);
    free(cur);
    *timerid = 0; // Clear it out safely

    return 0;
}