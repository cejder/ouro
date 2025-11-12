#include "job.hpp"
#include "core.hpp"
#include "info.hpp"
#include "log.hpp"
#include "memory.hpp"

JobSystem g_job_system = {};

S32 static i_job_worker_thread(void *arg) {
    auto *worker = (JobWorker *)arg;

    for (;;) {
        Job job      = {};
        BOOL has_job = false;

        // Check for work
        mtx_lock(&g_job_system.queue_mutex);
        {
            // Wait for work or exit signal
            while (g_job_system.jobs.count == 0 && !worker->should_exit) {
                cnd_wait(&g_job_system.work_available_cond, &g_job_system.queue_mutex);
            }

            // Check if we should exit
            if (worker->should_exit) {
                mtx_unlock(&g_job_system.queue_mutex);
                break;
            }

            // Get job from queue
            if (g_job_system.jobs.count > 0) {
                job     = ring_pop(&g_job_system.jobs);
                has_job = true;
                g_job_system.active_job_count++;
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

                if (result != 0) { lle("Job worker %zu: job failed with error %d", worker->worker_id, result); }

                // Signal if all work is done
                if (g_job_system.jobs.count == 0 && g_job_system.active_job_count == 0) {
                    cnd_broadcast(&g_job_system.work_done_cond);
                }
            }
            mtx_unlock(&g_job_system.queue_mutex);
        }
    }

    return 0;
}

void job_system_init(SZ worker_count) {
    // Auto-detect CPU core count if not specified
    if (worker_count == 0) { worker_count = g_core.cpu_core_count; }

    array_init(MEMORY_TYPE_ARENA_PERMANENT, &g_job_system.workers, worker_count);
    ring_init(MEMORY_TYPE_ARENA_PERMANENT, &g_job_system.jobs, JOB_SYSTEM_MAX_JOBS);

    g_job_system.active_job_count = 0;
    g_job_system.workers.count    = worker_count;;

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
    for (SZ i = 0; i < worker_count; ++i) {
        g_job_system.workers.data[i].worker_id   = i;
        g_job_system.workers.data[i].should_exit = false;

        if (thrd_create(&g_job_system.workers.data[i].thread, i_job_worker_thread, &g_job_system.workers.data[i]) != thrd_success) {
            lle("Failed to create job worker thread %zu", i);

            // Signal existing threads to exit
            for (SZ j = 0; j < i; ++j) { g_job_system.workers.data[j].should_exit = true; }
            cnd_broadcast(&g_job_system.work_available_cond);

            // Wait for created threads to exit
            for (SZ j = 0; j < i; ++j) { thrd_join(g_job_system.workers.data[j].thread, nullptr); }

            mtx_destroy(&g_job_system.queue_mutex);
            cnd_destroy(&g_job_system.work_available_cond);
            cnd_destroy(&g_job_system.work_done_cond);
            return;
        }
    }

    lli("Job system initialized with %zu worker threads", worker_count);
}

void job_system_quit() {
    // Signal all workers to exit
    mtx_lock(&g_job_system.queue_mutex);
    {
        for (SZ i = 0; i < g_job_system.workers.count; ++i) { g_job_system.workers.data[i].should_exit = true; }
    }
    mtx_unlock(&g_job_system.queue_mutex);

    // Wake up all waiting workers
    cnd_broadcast(&g_job_system.work_available_cond);

    // Wait for all workers to exit
    for (SZ i = 0; i < g_job_system.workers.count; ++i) {
        S32 result = S32_MAX;
        thrd_join(g_job_system.workers.data[i].thread, &result);
    }

    // Cleanup synchronization primitives
    mtx_destroy(&g_job_system.queue_mutex);
    cnd_destroy(&g_job_system.work_available_cond);
    cnd_destroy(&g_job_system.work_done_cond);

    lli("Job system shutdown complete");
}

BOOL job_system_submit(JobWorkFunc work_func, void *work_arg) {
    if (!work_func) {
        lle("Invalid work function");
        return false;
    }

    mtx_lock(&g_job_system.queue_mutex);
    {
        // Check if queue is full
        if (g_job_system.jobs.count >= JOB_SYSTEM_MAX_JOBS) {
            mtx_unlock(&g_job_system.queue_mutex);
            llw("Job queue is full (capacity %d)", JOB_SYSTEM_MAX_JOBS);
            return false;
        }

        // Add job to queue (ring buffer)
        Job job       = {};
        job.status    = JOB_STATUS_PENDING;
        job.work_arg  = work_arg;
        job.work_func = work_func;
        ring_push(&g_job_system.jobs, job);
    }
    mtx_unlock(&g_job_system.queue_mutex);

    // Signal that work is available
    cnd_signal(&g_job_system.work_available_cond);

    return true;
}

void job_system_wait() {
    mtx_lock(&g_job_system.queue_mutex);
    {
        // Wait until all jobs are done
        while (g_job_system.jobs.count > 0 || g_job_system.active_job_count > 0) {
            cnd_wait(&g_job_system.work_done_cond, &g_job_system.queue_mutex);
        }
    }
    mtx_unlock(&g_job_system.queue_mutex);
}
