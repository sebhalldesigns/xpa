/***************************************************************
**
** XPA Source File
**
** File         :  control.c
** Module       :  control
** Author       :  SH
** Created      :  2026-04-24 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Control Implementation
**
***************************************************************/

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include "control.h"

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

static struct nk_context *nk_ctx = NULL;
static struct nk_font *normal_font = NULL;
static struct nk_font *bold_font = NULL;
static struct nk_font *icon_regular_font = NULL;
static struct nk_font *icon_solid_font = NULL;
static nk_bool menu_bar_active = nk_false;
static const char *active_menu_label = NULL;

/***************************************************************
** MARK: STATIC FUNCTION DEFS
***************************************************************/

static void control_utf8_from_codepoint(uint32_t codepoint, char out[5]);

/***************************************************************
** MARK: PUBLIC FUNCTIONS
***************************************************************/

void control_set_context(struct nk_context *nk_context)
{
    nk_ctx = nk_context;
}

void control_set_normal_font(struct nk_font *font)
{
    normal_font = font;
}

void control_set_bold_font(struct nk_font *font)
{
    bold_font = font;
}

void control_set_icon_fonts(struct nk_font *regular_font, struct nk_font *solid_font)
{
    icon_regular_font = regular_font;
    icon_solid_font = solid_font;
}

void control_button(const char *label, xpa_rect_t frame)
{
    nk_layout_space_push(nk_ctx, *(struct nk_rect*)&frame);

    if (nk_button_label(nk_ctx, label))
    {
        // Button was clicked
        printf("Button '%s' clicked\n", label);
    }
}

bool control_icon_button(control_icon_t icon, xpa_rect_t frame)
{
    char label[5];
    struct nk_font *icon_font = icon_solid_font ? icon_solid_font : icon_regular_font;
    struct nk_style_button icon_button_style = nk_ctx->style.menu_button;
    struct nk_color button_tint = nk_rgba(255, 255, 255, 28);
    nk_bool pushed_font = nk_false;
    nk_bool clicked;

    control_utf8_from_codepoint((uint32_t)icon, label);
    nk_layout_space_push(nk_ctx, *(struct nk_rect*)&frame);

    icon_button_style.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0));
    icon_button_style.hover = nk_style_item_color(button_tint);
    icon_button_style.active = nk_style_item_color(button_tint);
    icon_button_style.padding = nk_vec2(0.0f, 2.0f);
    icon_button_style.touch_padding = nk_vec2(0.0f, 0.0f);
    icon_button_style.border = 0.0f;
    icon_button_style.rounding = 4.0f;
    icon_button_style.text_alignment = NK_TEXT_CENTERED;

    if (icon_font)
        pushed_font = nk_style_push_font(nk_ctx, &icon_font->handle);

    clicked = nk_button_label_styled(nk_ctx, &icon_button_style, label);

    if (pushed_font)
        nk_style_pop_font(nk_ctx);

    return clicked ? true : false;
}

void control_label(const char *text, xpa_rect_t frame, uint32_t flags)
{

    nk_layout_space_push(nk_ctx, *(struct nk_rect*)&frame);
    
    nk_bool pushed_font = nk_false;
    if (bold_font && (flags & CONTROL_TEXT_BOLD))
        pushed_font = nk_style_push_font(nk_ctx, &bold_font->handle);
    
    nk_label(nk_ctx, text, NK_TEXT_LEFT);

    if (pushed_font)
        nk_style_pop_font(nk_ctx);
}

void control_text_box(const char *buffer, size_t buffer_size, xpa_rect_t frame, uint32_t flags)
{
    nk_layout_space_push(nk_ctx, *(struct nk_rect*)&frame);

    nk_bool pushed_font = nk_false;
    if (bold_font && (flags & CONTROL_TEXT_BOLD))
        pushed_font = nk_style_push_font(nk_ctx, &bold_font->handle);
    nk_edit_string_zero_terminated(nk_ctx, NK_EDIT_FIELD, buffer, buffer_size, nk_filter_default);

    if (pushed_font)
        nk_style_pop_font(nk_ctx);
}

void control_menu(const char *label, xpa_rect_t frame, const char **items, const char **shortcuts, size_t item_count)
{
    const float item_row_height = 24.0f;
    const float popup_width = 220.0f;
    const float popup_height = nk_ctx->style.window.menu_padding.y +
        ((float)item_count * (item_row_height + nk_ctx->style.window.spacing.y));
    struct nk_rect header_frame = *(struct nk_rect*)&frame;
    struct nk_rect popup_frame = nk_rect(frame.x, frame.y + frame.height, popup_width, popup_height);
    struct nk_rect screen_header_frame = nk_layout_space_rect_to_screen(nk_ctx, header_frame);
    struct nk_rect screen_popup_frame = nk_layout_space_rect_to_screen(nk_ctx, popup_frame);
    struct nk_color popup_background = nk_rgb(38, 38, 46);
    struct nk_color shortcut_color = nk_rgb(110, 110, 135);
    struct nk_color menu_button_tint = nk_rgba(255, 255, 255, 28);
    nk_bool header_hovered = nk_input_is_mouse_hovering_rect(&nk_ctx->input, screen_header_frame);
    nk_bool popup_hovered = nk_input_is_mouse_hovering_rect(&nk_ctx->input, screen_popup_frame);
    nk_bool mouse_pressed = nk_input_is_mouse_pressed(&nk_ctx->input, NK_BUTTON_LEFT);
    nk_bool menu_is_active = menu_bar_active && active_menu_label && strcmp(active_menu_label, label) == 0;
    nk_bool close_active_menu = nk_false;

    nk_layout_space_push(nk_ctx, *(struct nk_rect*)&frame);

    nk_bool pushed_background = nk_style_push_color(
        nk_ctx, &nk_ctx->style.window.background, popup_background);
    nk_bool pushed_fixed_background = nk_style_push_style_item(
        nk_ctx, &nk_ctx->style.window.fixed_background, nk_style_item_color(popup_background));
    nk_bool pushed_popup_border = nk_style_push_float(
        nk_ctx, &nk_ctx->style.window.popup_border, 0.0f);
    nk_bool pushed_rounding = nk_style_push_float(
        nk_ctx, &nk_ctx->style.window.rounding, 8.0f);
    nk_bool pushed_contextual_padding = nk_style_push_vec2(
        nk_ctx, &nk_ctx->style.contextual_button.padding, nk_vec2(12.0f, 4.0f));
    nk_bool pushed_contextual_rounding = nk_style_push_float(
        nk_ctx, &nk_ctx->style.contextual_button.rounding, 4.0f);
    nk_bool pushed_menu_button_normal = nk_style_push_style_item(
        nk_ctx, &nk_ctx->style.menu_button.normal,
        nk_style_item_color(menu_is_active ? menu_button_tint : nk_rgba(0, 0, 0, 0)));
    nk_bool pushed_menu_button_hover = nk_style_push_style_item(
        nk_ctx, &nk_ctx->style.menu_button.hover, nk_style_item_color(menu_button_tint));
    nk_bool pushed_menu_button_active = nk_style_push_style_item(
        nk_ctx, &nk_ctx->style.menu_button.active, nk_style_item_color(menu_button_tint));

    nk_bool button_clicked = nk_button_label_styled(nk_ctx, &nk_ctx->style.menu_button, label);

    if (menu_is_active && header_hovered && mouse_pressed)
    {
        close_active_menu = nk_true;
    }
    else if (button_clicked)
    {
        menu_bar_active = nk_true;
        active_menu_label = label;
    }
    else if (menu_bar_active && header_hovered)
    {
        active_menu_label = label;
    }
    else if (menu_is_active && mouse_pressed && !header_hovered && !popup_hovered)
    {
        close_active_menu = nk_true;
    }

    menu_is_active = menu_bar_active && active_menu_label && strcmp(active_menu_label, label) == 0;
    if (menu_is_active && nk_popup_begin(nk_ctx, NK_POPUP_STATIC, label, NK_WINDOW_NO_SCROLLBAR, popup_frame))
    {
        struct nk_command_buffer *canvas = nk_window_get_canvas(nk_ctx);
        const struct nk_user_font *font = normal_font ? &normal_font->handle : nk_ctx->style.font;

        nk_layout_row_dynamic(nk_ctx, item_row_height, 1);
        for (size_t i = 0; i < item_count; ++i)
        {
            if (nk_menu_item_label(nk_ctx, items[i], NK_TEXT_LEFT))
            {
                // Menu item was clicked
                printf("Menu item '%s' clicked\n", items[i]);
                close_active_menu = nk_true;
            }

            if (shortcuts && shortcuts[i] && shortcuts[i][0] != '\0' && font)
            {
                struct nk_rect bounds = nk_layout_widget_bounds(nk_ctx);
                const char *shortcut = shortcuts[i];
                float pad = nk_ctx->style.contextual_button.padding.x;
                float shortcut_width = font->width(font->userdata, font->height, shortcut, (int)strlen(shortcut));
                struct nk_rect shortcut_frame;

                shortcut_frame.x = bounds.x + bounds.w - shortcut_width - pad;
                shortcut_frame.y = bounds.y + 4.0f;
                shortcut_frame.w = shortcut_width;
                shortcut_frame.h = bounds.h;

                nk_draw_text(canvas, shortcut_frame, shortcut, (int)strlen(shortcut), font,
                    nk_rgba(0, 0, 0, 0), shortcut_color);
            }
        }

        if (close_active_menu)
            nk_popup_close(nk_ctx);

        nk_popup_end(nk_ctx);

        if (close_active_menu)
        {
            menu_bar_active = nk_false;
            active_menu_label = NULL;
        }
    }

    if (pushed_contextual_rounding)
        nk_style_pop_float(nk_ctx);
    if (pushed_menu_button_active)
        nk_style_pop_style_item(nk_ctx);
    if (pushed_menu_button_hover)
        nk_style_pop_style_item(nk_ctx);
    if (pushed_menu_button_normal)
        nk_style_pop_style_item(nk_ctx);
    if (pushed_contextual_padding)
        nk_style_pop_vec2(nk_ctx);
    if (pushed_rounding)
        nk_style_pop_float(nk_ctx);
    if (pushed_popup_border)
        nk_style_pop_float(nk_ctx);
    if (pushed_fixed_background)
        nk_style_pop_style_item(nk_ctx);
    if (pushed_background)
        nk_style_pop_color(nk_ctx);
}

xpa_size_t control_text_size(const char *text, uint32_t flags)
{
    const struct nk_user_font *font = NULL;

    if (bold_font && (flags & CONTROL_TEXT_BOLD))
        font = &bold_font->handle;
    else if (normal_font)
        font = &normal_font->handle;
    else if (nk_ctx)
        font = nk_ctx->style.font;

    if (!font || !text)
        return (xpa_size_t){0.0f, 0.0f};

    return (xpa_size_t){
        font->width(font->userdata, font->height, text, (int)strlen(text)),
        font->height
    };
}

/***************************************************************
** MARK: STATIC FUNCTIONS
***************************************************************/

static void control_utf8_from_codepoint(uint32_t codepoint, char out[5])
{
    if (codepoint <= 0x7f)
    {
        out[0] = (char)codepoint;
        out[1] = '\0';
    }
    else if (codepoint <= 0x7ff)
    {
        out[0] = (char)(0xc0 | (codepoint >> 6));
        out[1] = (char)(0x80 | (codepoint & 0x3f));
        out[2] = '\0';
    }
    else if (codepoint <= 0xffff)
    {
        out[0] = (char)(0xe0 | (codepoint >> 12));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        out[2] = (char)(0x80 | (codepoint & 0x3f));
        out[3] = '\0';
    }
    else
    {
        out[0] = (char)(0xf0 | (codepoint >> 18));
        out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        out[3] = (char)(0x80 | (codepoint & 0x3f));
        out[4] = '\0';
    }
}
