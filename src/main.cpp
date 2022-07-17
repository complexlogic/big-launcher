#include <memory>
#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "basic_lazy_file_sink.hpp"

#include <SDL.h>
#include <SDL_image.h>

#include <lconfig.h>
#include "main.hpp"
#include "layout.hpp"
#include "image.hpp"
#include "util.hpp"

Display display;
Config config;
Ticks ticks;

#define TEST(name, fn, end)                  \
void print_##name() {                       \
    fn("Testing" end);                       \
}

TEST(test, fmt::print, "\n")
TEST(debug, spdlog::debug, "")

Display::Display()
{
    renderer = NULL;
    window = NULL;
    width = 0;
    height = 0;
    refresh_period = 0;
}

void Display::init()
{
    int flags = SDL_INIT_VIDEO;
#ifdef __unix__
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");
    //if (config.gamepad_enabled) {
      //  sdl_flags |= SDL_INIT_GAMECONTROLLER;
   // }
    // Initialize SDL
    if (SDL_Init(flags) < 0)
    {
        spdlog::critical("Could not initialize SDL");
        spdlog::critical("SDL Error: {}", SDL_GetError());
        quit(EXIT_FAILURE);
    }

    if (SDL_GetDesktopDisplayMode(0, &dm) < 0) {
        spdlog::critical("Could not get desktop display mode");
        spdlog::critical("SDL Error: {}\n", SDL_GetError());
        quit(EXIT_FAILURE);
    }
    width = dm.w;
    height = dm.h;
    refresh_period = 1000 / dm.refresh_rate;
    /*
    if (config.gamepad_enabled) {
        delay_period = GAMEPAD_REPEAT_DELAY / refresh_period;
        repeat_period = GAMEPAD_REPEAT_INTERVAL / refresh_period; 
    }
    */

    // Initialize SDL_image
    flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP; 
    if (!(IMG_Init(flags) & flags)) {
        spdlog::critical("Could not initialize SDL_image");
        spdlog::critical("SDL Error: {}", IMG_GetError());
        quit(EXIT_FAILURE);   
    }

    // Initialize SDL_ttf
    if (TTF_Init() == -1) {
        spdlog::critical("Could not initialize SDL_ttf");
        spdlog::critical("SDL Error: {}", TTF_GetError());
        quit(EXIT_FAILURE);
    }

    spdlog::debug("Successfully initialized display");
}

void Display::create_window()
{
    window = SDL_CreateWindow(PROJECT_NAME, 
                 SDL_WINDOWPOS_UNDEFINED,
                 SDL_WINDOWPOS_UNDEFINED,
                 0,
                 0,
                 SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS
             );
    if (window == NULL) {
        spdlog::critical("Could not create window");
        spdlog::critical("SDL Error: {}", SDL_GetError());
        quit(EXIT_FAILURE);
    }
    SDL_ShowCursor(SDL_DISABLE);

    // Create HW accelerated renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        spdlog::critical("Could not create renderer");
        spdlog::critical("SDL Error: {}", SDL_GetError());
        quit(EXIT_FAILURE);
    }

    // Set renderer properties
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x03, 0x01, 0x6e, 0);

    spdlog::debug("Successfully created window and renderer");
}

void Display::close()
{
    if (renderer != NULL) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window != NULL) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_Quit();
    IMG_Quit();
    TTF_Quit();
}

static void cleanup()
{
    display.close();
    quit_svg();
}

void quit(int status)
{
    spdlog::debug("Quitting program");
    cleanup();
    exit(status);
}

int main(int argc, char *argv[])
{

    Layout layout;
    SDL_Event event;

    // Initialize log
    auto file_sink = std::make_shared<spdlog::sinks::basic_lazy_file_sink_mt>(LOG_FILENAME, true);
    file_sink->set_level(spdlog::level::debug);
    std::vector<spdlog::sink_ptr> sinks {file_sink};
#ifdef __unix__
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::warn);
    console_sink->set_pattern("[%^%l%$] %v");
    sinks.push_back(console_sink);
#endif
    auto logger = std::make_shared<spdlog::logger>(PROJECT_NAME, sinks.begin(), sinks.end());
    spdlog::set_default_logger(logger);
    //spdlog::critical("test");

    layout.parse("layout.xml");
    config.parse("config.ini");


    display.init();
    init_svg();
    
    layout.load_surfaces(display.width, display.height);

    display.create_window();
    layout.load_textures(display.renderer);


    // Main program loop
    spdlog::debug("Begin main loop");
    while(1) {
        ticks.main = SDL_GetTicks();
        while(SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_QUIT:
                    quit(EXIT_SUCCESS);
                    break;

                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        quit(EXIT_SUCCESS);
                    }
                    else if (event.key.keysym.sym == SDLK_DOWN) {
                        layout.move_down();
                    }
                    else if (event.key.keysym.sym == SDLK_UP) {
                        layout.move_up();
                    }
                    else if (event.key.keysym.sym == SDLK_LEFT) {
                        layout.move_left();
                    }
                    else if (event.key.keysym.sym == SDLK_RIGHT) {
                        layout.move_right();
                    }
                    SDL_FlushEvent(SDL_KEYDOWN);
                    break;
            }
        }
        layout.draw();
        layout.shift();
    }

    return 0;
}
