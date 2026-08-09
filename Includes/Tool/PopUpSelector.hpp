#pragma once
#include <string>
#include <functional>

class PopUpSelector
{
public:
    void Open(const std::string &type, const std::function<void(const std::string &)> &callback, void *data = nullptr);
    void Update();
private:
    void Do(const std::string &result)
    {
        if (lastCallback)
            lastCallback(result);
        lastCallback = nullptr;
        userData = nullptr;
    }
    std::string needOpen;
    void *userData = nullptr;
    std::function<void(const std::string &)> lastCallback;
};
