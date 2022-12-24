#pragma once

#define SCREENSAVER_TRANSITION_TIME 2000

#define MIN_SCREENSAVER_IDLE_TIME 5
#define MAX_SCREENSAVER_IDLE_TIME 60000

class Screensaver {
    private:
        float opacity = 0.f;
        float opacity_change_rate;
        SDL_Surface *surface = nullptr;
        Uint32 current_ticks;
    
    public:
        bool active = false;
        bool transitioning = false;
        SDL_Texture *texture = nullptr;

        void render_surface(int w, int h);
        void render_texture(SDL_Renderer *renderer);
        void update(void);

};
