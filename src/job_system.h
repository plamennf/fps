#pragma once

struct Job {
    void (*fn)(void *);
    void *data;
};

void init_job_system();
void shutdown_job_system();

void enqueue_job(void (*fn)(void *), void *data);
