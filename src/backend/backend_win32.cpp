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
#include <vector>
#include <algorithm>
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
    uint32_t      pending_width;
    uint32_t      pending_height;
    bool          needs_resize;
    bool          render_queued;
} xpa_window_internal_t;

/***************************************************************
** MARK: STATIC VARIABLES
***************************************************************/

static HINSTANCE instance_handle;
static WNDCLASSW window_class;
static bool running = true;
static std::vector<xpa_window_internal_t*> windows;
static const UINT WM_XPA_RENDER = WM_APP + 1;

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
    data->pending_width = width;
    data->pending_height = height;
    data->needs_resize = false;
    data->render_queued = false;

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
    windows.push_back(data);

    ShowWindow(win32_window, SW_SHOW);
    UpdateWindow(win32_window);

    printf("[xpa] window shown at +%.2f ms\n", xpa_now_ms() - start_ms);

    *window = (xpa_window_t)win32_window;

    return true;
}

int xpa_backend_run(void)
{
    running = true;

    MSG msg;
    while (running)
    {
        BOOL got_message = GetMessageW(&msg, 0, 0, 0);
        if (got_message <= 0)
        {
            break;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
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
            DestroyWindow(window);
            return 0;
        }

        case WM_ERASEBKGND:
        {
            return 1;
        }

        case WM_SIZE:
        {
            if (data && data->surface && wparam != SIZE_MINIMIZED)
            {
                uint32_t width = LOWORD(lparam);
                uint32_t height = HIWORD(lparam);
                if (width > 0 && height > 0)
                {
                    data->pending_width = width;
                    data->pending_height = height;
                    data->needs_resize = true;

                    if (!data->render_queued)
                    {
                        data->render_queued = true;

                        if (!PostMessageW(data->hwnd, WM_XPA_RENDER, 0, 0))
                        {
                            data->render_queued = false;
                        }
                    }
                }
            }
            return 0;
        }

        case WM_XPA_RENDER:
        {
            if (data && data->surface)
            {
                if (data->needs_resize)
                {
                    xpa_gpu_surface_resize(
                        xpa_get_gpu_context(),
                        data->surface,
                        data->pending_width,
                        data->pending_height
                    );
                    data->needs_resize = false;
                }

                xpa_gpu_render(xpa_get_gpu_context(), data->surface, data->scene);
            }
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(window, &ps);
            EndPaint(window, &ps);

            if (data && data->surface && !data->render_queued)
            {
                data->render_queued = true;

                if (!PostMessageW(data->hwnd, WM_XPA_RENDER, 0, 0))
                {
                    data->render_queued = false;
                }
            }
            return 0;
        }

        case WM_NCDESTROY:
        {
            if (data)
            {
                SetWindowLongPtr(window, GWLP_USERDATA, 0);

                if (data->surface)
                {
                    xpa_gpu_surface_destroy(data->surface);
                    data->surface = NULL;
                }

                if (data->scene)
                {
                    xpa_scene_destroy(data->scene);
                    data->scene = NULL;
                }

                windows.erase(
                    std::remove(windows.begin(), windows.end(), data),
                    windows.end()
                );
                delete data;
            }

            if (windows.empty())
            {
                running = false;
                PostQuitMessage(0);
            }
            return 0;
        }

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
