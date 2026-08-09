#pragma once

#include <jni.h>
#include <android/log.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <dirent.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <elf.h>
#include <inttypes.h>
#include <stdint.h>
#include <locale.h>
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <istream>
#include <ostream>
#include <streambuf>
#include <iomanip>
#include <string>
#include <codecvt>
#include <vector>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <functional>
#include <tuple>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>
#include <ctime>
#include <random>
#include <numbers>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cctype>
#include <cerrno>
#include <cwctype>
#include <format>
#include <dobby.h>
#include <frida-gum.h>
#include <gumpp.hpp>
#include <asmjit/asmjit.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_android.h>
#include <imgui_impl_opengl3.h>
#include <jni.hpp>

#include <MemoryPatch.h>
#include <il2cpp_dump.h>
#include <UnityResolve.hpp>
#include <log.h>

inline bool setup;
inline int glWidth, glHeight;

#define HOOKINPUT(ret, func, ...)                                                                                      \
    ret (*orig##func)(__VA_ARGS__);                                                                                    \
    ret my##func(__VA_ARGS__)

HOOKINPUT(void, Input, void *thiz, void *ex_ab, void *ex_ac)
{
    origInput(thiz, ex_ab, ex_ac);
    if (setup)
        ImGui_ImplAndroid_HandleInputEvent((AInputEvent *)thiz);
    return;
}
