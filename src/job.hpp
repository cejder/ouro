#pragma once

#include "common.hpp"
#include "array.hpp"
#include "ring.hpp"

#include <tinycthread.h>

// Need for tinycthread on macOS
#ifdef call_once
#undef call_once
#endif

#define JOB_SYSTEM_MAX_JOBS 256

using JobWorkFunc = S32 (*)(void *arg);

enum JobStatus : U8 {
    JOB_STATUS_PENDING,
    JOB_STATUS_IN_PROGRESS,
    JOB_STATUS_COMPLETED,
    JOB_STATUS_ERROR,
};

struct Job {
    JobWorkFunc work_func;
    void *work_arg;
    JobStatus status;
};

struct JobWorker {
    thrd_t thread;
    SZ worker_id;
    BOOL should_exit;
};

ARRAY_DECLARE(JobWorkerArray, JobWorker);
RING_DECLARE(JobRing, Job);

struct JobSystem {
    JobWorkerArray workers;
    JobRing jobs;

    mtx_t queue_mutex;
    cnd_t work_available_cond; // Signal when work is available
    cnd_t work_done_cond;      // Signal when all work is done
    SZ active_job_count;       // Jobs currently being processed
};

JobSystem extern g_job_system;

void job_system_init(SZ worker_count);
void job_system_quit();
BOOL job_system_submit(JobWorkFunc work_func, void *work_arg);
void job_system_wait();
