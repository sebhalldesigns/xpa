/***************************************************************
**
** XPA Source File
**
** File         :  control.c
** Module       :  control
** Author       :  SH
** Created      :  2026-04-24 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Control Implementation
**
***************************************************************/

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include "control.h"

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

/***************************************************************
** MARK: STATIC FUNCTION DEFS
***************************************************************/

/***************************************************************
** MARK: PUBLIC FUNCTIONS
***************************************************************/

void control_set_context(struct nk_context *nk_context)
{
    nk_ctx = nk_context;
}

void control_button(const char *label, xpa_frame_t *frame)
{
    nk_layout_space_push(nk_ctx, nk_rect(frame->x, frame->y, frame->width, frame->height));

    if (nk_button_label(nk_ctx, label))
    {
        // Button was clicked
        printf("Button '%s' clicked at frame (%.2f, %.2f, %.2f, %.2f)\n", label, frame->x, frame->y, frame->width, frame->height);
    }
}


/***************************************************************
** MARK: STATIC FUNCTIONS
***************************************************************/
