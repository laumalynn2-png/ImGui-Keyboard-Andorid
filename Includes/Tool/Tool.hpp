#pragma once
#include "il2cpp/UnityResolve.hpp"

class ClassesTab;

namespace Tool {
    void ConfigSave();
    bool ToggleHooker(UnityResolve::Method* method, int state = -1);
    ClassesTab& GetFirstTab();
    ClassesTab& OpenNewTab();
    ClassesTab& OpenNewTabFromClass(UnityResolve::Class* klass);
}
