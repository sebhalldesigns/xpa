/***************************************************************
**
** XPA Source File
**
** File         :  menubar.c
** Module       :  menubar
** Author       :  SH
** Created      :  2026-04-24 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Menu Bar Implementation
**
***************************************************************/

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include "menubar.h"

#include <draw/draw.h>
#include <control/control.h>

#include <stdlib.h>

/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

#define WINDOW_BUTTONS_WIDTH (140.0f)

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/

/***************************************************************
** MARK: STATIC VARIABLES
***************************************************************/

static const char *menu_items[] = {"File", "Edit", "View", "Help"};
static const char *file_menu_items[] = {"New", "Open", "Save", "Exit"};
static const char *edit_menu_items[] = {"Undo", "Redo", "Cut", "Copy", "Paste"};
static const char *view_menu_items[] = {"Zoom In", "Zoom Out", "Reset Zoom"};
static const char *help_menu_items[] = {"Documentation", "About"};
static const char *file_menu_shortcuts[] = {"Ctrl+N", "Ctrl+O", "Ctrl+S", "Alt+F4"};
static const char *edit_menu_shortcuts[] = {"Ctrl+Z", "Ctrl+Y", "Ctrl+X", "Ctrl+C", "Ctrl+V"};
static const char *view_menu_shortcuts[] = {"Ctrl++", "Ctrl+-", "Ctrl+0"};
static const char *help_menu_shortcuts[] = {"F1", ""};

static struct { const char **items; const char **shortcuts; size_t count; } menus[] = {
    {file_menu_items, file_menu_shortcuts, sizeof(file_menu_items) / sizeof(file_menu_items[0])},
    {edit_menu_items, edit_menu_shortcuts, sizeof(edit_menu_items) / sizeof(edit_menu_items[0])},
    {view_menu_items, view_menu_shortcuts, sizeof(view_menu_items) / sizeof(view_menu_items[0])},
    {help_menu_items, help_menu_shortcuts, sizeof(help_menu_items) / sizeof(help_menu_items[0])}
};


/***************************************************************
** MARK: STATIC FUNCTION DEFS
***************************************************************/

/***************************************************************
** MARK: PUBLIC FUNCTIONS
***************************************************************/

void menubar_init(menubar_t *menubar)
{
    
}

void menubar_set_frame(menubar_t *menubar, xpa_rect_t frame)
{   
    frame.width -= WINDOW_BUTTONS_WIDTH;
    menubar->frame = frame;

    xpa_size_t logo_size = control_text_size("BusLab", CONTROL_TEXT_BOLD);

    menubar->logo_frame = (xpa_rect_t){
        frame.x + 10.0f,
        frame.y + (frame.height - logo_size.height) / 2.0f,
        logo_size.width,
        logo_size.height
    };

    menubar->menu_frame = (xpa_rect_t){
        menubar->logo_frame.x + menubar->logo_frame.width + 10.0f,
        frame.y,
        0.0f,
        frame.height
    };

    for (size_t i = 0; i < sizeof(menu_items) / sizeof(menu_items[0]); ++i)
    {
        xpa_size_t item_size = control_text_size(menu_items[i], 0);
        item_size.width += 10.0f + 4.0f; // padding + extra spacing

        menubar->menu_frame.width += item_size.width;
    }

    float buttons_width = 50.0f;

    menubar->buttons_frame = (xpa_rect_t){
        frame.width - buttons_width,
        frame.y,
        buttons_width,
        frame.height
    };

    float max_search_width = 500.0f;
    float min_handle_width = 50.0f;

    float menu_end = (menubar->menu_frame.x + menubar->menu_frame.width);

    float center_x = frame.x + (frame.width / 2.0f) + (WINDOW_BUTTONS_WIDTH / 2.0f);

    float search_x = menu_end + min_handle_width;

    if (center_x - search_x > max_search_width / 2.0f)
        search_x = center_x - max_search_width / 2.0f;
        
    float search_end = menubar->buttons_frame.x - min_handle_width; 
    if (search_end - center_x > max_search_width / 2.0f)
        search_end = center_x + max_search_width / 2.0f;

    menubar->search_frame = (xpa_rect_t){
        search_x,
        frame.y + 5.0f,
        search_end - search_x,
        frame.height - 10.0f
    };

}

void menubar_render(menubar_t *menubar)
{
    //draw_rect(menubar->search_frame, (xpa_color_t){1.0f, 0.15f, 0.15f, 1.0f});

    control_label("BusLab", menubar->logo_frame, CONTROL_TEXT_BOLD);

    
    float start_x = menubar->logo_frame.x + menubar->logo_frame.width + 10.0f;
    float item_spacing = 20.0f;

    for (size_t i = 0; i < sizeof(menu_items) / sizeof(menu_items[0]); ++i)
    {
        xpa_size_t button_size = control_text_size(menu_items[i], 0);
        button_size.width += 4.0f;
        button_size.height += 4.0f;

        xpa_rect_t item_frame = {
            start_x,
            menubar->frame.y + (menubar->frame.height - button_size.height) / 2.0f,
            button_size.width + 10.0f,
            button_size.height
        };

        control_menu(menu_items[i], item_frame, menus[i].items, menus[i].shortcuts, menus[i].count);

        start_x += item_frame.width;
    }

    float button_size = 20.0f;

    xpa_rect_t button_frame = menubar->buttons_frame;
    button_frame = (xpa_rect_t){
        button_frame.x,
        button_frame.y + (button_frame.height - button_size) / 2.0f,
        button_size,
        button_size
    };

    if (control_icon_button(CONTROL_ICON_HELP, button_frame))
    {
        /* dispatch close */
    }

    button_frame.x += button_size + 5.0f;

    if (control_icon_button(CONTROL_ICON_GEAR, button_frame))
    {
        /* dispatch settings */
    }

    static char ui_menu_search_text[256] = "";
    control_text_box(ui_menu_search_text, sizeof(ui_menu_search_text), menubar->search_frame, 0);
}

bool menubar_hit_test(menubar_t *menubar, xpa_point_t point)
{
    bool in_menu = point.x >= menubar->menu_frame.x && point.x <= (menubar->menu_frame.x + menubar->menu_frame.width) &&
                    point.y >= menubar->menu_frame.y && point.y <= (menubar->menu_frame.y + menubar->menu_frame.height);

    bool in_buttons = point.x >= menubar->buttons_frame.x && point.x <= (menubar->buttons_frame.x + menubar->buttons_frame.width) &&
                      point.y >= menubar->buttons_frame.y && point.y <= (menubar->buttons_frame.y + menubar->buttons_frame.height);

    bool in_search = point.x >= menubar->search_frame.x && point.x <= (menubar->search_frame.x + menubar->search_frame.width) &&
                     point.y >= menubar->search_frame.y && point.y <= (menubar->search_frame.y + menubar->search_frame.height);

    return in_menu || in_buttons || in_search;
}

/***************************************************************
** MARK: STATIC FUNCTIONS
***************************************************************/
