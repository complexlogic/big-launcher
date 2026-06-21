#include <string>
#include <cmath>
#include <fmt/base.h>
#if FMT_VERSION >= 120200
#include <fmt/format.h>
#else
#include <fmt/core.h>
#endif

#include "image.hpp"
#include "layout.hpp"
#include "renderer.hpp"
#include "menu_highlight.hpp"
#include "config.hpp"

#define MENU_HIGHLIGHT_FORMAT "<svg viewBox=\"0 0 {} {}\"><rect width=\"100%\" height=\"100%\" rx=\"{}\" fill=\"#{:02x}{:02x}{:02x}\" /><rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\" rx=\"{}\"  fill=\"#{:02x}{:02x}{:02x}\"/></svg>"
#define format_menu_highlight(w, h, color, mask_color, w_inner, h_inner, t, rx_outter, rx_inner) fmt::format(MENU_HIGHLIGHT_FORMAT, w, h, rx_outter, color.r, color.g, color.b, t, t, w_inner, h_inner, rx_inner, mask_color.r, mask_color.g, mask_color.b)

extern BL::Config config;

namespace BL {
    constexpr float SHADOW_ALPHA_HIGHLIGHT = 0.6f;
}

void BL::MenuHighlight::render_surface(BL::SVGRasterizer &rasterizer, int w, int h, int t)
{
    int w_inner = w - 2*t;
    int h_inner = h - 2*t;
    int rx_outter = (int) std::round((float) w * MENU_HIGHLIGHT_RX);
    int rx_inner = rx_outter / 2;

    // Render highlight
    SDL_Color mask_color = config.menu_highlight_color;
    mask_color.b & 0x01 ? mask_color.b-- : mask_color.b++;
    std::string format = format_menu_highlight(w,
                             h,
                             config.menu_highlight_color,
                             mask_color,
                             w_inner,
                             h_inner,
                             t,
                             rx_outter,
                             rx_inner
                         );
    SDL_Surface *highlight = rasterizer.rasterize_svg(format, -1, -1);

    // Render shadow
#ifdef __unix__
    constexpr
#endif
    Uint8 alpha = static_cast<Uint8>(std::round(255.f * BL::SHADOW_ALPHA_HIGHLIGHT));
    float f_h = static_cast<float>(h);
    float max_blur = BL::SHADOW_BLUR_SLOPE*f_h+ BL::SHADOW_BLUR_INTERCEPT;
    shadow_offset = std::round(max_blur * 2.0f);
    float max_y_offset = BL::SHADOW_OFFSET_SLOPE*f_h + BL::SHADOW_OFFSET_INTERCEPT;
    std::vector<BoxShadow> box_shadows = {
        {0, std::round(max_y_offset / 2.f), std::round(max_blur / 2.f), alpha},
        {0, std::round(max_y_offset),     std::round(max_blur),        alpha}
    };

    surface = BL::create_shadow(highlight, box_shadows, shadow_offset);
    SDL_Rect blit_rect = {
        static_cast<int>(std::round(shadow_offset)),
        static_cast<int>(std::round(shadow_offset)),
        highlight->w,
        highlight->h
    };
    SDL_BlitSurface(highlight, nullptr, surface, &blit_rect);
    BL::free_surface(highlight);
    Uint32 key = SDL_MapSurfaceRGBA(surface, mask_color.r, mask_color.g, mask_color.b, 0xFF);
    SDL_SetSurfaceColorKey(surface, true, key);
    set_w(static_cast<float>(w));
    set_h(f_h);
}


void BL::MenuHighlight::render_texture()
{
    texture = renderer->create_texture(*surface);
}
