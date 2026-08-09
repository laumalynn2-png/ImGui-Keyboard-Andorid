#include <Includes.hpp>
#include <Tool/Tool.hpp>
#include <Tool/Keyboard.hpp>
#include <atomic>

static std::atomic<bool> toolReady{false};

void DrawMenu() {
    if (!toolReady.load()) {
        ImGui::Begin("Tool");
        ImGui::Text("Loading...");
        ImGui::End();
        return;
    }
    ImGui::Begin("Tool");
    Tool::Draw();
    Tool::Dumper();
    ImGui::End();
}

EGLBoolean(*orig_eglSwapBuffers)(EGLDisplay, EGLSurface);
EGLBoolean _eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

    if (!setup) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        auto& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)glWidth, (float)glHeight);
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.Fonts->AddFontFromFileTTF("/system/fonts/NotoSansCJK-Regular.ttc", 40.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());

        ImGui::StyleColorsDark();
        ImGui::GetStyle().ScaleAllSizes(2.5f);
        ImGui_ImplOpenGL3_Init("#version 300 es");
        setup = true;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(glWidth, glHeight);
    static bool prevWantTextInput = false;
    bool wantTextInput = ImGui::GetIO().WantTextInput;
    if (wantTextInput != prevWantTextInput) {
        prevWantTextInput = wantTextInput;
        ShowKeyboard(wantTextInput);
    }
    UnityResolve::EnsureAttached();
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2((float)glWidth / 2, (float)glHeight / 2), ImGuiCond_Once);
    Keyboard::Update();
    DrawMenu();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return orig_eglSwapBuffers(dpy, surface);
}

void *input_thread(void *) {
    void* egl = DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    void* inp = DobbySymbolResolver("libinput.so", "_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE");

    if (egl) DobbyHook(egl, (void*)_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
    if (inp) DobbyHook(inp, (void*)myInput, (void**)&origInput);

    return nullptr;
}

void *MainThread(void *) {
    sleep(2);

    void *handle = xdl_open("libil2cpp.so", XDL_DEFAULT);
    if (!handle)
        handle = dlopen("libil2cpp.so", RTLD_NOW);

    if (handle) {
        il2cpp_api_init(handle);
        UnityResolve::EnsureAttached();
        Keyboard::Init();
        Tool::Init();
        toolReady = true;
    }

    return nullptr;
}

__attribute__((constructor))
void libmain() {
    pthread_t thread1, thread2;
    pthread_create(&thread1, nullptr, input_thread, nullptr);
    pthread_create(&thread2, nullptr, MainThread, nullptr);
    pthread_detach(thread1);
    pthread_detach(thread2);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    jvm = vm;
    LoadDex(imgui_dex, imgui_dex_len);
    return JNI_VERSION_1_6;
}
