#include "job.hpp"
#include "info.hpp"
#include "log.hpp"
#include "memory.hpp"

JobSystem g_job_system = {};

S32 static i_job_worker_thread(void *arg) {
    auto *worker = (JobWorker *)arg;

    lld("Job worker %u started", worker->worker_id);

    for (;;) {
        Job job = {};
        BOOL has_job = false;

        // Check for work
        mtx_lock(&g_job_system.queue_mutex);
        {
            // Wait for work or exit signal
            while (g_job_system.job_count == 0 && !worker->should_exit) {
                cnd_wait(&g_job_system.work_available_cond, &g_job_system.queue_mutex);
            }

            // Check if we should exit
            if (worker->should_exit) {
                mtx_unlock(&g_job_system.queue_mutex);
                break;
            }

            // Get job from queue (ring buffer)
            if (g_job_system.job_count > 0) {
                job = g_job_system.jobs[g_job_system.job_read_idx];
                g_job_system.jobs[g_job_system.job_read_idx].status = JOB_STATUS_IN_PROGRESS;
                g_job_system.job_read_idx = (g_job_system.job_read_idx + 1) % JOB_SYSTEM_MAX_JOBS;
                g_job_system.job_count--;
                g_job_system.active_job_count++;
                has_job = true;
            }
        }
        mtx_unlock(&g_job_system.queue_mutex);

        // Execute job outside of lock
        if (has_job) {
            S32 const result = job.work_func(job.work_arg);

            // Update job status
            mtx_lock(&g_job_system.queue_mutex);
            {
                g_job_system.active_job_count--;

                if (result != 0) {
                    lle("Job worker %u: job failed with error %d", worker->worker_id, result);
                }

                // Signal if all work is done
                if (g_job_system.job_count == 0 && g_job_system.active_job_count == 0) {
                    cnd_broadcast(&g_job_system.work_done_cond);
                }
            }
            mtx_unlock(&g_job_system.queue_mutex);
        }
    }

    lld("Job worker %u exiting", worker->worker_id);
    return 0;
}

void job_system_init(U32 worker_count) {
    if (g_job_system.initialized) {
        llw("Job system already initialized");
        return;
    }

    // Auto-detect CPU core count if not specified
    if (worker_count == 0) {
        worker_count = (U32)info_get_cpu_core_count();
    }

    if (worker_count > JOB_SYSTEM_MAX_WORKERS) {
        llw("Requested %u workers, clamping to max %d", worker_count, JOB_SYSTEM_MAX_WORKERS);
        worker_count = JOB_SYSTEM_MAX_WORKERS;
    }

    g_job_system.worker_count = worker_count;
    g_job_system.job_write_idx = 0;
    g_job_system.job_read_idx = 0;
    g_job_system.job_count = 0;
    g_job_system.active_job_count = 0;

    // Initialize synchronization primitives
    if (mtx_init(&g_job_system.queue_mutex, mtx_plain) != thrd_success) {
        lle("Failed to initialize job system mutex");
        return;
    }

    if (cnd_init(&g_job_system.work_available_cond) != thrd_success) {
        lle("Failed to initialize work available condition variable");
        mtx_destroy(&g_job_system.queue_mutex);
        return;
    }

    if (cnd_init(&g_job_system.work_done_cond) != thrd_success) {
        lle("Failed to initialize work done condition variable");
        mtx_destroy(&g_job_system.queue_mutex);
        cnd_destroy(&g_job_system.work_available_cond);
        return;
    }

    // Create worker threads
    for (U32 i = 0; i < worker_count; ++i) {
        g_job_system.workers[i].worker_id = i;
        g_job_system.workers[i].should_exit = false;

        if (thrd_create(&g_job_system.workers[i].thread, i_job_worker_thread, &g_job_system.workers[i]) != thrd_success) {
            lle("Failed to create job worker thread %u", i);

            // Signal existing threads to exit
            for (U32 j = 0; j < i; ++j) {
                g_job_system.workers[j].should_exit = true;
            }
            cnd_broadcast(&g_job_system.work_available_cond);

            // Wait for created threads to exit
            for (U32 j = 0; j < i; ++j) {
                thrd_join(g_job_system.workers[j].thread, nullptr);
            }

            mtx_destroy(&g_job_system.queue_mutex);
            cnd_destroy(&g_job_system.work_available_cond);
            cnd_destroy(&g_job_system.work_done_cond);
            return;
        }
    }

    g_job_system.initialized = true;
    lli("Job system initialized with %u worker threads", worker_count);
}

void job_system_quit() {
    if (!g_job_system.initialized) {
        return;
    }

    lld("Job system shutting down...");

    // Signal all workers to exit
    mtx_lock(&g_job_system.queue_mutex);
    {
        for (U32 i = 0; i < g_job_system.worker_count; ++i) {
            g_job_system.workers[i].should_exit = true;
        }
    }
    mtx_unlock(&g_job_system.queue_mutex);

    // Wake up all waiting workers
    cnd_broadcast(&g_job_system.work_available_cond);

    // Wait for all workers to exit
    for (U32 i = 0; i < g_job_system.worker_count; ++i) {
        S32 result = S32_MAX;
        thrd_join(g_job_system.workers[i].thread, &result);
        lld("Job worker %u joined", i);
    }

    // Cleanup synchronization primitives
    mtx_destroy(&g_job_system.queue_mutex);
    cnd_destroy(&g_job_system.work_available_cond);
    cnd_destroy(&g_job_system.work_done_cond);

    g_job_system.initialized = false;
    lli("Job system shutdown complete");
}

BOOL job_system_submit(JobWorkFunc work_func, void *work_arg) {
    if (!g_job_system.initialized) {
        lle("Job system not initialized");
        return false;
    }

    if (!work_func) {
        lle("Invalid work function");
        return false;
    }

    mtx_lock(&g_job_system.queue_mutex);
    {
        // Check if queue is full
        if (g_job_system.job_count >= JOB_SYSTEM_MAX_JOBS) {
            mtx_unlock(&g_job_system.queue_mutex);
            lle("Job queue is full");
            return false;
        }

        // Add job to queue (ring buffer)
        Job *job = &g_job_system.jobs[g_job_system.job_write_idx];
        job->work_func = work_func;
        job->work_arg = work_arg;
        job->status = JOB_STATUS_PENDING;

        g_job_system.job_write_idx = (g_job_system.job_write_idx + 1) % JOB_SYSTEM_MAX_JOBS;
        g_job_system.job_count++;
    }
    mtx_unlock(&g_job_system.queue_mutex);

    // Signal that work is available
    cnd_signal(&g_job_system.work_available_cond);

    return true;
}

void job_system_wait() {
    if (!g_job_system.initialized) {
        return;
    }

    mtx_lock(&g_job_system.queue_mutex);
    {
        // Wait until all jobs are done
        while (g_job_system.job_count > 0 || g_job_system.active_job_count > 0) {
            cnd_wait(&g_job_system.work_done_cond, &g_job_system.queue_mutex);
        }
    }
    mtx_unlock(&g_job_system.queue_mutex);
}

U32 job_system_get_worker_count() {
    return g_job_system.worker_count;
}
