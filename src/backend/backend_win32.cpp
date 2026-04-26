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

#include <draw/draw.h>
#include <control/control.h>
#include <workbench/workbench.h>


#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>


#include <glad/glad.h>
#include "wglext.h"

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include <string>
#include <vector>
#include <algorithm>
#include <climits>

#define NK_GL3_IMPLEMENTATION
#include "nk_gl3.h"


/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

#define WINDOW_BUTTONS_WIDTH (140)

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif


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
    workbench_t workbench;
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
static nk_bool ui_show_metrics = nk_true;
static nk_bool ui_enable_filter = nk_true;
static int ui_mode = 0;
static int ui_rate_hz = 50;
static float ui_threshold = 0.45f;
static nk_size ui_load_pct = 42;
static int ui_channel_index = 0;
static char ui_filter_text[64] = "0x180";
static char ui_menu_search_text[128] = "";
static float ui_chart_phase = 0.0f;
static float ui_chart_values[24] = {0};
static struct nk_font *ui_font_semibold = NULL;
static struct nk_font *ui_font_icon_regular = NULL;
static struct nk_font *ui_font_icon_solid = NULL;
static xpa_cursor_t current_cursor = XPA_CURSOR_ARROW;

extern "C" {
extern const unsigned char assets_fonts_Font_Awesome_7_Free_Regular_400_otf[];
extern const unsigned assets_fonts_Font_Awesome_7_Free_Regular_400_otf_size;
extern const unsigned char assets_fonts_Font_Awesome_7_Free_Solid_900_otf[];
extern const unsigned assets_fonts_Font_Awesome_7_Free_Solid_900_otf_size;
}

/***************************************************************
** MARK: STATIC FUNCTION DEFS
***************************************************************/

static void xpa_set_theme(struct nk_context *ctx);


static xpa_window_internal_t* get_window_data(HWND hwnd);

static LRESULT CALLBACK window_procedure(HWND window, UINT msg, WPARAM wparam, LPARAM lparam);

static std::string wide_to_utf8(const std::wstring& w);
static std::wstring utf8_to_wide(const std::string& s);
static void set_process_dpi_awareness(void);
static float get_window_dpi_scale(HWND hwnd);

static LRESULT titlebar_hit_test(HWND hwnd, int x, int y, int titlebar_height);

static void apply_dwm_frame(HWND hwnd);

static void render_dock(bool mouse_down, float x, float y, float w, float h);
static HCURSOR xpa_cursor_handle(xpa_cursor_t cursor);

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

    MARGINS margins = {0, 0, 32, 0};
    DwmExtendFrameIntoClientArea(win32_window, &margins);

    SetWindowPos(win32_window, NULL, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    /* Dark mode caption buttons */
    BOOL dark_mode = TRUE;
    DwmSetWindowAttribute(win32_window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark_mode, sizeof(dark_mode));

    /* Caption background — COLORREF is 0x00BBGGRR */
    COLORREF caption_color = RGB(0, 0, 0);  /* match your theme bg */
    DwmSetWindowAttribute(win32_window, DWMWA_CAPTION_COLOR, &caption_color, sizeof(caption_color));

    printf("[xpa] CreateWindowExW completed at +%.2f ms\n", xpa_now_ms() - start_ms);

    xpa_window_internal_t* data = (xpa_window_internal_t*)calloc(1, sizeof(xpa_window_internal_t));
    data->hwnd = win32_window;
    data->gldc = GetDC(win32_window);
    RECT client_rect = {0};
    GetClientRect(win32_window, &client_rect);
    data->width = client_rect.right - client_rect.left;
    data->height = client_rect.bottom - client_rect.top;
    workbench_init(&data->workbench);
    workbench_set_frame(&data->workbench, {0, 0, (float)data->width, (float)data->height});

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

    if (wglSwapIntervalEXT) {
        wglSwapIntervalEXT(0);   // disable vsync
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

    char semibold_font_path[MAX_PATH];
    ExpandEnvironmentStringsA("%WINDIR%\\Fonts\\seguisb.ttf", semibold_font_path, MAX_PATH);
    ui_font_semibold = nk_font_atlas_add_from_file(atlas, semibold_font_path, font_size, &cfg);
    if (!ui_font_semibold)
        ui_font_semibold = font;

    static const nk_rune font_awesome_ranges[] = {
        0x20, 0x7f,
        0xe000, 0xefff,
        0xf000, 0xf8ff,
        0
    };

    struct nk_font_config icon_cfg = nk_font_config(0);
    icon_cfg.oversample_h = 1;
    icon_cfg.oversample_v = 1;
    icon_cfg.pixel_snap = nk_true;
    icon_cfg.range = font_awesome_ranges;

    const float icon_font_size = 13.0f;
    ui_font_icon_regular = nk_font_atlas_add_from_memory(atlas,
        (void*)assets_fonts_Font_Awesome_7_Free_Regular_400_otf,
        (nk_size)assets_fonts_Font_Awesome_7_Free_Regular_400_otf_size,
        icon_font_size, &icon_cfg);
    ui_font_icon_solid = nk_font_atlas_add_from_memory(atlas,
        (void*)assets_fonts_Font_Awesome_7_Free_Solid_900_otf,
        (nk_size)assets_fonts_Font_Awesome_7_Free_Solid_900_otf_size,
        icon_font_size, &icon_cfg);

    if (font)
        atlas->default_font = font;

    control_set_normal_font(font);
    control_set_bold_font(ui_font_semibold);
    control_set_icon_fonts(ui_font_icon_regular, ui_font_icon_solid);


    nk_gl3_font_stash_end(&ctx);

    xpa_set_theme(&ctx);

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

    }

    return 0;
}

void xpa_backend_set_cursor(xpa_cursor_t cursor)
{
    current_cursor = cursor;
    SetCursor(xpa_cursor_handle(current_cursor));
}

/***************************************************************
** MARK: STATIC FUNCTIONS
***************************************************************/

static bool is_mouse_down = false;
static float mouse_x = 0.0f, mouse_y = 0.0f;

static LRESULT CALLBACK window_procedure(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    xpa_window_internal_t* data = get_window_data(window);

    /* Let DWM handle caption button hit testing first */
    LRESULT dwm_result = 0;
    if (DwmDefWindowProc(window, msg, wparam, lparam, &dwm_result))
    {
        return dwm_result;
    }
        

    switch (msg)
    {
        case WM_CREATE:
            apply_dwm_frame(window);
            return 0;

        case WM_DWMCOMPOSITIONCHANGED:
            apply_dwm_frame(window);
            return 0;

        case WM_CLOSE:
        {
            DestroyWindow(window);
            return 0;
        }

        case WM_NCCALCSIZE:
        {
            if (wparam == TRUE)
            {
                NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lparam;

                UINT dpi = GetDpiForWindow(window);
                int frame_x = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
                int frame_y = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
                int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

                // Keep resize borders working.
                params->rgrc[0].left   += frame_x + padding;
                params->rgrc[0].right  -= frame_x + padding;
                params->rgrc[0].bottom -= frame_y + padding;

                /*
                ** N.B - changing rgrc[0].bottom seems to break the caption buttons
                ** from when the window is maximised. Don't touch it and handle
                ** in the client area rendering code instead.
                */

                return 0;
            }

            return DefWindowProcW(window, msg, wparam, lparam);
        }

        case WM_NCHITTEST:
        {
      
            int titlebar_h = 30;  /* match your menu bar height */
            LRESULT hit = titlebar_hit_test(window,
                GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), titlebar_h);
            
            if (hit != HTCLIENT)
                return hit;
            break;
        }

        case WM_ERASEBKGND:
        {
            return 1;
        } break;

        case WM_SIZE:
        {
            uint32_t width = LOWORD(lparam);
            uint32_t height = HIWORD(lparam);

            static xpa_rect_t frame = {0, 0, 0, 0};

            if (data && wparam != SIZE_MINIMIZED)
            {
                data->width = width;
                data->height = height;
                frame.width = (float)width;
                frame.height = (float)height;
                frame.y = 0.0f;

                if (IsZoomed(window))
                {
                    frame.height -= 6.0f; // tune as needed to avoid caption button overlap
                    frame.y = 6.0f;
                }
                
                workbench_set_frame(&data->workbench, frame);

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

                #if 0
                const struct nk_user_font *f = ctx.style.font;
                const struct nk_user_font *brand_font = ui_font_semibold ? &ui_font_semibold->handle : f;
                const float menu_popup_rounding = 8.0f;

                auto text_width_for = [](const struct nk_user_font *font, const char *txt) -> float {
                    if (!font || !txt)
                        return 0.0f;
                    return font->width(font->userdata, font->height, txt, (int)strlen(txt));
                };

                float top_inset = 0.0f;

                if (IsZoomed(window))
                    top_inset += 6.0f; // tune as needed   

                if (nk_begin(&ctx, "Main Menu",
                    nk_rect(0.0f, top_inset, (float)data->width - WINDOW_BUTTONS_WIDTH, 32.0f),
                    NK_WINDOW_NO_SCROLLBAR))
                {
                    enum { num_menus = 4, items_per_menu = 8 };
                    const char *menu_labels[num_menus] = {"File", "Edit", "View", "Help"};
                    const char *menu_items[num_menus][items_per_menu] = {
                        {"New...", "Open...", "Open Workspace...", "Save", "Save As...", "Settings", "Exit", ""},
                        {"Cut", "Copy", "Paste", "Find", "Replace", "", "", ""},
                        {"New Window", "Open Layout", "Save Layout As...", "Reset Layout", "", "", "", ""},
                        {"Documentation", "About", "", "", "", "", "", ""}
                    };
                    const char *menu_shortcuts[num_menus][items_per_menu] = {
                        {"Ctrl+N", "Ctrl+O", "", "Ctrl+S", "Ctrl+Shift+S", "Ctrl+,", "Alt+F4", ""},
                        {"Ctrl+X", "Ctrl+C", "Ctrl+V", "Ctrl+F", "Ctrl+H", "", "", ""},
                        {"", "", "", "", "", "", "", ""},
                        {"F1", "", "", "", "", "", "", ""}
                    };

                    const float left_margin = 8.0f;
                    const float row_y = 4.0f;
                    const float row_h = 20.0f;
                    const float menu_content_y = row_y - 2.0f;
                    const float control_y = row_y;
                    const float control_h = row_h - 2.0f;
                    const float section_gap = 4.0f;
                    const float menu_item_gap = 2.0f;
                    const float button_gap = 8.0f;
                    const float right_gap = 100.0f;

                    const float label_w = text_width_for(brand_font, "BusLab") + 6.0f;

                    float menu_item_w[num_menus] = {0};
                    float menu_total_w = 0.0f;
                    for (int i = 0; i < num_menus; ++i)
                    {
                        menu_item_w[i] = text_width_for(f, menu_labels[i]) + (ctx.style.menu_button.padding.x * 2.0f) + 8.0f;
                        menu_total_w += menu_item_w[i];
                    }
                    menu_total_w += menu_item_gap * (num_menus - 1);

                    const float menu_x_start = left_margin + label_w + section_gap;
                    const float left_block_end = menu_x_start + menu_total_w;

                    const float button_chrome = (ctx.style.button.padding.x * 2.0f) + 12.0f;
                    const float settings_w = text_width_for(f, "Settings") + button_chrome;
                    const float help_w = text_width_for(f, "Help") + button_chrome;
                    const float help_x = (float)data->width - right_gap - help_w;
                    const float settings_x = help_x - button_gap - settings_w;

                    const float center_x = (float)data->width * 0.5f;
                    const float search_pref_w = 300.0f;
                    const float search_min_w = 150.0f;
                    const float max_centered_left_w = 2.0f * (center_x - (left_block_end + section_gap));
                    const float max_centered_right_w = 2.0f * ((settings_x - section_gap) - center_x);
                    float max_centered_w = NK_MIN(max_centered_left_w, max_centered_right_w);
                    if (max_centered_w < 0.0f)
                        max_centered_w = 0.0f;

                    float search_w = search_pref_w;
                    if (search_w > max_centered_w)
                        search_w = max_centered_w;
                    if (search_w < search_min_w && max_centered_w >= search_min_w)
                        search_w = search_min_w;
                    const float search_x = center_x - (search_w * 0.5f);

                    nk_bool pushed_button_padding = nk_style_push_vec2(
                        &ctx, &ctx.style.button.padding, nk_vec2(5.0f, 0.0f));
                    nk_bool pushed_button_touch_padding = nk_style_push_vec2(
                        &ctx, &ctx.style.button.touch_padding, nk_vec2(0.0f, 0.0f));
                    nk_bool pushed_edit_padding = nk_style_push_vec2(
                        &ctx, &ctx.style.edit.padding, nk_vec2(3.0f, 0.0f));
                    nk_bool pushed_edit_row_padding = nk_style_push_float(
                        &ctx, &ctx.style.edit.row_padding, 0.0f);
                    nk_bool pushed_button_border = nk_style_push_float(
                        &ctx, &ctx.style.button.border, 0.0f);
                    nk_bool pushed_edit_border = nk_style_push_float(
                        &ctx, &ctx.style.edit.border, 0.0f);

                    nk_menubar_begin(&ctx);
                    nk_layout_space_begin(&ctx, NK_STATIC, row_h, num_menus + 4);

                    nk_layout_space_push(&ctx, nk_rect(left_margin, menu_content_y, label_w, row_h));
                    nk_bool pushed_brand_font = nk_false;
                    if (brand_font)
                        pushed_brand_font = nk_style_push_font(&ctx, brand_font);
                    nk_label(&ctx, "BusLab", NK_TEXT_LEFT);
                    if (pushed_brand_font)
                        nk_style_pop_font(&ctx);

                    float menu_x = menu_x_start;
                    for (int i = 0; i < num_menus; ++i)
                    {
                        nk_layout_space_push(&ctx, nk_rect(menu_x, menu_content_y, menu_item_w[i], row_h));

                        nk_bool pushed_rounding = nk_style_push_float(
                            &ctx, &ctx.style.window.rounding, menu_popup_rounding);

                        int menu_item_count = 0;
                        for (int j = 0; j < items_per_menu; ++j)
                        {
                            if (menu_items[i][j][0] != '\0')
                                menu_item_count++;
                        }

                        const float popup_item_row_h = 24.0f + ctx.style.window.spacing.y;
                        const float popup_h = ctx.style.window.menu_padding.y +
                            (menu_item_count * popup_item_row_h);

                        if (nk_menu_begin_label(&ctx, menu_labels[i], NK_TEXT_CENTERED,
                            nk_vec2(220.0f, popup_h)))
                        {
                            struct nk_command_buffer *canvas = nk_window_get_canvas(&ctx);
                            nk_layout_row_dynamic(&ctx, 24, 1);
                            for (int j = 0; j < menu_item_count; ++j)
                            {
                                if (nk_menu_item_label(&ctx, menu_items[i][j], NK_TEXT_LEFT))
                                {
                                    printf("[ui] %s -> %s clicked\n", menu_labels[i], menu_items[i][j]);
                                    if (strcmp(menu_items[i][j], "Exit") == 0)
                                        PostQuitMessage(0);
                                }

                                struct nk_rect bounds = nk_layout_widget_bounds(&ctx);
                                const char *shortcut = menu_shortcuts[i][j];
                                if (shortcut[0] != '\0')
                                {
                                    float pad = ctx.style.contextual_button.padding.x;
                                    float shortcut_w = text_width_for(f, shortcut);
                                    struct nk_rect sc_rect;
                                    sc_rect.x = bounds.x + bounds.w - shortcut_w - pad;
                                    sc_rect.y = bounds.y + 4.0f;
                                    sc_rect.w = shortcut_w;
                                    sc_rect.h = bounds.h;
                                    nk_draw_text(canvas, sc_rect, shortcut, (int)strlen(shortcut), f,
                                        nk_rgba(0, 0, 0, 0), nk_rgb(110, 110, 135));
                                }
                            }
                            nk_menu_end(&ctx);
                        }

                        if (pushed_rounding)
                            nk_style_pop_float(&ctx);

                        menu_x += menu_item_w[i] + menu_item_gap;
                    }

                    nk_layout_space_push(&ctx, nk_rect(search_x, control_y, search_w, control_h));
                    if (search_w >= 40.0f)
                        nk_edit_string_zero_terminated(&ctx, NK_EDIT_FIELD,
                            ui_menu_search_text, (int)sizeof(ui_menu_search_text), nk_filter_default);
                    else
                        nk_label(&ctx, "", NK_TEXT_LEFT);

                    nk_layout_space_push(&ctx, nk_rect(settings_x, control_y, settings_w, control_h));
                    if (nk_button_label(&ctx, "Settings"))
                        printf("[ui] Settings clicked\n");

                    nk_layout_space_push(&ctx, nk_rect(help_x, control_y, help_w, control_h));
                    if (nk_button_label(&ctx, "Help"))
                        printf("[ui] Help clicked\n");

                    nk_layout_space_end(&ctx);
                    nk_menubar_end(&ctx);

                    if (pushed_edit_border)
                        nk_style_pop_float(&ctx);
                    if (pushed_button_border)
                        nk_style_pop_float(&ctx);
                    if (pushed_edit_row_padding)
                        nk_style_pop_float(&ctx);
                    if (pushed_edit_padding)
                        nk_style_pop_vec2(&ctx);
                    if (pushed_button_touch_padding)
                        nk_style_pop_vec2(&ctx);
                    if (pushed_button_padding)
                        nk_style_pop_vec2(&ctx);
                }

                


                nk_end(&ctx);

                if (nk_begin(&ctx, "Content",
                    nk_rect(0.0f, top_inset + 32.0f, (float)data->width, (float)data->height - top_inset - 32.0f),
                    NK_WINDOW_NO_SCROLLBAR))

                    render_dock(is_mouse_down, mouse_x, mouse_y, (float)data->width, (float)data->height);

                    /* Container with horizontal scroll */
                    nk_layout_row_dynamic(&ctx, 120, 1);
                    if (nk_group_begin(&ctx, "hscroll", NK_WINDOW_BORDER))
                    {
                        /* Single row of fixed-width items */
                        int item_count = 20;
                        float item_size = 80.0f;
                        float item_gap = 8.0f;

                        nk_layout_row_begin(&ctx, NK_STATIC, 80, item_count);

                        for (int i = 0; i < item_count; i++)
                        {
                            nk_layout_row_push(&ctx, item_size);

                            /* Each square is a button or custom widget */
                            char label[16];
                            snprintf(label, sizeof(label), "%d", i);

                            if (nk_button_label(&ctx, label))
                                printf("clicked %d\n", i);
                        }

                        nk_layout_row_end(&ctx);
                        nk_group_end(&ctx);
                    }
                nk_end(&ctx);

                
                #endif
                
                nk_begin(&ctx, "Workbench", nk_rect(0.0f, 0.0f, (float)data->width, (float)data->height), NK_WINDOW_NO_SCROLLBAR);
                nk_layout_space_begin(&ctx, NK_STATIC, (float)data->height, INT_MAX);

                draw_set_context(&ctx);
                control_set_context(&ctx);
                workbench_render(&data->workbench);
                
                nk_layout_space_end(&ctx);
                nk_end(&ctx);

                glViewport(0, 0, data->width, data->height);
                glDisable(GL_SCISSOR_TEST);
                glClearColor(38.0f/255.0f, 38.0f/255.0f, 46.0f/255.0f, 0.0f);
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
                free(data);
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
            mouse_x = (float)GET_X_LPARAM(lparam);
            mouse_y = (float)GET_Y_LPARAM(lparam);
            workbench_input_motion(&data->workbench, {mouse_x, mouse_y});
            InvalidateRect(window, NULL, FALSE);
            return 0;

        case WM_SETCURSOR:
        {
            if (LOWORD(lparam) == HTCLIENT)
            {
                SetCursor(xpa_cursor_handle(current_cursor));
                return TRUE;
            }
        } break;

        case WM_LBUTTONDOWN:
            SetCapture(window);
            nk_input_button(&ctx, NK_BUTTON_LEFT, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), 1);
            is_mouse_down = true;
            workbench_input_button(&data->workbench, XPA_BUTTON_PRIMARY, {mouse_x, mouse_y}, true);
            InvalidateRect(window, NULL, FALSE);
            return 0;

        case WM_LBUTTONUP:
            nk_input_button(&ctx, NK_BUTTON_LEFT, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), 0);
            ReleaseCapture();
            is_mouse_down = false;
            workbench_input_button(&data->workbench, XPA_BUTTON_PRIMARY, {mouse_x, mouse_y}, false);
            InvalidateRect(window, NULL, FALSE);
            return 0;

        case WM_RBUTTONDOWN:
            nk_input_button(&ctx, NK_BUTTON_RIGHT, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), 1);
            workbench_input_button(&data->workbench, XPA_BUTTON_SECONDARY, {mouse_x, mouse_y}, true);
            InvalidateRect(window, NULL, FALSE);
            return 0;

        case WM_RBUTTONUP:
            nk_input_button(&ctx, NK_BUTTON_RIGHT, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), 0);
            workbench_input_button(&data->workbench, XPA_BUTTON_SECONDARY, {mouse_x, mouse_y}, false);
            InvalidateRect(window, NULL, FALSE);
            return 0;

        case WM_MOUSEWHEEL:
            nk_input_scroll(&ctx, nk_vec2(0, (float)GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA));
            InvalidateRect(window, NULL, FALSE);
            return 0;

        case WM_CHAR:
            if (wparam >= 32)
                nk_input_unicode(&ctx, (nk_rune)wparam);
            InvalidateRect(window, NULL, FALSE);    
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

            InvalidateRect(window, NULL, FALSE);
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

static HCURSOR xpa_cursor_handle(xpa_cursor_t cursor)
{
    LPCWSTR cursor_id = IDC_ARROW;

    switch (cursor)
    {
        case XPA_CURSOR_RESIZE_EW:
            cursor_id = IDC_SIZEWE;
            break;

        case XPA_CURSOR_RESIZE_NS:
            cursor_id = IDC_SIZENS;
            break;

        case XPA_CURSOR_ARROW:
        default:
            cursor_id = IDC_ARROW;
            break;
    }

    return LoadCursorW(NULL, cursor_id);
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

static void xpa_set_theme(struct nk_context *ctx)
{
    struct nk_color bg        = nk_rgb(30, 30, 36);
    struct nk_color panel     = nk_rgb(38, 38, 46);
    struct nk_color border    = nk_rgb(55, 55, 65);
    struct nk_color header    = nk_rgb(42, 42, 52);
    struct nk_color text      = nk_rgb(210, 210, 220);
    struct nk_color text_dim  = nk_rgb(140, 140, 160);
    struct nk_color accent    = nk_rgb(60, 120, 215);
    struct nk_color hover     = nk_rgb(50, 50, 62);
    struct nk_color active    = nk_rgb(60, 60, 74);
    struct nk_color slider_bg = nk_rgb(48, 48, 58);

    struct nk_style *s = &ctx->style;

    /* Window */
    s->window.background            = nk_rgba(0, 0, 0, 0);
    s->window.fixed_background      = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    s->window.border_color          = border;
    s->window.border                = 0.0f;
    s->window.header.normal         = nk_style_item_color(header);
    s->window.header.hover          = nk_style_item_color(header);
    s->window.header.active         = nk_style_item_color(header);
    s->window.header.label_normal   = text;
    s->window.header.label_hover    = text;
    s->window.header.label_active   = text;
    s->window.header.padding        = nk_vec2(4, 2);
    s->window.padding               = nk_vec2(0, 0);
    s->window.spacing               = nk_vec2(4, 4);
    s->window.group_padding         = nk_vec2(4, 4);
    s->window.rounding = 0.0f;

    /* Menu popup background */
    s->window.contextual_border_color = border;
    s->window.contextual_border       = 1.0f;
    s->window.combo_border_color      = border;
    s->window.combo_border            = 1.0f;
    s->window.menu_border             = 0.0f;

    /* Button */
    s->button.normal          = nk_style_item_color(nk_rgb(52, 52, 64));
    s->button.hover           = nk_style_item_color(hover);
    s->button.active          = nk_style_item_color(accent);
    s->button.border_color    = border;
    s->button.border          = 1.0f;
    s->button.rounding        = 3.0f;
    s->button.text_normal     = text;
    s->button.text_hover      = text;
    s->button.text_active     = nk_rgb(255, 255, 255);
    s->button.padding         = nk_vec2(8, 4);

    /* Contextual button (menu items) */
    s->contextual_button.normal       = nk_style_item_color(panel);
    s->contextual_button.hover        = nk_style_item_color(accent);
    s->contextual_button.active       = nk_style_item_color(accent);
    s->contextual_button.text_normal  = text;
    s->contextual_button.text_hover   = nk_rgb(255, 255, 255);
    s->contextual_button.text_active  = nk_rgb(255, 255, 255);
    s->contextual_button.padding      = nk_vec2(12, 4);
    s->contextual_button.rounding     = 4.0f;

    /* Menu button (items inside dropdown menus) */
    s->menu_button.normal       = nk_style_item_color(panel);
    s->menu_button.hover        = nk_style_item_color(accent);
    s->menu_button.active       = nk_style_item_color(accent);
    s->menu_button.text_normal  = text;
    s->menu_button.text_hover   = nk_rgb(255, 255, 255);
    s->menu_button.text_active  = nk_rgb(255, 255, 255);
    s->menu_button.padding      = nk_vec2(2, 2);
    s->menu_button.rounding      = 4.0f;
    s->menu_button.border      = 0.0f;
    s->menu_button.touch_padding      = nk_vec2(0, 4);

    /* Text */
    s->text.color = text;

    /* Checkbox */
    s->checkbox.normal          = nk_style_item_color(slider_bg);
    s->checkbox.hover           = nk_style_item_color(hover);
    s->checkbox.active          = nk_style_item_color(active);
    s->checkbox.cursor_normal   = nk_style_item_color(accent);
    s->checkbox.cursor_hover    = nk_style_item_color(accent);
    s->checkbox.text_normal     = text;
    s->checkbox.text_hover      = text;
    s->checkbox.text_active     = text;
    s->checkbox.border_color    = border;
    s->checkbox.border          = 1.0f;
    s->checkbox.padding         = nk_vec2(3, 3);

    /* Slider */
    s->slider.normal            = nk_style_item_color(slider_bg);
    s->slider.hover             = nk_style_item_color(slider_bg);
    s->slider.active            = nk_style_item_color(slider_bg);
    s->slider.bar_normal        = border;
    s->slider.bar_hover         = border;
    s->slider.bar_active        = border;
    s->slider.bar_filled        = accent;
    s->slider.cursor_normal     = nk_style_item_color(accent);
    s->slider.cursor_hover      = nk_style_item_color(nk_rgb(80, 140, 235));
    s->slider.cursor_active     = nk_style_item_color(nk_rgb(100, 160, 255));
    s->slider.cursor_size       = nk_vec2(12, 20);
    s->slider.bar_height        = 4;

    /* Progress */
    s->progress.normal          = nk_style_item_color(slider_bg);
    s->progress.hover           = nk_style_item_color(slider_bg);
    s->progress.active          = nk_style_item_color(slider_bg);
    s->progress.cursor_normal   = nk_style_item_color(accent);
    s->progress.cursor_hover    = nk_style_item_color(accent);
    s->progress.cursor_active   = nk_style_item_color(accent);
    s->progress.border_color    = border;
    s->progress.border          = 1.0f;
    s->progress.rounding        = 2.0f;
    s->progress.cursor_rounding = 2.0f;

    /* Property (number edit) */
    s->property.normal          = nk_style_item_color(slider_bg);
    s->property.hover           = nk_style_item_color(hover);
    s->property.active          = nk_style_item_color(active);
    s->property.border_color    = border;
    s->property.border          = 1.0f;
    s->property.rounding        = 3.0f;
    s->property.label_normal    = text;
    s->property.label_hover     = text;
    s->property.label_active    = text;

    /* Edit (text input) */
    s->edit.normal              = nk_style_item_color(nk_rgb(34, 34, 42));
    s->edit.hover               = nk_style_item_color(nk_rgb(38, 38, 48));
    s->edit.active              = nk_style_item_color(nk_rgb(34, 34, 42));
    s->edit.border_color        = border;
    s->edit.border              = 1.0f;
    s->edit.rounding            = 3.0f;
    s->edit.cursor_normal       = text;
    s->edit.cursor_hover        = text;
    s->edit.cursor_text_normal  = bg;
    s->edit.cursor_text_hover   = bg;
    s->edit.text_normal         = text;
    s->edit.text_hover          = text;
    s->edit.text_active         = text;
    s->edit.selected_normal     = accent;
    s->edit.selected_hover      = accent;
    s->edit.selected_text_normal = nk_rgb(255, 255, 255);
    s->edit.selected_text_hover  = nk_rgb(255, 255, 255);
    s->edit.padding             = nk_vec2(4, 4);

    /* Combo */
    s->combo.normal             = nk_style_item_color(slider_bg);
    s->combo.hover              = nk_style_item_color(hover);
    s->combo.active             = nk_style_item_color(active);
    s->combo.border_color       = border;
    s->combo.border             = 1.0f;
    s->combo.rounding           = 3.0f;
    s->combo.label_normal       = text;
    s->combo.label_hover        = text;
    s->combo.label_active       = text;
    s->combo.symbol_normal      = text_dim;
    s->combo.symbol_hover       = text;
    s->combo.symbol_active      = text;
    s->combo.content_padding    = nk_vec2(6, 4);
    s->combo.button_padding     = nk_vec2(4, 4);

    /* Tab / tree */
    s->tab.background           = nk_style_item_color(panel);
    s->tab.border_color         = border;
    s->tab.border               = 1.0f;
    s->tab.text                 = text;
    s->tab.tab_maximize_button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    s->tab.tab_maximize_button.hover  = nk_style_item_color(hover);
    s->tab.tab_maximize_button.active = nk_style_item_color(active);
    s->tab.tab_minimize_button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    s->tab.tab_minimize_button.hover  = nk_style_item_color(hover);
    s->tab.tab_minimize_button.active = nk_style_item_color(active);
    s->tab.node_maximize_button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    s->tab.node_maximize_button.hover  = nk_style_item_color(hover);
    s->tab.node_maximize_button.active = nk_style_item_color(active);
    s->tab.node_minimize_button.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    s->tab.node_minimize_button.hover  = nk_style_item_color(hover);
    s->tab.node_minimize_button.active = nk_style_item_color(active);

    /* Scrollbar */
    s->scrollh.normal           = nk_style_item_color(bg);
    s->scrollh.hover            = nk_style_item_color(bg);
    s->scrollh.active           = nk_style_item_color(bg);
    s->scrollh.cursor_normal    = nk_style_item_color(nk_rgb(60, 60, 72));
    s->scrollh.cursor_hover     = nk_style_item_color(nk_rgb(80, 80, 96));
    s->scrollh.cursor_active    = nk_style_item_color(nk_rgb(100, 100, 116));
    s->scrollh.border_color     = nk_rgba(0, 0, 0, 0);
    s->scrollh.border           = 0.0f;
    s->scrollh.rounding         = 4.0f;
    s->scrollh.rounding_cursor  = 4.0f;
    s->scrollh.border_cursor    = 0.0f;

    s->scrollv = s->scrollh;
}


/* Custom hit testing — returns which part of the window the mouse is over */
static LRESULT titlebar_hit_test(HWND hwnd, int x, int y, int titlebar_height)
{
    POINT pt = { x, y };
    ScreenToClient(hwnd, &pt);

    RECT rc;
    GetClientRect(hwnd, &rc);

    const bool maximized = IsZoomed(hwnd);
    const int border = maximized ? 0 : 6;

    /* Resize borders only when not maximized */
    if (!maximized)
    {
        if (pt.y < border)
        {
            if (pt.x < border) return HTTOPLEFT;
            if (pt.x >= rc.right - border) return HTTOPRIGHT;
            return HTTOP;
        }

        if (pt.y >= rc.bottom - border)
        {
            if (pt.x < border) return HTBOTTOMLEFT;
            if (pt.x >= rc.right - border) return HTBOTTOMRIGHT;
            return HTBOTTOM;
        }

        if (pt.x < border) return HTLEFT;
        if (pt.x >= rc.right - border) return HTRIGHT;
    }


    /* Titlebar area — draggable, enables snap/aero shake */
    if (pt.y < titlebar_height)
    {

        if (workbench_hit_test(&(get_window_data(hwnd)->workbench), {(float)pt.x, (float)pt.y}))
        {
            /* MENU BAR HIT TESTING */
            return HTCLIENT;
        }
        else 
        {
            return HTCAPTION;
        }
        
    }
        

    return HTCLIENT;
}

static void apply_dwm_frame(HWND hwnd)
{
    MARGINS margins = { 0, 0, 32, 0 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

static void render_dock(bool mouse_down, float x, float y, float w, float h)
{
    static float splitter_thickness = 2.0f;
    struct nk_color splitter_normal = nk_rgba(60, 60, 60, 0);
    struct nk_color splitter_highlight = nk_rgba(60, 60, 60, 200);

    struct nk_command_buffer *canvas = nk_window_get_canvas(&ctx);

    static float left_width = 250.0f;
    

    struct nk_rect left = nk_rect(0.0f, 32.0f + 25.0f, left_width, h);
    nk_fill_rect(canvas, left, 0.0f, nk_rgb(0, 255, 0));

    static float right_width = 250.0f;

    struct nk_rect right = nk_rect(w - right_width, 32.0f + 25.0f, right_width, h);
    nk_fill_rect(canvas, right, 0.0f, nk_rgb(0, 0, 255));

    struct nk_rect left_splitter = nk_rect(left_width, 32.0f, splitter_thickness, h);
    struct nk_color left_splitter_color = splitter_normal;
    
    if (x >= left_splitter.x && x <= left_splitter.x + left_splitter.w && y >= left_splitter.y && y <= left_splitter.y + left_splitter.h)
    {
        left_splitter_color = splitter_highlight;
        
    }

    nk_fill_rect(canvas, left_splitter, 0.0f, left_splitter_color);

    struct nk_rect right_splitter = nk_rect(w - right_width - splitter_thickness, 32.0f, splitter_thickness, h);
    struct nk_color right_splitter_color = splitter_normal;

    if (x >= right_splitter.x && x <= right_splitter.x + right_splitter.w && y >= right_splitter.y && y <= right_splitter.y + right_splitter.h)
    {
        right_splitter_color = splitter_highlight;
    }

    nk_fill_rect(canvas, right_splitter, 0.0f, right_splitter_color);
    


    static bool is_dragging = false;
    static float drag_x, drag_y = 0.0f;
    static float start_x = 0.0f;
    static float start_y = 32.0f;
    
    

    struct nk_color highlight = nk_rgba(60, 120, 215, 80);

    static struct nk_rect preview = nk_rect(start_x, start_y, 100.0f, 25.0f);
    
    if (x >= preview.x && x <= preview.x + preview.w && y >= preview.y && y <= preview.y + preview.h)
    {
        highlight = nk_rgba(60, 120, 215, 255);

        if (mouse_down && !is_dragging)
        {
            /* drag start */
            is_dragging = true;
            drag_x = x;
            drag_y = y;
        }
    }

    if (mouse_down && is_dragging)
    {
        /* drag continue */
        float dx = x - drag_x;
        float dy = y - drag_y;

        preview.x += dx;
        preview.y += dy;

        drag_x = x;
        drag_y = y;
    }

    if (!mouse_down && is_dragging)
    {
        /* drag end */
        is_dragging = false;
        drag_x = drag_y = 0.0f;
        preview = nk_rect(start_x, start_y, 100.0f, 25.0f);
    }

    nk_fill_rect(canvas, preview, 4.0f, highlight);



}
