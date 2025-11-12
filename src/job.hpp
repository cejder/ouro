#pragma once

#include "common.hpp"

#include <tinycthread.h>

// Need for tinycthread on macOS
#ifdef call_once
#undef call_once
#endif

// Job system for parallel task execution
// Uses persistent worker threads to avoid thread creation overhead

#define JOB_SYSTEM_MAX_WORKERS 16
#define JOB_SYSTEM_MAX_JOBS 1024

// Function signature for job work
// arg: User data passed to the job
// Returns: 0 on success, non-zero on error
typedef S32 (*JobWorkFunc)(void *arg);

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
    U32 worker_id;
    BOOL should_exit;
};

struct JobSystem {
    BOOL initialized;

    // Worker threads
    JobWorker workers[JOB_SYSTEM_MAX_WORKERS];
    U32 worker_count;

    // Job queue (ring buffer)
    Job jobs[JOB_SYSTEM_MAX_JOBS];
    U32 job_write_idx;  // Where to write next job
    U32 job_read_idx;   // Where to read next job
    U32 job_count;      // Number of pending jobs

    // Synchronization
    mtx_t queue_mutex;
    cnd_t work_available_cond;  // Signal when work is available
    cnd_t work_done_cond;       // Signal when all work is done
    U32 active_job_count;       // Jobs currently being processed
};

JobSystem extern g_job_system;

// Initialize job system with N worker threads (0 = auto-detect CPU cores)
void job_system_init(U32 worker_count);

// Shutdown job system and cleanup worker threads
void job_system_quit();

// Submit a job to be executed by a worker thread
// Returns: BOOL indicating success
BOOL job_system_submit(JobWorkFunc work_func, void *work_arg);

// Wait for all submitted jobs to complete
void job_system_wait();

// Get number of worker threads
U32 job_system_get_worker_count();
