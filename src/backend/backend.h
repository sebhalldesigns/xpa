/***************************************************************
**
** XPA Header File
**
** File         :  backend.h
** Module       :  backend
** Author       :  SH
** Created      :  2026-04-18 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Backend Interface Definition
**
***************************************************************/

#ifndef BACKEND_H
#define BACKEND_H

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

/***************************************************************
** MARK: FUNCTION DEFS
***************************************************************/

bool xpa_backend_init(void);

bool xpa_backend_create_window(const char *title, uint32_t width, uint32_t height, xpa_window_t *window);

int xpa_backend_run(void);

void xpa_backend_set_cursor(xpa_cursor_t cursor);



#ifdef __cplusplus
}
#endif

#endif /* BACKEND_H */
