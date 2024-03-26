#pragma once

#include <type_traits>
#include <atomic>
#include <cstdint>


void startJobSystem();
void stopJobSystem();
void dispatchJobs();

class JobFunc;
void _enqueueJobInternal(JobFunc &func);

template<class Func>
inline void enqueueJob(Func func) {
    JobFunc jobFunc(func);
    _enqueueJobInternal(jobFunc);
}


class JobScope {
    friend JobFunc;
    friend struct ThreadContext;

    struct ThreadContext &threadContext;
    JobScope *prevActiveScope;
    JobScope *parentScope;
    std::atomic<uint32_t> pendingCount;

    JobScope(struct ThreadContext &threadContext);
    void doEnqueueJob(JobFunc &func);

public:
    JobScope();
    JobScope(JobScope &parentScope);
    ~JobScope();

    JobScope(const JobScope&) = delete;
    JobScope& operator=(const JobScope&) = delete;

    template<class Func>
    void enqueueJob(Func func) {
        JobFunc jobFunc(func);
        doEnqueueJob(jobFunc);
    }

    void dispatchJobs();
};


class JobFunc {
    friend struct ThreadContext;

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

public:
    JobFunc() = default;

    template <typename Func>
    JobFunc(const Func &func) : scope(nullptr), invoker(Helper<Func>::invoke) {
        new (data) Helper<Func>(func);
    }

    void invoke() {
        invoker((void *)data);
        --scope->pendingCount;
    }
};
static_assert(sizeof(JobFunc) == 64);
