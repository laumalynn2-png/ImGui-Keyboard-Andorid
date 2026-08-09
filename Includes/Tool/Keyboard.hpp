#pragma once
#include <functional>
#include <string>

enum TouchScreenKeyboardStatus
{
    Visible = 0,
    Done = 1,
    Canceled = 2,
    LostFocus = 3
};

namespace Keyboard
{
    void Init();
    void Open(const std::function<void(const std::string &)> &callback);
    void Open(const char *text, const std::function<void(const std::string &)> &callback);
    void Reset();
    void Update();
    bool IsOpen();
    const std::string &GetCurrentText();
}
