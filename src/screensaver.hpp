#ifndef HAS_SCREENSAVER_H
#define HAS_SCREENSAVER_H
#define SCREENSAVER_TRANSITION_TIME 2000

#define MIN_SCREENSAVER_IDLE_TIME 5
#define MAX_SCREENSAVER_IDLE_TIME 60000

class Screensaver {
    private:
        float opacity;
        float opacity_change_rate;
        SDL_Surface *surface;
        Uint32 current_ticks;
    
    public:
        bool active;
        bool transitioning;
        SDL_Texture *texture;

        Screensaver() : active(false), transitioning(false), opacity(0.f), surface(NULL), texture(NULL) {};
        void render_surface(int w, int h);
        void render_texture(SDL_Renderer *renderer);
        void update(void);

};

#endif