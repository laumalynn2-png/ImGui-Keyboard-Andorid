#pragma once
#include "UnityResolve.hpp"
#include "il2cpp-tabledefs.h"
#include <json.hpp>
#include <sstream>
#include <algorithm>

class UnityDump {
public:
	using json = nlohmann::ordered_json;
	using Handler = std::function<json(void*, const UnityResolve::Type&, size_t)>;

	inline static std::vector<Handler> handlers;
	inline static int maxListArraySize = 5;
	inline static bool detectCircularRef = true;

	static void AddCustomDumpHandler(Handler handler) {
		handlers.push_back(std::move(handler));
	}

	static json DumpObject(void* object, const UnityResolve::Type& type, std::vector<uintptr_t>& visited, size_t maxDepth) {
		if (type.isPointer()) {
			return "(pointer)";
		}

		auto custom = callHandlers(object, type, maxDepth);
		if (!custom.is_null() && !custom.empty()) {
			return custom;
		}

		const auto& typeName = type.name;

		if (typeName == "System.String") {
			if (object) return stringFromObj(object);
			return "(null-string)";
		}

		if (type.isPrimitive()) {
			if (!object) return "(null-primitive)";
			if (typeName == "System.Int32") return UnityResolve::GetUnboxedValue<int32_t>(object);
			if (typeName == "System.Byte") return UnityResolve::GetUnboxedValue<uint8_t>(object);
			if (typeName == "System.SByte") return UnityResolve::GetUnboxedValue<int8_t>(object);
			if (typeName == "System.Int16") return UnityResolve::GetUnboxedValue<int16_t>(object);
			if (typeName == "System.UInt16") return UnityResolve::GetUnboxedValue<uint16_t>(object);
			if (typeName == "System.UInt32") return UnityResolve::GetUnboxedValue<uint32_t>(object);
			if (typeName == "System.Int64") return UnityResolve::GetUnboxedValue<int64_t>(object);
			if (typeName == "System.UInt64") return UnityResolve::GetUnboxedValue<uint64_t>(object);
			if (typeName == "System.Single") return UnityResolve::GetUnboxedValue<float>(object);
			if (typeName == "System.Double") return UnityResolve::GetUnboxedValue<double>(object);
			if (typeName == "System.Boolean") return UnityResolve::GetUnboxedValue<bool>(object);
			if (typeName == "System.Char") return UnityResolve::GetUnboxedValue<uint16_t>(object);
			return callToString(object);
		}

		if (type.isEnum()) {
			if (!object) return "(null-enum)";
			return callToString(object);
		}

		if (type.isValueType()) {
			if (!object) return "(null-value-type)";
			if (maxDepth == 0) return toHex((uintptr_t)object);
			return DumpFields(object, visited, (int)maxDepth - 1);
		}

		if (type.isArray()) {
			if (!object) return "(null-array)";
			return dumpArray(object, type, visited, maxDepth);
		}

		if (type.isList()) {
			if (!object) return "(null-list)";
			return dumpList(object, type, visited, maxDepth);
		}

		if (type.isObject()) {
			if (!object) return "(null-object)";
			if (maxDepth == 0) return toHex((uintptr_t)object);
			return DumpFields(object, visited, (int)maxDepth - 1);
		}

		return "(unhandled)";
	}

	static json DumpFields(void* object, std::vector<uintptr_t>& visited, int maxDepth) {
		if (!object) return "(null)";

		if (detectCircularRef) {
			if (isVisited(visited, object)) {
				return "(circular " + toHex((uintptr_t)object) + ")";
			}
			visited.push_back((uintptr_t)object);
		}

		if (maxDepth <= 0) return toHex((uintptr_t)object);

		if (!il2cpp_object_get_class || !il2cpp_class_get_type) return "(no-class)";

		auto klass = il2cpp_object_get_class(object);
		if (!klass) return "(no-class)";

		auto typeAddr = il2cpp_class_get_type(klass);
		auto objType = makeType(typeAddr);

		if (objType.isArray() || objType.isList() || objType.isEnum() || objType.name == "System.String") {
			return DumpObject(object, objType, visited, (size_t)maxDepth);
		}

		std::vector<void*> classChain;
		auto k = klass;
		int chainDepth = 50;
		while (k && chainDepth-- > 0) {
			classChain.push_back(k);
			k = il2cpp_class_get_parent ? il2cpp_class_get_parent(k) : nullptr;
		}
		std::reverse(classChain.begin(), classChain.end());

		json result = json::object();
		bool hasFields = false;

		for (auto c : classChain) {
			if (!il2cpp_class_get_fields) break;
			void* iter = nullptr;
			while (auto field = il2cpp_class_get_fields(c, &iter)) {
				if (!il2cpp_field_get_flags) continue;
				auto flags = il2cpp_field_get_flags(field);
				if (flags & FIELD_ATTRIBUTE_STATIC) continue;

				auto fieldTypeAddr = il2cpp_field_get_type ? il2cpp_field_get_type(field) : nullptr;
				if (!fieldTypeAddr) continue;

				auto fieldType = makeType(fieldTypeAddr);
				auto fieldName = il2cpp_field_get_name ? il2cpp_field_get_name(field) : "";
				if (!fieldName) fieldName = "";

				std::string key = fieldType.name + " " + fieldName;
				auto fieldValue = il2cpp_field_get_value_object ? il2cpp_field_get_value_object(field, object) : nullptr;
				result[key] = DumpObject(fieldValue, fieldType, visited, (size_t)maxDepth);
				hasFields = true;
			}
		}

		if (!hasFields) return "(no-fields)";
		return result;
	}

	static std::pair<void*, json> DumpByPath(void* object, const std::vector<std::string>& paths, bool noDump) {
		void* current = object;

		for (const auto& path : paths) {
			if (!current) break;

			auto klass = il2cpp_object_get_class ? il2cpp_object_get_class(current) : nullptr;
			if (!klass) break;

			auto typeAddr = il2cpp_class_get_type ? il2cpp_class_get_type(klass) : nullptr;
			auto type = makeType(typeAddr);

			if (type.isArray() || type.isList()) {
				int index = 0;
				try { index = std::stoi(path); } catch (...) { break; }
				current = invokeGetItem(current, index);
			} else {
				std::istringstream iss(path);
				std::string discard, fieldName;
				iss >> discard >> fieldName;
				if (fieldName.empty()) fieldName = discard;

				auto field = il2cpp_class_get_field_from_name ? il2cpp_class_get_field_from_name(klass, fieldName.c_str()) : nullptr;
				if (!field) break;
				current = il2cpp_field_get_value_object ? il2cpp_field_get_value_object(field, current) : nullptr;
			}
		}

		if (!current) {
			return {nullptr, json{}};
		}

		if (noDump) {
			return {current, json{}};
		}

		auto oldSize = maxListArraySize;
		auto oldCircular = detectCircularRef;
		maxListArraySize = 100;
		detectCircularRef = false;

		std::vector<uintptr_t> visited;
		auto result = DumpFields(current, visited, 2);

		maxListArraySize = oldSize;
		detectCircularRef = oldCircular;

		return {current, result};
	}

private:
	static json callHandlers(void* object, const UnityResolve::Type& type, size_t maxDepth) {
		for (auto& h : handlers) {
			auto result = h(object, type, maxDepth);
			if (!result.is_null() && !result.empty()) return result;
		}
		return json{};
	}

	static bool isVisited(const std::vector<uintptr_t>& visited, void* object) {
		return std::find(visited.begin(), visited.end(), (uintptr_t)object) != visited.end();
	}

	static std::string toHex(uintptr_t addr) {
		std::stringstream ss;
		ss << "0x" << std::uppercase << std::hex << addr;
		return ss.str();
	}

	static std::string stringFromObj(void* strObj) {
		if (!strObj || !il2cpp_string_length || !il2cpp_string_chars) return "";
		auto len = il2cpp_string_length(strObj);
		auto chars = (const uint16_t*)il2cpp_string_chars(strObj);
		std::string result;
		result.reserve(len);
		for (int32_t i = 0; i < len; i++) {
			uint16_t ch = chars[i];
			if (ch <= 0x7F) {
				result += (char)ch;
			} else if (ch <= 0x7FF) {
				result += (char)(0xC0 | (ch >> 6));
				result += (char)(0x80 | (ch & 0x3F));
			} else {
				result += (char)(0xE0 | (ch >> 12));
				result += (char)(0x80 | ((ch >> 6) & 0x3F));
				result += (char)(0x80 | (ch & 0x3F));
			}
		}
		return result;
	}

	static std::string callToString(void* object) {
		if (!object || !il2cpp_object_get_class || !il2cpp_class_get_method_from_name || !il2cpp_runtime_invoke) return "";
		auto klass = il2cpp_object_get_class(object);
		if (!klass) return "";
		auto method = il2cpp_class_get_method_from_name(klass, "ToString", 0);
		if (!method) return "";
		void* exc = nullptr;
		auto str = il2cpp_runtime_invoke(method, object, nullptr, &exc);
		if (!str) return "";
		return stringFromObj(str);
	}

	static UnityResolve::Type makeType(void* typeAddr) {
		UnityResolve::Type t;
		t.address = typeAddr;
		t.size = -1;
		if (typeAddr && il2cpp_type_get_name) {
			auto tn = il2cpp_type_get_name(typeAddr);
			if (tn) { t.name = tn; il2cpp_free(tn); }
		}
		return t;
	}

	static void* invokeGetItem(void* object, int index) {
		if (!il2cpp_object_get_class || !il2cpp_class_get_method_from_name || !il2cpp_runtime_invoke) return nullptr;
		auto klass = il2cpp_object_get_class(object);
		if (!klass) return nullptr;
		auto method = il2cpp_class_get_method_from_name(klass, "get_Item", 1);
		if (!method) return nullptr;
		void* params[1] = { &index };
		void* exc = nullptr;
		return il2cpp_runtime_invoke(method, object, params, &exc);
	}

	static int getListCount(void* object) {
		if (!il2cpp_object_get_class || !il2cpp_class_get_method_from_name || !il2cpp_runtime_invoke) return 0;
		auto klass = il2cpp_object_get_class(object);
		if (!klass) return 0;
		auto method = il2cpp_class_get_method_from_name(klass, "get_Count", 0);
		if (!method) return 0;
		void* exc = nullptr;
		auto result = il2cpp_runtime_invoke(method, object, nullptr, &exc);
		if (!result) return 0;
		return UnityResolve::GetUnboxedValue<int32_t>(result);
	}

	static json dumpArray(void* object, const UnityResolve::Type& type, std::vector<uintptr_t>& visited, size_t maxDepth) {
		auto len = il2cpp_array_length ? il2cpp_array_length(object) : 0;
		int limit = std::min((int)len, maxListArraySize);
		json arr = json::array();

		for (int i = 0; i < limit; i++) {
			void* element = invokeGetItem(object, i);
			if (element) {
				auto elemClass = il2cpp_object_get_class ? il2cpp_object_get_class(element) : nullptr;
				auto elemTypeAddr = elemClass && il2cpp_class_get_type ? il2cpp_class_get_type(elemClass) : nullptr;
				auto elemType = makeType(elemTypeAddr);
				arr.push_back(DumpObject(element, elemType, visited, maxDepth > 0 ? maxDepth - 1 : 0));
			} else {
				arr.push_back("(null)");
			}
		}

		if ((int)len > maxListArraySize) {
			arr.push_back(std::to_string((int)len - maxListArraySize) + " more...");
		}

		return arr;
	}

	static json dumpList(void* object, const UnityResolve::Type& type, std::vector<uintptr_t>& visited, size_t maxDepth) {
		auto len = getListCount(object);
		int limit = std::min(len, maxListArraySize);
		json arr = json::array();

		for (int i = 0; i < limit; i++) {
			void* element = invokeGetItem(object, i);
			if (element) {
				auto elemClass = il2cpp_object_get_class ? il2cpp_object_get_class(element) : nullptr;
				auto elemTypeAddr = elemClass && il2cpp_class_get_type ? il2cpp_class_get_type(elemClass) : nullptr;
				auto elemType = makeType(elemTypeAddr);
				arr.push_back(DumpObject(element, elemType, visited, maxDepth > 0 ? maxDepth - 1 : 0));
			} else {
				arr.push_back("(null)");
			}
		}

		if (len > maxListArraySize) {
			arr.push_back(std::to_string(len - maxListArraySize) + " more...");
		}

		return arr;
	}
};
