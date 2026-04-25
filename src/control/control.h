/***************************************************************
**
** XPA Header File
**
** File         :  control.h
** Module       :  control
** Author       :  SH
** Created      :  2026-04-24 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Control Implementation
**
***************************************************************/

#ifndef CONTROL_H
#define CONTROL_H

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include <xpa/xpa.h>

#include <stddef.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include <nuklear/nuklear.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/

typedef enum
{
    CONTROL_TEXT_NORMAL = 0x00,
    CONTROL_TEXT_BOLD = 0x01
} control_text_flags_t;

typedef enum
{
    CONTROL_ICON_NONE = 0x20,
    CONTROL_ICON_GEAR = 0xf013,
    CONTROL_ICON_NETWORK = 0xf6ff,
    CONTROL_ICON_WIFI = 0xf1eb,
    CONTROL_ICON_SEARCH = 0xf002,
    CONTROL_ICON_SAVE = 0xf0c7,
    CONTROL_ICON_FOLDER = 0xf07b,
    CONTROL_ICON_PLUS = 0xf067,
    CONTROL_ICON_X = 0xf00d,
    CONTROL_ICON_HELP = 0xf128
} control_icon_t;

/***************************************************************
** MARK: FUNCTION DEFS
***************************************************************/

void control_set_context(struct nk_context *nk_context);
void control_set_normal_font(struct nk_font *font);
void control_set_bold_font(struct nk_font *font);
void control_set_icon_fonts(struct nk_font *regular_font, struct nk_font *solid_font);

void control_button(const char *label, xpa_rect_t frame);
bool control_icon_button(control_icon_t icon, xpa_rect_t frame);
void control_label(const char *text, xpa_rect_t frame, uint32_t flags);

void control_text_box(const char *buffer, size_t buffer_size, xpa_rect_t frame, uint32_t flags);

void control_menu(const char *label, xpa_rect_t frame, const char **items, const char **shortcuts, size_t item_count);

xpa_size_t control_text_size(const char *text, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_H */
