#include "Unity.hpp"
#include "Macros.h"
#include "il2cpp/UnityResolve.hpp"
#include "il2cpp/log.h"
#include <imgui.h>

int (*o_get_touchCount)();
int get_touchCount();
bool (*oInput_GetMouseButton)(int n);
bool Input_GetMouseButton(int n);

static UnityResolve::Class* inputClass = nullptr;

bool Input_GetMouseButton(int n)
{
    if (!ImGui::GetCurrentContext())
        return oInput_GetMouseButton(n);

    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return false;
    return oInput_GetMouseButton(n);
}

int get_touchCount()
{
    if (!ImGui::GetCurrentContext())
        return o_get_touchCount();

    ImGuiIO &io = ImGui::GetIO();
    auto count = o_get_touchCount();

    if (count > 0 && inputClass)
    {
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        auto getTouchMethod = inputClass->Get<UnityResolve::Method>("GetTouch");
        if (getTouchMethod && getTouchMethod->function)
        {
            auto touch = getTouchMethod->Invoke<UnityEngine_Touch>(0, getTouchMethod->address);
            float x = touch.m_Position.x;
            float y = io.DisplaySize.y - touch.m_Position.y;

            if (touch.m_Phase == UnityEngine_TouchPhase::Began)
            {
                io.AddMousePosEvent(x, y);
                io.AddMouseButtonEvent(0, true);
            }
            else if (touch.m_Phase == UnityEngine_TouchPhase::Ended)
            {
                io.AddMousePosEvent(x, y);
                io.AddMouseButtonEvent(0, false);
                io.AddMousePosEvent(-1, -1);
            }
            else if (touch.m_Phase == UnityEngine_TouchPhase::Moved)
            {
                io.AddMousePosEvent(x, y);
            }
        }
    }

    if (io.WantCaptureMouse)
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
}
