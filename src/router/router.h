/***************************************************************
**
** XPA Header File
**
** File         :  router.h
** Module       :  router
** Author       :  SH
** Created      :  2026-01-23 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA in-memory web router
**
***************************************************************/

#ifndef ROUTER_H
#define ROUTER_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include <stddef.h>
#include <stdbool.h>

/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/

/***************************************************************
** MARK: FUNCTION DEFS
***************************************************************/

bool router_resolve(const char *path, const char **data, size_t *length, const char **mime_type);

#ifdef __cplusplus
}
#endif

#endif /* ROUTER_H */