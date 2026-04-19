/***************************************************************
**
** nk_gl3.h — Minimal Nuklear GL3 rendering backend
**
** Usage:
**   In exactly ONE .c/.cpp file, before including this header:
**     #define NK_GL3_IMPLEMENTATION
**     #include "nk_gl3.h"
**
**   All other files can include this header without the define
**   to get the function declarations.
**
** Requirements:
**   - glad (or another GL loader) already initialised
**   - nuklear.h already included with NK_INCLUDE_VERTEX_BUFFER_OUTPUT,
**     NK_INCLUDE_FONT_BAKING, NK_INCLUDE_DEFAULT_FONT
**
***************************************************************/

#ifndef NK_GL3_H
#define NK_GL3_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the GL3 backend. Call once after GL context + glad are ready. */
void nk_gl3_init(struct nk_context *ctx, int width, int height);

/* Begin font atlas construction. Add fonts between begin/end. */
void nk_gl3_font_stash_begin(struct nk_font_atlas **atlas);

/* Finish font atlas, upload to GL, set as default font. */
void nk_gl3_font_stash_end(struct nk_context *ctx);

/* Render Nuklear's draw commands. Call after nk_end(), before SwapBuffers. */
void nk_gl3_render(struct nk_context *ctx, int width, int height);

/* Clean up all GL resources. */
void nk_gl3_shutdown(struct nk_context *ctx);

#ifdef __cplusplus
}
#endif

/* ───────────────────────────────────────────────────────────── */
/*                     IMPLEMENTATION                           */
/* ───────────────────────────────────────────────────────────── */

#ifdef NK_GL3_IMPLEMENTATION

#include <string.h>
#include <math.h>

#ifndef NK_GL3_MAX_VERTEX_MEMORY
#define NK_GL3_MAX_VERTEX_MEMORY  (512 * 1024)
#endif
#ifndef NK_GL3_MAX_ELEMENT_MEMORY
#define NK_GL3_MAX_ELEMENT_MEMORY (128 * 1024)
#endif

struct nk_gl3_vertex {
    float position[2];
    float uv[2];
    nk_byte col[4];
};

static struct {
    GLuint prog;
    GLuint vert_shader;
    GLuint frag_shader;
    GLint  uniform_tex;
    GLint  uniform_proj;
    GLuint vbo;
    GLuint ebo;
    GLuint vao;
    GLuint font_tex;
    struct nk_font_atlas atlas;
    struct nk_draw_null_texture tex_null;
    struct nk_buffer cmds;
} nk_gl3;

static const char *nk_gl3_vertex_shader_src =
    "#version 330\n"
    "uniform mat4 ProjMtx;\n"
    "in vec2 Position;\n"
    "in vec2 TexCoord;\n"
    "in vec4 Color;\n"
    "out vec2 Frag_UV;\n"
    "out vec4 Frag_Color;\n"
    "void main() {\n"
    "   Frag_UV = TexCoord;\n"
    "   Frag_Color = Color;\n"
    "   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
    "}\n";

static const char *nk_gl3_fragment_shader_src =
    "#version 330\n"
    "precision mediump float;\n"
    "uniform sampler2D Texture;\n"
    "in vec2 Frag_UV;\n"
    "in vec4 Frag_Color;\n"
    "out vec4 Out_Color;\n"
    "void main() {\n"
    "   Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
    "}\n";

static GLuint
nk_gl3_compile_shader(GLenum type, const char *src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "[nk_gl3] shader compile error: %s\n", log);
    }
    return shader;
}

void
nk_gl3_init(struct nk_context *ctx, int width, int height)
{
    (void)width; (void)height;

    nk_buffer_init_default(&nk_gl3.cmds);

    /* Compile shaders */
    nk_gl3.vert_shader = nk_gl3_compile_shader(GL_VERTEX_SHADER, nk_gl3_vertex_shader_src);
    nk_gl3.frag_shader = nk_gl3_compile_shader(GL_FRAGMENT_SHADER, nk_gl3_fragment_shader_src);

    nk_gl3.prog = glCreateProgram();
    glAttachShader(nk_gl3.prog, nk_gl3.vert_shader);
    glAttachShader(nk_gl3.prog, nk_gl3.frag_shader);
    glLinkProgram(nk_gl3.prog);

    GLint status;
    glGetProgramiv(nk_gl3.prog, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        char log[512];
        glGetProgramInfoLog(nk_gl3.prog, sizeof(log), NULL, log);
        fprintf(stderr, "[nk_gl3] program link error: %s\n", log);
    }

    nk_gl3.uniform_tex  = glGetUniformLocation(nk_gl3.prog, "Texture");
    nk_gl3.uniform_proj = glGetUniformLocation(nk_gl3.prog, "ProjMtx");
    GLint attrib_pos = glGetAttribLocation(nk_gl3.prog, "Position");
    GLint attrib_uv  = glGetAttribLocation(nk_gl3.prog, "TexCoord");
    GLint attrib_col = glGetAttribLocation(nk_gl3.prog, "Color");

    /* Create buffers */
    glGenBuffers(1, &nk_gl3.vbo);
    glGenBuffers(1, &nk_gl3.ebo);
    glGenVertexArrays(1, &nk_gl3.vao);

    glBindVertexArray(nk_gl3.vao);
    glBindBuffer(GL_ARRAY_BUFFER, nk_gl3.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, nk_gl3.ebo);

    glEnableVertexAttribArray((GLuint)attrib_pos);
    glEnableVertexAttribArray((GLuint)attrib_uv);
    glEnableVertexAttribArray((GLuint)attrib_col);

    GLsizei vs = (GLsizei)sizeof(struct nk_gl3_vertex);
    size_t vp = offsetof(struct nk_gl3_vertex, position);
    size_t vt = offsetof(struct nk_gl3_vertex, uv);
    size_t vc = offsetof(struct nk_gl3_vertex, col);

    glVertexAttribPointer((GLuint)attrib_pos, 2, GL_FLOAT,         GL_FALSE, vs, (void *)vp);
    glVertexAttribPointer((GLuint)attrib_uv,  2, GL_FLOAT,         GL_FALSE, vs, (void *)vt);
    glVertexAttribPointer((GLuint)attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE,  vs, (void *)vc);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    /* Init nuklear context with default font (will be replaced by font_stash_end) */
    nk_init_default(ctx, NULL);
}

void
nk_gl3_font_stash_begin(struct nk_font_atlas **atlas)
{
    nk_font_atlas_init_default(&nk_gl3.atlas);
    nk_font_atlas_begin(&nk_gl3.atlas);
    *atlas = &nk_gl3.atlas;
}

void
nk_gl3_font_stash_end(struct nk_context *ctx)
{
    const void *image;
    int w, h;
    image = nk_font_atlas_bake(&nk_gl3.atlas, &w, &h, NK_FONT_ATLAS_RGBA32);

    /* Upload font atlas texture */
    glGenTextures(1, &nk_gl3.font_tex);
    glBindTexture(GL_TEXTURE_2D, nk_gl3.font_tex);
    /* Keep glyphs sharp with pixel-snapped font quads. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    nk_font_atlas_end(&nk_gl3.atlas, nk_handle_id((int)nk_gl3.font_tex), &nk_gl3.tex_null);

    /* Set the baked font as default */
    if (nk_gl3.atlas.default_font)
        nk_style_set_font(ctx, &nk_gl3.atlas.default_font->handle);
}

void
nk_gl3_render(struct nk_context *ctx, int width, int height)
{
    /* Save GL state */
    GLint last_prog, last_tex, last_vao, last_vbo, last_ebo;
    GLint last_viewport[4];
    GLint last_scissor_box[4];
    GLint last_blend_src_rgb, last_blend_dst_rgb;
    GLint last_blend_src_alpha, last_blend_dst_alpha;
    GLint last_blend_eq_rgb, last_blend_eq_alpha;
    GLboolean last_enable_blend, last_enable_cull, last_enable_depth, last_enable_scissor;

    glGetIntegerv(GL_CURRENT_PROGRAM,             &last_prog);
    glGetIntegerv(GL_TEXTURE_BINDING_2D,          &last_tex);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,        &last_vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,        &last_vbo);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_ebo);
    glGetIntegerv(GL_VIEWPORT,                     last_viewport);
    glGetIntegerv(GL_SCISSOR_BOX,                  last_scissor_box);
    glGetIntegerv(GL_BLEND_SRC_RGB,               &last_blend_src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB,               &last_blend_dst_rgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,             &last_blend_src_alpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA,             &last_blend_dst_alpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB,          &last_blend_eq_rgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA,        &last_blend_eq_alpha);
    last_enable_blend   = glIsEnabled(GL_BLEND);
    last_enable_cull    = glIsEnabled(GL_CULL_FACE);
    last_enable_depth   = glIsEnabled(GL_DEPTH_TEST);
    last_enable_scissor = glIsEnabled(GL_SCISSOR_TEST);

    /* Setup render state */
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);

    glViewport(0, 0, width, height);

    /* Orthographic projection matrix */
    {
        float proj[4][4];
        float L = 0.0f;
        float R = (float)width;
        float T = 0.0f;
        float B = (float)height;
        memset(proj, 0, sizeof(proj));
        proj[0][0] = 2.0f / (R - L);
        proj[1][1] = 2.0f / (T - B);
        proj[2][2] = -1.0f;
        proj[3][0] = -(R + L) / (R - L);
        proj[3][1] = -(T + B) / (T - B);
        proj[3][3] = 1.0f;

        glUseProgram(nk_gl3.prog);
        glUniform1i(nk_gl3.uniform_tex, 0);
        glUniformMatrix4fv(nk_gl3.uniform_proj, 1, GL_FALSE, &proj[0][0]);
    }

    /* Convert Nuklear draw commands to vertex buffer */
    {
        static const struct nk_draw_vertex_layout_element vertex_layout[] = {
            {NK_VERTEX_POSITION, NK_FORMAT_FLOAT,    offsetof(struct nk_gl3_vertex, position)},
            {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT,    offsetof(struct nk_gl3_vertex, uv)},
            {NK_VERTEX_COLOR,    NK_FORMAT_R8G8B8A8, offsetof(struct nk_gl3_vertex, col)},
            {NK_VERTEX_LAYOUT_END}
        };

        struct nk_convert_config cfg = {0};
        cfg.vertex_layout        = vertex_layout;
        cfg.vertex_size          = sizeof(struct nk_gl3_vertex);
        cfg.vertex_alignment     = NK_ALIGNOF(struct nk_gl3_vertex);
        cfg.tex_null             = nk_gl3.tex_null;
        cfg.circle_segment_count = 22;
        cfg.curve_segment_count  = 22;
        cfg.arc_segment_count    = 22;
        cfg.global_alpha         = 1.0f;
        cfg.shape_AA             = NK_ANTI_ALIASING_ON;
        cfg.line_AA              = NK_ANTI_ALIASING_ON;

        struct nk_buffer vbuf, ebuf;
        nk_buffer_init_default(&vbuf);
        nk_buffer_init_default(&ebuf);
        nk_convert(ctx, &nk_gl3.cmds, &vbuf, &ebuf, &cfg);

        /* Upload vertex and element data */
        glBindVertexArray(nk_gl3.vao);
        glBindBuffer(GL_ARRAY_BUFFER, nk_gl3.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, nk_gl3.ebo);

        glBufferData(GL_ARRAY_BUFFER,         (GLsizeiptr)nk_buffer_total(&vbuf), NULL, GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)nk_buffer_total(&ebuf), NULL, GL_STREAM_DRAW);

        {
            void *vertices = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
            void *elements = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
            if (vertices && elements) {
                memcpy(vertices, nk_buffer_memory_const(&vbuf), nk_buffer_total(&vbuf));
                memcpy(elements, nk_buffer_memory_const(&ebuf), nk_buffer_total(&ebuf));
            }
            glUnmapBuffer(GL_ARRAY_BUFFER);
            glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
        }

        /* Draw each command */
        {
            const nk_draw_index *offset = NULL;
            const struct nk_draw_command *cmd;
            nk_draw_foreach(cmd, ctx, &nk_gl3.cmds)
            {
                if (!cmd->elem_count) continue;
                GLint clip_x = (GLint)floorf(cmd->clip_rect.x);
                GLint clip_y = (GLint)floorf((float)height - (cmd->clip_rect.y + cmd->clip_rect.h));
                GLint clip_w = (GLint)ceilf(cmd->clip_rect.w);
                GLint clip_h = (GLint)ceilf(cmd->clip_rect.h);
                glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
                glScissor(clip_x, clip_y, clip_w, clip_h);
                glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
                offset += cmd->elem_count;
            }
        }

        nk_buffer_free(&vbuf);
        nk_buffer_free(&ebuf);
    }

    nk_clear(ctx);

    /* Restore GL state */
    glUseProgram((GLuint)last_prog);
    glBindTexture(GL_TEXTURE_2D, (GLuint)last_tex);
    glBindVertexArray((GLuint)last_vao);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)last_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)last_ebo);
    glBlendEquationSeparate((GLenum)last_blend_eq_rgb, (GLenum)last_blend_eq_alpha);
    glBlendFuncSeparate(
        (GLenum)last_blend_src_rgb, (GLenum)last_blend_dst_rgb,
        (GLenum)last_blend_src_alpha, (GLenum)last_blend_dst_alpha
    );
    if (last_enable_blend)   glEnable(GL_BLEND);   else glDisable(GL_BLEND);
    if (last_enable_cull)    glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);
    if (last_enable_depth)   glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
    if (last_enable_scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], last_scissor_box[2], last_scissor_box[3]);
}

void
nk_gl3_shutdown(struct nk_context *ctx)
{
    nk_free(ctx);
    nk_font_atlas_clear(&nk_gl3.atlas);
    glDeleteTextures(1, &nk_gl3.font_tex);
    glDeleteProgram(nk_gl3.prog);
    glDeleteShader(nk_gl3.vert_shader);
    glDeleteShader(nk_gl3.frag_shader);
    glDeleteBuffers(1, &nk_gl3.vbo);
    glDeleteBuffers(1, &nk_gl3.ebo);
    glDeleteVertexArrays(1, &nk_gl3.vao);
    nk_buffer_free(&nk_gl3.cmds);
    memset(&nk_gl3, 0, sizeof(nk_gl3));
}

#endif /* NK_GL3_IMPLEMENTATION */
#endif /* NK_GL3_H */
