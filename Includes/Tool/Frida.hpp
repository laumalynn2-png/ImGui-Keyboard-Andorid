#pragma once
#include "HookerData.hpp"
#include "il2cpp/UnityResolve.hpp"

namespace Frida {
    void Init();
    bool Trace(UnityResolve::Method* method, HookerData* data);
    bool Untrace(UnityResolve::Method* method);
    bool isTraced(UnityResolve::Method* method);
}
