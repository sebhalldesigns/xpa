/***************************************************************
**
** XPA Source File
**
** File         :  statusbar.c
** Module       :  statusbar
** Author       :  SH
** Created      :  2026-04-25 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Status Bar Implementation
**
***************************************************************/

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include "statusbar.h"

#include <draw/draw.h>
#include <control/control.h>

#include <stdlib.h>

/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/

/***************************************************************
** MARK: STATIC VARIABLES
***************************************************************/


/***************************************************************
** MARK: STATIC FUNCTION DEFS
***************************************************************/

/***************************************************************
** MARK: PUBLIC FUNCTIONS
***************************************************************/


void statusbar_init(statusbar_t *statusbar)
{
}

void statusbar_set_frame(statusbar_t *statusbar, xpa_rect_t frame)
{
    statusbar->frame = frame;

    statusbar->label_frame = (xpa_rect_t){
        frame.x + 10.0f,
        frame.y,
        100.0f,
        frame.height
    };
}

void statusbar_render(statusbar_t *statusbar)
{
    control_label("Status: Ready", statusbar->label_frame, 0);
}

/***************************************************************
** MARK: STATIC FUNCTIONS
***************************************************************/
