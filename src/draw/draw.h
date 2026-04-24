/***************************************************************
**
** XPA Header File
**
** File         :  draw.h
** Module       :  draw
** Author       :  SH
** Created      :  2026-04-24 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Drawing Implementation
**
***************************************************************/

#ifndef DRAW_H
#define DRAW_H

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include <xpa/xpa.h>

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

/***************************************************************
** MARK: FUNCTION DEFS
***************************************************************/

void draw_set_context(struct nk_context *nk_context);

void draw_rect(xpa_frame_t *frame, xpa_color_t *color);

#ifdef __cplusplus
}
#endif

#endif /* DRAW_H */
