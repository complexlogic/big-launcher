#include <SDL.h>

#define DISPLAY_ASPECT_RATIO 1.77777778
#define DISPLAY_ASPECT_RATIO_TOLERANCE 0.01f 

class Display {
    public:
        SDL_Renderer *renderer;
        SDL_Window *window;
        SDL_DisplayMode dm;
        SDL_RendererInfo ri;
        int width;
        int height;
        int refresh_period;

        Display();
        void init();
        void create_window();
        void close();
        void print_debug_info();
};

typedef struct {
    Uint32 main;
} Ticks;

#ifdef __GNUC__
#define COMPILER_INFO(f, end) f("Compiler:   GCC {}.{}" end, __GNUC__, __GNUC_MINOR__);
#endif

#ifdef _MSC_VER
#define COMPILER_INFO(f, end) f("Compiler:   Microsoft C/C++ {:.2f}" end, (float) _MSC_VER / 100.0f);
#endif


#define VERSION(name, f, end)                                                                    \
static void name##_version() {                                                                   \
    SDL_version sdl_version;                                                                     \
    SDL_GetVersion(&sdl_version);                                                                \
    const SDL_version *img_version = IMG_Linked_Version();                                       \
    const SDL_version *ttf_version = TTF_Linked_Version();                                       \
    f(PROJECT_NAME " version " PROJECT_VERSION ", using:" end);                                  \
    f("  SDL       {}.{}.{}" end, sdl_version.major, sdl_version.minor, sdl_version.patch);      \
    f("  SDL_image {}.{}.{}" end, img_version->major, img_version->minor, img_version->patch);   \
    f("  SDL_ttf   {}.{}.{}" end, ttf_version->major, ttf_version->minor, ttf_version->patch);   \
    f(end);                                                                                      \
    f("Build date: " __DATE__ end);                                                              \
    COMPILER_INFO(f, end);                                                                       \
}

void quit(int status);