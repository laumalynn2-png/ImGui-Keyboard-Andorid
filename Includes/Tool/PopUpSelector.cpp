#include "PopUpSelector.hpp"
#include "il2cpp/UnityResolve.hpp"
#include "il2cpp/il2cpp-tabledefs.h"
#include "il2cpp/log.h"
#include <imgui.h>

void PopUpSelector::Open(const std::string &type, const std::function<void(const std::string &)> &callback, void *data)
{
    LOGD("PopUpSelector Open %s", type.c_str());
    lastCallback = callback;
    needOpen = type;
    userData = data;
}

void PopUpSelector::Update()
{
    if (!needOpen.empty())
    {
        ImGui::OpenPopup(needOpen.c_str());
        needOpen = "";
    }

    if (!lastCallback)
        return;

    if (ImGui::BeginPopup("BooleanSelector"))
    {
        if (ImGui::Button("True"))
        {
            Do("True");
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("False"))
        {
            Do("False");
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    auto &io = ImGui::GetIO();
    ImGui::SetNextWindowSizeConstraints(ImVec2(-1, 0.f), ImVec2(-1, io.DisplaySize.y / 3.f));
    if (ImGui::BeginPopup("EnumSelector"))
    {
        auto type = static_cast<UnityResolve::Type*>(userData);
        if (type)
        {
            auto klass = type->getClass();
            if (klass)
            {
                auto fields = klass->getFields(false);
                for (auto field : fields)
                {
                    if (!field || !field->address) continue;
                    auto flags = il2cpp_field_get_flags(field->address);
                    if (flags & FIELD_ATTRIBUTE_STATIC)
                    {
                        if (ImGui::Button(field->name.c_str()))
                        {
                            Do(field->name);
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
            }
        }
        ImGui::EndPopup();
    }
}
