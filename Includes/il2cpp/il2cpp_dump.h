#pragma once
#include <functional>
#include <string>

void il2cpp_api_init(void *handle);
void il2cpp_dump();
void il2cpp_dump(const std::string& outPath, const std::function<void(const char*, int, int)>& progress);
