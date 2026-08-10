#pragma once
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "il2cpp/UnityResolve.hpp"

template <typename T>
class RingBuffer {
    std::vector<T> buf;
    size_t cap;
    size_t head = 0;
    size_t cnt = 0;
    mutable std::mutex mtx;
public:
    explicit RingBuffer(size_t capacity = 10) : cap(capacity > 0 ? capacity : 1), buf(capacity > 0 ? capacity : 1) {}

    RingBuffer(const RingBuffer& other) : buf(other.buf), cap(other.cap), head(other.head), cnt(other.cnt) {}

    RingBuffer(RingBuffer&& other) noexcept
        : buf(std::move(other.buf)), cap(other.cap), head(other.head), cnt(other.cnt) {
        other.cnt = 0;
        other.head = 0;
    }

    RingBuffer& operator=(const RingBuffer& other) {
        if (this != &other) {
            std::lock(mtx, other.mtx);
            std::lock_guard<std::mutex> l1(mtx, std::adopt_lock);
            std::lock_guard<std::mutex> l2(other.mtx, std::adopt_lock);
            buf = other.buf;
            cap = other.cap;
            head = other.head;
            cnt = other.cnt;
        }
        return *this;
    }

    RingBuffer& operator=(RingBuffer&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::mutex> lock(mtx);
            buf = std::move(other.buf);
            cap = other.cap;
            head = other.head;
            cnt = other.cnt;
            other.cnt = 0;
            other.head = 0;
        }
        return *this;
    }

    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mtx);
        size_t idx = (head + cnt) % cap;
        if (cnt < cap) {
            cnt++;
        } else {
            head = (head + 1) % cap;
        }
        buf[idx] = item;
    }

    bool empty() const { return cnt == 0; }
    size_t size() const { return cnt; }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx);
        head = 0;
        cnt = 0;
    }

    T& operator[](size_t i) { return buf[(head + i) % cap]; }
    const T& operator[](size_t i) const { return buf[(head + i) % cap]; }

    T& back() { return buf[(head + cnt - 1) % cap]; }
    const T& back() const { return buf[(head + cnt - 1) % cap]; }

    struct Iterator {
        RingBuffer* rb;
        size_t idx;
        Iterator& operator++() { idx++; return *this; }
        T& operator*() { return (*rb)[idx]; }
        T* operator->() { return &(*rb)[idx]; }
        bool operator!=(const Iterator& o) const { return idx != o.idx; }
    };

    Iterator begin() { return {this, 0}; }
    Iterator end() { return {this, cnt}; }

    struct ReverseIterator {
        RingBuffer* rb;
        size_t idx;
        ReverseIterator& operator++() { idx--; return *this; }
        T& operator*() { return (*rb)[idx]; }
        T* operator->() { return &(*rb)[idx]; }
        bool operator!=(const ReverseIterator& o) const { return idx != o.idx; }
    };

    ReverseIterator rbegin() { return {this, cnt > 0 ? cnt - 1 : static_cast<size_t>(-1)}; }
    ReverseIterator rend() { return {this, static_cast<size_t>(-1)}; }
};

struct HookerTrace {
    std::string name;
    float time = 0.f;
    float goneTime = 0.f;
    int hitCount = 0;
    std::string argsStr;
    std::string retStr;
};

struct HookerData {
    int hitCount = 0;
    float time = 0.f;
    UnityResolve::Method* method = nullptr;
    bool backtracing = false;
    RingBuffer<std::vector<std::string>> backtraced{10};
    std::string lastArgs;
    std::string lastRet;
    inline static RingBuffer<HookerTrace> visited{100};
    inline static std::unordered_map<void*, std::set<void*>> collectSet;
    inline static std::mutex traceMtx;
};
