#include <math.h>
#include <SDL.h>
#include <SDL_image.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "image.hpp"
#include "util.hpp"
#define NANOSVG_IMPLEMENTATION
#include "external/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "external/nanosvgrast.h"

SDL_Surface *rasterize_svg_image(NSVGimage *image, int w, int h);

NSVGrasterizer *rasterizer = NULL;

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

    // Convert the loaded surface if scaling is required or different pixel format
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
    else {
        out = img;
    }

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

Font::Font()
{
    font = NULL;
    color = {0xFF, 0xFF, 0xFF, 0xFF};
}

Font::~Font()
{
    if (font != NULL) {
        TTF_CloseFont(font);
        font = NULL;
    }
}


int Font::load(const char *path, int height)
{
    font = TTF_OpenFont(path, height);
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
    char *truncated_text = NULL;
    char *out_text = (char*) text;
    TTF_SizeUTF8(font, text, &width, NULL);
    if (width > max_width) {
        fmt::print("Width {} excceds max {}\n", width, max_width);
        copy_string_alloc(&truncated_text, text);
        utf8_truncate(truncated_text, width, max_width);
        out_text = truncated_text;
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
    free(truncated_text);
    return surface;
}