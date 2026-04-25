/***************************************************************
**
** XPA Source File
**
** File         :  workbench.c
** Module       :  workbench
** Author       :  SH
** Created      :  2026-04-20 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Workbench Implementation
**
***************************************************************/

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include "workbench.h"
#include <draw/draw.h>

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

void workbench_init(workbench_t *workbench)
{
    menubar_init(&workbench->menubar);
}

void workbench_set_frame(workbench_t *workbench, xpa_rect_t frame)
{
    workbench->frame = frame;
    workbench->menubar_frame = (xpa_rect_t){frame.x, frame.y, frame.width, 30.0f};
    workbench->dock_frame = (xpa_rect_t){frame.x, frame.y + 30.0f, frame.width, frame.height - 30.0f};
    menubar_set_frame(&workbench->menubar, workbench->menubar_frame);
}

void workbench_render(workbench_t *workbench)
{
    menubar_render(&workbench->menubar);

    draw_rect(workbench->dock_frame, (xpa_color_t){0.1f, 1.0f, 0.1f, 1.0f});
}

bool workbench_hit_test(workbench_t *workbench, xpa_point_t point)
{
    return menubar_hit_test(&workbench->menubar, point);
}

/***************************************************************
** MARK: STATIC FUNCTIONS
***************************************************************/
