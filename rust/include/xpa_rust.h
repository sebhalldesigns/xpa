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

typedef struct XpaGpuContext XpaGpuContext;
typedef struct XpaScene XpaScene;
typedef struct XpaGpuSurface XpaGpuSurface;

/***************************************************************
** MARK: FUNCTION DEFS
***************************************************************/

bool xpa_rust_init(void);

// GPU context - one per application, holds device + queue
XpaGpuContext* xpa_gpu_context_create(void);
XpaGpuContext* xpa_gpu_context_create_with_surface(void* native_handle, uint32_t width, uint32_t height, XpaGpuSurface** out_surface);
void xpa_gpu_context_destroy(XpaGpuContext* ctx);

// Surface - one per window/panel that needs GPU rendering
XpaGpuSurface* xpa_gpu_surface_create(XpaGpuContext* ctx, void* native_handle, uint32_t width, uint32_t height);
void xpa_gpu_surface_destroy(XpaGpuSurface* surface);
void xpa_gpu_surface_resize(XpaGpuContext* ctx, XpaGpuSurface* surface, uint32_t width, uint32_t height);

// Scene building + rendering
XpaScene* xpa_scene_create(void);
void xpa_scene_destroy(XpaScene* scene);
void xpa_scene_clear(XpaScene* scene);
void xpa_gpu_render(XpaGpuContext* ctx, XpaGpuSurface* surface, XpaScene* scene);

#ifdef __cplusplus
}
#endif

#endif /* XPA_RUST_H */
