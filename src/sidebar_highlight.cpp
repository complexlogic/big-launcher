#include <string>
#include <cmath>
#include <fmt/base.h>
#if FMT_VERSION >= 120200
#include <fmt/format.h>
#else
#include <fmt/core.h>
#endif

#include "sidebar_highlight.hpp"
#include "config.hpp"
#include "image.hpp"
#include "layout.hpp"


#define HIGHLIGHT_FORMAT "<svg viewBox=\"0 0 {} {}\"><rect x=\"0\" width=\"{}\" height=\"{}\" rx=\"{}\" fill=\"#{:02x}{:02x}{:02x}\"/></svg>"
#define format_highlight(w, h, cx, color) fmt::format(HIGHLIGHT_FORMAT, w, h, w, h, cx, color.r, color.g, color.b)

extern BL::Config config;

void BL::SidebarHighlight::render_surface(BL::SVGRasterizer &rasterizer, float w, float h, int cx)
{
    std::string highlight_buffer = format_highlight(static_cast<int>(w), static_cast<int>(h), cx, config.sidebar_highlight_color);
    SDL_Surface *highlight = rasterizer.rasterize_svg(highlight_buffer, -1, -1);
#ifdef __unix
    constexpr
#endif
    Uint8 alpha = static_cast<Uint8>(std::round((255.f * BL::SHADOW_ALPHA)));

    float max_blur = BL::SHADOW_BLUR_SLOPE*h + BL::SHADOW_BLUR_INTERCEPT;
    float max_y_offset = BL::SHADOW_OFFSET_SLOPE*h + BL::SHADOW_OFFSET_INTERCEPT;
    shadow_offset = std::round(max_blur * 2.f);
    std::vector<BoxShadow> box_shadows = {
        {0, std::round(max_y_offset / 2.f), std::round(max_blur / 2.f), alpha},
        {0, std::round(max_y_offset),     std::round(max_blur),        alpha}
    };


    shadow_offset = std::round(max_blur * 2.0f);
    surface = BL::create_shadow(highlight, box_shadows, shadow_offset);
    SDL_Rect tmp = {static_cast<int>(shadow_offset), static_cast<int>(shadow_offset), highlight->w, highlight->h};
    SDL_BlitSurface(highlight, nullptr, surface, &tmp);
    BL::free_surface(highlight);
    set_w(w);
    set_h(h);
}

void BL::SidebarHighlight::render_texture()
{
    texture = renderer->create_texture(*surface);
    BL::free_surface(surface);
    surface = nullptr;
}
