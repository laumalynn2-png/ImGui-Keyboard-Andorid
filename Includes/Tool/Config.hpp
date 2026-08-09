#pragma once
#include <json.hpp>
#include <unistd.h>
#include <sstream>
#include "Util.hpp"
#include "il2cpp/log.h"

inline void logcatJson(nlohmann::ordered_json &json)
{
    auto str = json.dump(4);
    std::istringstream iss(str);
    std::string line;
    while (std::getline(iss, line))
    {
        usleep(100);
        LOGD("%s", line.c_str());
    }
}

inline nlohmann::ordered_json &GetConfig()
{
    static nlohmann::ordered_json gConf;
    return gConf;
}

template <typename T>
inline void ConfigSet(const char *key, T value)
{
    auto &gConf = GetConfig();
    gConf[key] = value;
    LOGD("ConfigWrite %s = %s", key, gConf[key].dump().c_str());
    Util::FileWriter fileWriter("tool_conf.json");
    fileWriter.write(gConf.dump(2).c_str());
}

template <typename T>
inline T ConfigGet(const char *key, T defaultValue)
{
    auto &gConf = GetConfig();
    if (gConf.contains(key))
    {
        LOGD("ConfigGet %s = %s", key, gConf[key].dump().c_str());
        return gConf[key].get<T>();
    }
    ConfigSet(key, defaultValue);
    return defaultValue;
}

inline void ConfigInit()
{
    auto &gConf = GetConfig();
    Util::FileReader fileReader("tool_conf.json");
    if (fileReader.exists())
    {
        auto data = fileReader.read();
        try
        {
            gConf = nlohmann::json::parse(data);
            logcatJson(gConf);
        }
        catch (nlohmann::json::exception &e)
        {
            LOGE("ConfigInit error : %s", e.what());
            Util::FileWriter fileWriter("tool_conf.json");
            fileWriter.write("{}");
        }
    }
    else
    {
        Util::FileWriter fileWriter("tool_conf.json");
        fileWriter.write("{}");
    }
}
