#pragma once
#include <dlfcn.h>
#include "il2cpp/UnityResolve.hpp"
#include "il2cpp/log.h"

#define REPLACE_METHOD(method, to) \
    [&] { \
        void* old = method->function; \
        void* n = method->replace(to); \
        LOGI("%s::%s (%p -> %p) HOOKED", \
             method->klass ? method->klass->name.c_str() : "?", \
             method->name.c_str(), old, n); \
        return n; \
    }()

#define REPLACE_NAME_METHOD_ORIG(method, to, orig) \
    orig = (decltype(orig))REPLACE_METHOD(method, to)

#define REPLACE_NAME_KLASS(klass, methodName, to) \
    REPLACE_METHOD(klass->Get<UnityResolve::Method>(methodName), to)

#define REPLACE_NAME_KLASS_ORIG(klass, methodName, to, orig) \
    REPLACE_NAME_METHOD_ORIG(klass->Get<UnityResolve::Method>(methodName), to, orig)

#define REPLACE_KLASS(klass, to) \
    REPLACE_NAME_KLASS(klass, #to, to)

#define REPLACE_NAME(className, methodName, to) \
    REPLACE_NAME_KLASS(UnityResolve::FindClass(className), methodName, to)

#define REPLACE_NAME_ORIG(className, methodName, to, orig) \
    REPLACE_NAME_KLASS_ORIG(UnityResolve::FindClass(className), methodName, to, orig)

#define REPLACE(className, to) \
    REPLACE_NAME(className, #to, to)

#define REPLACE_ORIG(className, to, orig) \
    REPLACE_NAME_ORIG(className, #to, to, orig)

inline bool IsLibraryLoaded(const char* libName) {
    void* handle = dlopen(libName, RTLD_NOW | RTLD_NOLOAD);
    if (handle) { dlclose(handle); return true; }
    return false;
}
