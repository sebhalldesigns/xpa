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
#include <backend/backend.h>

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
    printf("Initializing xpa\n");
    
    if (!xpa_backend_init())
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

/***************************************************************
** MARK: STATIC FUNCTIONS
***************************************************************/
