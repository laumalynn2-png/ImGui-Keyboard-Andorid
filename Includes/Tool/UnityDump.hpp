#pragma once
#include <json.hpp>
#include <string>
#include <unordered_set>
#include <vector>
#include "il2cpp/UnityResolve.hpp"

namespace UnityDump {
    struct DumpResult {
        void* obj;
        UnityResolve::Class* klass;
        nlohmann::ordered_json json;
    };

    DumpResult dumpObject(void* obj, const std::vector<std::string>& paths = {});
    std::string readString(void* strObj);
    std::string boxAndToString(void* klass, void* data);
    void SetMaxArraySize(size_t size);
}
