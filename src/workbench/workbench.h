/***************************************************************
**
** XPA Header File
**
** File         :  workbench.h
** Module       :  workbench
** Author       :  SH
** Created      :  2026-04-20 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Workbench Implementation
**
***************************************************************/

#ifndef WORKBENCH_H
#define WORKBENCH_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include <xpa/xpa.h>

#include <menubar/menubar.h>


/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/

typedef struct
{   
    menubar_t menubar;

    xpa_rect_t frame;

    xpa_rect_t menubar_frame;
    xpa_rect_t dock_frame;
} workbench_t;

/***************************************************************
** MARK: FUNCTION DEFS
***************************************************************/

void workbench_init(workbench_t *workbench);

void workbench_set_frame(workbench_t *workbench, xpa_rect_t frame);

void workbench_render(workbench_t *workbench);

bool workbench_hit_test(workbench_t *workbench, xpa_point_t point);

#ifdef __cplusplus
}
#endif

#endif /* WORKBENCH_H */
