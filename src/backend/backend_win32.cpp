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

#include "../router/router.h"

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <windowsx.h>

#include <stdio.h>
#include <time.h>

#include <string>
#include <vector>
#include <algorithm>
#include <wrl.h>
#include <wil/com.h>
#include <shlwapi.h> 
#include <urlmon.h>
#include <shobjidl.h> 
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <dwmapi.h>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;


/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/

typedef struct {
    HWND         hwnd;
    HWND         webview_hwnd;
    XpaRenderer*  renderer;
    uint32_t      pending_width;
    uint32_t      pending_height;
    bool          needs_resize;
} xpa_window_internal_t;

/***************************************************************
** MARK: STATIC VARIABLES
***************************************************************/

static HINSTANCE instance_handle;
static WNDCLASSW window_class;
static bool running = true;
static std::vector<xpa_window_internal_t*> windows;

static ICoreWebView2Controller* g_controller = nullptr;
static ICoreWebView2 *webview = NULL;
static ICoreWebView2Environment* webview_environment = NULL;

static WNDPROC original_overlay_window_procedure;
static HHOOK mouse_hook = NULL;

static bool g_mouse_in_ui = false;



/***************************************************************
** MARK: STATIC FUNCTION DEFS
***************************************************************/

static LRESULT CALLBACK window_procedure(HWND window, UINT msg, WPARAM wparam, LPARAM lparam);
static LRESULT CALLBACK overlay_window_procedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK low_level_mouse_window_procedure(int nCode, WPARAM wParam, LPARAM lParam);

static xpa_window_internal_t* get_window_data(HWND hwnd);

static std::string wide_to_utf8(const std::wstring& w);
static std::wstring utf8_to_wide(const std::string& s);
static std::wstring uri_path_only(const std::wstring& fullUri);

static HRESULT MakeStreamFromBytes(const void* data, size_t length, IStream** streamOut);
static void SetResponseHeaders(std::wstring& headers, const char* mime_type, size_t length);

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
    xpa_window_internal_t* data = new xpa_window_internal_t();
    data->hwnd = win32_window;
    data->renderer = NULL;
    data->pending_width = width;
    data->pending_height = height;
    data->needs_resize = false;

    data->renderer = xpa_renderer_init((void*)win32_window, width, height);
    if (!data->renderer)
    {
        delete data;
        DestroyWindow(win32_window);
        return false;
    }

    printf("[xpa] renderer ready at +%.2f ms\n", xpa_now_ms() - start_ms);

    SetWindowLongPtr(win32_window, GWLP_USERDATA, (LONG_PTR)data);
    windows.push_back(data);

    data->webview_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        L"Static",
        L"WebView Overlay",
        WS_VISIBLE | WS_POPUP, /* not WM_CHILD for now - can't be hidden for webview creation */
        0, 0, 1, 1, /* 1x1 pixels for now */
        data->hwnd, /* window as parent */
        NULL, 
        instance_handle,
        NULL
    );

    ShowWindow(data->webview_hwnd, SW_SHOW);
    UpdateWindow(data->webview_hwnd);

    original_overlay_window_procedure = (WNDPROC)SetWindowLongPtr(data->webview_hwnd, GWLP_WNDPROC, (LONG_PTR)overlay_window_procedure);

    mouse_hook = SetWindowsHookEx(WH_MOUSE_LL, low_level_mouse_window_procedure, instance_handle, 0);

    SetLayeredWindowAttributes(data->webview_hwnd, RGB(0,0,0), 0, LWA_COLORKEY);

    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    options->put_AdditionalBrowserArguments(
        L"--allow-file-access-from-files --disable-web-security"
    );

    const WCHAR* allowedOrigins[1] = {L"*"};

    auto customSchemeRegistration =
        Microsoft::WRL::Make<CoreWebView2CustomSchemeRegistration>(L"app");
    customSchemeRegistration->put_TreatAsSecure(TRUE);
    customSchemeRegistration->SetAllowedOrigins(1, allowedOrigins);
    customSchemeRegistration->put_HasAuthorityComponent(TRUE);
   
    ICoreWebView2CustomSchemeRegistration* registrations[1] = {
        customSchemeRegistration.Get()};
    options->SetCustomSchemeRegistrations(
        1, static_cast<ICoreWebView2CustomSchemeRegistration**>(registrations));

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [data](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                printf("Environment created: 0x%08X\n", result);

                if (FAILED(result))
                {
                    printf("Failed to create WebView2 environment: 0x%08X\n", result);
                    return result;
                }

                webview_environment = env;

                env->CreateCoreWebView2Controller(
                    data->webview_hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [data](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT
                        {
                            printf("Controller created: 0x%08X\n", result);
                            
                            controller->get_CoreWebView2(&webview);
        
                            // Enable transparency
                            ICoreWebView2Controller2* controller2;
                            controller->QueryInterface(IID_PPV_ARGS(&controller2));
                            COREWEBVIEW2_COLOR transparent = {0, 0, 0, 0};
                            controller2->put_DefaultBackgroundColor(transparent);

                            g_controller = controller;

                            // Make sure the host gets focus
                            SetFocus(data->webview_hwnd);

                            // And push focus into the WebView2 content:
                            controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);

                            ICoreWebView2Settings* settings = nullptr;
                            webview->get_Settings(&settings);


                            /* Intercept all requests; we'll only respond to https://app.local/* */
                            webview->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

                            webview->add_WebResourceRequested(
                                Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                                    [data](ICoreWebView2* sender, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {

                                        printf("Web resource requested\n");

                                        
                                        ComPtr<ICoreWebView2WebResourceRequest> req;
                                        args->get_Request(&req);

                                        LPWSTR uriLPW = nullptr;
                                        req->get_Uri(&uriLPW);

                                        std::wstring full = uriLPW ? uriLPW : L"";
                                        if (uriLPW) CoTaskMemFree(uriLPW);

                                        std::wstring path = uri_path_only(full);
                                        std::string request = wide_to_utf8(path);


                                        printf("%s\n", request.c_str());

                                        const char *data;
                                        size_t length;
                                        const char* mime_type;
                                        
                                        if (router_resolve(request.c_str(), &data, &length, &mime_type))
                                        {   
                                            ComPtr<IStream> body;
                                            HRESULT hr = MakeStreamFromBytes(data, length, &body);
                                            if (FAILED(hr)) return hr;

                                            std::wstring headers;
                                            SetResponseHeaders(headers, mime_type, length);

                                            ComPtr<ICoreWebView2WebResourceResponse> resp;
                                            hr = webview_environment->CreateWebResourceResponse(
                                                body.Get(),
                                                200,
                                                L"OK",
                                                headers.c_str(),
                                                &resp
                                            );
                                            if (FAILED(hr)) return hr;

                                            return args->put_Response(resp.Get());
                                        }   
                                        else
                                        {
                                            // 404 body (optional but nice)
                                            const char notFoundHtml[] =
                                                "<!doctype html><meta charset='utf-8'>"
                                                "<title>404</title><h1>Not found</h1>";

                                            ComPtr<IStream> body;
                                            HRESULT hr = MakeStreamFromBytes(notFoundHtml, sizeof(notFoundHtml) - 1, &body);
                                            if (FAILED(hr)) return hr;

                                            std::wstring headers;
                                            SetResponseHeaders(headers, "text/html; charset=utf-8", sizeof(notFoundHtml) - 1);

                                            ComPtr<ICoreWebView2WebResourceResponse> resp;
                                            hr = webview_environment->CreateWebResourceResponse(
                                                body.Get(),
                                                404,
                                                L"Not Found",
                                                headers.c_str(),
                                                &resp
                                            );
                                            if (FAILED(hr)) return hr;

                                            return args->put_Response(resp.Get());
                                        }

                                        

                                        return S_OK;
                                    
                                    }).Get(),
                                nullptr);

                            xpa_backend_webview_load_url("app://app/index.html");

                            xpa_app_init();
                            InvalidateRect(data->hwnd, NULL, FALSE);

                            SetParent(data->webview_hwnd, data->hwnd);
                            SetWindowLongPtr(data->webview_hwnd, GWL_STYLE, WS_CHILD | WS_VISIBLE);

                            ShowWindow(data->hwnd, SW_SHOW);
                            UpdateWindow(data->hwnd);
                            
                
                            
                            webview->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [](ICoreWebView2* sender,
                                    ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
                                    {
                                        wil::unique_cotaskmem_string message;
                                        args->TryGetWebMessageAsString(&message);

                                        // message is UTF-16 JSON
                                        std::wstring message_str = message.get();

                                        std::string message_utf8 = wide_to_utf8(message_str);

                                        printf("Received message from WebView: %s\n", message_utf8.c_str());
                                        return S_OK;
                                    }
                                ).Get(),
                                nullptr
                            );
                            
                            // Position over OpenGL window
                            RECT bounds;
                            GetClientRect(data->webview_hwnd, &bounds);
                            controller->put_Bounds(bounds);

                            return S_OK;
                        }
                    ).Get()
                );

                return S_OK;
            }).Get());




    printf("[xpa] window shown at +%.2f ms\n", xpa_now_ms() - start_ms);

    *window = (xpa_window_t)win32_window;

    return true;
}

void xpa_backend_webview_load_url(char *url)
{
    printf("loading %s\n", url);
    
    size_t len = strlen(url) + 1;
    wchar_t *wstr = new wchar_t[len];
    mbstowcs(wstr, url, len);

    std::wstring wide(wstr);

    webview->Navigate(wide.c_str()); 
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

            if (data && data->renderer && wparam != SIZE_MINIMIZED)
            {
               
                if (width > 0 && height > 0)
                {
                    data->pending_width = width;
                    data->pending_height = height;
                    data->needs_resize = true;
                    InvalidateRect(window, NULL, FALSE);

                    SetWindowPos(data->webview_hwnd, HWND_TOP, 0, 0, width, height, SWP_SHOWWINDOW);

                    if (g_controller)
                    {
                        RECT bounds = {0, 0, width, height};
                        g_controller->put_Bounds(bounds);
                    }
                }
            }

            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(window, &ps);

            if (data && data->renderer)
            {
                if (data->needs_resize)
                {
                    xpa_renderer_resize(data->renderer, data->pending_width, data->pending_height);
                    data->needs_resize = false;
                }

                xpa_renderer_render(data->renderer);
            }

            

            EndPaint(window, &ps);


            return 0;
        }

        case WM_NCDESTROY:
        {
            if (data)
            {
                SetWindowLongPtr(window, GWLP_USERDATA, 0);

                if (data->renderer)
                {
                    xpa_renderer_destroy(data->renderer);
                    data->renderer = NULL;
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


static LRESULT CALLBACK overlay_window_procedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_DESTROY:
        {
            if (mouse_hook)
            {
                UnhookWindowsHookEx(mouse_hook);
                mouse_hook = NULL;
            }
        } break;
    }

    return CallWindowProc(original_overlay_window_procedure, hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK low_level_mouse_window_procedure(int nCode, WPARAM wParam, LPARAM lParam)
{
    #if 0
    if (nCode == HC_ACTION && wParam == WM_MOUSEMOVE)
    {
        MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;

        if (webview_window && window)
        {
            POINT point = pMouseStruct->pt;
            ScreenToClient(window, &point);

            bool in_ui = false;

            const int caption_bottom = get_caption_area_height();

            if (point.y < caption_bottom)
            {
                // --- CAPTION AREA ---
                // The top resize border must always pass through to the
                // parent (transparent). For the rest of the caption, use
                // app_hit_test_webui which checks the draggable regions
                // sent from JavaScript.

                const UINT dpi = GetDpiForWindow(window);
                const int border = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi)
                                 + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

                if (point.y < border)
                {
                    // Top resize border — always pass through
                    in_ui = false;
                }
                else
                {
                    // Caption bar region — use app_hit_test_webui which
                    // checks the titlebar draggable regions from JavaScript.
                    // Returns false for draggable areas, true for interactive UI.
                    in_ui = app_hit_test_webui((float)point.x, (float)point.y);
                }
            }
            else
            {
                // --- CONTENT AREA (below caption) ---
                in_ui = app_hit_test_webui((float)point.x, (float)point.y);
            }

            if (in_ui != g_mouse_in_ui)
            {
                g_mouse_in_ui = in_ui;

                LONG_PTR exStyle = GetWindowLongPtr(webview_window, GWL_EXSTYLE);

                if (in_ui)
                {
                    exStyle &= ~WS_EX_TRANSPARENT;
                }
                else
                {
                    exStyle |= WS_EX_TRANSPARENT;
                }

                SetWindowLongPtr(webview_window, GWL_EXSTYLE, exStyle);
            }
        }
    }
    #endif

    return CallNextHookEx(mouse_hook, nCode, wParam, lParam);
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

static std::wstring uri_path_only(const std::wstring& fullUri)
{
    Microsoft::WRL::ComPtr<IUri> uri;
    HRESULT hr = CreateUri(fullUri.c_str(), Uri_CREATE_CANONICALIZE, 0, &uri);
    if (FAILED(hr) || !uri) return L"";

    BSTR pathBstr = nullptr;
    hr = uri->GetPath(&pathBstr);           // returns path only (starts with '/')
    if (FAILED(hr) || !pathBstr) return L"";

    std::wstring path(pathBstr, SysStringLen(pathBstr));
    SysFreeString(pathBstr);
    return path;
}


static HRESULT MakeStreamFromBytes(const void* data, size_t length, IStream** streamOut)
{
    if (!streamOut) return E_POINTER;
    *streamOut = nullptr;

    // Allocate movable global memory and wrap as an IStream
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, length ? length : 1);
    if (!hMem) return E_OUTOFMEMORY;

    void* dst = GlobalLock(hMem);
    if (!dst) { GlobalFree(hMem); return E_FAIL; }

    if (length) memcpy(dst, data, length);
    GlobalUnlock(hMem);

    ComPtr<IStream> stream;
    HRESULT hr = CreateStreamOnHGlobal(hMem, TRUE /* fDeleteOnRelease */, &stream);
    if (FAILED(hr)) {
        GlobalFree(hMem); // only if stream didn't take ownership
        return hr;
    }

    *streamOut = stream.Detach();
    return S_OK;
}

static void SetResponseHeaders(std::wstring& headers, const char* mime_type, size_t length)
{
    // WebView2 expects a single header string with CRLF line endings.
    // Keep it minimal.
    headers = L"Content-Type: " + utf8_to_wide(mime_type ? mime_type : "application/octet-stream") + L"\r\n";
    headers += L"Content-Length: " + std::to_wstring(length) + L"\r\n";
    headers += L"Cache-Control: no-cache\r\n";
}
