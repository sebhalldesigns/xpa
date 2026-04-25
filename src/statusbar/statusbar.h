/***************************************************************
**
** XPA Header File
**
** File         :  statusbar.h
** Module       :  statusbar
** Author       :  SH
** Created      :  2026-04-25 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Status Bar Implementation
**
***************************************************************/

#ifndef STATUSBAR_H
#define STATUSBAR_H

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

typedef struct
{   
    xpa_rect_t frame;
    xpa_rect_t label_frame;

} statusbar_t;

/***************************************************************
** MARK: FUNCTION DEFS
***************************************************************/

void statusbar_init(statusbar_t *statusbar);

void statusbar_set_frame(statusbar_t *statusbar, xpa_rect_t frame);
void statusbar_render(statusbar_t *statusbar);

#ifdef __cplusplus
}
#endif

#endif /* STATUSBAR_H */
