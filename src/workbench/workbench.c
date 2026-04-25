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

#include <string.h>

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

static void command_callback(const char *command, void *data);

/***************************************************************
** MARK: PUBLIC FUNCTIONS
***************************************************************/

void workbench_init(workbench_t *workbench)
{
    menubar_init(&workbench->menubar, command_callback, workbench);
    statusbar_init(&workbench->statusbar);
    dock_init(&workbench->dock);
}

void workbench_set_frame(workbench_t *workbench, xpa_rect_t frame)
{
    workbench->frame = frame;

    workbench->menubar_frame = (xpa_rect_t){frame.x, frame.y, frame.width, 30.0f};
    menubar_set_frame(&workbench->menubar, workbench->menubar_frame);

    workbench->statusbar_frame = (xpa_rect_t){frame.x, frame.y + frame.height - 25.0f, frame.width, 25.0f};
    statusbar_set_frame(&workbench->statusbar, workbench->statusbar_frame);

    workbench->dock_frame = (xpa_rect_t){frame.x, frame.y + 30.0f, frame.width, frame.height - 30.0f - 25.0f};
    dock_set_frame(&workbench->dock, workbench->dock_frame);

}

void workbench_render(workbench_t *workbench)
{
    menubar_render(&workbench->menubar);
    statusbar_render(&workbench->statusbar);
    dock_render(&workbench->dock);

    //draw_rect(workbench->dock_frame, (xpa_color_t){0.1f, 1.0f, 0.1f, 1.0f});
}

bool workbench_hit_test(workbench_t *workbench, xpa_point_t point)
{
    return menubar_hit_test(&workbench->menubar, point);
}

void workbench_input_button(workbench_t *workbench, xpa_button_t button, xpa_point_t position, bool state)
{
    dock_input_button(&workbench->dock, button, position, state);
}

void workbench_input_motion(workbench_t *workbench, xpa_point_t position)
{
    dock_input_motion(&workbench->dock, position);
}

/***************************************************************
** MARK: STATIC FUNCTIONS
***************************************************************/

static void command_callback(const char *command, void *command_callback_data)
{
    printf("Menu command: %s\n", command);

    workbench_t *workbench = (workbench_t*)command_callback_data;

    if (strcmp(command, "dock.toggle_left") == 0)
    {
        dock_toggle_left(&workbench->dock);
    }
    else if (strcmp(command, "dock.toggle_right") == 0)
    {
        dock_toggle_right(&workbench->dock);
    }
    else if (strcmp(command, "dock.toggle_down") == 0)
    {
        dock_toggle_bottom(&workbench->dock);
    }
    
   
};