#include <memory>
#include <string>
#include <algorithm>
#include <filesystem>
#include <getopt.h>
#include <stdlib.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "basic_lazy_file_sink.hpp"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <lconfig.h>
#include "main.hpp"
#include "layout.hpp"
#include "image.hpp"
#include "util.hpp"

Display display;
Config config;
Ticks ticks;
char *executable_dir;
std::string log_path;


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

    // Force 16:9 aspect ratio
    float aspect_ratio = (float) width / (float) height;
    if (aspect_ratio > DISPLAY_ASPECT_RATIO + DISPLAY_ASPECT_RATIO_TOLERANCE) {
        width = (int) std::round((float) height * DISPLAY_ASPECT_RATIO);
    }
    if (aspect_ratio < DISPLAY_ASPECT_RATIO - DISPLAY_ASPECT_RATIO_TOLERANCE) {
        height = (int) std::round((float) width / DISPLAY_ASPECT_RATIO);
    }

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
    SDL_GetRendererInfo(renderer, &ri);

    // Make sure the renderer supports the required format
    auto end = std::cbegin(ri.texture_formats) + ri.num_texture_formats;
    auto it = std::find(std::cbegin(ri.texture_formats), end, SDL_PIXELFORMAT_ARGB8888);
    if (it == end) {
        spdlog::critical("GPU does not support the required pixel format");
        SDL_DestroyWindow(window);
        window = NULL;
        quit(EXIT_FAILURE);
    }

    // Make sure we can render to texture
    if (!ri.flags & SDL_RENDERER_TARGETTEXTURE) {
        spdlog::critical("GPU does not support rendering to texture");
        SDL_DestroyWindow(window);
        window = NULL;
        quit(EXIT_FAILURE);
    }

    // Set renderer properties
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

    spdlog::debug("Successfully created window and renderer");
    if (config.debug)
        this->print_debug_info();
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

void Display::print_debug_info()
{
    spdlog::debug("Video Information:");
    spdlog::debug("  Resolution:   {}x{}", dm.w, dm.h);
    spdlog::debug("  Refresh Rate: {} Hz", dm.refresh_rate);
    spdlog::debug("  Driver:       {}", SDL_GetCurrentVideoDriver());
    spdlog::debug("  Supported texture formats:");
    std::for_each(std::cbegin(ri.texture_formats), 
        std::cbegin(ri.texture_formats) + ri.num_texture_formats, 
        [](Uint32 f){spdlog::debug("    {}", SDL_GetPixelFormatName(f));}
    );
}

static void cleanup()
{
    display.close();
    quit_svg();
}

void quit(int status)
{
    spdlog::debug("Quitting program");
    if (status == EXIT_FAILURE) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, 
            PROJECT_NAME, 
            fmt::format("A critical error occurred. Check the log file '{}' for details", log_path).c_str(), 
            NULL
        );
    }
    cleanup();
    exit(status);
}

VERSION(log, spdlog::debug, "")
#ifdef __unix__
VERSION(print, fmt::print, "\n")
static void print_help()
{
    fmt::print("Usage: " EXECUTABLE_TITLE " [OPTIONS]\n");
    fmt::print("  -c p, --config=p   Load config file from path p.\n");
    fmt::print("  -l p, --layout=p   Load layout file from path p.\n");
    fmt::print("  -d,   --debug      Enable debug messages.\n");
    fmt::print("  -h,   --help       Show this help message.\n");
    fmt::print("  -v,   --version    Print version information.\n");
}
#endif

#ifdef DEBUG
static inline void parse_render_resolution(const char *string, int &w, int &h)
{
    std::string s = string;
    size_t x = s.find_first_of('x');
    if (x == std::string::npos) {
        spdlog::error("Invalid resolution argument '{}'", string);
        return;
    }
    w = std::atoi(s.substr(0, x).c_str());
    h = std::atoi(s.substr(x + 1, s.size() - x).c_str());
    if (!w || !h) {
        spdlog::error("Invalid resolution argument '{}'", string);
    }
}
#endif

int main(int argc, char *argv[])
{
    Layout layout;
    SDL_Event event;
    std::string config_path;
    std::string layout_path;
    char c;
    executable_dir = SDL_GetBasePath();
    
    // Parse command line
    const char *short_opts = "+c:l:dhv";
    static struct option long_opts[] = {
        { "config",       required_argument, NULL, 'c' },
        { "layout",       required_argument, NULL, 'l' },
        { "debug",        no_argument,       NULL, 'd' },
        { "help",         no_argument,       NULL, 'h' },
        { "version",      no_argument,       NULL, 'v' },
        { 0, 0, 0, 0 }
    };
  
    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) !=-1) {
        switch (c) {
            case 'c':
                config_path = optarg;
                break;  

            case 'l':
                layout_path = optarg;
                break;

            case 'd':
                config.debug = true;
                break;
#ifdef __unix__
            case 'h':
                print_help();
                quit(EXIT_SUCCESS);
                break;

            case 'v':
                print_version();
                quit(EXIT_SUCCESS);
                break;
#endif              
        }
    }

    // Initialize logging
#ifdef __unix__
    char *home_dir = getenv("HOME");
    join_paths(log_path, {home_dir, ".local", "share", EXECUTABLE_TITLE, LOG_FILENAME});
#endif
    auto file_sink = std::make_shared<spdlog::sinks::basic_lazy_file_sink_mt>(log_path, true);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    std::vector<spdlog::sink_ptr> sinks {file_sink};
#ifdef __unix__
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::warn);
    console_sink->set_pattern("[%^%l%$] %v");
    sinks.push_back(console_sink);
#endif
    auto logger = std::make_shared<spdlog::logger>(PROJECT_NAME, sinks.begin(), sinks.end());
    logger->set_level((config.debug) ? spdlog::level::debug : spdlog::level::info);
    spdlog::set_default_logger(logger);
    if (config.debug) {
        log_version();
        spdlog::debug("");
    }
    
    // Find layout and config files
#ifdef __unix__
    std::string home_config;
    join_paths(home_config, {home_dir, ".config", EXECUTABLE_TITLE});
    std::initializer_list<const char*> prefixes = {
        CURRENT_DIRECTORY,
        executable_dir,
        home_config.c_str(),
        SYSTEM_SHARE_DIR
    };
#endif
    if (!layout_path.empty() && !std::filesystem::exists(layout_path)) {
        spdlog::critical("Layout file '{}' does not exist", layout_path);
        quit(EXIT_FAILURE);
    }
    else if (!find_file(layout_path, LAYOUT_FILENAME, prefixes)) {
        spdlog::critical("Could not locate layout file");
        quit(EXIT_FAILURE);
    }

    if (!config_path.empty() && !std::filesystem::exists(config_path)) {
        spdlog::critical("Config file '{}' does not exist", config_path);
        quit(EXIT_FAILURE);
    }
    else if (!find_file(config_path, CONFIG_FILENAME, prefixes)) {
        spdlog::critical("Could not locate config file");
        quit(EXIT_FAILURE);
    }

    layout.parse(layout_path);
    config.parse(config_path);
    display.init();
    init_svg();

#ifdef DEBUG
    int render_w = 0;
    int render_h = 0;
    int output_w, output_h;
    if (argc > optind) {
        parse_render_resolution(argv[optind], render_w, render_h);
        if (render_w != 0 && render_h != 0) {
            spdlog::debug("Rendering layout at {}x{}", render_w, render_h);
            output_w = display.width;
            output_h = display.height;
            display.width = render_w;
            display.height = render_h;
        }
    }
#endif

    layout.load_surfaces(display.width, display.height);
    display.create_window();
    layout.load_textures(display.renderer);

#ifdef DEBUG
    if (render_w != 0 && render_h != 0) {
        SDL_RenderSetScale(display.renderer, 
            (float) output_w / (float) render_w, 
            (float) output_h / (float) render_h
        );
    }
#else
    if (display.dm.w != display.width || display.dm.h != display.height) {
        SDL_RenderSetLogicalSize(display.renderer, display.width, display.height);
    }
#endif

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
