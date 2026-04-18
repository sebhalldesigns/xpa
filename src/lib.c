/***************************************************************
**
** XPA Source File
**
** File         :  lib.c
** Module       :  root
** Author       :  SH
** Created      :  2026-04-18 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Root Interface
**
***************************************************************/

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include <xpa/xpa.h>
#include <xpa_rust.h>

#include <stdio.h>
#include <time.h>

/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/

/***************************************************************
** MARK: STATIC VARIABLES
***************************************************************/

static XpaGpuContext* gpu_ctx = NULL;

static double xpa_now_ms(void)
{
    return ((double)clock() * 1000.0) / (double)CLOCKS_PER_SEC;
}

/***************************************************************
** MARK: STATIC FUNCTION DEFS
***************************************************************/

/***************************************************************
** MARK: PUBLIC FUNCTIONS
***************************************************************/

bool xpa_init(void)
{
    double start_ms = xpa_now_ms();
    printf("Initializing xpa_rust from C...\n");
    
    if (!xpa_backend_init() || !xpa_rust_init())
    {
        return false;
    }

    printf("[xpa] xpa_init done at +%.2f ms\n", xpa_now_ms() - start_ms);

    return true;
}

bool xpa_create_window(const char *title, uint32_t width, uint32_t height, xpa_window_t *window)
{
    return xpa_backend_create_window(title, width, height, window);
}

int xpa_run(void)
{
    return xpa_backend_run();
}

XpaGpuContext* xpa_get_gpu_context(void)
{
    return gpu_ctx;
}

bool xpa_create_surface_for_window(void* native_handle, uint32_t width, uint32_t height, XpaGpuSurface** out_surface)
{
    double start_ms = xpa_now_ms();

    if (!out_surface)
    {
        return false;
    }

    if (!gpu_ctx)
    {
        gpu_ctx = xpa_gpu_context_create_with_surface(native_handle, width, height, out_surface);

        if (!gpu_ctx || !*out_surface)
        {
            fprintf(stderr, "Failed to create GPU context.\n");
            gpu_ctx = NULL;
            return false;
        }

        printf("[xpa] first window gpu bootstrap done in %.2f ms\n", xpa_now_ms() - start_ms);
        return true;
    }

    *out_surface = xpa_gpu_surface_create(gpu_ctx, native_handle, width, height);
    printf("[xpa] additional window surface create done in %.2f ms\n", xpa_now_ms() - start_ms);
    return *out_surface != NULL;
}

/***************************************************************
** MARK: STATIC FUNCTIONS
***************************************************************/
