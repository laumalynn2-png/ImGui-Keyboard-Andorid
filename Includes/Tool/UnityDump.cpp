#include "UnityDump.hpp"
#include <cstring>
#include "il2cpp/log.h"

namespace UnityDump {

std::string readString(void* strObj) {
    if (!strObj || !il2cpp_string_length || !il2cpp_string_chars) return "null";
    auto len = il2cpp_string_length(strObj);
    auto chars = il2cpp_string_chars(strObj);
    if (!chars || len <= 0) return "";
    std::string result;
    result.reserve(len);
    for (int i = 0; i < len; i++) {
        auto wc = reinterpret_cast<const uint16_t*>(chars)[i];
        if (wc < 128) result += static_cast<char>(wc);
        else result += '?';
    }
    return result;
}

std::string boxAndToString(void* klass, void* data) {
    if (!klass || !il2cpp_value_box) return "?";
    auto boxed = il2cpp_value_box(klass, data);
    if (!boxed) return "null";
    auto objectClass = il2cpp_object_get_class ? il2cpp_object_get_class(boxed) : nullptr;
    if (!objectClass) return "?";
    void* iter = nullptr;
    void* toStringMethod = nullptr;
    while (auto m = il2cpp_class_get_methods(objectClass, &iter)) {
        const char* name = il2cpp_method_get_name ? il2cpp_method_get_name(m) : nullptr;
        if (name && strcmp(name, "ToString") == 0) {
            auto pc = il2cpp_method_get_param_count ? il2cpp_method_get_param_count(m) : 0;
            if (pc == 0) { toStringMethod = m; break; }
        }
    }
    if (!toStringMethod || !il2cpp_runtime_invoke) return "?";
    void* exc = nullptr;
    auto result = il2cpp_runtime_invoke(toStringMethod, boxed, nullptr, &exc);
    if (result) return readString(result);
    return "null";
}

static nlohmann::ordered_json dumpValue(void* obj, size_t offset, void* fieldType, std::unordered_set<void*>& visited);

static nlohmann::ordered_json dumpObjectInner(void* obj, void* klass, std::unordered_set<void*>& visited) {
    nlohmann::ordered_json result = nlohmann::ordered_json::object();
    if (!obj || !klass || !il2cpp_class_get_fields) return result;

    void* currentClass = klass;
    while (currentClass) {
        void* iter = nullptr;
        while (auto field = il2cpp_class_get_fields(currentClass, &iter)) {
            if (!il2cpp_field_get_flags) break;
            auto flags = il2cpp_field_get_flags(field);
            if (flags & 0x0010) continue;

            const char* fieldName = il2cpp_field_get_name ? il2cpp_field_get_name(field) : nullptr;
            if (!fieldName) continue;
            auto fieldType = il2cpp_field_get_type ? il2cpp_field_get_type(field) : nullptr;
            if (!fieldType) continue;
            size_t fieldOffset = il2cpp_field_get_offset ? il2cpp_field_get_offset(field) : 0;

            result[fieldName] = dumpValue(obj, fieldOffset, fieldType, visited);
        }
        currentClass = il2cpp_class_get_parent ? il2cpp_class_get_parent(currentClass) : nullptr;
        if (currentClass) {
            const char* parentName = il2cpp_class_get_name ? il2cpp_class_get_name(currentClass) : nullptr;
            if (parentName && strcmp(parentName, "Object") == 0) currentClass = nullptr;
        }
    }
    return result;
}

static nlohmann::ordered_json dumpValue(void* obj, size_t offset, void* fieldType, std::unordered_set<void*>& visited) {
    if (!fieldType || !il2cpp_type_get_type) return "null";
    auto typeKind = il2cpp_type_get_type(fieldType);
    auto ptr = reinterpret_cast<char*>(obj) + offset;

    switch (typeKind) {
        case IL2CPP_TYPE_BOOLEAN:
            return *reinterpret_cast<bool*>(ptr);
        case IL2CPP_TYPE_CHAR: {
            auto c = *reinterpret_cast<uint16_t*>(ptr);
            return std::string(1, c < 128 ? static_cast<char>(c) : '?');
        }
        case IL2CPP_TYPE_I1:
            return static_cast<int64_t>(*reinterpret_cast<int8_t*>(ptr));
        case IL2CPP_TYPE_U1:
            return static_cast<uint64_t>(*reinterpret_cast<uint8_t*>(ptr));
        case IL2CPP_TYPE_I2:
            return static_cast<int64_t>(*reinterpret_cast<int16_t*>(ptr));
        case IL2CPP_TYPE_U2:
            return static_cast<uint64_t>(*reinterpret_cast<uint16_t*>(ptr));
        case IL2CPP_TYPE_I4:
            return static_cast<int64_t>(*reinterpret_cast<int32_t*>(ptr));
        case IL2CPP_TYPE_U4:
            return static_cast<uint64_t>(*reinterpret_cast<uint32_t*>(ptr));
        case IL2CPP_TYPE_I8:
            return *reinterpret_cast<int64_t*>(ptr);
        case IL2CPP_TYPE_U8:
            return *reinterpret_cast<uint64_t*>(ptr);
        case IL2CPP_TYPE_R4:
            return *reinterpret_cast<float*>(ptr);
        case IL2CPP_TYPE_R8:
            return *reinterpret_cast<double*>(ptr);
        case IL2CPP_TYPE_STRING: {
            auto strObj = *reinterpret_cast<void**>(ptr);
            if (!strObj) return "null";
            return readString(strObj);
        }
        case IL2CPP_TYPE_SZARRAY:
        case IL2CPP_TYPE_ARRAY: {
            auto arrObj = *reinterpret_cast<void**>(ptr);
            if (!arrObj) return "null";
            if (visited.count(arrObj)) return "circular";
            visited.insert(arrObj);
            nlohmann::ordered_json arr = nlohmann::ordered_json::array();
            auto len = il2cpp_array_length ? il2cpp_array_length(arrObj) : 0;
            for (uint32_t i = 0; i < len && i < 50; i++) {
                auto elemPtr = reinterpret_cast<char*>(arrObj) + 16 + i * sizeof(void*);
                auto elem = *reinterpret_cast<void**>(elemPtr);
                if (!elem) { arr.push_back("null"); continue; }
                if (visited.count(elem)) { arr.push_back("circular"); continue; }
                visited.insert(elem);
                auto elemClass = il2cpp_object_get_class ? il2cpp_object_get_class(elem) : nullptr;
                arr.push_back(elemClass ? dumpObjectInner(elem, elemClass, visited) : "null");
                visited.erase(elem);
            }
            visited.erase(arrObj);
            return arr;
        }
        case IL2CPP_TYPE_CLASS:
        case IL2CPP_TYPE_OBJECT: {
            auto childObj = *reinterpret_cast<void**>(ptr);
            if (!childObj) return "null";
            if (visited.count(childObj)) return "circular";
            visited.insert(childObj);
            auto childClass = il2cpp_object_get_class ? il2cpp_object_get_class(childObj) : nullptr;
            auto json = childClass ? dumpObjectInner(childObj, childClass, visited) : "null";
            visited.erase(childObj);
            return json;
        }
        case IL2CPP_TYPE_VALUETYPE: {
            auto klass = il2cpp_class_from_type ? il2cpp_class_from_type(fieldType) : nullptr;
            if (!klass) return "?";
            return boxAndToString(klass, ptr);
        }
        case IL2CPP_TYPE_ENUM: {
            auto klass = il2cpp_class_from_type ? il2cpp_class_from_type(fieldType) : nullptr;
            if (!klass) return 0;
            auto underlying = il2cpp_class_enum_basetype ? il2cpp_class_enum_basetype(klass) : nullptr;
            if (!underlying) return 0;
            auto baseKind = il2cpp_type_get_type(underlying);
            switch (baseKind) {
                case IL2CPP_TYPE_I4: return static_cast<int64_t>(*reinterpret_cast<int32_t*>(ptr));
                case IL2CPP_TYPE_U4: return static_cast<uint64_t>(*reinterpret_cast<uint32_t*>(ptr));
                case IL2CPP_TYPE_I8: return *reinterpret_cast<int64_t*>(ptr);
                case IL2CPP_TYPE_U8: return *reinterpret_cast<uint64_t*>(ptr);
                case IL2CPP_TYPE_I1: return static_cast<int64_t>(*reinterpret_cast<int8_t*>(ptr));
                case IL2CPP_TYPE_U1: return static_cast<uint64_t>(*reinterpret_cast<uint8_t*>(ptr));
                case IL2CPP_TYPE_I2: return static_cast<int64_t>(*reinterpret_cast<int16_t*>(ptr));
                case IL2CPP_TYPE_U2: return static_cast<uint64_t>(*reinterpret_cast<uint16_t*>(ptr));
                default: return 0;
            }
        }
        case IL2CPP_TYPE_PTR: {
            auto val = *reinterpret_cast<void**>(ptr);
            char buf[32];
            snprintf(buf, sizeof(buf), "%p", val);
            return buf;
        }
        default: {
            auto klass = il2cpp_class_from_type ? il2cpp_class_from_type(fieldType) : nullptr;
            if (klass && il2cpp_class_is_valuetype && il2cpp_class_is_valuetype(klass)) {
                return boxAndToString(klass, ptr);
            }
            auto childObj = *reinterpret_cast<void**>(ptr);
            if (!childObj) return "null";
            if (visited.count(childObj)) return "circular";
            visited.insert(childObj);
            auto childClass = il2cpp_object_get_class ? il2cpp_object_get_class(childObj) : nullptr;
            auto json = childClass ? dumpObjectInner(childObj, childClass, visited) : "null";
            visited.erase(childObj);
            return json;
        }
    }
}

DumpResult dumpObject(void* obj, const std::vector<std::string>& paths) {
    DumpResult result;
    result.obj = obj;
    result.klass = nullptr;
    result.json = "null";

    if (!obj) return result;

    void* currentObj = obj;
    void* currentClass = il2cpp_object_get_class ? il2cpp_object_get_class(obj) : nullptr;

    for (const auto& path : paths) {
        if (!currentObj || !currentClass || !il2cpp_class_get_fields) break;
        bool found = false;
        void* searchClass = currentClass;
        while (searchClass && !found) {
            void* iter = nullptr;
            while (auto field = il2cpp_class_get_fields(searchClass, &iter)) {
                const char* fieldName = il2cpp_field_get_name ? il2cpp_field_get_name(field) : nullptr;
                if (fieldName && path == fieldName) {
                    auto fieldType = il2cpp_field_get_type ? il2cpp_field_get_type(field) : nullptr;
                    size_t fieldOffset = il2cpp_field_get_offset ? il2cpp_field_get_offset(field) : 0;
                    auto typeKind = (fieldType && il2cpp_type_get_type) ? il2cpp_type_get_type(fieldType) : 0;

                    if (typeKind == IL2CPP_TYPE_VALUETYPE || typeKind == IL2CPP_TYPE_ENUM) {
                        currentObj = reinterpret_cast<char*>(currentObj) + fieldOffset;
                        currentClass = il2cpp_class_from_type ? il2cpp_class_from_type(fieldType) : nullptr;
                    } else {
                        auto childPtr = *reinterpret_cast<void**>(reinterpret_cast<char*>(currentObj) + fieldOffset);
                        if (!childPtr) {
                            result.obj = nullptr;
                            result.klass = nullptr;
                            result.json = "null";
                            return result;
                        }
                        currentObj = childPtr;
                        currentClass = il2cpp_object_get_class ? il2cpp_object_get_class(currentObj) : nullptr;
                    }
                    found = true;
                    break;
                }
            }
            if (!found) searchClass = il2cpp_class_get_parent ? il2cpp_class_get_parent(searchClass) : nullptr;
        }
        if (!found) {
            result.obj = nullptr;
            result.klass = nullptr;
            result.json = "null";
            return result;
        }
    }

    std::unordered_set<void*> visited;
    visited.insert(obj);
    result.obj = currentObj;
    result.klass = currentClass ? UnityResolve::GetOrCreateClass(currentClass) : nullptr;
    result.json = dumpObjectInner(currentObj, currentClass, visited);
    return result;
}

}
