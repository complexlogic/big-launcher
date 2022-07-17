#include <SDL.h>
class Display {
    public:
        SDL_Renderer *renderer;
        SDL_Window *window;
        SDL_DisplayMode dm;
        int width;
        int height;
        int refresh_period;

        Display();
        void init();
        void create_window();
        void close();

};

typedef struct {
    Uint32 main;
} Ticks;

void quit(int status);