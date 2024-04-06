#pragma once

#include <atomic>
#include <utility>

class Job;

class JobScope {
    friend Job;
    friend class JobSystem;
    friend class ThreadContext;

    class ThreadContext *threadContext;
    JobScope *prevActiveScope;
    JobScope *parentScope;
    std::atomic<int> pendingCount;

    void enqueueJob(Job &job);

public:
    JobScope();
    JobScope(JobScope &parentScope);
    JobScope(class ThreadContext *threadContext);
    ~JobScope();

    JobScope(const JobScope&) = delete;
    JobScope& operator=(const JobScope&) = delete;

    static JobScope *getActiveScope();

    void addPendingCount(int diff) {
        pendingCount += diff;
    }

    template <typename Func>
    inline void enqueue(Func &&func);

    void dispatch();
};

class Job {
    friend JobScope;
    friend class ThreadContext;

    using Invoker = void (*)(void *);
    enum { MAX_DATA = 64 - sizeof(Invoker) - sizeof(JobScope *) };

    JobScope *scope;
    Invoker invoker;
    char data[MAX_DATA];

    template <typename Func>
    struct Helper {
        static_assert(sizeof(Func) <= MAX_DATA);

        Func func;

        Helper(Func &&func) : func(std::forward<Func>(func)) {}

        static void invoker(void *data) {
            Helper *self = static_cast<Helper *>(data);
            self->func();
            self->func.~Func(); // all enqueued jobs will be invoked precisely 1 time, so explicitly calling the destructor like this is suitable
        }
    };

    template <typename Func>
    void setFunc(Func &&func) {
        scope = nullptr; // will be set when enqueued
        invoker = Helper<Func>::invoker;
        new (data) Helper<Func>(std::forward<Func>(func));
    }

    void run() {
        invoker((void *)data);
        --scope->pendingCount;
    }

    static void enqueueJob(Job &job);

public:
    template <typename Func>
    inline static void enqueue(Func &&func) {
        Job job;
        job.setFunc(std::forward<Func>(func));
        enqueueJob(job);
    }
};

static_assert(sizeof(Job) == 64);

template <typename Func>
inline void JobScope::enqueue(Func &&func) {
    Job job;
    job.setFunc(std::forward<Func>(func));
    enqueueJob(job);
}

class JobSystem {
public:
    static void dispatch();
    static void start();
    static void stop();
};

void testJobSystem();
