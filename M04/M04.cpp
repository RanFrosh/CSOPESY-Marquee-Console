// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#ifndef RICK_PNG_PATH
#define RICK_PNG_PATH "rick.png"
#endif
#ifndef BEE_IMG_PATH
#define BEE_IMG_PATH "bee.png"
#endif
#ifndef BEE_TXT_PATH
#define BEE_TXT_PATH "bee.txt"
#endif
#ifndef GOOGOL_PNG_PATH
#define GOOGOL_PNG_PATH "googol.png"
#endif
#ifndef MGR_PNG_PATH
#define MGR_PNG_PATH "mgr.png"
#endif
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#include <chrono>
#include <ctime>
#include <fstream>
#include <string>
#include <sstream>

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

struct Texture { GLuint id = 0; int width = 0; int height = 0; };
static Texture g_bg;
static Texture g_bee;
static Texture g_googol;
static Texture g_mgr;
static std::string g_bee_text;

static std::string LoadTextFile(const char* path) {
    std::ifstream f(path);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

static bool LoadTexture(const char* path, Texture& tex) {
    int w, h, n;
    unsigned char* pixels = stbi_load(path, &w, &h, &n, 4);      // forces RGBA
    if (!pixels) { fprintf(stderr, "Failed to load %s: %s\n", path, stbi_failure_reason()); return false; }
    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);
    tex.width = w;
    tex.height = h;
    return true;
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Select GL version + let the backend select a GLSL version
    const char* glsl_version = nullptr;
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100 (WebGL 1.0)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + generally GLSL 150
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + generally GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);   // strip title bar + border
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "CSOPESY and the Justins", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    int mon_x, mon_y;
    glfwGetMonitorPos(monitor, &mon_x, &mon_y);
    glfwSetWindowPos(window, mon_x, mon_y);       // align exactly onto the monitor
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);
    if (!LoadTexture(RICK_PNG_PATH, g_bg))
        fprintf(stderr, "Background image failed to load.\n");
    if (!LoadTexture(BEE_IMG_PATH, g_bee))
        fprintf(stderr, "Bee Movie image failed to load.\n");
    if (!LoadTexture(GOOGOL_PNG_PATH, g_googol))
        fprintf(stderr, "Googol image failed to load.\n");
    if (!LoadTexture(MGR_PNG_PATH, g_mgr))
        fprintf(stderr, "Tax Manager image failed to load.\n");
    g_bee_text = LoadTextFile(BEE_TXT_PATH);
    if (g_bee_text.empty())
        fprintf(stderr, "Bee Movie text failed to load.\n");

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefaultVector();
    //io.Fonts->AddFontDefaultBitmap();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    bool taskbar = true;
    bool clock = true;
    bool taskman = false;
    bool show_bee_movie = false;
    bool show_googol = false;
    bool show_tax_manager = false;
    char googol_query[256] = "";

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
        ImGuiWindowFlags neoflags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##background", nullptr, flags);
        ImGui::Image((ImTextureID)(intptr_t)g_bg.id, ImGui::GetIO().DisplaySize, ImVec2(0, 0), ImVec2(1, 1));
        ImGui::End();
        ImGui::PopStyleVar();

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(
            ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y),
            ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::Begin("##topbar", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
        // widgets
        if (clock) {
            std::time_t t = std::chrono::system_clock::to_time_t(
                std::chrono::system_clock::now());
            std::tm tm_now{};
            localtime_s(&tm_now, &t);
            char time_buf[64];
            std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d || %I:%M:%S %p", &tm_now);
            ImGui::Text(time_buf);
            ImGui::SameLine();
        }
        if (ImGui::Button("Turn off")) { 
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        ImGui::SameLine();    
        ImGui::End();

        if (show_bee_movie)
        {
            ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Bee Movie", &show_bee_movie))      
            {
                const float spacing = ImGui::GetStyle().ItemSpacing.x;
                const ImVec2 avail = ImGui::GetContentRegionAvail();
                const float left_w = avail.x * 0.5f - spacing * 0.5f;

                // LEFT: scrolling script, wrapped to the column
                ImGui::BeginChild("##script", ImVec2(left_w, avail.y), ImGuiChildFlags_Borders);
                ImGui::PushTextWrapPos(0.0f);                    
                if (g_bee_text.empty())
                    ImGui::TextUnformatted("(text failed to load)");
                else
                    ImGui::TextUnformatted(g_bee_text.c_str(), g_bee_text.c_str() + g_bee_text.size());
                ImGui::PopTextWrapPos();
                ImGui::EndChild();

                ImGui::SameLine();

                // RIGHT: static poster, fit-to-box, centered both axes
                ImGui::BeginChild("##poster", ImVec2(0.0f, avail.y), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                if (g_bee.id != 0)
                {
                    const ImVec2 room = ImGui::GetContentRegionAvail();
                    float w = (float)g_bee.width, h = (float)g_bee.height;
                    float s = room.x / w;
                    if (room.y / h < s) s = room.y / h;
                    if (s > 1.0f) s = 1.0f;
                    w *= s; h *= s;
                    ImGui::SetCursorPos(ImVec2(
                        ImGui::GetCursorPosX() + (room.x - w) * 0.5f,
                        ImGui::GetCursorPosY() + (room.y - h) * 0.5f));
                    ImGui::Image((ImTextureID)(intptr_t)g_bee.id, ImVec2(w, h));
                }
                else
                    ImGui::TextUnformatted("(image failed to load)");
                ImGui::EndChild();
            }
            ImGui::End();
        }

        if (show_googol)
        {
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.45f, 0.45f, 0.45f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.40f, 0.40f, 0.40f, 1.0f));
            ImGui::SetNextWindowSize(ImVec2(560, 270), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Googol Chrom", &show_googol))
            {
                const char* heading = "Googol";
                ImGui::PushFont(NULL, ImGui::GetStyle().FontSizeBase * 2.5f);
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(heading).x) * 0.5f);
                ImGui::TextUnformatted(heading);
                ImGui::PopFont();

                const float field_w = 320.0f;
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - field_w) * 0.5f);
                ImGui::SetNextItemWidth(field_w);
                ImGui::InputText("##search", googol_query, sizeof(googol_query));
            }
            ImGui::End();
            ImGui::PopStyleColor(3);
        }

        if (show_tax_manager)
        {
            ImGui::SetNextWindowSize(ImVec2(560, 270), ImGuiCond_FirstUseEver);
            ImGui::Begin("Fanum Tax Manager", &show_tax_manager);

            const ImVec2 avail = ImGui::GetContentRegionAvail();

            // LEFT: nav pane, width sized to widest label
            const char* nav_items[] = { "Processes", "Performance", "App History",
                                        "Startup Apps", "Users", "Details", "Services" };
            float nav_text_w = 0.0f;
            for (const char* s : nav_items)
                nav_text_w = (ImGui::CalcTextSize(s).x > nav_text_w) ? ImGui::CalcTextSize(s).x : nav_text_w;
            const float nav_w = nav_text_w + 2.0f * style.WindowPadding.x;

            ImGui::BeginChild("##mgmt_nav", ImVec2(nav_w, avail.y), ImGuiChildFlags_Borders);
            for (const char* s : nav_items)
                ImGui::TextUnformatted(s);
            ImGui::EndChild();

            ImGui::SameLine();

            // RIGHT: header row + (empty) body
            ImGui::BeginChild("##mgmt_main", ImVec2(0.0f, avail.y), ImGuiChildFlags_Borders);
            if (ImGui::BeginTable("##mgmt_tabs", 5,
                    ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Processes");
                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("67% CPU");
                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("67% Memory");
                ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted("67% Disk");
                ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted("67% Network");
                ImGui::EndTable();
            }
            ImGui::Separator();
            // rest of right pane intentionally empty for now
            ImGui::EndChild();

            ImGui::End();
        }

        ImGuiViewport* ap = ImGui::GetMainViewport();
        const float TASKBAR_H = 50.0f;
        const float BTN_H = TASKBAR_H - 2.0f * ImGui::GetStyle().WindowPadding.y;
        const float img_h = BTN_H - 2.0f * style.FramePadding.y;
        const bool bee_ok = g_bee.id != 0 && g_bee.height > 0;
        const bool googol_ok = g_googol.id != 0 && g_googol.height > 0;
        const bool mgr_ok = g_mgr.id != 0 && g_mgr.height > 0;
        const float bee_w = bee_ok ? img_h * ((float)g_bee.width / (float)g_bee.height) : ImGui::CalcTextSize("Bee Movie").x;
        const float googol_w = googol_ok ? img_h * ((float)g_googol.width / (float)g_googol.height) : ImGui::CalcTextSize("Googol Chrom").x;
        const float mgr_w = mgr_ok ? img_h * ((float)g_mgr.width / (float)g_mgr.height) : ImGui::CalcTextSize("Fanum Tax Manager").x;

        ImGui::SetNextWindowPos(
            ImVec2(ap->WorkPos.x, ap->WorkPos.y + ap->WorkSize.y),   
            ImGuiCond_Always, ImVec2(0.0f, 1.0f));                   
        ImGui::SetNextWindowSize(
            ImVec2(ap->WorkSize.x, TASKBAR_H), ImGuiCond_Always);    

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));   
        ImGui::Begin("##botbar", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse);
        const float CONTENT_W = bee_w + 2 * style.FramePadding.x + googol_w + 2 * style.FramePadding.x
            + mgr_w + 2 * style.FramePadding.x + 2 * style.ItemSpacing.x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - CONTENT_W) * 0.5f);
        ImGui::BeginChild("##botbar_center", ImVec2(CONTENT_W, 0), ImGuiChildFlags_AutoResizeY);
        
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, BTN_H * 0.5f);
        if (bee_ok)
        {
            if (ImGui::ImageButton("##bee", (ImTextureID)(intptr_t)g_bee.id,
                ImVec2(bee_w, img_h)))
                show_bee_movie = true;
        }
        else
        {
            if (ImGui::Button("Bee Movie", ImVec2(0, BTN_H)))
                show_bee_movie = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetItemTooltip("Bee Movie");
        ImGui::SameLine();
        if (googol_ok)
        {
            if (ImGui::ImageButton("##googol", (ImTextureID)(intptr_t)g_googol.id,
                ImVec2(googol_w, img_h)))
                show_googol = true;
        }
        else
        {
            if (ImGui::Button("Googol Chrom", ImVec2(0, BTN_H)))
                show_googol = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetItemTooltip("Googol Chrom");
        ImGui::SameLine();
        if (mgr_ok)
        {
            if (ImGui::ImageButton("##mgr", (ImTextureID)(intptr_t)g_mgr.id,
                ImVec2(mgr_w, img_h)))
                show_tax_manager = true;
        }
        else
        {
            if (ImGui::Button("Fanum Tax Manager", ImVec2(0, BTN_H)))
                show_tax_manager = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetItemTooltip("Fanum Tax Manager");
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::End();
        ImGui::PopStyleVar();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glDeleteTextures(1, &g_bg.id);
    glDeleteTextures(1, &g_bee.id);
    glDeleteTextures(1, &g_googol.id);
    glDeleteTextures(1, &g_mgr.id);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
