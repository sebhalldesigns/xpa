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

#include <stdlib.h>
#include <string.h>

/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

#define SPLITTER_THICKNESS (2.0f)
#define MIN_SIDEBAR_SIZE (100.0f)
#define MAX_SIDEBAR_SIZE (500.0f)
#define MIN_CONTENT_SIZE (100.0f)
#define MIN_PANE_SIZE (80.0f)
#define TAB_BAR_HEIGHT (28.0f)
#define TAB_MIN_WIDTH (120.0f)

/***************************************************************
** MARK: STATIC FUNCTION DEFS
***************************************************************/

static void update_input(dock_t *dock);
static float clampf(float value, float minimum, float maximum);
static float clamp_ratio(float ratio);
static void free_tab_list(tab_t *head);
static void layout_tabs(tab_group_t *group);
static float split_available_size(const dock_node_t *node);
static float split_first_size(const dock_node_t *node);
static xpa_rect_t split_frame(const dock_node_t *node);
static bool point_in_rect(xpa_point_t point, xpa_rect_t rect);
static bool find_hovered_split(dock_node_t *node, xpa_point_t point, dock_node_t **hovered_node, xpa_rect_t *splitter_frame);
static xpa_color_t group_color(const tab_group_t *group);
static void render_node(dock_t *dock, dock_node_t *node);
static void layout_node(dock_node_t *node, xpa_rect_t frame);

/***************************************************************
** MARK: PUBLIC FUNCTIONS
***************************************************************/

void dock_init(dock_t *dock)
{
    tab_group_t *primary_group = NULL;
    tab_group_t *secondary_group = NULL;
    tab_group_t *tool_group = NULL;
    dock_node_t *right_leaf = NULL;

    memset(dock, 0, sizeof(*dock));

    dock->left_width = 200.0f;
    dock->right_width = 200.0f;
    dock->bottom_height = 0.0f;

    dock->previous_left_width = dock->left_width;
    dock->previous_right_width = dock->right_width;
    dock->previous_bottom_height = 180.0f;

    dock->active_splitter = -1;

    primary_group = dock_group_create(false);
    if (primary_group == NULL)
    {
        return;
    }
    dock_group_add_tab(primary_group, "main.c");
    dock_group_add_tab(primary_group, "README.md");
    dock_group_add_tab(primary_group, "dock.c");

    dock->root_document = dock_node_create_leaf(primary_group);
    if (dock->root_document == NULL)
    {
        dock_group_destroy(primary_group);
        return;
    }

    secondary_group = dock_group_create(false);
    if (secondary_group != NULL)
    {
        dock_group_add_tab(secondary_group, "Properties");
        dock_group_add_tab(secondary_group, "Search");
        if (!dock_node_insert_split(dock, dock->root_document, true, 0.70f, secondary_group, false))
        {
            dock_group_destroy(secondary_group);
        }
    }

    if (dock->root_document != NULL && !dock->root_document->is_leaf)
    {
        right_leaf = dock->root_document->split.second;
    }

    if (right_leaf != NULL)
    {
        tool_group = dock_group_create(true);
        if (tool_group != NULL)
        {
            dock_group_add_tab(tool_group, "Terminal");
            dock_group_add_tab(tool_group, "Problems");
            if (!dock_node_insert_split(dock, right_leaf, false, 0.65f, tool_group, false))
            {
                dock_group_destroy(tool_group);
            }
        }
    }
}

void dock_reset_document(dock_t *dock)
{
    dock_node_destroy(dock->root_document);
    dock->root_document = NULL;
}

void dock_set_frame(dock_t *dock, xpa_rect_t frame)
{
    float left_splitter_thickness = 0.0f;
    float right_splitter_thickness = 0.0f;
    float bottom_splitter_thickness = 0.0f;
    float content_width = 0.0f;
    float content_height = 0.0f;

    dock->frame = frame;

    dock->left_frame = (xpa_rect_t){
        frame.x,
        frame.y,
        dock->left_width,
        frame.height
    };

    left_splitter_thickness = dock->left_frame.width > 0.0f ? SPLITTER_THICKNESS : 0.0f;

    dock->left_splitter_frame = (xpa_rect_t){
        frame.x + dock->left_width,
        frame.y,
        left_splitter_thickness,
        frame.height
    };

    dock->right_frame = (xpa_rect_t){
        frame.x + frame.width - dock->right_width,
        frame.y,
        dock->right_width,
        frame.height
    };

    right_splitter_thickness = dock->right_frame.width > 0.0f ? SPLITTER_THICKNESS : 0.0f;

    dock->right_splitter_frame = (xpa_rect_t){
        frame.x + frame.width - dock->right_width - right_splitter_thickness,
        frame.y,
        right_splitter_thickness,
        frame.height
    };

    content_width = frame.width - dock->left_width - dock->right_width - left_splitter_thickness - right_splitter_thickness;
    if (content_width < 0.0f)
    {
        content_width = 0.0f;
    }

    dock->bottom_frame = (xpa_rect_t){
        frame.x + dock->left_width + left_splitter_thickness,
        frame.y + frame.height - dock->bottom_height,
        content_width,
        dock->bottom_height
    };

    bottom_splitter_thickness = dock->bottom_frame.height > 0.0f ? SPLITTER_THICKNESS : 0.0f;

    dock->bottom_splitter_frame = (xpa_rect_t){
        dock->bottom_frame.x,
        frame.y + frame.height - dock->bottom_height - bottom_splitter_thickness,
        dock->bottom_frame.width,
        bottom_splitter_thickness
    };

    content_height = frame.height - dock->bottom_height - bottom_splitter_thickness;
    if (content_height < 0.0f)
    {
        content_height = 0.0f;
    }

    dock->content_frame = (xpa_rect_t){
        frame.x + dock->left_width + left_splitter_thickness,
        frame.y,
        content_width,
        content_height
    };

    dock_layout(dock);
}

void dock_layout(dock_t *dock)
{
    if (dock->root_document == NULL)
    {
        return;
    }

    layout_node(dock->root_document, dock->content_frame);
}

void dock_render(dock_t *dock)
{
    draw_rect(dock->left_frame, (xpa_color_t){0.8f, 0.8f, 0.8f, 1.0f});
    draw_rect(dock->right_frame, (xpa_color_t){0.8f, 0.8f, 0.8f, 1.0f});
    draw_rect(dock->bottom_frame, (xpa_color_t){0.8f, 0.8f, 0.8f, 1.0f});
    draw_rect(dock->content_frame, (xpa_color_t){0.9f, 0.9f, 0.9f, 1.0f});

    render_node(dock, dock->root_document);

    if (dock->hover_left_splitter)
    {
        draw_rect(dock->left_splitter_frame, (xpa_color_t){0.6f, 0.6f, 0.6f, 1.0f});
    }
    else
    {
        draw_rect(dock->left_splitter_frame, (xpa_color_t){0.3f, 0.3f, 0.3f, 1.0f});
    }

    if (dock->hover_right_splitter)
    {
        draw_rect(dock->right_splitter_frame, (xpa_color_t){0.6f, 0.6f, 0.6f, 1.0f});
    }
    else
    {
        draw_rect(dock->right_splitter_frame, (xpa_color_t){0.3f, 0.3f, 0.3f, 1.0f});
    }

    if (dock->hover_bottom_splitter)
    {
        draw_rect(dock->bottom_splitter_frame, (xpa_color_t){0.6f, 0.6f, 0.6f, 1.0f});
    }
    else
    {
        draw_rect(dock->bottom_splitter_frame, (xpa_color_t){0.3f, 0.3f, 0.3f, 1.0f});
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
    if (dock->left_width >= MIN_SIDEBAR_SIZE)
    {
        dock->previous_left_width = dock->left_width;
        dock->left_width = 0.0f;
    }
    else
    {
        dock->left_width = dock->previous_left_width > MIN_SIDEBAR_SIZE ? dock->previous_left_width : MIN_SIDEBAR_SIZE;
    }

    dock_set_frame(dock, dock->frame);
}

void dock_toggle_right(dock_t *dock)
{
    if (dock->right_width >= MIN_SIDEBAR_SIZE)
    {
        dock->previous_right_width = dock->right_width;
        dock->right_width = 0.0f;
    }
    else
    {
        dock->right_width = dock->previous_right_width > MIN_SIDEBAR_SIZE ? dock->previous_right_width : MIN_SIDEBAR_SIZE;
    }

    dock_set_frame(dock, dock->frame);
}

void dock_toggle_bottom(dock_t *dock)
{
    if (dock->bottom_height >= MIN_SIDEBAR_SIZE)
    {
        dock->previous_bottom_height = dock->bottom_height;
        dock->bottom_height = 0.0f;
    }
    else
    {
        dock->bottom_height = dock->previous_bottom_height > MIN_SIDEBAR_SIZE ? dock->previous_bottom_height : MIN_SIDEBAR_SIZE;
    }

    dock_set_frame(dock, dock->frame);
}

tab_group_t *dock_group_create(bool is_tool)
{
    tab_group_t *group = (tab_group_t *)calloc(1, sizeof(*group));
    if (group == NULL)
    {
        return NULL;
    }

    group->is_tool = is_tool;
    return group;
}

void dock_group_destroy(tab_group_t *group)
{
    if (group == NULL)
    {
        return;
    }

    free_tab_list(group->tabs);
    free(group);
}

tab_t *dock_group_add_tab(tab_group_t *group, const char *title)
{
    tab_t *tab = NULL;
    tab_t *tail = NULL;

    if (group == NULL)
    {
        return NULL;
    }

    tab = (tab_t *)calloc(1, sizeof(*tab));
    if (tab == NULL)
    {
        return NULL;
    }

    tab->title = title;

    if (group->tabs == NULL)
    {
        group->tabs = tab;
    }
    else
    {
        tail = group->tabs;
        while (tail->next != NULL)
        {
            tail = tail->next;
        }
        tail->next = tab;
    }

    if (group->active_tab == NULL)
    {
        group->active_tab = tab;
    }

    return tab;
}

bool dock_group_remove_tab(tab_group_t *group, tab_t *tab)
{
    tab_t *current = NULL;
    tab_t *previous = NULL;

    if (group == NULL || tab == NULL)
    {
        return false;
    }

    current = group->tabs;
    while (current != NULL)
    {
        if (current == tab)
        {
            if (previous != NULL)
            {
                previous->next = current->next;
            }
            else
            {
                group->tabs = current->next;
            }

            if (group->active_tab == current)
            {
                group->active_tab = group->tabs;
            }

            free(current);
            return true;
        }

        previous = current;
        current = current->next;
    }

    return false;
}

dock_node_t *dock_node_create_leaf(tab_group_t *group)
{
    dock_node_t *node = NULL;

    if (group == NULL)
    {
        return NULL;
    }

    node = (dock_node_t *)calloc(1, sizeof(*node));
    if (node == NULL)
    {
        return NULL;
    }

    node->is_leaf = true;
    node->leaf.group = group;
    return node;
}

dock_node_t *dock_node_create_split(bool vertical_split, float split_ratio, dock_node_t *first, dock_node_t *second)
{
    dock_node_t *node = NULL;

    if (first == NULL || second == NULL)
    {
        return NULL;
    }

    node = (dock_node_t *)calloc(1, sizeof(*node));
    if (node == NULL)
    {
        return NULL;
    }

    node->is_leaf = false;
    node->split.vertical_split = vertical_split;
    node->split.split_ratio = clamp_ratio(split_ratio);
    node->split.first = first;
    node->split.second = second;

    first->parent = node;
    second->parent = node;

    return node;
}

void dock_node_destroy(dock_node_t *node)
{
    if (node == NULL)
    {
        return;
    }

    if (node->is_leaf)
    {
        dock_group_destroy(node->leaf.group);
        free(node);
        return;
    }

    dock_node_destroy(node->split.first);
    dock_node_destroy(node->split.second);
    free(node);
}

bool dock_node_insert_split(dock_t *dock, dock_node_t *target_leaf, bool vertical_split, float split_ratio, tab_group_t *new_group, bool place_new_first)
{
    dock_node_t *new_leaf = NULL;
    dock_node_t *split_node = NULL;
    dock_node_t *old_parent = NULL;

    if (dock == NULL || target_leaf == NULL || !target_leaf->is_leaf || new_group == NULL)
    {
        return false;
    }

    old_parent = target_leaf->parent;

    new_leaf = dock_node_create_leaf(new_group);
    if (new_leaf == NULL)
    {
        return false;
    }

    if (place_new_first)
    {
        split_node = dock_node_create_split(vertical_split, split_ratio, new_leaf, target_leaf);
    }
    else
    {
        split_node = dock_node_create_split(vertical_split, split_ratio, target_leaf, new_leaf);
    }

    if (split_node == NULL)
    {
        free(new_leaf);
        return false;
    }

    split_node->parent = old_parent;

    if (old_parent == NULL)
    {
        dock->root_document = split_node;
    }
    else if (old_parent->split.first == target_leaf)
    {
        old_parent->split.first = split_node;
    }
    else if (old_parent->split.second == target_leaf)
    {
        old_parent->split.second = split_node;
    }
    else
    {
        target_leaf->parent = old_parent;
        free(new_leaf);
        free(split_node);
        return false;
    }

    dock_layout(dock);
    return true;
}

bool dock_node_remove_leaf(dock_t *dock, dock_node_t *leaf_node, bool destroy_group)
{
    dock_node_t *parent = NULL;
    dock_node_t *sibling = NULL;
    dock_node_t *grand_parent = NULL;

    if (dock == NULL || leaf_node == NULL || !leaf_node->is_leaf)
    {
        return false;
    }

    parent = leaf_node->parent;
    if (parent == NULL)
    {
        if (destroy_group)
        {
            dock_group_destroy(leaf_node->leaf.group);
        }
        free(leaf_node);
        dock->root_document = NULL;
        dock_layout(dock);
        return true;
    }

    sibling = parent->split.first == leaf_node ? parent->split.second : parent->split.first;
    if (sibling == NULL)
    {
        return false;
    }

    grand_parent = parent->parent;
    sibling->parent = grand_parent;

    if (grand_parent == NULL)
    {
        dock->root_document = sibling;
    }
    else if (grand_parent->split.first == parent)
    {
        grand_parent->split.first = sibling;
    }
    else if (grand_parent->split.second == parent)
    {
        grand_parent->split.second = sibling;
    }
    else
    {
        sibling->parent = parent;
        return false;
    }

    if (destroy_group)
    {
        dock_group_destroy(leaf_node->leaf.group);
    }

    free(leaf_node);
    free(parent);

    dock_layout(dock);
    return true;
}

/***************************************************************
** MARK: STATIC FUNCTIONS
***************************************************************/

static void update_input(dock_t *dock)
{
    dock_node_t *hovered_split_node = NULL;
    xpa_rect_t hovered_splitter_frame = {0};
    xpa_cursor_t cursor = XPA_CURSOR_ARROW;

    dock->hover_left_splitter = dock->mouse_position.x >= dock->left_splitter_frame.x && dock->mouse_position.x <= dock->left_splitter_frame.x + dock->left_splitter_frame.width &&
                          dock->mouse_position.y >= dock->left_splitter_frame.y && dock->mouse_position.y <= dock->left_splitter_frame.y + dock->left_splitter_frame.height;

    dock->hover_right_splitter = dock->mouse_position.x >= dock->right_splitter_frame.x && dock->mouse_position.x <= dock->right_splitter_frame.x + dock->right_splitter_frame.width &&
                           dock->mouse_position.y >= dock->right_splitter_frame.y && dock->mouse_position.y <= dock->right_splitter_frame.y + dock->right_splitter_frame.height;

    dock->hover_bottom_splitter = dock->mouse_position.x >= dock->bottom_splitter_frame.x && dock->mouse_position.x <= dock->bottom_splitter_frame.x + dock->bottom_splitter_frame.width &&
                            dock->mouse_position.y >= dock->bottom_splitter_frame.y && dock->mouse_position.y <= dock->bottom_splitter_frame.y + dock->bottom_splitter_frame.height;

    if (dock->root_document != NULL)
    {
        find_hovered_split(dock->root_document, dock->mouse_position, &hovered_split_node, &hovered_splitter_frame);
    }
    dock->hover_document_split = hovered_split_node;
    dock->hover_document_splitter_frame = hovered_splitter_frame;

    if (!dock->primary_down)
    {
        dock->active_splitter = -1;
        dock->active_document_split = NULL;
        if (dock->hover_left_splitter || dock->hover_right_splitter)
        {
            cursor = XPA_CURSOR_RESIZE_EW;
        }
        else if (dock->hover_bottom_splitter)
        {
            cursor = XPA_CURSOR_RESIZE_NS;
        }
        else if (dock->hover_document_split != NULL)
        {
            cursor = dock->hover_document_split->split.vertical_split ? XPA_CURSOR_RESIZE_EW : XPA_CURSOR_RESIZE_NS;
        }
        xpa_set_cursor(cursor);
        return;
    }

    if (dock->active_document_split != NULL)
    {
        float available = split_available_size(dock->active_document_split);
        float first_size = 0.0f;
        float delta = 0.0f;

        if (available > 0.0f)
        {
            first_size = dock->document_split_ratio_start * available;

            if (dock->active_document_split->split.vertical_split)
            {
                delta = dock->mouse_position.x - dock->document_split_drag_start.x;
            }
            else
            {
                delta = dock->mouse_position.y - dock->document_split_drag_start.y;
            }

            first_size += delta;

            if (available >= (MIN_PANE_SIZE * 2.0f))
            {
                first_size = clampf(first_size, MIN_PANE_SIZE, available - MIN_PANE_SIZE);
            }
            else
            {
                first_size = clampf(first_size, 0.0f, available);
            }

            dock->active_document_split->split.split_ratio = clamp_ratio(first_size / available);
        }

        dock_set_frame(dock, dock->frame);
        cursor = dock->active_document_split->split.vertical_split ? XPA_CURSOR_RESIZE_EW : XPA_CURSOR_RESIZE_NS;
        xpa_set_cursor(cursor);
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
                else if (dock->hover_document_split != NULL)
                {
                    dock->active_document_split = dock->hover_document_split;
                    dock->document_split_drag_start = dock->mouse_position;
                    dock->document_split_ratio_start = dock->active_document_split->split.split_ratio;
                }
            }
        } break;
    }

    if (dock->active_splitter == SPLITTER_LEFT || dock->active_splitter == SPLITTER_RIGHT)
    {
        cursor = XPA_CURSOR_RESIZE_EW;
    }
    else if (dock->active_splitter == SPLITTER_BOTTOM)
    {
        cursor = XPA_CURSOR_RESIZE_NS;
    }
    else if (dock->hover_left_splitter || dock->hover_right_splitter)
    {
        cursor = XPA_CURSOR_RESIZE_EW;
    }
    else if (dock->hover_bottom_splitter)
    {
        cursor = XPA_CURSOR_RESIZE_NS;
    }
    else if (dock->hover_document_split != NULL)
    {
        cursor = dock->hover_document_split->split.vertical_split ? XPA_CURSOR_RESIZE_EW : XPA_CURSOR_RESIZE_NS;
    }

    dock_set_frame(dock, dock->frame);
    xpa_set_cursor(cursor);
}

static float clampf(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static float clamp_ratio(float ratio)
{
    return clampf(ratio, 0.05f, 0.95f);
}

static void free_tab_list(tab_t *head)
{
    tab_t *tab = head;
    while (tab != NULL)
    {
        tab_t *next = tab->next;
        free(tab);
        tab = next;
    }
}

static void layout_tabs(tab_group_t *group)
{
    float tab_x = 0.0f;
    float tab_bar_height = 0.0f;
    tab_t *tab = NULL;

    if (group == NULL)
    {
        return;
    }

    tab_x = group->frame.x;
    tab_bar_height = group->frame.height < TAB_BAR_HEIGHT ? group->frame.height : TAB_BAR_HEIGHT;
    tab = group->tabs;

    while (tab != NULL)
    {
        float remaining = (group->frame.x + group->frame.width) - tab_x;
        float width = remaining < TAB_MIN_WIDTH ? remaining : TAB_MIN_WIDTH;
        if (width < 0.0f)
        {
            width = 0.0f;
        }

        tab->frame = (xpa_rect_t){
            tab_x,
            group->frame.y,
            width,
            tab_bar_height
        };

        tab_x += TAB_MIN_WIDTH;
        tab = tab->next;
    }
}

static float split_available_size(const dock_node_t *node)
{
    if (node == NULL || node->is_leaf)
    {
        return 0.0f;
    }

    if (node->split.vertical_split)
    {
        if (node->frame.width <= SPLITTER_THICKNESS)
        {
            return node->frame.width;
        }
        return node->frame.width - SPLITTER_THICKNESS;
    }

    if (node->frame.height <= SPLITTER_THICKNESS)
    {
        return node->frame.height;
    }
    return node->frame.height - SPLITTER_THICKNESS;
}

static float split_first_size(const dock_node_t *node)
{
    float available = split_available_size(node);
    float first_size = 0.0f;

    if (available <= 0.0f)
    {
        return 0.0f;
    }

    first_size = available * clamp_ratio(node->split.split_ratio);

    if (available >= (MIN_PANE_SIZE * 2.0f))
    {
        first_size = clampf(first_size, MIN_PANE_SIZE, available - MIN_PANE_SIZE);
    }
    else
    {
        first_size = clampf(first_size, 0.0f, available);
    }

    return first_size;
}

static xpa_rect_t split_frame(const dock_node_t *node)
{
    xpa_rect_t frame = {0};
    float first_size = 0.0f;

    if (node == NULL || node->is_leaf)
    {
        return frame;
    }

    first_size = split_first_size(node);

    if (node->split.vertical_split)
    {
        frame = (xpa_rect_t){
            node->frame.x + first_size,
            node->frame.y,
            node->frame.width > SPLITTER_THICKNESS ? SPLITTER_THICKNESS : 0.0f,
            node->frame.height
        };
    }
    else
    {
        frame = (xpa_rect_t){
            node->frame.x,
            node->frame.y + first_size,
            node->frame.width,
            node->frame.height > SPLITTER_THICKNESS ? SPLITTER_THICKNESS : 0.0f
        };
    }

    return frame;
}

static bool point_in_rect(xpa_point_t point, xpa_rect_t rect)
{
    return point.x >= rect.x && point.x <= rect.x + rect.width &&
           point.y >= rect.y && point.y <= rect.y + rect.height;
}

static bool find_hovered_split(dock_node_t *node, xpa_point_t point, dock_node_t **hovered_node, xpa_rect_t *splitter_frame)
{
    xpa_rect_t current_splitter_frame = {0};

    if (node == NULL || node->is_leaf)
    {
        return false;
    }

    if (find_hovered_split(node->split.first, point, hovered_node, splitter_frame))
    {
        return true;
    }

    if (find_hovered_split(node->split.second, point, hovered_node, splitter_frame))
    {
        return true;
    }

    current_splitter_frame = split_frame(node);
    if (point_in_rect(point, current_splitter_frame))
    {
        *hovered_node = node;
        *splitter_frame = current_splitter_frame;
        return true;
    }

    return false;
}

static xpa_color_t group_color(const tab_group_t *group)
{
    (void)group;
    return (xpa_color_t){0.22f, 0.22f, 0.22f, 1.0f};
}

static void render_node(dock_t *dock, dock_node_t *node)
{
    if (node == NULL)
    {
        return;
    }

    if (node->is_leaf)
    {
        tab_group_t *group = node->leaf.group;
        if (group == NULL)
        {
            return;
        }

        draw_rect(group->frame, group_color(group));
        return;
    }

    render_node(dock, node->split.first);
    render_node(dock, node->split.second);

    {
        xpa_rect_t splitter = split_frame(node);
        xpa_color_t splitter_color = (xpa_color_t){0.18f, 0.18f, 0.18f, 1.0f};

        if (node == dock->active_document_split)
        {
            splitter_color = (xpa_color_t){0.90f, 0.68f, 0.24f, 1.0f};
        }
        else if (node == dock->hover_document_split)
        {
            splitter_color = (xpa_color_t){0.70f, 0.70f, 0.70f, 1.0f};
        }

        draw_rect(splitter, splitter_color);
    }
}

static void layout_node(dock_node_t *node, xpa_rect_t frame)
{
    if (node == NULL)
    {
        return;
    }

    if (frame.width < 0.0f)
    {
        frame.width = 0.0f;
    }
    if (frame.height < 0.0f)
    {
        frame.height = 0.0f;
    }

    node->frame = frame;

    if (node->is_leaf)
    {
        if (node->leaf.group != NULL)
        {
            node->leaf.group->frame = frame;
            layout_tabs(node->leaf.group);
        }
        return;
    }

    if (node->split.first == NULL || node->split.second == NULL)
    {
        return;
    }

    if (node->split.vertical_split)
    {
        xpa_rect_t first_frame = frame;
        xpa_rect_t second_frame = frame;
        float first_width = split_first_size(node);
        float splitter_width = SPLITTER_THICKNESS;

        if (frame.width <= SPLITTER_THICKNESS)
        {
            splitter_width = 0.0f;
        }

        first_frame.width = first_width;
        second_frame.x = frame.x + first_width + splitter_width;
        second_frame.width = frame.width - first_width - splitter_width;

        layout_node(node->split.first, first_frame);
        layout_node(node->split.second, second_frame);
    }
    else
    {
        xpa_rect_t first_frame = frame;
        xpa_rect_t second_frame = frame;
        float first_height = split_first_size(node);
        float splitter_height = SPLITTER_THICKNESS;

        if (frame.height <= SPLITTER_THICKNESS)
        {
            splitter_height = 0.0f;
        }

        first_frame.height = first_height;
        second_frame.y = frame.y + first_height + splitter_height;
        second_frame.height = frame.height - first_height - splitter_height;

        layout_node(node->split.first, first_frame);
        layout_node(node->split.second, second_frame);
    }
}
