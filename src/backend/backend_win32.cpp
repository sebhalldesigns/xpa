/***************************************************************
**
** XPA Source File
**
** File         :  backend_win32.cpp
** Module       :  backend
** Author       :  SH
** Created      :  2026-04-18 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Win32 Backend Implementation
**
***************************************************************/

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include "backend.h"

#include <xpa_rust.h>

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <windowsx.h>

#include <string>
#include <stdio.h>
#include <time.h>

/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/

typedef struct {
    HWND          hwnd;
    XpaGpuSurface* surface;
    XpaScene*     scene;
} xpa_window_internal_t;

/***************************************************************
** MARK: STATIC VARIABLES
***************************************************************/

static HINSTANCE instance_handle;
static WNDCLASSW window_class;
static bool running = true;

static double xpa_now_ms(void)
{
    return ((double)clock() * 1000.0) / (double)CLOCKS_PER_SEC;
}

/***************************************************************
** MARK: STATIC FUNCTION DEFS
***************************************************************/

static LRESULT CALLBACK window_procedure(HWND window, UINT msg, WPARAM wparam, LPARAM lparam);

static xpa_window_internal_t* get_window_data(HWND hwnd);

static std::string wide_to_utf8(const std::wstring& w);
static std::wstring utf8_to_wide(const std::string& s);

/***************************************************************
** MARK: PUBLIC FUNCTIONS
***************************************************************/


bool xpa_backend_init(void)
{
    instance_handle = GetModuleHandle(NULL);

    window_class = {0};
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = window_procedure;
    window_class.hInstance = instance_handle;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hbrBackground = NULL;
    window_class.lpszClassName = L"XPA_WINDOW_CLASS";

    if (!RegisterClassW(&window_class))
    {
        fprintf(stderr, "Failed to register XPA_WINDOW_CLASS.");
        return false;
    }

    return true;

}

bool xpa_backend_create_window(const char *title, uint32_t width, uint32_t height, xpa_window_t *window)
{
    double start_ms = xpa_now_ms();
    std::string title_str(title);
    std::wstring wtitle = utf8_to_wide(title_str);

    HWND win32_window = CreateWindowExW(
        0,
        window_class.lpszClassName,
        wtitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        0,
        0,
        instance_handle,
        0
    );

    if (!win32_window)
    {
        fprintf(stderr, "Failed to create window.");
        return false;
    }

    printf("[xpa] CreateWindowExW completed at +%.2f ms\n", xpa_now_ms() - start_ms);

    // Attach GPU surface and scene to the window
    xpa_window_internal_t* data = new xpa_window_internal_t();
    data->hwnd = win32_window;
    data->surface = NULL;
    data->scene = xpa_scene_create();

    if (!data->scene || !xpa_create_surface_for_window((void*)win32_window, width, height, &data->surface))
    {
        if (data->surface)
        {
            xpa_gpu_surface_destroy(data->surface);
        }

        if (data->scene)
        {
            xpa_scene_destroy(data->scene);
        }

        delete data;
        DestroyWindow(win32_window);
        return false;
    }

    printf("[xpa] scene + gpu surface ready at +%.2f ms\n", xpa_now_ms() - start_ms);

    SetWindowLongPtr(win32_window, GWLP_USERDATA, (LONG_PTR)data);

    ShowWindow(win32_window, SW_SHOW);
    UpdateWindow(win32_window);

    printf("[xpa] window shown at +%.2f ms\n", xpa_now_ms() - start_ms);

    *window = (xpa_window_t)win32_window;

    return true;
}

int xpa_backend_run(void)
{
    running = true;

    while (running)
    {

        MSG msg;
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
                printf("QUIT\n");
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        Sleep(0);
        YieldProcessor();

    }

    return 0;
}

/***************************************************************
** MARK: STATIC FUNCTIONS
***************************************************************/


static LRESULT CALLBACK window_procedure(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    xpa_window_internal_t* data = get_window_data(window);


    switch (msg)
    {
        case WM_CLOSE:
        {
            running = false;
            printf("CLOSE\n");
        } break;

        case WM_SIZE:
        {
            if (data && data->surface)
            {
                uint32_t w = LOWORD(lparam);
                uint32_t h = HIWORD(lparam);
                if (w > 0 && h > 0)
                {
                    xpa_gpu_surface_resize(xpa_get_gpu_context(), data->surface, w, h);
                }
            }
        } break;

        case WM_PAINT:
        {
            if (data && data->surface)
            {
                PAINTSTRUCT ps;
                BeginPaint(window, &ps);

                // Clear and rebuild scene from current app state
                xpa_scene_clear(data->scene);
                // ... app-specific drawing would go here via a callback
                xpa_gpu_render(xpa_get_gpu_context(), data->surface, data->scene);

                EndPaint(window, &ps);
            }
            else
            {
                // Fallback before GPU is ready
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(window, &ps);
                FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
                EndPaint(window, &ps);
            }
        } break;

        default:
        {

        } break;
    }

    return DefWindowProcW(window, msg, wparam, lparam);
}

static xpa_window_internal_t* get_window_data(HWND hwnd)
{
    return (xpa_window_internal_t*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}


static std::string wide_to_utf8(const std::wstring& w)
{
    int size = WideCharToMultiByte(
        CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);

    if (size <= 0)
    {
        return {};
    }

    std::string result(size, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, w.c_str(), -1, &result[0], size, nullptr, nullptr);

    result.pop_back();

    return result;
}

static std::wstring utf8_to_wide(const std::string& s)
{
    int size = MultiByteToWideChar(
        CP_UTF8, 0, s.c_str(), -1, nullptr, 0);

    if (size <= 0)
    {
        return {};
    }

    std::wstring result(size, L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, s.c_str(), -1, &result[0], size);

    result.pop_back();

    return result;
}
