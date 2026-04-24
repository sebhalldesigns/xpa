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
    xpa_frame_t frame;
    menubar_t menubar;
} workbench_t;

/***************************************************************
** MARK: FUNCTION DEFS
***************************************************************/

void workbench_init(workbench_t *workbench);

void workbench_set_frame(workbench_t *workbench, xpa_frame_t frame);

void workbench_render(workbench_t *workbench);

#ifdef __cplusplus
}
#endif

#endif /* WORKBENCH_H */
