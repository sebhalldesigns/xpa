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

#define SPLITTER_LEFT (0)
#define SPLITTER_RIGHT (1)
#define SPLITTER_BOTTOM (2)

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/



/* struct to represent single instance of a dock */
typedef struct
{   
    xpa_rect_t frame;

    xpa_rect_t left_frame;
    xpa_rect_t left_splitter_frame;
    xpa_rect_t right_frame;
    xpa_rect_t right_splitter_frame;
    xpa_rect_t bottom_frame;
    xpa_rect_t bottom_splitter_frame;
    xpa_rect_t content_frame;

    float left_width;
    float right_width;
    float bottom_height;

    float previous_left_width;
    float previous_right_width;
    float previous_bottom_height;

    xpa_point_t mouse_position;
    bool primary_down;

    bool hover_left_splitter;
    bool hover_right_splitter;
    bool hover_bottom_splitter;

    int active_splitter; /* -1 for none, 0 for left, 1 for right, 2 for bottom */
    xpa_point_t splitter_drag_start;
    float size_drag_start;
} dock_t;

/***************************************************************
** MARK: FUNCTION DEFS
***************************************************************/

void dock_init(dock_t *dock);

void dock_set_frame(dock_t *dock, xpa_rect_t frame);
void dock_render(dock_t *dock);

void dock_input_button(dock_t *dock, xpa_button_t button, xpa_point_t position, bool state);
void dock_input_motion(dock_t *dock, xpa_point_t position);

void dock_toggle_left(dock_t *dock);
void dock_toggle_right(dock_t *dock);
void dock_toggle_bottom(dock_t *dock);

#ifdef __cplusplus
}
#endif

#endif /* DOCK_H */
