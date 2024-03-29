#pragma once

#include <type_traits>
#include <atomic>

enum class JobType {
    NORMAL,
    BACKGROUND,
};


class JobScope {
    friend class Job;
    friend class JobSystem;
    friend class ThreadContext;

    class ThreadContext *threadContext;
    JobScope *prevActiveScope;
    JobScope *parentScope;
    std::atomic<int> pendingCount;

public:
    JobScope();
    JobScope(JobScope &parentScope);
    JobScope(class ThreadContext *threadContext);
    ~JobScope();

    JobScope(const JobScope&) = delete;
    JobScope& operator=(const JobScope&) = delete;

    void enqueue(const class Job &func, JobType type = JobType::NORMAL);
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
        static_assert(std::is_trivially_copyable_v<Func> == true);
        static_assert(std::is_invocable_v<Func> == true);
        static_assert(sizeof(Func) <= MAX_DATA);

        Func func;

        Helper(const Func &func) : func(func) {}

        static void invoke(void *data) {
            Helper *self = static_cast<Helper *>(data);
            self->func();
        }
    };

    void invoke() {
        invoker((void *)data);
        --scope->pendingCount;
    }

    static void enqueueBackgroundJob(const Job &func);

public:
    Job() : scope(nullptr), invoker(nullptr) { }

    template <typename Func>
    Job(const Func &func) : scope(nullptr), invoker(Helper<Func>::invoke) {
        new (data) Helper<Func>(func);
    }

    static void enqueue(const Job &func, JobType type = JobType::NORMAL);
};

static_assert(sizeof(Job) == 64);


class JobSystem {
public:
    static void dispatch();
    static void start();
    static void stop();
};
