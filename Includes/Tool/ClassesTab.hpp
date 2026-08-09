#pragma once
#include <functional>
#include <list>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <json.hpp>
#include "il2cpp/UnityResolve.hpp"
#include "HookerData.hpp"
#include "PopUpSelector.hpp"
#include "UnityDump.hpp"

class ClassesTab {
public:
    using Object = void*;
    using Class = UnityResolve::Class*;
    using Method = UnityResolve::Method*;
    using Type = UnityResolve::Type*;
    using Json = nlohmann::ordered_json;
    using Paths = std::vector<std::string>;
    using DumpResult = UnityDump::DumpResult;
    using DataPair = std::pair<DumpResult, Paths>;
    using MethodParamList = std::vector<UnityResolve::Method::Arg*>;
    using MethodList = std::vector<std::pair<Method, MethodParamList>>;
    using ClassMethodMap = std::unordered_map<Class, MethodList>;

    struct ParamValue {
        std::string value;
        Object object;
    };

    struct OriginalMethodBytes {
        std::vector<uint8_t> bytes;
        std::string text;
    };

    std::unordered_map<Object, DataPair> dataMap;
    std::map<Object, bool> tabMap;

    bool caseSensitive = true;
    bool filterByClass = true;
    bool filterByMethod = false;
    bool filterByField = false;
    bool showAllClasses = false;
    bool includeAllImages = false;

    void* selectedImage = nullptr;
    std::vector<void*> allImages;

    std::vector<Class> classes;
    std::vector<Class> filteredClasses;
    std::vector<Method> tracedMethods;

    static std::unordered_map<Class, std::vector<Object>> objectMap;
    static std::unordered_map<Class, std::vector<Object>> newObjectMap;
    static std::unordered_map<Class, std::set<Object>> savedSet;
    static std::unordered_map<void*, OriginalMethodBytes> oMap;
    static std::unordered_map<Class, bool> states;
    static PopUpSelector poper;

    std::unordered_map<void*, std::unordered_map<std::string, ParamValue>> paramMap;
    std::unordered_map<void*, RingBuffer<std::pair<std::string, Object>>> callResults;

    ClassMethodMap methodMap;

    std::string filter;
    bool filterKeyboardOpen = false;
    int selectedImageIndex = -1;
    bool traceState = false;
    int maxProgress = 0;
    int progress = 0;
    bool opened = true;
    bool currentlyOpened = false;
    bool setOpenedTab = false;

    ClassesTab();

    MethodList& buildMethodMap(Class klass);
    Paths& getJsonPaths(Object object);
    void setJsonObject(Object object);
    Json& getJsonObject(Object object);

    void ImGuiObjectSelector(int id, Class klass, const char* prefix,
                             std::function<void(Object)> onSelect, bool canNew = false);
    void CallerView(Class klass, Method method, const MethodParamList& paramsInfo, Object thiz = nullptr);
    void PatcherView(Class klass, Method method, const MethodParamList& paramsInfo, Object thiz = nullptr);
    void HookerView(Class klass, Method method, const MethodParamList& paramsInfo, Object thiz = nullptr);
    bool isMethodHooked(Method method);
    bool MethodViewer(Class klass, Method method, const MethodParamList& paramsInfo,
                      Object thiz = nullptr, bool includeInflated = false);
    void ClassViewer(Class klass);
    void Draw(int index = -1, bool closeable = false);
    void DrawTabMap();
    void ImGuiJson(Object rootObj);
    void FilterClasses(const std::string& filter);
};

void to_json(nlohmann::ordered_json& j, const ClassesTab& p);
void from_json(const nlohmann::ordered_json& j, ClassesTab& p);
