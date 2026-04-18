/***************************************************************
**
** XPA Header File
**
** File         :  xpa.h
** Module       :  xpa
** Author       :  SH
** Created      :  2026-04-18 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Interface Definition
**
***************************************************************/

#ifndef XPA_H
#define XPA_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include <stdint.h>
#include <stdbool.h>

/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/

typedef uintptr_t xpa_window_t;

/***************************************************************
** MARK: FUNCTION DEFS
***************************************************************/

bool xpa_init(void);

bool xpa_create_window(const char *title, uint32_t width, uint32_t height, xpa_window_t *window);

int xpa_run(void);

#ifdef __cplusplus
}
#endif

#endif /* XPA_H */