#include "JobSystem.h"

#include "wsq.hpp"
#include "MPMCQueue.h"
#include <thread>
#include <mutex>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <xmmintrin.h> // for _mm_pause

#define LOG_DEBUG(...)
#define SET_THREAD_NAME(name) pthread_setname_np(pthread_self(), name)
#define PAUSE() _mm_pause()

#define PRINT_STATS 0
#if PRINT_STATS
#define INC_STAT(stat) ++(stat)
#else
#define INC_STAT(stat)
#endif


typedef WorkStealingQueue<Job> JobQueue;
typedef rigtorp::MPMCQueue<Job> ExternalJobQueue;

static std::atomic<bool> workersShouldStop;
static int workerCount;
static JobQueue *workerQueues;
static std::vector<std::thread> workerThreads;
static JobQueue mainQueue; // belonging to the main thread
static ExternalJobQueue externalMainQueue(16384);
static ExternalJobQueue externalWorkerQueue(16384);


class ThreadContext {
public:
    JobQueue *queue;
    ExternalJobQueue *externalQueue;
    JobScope *activeScope;
    JobScope *threadScope;
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
        int count = workerCount + 1; // add one so we can dedicate an index to dispatching from the external queue
        int start = rand() % count;
        for (int i = 0; i < count; ++i) {
            int idx = (start + i) % count;
            if (idx < workerCount) {
                JobQueue *workerQueue = &workerQueues[idx];
                if (workerQueue != queue) {
                    job = workerQueue->steal();
                    if (job.has_value()) {
                        LOG_DEBUG("%s stealing from worker%d\n", threadName, idx);
                        INC_STAT(stealWorkerCount);
                        job->run();
                        return true;
                    }
                }
            } else {
                Job externalJob;
                if (externalQueue->try_pop(externalJob)) {
                    externalJob.run();
                    return true;
                }
            }
        }

        return false;
    }

    void runWorker(int workerIndex) {
        sprintf(threadName, "worker%d", workerIndex);
        SET_THREAD_NAME(threadName);
        LOG_DEBUG("%s starting\n", threadName);

        queue = &workerQueues[workerIndex];
        externalQueue = &externalWorkerQueue;
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

void JobScope::enqueueJob(Job &job) {
    threadContext->enqueueJob(job);
}

JobScope *JobScope::getActiveScope() {
    return currentThreadContext.activeScope;
}

void JobScope::dispatch() {
    assert(threadContext->queue);
    while (pendingCount) {
        if (!threadContext->dispatchSingleJob()) {
            PAUSE();
        }
    }
    if (threadContext->externalQueue == &externalMainQueue) {
        for (;;) {
            Job job;
            if (!externalMainQueue.try_pop(job)) {
                break;
            }
            job.run();
        }
    }
}

void Job::enqueueJob(Job &job) {
    currentThreadContext.enqueueJob(job);
}

void Job::enqueueJobOnMain(Job &job) {
    externalMainQueue.push(job);
}

void Job::enqueueJobOnWorker(Job &job) {
    externalWorkerQueue.push(job);
}

void JobSystem::dispatch() {
    currentThreadContext.dispatchActiveScope();
}

void JobSystem::start() {
    sprintf(currentThreadContext.threadName, "main");
    SET_THREAD_NAME(currentThreadContext.threadName);
    currentThreadContext.queue = &mainQueue;
    currentThreadContext.externalQueue = &externalMainQueue;
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
    assert(mainQueue.empty());
    assert(externalMainQueue.empty());
    assert(externalWorkerQueue.empty());
    workerThreads.clear();
    delete [] workerQueues;
    workerQueues = nullptr;
}


#if 0
#include <chrono>
#include "Logger.h"

static std::atomic<int> dtorCount;
static std::atomic<int> ctorCount;
static std::atomic<int> cctorCount;
static std::atomic<int> mctorCount;
static std::atomic<int> cassignCount;
static std::atomic<int> massignCount;
struct Counter {
    __attribute__((noinline)) ~Counter() { ++dtorCount; }
    __attribute__((noinline)) Counter() { ++ctorCount; }
    __attribute__((noinline)) Counter(const Counter &) { ++cctorCount; }
    __attribute__((noinline)) Counter(Counter &&) { ++mctorCount; }
    __attribute__((noinline)) Counter& operator=(const Counter& other) { ++cassignCount; return *this; }
    __attribute__((noinline)) Counter& operator=(Counter&& other) noexcept { ++massignCount; return *this; }
};

void testJobSystem() {
    auto start = std::chrono::high_resolution_clock::now();
    std::atomic<int> counter(0);
    Counter counterObject;
    {
        JobScope scope;
        for (int i = 0; i < 1000; ++i) {
            Job::enqueue([&counter, &scope, counterObject = std::move(counterObject)] {
                JobScope scope2(scope);
                for (int j = 0; j < 1000; ++j) {
                    Job::enqueue([&counter] {
                        ++counter;
                    });
                }
            });
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    logger->info("Test counter: %d in %d ms", (int)counter, (int)((end-start)/std::chrono::milliseconds(1)));
    logger->info("dtorCount %d, ctorCount: %d, cctorCount: %d, mctorCount: %d, cassignCount: %d, massignCount: %d", (int)dtorCount, (int)ctorCount, (int)cctorCount, (int)mctorCount, (int)cassignCount, (int)massignCount);
}
#endif
