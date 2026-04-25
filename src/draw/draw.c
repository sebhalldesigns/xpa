/***************************************************************
**
** XPA Source File
**
** File         :  draw.c
** Module       :  draw
** Author       :  SH
** Created      :  2026-04-24 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Drawing Implementation
**
***************************************************************/

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include "draw.h"

/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/

/***************************************************************
** MARK: STATIC VARIABLES
***************************************************************/

static struct nk_context *nk_ctx = NULL;
static struct nk_command_buffer *canvas = NULL;

/***************************************************************
** MARK: STATIC FUNCTION DEFS
***************************************************************/

/***************************************************************
** MARK: PUBLIC FUNCTIONS
***************************************************************/

void draw_set_context(struct nk_context *nk_context)
{
    nk_ctx = nk_context;
    canvas = nk_window_get_canvas(nk_ctx);
}

void draw_rect(xpa_rect_t frame, xpa_color_t color)
{
    nk_fill_rect(canvas, *(struct nk_rect*)&frame, 0.0f, nk_rgba_f(color.red, color.green, color.blue, color.alpha));
}

/***************************************************************
** MARK: STATIC FUNCTIONS
***************************************************************/
