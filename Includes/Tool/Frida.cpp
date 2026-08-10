#include "Frida.hpp"
#include <gumpp.hpp>
#include <frida-gum.h>
#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "il2cpp/log.h"

static std::vector<UnityResolve::Method*> s_sortedMethods;
static bool s_methodsSorted = false;

static void ensureMethodsSorted() {
    if (s_methodsSorted) return;
    std::lock_guard<std::mutex> lock(UnityResolve::g_MethodsMtx);
    s_sortedMethods = UnityResolve::g_Methods;
    std::sort(s_sortedMethods.begin(), s_sortedMethods.end(),
        [](UnityResolve::Method* a, UnityResolve::Method* b) {
            return (uintptr_t)a->function < (uintptr_t)b->function;
        });
    s_methodsSorted = true;
}

static UnityResolve::Method* binarySearchClosest(uintptr_t addr) {
    ensureMethodsSorted();
    if (s_sortedMethods.empty()) return nullptr;
    int left = 0;
    int right = (int)s_sortedMethods.size() - 1;
    while (left <= right) {
        int mid = (left + right) / 2;
        uintptr_t methodAddr = (uintptr_t)s_sortedMethods[mid]->function;
        if (methodAddr == addr) return s_sortedMethods[mid];
        if (methodAddr < addr) left = mid + 1;
        else right = mid - 1;
    }
    if (right < 0) return nullptr;
    return s_sortedMethods[right];
}

#ifdef __aarch64__
using GumVectorReg = GumArm64VectorReg;
#else
using GumVectorReg = GumArmVectorReg;
#endif

static bool isFpType(const char* typeName) {
    return strcmp(typeName, "System.Single") == 0 || strcmp(typeName, "System.Double") == 0;
}

static std::string valueToString(UnityResolve::Type* type, void* intVal, const GumVectorReg* fpReg = nullptr) {
    if (!type) return "null";
    const char* typeName = type->name.c_str();

    if (strcmp(typeName, "System.Boolean") == 0)
        return (uintptr_t)intVal ? "true" : "false";
    if (strcmp(typeName, "System.Byte") == 0 || strcmp(typeName, "System.SByte") == 0 ||
        strcmp(typeName, "System.Int16") == 0 || strcmp(typeName, "System.UInt16") == 0 ||
        strcmp(typeName, "System.Int32") == 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", (int32_t)(intptr_t)intVal);
        return buf;
    }
    if (strcmp(typeName, "System.UInt32") == 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u", (uint32_t)(uintptr_t)intVal);
        return buf;
    }
    if (strcmp(typeName, "System.Int64") == 0) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%" PRId64, (int64_t)(intptr_t)intVal);
        return buf;
    }
    if (strcmp(typeName, "System.UInt64") == 0) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%" PRIu64, (uint64_t)(uintptr_t)intVal);
        return buf;
    }
    if (strcmp(typeName, "System.Single") == 0) {
#ifdef __aarch64__
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4g", fpReg ? fpReg->s : 0.f);
        return buf;
#else
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4g", fpReg ? fpReg->s[0] : 0.f);
        return buf;
#endif
    }
    if (strcmp(typeName, "System.Double") == 0) {
#ifdef __aarch64__
        char buf[32];
        snprintf(buf, sizeof(buf), "%.6g", fpReg ? fpReg->d : 0.0);
        return buf;
#else
        char buf[32];
        snprintf(buf, sizeof(buf), "%.6g", fpReg ? fpReg->d[0] : 0.0);
        return buf;
#endif
    }
    if (strcmp(typeName, "System.Char") == 0) {
        char buf[8];
        snprintf(buf, sizeof(buf), "'%c'", (char)(uintptr_t)intVal);
        return buf;
    }
    if (strcmp(typeName, "System.String") == 0) {
        if (!intVal) return "null";
        try {
            auto str = reinterpret_cast<UnityResolve::UnityType::String*>(intVal);
            return "\"" + str->ToString() + "\"";
        } catch (...) { return "?"; }
    }

    if (type->isValueType() || type->isEnum()) {
        try {
            auto klass = type->getClass();
            if (!klass) return "?";
            auto toString = il2cpp_class_get_method_from_name(klass->address, "ToString", 0);
            if (!toString) return "?";
            auto boxed = UnityResolve::GetBoxedValue(klass, &intVal);
            if (!boxed) return "null";
            void* exc = nullptr;
            auto result = il2cpp_runtime_invoke(toString, boxed, nullptr, &exc);
            if (result) {
                auto strObj = (UnityResolve::UnityType::String*)result;
                return strObj->ToString();
            }
            return "null";
        } catch (...) { return "?"; }
    }

    if (!intVal) return "null";
    try {
        auto klass = type->getClass();
        if (!klass) return "?";
        auto toString = il2cpp_class_get_method_from_name(klass->address, "ToString", 0);
        if (!toString) return "?";
        void* exc = nullptr;
        auto result = il2cpp_runtime_invoke(toString, intVal, nullptr, &exc);
        if (result) {
            auto strObj = (UnityResolve::UnityType::String*)result;
            return strObj->ToString();
        }
        return "null";
    } catch (...) { return "?"; }
}

static std::string buildArgsInner(Gum::InvocationContext* context, HookerData* hookerData) {
    auto method = hookerData->method;
    auto& params = method->args;
    bool isStatic = method->static_function;
    std::string inner;

#ifdef __aarch64__
    auto cpuCtx = reinterpret_cast<GumArm64CpuContext*>(context->get_cpu_context());
    int intIdx = isStatic ? 0 : 1;
    int fpIdx = 0;
    int argIdx = isStatic ? 0 : 1;

    for (size_t i = 0; i < params.size(); i++) {
        auto param = params[i];
        if (!param || !param->pType) continue;
        const char* typeName = param->pType->name.c_str();
        std::string valStr;

        if (isFpType(typeName)) {
            const GumVectorReg* fpReg = (fpIdx < 8) ? &cpuCtx->v[fpIdx] : nullptr;
            if (fpReg) {
                valStr = valueToString(param->pType, nullptr, fpReg);
            } else {
                void* argPtr = context->get_nth_argument_ptr(argIdx);
                valStr = valueToString(param->pType, argPtr ? *(void**)argPtr : nullptr);
            }
            fpIdx++;
        } else {
            void* intVal;
            if (intIdx < 8) {
                intVal = (void*)(uintptr_t)cpuCtx->x[intIdx];
            } else {
                void* argPtr = context->get_nth_argument_ptr(argIdx);
                intVal = argPtr ? *(void**)argPtr : nullptr;
            }
            valStr = valueToString(param->pType, intVal);
            intIdx++;
        }
        argIdx++;

        if (!inner.empty()) inner += ", ";
        inner += param->name;
        inner += " = ";
        inner += valStr;
    }
#else
    auto cpuCtx = reinterpret_cast<GumArmCpuContext*>(context->get_cpu_context());
    int intIdx = isStatic ? 0 : 1;
    int fpIdx = 0;
    int argIdx = isStatic ? 0 : 1;

    for (size_t i = 0; i < params.size(); i++) {
        auto param = params[i];
        if (!param || !param->pType) continue;
        const char* typeName = param->pType->name.c_str();
        std::string valStr;

        if (strcmp(typeName, "System.Single") == 0) {
            if (fpIdx < 16) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.4g", cpuCtx->v[fpIdx / 4].s[fpIdx % 4]);
                valStr = buf;
            } else {
                void* argPtr = context->get_nth_argument_ptr(argIdx);
                if (argPtr) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.4g", *(float*)argPtr);
                    valStr = buf;
                } else valStr = "?";
            }
            fpIdx += 1;
        } else if (strcmp(typeName, "System.Double") == 0) {
            if (fpIdx % 2 != 0) fpIdx++;
            if (fpIdx < 16) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.6g", cpuCtx->v[fpIdx / 4].d[(fpIdx / 2) % 2]);
                valStr = buf;
            } else {
                void* argPtr = context->get_nth_argument_ptr(argIdx);
                if (argPtr) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.6g", *(double*)argPtr);
                    valStr = buf;
                } else valStr = "?";
            }
            fpIdx += 2;
        } else {
            void* intVal;
            if (intIdx < 4) {
                intVal = (void*)(uintptr_t)cpuCtx->r[intIdx];
            } else {
                void* argPtr = context->get_nth_argument_ptr(argIdx);
                intVal = argPtr ? *(void**)argPtr : nullptr;
            }
            valStr = valueToString(param->pType, intVal);
            intIdx++;
        }
        argIdx++;

        if (!inner.empty()) inner += ", ";
        inner += param->name;
        inner += " = ";
        inner += valStr;
    }
#endif
    return inner;
}

static std::string buildArgsString(Gum::InvocationContext* context, HookerData* hookerData) {
    std::string inner = buildArgsInner(context, hookerData);
    std::string result;
    result += hookerData->method->name;
    result += "(";
    result += inner;
    result += ")";
    return result;
}

static std::string buildRetString(Gum::InvocationContext* context, HookerData* hookerData) {
    auto retType = hookerData->method->return_type;
    if (!retType || retType->name == "System.Void") return "";

    const char* retTypeName = retType->name.c_str();

#ifdef __aarch64__
    auto cpuCtx = reinterpret_cast<GumArm64CpuContext*>(context->get_cpu_context());
    if (isFpType(retTypeName))
        return valueToString(retType, nullptr, &cpuCtx->v[0]);
    void* retVal = (void*)(uintptr_t)cpuCtx->x[0];
    return valueToString(retType, retVal);
#else
    auto cpuCtx = reinterpret_cast<GumArmCpuContext*>(context->get_cpu_context());
    if (strcmp(retTypeName, "System.Single") == 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4g", cpuCtx->v[0].s[0]);
        return buf;
    }
    if (strcmp(retTypeName, "System.Double") == 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.6g", cpuCtx->v[0].d[0]);
        return buf;
    }
    void* retVal = (void*)(uintptr_t)cpuCtx->r[0];
    return valueToString(retType, retVal);
#endif
}

class TraceListener : public Gum::InvocationListener {
private:
    Gum::RefPtr<Gum::Backtracer> backtracer;

    void doBacktrace(Gum::InvocationContext* context) {
        auto hookerData = context->get_listener_function_data<HookerData>();
        Gum::ReturnAddressArray returnAddresses;
        backtracer->generate(context->get_cpu_context(), returnAddresses);
        LOGD("========================================");
        std::vector<std::string> result;
        for (unsigned int i = 0; i < returnAddresses.len; i++) {
            auto addr = returnAddresses.items[i];
            auto closestMethod = binarySearchClosest((uintptr_t)addr);
            if (closestMethod) {
                intptr_t offset = (intptr_t)addr - (intptr_t)closestMethod->function;
                if (offset < 0) offset = -offset;
                if (offset <= 0x1000) {
                    char buffer[265];
                    snprintf(buffer, sizeof(buffer), "%s::%s+0x%" PRIxPTR,
                             closestMethod->klass ? closestMethod->klass->getFullName().c_str() : "?",
                             closestMethod->name.c_str(), offset);
                    result.push_back(buffer);
                } else {
                    LOGE("Offset too big: %" PRIxPTR, offset);
                    LOGD("addr => %p", (void*)addr);
                }
            } else {
                LOGE("Not found: %p", (void*)addr);
                LOGD("addr => %p", (void*)addr);
            }
        }
        if (!result.empty()) {
            std::lock_guard<std::mutex> lock(HookerData::traceMtx);
            hookerData->backtraced.push(result);
        }
    }

public:
    TraceListener() : backtracer(Gum::Backtracer_make_accurate()) {
        if (!backtracer) {
            LOGE("Failed to create backtracer");
        }
    }

    virtual void on_enter(Gum::InvocationContext* context) {
        auto hookerData = context->get_listener_function_data<HookerData>();
        if (hookerData->backtracing) {
            doBacktrace(context);
        }

        std::string innerArgs = buildArgsInner(context, hookerData);
        std::string argsStr = hookerData->method->name;
        argsStr += "(";
        argsStr += innerArgs;
        argsStr += ")";

        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%p | Method::%s",
                 (void*)hookerData->method->getAbsAddress(),
                 hookerData->method->name.c_str());

        void* thiz = nullptr;
        if (!hookerData->method->static_function) {
            thiz = context->get_nth_argument<void*>(0);
        }

        std::lock_guard<std::mutex> lock(HookerData::traceMtx);
        hookerData->hitCount++;
        hookerData->time = 1.f;
        hookerData->lastArgs = argsStr;

        if (thiz && hookerData->method->klass) {
            HookerData::collectSet[hookerData->method->klass->address].emplace(thiz);
        }

        for (auto it = HookerData::visited.rbegin(); it != HookerData::visited.rend(); ++it) {
            if (it->name == buffer) {
                it->goneTime = 10.f;
                it->time = 2.f;
                it->hitCount++;
                it->argsStr = innerArgs;
                return;
            }
        }
        HookerData::visited.push({buffer, 2.f, 10.f, 0, innerArgs, ""});
    }

    virtual void on_leave(Gum::InvocationContext* context) {
        auto hookerData = context->get_listener_function_data<HookerData>();
        std::string retStr = buildRetString(context, hookerData);

        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%p | Method::%s",
                 (void*)hookerData->method->getAbsAddress(),
                 hookerData->method->name.c_str());

        std::lock_guard<std::mutex> lock(HookerData::traceMtx);
        hookerData->lastRet = retStr;
        for (auto it = HookerData::visited.rbegin(); it != HookerData::visited.rend(); ++it) {
            if (it->name == buffer) {
                it->retStr = retStr;
                break;
            }
        }
    }
};

static Gum::RefPtr<Gum::Interceptor> s_interceptor;
static std::unordered_map<void*, std::unique_ptr<TraceListener>> s_traceListeners;

void Frida::Init() {
    s_interceptor = Gum::Interceptor_obtain();
}

bool Frida::Trace(UnityResolve::Method* method, HookerData* data) {
    if (!method || !method->function) return false;
    auto it = s_traceListeners.find(method->function);
    if (it != s_traceListeners.end()) {
        LOGE("Already traced %s", method->name.c_str());
        return false;
    }
    auto listener = std::make_unique<TraceListener>();
    bool result = s_interceptor->attach(method->function, listener.get(), data);
    if (!result) return false;
    s_traceListeners[method->function] = std::move(listener);
    LOGI("Trace attached: %s::%s (%p)",
         method->klass ? method->klass->name.c_str() : "?",
         method->name.c_str(), method->function);
    return true;
}

bool Frida::Untrace(UnityResolve::Method* method) {
    if (!method || !method->function) return false;
    auto it = s_traceListeners.find(method->function);
    if (it == s_traceListeners.end()) return false;
    s_interceptor->detach(it->second.get());
    s_traceListeners.erase(it);
    LOGI("Trace detached: %s::%s",
         method->klass ? method->klass->name.c_str() : "?",
         method->name.c_str());
    return true;
}

bool Frida::isTraced(UnityResolve::Method* method) {
    if (!method || !method->function) return false;
    return s_traceListeners.find(method->function) != s_traceListeners.end();
}
