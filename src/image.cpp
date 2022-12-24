#include <math.h>
#include <algorithm>
#include <SDL.h>
#include <SDL_image.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include <lconfig.h>
#include "main.hpp"
#include "image.hpp"
#include "util.hpp"
#define NANOSVG_IMPLEMENTATION
#include "external/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "external/nanosvgrast.h"
#include "external/fast_gaussian_blur_template.h"

SDL_Surface *rasterize_svg_image(NSVGimage *image, int w, int h);

NSVGrasterizer *rasterizer = NULL;
extern char *executable_dir;

SDL_Surface *load_surface(std::string &file)
{
    SDL_Surface *img = NULL;
    SDL_Surface *out = NULL;
    img = IMG_Load(file.c_str());
    if (img == NULL) {
        spdlog::error("Could not load image from {}", file);
        spdlog::error("SDL Error: {}", IMG_GetError());
        return out;
    }

    // Convert the loaded surface if different pixel format
    if (img->format->format != SDL_PIXELFORMAT_ARGB8888) {
        out = SDL_CreateRGBSurfaceWithFormat(0,
                  img->w,
                  img->h,
                  32,
                  SDL_PIXELFORMAT_ARGB8888
              );
        Uint32 color = SDL_MapRGBA(out->format, 0xFF, 0xFF, 0xFF, 0);
        SDL_FillRect(out, NULL, color);
        SDL_BlitSurface(img, NULL, out, NULL);
        free_surface(img);
    }
    else
        out = img;

    return out;
}

// A function to initalize SVG rasterization
int init_svg()
{
    rasterizer = nsvgCreateRasterizer();
    if (rasterizer == NULL) {
        spdlog::critical("Could not initialize SVG rasterizer");
        return 1;
    }
    return 0;
}

// A function to quit the SVG subsystem
void quit_svg()
{
    nsvgDeleteRasterizer(rasterizer);
}

SDL_Surface *rasterize_svg_from_file(const std::string &file, int w, int h)
{
    NSVGimage *image = nsvgParseFromFile((char*) file.c_str(), "px", 96.0f);
    if (image == NULL) {
        spdlog::error("Could not load SVG");
        return NULL;
    }
    return rasterize_svg_image(image, w, h);
}

// A function to rasterize an SVG from an existing text buffer
SDL_Surface *rasterize_svg(const std::string &buffer, int w, int h)
{
    NSVGimage *image = nsvgParse((char*) buffer.c_str(), "px", 96.0f);
    if (image == NULL) {
        spdlog::error("Could not parse SVG");
        return NULL;
    }
    return rasterize_svg_image(image, w, h);
}

SDL_Surface *rasterize_svg_image(NSVGimage *image, int w, int h)
{
    unsigned char *pixel_buffer = NULL;
    int width, height, pitch;
    float scale;

    // Calculate scaling and dimensions
    if (w == -1 && h == -1) {
        scale = 1.0f;
        width = (int) image->width;
        height = (int) image->height;
    }
    else if (w == -1 && h != -1) {
        scale = (float) h / (float) image->height;
        width = (int) ceil((double) image->width * (double) scale);
        height = h;
    }
    else if (w != -1 && h == -1) {
        scale = (float) w / (float) image->width;
        width = w;
        height = (int) ceil((double) image->height * (double) scale);
    }
    else {
        scale = (float) w / (float) image->width;
        width = w;
        height = h;
    }
    
    // Allocate memory
    pitch = 4*width;
    pixel_buffer = (unsigned char*) malloc(4*width*height);
    if (pixel_buffer == NULL) {
        spdlog::error("Could not alloc SVG pixel buffer");
        return NULL;
    }

    // Rasterize image
    nsvgRasterize(rasterizer, image, 0, 0, scale, pixel_buffer, width, height, pitch);
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixel_buffer,
                               width,
                               height,
                               32,
                               pitch,
                               COLOR_MASKS
                           );
    nsvgDelete(image);
    return surface;
}

int Font::load(const char *file, int height)
{
    std::string font_path;
    if (!find_file<TYPE_FONT>(font_path, file)) {
        spdlog::critical("Could not locate font '{}'", file);
        quit(EXIT_FAILURE);
    }

    font = TTF_OpenFont(font_path.c_str(), height);
    if (!font) {
        spdlog::error("Could not open font\nSDL Error: {}\n", TTF_GetError());
        return 1;
    }
    return 0;
}

SDL_Surface *Font::render_text(const char *text, SDL_Rect *src_rect, SDL_Rect *dst_rect, int max_width)
{
    SDL_Surface *surface = NULL;
    int width;
    static std::string truncated_text;
    char *out_text = const_cast<char*>(text);
    TTF_SizeUTF8(font, text, &width, NULL);
    if (width > max_width) {
        utf8_truncate(text, truncated_text, width, max_width);
        out_text = truncated_text.data();
    }

    surface = TTF_RenderUTF8_Blended(font, out_text, color);
    if (surface == NULL) {
        spdlog::error("Could not render text '{}'", text);
        return surface;
    } 
    
    if (src_rect != NULL) {
        int y_asc_max = 0;
        int y_dsc_max = 0;
        int y_asc, y_dsc;
        int bytes = 0;
        char *p = out_text;
        Uint16 code_point;
        while (*p != '\0') {
            code_point = get_unicode_code_point(p, bytes);
            TTF_GlyphMetrics(font, 
                code_point, 
                NULL, 
                NULL, 
                &y_dsc, 
                &y_asc,
                NULL
            );
            if (y_asc > y_asc_max)
                y_asc_max = y_asc;
            if (y_dsc < y_dsc_max)
                y_dsc_max = y_dsc;
            p += bytes;
        }
        src_rect->x = 0;
        src_rect->y = TTF_FontAscent(font) - y_asc_max;
        src_rect->w = surface->w;
        src_rect->h = y_asc_max - y_dsc_max;
    }
    if (dst_rect != NULL && surface != NULL) {
        dst_rect->w = surface->w;
        dst_rect->h = (src_rect != NULL) ? src_rect->h : surface->h;
    }
    return surface;
}


SDL_Surface* create_shadow(SDL_Surface *in, const std::vector<BoxShadow> &box_shadows, int s_offset)
{
    float max_radius = 0.0f;
    std::for_each(box_shadows.begin(), 
        box_shadows.end(), 
        [&](const BoxShadow &bs){if(bs.radius > max_radius) max_radius = bs.radius;}
    );
    int padding = 2 * (int) ceil(max_radius);

    SDL_Color mod;
    SDL_GetSurfaceColorMod(in, &mod.r, &mod.g, &mod.b);
    SDL_GetSurfaceAlphaMod(in, &mod.a);
    SDL_SetSurfaceColorMod(in, 0, 0, 0);

    // Set up shadow
    SDL_Surface *shadow = SDL_CreateRGBSurfaceWithFormat(0, 
                              in->w + 2*s_offset, 
                              in->h + 2*s_offset, 
                              32,
                              SDL_PIXELFORMAT_ARGB8888
                          );
    Uint32 color = SDL_MapRGBA(shadow->format, 0, 0, 0, 0);
    SDL_FillRect(shadow, NULL, color);

    // Set up alpha mask
    SDL_Surface *alpha_mask = SDL_CreateRGBSurfaceWithFormat(0, 
                                  in->w + 2*(padding + s_offset), 
                                  in->h + 2*(padding + s_offset), 
                                  32,
                                  SDL_PIXELFORMAT_ARGB8888
                              );
    SDL_Rect alpha_mask_rect = {padding + s_offset, padding + s_offset, in->w, in->h};
    
    Uint8 *in_pixels;
    Uint8 *out_pixels;
    Uint8 *out_pixels0; // fast guassian blur swaps the address of the in/out pointers, need to rememeber it
    SDL_Surface *tmp;
    SDL_Rect src_rect;
    SDL_Rect dst_rect;
    int w, h;
    for (const BoxShadow &bs : box_shadows) {

        // Make alpha mask
        SDL_FillRect(alpha_mask, NULL, color);
        SDL_SetSurfaceAlphaMod(in, bs.alpha);
        SDL_BlitSurface(in, NULL, alpha_mask, &alpha_mask_rect);

        // Blur alpha mask
        in_pixels = (Uint8*) alpha_mask->pixels;
        out_pixels = (Uint8*) malloc(alpha_mask->w*alpha_mask->h*4);
        out_pixels0 = out_pixels;
        fast_gaussian_blur<Uint8>(in_pixels, out_pixels, alpha_mask->w, alpha_mask->h, 4, bs.radius);
        tmp = SDL_CreateRGBSurfaceFrom(out_pixels,
                  alpha_mask->w,
                  alpha_mask->h,
                  32,
                  alpha_mask->w*4,
                  COLOR_MASKS
              );

        // Composit onto shadow surface
        w = alpha_mask->w - 2*padding - abs(bs.x_offset);
        h = alpha_mask->w - 2*padding - abs(bs.y_offset);
        src_rect = {
            (bs.x_offset >= 0) ? padding : padding + bs.x_offset, 
            (bs.y_offset >= 0) ? padding : padding + bs.y_offset, 
            w,
            h
        };
        dst_rect = {
            (bs.x_offset > 0) ? bs.x_offset : 0,
            (bs.y_offset > 0) ? bs.y_offset : 0,
            w,
            h
        };
        SDL_BlitSurface(tmp, &src_rect, shadow, &dst_rect);
        free(out_pixels0);
        SDL_FreeSurface(tmp);
    }

    free_surface(alpha_mask);

    SDL_SetSurfaceColorMod(in, mod.r, mod.g, mod.b);
    SDL_SetSurfaceAlphaMod(in, mod.a);

    return shadow;
}