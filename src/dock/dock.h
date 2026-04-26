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


typedef struct tab_t
{
    xpa_rect_t frame;

    const char *title;

    struct tab_t *next;
} tab_t;

typedef struct tab_group_t
{
    xpa_rect_t frame;

    bool is_tool; 

    tab_t *tabs;
    tab_t *active_tab;
} tab_group_t;


typedef struct dock_node_t
{
    xpa_rect_t frame;

    struct dock_node_t *parent;

    bool is_leaf;
    
    union {
        struct
        {
            bool vertical_split;
            float split_ratio; /* 0.0 to 1.0 */
            struct dock_node_t *first;
            struct dock_node_t *second;
        } split;

        struct
        {
            tab_group_t *group;
        } leaf;
    };
} dock_node_t;


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

    dock_node_t *root_document;
    dock_node_t *hover_document_split;
    dock_node_t *active_document_split;
    xpa_rect_t hover_document_splitter_frame;
    xpa_point_t document_split_drag_start;
    float document_split_ratio_start;

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
void dock_reset_document(dock_t *dock);

void dock_set_frame(dock_t *dock, xpa_rect_t frame);
void dock_layout(dock_t *dock);
void dock_render(dock_t *dock);

void dock_input_button(dock_t *dock, xpa_button_t button, xpa_point_t position, bool state);
void dock_input_motion(dock_t *dock, xpa_point_t position);

void dock_toggle_left(dock_t *dock);
void dock_toggle_right(dock_t *dock);
void dock_toggle_bottom(dock_t *dock);

tab_group_t *dock_group_create(bool is_tool);
void dock_group_destroy(tab_group_t *group);
tab_t *dock_group_add_tab(tab_group_t *group, const char *title);
bool dock_group_remove_tab(tab_group_t *group, tab_t *tab);

dock_node_t *dock_node_create_leaf(tab_group_t *group);
dock_node_t *dock_node_create_split(bool vertical_split, float split_ratio, dock_node_t *first, dock_node_t *second);
void dock_node_destroy(dock_node_t *node);
bool dock_node_insert_split(dock_t *dock, dock_node_t *target_leaf, bool vertical_split, float split_ratio, tab_group_t *new_group, bool place_new_first);
bool dock_node_remove_leaf(dock_t *dock, dock_node_t *leaf_node, bool destroy_group);

#ifdef __cplusplus
}
#endif

#endif /* DOCK_H */
