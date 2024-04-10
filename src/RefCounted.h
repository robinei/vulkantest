#pragma once

#include <atomic>

class RefCounted {
    std::atomic<int> refcount = 0;

protected:
    virtual ~RefCounted() noexcept { }

public:
    RefCounted() = default;
    RefCounted(const RefCounted &) = delete;
    RefCounted &operator=(const RefCounted &) = delete;

    int getRefCount() const noexcept {
        return refcount;
    }

    void addRef() noexcept {
        ++refcount;
    }

    void release() noexcept {
        if (--refcount <= 0) {
            delete this;
        }
    }
};

template <typename T>
class Ref {
    T *ptr;
public:
    ~Ref() noexcept {
        if (ptr) {
            ptr->release();
        }
    }

    Ref() noexcept : ptr(nullptr) { }
    
    Ref(T *p) noexcept : ptr(p) {
        if (p) {
            p->addRef();
        }
    }
    
    Ref(const Ref &ref) noexcept : ptr(ref.ptr) {
        if (ptr) {
            ptr->addRef();
        }
    }
    
    Ref(Ref &&ref) noexcept : ptr(ref.ptr) {
        ref.ptr = nullptr;
    }

    Ref &operator=(T *p) noexcept {
        if (p) {
            p->addRef();
        }
        if (ptr) {
            ptr->release();
        }
        ptr = p;
        return *this;
    }

    Ref &operator=(const Ref &ref) noexcept {
        if (ref.ptr) {
            ref.ptr->addRef();
        }
        if (ptr) {
            ptr->release();
        }
        ptr = ref.ptr;
        return *this;
    }

    Ref &operator=(Ref &&ref) noexcept {
        if (this != &ref) {
            ptr = ref.ptr;
            ref.ptr = nullptr;
        }
        return *this;
    }

    operator bool() const noexcept { return ptr != nullptr; }
    
    bool operator==(const T *p) const noexcept { return ptr == p; }
    bool operator!=(const T *p) const noexcept { return ptr != p; }
    
    bool operator==(const Ref &ref) const noexcept { return ptr == ref.ptr; }
    bool operator!=(const Ref &ref) const noexcept { return ptr != ref.ptr; }
    
    T *operator->() noexcept { return ptr; }
    const T *operator->() const noexcept { return ptr; }

    T &operator*() noexcept { return *ptr; }
    const T &operator*() const noexcept { return *ptr; }

    T *get() noexcept { return ptr; }
};

