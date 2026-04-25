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
#include <stdio.h>

/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

#define XPA_COLOR_WHITE {1.0f, 1.0f, 1.0f, 1.0f}
#define XPA_COLOR_BLACK {0.0f, 0.0f, 0.0f, 1.0f}
#define XPA_COLOR_RED   {1.0f, 0.0f, 0.0f, 1.0f}
#define XPA_COLOR_GREEN {0.0f, 1.0f, 0.0f, 1.0f}
#define XPA_COLOR_BLUE  {0.0f, 0.0f, 1.0f, 1.0f}

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/

typedef enum
{
    XPA_BUTTON_PRIMARY,
    XPA_BUTTON_SECONDARY,
    XPA_BUTTON_TERTIARY,
    XPA_BUTTON_MAX = XPA_BUTTON_TERTIARY
} xpa_button_t;

typedef struct
{
    float x;
    float y;
} xpa_point_t;

typedef struct
{
    float width;
    float height;
} xpa_size_t;

typedef struct
{
    float x;
    float y;
    float width;
    float height;
} xpa_rect_t;

typedef struct
{
    float red;
    float green;
    float blue;
    float alpha;
} xpa_color_t;

typedef uintptr_t xpa_window_t;



/***************************************************************
** MARK: FUNCTION DEFS
***************************************************************/

void xpa_app_init(void);

bool xpa_init(void);

bool xpa_create_window(const char *title, uint32_t width, uint32_t height, xpa_window_t *window);

int xpa_run(void);

#ifdef __cplusplus
}
#endif

#endif /* XPA_H */