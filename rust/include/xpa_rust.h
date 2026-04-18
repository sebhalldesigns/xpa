/***************************************************************
**
** XPA Header File
**
** File         :  xpa_rust.h
** Module       :  rust
** Author       :  SH
** Created      :  2026-04-18 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA Rust Module Interface Definition
**
***************************************************************/

#ifndef XPA_RUST_H
#define XPA_RUST_H

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

typedef struct XpaRenderer XpaRenderer;

/***************************************************************
** MARK: FUNCTION DEFS
***************************************************************/

bool xpa_rust_init(void);

// Simple per-window renderer lifecycle
XpaRenderer* xpa_renderer_init(void* native_handle, uint32_t width, uint32_t height);
void xpa_renderer_resize(XpaRenderer* renderer, uint32_t width, uint32_t height);
void xpa_renderer_render(XpaRenderer* renderer);
void xpa_renderer_destroy(XpaRenderer* renderer);

#ifdef __cplusplus
}
#endif

#endif /* XPA_RUST_H */
