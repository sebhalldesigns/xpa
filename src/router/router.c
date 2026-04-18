/***************************************************************
**
** XPA Source File
**
** File         :  router.c
** Module       :  router
** Author       :  SH
** Created      :  2026-01-23 (YYYY-MM-DD)
** License      :  MIT
** Description  :  XPA in-memory web router
**
***************************************************************/

/***************************************************************
** MARK: INCLUDES
***************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "router.h"

/***************************************************************
** MARK: CONSTANTS & MACROS
***************************************************************/

#define TEXT_HTML "text/html"
#define TEXT_CSS "text/css"
#define TEXT_JAVASCRIPT "text/javascript"
#define FONT_WOFF2 "font/woff2"

/***************************************************************
** MARK: TYPEDEFS
***************************************************************/

typedef struct
{
    const uint8_t *data;
    const uint32_t *length;
    const char *path;
    const char *mime_type;
} web_resource_t;

/***************************************************************
** MARK: STATIC VARIABLES
***************************************************************/

/* HTML */
extern const uint8_t webui_index_html[];
extern const uint32_t webui_index_html_size;

/* CSS */
extern const uint8_t webui_assets_style_css[];
extern const uint32_t webui_assets_style_css_size;

/* JAVASCRIPT */
extern const uint8_t webui_assets_index_js[];
extern const uint32_t webui_assets_index_js_size;

static web_resource_t resources[] = {
    {webui_index_html, &webui_index_html_size, "/index.html", TEXT_HTML},
    {webui_assets_style_css, &webui_assets_style_css_size, "/assets/style.css", TEXT_CSS},
    {webui_assets_index_js, &webui_assets_index_js_size, "/assets/index.js", TEXT_JAVASCRIPT}
};

/***************************************************************
** MARK: STATIC FUNCTION DEFS
***************************************************************/

/***************************************************************
** MARK: PUBLIC FUNCTIONS
***************************************************************/

bool router_resolve(const char *path, const char **data, size_t *length, const char **mime_type)
{
    if (!path)
    {
        return false;
    }

    for (int i = 0; i < sizeof(resources)/sizeof(web_resource_t); i++)
    {
        if (strcmp(resources[i].path, path) == 0)
        {
            *data = (const char*)resources[i].data;
            *length = *resources[i].length;
            *mime_type = resources[i].mime_type;
            return true;
        }
    }

    fprintf("NOT FOUND: %s\n", path);

    return false;
}

/***************************************************************
** MARK: STATIC FUNCTIONS
***************************************************************/
