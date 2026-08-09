#include "Keyboard.hpp"
#include "il2cpp/UnityResolve.hpp"
#include "il2cpp/log.h"
#include <mutex>

namespace Keyboard
{
    static UnityResolve::Class* keyboardClass = nullptr;
    static UnityResolve::UnityType::Object* openedKeyboard = nullptr;
    static std::function<void(const std::string &)> lastCallback = nullptr;
    static std::string currentText;

    struct PendingOpen
    {
        bool pending = false;
        std::string text;
        std::function<void(const std::string &)> callback;
    };
    static PendingOpen pendingOpen;
    static std::mutex pendingMutex;

    void Init()
    {
        keyboardClass = UnityResolve::FindClass("UnityEngine.TouchScreenKeyboard");
        LOGD("TouchScreenKeyboard: %p", keyboardClass);
    }

    void Open(const std::function<void(const std::string &)> &callback)
    {
        Open("", callback);
    }

    void Open(const char *text, const std::function<void(const std::string &)> &callback)
    {
        std::lock_guard<std::mutex> lock(pendingMutex);
        pendingOpen.pending = true;
        pendingOpen.text = text ? text : "";
        pendingOpen.callback = callback;
    }

    void Reset()
    {
        auto kb = openedKeyboard;
        openedKeyboard = nullptr;
        lastCallback = nullptr;
        currentText.clear();

        if (kb && keyboardClass)
        {
            auto destroyMethod = keyboardClass->Get<UnityResolve::Method>("Destroy");
            if (destroyMethod && destroyMethod->address)
            {
                void* exc = nullptr;
                il2cpp_runtime_invoke(destroyMethod->address, kb, nullptr, &exc);
            }
        }
    }

    void Update()
    {
        {
            std::lock_guard<std::mutex> lock(pendingMutex);
            if (pendingOpen.pending)
            {
                pendingOpen.pending = false;

                if (openedKeyboard)
                    Reset();

                if (!keyboardClass) return;

                LOGD("Keyboard Open");
                auto openMethod = keyboardClass->Get<UnityResolve::Method>("Open");
                if (!openMethod || !openMethod->function) return;

                auto textStr = il2cpp_string_new(pendingOpen.text.c_str());
                auto emptyStr = il2cpp_string_new("");
                openedKeyboard = openMethod->Invoke<UnityResolve::UnityType::Object*>(
                    textStr, 0, 0, 0, 0, emptyStr, 0, openMethod->address);
                lastCallback = pendingOpen.callback;
                pendingOpen.callback = nullptr;

                if (openedKeyboard)
                    UnityResolve::GC::KeepAlive(openedKeyboard);
                else
                {
                    LOGE("Keyboard Open returned null");
                    lastCallback = nullptr;
                }
            }
        }

        if (!openedKeyboard || !keyboardClass)
            return;

        static auto getStatusMethod = keyboardClass->Get<UnityResolve::Method>("get_status");
        static auto getTextMethod = keyboardClass->Get<UnityResolve::Method>("get_text");

        if (!getStatusMethod || !getStatusMethod->address)
            return;

        void* exc = nullptr;
        auto statusResult = il2cpp_runtime_invoke(getStatusMethod->address, openedKeyboard, nullptr, &exc);
        if (!statusResult || exc)
        {
            LOGE("Failed to get keyboard status");
            Reset();
            return;
        }

        auto status = *static_cast<TouchScreenKeyboardStatus*>(il2cpp_object_unbox(statusResult));

        if (status == Done)
        {
            std::string result;
            if (getTextMethod && getTextMethod->address)
            {
                auto textResult = il2cpp_runtime_invoke(getTextMethod->address, openedKeyboard, nullptr, nullptr);
                if (textResult)
                    result = static_cast<UnityResolve::UnityType::String*>(textResult)->ToString();
            }

            auto cb = lastCallback;
            Reset();
            if (cb)
                cb(result);
            LOGD("Keyboard Done: %s", result.c_str());
        }
        else if (status == Visible)
        {
            if (getTextMethod && getTextMethod->address)
            {
                auto textResult = il2cpp_runtime_invoke(getTextMethod->address, openedKeyboard, nullptr, nullptr);
                if (textResult)
                    currentText = static_cast<UnityResolve::UnityType::String*>(textResult)->ToString();
            }
        }
        else
        {
            Reset();
            LOGD("Keyboard Canceled");
        }
    }

    bool IsOpen()
    {
        return openedKeyboard != nullptr;
    }

    const std::string &GetCurrentText()
    {
        return currentText;
    }
}
