#include "UnityResolve.hpp"
#include "log.h"
#include "il2cpp-tabledefs.h"
#include <regex>

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
