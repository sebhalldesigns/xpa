/***************************************************************
**
** XPA Source File
**
** File         :  dock.cpp
** Module       :  dock
** Author       :  SH
** Created      :  2026-04-20 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Dock Implementation
**
***************************************************************/

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include "dock.h"

#include <draw/draw.h>

/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

#define SPLITTER_THICKNESS (2.0f)
#define MIN_SIDEBAR_SIZE (100.0f)
#define MAX_SIDEBAR_SIZE (500.0f)
#define MIN_CONTENT_SIZE (100.0f)

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/

/***************************************************************
** MARK: STATIC VARIABLES
***************************************************************/

/***************************************************************
** MARK: STATIC FUNCTION DEFS
***************************************************************/

static void update_input(dock_t *dock);

/***************************************************************
** MARK: PUBLIC FUNCTIONS
***************************************************************/

void dock_init(dock_t *dock)
{
    dock->left_width = 200.0f;
    dock->right_width = 200.0f;
    dock->bottom_height = 0.0f;
    dock->primary_down = false;
    dock->active_splitter = -1;
}

void dock_set_frame(dock_t *dock, xpa_rect_t frame)
{
    dock->frame = frame;

    dock->left_frame = {
        frame.x,
        frame.y,
        dock->left_width,
        frame.height
    };

    float left_splitter_thickness = dock->left_frame.width > 0.0f ? SPLITTER_THICKNESS : 0.0f;

    dock->left_splitter_frame = {
        frame.x + dock->left_width,
        frame.y,
        left_splitter_thickness,
        frame.height
    };

    dock->right_frame = {
        frame.x + frame.width - dock->right_width,
        frame.y,
        dock->right_width,
        frame.height
    };

    float right_splitter_thickness = dock->right_frame.width > 0.0f ? SPLITTER_THICKNESS : 0.0f;

    dock->right_splitter_frame = {
        frame.x + frame.width - dock->right_width - right_splitter_thickness,
        frame.y,
        right_splitter_thickness,
        frame.height
    };


    dock->bottom_frame = {
        frame.x + dock->left_width + left_splitter_thickness,
        frame.y + frame.height - dock->bottom_height,
        frame.width - dock->left_width - dock->right_width - left_splitter_thickness - right_splitter_thickness,
        dock->bottom_height
    };

    float bottom_splitter_thickness = dock->bottom_frame.height > 0.0f ? SPLITTER_THICKNESS : 0.0f;

    dock->bottom_splitter_frame = {
        dock->bottom_frame.x,
        frame.y + frame.height - dock->bottom_height - bottom_splitter_thickness,
        dock->bottom_frame.width,
        bottom_splitter_thickness
    };

    dock->content_frame = {
        frame.x + dock->left_width + left_splitter_thickness,
        frame.y,
        frame.width - dock->left_width - dock->right_width - left_splitter_thickness - right_splitter_thickness,
        frame.height - dock->bottom_height - bottom_splitter_thickness
    };
}

void dock_render(dock_t *dock)
{
    draw_rect(dock->left_frame, {0.8f, 0.8f, 0.8f, 1.0f});
    draw_rect(dock->right_frame, {0.8f, 0.8f, 0.8f, 1.0f});
    draw_rect(dock->bottom_frame, {0.8f, 0.8f, 0.8f, 1.0f});
    draw_rect(dock->content_frame, {0.9f, 0.9f, 0.9f, 1.0f});

    if (dock->hover_left_splitter)
    {
        draw_rect(dock->left_splitter_frame, {0.6f, 0.6f, 0.6f, 1.0f});
    }
    else
    {
        draw_rect(dock->left_splitter_frame, {0.3f, 0.3f, 0.3f, 1.0f});
    }
    
    if (dock->hover_right_splitter)
    {
        draw_rect(dock->right_splitter_frame, {0.6f, 0.6f, 0.6f, 1.0f});
    }
    else
    {
        draw_rect(dock->right_splitter_frame, {0.3f, 0.3f, 0.3f, 1.0f});
    }

    if (dock->hover_bottom_splitter)
    {
        draw_rect(dock->bottom_splitter_frame, {0.6f, 0.6f, 0.6f, 1.0f});
    }
    else
    {
        draw_rect(dock->bottom_splitter_frame, {0.3f, 0.3f, 0.3f, 1.0f});
    }
}

void dock_input_button(dock_t *dock, xpa_button_t button, xpa_point_t position, bool state)
{
    dock->mouse_position = position;

    if (button == XPA_BUTTON_PRIMARY)
    {
        dock->primary_down = state;
    }

    update_input(dock);
}

void dock_input_motion(dock_t *dock, xpa_point_t position)
{
    dock->mouse_position = position;

    update_input(dock);
}

void dock_toggle_left(dock_t *dock)
{
    printf("Toggling left dock\n");
    if (dock->left_width >= MIN_SIDEBAR_SIZE)
    {
        dock->previous_left_width = dock->left_width; /* store previous size for toggling back */
        dock->left_width = 0.0f;
    }
    else
    {
        dock->left_width = dock->previous_left_width > MIN_SIDEBAR_SIZE ? dock->previous_left_width : MIN_SIDEBAR_SIZE;    
    }

    /* update frames based on new splitter positions */
    dock_set_frame(dock, dock->frame);
}

void dock_toggle_right(dock_t *dock)
{
    if (dock->right_width >= MIN_SIDEBAR_SIZE)
    {
        dock->previous_right_width = dock->right_width; /* store previous size for toggling back */
        dock->right_width = 0.0f;
    }
    else
    {
        dock->right_width = dock->previous_right_width > MIN_SIDEBAR_SIZE ? dock->previous_right_width : MIN_SIDEBAR_SIZE;
    }

    /* update frames based on new splitter positions */
    dock_set_frame(dock, dock->frame);
}

void dock_toggle_bottom(dock_t *dock)
{
    if (dock->bottom_height >= MIN_SIDEBAR_SIZE)
    {
        dock->previous_bottom_height = dock->bottom_height; /* store previous size for toggling back */
        dock->bottom_height = 0.0f;
    }
    else
    {
        dock->bottom_height = dock->previous_bottom_height > MIN_SIDEBAR_SIZE ? dock->previous_bottom_height : MIN_SIDEBAR_SIZE;
    }

    /* update frames based on new splitter positions */
    dock_set_frame(dock, dock->frame);
}

/***************************************************************
** MARK: STATIC FUNCTIONS
***************************************************************/

static void update_input(dock_t *dock)
{
    dock->hover_left_splitter = dock->mouse_position.x >= dock->left_splitter_frame.x && dock->mouse_position.x <= dock->left_splitter_frame.x + dock->left_splitter_frame.width &&
                          dock->mouse_position.y >= dock->left_splitter_frame.y && dock->mouse_position.y <= dock->left_splitter_frame.y + dock->left_splitter_frame.height;

    dock->hover_right_splitter = dock->mouse_position.x >= dock->right_splitter_frame.x && dock->mouse_position.x <= dock->right_splitter_frame.x + dock->right_splitter_frame.width &&
                           dock->mouse_position.y >= dock->right_splitter_frame.y && dock->mouse_position.y <= dock->right_splitter_frame.y + dock->right_splitter_frame.height;
    
    dock->hover_bottom_splitter = dock->mouse_position.x >= dock->bottom_splitter_frame.x && dock->mouse_position.x <= dock->bottom_splitter_frame.x + dock->bottom_splitter_frame.width &&
                            dock->mouse_position.y >= dock->bottom_splitter_frame.y && dock->mouse_position.y <= dock->bottom_splitter_frame.y + dock->bottom_splitter_frame.height;

    if (!dock->primary_down)
    {
        dock->active_splitter = -1;
        return;
    }

    switch (dock->active_splitter)
    {
        case SPLITTER_LEFT:
        {
            float delta = dock->mouse_position.x - dock->splitter_drag_start.x;
            dock->left_width = dock->size_drag_start + delta;

            if (dock->left_width < MIN_SIDEBAR_SIZE)
            {   
                dock->left_width = MIN_SIDEBAR_SIZE;
            }
            else if (dock->left_width > MAX_SIDEBAR_SIZE)
            {
                dock->left_width = MAX_SIDEBAR_SIZE;
            }
            else if (dock->left_width > dock->frame.width - dock->right_width - MIN_CONTENT_SIZE)
            {
                dock->left_width = dock->frame.width - dock->right_width - MIN_CONTENT_SIZE;
            }
        } break;

        case SPLITTER_RIGHT:
        {
            float delta = dock->splitter_drag_start.x - dock->mouse_position.x;
            dock->right_width = dock->size_drag_start + delta;

            if (dock->right_width < MIN_SIDEBAR_SIZE)
            {
                dock->right_width = MIN_SIDEBAR_SIZE;
            }
            else if (dock->right_width > MAX_SIDEBAR_SIZE)
            {
                dock->right_width = MAX_SIDEBAR_SIZE;
            }
            else if (dock->right_width > dock->frame.width - dock->left_width - MIN_CONTENT_SIZE)
            {
                dock->right_width = dock->frame.width - dock->left_width - MIN_CONTENT_SIZE;
            }
        } break;

        case SPLITTER_BOTTOM:
        {
            float delta = dock->splitter_drag_start.y - dock->mouse_position.y;
            dock->bottom_height = dock->size_drag_start + delta;

            if (dock->bottom_height < MIN_SIDEBAR_SIZE)
            {
                dock->bottom_height = MIN_CONTENT_SIZE;
            }
            else if (dock->bottom_height > dock->frame.height - MIN_CONTENT_SIZE)
            {
                dock->bottom_height = dock->frame.height - MIN_CONTENT_SIZE;
            }
        } break;


        default:
        {
            /* i.e none */
            if (dock->primary_down)
            {
                if (dock->hover_left_splitter)
                {
                    dock->active_splitter = SPLITTER_LEFT;
                    dock->splitter_drag_start = dock->mouse_position;
                    dock->size_drag_start = dock->left_width;
                }
                else if (dock->hover_right_splitter)
                {
                    dock->active_splitter = SPLITTER_RIGHT;
                    dock->splitter_drag_start = dock->mouse_position;
                    dock->size_drag_start = dock->right_width;
                }
                else if (dock->hover_bottom_splitter)
                {
                    dock->active_splitter = SPLITTER_BOTTOM;
                    dock->splitter_drag_start = dock->mouse_position;
                    dock->size_drag_start = dock->bottom_height;
                }
            }
        } break;
    }

    /* update frames based on new splitter positions */
    dock_set_frame(dock, dock->frame);
}