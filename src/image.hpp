#pragma once

#include <string>
#include <vector>
#include "external/nanosvg.h"
#include <SDL_ttf.h>

// Color masking bit logic
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define RMASK 0xff000000
#define GMASK 0x00ff0000
#define BMASK 0x0000ff00
#define AMASK 0x000000ff
#else
#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000
#endif
#define COLOR_MASKS RMASK, GMASK, BMASK, AMASK
#define STR(x) (const char*) x.c_str()

class Font {
    private:
        TTF_Font *font = nullptr;
        SDL_Color color = { 0xFF, 0xFF, 0xFF, 0xFF };

    public:
        int load(const char *path, int height);
        SDL_Surface *render_text(const std::string &text, SDL_Rect *src_rect, SDL_Rect *dst_rect, int max_width);
};

struct BoxShadow{
    int x_offset;
    int y_offset;
    float radius;
    Uint8 alpha;
};

#define free_surface(s)               \
if (s != NULL) {                      \
    if(s->flags & SDL_PREALLOC) {     \
        free(s->pixels);              \
        s->pixels = NULL;             \
    }                                 \
    SDL_FreeSurface(s);               \
}

SDL_Surface *load_surface(std::string &file);
int init_svg();
void quit_svg();
SDL_Surface *rasterize_svg_from_file(const std::string &file, int w, int h);
SDL_Surface *rasterize_svg(const std::string &buffer, int w, int h);
SDL_Surface *rasterize_svg_image(NSVGimage *image, int w, int h);
SDL_Surface* create_shadow(SDL_Surface *in, const std::vector<BoxShadow> &box_shadows, int s_offset);
