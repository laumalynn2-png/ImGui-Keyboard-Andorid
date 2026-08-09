#include "il2cpp_dump.h"
#include "UnityResolve.hpp"
#include "log.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <regex>
#include "xdl/include/xdl.h"
#include "il2cpp-tabledefs.h"

#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

static uint64_t il2cpp_base = 0;

void init_il2cpp_api(void *handle) {
#define DO_API(r, n, p) {                      \
    n = (r (*) p)xdl_sym(handle, #n, nullptr); \
    if(!n) {                                   \
        LOGW("api not found %s", #n);          \
    }                                          \
}

#include "il2cpp-api-functions.h"

#undef DO_API
}

std::string get_method_modifier(uint32_t flags) {
    std::stringstream outPut;
    auto access = flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (access) {
        case METHOD_ATTRIBUTE_PRIVATE:
            outPut << "private ";
            break;
        case METHOD_ATTRIBUTE_PUBLIC:
            outPut << "public ";
            break;
        case METHOD_ATTRIBUTE_FAMILY:
            outPut << "protected ";
            break;
        case METHOD_ATTRIBUTE_ASSEM:
        case METHOD_ATTRIBUTE_FAM_AND_ASSEM:
            outPut << "internal ";
            break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & METHOD_ATTRIBUTE_STATIC) {
        outPut << "static ";
    }
    if (flags & METHOD_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_FINAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "sealed override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_VIRTUAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT) {
            outPut << "virtual ";
        } else {
            outPut << "override ";
        }
    }
    if (flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) {
        outPut << "extern ";
    }
    return outPut.str();
}

bool _il2cpp_type_is_byref(void *type) {
    bool byref = (((uint8_t*)type)[11] >> 6) & 1;
    if (il2cpp_type_is_byref) {
        byref = il2cpp_type_is_byref(type);
    }
    return byref;
}

std::string dump_method(void *klass) {
    std::stringstream outPut;
    void *iter = nullptr;
    bool first = true;
    while (auto method = il2cpp_class_get_methods(klass, &iter)) {
        if (first) {
            outPut << "\n\t// Methods\n";
            first = false;
        } else {
            outPut << "\n";
        }

        void *methodPointer = *(void**)method;
        if (methodPointer) {
            outPut << "\t// RVA: 0x";
            outPut << std::hex << (uint64_t)methodPointer - il2cpp_base;
            outPut << " VA: 0x";
            outPut << std::hex << (uint64_t)methodPointer;
        } else {
            outPut << "\t// RVA: 0x VA: 0x0";
        }
        outPut << "\n\t";
        uint32_t iflags = 0;
        auto flags = il2cpp_method_get_flags(method, &iflags);
        outPut << get_method_modifier(flags);
        auto return_type = il2cpp_method_get_return_type(method);
        if (_il2cpp_type_is_byref(return_type)) {
            outPut << "ref ";
        }
        auto return_class = il2cpp_class_from_type(return_type);
        outPut << il2cpp_class_get_name(return_class) << " " << il2cpp_method_get_name(method)
               << "(";
        auto param_count = il2cpp_method_get_param_count(method);
        for (uint32_t i = 0; i < param_count; ++i) {
            auto param = il2cpp_method_get_param(method, i);

            uint16_t attrs = *(uint16_t*)((uint8_t*)param + 8);
            if (_il2cpp_type_is_byref(param)) {
                if (attrs & PARAM_ATTRIBUTE_OUT && !(attrs & PARAM_ATTRIBUTE_IN)) {
                    outPut << "out ";
                } else if (attrs & PARAM_ATTRIBUTE_IN && !(attrs & PARAM_ATTRIBUTE_OUT)) {
                    outPut << "in ";
                } else {
                    outPut << "ref ";
                }
            } else {
                if (attrs & PARAM_ATTRIBUTE_IN) {
                    outPut << "[In] ";
                }
                if (attrs & PARAM_ATTRIBUTE_OUT) {
                    outPut << "[Out] ";
                }
            }
            auto parameter_class = il2cpp_class_from_type(param);
            outPut << il2cpp_class_get_name(parameter_class) << " "
                   << il2cpp_method_get_param_name(method, i);
            outPut << ", ";
        }
        if (param_count > 0) {
            outPut.seekp(-2, outPut.cur);
        }
        outPut << ") { }\n";
    }
    return outPut.str();
}

std::string dump_property(void *klass) {
    std::stringstream outPut;
    void *iter = nullptr;
    bool first = true;
    while (auto prop_const = il2cpp_class_get_properties(klass, &iter)) {
        auto prop = const_cast<void*>(prop_const);
        auto getter = il2cpp_property_get_get_method(prop);
        auto setter = il2cpp_property_get_set_method(prop);
        auto prop_name = il2cpp_property_get_name(prop);
        if (first) {
            outPut << "\n\t// Properties\n";
            first = false;
        }
        outPut << "\t";
        void *prop_class = nullptr;
        uint32_t iflags = 0;
        if (getter) {
            outPut << get_method_modifier(il2cpp_method_get_flags(getter, &iflags));
            prop_class = il2cpp_class_from_type(il2cpp_method_get_return_type(getter));
        } else if (setter) {
            outPut << get_method_modifier(il2cpp_method_get_flags(setter, &iflags));
            auto param = il2cpp_method_get_param(setter, 0);
            prop_class = il2cpp_class_from_type(param);
        }
        if (prop_class) {
            outPut << il2cpp_class_get_name(prop_class) << " " << prop_name << " { ";
            if (getter) {
                outPut << "get; ";
            }
            if (setter) {
                outPut << "set; ";
            }
            outPut << "}\n";
        } else {
            if (prop_name) {
                outPut << " // unknown property " << prop_name;
            }
        }
    }
    return outPut.str();
}

std::string dump_field(void *klass) {
    std::stringstream outPut;
    auto is_enum = il2cpp_class_is_enum(klass);
    void *iter = nullptr;
    bool first = true;
    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        if (first) {
            outPut << "\n\t// Fields\n";
            first = false;
        }
        outPut << "\t";
        auto attrs = il2cpp_field_get_flags(field);
        auto access = attrs & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK;
        switch (access) {
            case FIELD_ATTRIBUTE_PRIVATE:
                outPut << "private ";
                break;
            case FIELD_ATTRIBUTE_PUBLIC:
                outPut << "public ";
                break;
            case FIELD_ATTRIBUTE_FAMILY:
                outPut << "protected ";
                break;
            case FIELD_ATTRIBUTE_ASSEMBLY:
            case FIELD_ATTRIBUTE_FAM_AND_ASSEM:
                outPut << "internal ";
                break;
            case FIELD_ATTRIBUTE_FAM_OR_ASSEM:
                outPut << "protected internal ";
                break;
        }
        if (attrs & FIELD_ATTRIBUTE_LITERAL) {
            outPut << "const ";
        } else {
            if (attrs & FIELD_ATTRIBUTE_STATIC) {
                outPut << "static ";
            }
            if (attrs & FIELD_ATTRIBUTE_INIT_ONLY) {
                outPut << "readonly ";
            }
        }
        auto field_type = il2cpp_field_get_type(field);
        auto field_class = il2cpp_class_from_type(field_type);
        outPut << il2cpp_class_get_name(field_class) << " " << il2cpp_field_get_name(field);
        if (attrs & FIELD_ATTRIBUTE_LITERAL && is_enum) {
            uint64_t val = 0;
            il2cpp_field_static_get_value(field, &val);
            outPut << " = " << std::dec << val;
        }
        outPut << "; // 0x" << std::hex << il2cpp_field_get_offset(field) << "\n";
    }
    return outPut.str();
}

std::string dump_type(void *type) {
    std::stringstream outPut;
    auto *klass = il2cpp_class_from_type(type);
    outPut << "\n// Namespace: " << il2cpp_class_get_namespace(klass) << "\n";
    auto flags = il2cpp_class_get_flags(klass);
    if (flags & TYPE_ATTRIBUTE_SERIALIZABLE) {
        outPut << "[Serializable]\n";
    }
    auto is_valuetype = il2cpp_class_is_valuetype(klass);
    auto is_enum = il2cpp_class_is_enum(klass);
    auto visibility = flags & TYPE_ATTRIBUTE_VISIBILITY_MASK;
    switch (visibility) {
        case TYPE_ATTRIBUTE_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_PUBLIC:
            outPut << "public ";
            break;
        case TYPE_ATTRIBUTE_NOT_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM:
        case TYPE_ATTRIBUTE_NESTED_ASSEMBLY:
            outPut << "internal ";
            break;
        case TYPE_ATTRIBUTE_NESTED_PRIVATE:
            outPut << "private ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAMILY:
            outPut << "protected ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & TYPE_ATTRIBUTE_ABSTRACT && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "static ";
    } else if (!(flags & TYPE_ATTRIBUTE_INTERFACE) && flags & TYPE_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
    } else if (!is_valuetype && !is_enum && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "sealed ";
    }
    if (flags & TYPE_ATTRIBUTE_INTERFACE) {
        outPut << "interface ";
    } else if (is_enum) {
        outPut << "enum ";
    } else if (is_valuetype) {
        outPut << "struct ";
    } else {
        outPut << "class ";
    }
    outPut << il2cpp_class_get_name(klass);
    std::vector<std::string> extends;
    auto parent = il2cpp_class_get_parent(klass);
    if (!is_valuetype && !is_enum && parent) {
        auto parent_type = il2cpp_class_get_type(parent);

        if (parent_type && ((uint8_t*)parent_type)[10] != 0x1c) {
            extends.emplace_back(il2cpp_class_get_name(parent));
        }
    }
    void *iter_iface = nullptr;
    while (auto itf = il2cpp_class_get_interfaces(klass, &iter_iface)) {
        extends.emplace_back(il2cpp_class_get_name(itf));
    }
    if (!extends.empty()) {
        outPut << " : " << extends[0];
        for (size_t i = 1; i < extends.size(); ++i) {
            outPut << ", " << extends[i];
        }
    }
    auto fieldStr = dump_field(klass);
    auto propStr = dump_property(klass);
    auto methodStr = dump_method(klass);
    if (fieldStr.empty() && propStr.empty() && methodStr.empty()) {
        return "";
    }
    outPut << "\n{";
    outPut << fieldStr;
    outPut << propStr;
    outPut << methodStr;
    outPut << "}\n";
    return outPut.str();
}

void il2cpp_api_init(void *handle) {
    LOGI("il2cpp_handle: %p", handle);
    init_il2cpp_api(handle);
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        }
        LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
        UnityResolve::il2cpp_base = il2cpp_base;
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return;
    }
    while (!il2cpp_is_vm_thread(nullptr)) {
        LOGI("Waiting for il2cpp_init...");
        sleep(1);
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
}

void il2cpp_dump() {
    LOGI("dumping...");

    auto pathStr = UnityResolve::UnityType::Application::get_persistentDataPath();
    std::string outDir = pathStr->ToString();

    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    std::stringstream imageOutput;
    for (size_t i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        imageOutput << "// Image " << i << ": " << il2cpp_image_get_name(image) << "\n";
    }
    std::vector<std::string> outPuts;
    if (il2cpp_image_get_class) {
        LOGI("Version greater than 2018.3");
        for (size_t i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            imageStr << "\n// Dll : " << il2cpp_image_get_name(image);
            auto classCount = il2cpp_image_get_class_count(image);
            for (size_t j = 0; j < classCount; ++j) {
                auto klass = il2cpp_image_get_class(image, j);
                auto type = il2cpp_class_get_type(const_cast<void*>(klass));
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
            }
        }
    } else {
        LOGI("Version less than 2018.3");
        auto corlib = il2cpp_get_corlib();
        auto assemblyClass = il2cpp_class_from_name(corlib, "System.Reflection", "Assembly");
        auto assemblyLoad = il2cpp_class_get_method_from_name(assemblyClass, "Load", 1);
        auto assemblyGetTypes = il2cpp_class_get_method_from_name(assemblyClass, "GetTypes", 0);

        void *loadPtr = assemblyLoad ? *(void**)assemblyLoad : nullptr;
        void *getTypesPtr = assemblyGetTypes ? *(void**)assemblyGetTypes : nullptr;
        if (loadPtr) {
            LOGI("Assembly::Load: %p", loadPtr);
        } else {
            LOGI("miss Assembly::Load");
            return;
        }
        if (getTypesPtr) {
            LOGI("Assembly::GetTypes: %p", getTypesPtr);
        } else {
            LOGI("miss Assembly::GetTypes");
            return;
        }
        typedef void *(*Assembly_Load_ftn)(void *, void *, void *);
        typedef void *(*Assembly_GetTypes_ftn)(void *, void *);
        auto assemblyLoad_fn = (Assembly_Load_ftn)loadPtr;
        auto assemblyGetTypes_fn = (Assembly_GetTypes_ftn)getTypesPtr;
        for (size_t i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            auto image_name = il2cpp_image_get_name(image);
            imageStr << "\n// Dll : " << image_name;
            auto imageName = std::string(image_name);
            auto pos = imageName.rfind('.');
            auto imageNameNoExt = imageName.substr(0, pos);
            auto assemblyFileName = il2cpp_string_new(imageNameNoExt.data());
            auto reflectionAssembly = assemblyLoad_fn(nullptr, assemblyFileName, nullptr);
            auto reflectionTypes = assemblyGetTypes_fn(reflectionAssembly, nullptr);

            auto max_length = *(size_t*)((uint8_t*)reflectionTypes + 24);
            auto items = (void**)((uint8_t*)reflectionTypes + 32);
            for (size_t j = 0; j < max_length; ++j) {
                auto klass = il2cpp_class_from_system_type(items[j]);
                auto type = il2cpp_class_get_type(klass);
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
            }
        }
    }
    LOGI("write dump file");
    auto outPath = outDir + "/dump.cs";
    std::ofstream outStream(outPath);
    outStream << imageOutput.str();
    auto count = outPuts.size();
    for (size_t i = 0; i < count; ++i) {
        outStream << outPuts[i];
    }
    outStream.close();
    LOGI("dump done! Output: %s", outPath.c_str());
}

namespace UnityVersion {
	static const std::regex pattern("(20\\d{2}|\\d)\\.(\\d)\\.(\\d{1,2})(?:[abcfp]|rc){0,2}\\d?");

	std::string find(const std::string& str) {
		std::smatch match;
		std::regex_search(str, match, pattern);
		return match.empty() ? "" : match[0].str();
	}

	bool gte(const std::string& a, const std::string& b) {
		return compare(a, b) >= 0;
	}

	bool lt(const std::string& a, const std::string& b) {
		return compare(a, b) < 0;
	}

	int compare(const std::string& a, const std::string& b) {
		std::smatch aMatches, bMatches;
		std::regex_search(a, aMatches, pattern);
		std::regex_search(b, bMatches, pattern);
		for (int i = 1; i <= 3; ++i) {
			int aValue = std::stoi(aMatches[i].str());
			int bValue = std::stoi(bMatches[i].str());
			if (aValue > bValue) return 1;
			else if (aValue < bValue) return -1;
		}
		return 0;
	}
}

std::string UnityResolve::getUnityVersion() {
	static Class* appClass = nullptr;
	static Method* method = nullptr;
	if (!appClass) appClass = FindClass("UnityEngine.Application");
	if (!method && appClass && appClass->address) method = appClass->Get<Method>("get_unityVersion");
	if (method) {
		auto str = method->Invoke<UnityType::String*>();
		if (str) return str->ToString();
	}
	return "unknown";
}

std::string UnityResolve::getDataPath() {
	auto str = UnityType::Application::get_persistentDataPath();
	if (str) return str->ToString();
	return "";
}

std::string UnityResolve::getPackageName() {
	static Class* appClass = nullptr;
	static Method* method = nullptr;
	if (!appClass) appClass = FindClass("UnityEngine.Application");
	if (!method && appClass && appClass->address) method = appClass->Get<Method>("get_identifier");
	if (method) {
		auto str = method->Invoke<UnityType::String*>();
		if (str) return str->ToString();
	}
	return "unknown";
}

std::string UnityResolve::getGameVersion() {
	static Class* appClass = nullptr;
	static Method* method = nullptr;
	if (!appClass) appClass = FindClass("UnityEngine.Application");
	if (!method && appClass && appClass->address) method = appClass->Get<Method>("get_version");
	if (method) {
		auto str = method->Invoke<UnityType::String*>();
		if (str) return str->ToString();
	}
	return "unknown";
}

std::string UnityResolve::getProductName() {
	static Class* appClass = nullptr;
	static Method* method = nullptr;
	if (!appClass) appClass = FindClass("UnityEngine.Application");
	if (!method && appClass && appClass->address) method = appClass->Get<Method>("get_productName");
	if (method) {
		auto str = method->Invoke<UnityType::String*>();
		if (str) return str->ToString();
	}
	return "unknown";
}

UnityResolve::Class* UnityResolve::FindClass(const std::string& fullName) {
	if (!il2cpp_domain_get || !il2cpp_domain_get_assemblies) return nullptr;
	auto domain = il2cpp_domain_get();
	if (!domain) return nullptr;
	size_t size = 0;
	auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
	if (!assemblies) return nullptr;
	for (decltype(size) i = 0; i < size; i++) {
		auto ptr = assemblies[i];
		if (!ptr) continue;
		auto image = il2cpp_assembly_get_image(ptr);
		if (!image) continue;
		auto count = il2cpp_image_get_class_count(image);
		for (decltype(count) j = 0; j < count; j++) {
			auto klass = il2cpp_image_get_class(image, j);
			if (!klass) continue;
			auto ns = il2cpp_class_get_namespace(klass);
			auto name = il2cpp_class_get_name(klass);
			std::string full;
			if (ns && *ns) full = std::string(ns) + "." + name;
			else full = name;
			if (full == fullName) {
				il2cpp_free(assemblies);
				return GetOrCreateClass(klass);
			}
		}
	}
	il2cpp_free(assemblies);
	return nullptr;
}

UnityResolve::Class* UnityResolve::GetClassParent(Class* klass) {
	if (!klass || !klass->address || !il2cpp_class_get_parent) return nullptr;
	auto parent = il2cpp_class_get_parent(klass->address);
	if (!parent) return nullptr;
	return GetOrCreateClass(parent);
}

UnityResolve::Class* UnityResolve::GetObjectClass(void* obj) {
	if (!obj || !il2cpp_object_get_class) return nullptr;
	auto klass = il2cpp_object_get_class(obj);
	if (!klass) return nullptr;
	return GetOrCreateClass(klass);
}

bool UnityResolve::IsClassParentOf(Class* klass, Class* parent) {
	if (!klass || !parent || !il2cpp_class_get_parent) return false;
	void* k = klass->address;
	int maxDepth = 50;
	while (k && maxDepth-- > 0) {
		if (k == parent->address) return true;
		k = il2cpp_class_get_parent(k);
	}
	return false;
}

bool UnityResolve::GetClassIsValueType(Class* klass) {
	if (!klass || !klass->address || !il2cpp_class_is_valuetype) return false;
	return il2cpp_class_is_valuetype(klass->address);
}

bool UnityResolve::GetClassIsEnum(Class* klass) {
	if (!klass || !klass->address || !il2cpp_class_is_enum) return false;
	return il2cpp_class_is_enum(klass->address);
}

bool UnityResolve::GetClassIsStatic(Class* klass) {
	if (!klass || !klass->address || !il2cpp_class_get_flags) return false;
	auto flags = il2cpp_class_get_flags(klass->address);
	return (flags & TYPE_ATTRIBUTE_ABSTRACT) && (flags & TYPE_ATTRIBUTE_SEALED);
}

int32_t UnityResolve::GetClassValueSize(Class* klass) {
	if (!klass || !klass->address || !il2cpp_class_value_size) return 0;
	uint32_t align = 0;
	return il2cpp_class_value_size(klass->address, &align);
}

std::vector<UnityResolve::Class*> UnityResolve::GetSubClasses(Class* klass) {
	std::vector<Class*> result;
	if (!klass || !klass->address || !il2cpp_class_get_nested_types) return result;
	void* iter = nullptr;
	while (auto subKlass = il2cpp_class_get_nested_types(klass->address, &iter)) {
		result.push_back(GetOrCreateClass(subKlass));
	}
	return result;
}

std::vector<UnityResolve::UnityType::Object*> UnityResolve::FindObjectsOfType(const std::string& className) {
	auto klass = FindClass(className);
	if (!klass || !klass->address) {
		LOGE("FindObjectsOfType: class not found: %s", className.c_str());
		return {};
	}
	static auto unityObjectClass = FindClass("UnityEngine.Object");
	if (!unityObjectClass || !unityObjectClass->address) return {};
	auto method = unityObjectClass->Get<Method>("FindObjectsOfType", {"System.Type"});
	if (!method || !method->function) return {};
	if (!il2cpp_class_get_type || !il2cpp_type_get_object) return {};
	auto typeObj = il2cpp_type_get_object(il2cpp_class_get_type(klass->address));
	if (!typeObj) return {};
	auto arr = method->Invoke<UnityType::Array<UnityType::Object*>*>(typeObj, method->address);
	if (!arr) return {};
	std::vector<UnityType::Object*> result;
	if (!il2cpp_array_length) return result;
	auto len = il2cpp_array_length(arr);
	result.reserve(len);
	for (uint32_t i = 0; i < len; i++) {
		auto obj = arr->At(i);
		if (obj) result.push_back(obj);
	}
	return result;
}

std::vector<UnityResolve::UnityType::Object*> UnityResolve::GC::FindObjects(Class* klass) {
	if (!klass || !klass->address) return {};
	static auto version = getUnityVersion();
	static bool useOldApi = UnityVersion::lt(version, "2021.2.0");
	std::vector<UnityType::Object*> objects;
	auto callback = +[](void** arr, int size, void* userdata) {
		auto vec = static_cast<std::vector<UnityType::Object*>*>(userdata);
		for (int i = 0; i < size; i++)
			vec->push_back(static_cast<UnityType::Object*>(arr[i]));
	};
	if (useOldApi) {
		if (!il2cpp_unity_liveness_calculation_begin || !il2cpp_unity_liveness_calculation_from_statics || !il2cpp_unity_liveness_calculation_end) return objects;
		auto onWorld = +[]() {};
		auto state = il2cpp_unity_liveness_calculation_begin(klass->address, 0, (void*)callback, &objects, (void*)onWorld, (void*)onWorld);
		if (state) {
			il2cpp_unity_liveness_calculation_from_statics(state);
			il2cpp_unity_liveness_calculation_end(state);
		}
	} else {
		if (!il2cpp_unity_liveness_allocate_struct || !il2cpp_unity_liveness_calculation_from_statics || !il2cpp_unity_liveness_finalize || !il2cpp_unity_liveness_free_struct || !il2cpp_stop_gc_world || !il2cpp_start_gc_world) return objects;
		auto realloc = +[](void* ptr, size_t size, void* userdata) -> void* {
			if (ptr != nullptr && size == 0) {
				il2cpp_free(ptr);
				return nullptr;
			}
			return il2cpp_alloc(size);
		};
		il2cpp_stop_gc_world();
		auto state = il2cpp_unity_liveness_allocate_struct(klass->address, 0, (void*)callback, &objects, (void*)realloc);
		if (state) {
			il2cpp_unity_liveness_calculation_from_statics(state);
			il2cpp_unity_liveness_finalize(state);
		}
		il2cpp_start_gc_world();
		if (state) il2cpp_unity_liveness_free_struct(state);
	}
	LOGI("GC::FindObjects found %zu objects for %s", objects.size(), klass->name.c_str());
	return objects;
}

void UnityResolve::GC::KeepAlive(UnityType::Object* object) {
	if (!object) return;
	static auto gcClass = FindClass("System.GC");
	if (!gcClass || !gcClass->address) return;
	auto method = gcClass->Get<Method>("KeepAlive");
	if (!method || !method->function) return;
	method->Invoke<void>(object, method->address);
}
