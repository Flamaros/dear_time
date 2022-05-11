#include "application.h"

#include "wmi.h"
#include "ui.h"
#include "d3d11_helpers.h"

#include <imgui/imgui.h>
#include <implot/implot.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx11.h>

#include <Windows.h>
#include <tchar.h>

#if defined(_CONSOLE) || defined(_DEBUG)
#   include <io.h>
#   include <fcntl.h>
#endif

#include <iostream> // @TODO remove that (only used for wcout)

DearTime g_dear_time;

void run_tests();

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void draw_application(HWND hWnd);

#if defined(_CONSOLE) || defined(_DEBUG)
// https://stackoverflow.com/questions/311955/redirecting-cout-to-a-console-in-windows
void BindCrtHandlesToStdHandles(bool bindStdIn, bool bindStdOut, bool bindStdErr);
#endif

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
#if defined(_DEBUG)
    run_tests();
#endif

#if defined(_CONSOLE) || defined(_DEBUG)
    AllocConsole();
    BindCrtHandlesToStdHandles(true, true, true);
#endif

    initialize_application();

    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
    ::RegisterClassEx(&wc);
    HWND hWnd = ::CreateWindow(wc.lpszClassName, _T("Dear Time"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hWnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hWnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hWnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    initialize_ui();

    // Setup Platform/Renderer backends
    d3d11_init();
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplWin32_EnableDpiAwareness();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    if (!initialize_wmi_events_sink())
        return 1; // Program has failed.

    g_dear_time.ready_to_draw = true;

    ImGuiIO& io = ImGui::GetIO();
    bool previous_WantTextInput = io.WantTextInput;

    // Main loop
    while (!g_dear_time.done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);

            // @TODO
            // DispatchMessage is blocking when the user interact with the window decoration for moving or resing it
            //
            // A suggested way to be able to render during this blocking event process is to use WM_TIMER event system.
            // An other possible solution is to handle manually NC events which can be tricky.
            //
            // So it might be interesting to use WM_TIMER with a little higher frequency than the display refresh rate.
            // I certainly should separate the update and the rendering parts. To be able to do update at higher frequency than
            // the rendering. The rendering should happens only when the application content changed or when is it requested by the UI.
            // Ideally I want to use adaptative sync for best UI reactivity. And use the integrated GPU.
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                g_dear_time.done = true;
        }
        if (g_dear_time.done)
            break;

        if (io.WantTextInput != previous_WantTextInput)
        {
            printf("io.WantTextInput %s\n", io.WantTextInput ? "true" : "false");

            if (io.WantTextInput)
                start_permanent_redraw();
            else
                stop_permanent_redraw();
            previous_WantTextInput = io.WantTextInput;
        }

        draw_application(hWnd);
    }

    g_dear_time.ready_to_draw = false;

    EnterCriticalSection(&g_dear_time.is_quitting_critical_section);
    {
        g_dear_time.is_quitting = true;
    }
    LeaveCriticalSection(&g_dear_time.is_quitting_critical_section);

    terminate_wmi_event_sink();

    // Cleanup
    d3d11_shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hWnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    shutdown_application();

    return 0;   // Program successfully completed.
}

void draw_application(HWND hWnd)
{
    if (IsIconic(hWnd)) {
        Sleep(sleeping_duration_ms);
        return;  // early return, window is minimized (iconic)
    }

    if (*g_dear_time.nb_requested_redraws == 0) {
        Sleep(sleeping_duration_ms);
        return;
    }
    // g_dear_time.nb_requested_redraws may have been incremented since the test, but it is not an issue.
    InterlockedDecrement(g_dear_time.nb_requested_redraws);

    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ui_frame();

    // Rendering
    ImGui::Render();

    d3d11_present_framebuffer();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    // @Warning Actually ImGUI can't tell us if an event trigger an interaction with the UI.
    // So we request a redraw for every events we receive.
    // Optimally we should only handle Window events (resize, redraw request,...) and UI interactions
    // (butons clicks, hover effects, text edition, UI animations (text cursor), ...)
    request_redraw();

    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
            d3d11_resize_framebuffer((UINT)LOWORD(lParam), (UINT)HIWORD(lParam));

        if (g_dear_time.ready_to_draw)
            draw_application(hWnd);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;

    case WM_QUERYENDSESSION:
        return TRUE; // Don't lock the shutdown of user session
    case WM_ENDSESSION:
        safe_backup();
        // https://docs.microsoft.com/fr-fr/windows/win32/shutdown/wm-endsession
        g_dear_time.done = true;
        g_dear_time.done_by_end_session = true;
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

static void run_tests()
{
    test_ui_insert_merge_entry();
}

#if defined(_CONSOLE) || defined(_DEBUG)
static void BindCrtHandlesToStdHandles(bool bindStdIn, bool bindStdOut, bool bindStdErr)
{
    // Re-initialize the C runtime "FILE" handles with clean handles bound to "nul". We do this because it has been
    // observed that the file number of our standard handle file objects can be assigned internally to a value of -2
    // when not bound to a valid target, which represents some kind of unknown internal invalid state. In this state our
    // call to "_dup2" fails, as it specifically tests to ensure that the target file number isn't equal to this value
    // before allowing the operation to continue. We can resolve this issue by first "re-opening" the target files to
    // use the "nul" device, which will place them into a valid state, after which we can redirect them to our target
    // using the "_dup2" function.
    if (bindStdIn)
    {
        FILE* dummyFile;
        freopen_s(&dummyFile, "nul", "r", stdin);
    }
    if (bindStdOut)
    {
        FILE* dummyFile;
        freopen_s(&dummyFile, "nul", "w", stdout);
    }
    if (bindStdErr)
    {
        FILE* dummyFile;
        freopen_s(&dummyFile, "nul", "w", stderr);
    }

    // Redirect unbuffered stdin from the current standard input handle
    if (bindStdIn)
    {
        HANDLE stdHandle = GetStdHandle(STD_INPUT_HANDLE);
        if (stdHandle != INVALID_HANDLE_VALUE)
        {
            int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
            if (fileDescriptor != -1)
            {
                FILE* file = _fdopen(fileDescriptor, "r");
                if (file != NULL)
                {
                    int dup2Result = _dup2(_fileno(file), _fileno(stdin));
                    if (dup2Result == 0)
                    {
                        setvbuf(stdin, NULL, _IONBF, 0);
                    }
                }
            }
        }
    }

    // Redirect unbuffered stdout to the current standard output handle
    if (bindStdOut)
    {
        HANDLE stdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdHandle != INVALID_HANDLE_VALUE)
        {
            int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
            if (fileDescriptor != -1)
            {
                FILE* file = _fdopen(fileDescriptor, "w");
                if (file != NULL)
                {
                    int dup2Result = _dup2(_fileno(file), _fileno(stdout));
                    if (dup2Result == 0)
                    {
                        setvbuf(stdout, NULL, _IONBF, 0);
                    }
                }
            }
        }
    }

    // Redirect unbuffered stderr to the current standard error handle
    if (bindStdErr)
    {
        HANDLE stdHandle = GetStdHandle(STD_ERROR_HANDLE);
        if (stdHandle != INVALID_HANDLE_VALUE)
        {
            int fileDescriptor = _open_osfhandle((intptr_t)stdHandle, _O_TEXT);
            if (fileDescriptor != -1)
            {
                FILE* file = _fdopen(fileDescriptor, "w");
                if (file != NULL)
                {
                    int dup2Result = _dup2(_fileno(file), _fileno(stderr));
                    if (dup2Result == 0)
                    {
                        setvbuf(stderr, NULL, _IONBF, 0);
                    }
                }
            }
        }
    }

    // Clear the error state for each of the C++ standard stream objects. We need to do this, as attempts to access the
    // standard streams before they refer to a valid target will cause the iostream objects to enter an error state. In
    // versions of Visual Studio after 2005, this seems to always occur during startup regardless of whether anything
    // has been read from or written to the targets or not.
    if (bindStdIn)
    {
        std::wcin.clear();
        std::cin.clear();
    }
    if (bindStdOut)
    {
        std::wcout.clear();
        std::cout.clear();
    }
    if (bindStdErr)
    {
        std::wcerr.clear();
        std::cerr.clear();
    }
}
#endif
