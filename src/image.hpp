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
#define ERROR_FORMAT "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?> <svg version=\"1.1\" id=\"Ebene_1\" x=\"0px\" y=\"0px\" width=\"140.50626\" height=\"140.50626\" viewBox=\"0 0 140.50625 140.50626\" xml:space=\"preserve\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:svg=\"http://www.w3.org/2000/svg\"><defs id=\"defs17\" /> <g id=\"layer1\" transform=\"matrix(1.0014475,0,0,0.99627733,-130.32833,-78.42333)\" style=\"fill:#ffffff\" /><g id=\"g4\" transform=\"matrix(0.32150107,0,0,0.32150107,-27.692492,-70.907371)\"> <path style=\"fill:#ffffff\" d=\"m 326.039,513.568 h -69.557 v -9.441 c 0,-10.531 2.12,-19.876 6.358,-28.034 4.239,-8.156 13.165,-18.527 26.783,-31.117 l 12.33,-11.176 c 7.322,-6.678 12.684,-12.973 16.09,-18.882 3.4,-5.907 5.105,-11.817 5.105,-17.727 0,-8.99 -3.084,-16.022 -9.248,-21.098 -6.166,-5.073 -14.773,-7.611 -25.819,-7.611 -10.405,0 -21.646,2.152 -33.719,6.455 -12.075,4.305 -24.663,10.693 -37.765,19.171 v -60.5 c 15.541,-5.395 29.735,-9.375 42.582,-11.946 12.843,-2.568 25.241,-3.854 37.186,-3.854 31.342,0 55.232,6.392 71.678,19.171 16.439,12.783 24.662,31.439 24.662,55.973 0,12.591 -2.506,23.862 -7.516,33.815 -5.008,9.956 -13.553,20.649 -25.625,32.08 l -12.332,10.983 c -8.736,7.966 -14.451,14.354 -17.148,19.171 -2.697,4.817 -4.045,10.115 -4.045,15.896 z m -69.557,28.517 h 69.557 v 68.593 h -69.557 z\" id=\"path2\" /> </g> <circle style=\"fill:#f44336;stroke-width:0.321501\" cx=\"70.253128\" cy=\"70.253128\" id=\"circle6\" r=\"70.253128\" /> <g id=\"g12\" transform=\"matrix(0.32150107,0,0,0.32150107,-26.147362,-70.907371)\"> <rect x=\"267.16199\" y=\"307.978\" transform=\"matrix(0.7071,-0.7071,0.7071,0.7071,-222.6202,340.6915)\" style=\"fill:#ffffff\" width=\"65.544998\" height=\"262.17999\" id=\"rect8\" /> <rect x=\"266.98801\" y=\"308.15302\" transform=\"matrix(0.7071,0.7071,-0.7071,0.7071,398.3889,-83.3116)\" style=\"fill:#ffffff\" width=\"65.543999\" height=\"262.17899\" id=\"rect10\" /> </g> <g id=\"g179\" transform=\"matrix(0.32150107,0,0,0.32150107,-27.692492,-70.907371)\"> <path style=\"fill:#ffffff\" d=\"m 326.039,513.568 h -69.557 v -9.441 c 0,-10.531 2.12,-19.876 6.358,-28.034 4.239,-8.156 13.165,-18.527 26.783,-31.117 l 12.33,-11.176 c 7.322,-6.678 12.684,-12.973 16.09,-18.882 3.4,-5.907 5.105,-11.817 5.105,-17.727 0,-8.99 -3.084,-16.022 -9.248,-21.098 -6.166,-5.073 -14.773,-7.611 -25.819,-7.611 -10.405,0 -21.646,2.152 -33.719,6.455 -12.075,4.305 -24.663,10.693 -37.765,19.171 v -60.5 c 15.541,-5.395 29.735,-9.375 42.582,-11.946 12.843,-2.568 25.241,-3.854 37.186,-3.854 31.342,0 55.232,6.392 71.678,19.171 16.439,12.783 24.662,31.439 24.662,55.973 0,12.591 -2.506,23.862 -7.516,33.815 -5.008,9.956 -13.553,20.649 -25.625,32.08 l -12.332,10.983 c -8.736,7.966 -14.451,14.354 -17.148,19.171 -2.697,4.817 -4.045,10.115 -4.045,15.896 z m -69.557,28.517 h 69.557 v 68.593 h -69.557 z\" id=\"path177\" /> </g><circle style=\"fill:#f44336;stroke-width:0.321501\" cx=\"70.253128\" cy=\"70.253128\" id=\"circle181\" r=\"70.253128\" /><g id=\"g187\" transform=\"matrix(0.32150107,0,0,0.32150107,-26.147362,-70.907371)\"> <rect x=\"267.16199\" y=\"307.978\" transform=\"matrix(0.7071,-0.7071,0.7071,0.7071,-222.6202,340.6915)\" style=\"fill:#ffffff\" width=\"65.544998\" height=\"262.17999\" id=\"rect183\" /> <rect x=\"266.98801\" y=\"308.15302\" transform=\"matrix(0.7071,0.7071,-0.7071,0.7071,398.3889,-83.3116)\" style=\"fill:#ffffff\" width=\"65.543999\" height=\"262.17899\" id=\"rect185\" /> </g></svg>"

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
if (s != nullptr) {                      \
    if(s->flags & SDL_PREALLOC) {     \
        free(s->pixels);              \
        s->pixels = nullptr;             \
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
