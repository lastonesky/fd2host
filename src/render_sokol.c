/* render_sokol.c - sokol_gfx implementation of render.h.
 *
 * Pixel contract (render.h): the shared layer hands over 320x200 pixels that
 * are 32bpp BI_RGB, i.e. **BGRA in memory**. We upload them verbatim into an
 * RGBA8 texture and do the channel swizzle once, in the fragment shader, so
 * the per-frame CPU cost stays the same as the GDI path.
 *
 * Everything is sized from the render_desc: the quad covers the whole client
 * area, so the window can be resized freely (the GDI path draws at a fixed
 * integer scale). Sampling is NEAREST, which is what makes the output
 * pixel-identical to the GDI reference. */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "render.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"

#if defined(SOKOL_GLCORE)
#include <GL/gl.h>
/* Orientation self-check for the GL path (docs/PITFALLS.md §8-67). The window
 * content is *not* what --screenshot captures (that is the shared layer's
 * BMP), so a flipped present path used to be invisible to the frame diff.
 * With FD2_GL_READBACK=<n> every n-th frame reads the just-rendered default
 * framebuffer back and prints how many pixels match the source upright vs
 * vertically flipped; FD2_TESTPATTERN=1 replaces the guest frame with a
 * static high-contrast pattern (green ramps with the row, white block
 * top-left, red block bottom-right) so the answer has no animation noise.
 * Off by default: the cost is one getenv per frame. */
static void gl_readback_check(const uint32_t *bgra, int w, int h)
{
    static long frames;
    const char *env = getenv("FD2_GL_READBACK");
    long interval = env ? atol(env) : 0;
    int fw, fh, s = 3;                  /* the host's 3x window */
    unsigned char *px;
    long up = 0, dn = 0, n = 0;
    int x, y;

    if (interval <= 0)
        return;
    if (++frames % interval)
        return;
    fw = w * s;
    fh = h * s;
    px = malloc((size_t)fw * fh * 4);
    if (!px)
        return;
    glReadBuffer(GL_BACK);
    glFinish();
    glReadPixels(0, 0, fw, fh, GL_RGBA, GL_UNSIGNED_BYTE, px);
    for (y = 0; y < fh; y += s) {
        for (x = 0; x < fw; x += s) {
            /* the shader writes the true (r, g, b) for a source 0x00RRGGBB */
            uint32_t su = bgra[(h - 1 - y / s) * w + x / s];   /* upright   */
            uint32_t sd = bgra[(y / s) * w + x / s];           /* flipped   */
            const unsigned char *p = &px[((size_t)y * fw + x) * 4];

            if (p[0] != ((su >> 16) & 0xFF) || p[1] != ((su >> 8) & 0xFF) ||
                p[2] != (su & 0xFF))
                up++;
            if (p[0] != ((sd >> 16) & 0xFF) || p[1] != ((sd >> 8) & 0xFF) ||
                p[2] != (sd & 0xFF))
                dn++;
            n++;
        }
    }
    printf("sokol: GL readback f%ld: upright mismatches=%ld/%ld, "
           "flipped mismatches=%ld/%ld\n", frames, up, n, dn, n);
    free(px);
}
#endif

/* ------------------------------------------------------------- shaders ----
 *
 * D3D11 compiles HLSL source at runtime (d3dcompiler_47.dll, loaded on
 * demand, system built-in on Win8+). GL backends need GLSL source instead -
 * sokol cannot cross-compile, so both variants live here and the right one is
 * picked from sg_query_backend(). macOS/Metal would need a third one (MSL),
 * which is what sokol-shdc is for; we only add it when macOS becomes a
 * target (§13.1 cost list). */
static const char *HLSL_VS =
    "struct vs_in  { float2 pos : POSITION; float2 uv : TEXCOORD0; };\n"
    "struct vs_out { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
    "vs_out main(vs_in i) {\n"
    "    vs_out o;\n"
    "    o.pos = float4(i.pos, 0.0, 1.0);\n"
    "    o.uv  = i.uv;\n"
    "    return o;\n"
    "}\n";

static const char *HLSL_FS =
    "Texture2D tex : register(t0);\n"
    "SamplerState smp : register(s0);\n"
    "float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {\n"
    "    float4 c = tex.Sample(smp, uv);\n"
    "    /* source bytes are BGRA (32bpp BI_RGB) sampled as RGBA -> swizzle */\n"
    "    return float4(c.b, c.g, c.r, 1.0);\n"
    "}\n";

static const char *GLSL_VS =
    "#version 330\n"
    "layout(location=0) in vec2 pos;\n"
    "layout(location=1) in vec2 uv;\n"
    "out vec2 uvv;\n"
    "void main() {\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "    uvv = uv;\n"                 /* v is flipped in the fragment stage */
    "}\n";

static const char *GLSL_FS =
    "#version 330\n"
    "in vec2 uvv;\n"
    "out vec4 frag;\n"
    "uniform sampler2D tex_smp;\n"
    "void main() {\n"
    /* NO v flip: sokol-gfx uploads image rows as-is, so texel v=0 is the first
     * uploaded row (the top scanline) exactly like D3D11's convention - the
     * quad already maps uv.y=0 to the top. Flipping here made the GL window
     * 180-degrees upside down while --screenshot (shared layer) stayed right;
     * see docs/PITFALLS.md §8-67. */
    "    vec4 c = texture(tex_smp, uvv);\n"
    "    frag = vec4(c.b, c.g, c.r, 1.0);\n"
    "}\n";

static const float QUAD[6 * 4] = {
    /* x, y, u, v   (v = 0 is the top row on D3D) */
    -1.0f, -1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 1.0f, 1.0f,
    -1.0f,  1.0f, 0.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 1.0f,
     1.0f,  1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 0.0f,
};

static sg_image   g_img;
static sg_view    g_view;
static sg_sampler g_smp;
static sg_buffer  g_vbuf;
static sg_shader  g_shd;
static sg_pipeline g_pipe;
static int        g_ok;
static int        g_w = 320, g_h = 200;

static const char *backend_name(sg_backend b)
{
    switch (b) {
    case SG_BACKEND_GLCORE:      return "GLCORE";
    case SG_BACKEND_GLES3:       return "GLES3";
    case SG_BACKEND_D3D11:       return "D3D11";
    case SG_BACKEND_METAL_MACOS: return "METAL";
    case SG_BACKEND_WGPU:        return "WGPU";
    case SG_BACKEND_VULKAN:      return "VULKAN";
    default:                     return "DUMMY/unknown";
    }
}

int render_init(const render_desc *desc)
{
    sg_environment env;
    sg_image_desc id;
    sg_buffer_desc bd;
    sg_shader_desc sd;
    sg_pipeline_desc pd;
    const char *vs, *fs;

    if (desc && desc->logical_w > 0 && desc->logical_h > 0) {
        g_w = desc->logical_w;
        g_h = desc->logical_h;
    }

    env = sglue_environment();
    {
        sg_desc gd;
        memset(&gd, 0, sizeof gd);
        gd.environment = env;
        gd.logger.func = slog_func;
        sg_setup(&gd);
    }
    if (sg_query_backend() == SG_BACKEND_DUMMY) {
        printf("sokol: sg_setup failed\n");
        return -1;
    }

    /* 320x200 texture, rewritten every frame with the guest framebuffer */
    memset(&id, 0, sizeof id);
    id.type            = SG_IMAGETYPE_2D;
    id.usage.dynamic_update = true;
    id.width           = g_w;
    id.height          = g_h;
    id.pixel_format    = SG_PIXELFORMAT_RGBA8;
    id.num_mipmaps     = 1;
    id.label           = "fd2 framebuffer";
    g_img = sg_make_image(&id);

    {
        sg_view_desc vd;
        memset(&vd, 0, sizeof vd);
        vd.texture.image = g_img;
        g_view = sg_make_view(&vd);
    }
    {
        sg_sampler_desc smd;
        memset(&smd, 0, sizeof smd);
        /* NEAREST is required for pixel parity with the GDI reference */
        smd.min_filter = SG_FILTER_NEAREST;
        smd.mag_filter = SG_FILTER_NEAREST;
        smd.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        smd.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        smd.label  = "fd2 nearest";
        g_smp = sg_make_sampler(&smd);
    }

    memset(&bd, 0, sizeof bd);
    bd.usage.vertex_buffer = true;
    bd.usage.immutable     = true;
    bd.data.ptr = QUAD;
    bd.data.size = sizeof QUAD;
    bd.label = "fd2 quad";
    g_vbuf = sg_make_buffer(&bd);

    /* pick the shader language the active backend can actually compile */
    if (sg_query_backend() == SG_BACKEND_D3D11 ||
        sg_query_backend() == SG_BACKEND_METAL_MACOS) {
        vs = HLSL_VS; fs = HLSL_FS;          /* D3D11 compiles HLSL source */
    } else {
        vs = GLSL_VS; fs = GLSL_FS;          /* GL backends need GLSL      */
    }

    memset(&sd, 0, sizeof sd);
    sd.vertex_func.source = vs;
    sd.vertex_func.entry  = "main";
    sd.fragment_func.source = fs;
    sd.fragment_func.entry  = "main";
    sd.attrs[0].base_type      = SG_SHADERATTRBASETYPE_FLOAT;
    sd.attrs[0].glsl_name       = "pos";
    sd.attrs[0].hlsl_sem_name   = "POSITION";
    sd.attrs[0].hlsl_sem_index  = 0;
    sd.attrs[1].base_type      = SG_SHADERATTRBASETYPE_FLOAT;
    sd.attrs[1].glsl_name       = "uv";
    sd.attrs[1].hlsl_sem_name   = "TEXCOORD";
    sd.attrs[1].hlsl_sem_index  = 0;
    sd.views[0].texture.stage       = SG_SHADERSTAGE_FRAGMENT;
    sd.views[0].texture.image_type  = SG_IMAGETYPE_2D;
    sd.views[0].texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
    sd.views[0].texture.hlsl_register_t_n = 0;
    sd.samplers[0].stage      = SG_SHADERSTAGE_FRAGMENT;
    sd.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;
    sd.samplers[0].hlsl_register_s_n = 0;
    sd.texture_sampler_pairs[0].stage      = SG_SHADERSTAGE_FRAGMENT;
    sd.texture_sampler_pairs[0].view_slot  = 0;
    sd.texture_sampler_pairs[0].sampler_slot = 0;
    sd.texture_sampler_pairs[0].glsl_name  = "tex_smp";
    sd.label = "fd2 blit";
    g_shd = sg_make_shader(&sd);

    memset(&pd, 0, sizeof pd);
    pd.shader                 = g_shd;
    pd.layout.buffers[0].stride = 16;
    pd.layout.attrs[0].buffer_index = 0;
    pd.layout.attrs[0].offset = 0;
    pd.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
    pd.layout.attrs[1].buffer_index = 0;
    pd.layout.attrs[1].offset = 8;
    pd.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT2;
    pd.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pd.cull_mode      = SG_CULLMODE_NONE;
    pd.colors[0].pixel_format = env.defaults.color_format;
    pd.sample_count   = env.defaults.sample_count;
    pd.label          = "fd2 blit";
    g_pipe = sg_make_pipeline(&pd);

    g_ok = 1;
    printf("sokol: backend=%s image=%dx%d img=%d view=%d sampler=%d vbuf=%d shader=%d pip=%d\n",
           backend_name(sg_query_backend()), g_w, g_h,
           g_img.id, g_view.id, g_smp.id, g_vbuf.id, g_shd.id, g_pipe.id);
    return 0;
}

void render_present(const uint32_t *bgra, int w, int h)
{
    sg_image_data data;
    sg_pass pass;
    sg_bindings binds;
#if defined(SOKOL_GLCORE)
    static uint32_t pattern[320 * 200];

    if (getenv("FD2_TESTPATTERN")) {
        /* Diagnostic: a static pattern with an unmistakable orientation -
         * green ramps with the source row, a white block at the top-left and
         * a red block at the bottom-right. FD2_GL_READBACK then tells
         * orientation without any animation noise. */
        int i, r, c;
        for (i = 0; i < 320 * 200; i++)
            pattern[i] = ((uint32_t)((i / 320) * 255 / 199) << 8) | 0x00000080u;
        for (r = 0; r < 8; r++)
            for (c = 0; c < 8; c++) {
                pattern[r * 320 + c] = 0x00FFFFFFu;                  /* white, TL */
                pattern[(199 - r) * 320 + (319 - c)] = 0x000000FFu;  /* red, BR   */
            }
        bgra = pattern;
        w = 320;
        h = 200;
    }
#endif

    if (!g_ok || !bgra)
        return;

    memset(&data, 0, sizeof data);
    data.mip_levels[0].ptr  = bgra;
    data.mip_levels[0].size = (size_t)w * (size_t)h * 4;
    sg_update_image(g_img, &data);

    memset(&pass, 0, sizeof pass);
    pass.action.colors[0].load_action  = SG_LOADACTION_CLEAR;
    pass.action.colors[0].clear_value.r = 0.0f;
    pass.action.colors[0].clear_value.g = 0.0f;
    pass.action.colors[0].clear_value.b = 0.0f;
    pass.action.colors[0].clear_value.a = 1.0f;
    pass.swapchain = sglue_swapchain();
    if (pass.swapchain.invalid)
        return;

    {
        static int logged;
        if (!logged) {
            logged = 1;
            printf("sokol: swapchain=%dx%d sample=%d color_format=%d (framebuffer %dx%d)\n",
                   pass.swapchain.width, pass.swapchain.height,
                   pass.swapchain.sample_count, (int)pass.swapchain.color_format,
                   g_w, g_h);
        }
    }
    sg_begin_pass(&pass);

    memset(&binds, 0, sizeof binds);
    binds.vertex_buffers[0] = g_vbuf;
    binds.views[0]          = g_view;
    binds.samplers[0]       = g_smp;

    sg_apply_pipeline(g_pipe);
    sg_apply_bindings(&binds);
    sg_draw(0, 6, 1);
    sg_end_pass();
#if defined(SOKOL_GLCORE)
    gl_readback_check(bgra, w, h);
#endif
    sg_commit();
}

void render_resize(int w, int h)
{
    /* the swapchain follows the window automatically; nothing to do */
    (void)w; (void)h;
}

void render_shutdown(void)
{
    if (g_ok) {
        sg_shutdown();
        g_ok = 0;
    }
}

const char *render_name(void)
{
    return "sokol";
}
