#include "Unity.hpp"
#include "Macros.h"
#include "il2cpp/UnityResolve.hpp"
#include "il2cpp/log.h"
#include <imgui.h>
#include <atomic>

int (*o_get_touchCount)();
int get_touchCount();
bool (*oInput_GetMouseButton)(int n);
bool Input_GetMouseButton(int n);

static UnityResolve::Class* inputClass = nullptr;

static std::atomic<bool> g_hasTouch{false};
static std::atomic<float> g_touchX{0.f};
static std::atomic<float> g_touchY{0.f};
static std::atomic<int> g_touchPhase{0};
static std::atomic<bool> g_wantCaptureMouse{false};

bool Input_GetMouseButton(int n)
{
    if (g_wantCaptureMouse.load(std::memory_order_relaxed))
        return false;
    return oInput_GetMouseButton(n);
}

int get_touchCount()
{
    int count = o_get_touchCount();

    if (count > 0 && inputClass)
    {
        auto getTouchMethod = inputClass->Get<UnityResolve::Method>("GetTouch");
        if (getTouchMethod && getTouchMethod->function)
        {
            auto touch = getTouchMethod->Invoke<UnityEngine_Touch>(0, getTouchMethod->address);
            g_touchX.store(touch.m_Position.x, std::memory_order_relaxed);
            g_touchY.store(touch.m_Position.y, std::memory_order_relaxed);
            g_touchPhase.store(static_cast<int>(touch.m_Phase), std::memory_order_relaxed);
            g_hasTouch.store(true, std::memory_order_relaxed);
        }
    }

    if (g_wantCaptureMouse.load(std::memory_order_relaxed))
        return 0;

    return count;
}

namespace Unity
{
    void HookInput()
    {
        inputClass = UnityResolve::FindClass("UnityEngine.Input");
        if (!inputClass) return;
        REPLACE_NAME_ORIG("UnityEngine.Input", "get_touchCount", get_touchCount, o_get_touchCount);
        REPLACE_NAME_ORIG("UnityEngine.Input", "GetMouseButton", Input_GetMouseButton, oInput_GetMouseButton);
    }

    void ProcessInput()
    {
        if (!ImGui::GetCurrentContext())
            return;

        ImGuiIO& io = ImGui::GetIO();
        g_wantCaptureMouse.store(io.WantCaptureMouse, std::memory_order_relaxed);

        if (g_hasTouch.load(std::memory_order_relaxed))
        {
            float x = g_touchX.load(std::memory_order_relaxed);
            float y = io.DisplaySize.y - g_touchY.load(std::memory_order_relaxed);
            int phase = g_touchPhase.load(std::memory_order_relaxed);

            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);

            if (phase == static_cast<int>(UnityEngine_TouchPhase::Began))
            {
                io.AddMousePosEvent(x, y);
                io.AddMouseButtonEvent(0, true);
            }
            else if (phase == static_cast<int>(UnityEngine_TouchPhase::Ended))
            {
                io.AddMousePosEvent(x, y);
                io.AddMouseButtonEvent(0, false);
                io.AddMousePosEvent(-1, -1);
            }
            else if (phase == static_cast<int>(UnityEngine_TouchPhase::Moved))
            {
                io.AddMousePosEvent(x, y);
            }

            g_hasTouch.store(false, std::memory_order_relaxed);
        }
    }
}
