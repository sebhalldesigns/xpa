/***************************************************************
**
** XPA Header File
**
** File         :  menubar.h
** Module       :  menubar
** Author       :  SH
** Created      :  2026-04-24 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Menu Bar Implementation
**
***************************************************************/

#ifndef MENUBAR_H
#define MENUBAR_H

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

/* struct to represent single instance of a dock */
typedef struct
{   
    size_t entries;
    xpa_frame_t frame;
} menubar_t;

/***************************************************************
** MARK: FUNCTION DEFS
***************************************************************/

void menubar_init(menubar_t *menubar);

void menubar_set_frame(menubar_t *menubar, xpa_frame_t frame);
void menubar_render(menubar_t *menubar);

#ifdef __cplusplus
}
#endif

#endif /* MENUBAR_H */
