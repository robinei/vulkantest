#pragma once

#include <atomic>

class RefCounted {
    std::atomic<int> refcount;
public:
    virtual ~RefCounted() { }
    RefCounted() : refcount(1) { }

    void addRef() {
        ++refcount;
    }

    void release() {
        if (--refcount == 0) {
            delete this;
        }
    }
};

template <typename T>
class Ref {
    T *ptr;
public:
    ~Ref() {
        if (ptr) {
            ptr->release();
        }
    }

    Ref() : ptr(nullptr) { }
    
    Ref(T *p) : ptr(p) {
        if (p) {
            p->addRef();
        }
    }
    
    Ref(const Ref &ref) : ptr(ref.ptr) {
        if (ptr) {
            ptr->addRef();
        }
    }
    
    Ref(Ref &&ref) : ptr(ref.ptr) {
        ref.ptr = nullptr;
    }

    Ref &operator=(T *p) {
        if (p) {
            p->addRef();
        }
        if (ptr) {
            ptr->release();
        }
        ptr = p;
        return *this;
    }

    Ref &operator=(const Ref &ref) {
        if (ref.ptr) {
            ref.ptr->addRef();
        }
        if (ptr) {
            ptr->release();
        }
        ptr = ref.ptr;
        return *this;
    }

    Ref &operator=(Ref &&ref) {
        if (this != &ref) {
            ptr = ref.ptr;
            ref.ptr = nullptr;
        }
        return *this;
    }

    operator bool() const { return ptr != nullptr; }
    
    bool operator==(const T *p) const { return ptr == p; }
    bool operator!=(const T *p) const { return ptr != p; }
    
    bool operator==(const Ref &ref) const { return ptr == ref.ptr; }
    bool operator!=(const Ref &ref) const { return ptr != ref.ptr; }
    
    T *operator->() { return ptr; }
    const T *operator->() const { return ptr; }

    T &operator*() { return *ptr; }
    const T &operator*() const { return *ptr; }

    T *get() { return ptr; }
};

