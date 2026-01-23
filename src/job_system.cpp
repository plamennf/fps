#include "main.h"
#include "job_system.h"

#include <SDL3/SDL_thread.h>

static SDL_Thread *job_thread = NULL;
static SDL_Mutex *job_mutex = NULL;
static SDL_Condition *job_cond = NULL;

static Job job_queue[256];
static int job_head = 0;
static int job_tail = 0;

static bool job_thread_running = true;

int job_thread_proc(void *unused) {
    while (job_thread_running) {
        Job job = {};

        SDL_LockMutex(job_mutex);
        while (job_head == job_tail && job_thread_running) {
            SDL_WaitCondition(job_cond, job_mutex);
        }

        if (!job_thread_running) {
            SDL_UnlockMutex(job_mutex);
            break;
        }

        job = job_queue[job_head];
        job_head = (job_head + 1) % 256;

        SDL_UnlockMutex(job_mutex);

        job.fn(job.data);
    }

    return 0;
}

void init_job_system() {
    job_mutex = SDL_CreateMutex();
    job_cond  = SDL_CreateCondition();
    job_thread = SDL_CreateThread(job_thread_proc, "asset_worker", NULL);
}

void shutdown_job_system() {
    SDL_LockMutex(job_mutex);
    job_thread_running = false;
    SDL_SignalCondition(job_cond);
    SDL_UnlockMutex(job_mutex);

    SDL_WaitThread(job_thread, NULL);
    SDL_DestroyCondition(job_cond);
    SDL_DestroyMutex(job_mutex);
}

void enqueue_job(void (*fn)(void *), void *data) {
    SDL_LockMutex(job_mutex);

    job_queue[job_tail] = { fn, data };
    job_tail = (job_tail + 1) % ArrayCount(job_queue);

    SDL_SignalCondition(job_cond);
    SDL_UnlockMutex(job_mutex);
}
