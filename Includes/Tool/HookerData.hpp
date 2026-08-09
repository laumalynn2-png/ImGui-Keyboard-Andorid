#pragma once
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "il2cpp/UnityResolve.hpp"

template <typename T>
class RingBuffer {
    std::deque<T> data;
    size_t cap;
public:
    explicit RingBuffer(size_t capacity = 10) : cap(capacity) {}
    void push(const T& item) {
        data.push_back(item);
        if (data.size() > cap) data.pop_front();
    }
    auto rbegin() { return data.rbegin(); }
    auto rend() { return data.rend(); }
    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
    bool empty() const { return data.empty(); }
    size_t size() const { return data.size(); }
    void clear() { data.clear(); }
    T& back() { return data.back(); }
    T& operator[](size_t i) { return data[i]; }
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
