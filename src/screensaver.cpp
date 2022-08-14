#include <SDL.h>
#include "screensaver.hpp"
#include "image.hpp"
#include "util.hpp"

extern Ticks ticks;
extern Config config;

void Screensaver::render_surface(int w, int h)
{
    surface = SDL_CreateRGBSurfaceWithFormat(0, 
                  w, 
                  h, 
                  32,
                  SDL_PIXELFORMAT_ARGB8888
              );
    Uint32 color = SDL_MapRGBA(surface->format, 0, 0, 0, 0xFF);
    SDL_FillRect(surface, NULL, color);
    opacity_change_rate = (float) config.screensaver_intensity / (float) SCREENSAVER_TRANSITION_TIME;
}

void Screensaver::render_texture(SDL_Renderer *renderer)
{
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    free_surface(surface);
    surface = NULL;
}

void Screensaver::update()
{
    if (!active) {
        if (ticks.last_input - ticks.main > config.screensaver_idle_time) {
            active = true;
            transitioning = true;
            current_ticks = ticks.main;
            SDL_SetTextureAlphaMod(texture, 0);
        }
    }
    else {
        if (transitioning) {
            float delta = ((float) (ticks.main - current_ticks)) * opacity_change_rate;
            opacity += delta;
            Uint8 op = (Uint8) std::round(opacity);
            if (op >= config.screensaver_intensity) {
                op = config.screensaver_intensity;
                transitioning = false;
                opacity = 0.f;
            }
            SDL_SetTextureAlphaMod(texture, op);
            current_ticks = ticks.main;
        }
        if ((ticks.main - ticks.last_input) < config.screensaver_idle_time) {
            active = false;
            transitioning = false;
            opacity = 0.f;
        }
    }
}