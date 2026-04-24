/***************************************************************
**
** XPA Header File
**
** File         :  dock.h
** Module       :  dock
** Author       :  SH
** Created      :  2026-04-20 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Dock Implementation
**
***************************************************************/

#ifndef DOCK_H
#define DOCK_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include <xpa/xpa.h>

/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/



/* struct to represent single instance of a dock */
typedef struct
{   
    bool show_left;
    bool show_right;
    bool show_bottom;
    float left_width;
    float right_width;
    float bottom_width;
} dock_t;

/***************************************************************
** MARK: FUNCTION DEFS
***************************************************************/

dock_t *dock_create();
void dock_destroy(dock_t *dock);

void dock_set_frame(dock_t *dock, float x, float y, float w, float h);
void dock_input_button(dock_t *dock, xpa_button_t button, float x, float y, bool state);
void dock_input_motion(dock_t *dock, float x, float y);

#ifdef __cplusplus
}
#endif

#endif /* DOCK_H */
