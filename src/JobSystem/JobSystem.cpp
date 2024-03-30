#include "JobSystem.h"

#include "wsq.hpp"
#include "MPMCQueue.h"
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <xmmintrin.h> // for _mm_pause

#define LOG_DEBUG(...)
#define SET_THREAD_NAME(name) pthread_setname_np(pthread_self(), name)
#define PAUSE() _mm_pause()

#define PRINT_STATS 1
#if PRINT_STATS
#define INC_STAT(stat) ++(stat)
#else
#define INC_STAT(stat)
#endif

// max number of concurrent background jobs
#define DEFAULT_BACKGROUND_CONCURRENCY 2


typedef WorkStealingQueue<Job> JobQueue;
typedef rigtorp::MPMCQueue<Job> BGQueue;

static std::atomic<bool> workersShouldStop;
static int workerCount;
static JobQueue *workerQueues;
static std::vector<std::thread> workerThreads;
static JobQueue mainQueue; // belonging to the main thread
static BGQueue bgQueue(65536);
static std::atomic<int> bgSemaphore(DEFAULT_BACKGROUND_CONCURRENCY);
static JobScope rootScope((ThreadContext *)nullptr);


class ThreadContext {
public:
    JobQueue *queue;
    int stealStart; // at which worker index to start stealing probe
    JobScope *activeScope;
    JobScope *threadScope;
    std::atomic<int> bgQuotaUsed;
    char threadName[16];

#if PRINT_STATS
    int sleepCount = 0;
    int yieldCount = 0;
    int pauseCount = 0;
    int runOwnCount = 0;
    int stealMainCount = 0;
    int stealWorkerCount = 0;
    int bgCount = 0;
#endif

    ThreadContext() {
        threadScope = new JobScope(this);
    }

    void finish() {
        threadScope->dispatch();
        delete threadScope;
        threadScope = nullptr;
        activeScope = nullptr;
        queue = nullptr;
#if PRINT_STATS
        printf("%s   S:%d Y:%d P:%d   s:%d mt:%d wt:%d bg:%d\n", threadName, sleepCount, yieldCount, pauseCount, runOwnCount, stealMainCount, stealWorkerCount, bgCount);
#endif
    }

    void enqueueJob(Job &job) {
        assert(queue);
        job.scope = activeScope;
        ++activeScope->pendingCount;
        queue->push(job);
    }

    void dispatchActiveScope() {
        activeScope->dispatch();
    }

    bool dispatchSingleJob() {
        // service own queue till empty
        auto job = queue->pop();
        if (job.has_value()) {
            LOG_DEBUG("%s running own job\n", threadName);
            INC_STAT(runOwnCount);
            job->run();
            return true;
        }

        // steal from main queue
        if (queue != &mainQueue) {
            job = mainQueue.steal();
            if (job.has_value()) {
                LOG_DEBUG("%s stealing from main\n", threadName);
                INC_STAT(stealMainCount);
                job->run();
                return true;
            }
        }

        // steal from other worker queues (not self)
        for (int i = 0; i < workerCount; ++i) {
            int idx = (stealStart + i) % workerCount;
            JobQueue *workerQueue = &workerQueues[idx];
            if (workerQueue != queue) {
                auto job = workerQueues[idx].steal();
                if (job.has_value()) {
                    LOG_DEBUG("%s stealing from worker%d\n", threadName, idx);
                    INC_STAT(stealWorkerCount);
                    stealStart = idx; // next time, start trying to steal from this queue
                    job->run();
                    return true;
                }
            }
        }
    
        // check if we should do a background job
        if (--bgSemaphore >= 0) {
            Job bgJob;
            if (bgQueue.try_pop(bgJob)) {
                ++bgQuotaUsed;
                bgJob.run();
                --bgQuotaUsed;
                ++bgSemaphore;
                return true;
            }
        }
        ++bgSemaphore;
        return false;
    }

    void runWorker(int workerIndex) {
        sprintf(threadName, "worker%d", workerIndex);
        SET_THREAD_NAME(threadName);
        LOG_DEBUG("%s starting\n", threadName);

        queue = &workerQueues[workerIndex];
        stealStart = (workerIndex + 1) % workerCount;
        uint64_t joblessIterations = 0;

        while (!workersShouldStop) {
            while (dispatchSingleJob()) {
                joblessIterations = 0; // we did some work!
            }

            // we couldn't find any more jobs to run (after looking once at each queue)

            // make this loop progressively lighter on the CPU by yielding more CPU time as more and more iteratons pass without work done
            if (++joblessIterations < 1000) {
                INC_STAT(pauseCount);
                PAUSE();
            } else if (joblessIterations < 10000) {
                INC_STAT(yieldCount);
                std::this_thread::yield();
            } else {
                INC_STAT(sleepCount);
                std::this_thread::sleep_for(std::chrono::milliseconds(4)); // don't sleep longer. keeps things responsive, while mostly eliminating CPU use
            }
        }

        finish();
        LOG_DEBUG("%s stopped\n", threadName);
    }
};

static thread_local ThreadContext currentThreadContext;


JobScope::JobScope() :
    threadContext(&currentThreadContext),
    prevActiveScope(threadContext->activeScope),
    parentScope(threadContext->activeScope),
    pendingCount(0)
{
    assert(threadContext->queue);
    threadContext->activeScope = this;
    ++parentScope->pendingCount;
}

JobScope::JobScope(JobScope &parentScope) :
    threadContext(&currentThreadContext),
    prevActiveScope(threadContext->activeScope),
    parentScope(&parentScope),
    pendingCount(0)
{
    assert(threadContext->queue);
    threadContext->activeScope = this;
    ++parentScope.pendingCount;
}

JobScope::JobScope(ThreadContext *threadContext) :
    threadContext(threadContext),
    prevActiveScope(nullptr),
    parentScope(nullptr),
    pendingCount(0)
{
    if (threadContext) {
        threadContext->activeScope = this;
        parentScope = &rootScope;
        ++parentScope->pendingCount;
    }
}

JobScope::~JobScope() {
    if (threadContext) {
        dispatch();
        threadContext->activeScope = prevActiveScope;
        if (parentScope) {
            --parentScope->pendingCount;
        }
    }
}

void JobScope::enqueueNormalJob(Job &job) {
    threadContext->enqueueJob(job);
}

JobScope *JobScope::getActiveScope() {
    return currentThreadContext.activeScope;
}

void JobScope::dispatch() {
    assert(threadContext->queue);
    // We're "blocking" this thread while possibly holding background-quota.
    // Release any held quota for the duration, to avoid creating starvation scenarios
    // (where this dispatch runs jobs that "dispatch-block" on further background jobs which never complete because the quota is exhausted).
    // This can lead to the maximum background concurrency temporarily exceeding the regular limit, but that is fine.
    int bgQuotaUsed = threadContext->bgQuotaUsed;
    bgSemaphore += bgQuotaUsed;
    while (pendingCount) {
        if (!threadContext->dispatchSingleJob()) {
            PAUSE();
        }
    }
    bgSemaphore -= bgQuotaUsed;
}

void Job::enqueueNormalJob(Job &job) {
    currentThreadContext.enqueueJob(job);
}

void Job::enqueueBackgroundJob(Job &job) {
    job.scope = &rootScope;
    ++rootScope.pendingCount;
    bgQueue.push(job);
}


void JobSystem::modifyBackgroundConcurrency(int diff) {
    bgSemaphore += diff;
}

void JobSystem::dispatch() {
    currentThreadContext.dispatchActiveScope();
}

void JobSystem::start() {
    sprintf(currentThreadContext.threadName, "main");
    SET_THREAD_NAME(currentThreadContext.threadName);
    currentThreadContext.queue = &mainQueue;
    workerCount = std::thread::hardware_concurrency();
    if (workerCount > 2) {
        --workerCount; // subtract one, since we also will have the main thread
    }
    workerQueues = new JobQueue[workerCount];
    workerThreads.reserve(workerCount);
    for (int i = 0; i < workerCount; ++i) {
        workerThreads.emplace_back([i] { currentThreadContext.runWorker(i); });
    }
}

void JobSystem::stop() {
    currentThreadContext.finish();
    workersShouldStop = true;
    for (auto& thread : workerThreads) {
        thread.join();
    }
    workersShouldStop = false;
    assert(rootScope.pendingCount == 0);
    assert(bgQueue.empty());
    assert(mainQueue.empty());
    workerThreads.clear();
    delete [] workerQueues;
    workerQueues = nullptr;
}
