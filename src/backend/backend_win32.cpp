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

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <windowsx.h>

#include <glad/glad.h>
#include "wglext.h"

#include <stdio.h>
#include <time.h>
#include <math.h>

#include <string>
#include <vector>
#include <algorithm>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include <nuklear/nuklear.h>

#define NK_GL3_IMPLEMENTATION
#include "nk_gl3.h"

/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/


// See https://www.khronos.org/registry/OpenGL/extensions/ARB/WGL_ARB_create_context.txt for all values
#define WGL_CONTEXT_MAJOR_VERSION_ARB             0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB             0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB              0x9126

#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB          0x00000001

// See https://www.khronos.org/registry/OpenGL/extensions/ARB/WGL_ARB_pixel_format.txt for all values
#define WGL_DRAW_TO_WINDOW_ARB                    0x2001
#define WGL_ACCELERATION_ARB                      0x2003
#define WGL_SUPPORT_OPENGL_ARB                    0x2010
#define WGL_DOUBLE_BUFFER_ARB                     0x2011
#define WGL_PIXEL_TYPE_ARB                        0x2013
#define WGL_COLOR_BITS_ARB                        0x2014
#define WGL_DEPTH_BITS_ARB                        0x2022
#define WGL_STENCIL_BITS_ARB                      0x2023

#define WGL_FULL_ACCELERATION_ARB                 0x2027
#define WGL_TYPE_RGBA_ARB                         0x202B

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/

typedef HGLRC WINAPI wglCreateContextAttribsARB_type(HDC hdc, HGLRC hShareContext,
        const int *attribList);
typedef BOOL WINAPI wglChoosePixelFormatARB_type(HDC hdc, const int *piAttribIList,
        const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);


typedef struct {
    HWND         hwnd;
    HDC          gldc;
    int width;
    int height;
} xpa_window_internal_t;

/***************************************************************
** MARK: STATIC VARIABLES
***************************************************************/

static wglCreateContextAttribsARB_type *wglCreateContextAttribsARB = NULL;
static wglChoosePixelFormatARB_type *wglChoosePixelFormatARB = NULL;
static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
static PFNWGLGETSWAPINTERVALEXTPROC wglGetSwapIntervalEXT = NULL;

static struct nk_context ctx;
static HINSTANCE instance_handle;
static WNDCLASSW window_class;
static bool running = true;
static std::vector<xpa_window_internal_t*> windows;
static int pixel_format;
static PIXELFORMATDESCRIPTOR pfd = { 0 };

/***************************************************************
** MARK: STATIC FUNCTION DEFS
***************************************************************/

static xpa_window_internal_t* get_window_data(HWND hwnd);

static LRESULT CALLBACK window_procedure(HWND window, UINT msg, WPARAM wparam, LPARAM lparam);

static std::string wide_to_utf8(const std::wstring& w);
static std::wstring utf8_to_wide(const std::string& s);
static void set_process_dpi_awareness(void);
static float get_window_dpi_scale(HWND hwnd);

static inline double xpa_now_ms(void)
{
    return ((double)clock() * 1000.0) / (double)CLOCKS_PER_SEC;
}

/***************************************************************
** MARK: PUBLIC FUNCTIONS
***************************************************************/


bool xpa_backend_init(void)
{
    instance_handle = GetModuleHandle(NULL);
    set_process_dpi_awareness();

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

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

    WNDCLASSW gl_window_class = { 0 };
    gl_window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    gl_window_class.lpfnWndProc = DefWindowProcW;
    gl_window_class.hInstance = GetModuleHandle(0);
    gl_window_class.lpszClassName = L"XPA_WGL_INIT_WINDOW_CLASS";
    

    if (!RegisterClassW(&gl_window_class)) 
    {
        fprintf(stderr, "Failed to register dummy OpenGL window.");
        return false;
    }

    HWND dummy_window = CreateWindowExW(
        0,
        gl_window_class.lpszClassName,
        L"Dummy OpenGL Window",
        0,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        gl_window_class.hInstance,
        0);

    if (!dummy_window) 
    {
        fprintf(stderr, "Failed to create dummy OpenGL window.");
        return false; 
    }

    HDC dummy_dc = GetDC(dummy_window);

    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    

    pixel_format = ChoosePixelFormat(dummy_dc, &pfd);
    if (!pixel_format) 
    {
        fprintf(stderr, "Failed to find a suitable pixel format.");
        return false;
    }

    if (!SetPixelFormat(dummy_dc, pixel_format, &pfd)) 
    {
        fprintf(stderr, "Failed to set the pixel format.");
        return false;
    }

    HGLRC dummy_context = wglCreateContext(dummy_dc);
    if (!dummy_context) 
    {
        fprintf(stderr, "Failed to create a dummy OpenGL rendering context.");
        return false;
    }

    if (!wglMakeCurrent(dummy_dc, dummy_context)) 
    {
        fprintf(stderr, "Failed to activate dummy OpenGL rendering context.");
        return false;
    }

    wglCreateContextAttribsARB = (wglCreateContextAttribsARB_type*)wglGetProcAddress(
        "wglCreateContextAttribsARB");
    wglChoosePixelFormatARB = (wglChoosePixelFormatARB_type*)wglGetProcAddress(
        "wglChoosePixelFormatARB");

    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)
        wglGetProcAddress("wglSwapIntervalEXT");

    wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)
        wglGetProcAddress("wglGetSwapIntervalEXT");

    wglMakeCurrent(dummy_dc, 0);
    wglDeleteContext(dummy_context);
    ReleaseDC(dummy_window, dummy_dc);
    DestroyWindow(dummy_window);


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

    // Attach GPU renderer to the window
    xpa_window_internal_t* data = (xpa_window_internal_t*)calloc(1, sizeof(xpa_window_internal_t));
    data->hwnd = win32_window;
    data->gldc = GetDC(win32_window);
    RECT client_rect = {0};
    GetClientRect(win32_window, &client_rect);
    data->width = client_rect.right - client_rect.left;
    data->height = client_rect.bottom - client_rect.top;

    printf("[xpa] renderer ready at +%.2f ms\n", xpa_now_ms() - start_ms);

    SetWindowLongPtr(win32_window, GWLP_USERDATA, (LONG_PTR)data);
    windows.push_back(data);

    int pixel_format_attribs[] = {
        WGL_DRAW_TO_WINDOW_ARB,     GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB,     GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB,      GL_TRUE,
        WGL_ACCELERATION_ARB,       WGL_FULL_ACCELERATION_ARB,
        WGL_PIXEL_TYPE_ARB,         WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB,         32,
        WGL_DEPTH_BITS_ARB,         24,
        WGL_STENCIL_BITS_ARB,       8,
        0
    };

    UINT num_formats;
    wglChoosePixelFormatARB(data->gldc, pixel_format_attribs, 0, 1, &pixel_format, &num_formats);
    if (!num_formats) {
        fprintf(stderr, "Failed to set the OpenGL 3.3 pixel format.");
    }


    DescribePixelFormat(data->gldc, pixel_format, sizeof(pfd), &pfd);
    if (!SetPixelFormat(data->gldc, pixel_format, &pfd)) {
        fprintf(stderr, "Failed to set the OpenGL 3.3 pixel format.");
    }

    // Specify that we want to create an OpenGL 3.3 core profile context
    int gl33_attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0,
    };

    HGLRC gl33_context = wglCreateContextAttribsARB(data->gldc, 0, gl33_attribs);
    if (!gl33_context) {
        fprintf(stderr, "Failed to create OpenGL 3.3 context.");
    }

    if (!wglMakeCurrent(data->gldc, gl33_context)) {
        fprintf(stderr, "Failed to activate OpenGL 3.3 rendering context.");
    }

    if (!gladLoadGL()) 
    {
        fprintf(stderr, "Failed to load OpenGL functions\n");
        return false;
    }

    nk_gl3_init(&ctx, width, height);

    struct nk_font_atlas *atlas;
    nk_gl3_font_stash_begin(&atlas);

    struct nk_font_config cfg = nk_font_config(0);
    cfg.oversample_h = 1;
    cfg.oversample_v = 1;
    cfg.pixel_snap = nk_true;

    char font_path[MAX_PATH];
    ExpandEnvironmentStringsA("%WINDIR%\\Fonts\\segoeui.ttf", font_path, MAX_PATH);

    const float base_font_size = 16.0f;
    const float dpi_scale = get_window_dpi_scale(win32_window);
    float font_size = floorf(base_font_size * dpi_scale + 0.5f);
    if (font_size < 12.0f)
        font_size = 12.0f;

    struct nk_font *font = nk_font_atlas_add_from_file(atlas, font_path, font_size, &cfg);
    if (!font)
        font = nk_font_atlas_add_default(atlas, font_size, &cfg);

    if (font)
        atlas->default_font = font;

    nk_gl3_font_stash_end(&ctx);

    // Start input collection for the next frame.
    nk_input_begin(&ctx);

    ShowWindow(data->hwnd, SW_SHOW);
    UpdateWindow(data->hwnd);
    
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
        MSG msg;

        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
                break;
            }
            
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
 
        }

        for (xpa_window_internal_t* data : windows)
        {
            InvalidateRect(data->hwnd, NULL, FALSE);
        }
       
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
            uint32_t width = LOWORD(lparam);
            uint32_t height = HIWORD(lparam);

            if (data && wparam != SIZE_MINIMIZED)
            {
                data->width = width;
                data->height = height;

                InvalidateRect(window, NULL, FALSE);
            }

            return 0;
        }

        case WM_PAINT:
        {
            if (data)
            {

                

                PAINTSTRUCT ps;
                BeginPaint(window, &ps);

                 /* End input collection, build UI, render */
                nk_input_end(&ctx);

                if (nk_begin(&ctx, "Test", nk_rect(20, 20, 300, 200),
                    NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE))
                {
                    nk_layout_row_dynamic(&ctx, 25, 1);
                    nk_label(&ctx, "Hello from Nuklear", NK_TEXT_LEFT);
                    if (nk_button_label(&ctx, "Click me"))
                        printf("clicked!\n");
                }
                nk_end(&ctx);

            
                glViewport(0, 0, data->width, data->height);
                glDisable(GL_SCISSOR_TEST);
                glClearColor(0.1f, 0.15f, 0.2f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                nk_gl3_render(&ctx, data->width, data->height);

                
                SwapBuffers(data->gldc);

                EndPaint(window, &ps);

                /* Begin collecting input for next frame */
                nk_input_begin(&ctx);
            }
        
            return 0;
        }

        case WM_NCDESTROY:
        {
            if (data)
            {
                SetWindowLongPtr(window, GWLP_USERDATA, 0);

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

        case WM_MOUSEMOVE:
            nk_input_motion(&ctx, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;

        case WM_LBUTTONDOWN:
            SetCapture(window);
            nk_input_button(&ctx, NK_BUTTON_LEFT, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), 1);
            return 0;

        case WM_LBUTTONUP:
            nk_input_button(&ctx, NK_BUTTON_LEFT, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), 0);
            ReleaseCapture();
            return 0;

        case WM_RBUTTONDOWN:
            nk_input_button(&ctx, NK_BUTTON_RIGHT, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), 1);
            return 0;

        case WM_RBUTTONUP:
            nk_input_button(&ctx, NK_BUTTON_RIGHT, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), 0);
            return 0;

        case WM_MOUSEWHEEL:
            nk_input_scroll(&ctx, nk_vec2(0, (float)GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA));
            return 0;

        case WM_CHAR:
            if (wparam >= 32)
                nk_input_unicode(&ctx, (nk_rune)wparam);
            return 0;

        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            int down = (msg == WM_KEYDOWN);
            BOOL ctrl = GetKeyState(VK_CONTROL) & (1 << 15);

            if (wparam == VK_BACK)        nk_input_key(&ctx, NK_KEY_BACKSPACE, down);
            else if (wparam == VK_DELETE) nk_input_key(&ctx, NK_KEY_DEL, down);
            else if (wparam == VK_RETURN) nk_input_key(&ctx, NK_KEY_ENTER, down);
            else if (wparam == VK_TAB)    nk_input_key(&ctx, NK_KEY_TAB, down);
            else if (wparam == VK_LEFT)   nk_input_key(&ctx, NK_KEY_LEFT, down);
            else if (wparam == VK_RIGHT)  nk_input_key(&ctx, NK_KEY_RIGHT, down);
            else if (wparam == VK_UP)     nk_input_key(&ctx, NK_KEY_UP, down);
            else if (wparam == VK_DOWN)   nk_input_key(&ctx, NK_KEY_DOWN, down);
            else if (wparam == 'C' && ctrl) nk_input_key(&ctx, NK_KEY_COPY, down);
            else if (wparam == 'V' && ctrl) nk_input_key(&ctx, NK_KEY_PASTE, down);
            else if (wparam == 'X' && ctrl) nk_input_key(&ctx, NK_KEY_CUT, down);
            else if (wparam == 'A' && ctrl) nk_input_key(&ctx, NK_KEY_TEXT_SELECT_ALL, down);
            else if (wparam == 'Z' && ctrl) nk_input_key(&ctx, NK_KEY_TEXT_UNDO, down);
            else if (wparam == 'Y' && ctrl) nk_input_key(&ctx, NK_KEY_TEXT_REDO, down);
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

static void set_process_dpi_awareness(void)
{
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE)-4)
#endif
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE ((HANDLE)-3)
#endif

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32)
        return;

    typedef BOOL (WINAPI *SetProcessDpiAwarenessContextProc)(HANDLE);
    SetProcessDpiAwarenessContextProc set_dpi_context =
        (SetProcessDpiAwarenessContextProc)GetProcAddress(user32, "SetProcessDpiAwarenessContext");

    if (set_dpi_context)
    {
        if (set_dpi_context(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
            return;
        if (set_dpi_context(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE))
            return;
    }

    typedef BOOL (WINAPI *SetProcessDPIAwareProc)(void);
    SetProcessDPIAwareProc set_dpi_aware =
        (SetProcessDPIAwareProc)GetProcAddress(user32, "SetProcessDPIAware");
    if (set_dpi_aware)
        set_dpi_aware();
}

static float get_window_dpi_scale(HWND hwnd)
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32)
    {
        typedef UINT (WINAPI *GetDpiForWindowProc)(HWND);
        GetDpiForWindowProc get_dpi_for_window =
            (GetDpiForWindowProc)GetProcAddress(user32, "GetDpiForWindow");
        if (get_dpi_for_window)
        {
            UINT dpi = get_dpi_for_window(hwnd);
            if (dpi > 0)
                return (float)dpi / 96.0f;
        }
    }
    return 1.0f;
}
