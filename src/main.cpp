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
#include "sound.hpp"
#include "util.hpp"
#include "platform/platform.hpp"

Display display;
Layout layout;
Config config;
Gamepad gamepad;
Sound sound;
Ticks ticks;
char *executable_dir;
std::string log_path;

State state = { false };


Display::Display()
{
    renderer = NULL;
    window = NULL;
    width = 0;
    height = 0;
}

void Display::init()
{
#ifdef __unix__
    //SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1");

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        spdlog::critical("Could not initialize SDL");
        spdlog::critical("SDL Error: {}", SDL_GetError());
        quit(EXIT_FAILURE);
    }

    if (SDL_GetDesktopDisplayMode(0, &dm) < 0) {
        spdlog::critical("Could not get desktop display mode");
        spdlog::critical("SDL Error: {}", SDL_GetError());
        quit(EXIT_FAILURE);
    }
    width = dm.w;
    height = dm.h;

    // Force 16:9 aspect ratio
    float aspect_ratio = (float) width / (float) height;
    if (aspect_ratio > DISPLAY_ASPECT_RATIO + DISPLAY_ASPECT_RATIO_TOLERANCE) {
        width = (int) std::round((float) height * DISPLAY_ASPECT_RATIO);
    }
    if (aspect_ratio < DISPLAY_ASPECT_RATIO - DISPLAY_ASPECT_RATIO_TOLERANCE) {
        height = (int) std::round((float) width / DISPLAY_ASPECT_RATIO);
    }

    // Initialize SDL_image
    constexpr int flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP; 
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
    if (!(ri.flags & SDL_RENDERER_TARGETTEXTURE)) {
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

Gamepad::Gamepad()
{
    controller = NULL;
    selected_axis = NULL;
}

int Gamepad::init()
{
    spdlog::debug("Initializing game controller subsystem...");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0) {
        spdlog::error("Could not initialize game controller subsystem");
        spdlog::error("SDL Error: {}", SDL_GetError());
        return 1;
    }
    spdlog::debug("Successfully initialized game controller subsystem");

    int refresh_period = 1000 / display.dm.refresh_rate;
    delay_period = GAMEPAD_REPEAT_DELAY / refresh_period;
    repeat_period = GAMEPAD_REPEAT_INTERVAL / refresh_period;

    if (!config.gamepad_mappings_file.empty()) {
        if (SDL_GameControllerAddMappingsFromFile(config.gamepad_mappings_file.c_str()) < 0) {
            spdlog::error("Could not load gamepad mappings from file '{}'", 
                config.gamepad_mappings_file.c_str()
            );
        }
    }
    return 0;
}

void Gamepad::connect(int device_index, bool raise_error)
{
    controller = SDL_GameControllerOpen(device_index);
    if (controller == NULL) {
        connected = false;
        if (raise_error) {
            spdlog::error("Could not connect to gamepad");
            spdlog::error("SDL Error: {}", SDL_GetError());
        }
        return;
    }
    connected = true;
    spdlog::debug("Sucessfully connected to gamepad");
    if (config.debug && raise_error) {
        char *mapping = SDL_GameControllerMappingForDeviceIndex(device_index);
        if (mapping == NULL) {
            spdlog::debug("Could not get mapping");
        }
        else {
            spdlog::debug("Gamepad mapping: {}", mapping);
            SDL_free(mapping);
        }
    }
}

void Gamepad::disconnect()
{
    SDL_GameControllerClose(controller);
    controller = NULL;
    connected = false;
    spdlog::debug("Disconnected gamepad");
}

void Gamepad::add_control(const char *label, const char *cmd)
{
    static const GamepadInfo infos[] = {
        {"LStickX-",         TYPE_LSTICK,  DIRECTION_XM,   SDL_CONTROLLER_AXIS_LEFTX},
        {"LStickX+",         TYPE_LSTICK,  DIRECTION_XP,   SDL_CONTROLLER_AXIS_LEFTX},
        {"LStickY-",         TYPE_LSTICK,  DIRECTION_YM,   SDL_CONTROLLER_AXIS_LEFTY},
        {"LStickY+",         TYPE_LSTICK,  DIRECTION_YP,   SDL_CONTROLLER_AXIS_LEFTY},
        {"RStickX-",         TYPE_RSTICK,  DIRECTION_XM,   SDL_CONTROLLER_AXIS_RIGHTX},
        {"RStickX+",         TYPE_RSTICK,  DIRECTION_XP,   SDL_CONTROLLER_AXIS_RIGHTX},
        {"RStickY-",         TYPE_RSTICK,  DIRECTION_YM,   SDL_CONTROLLER_AXIS_RIGHTY},
        {"RStickY+",         TYPE_RSTICK,  DIRECTION_YP,   SDL_CONTROLLER_AXIS_RIGHTY},
        {"LTrigger",         TYPE_TRIGGER, DIRECTION_NONE, SDL_CONTROLLER_AXIS_TRIGGERLEFT},
        {"RTrigger",         TYPE_TRIGGER, DIRECTION_NONE, SDL_CONTROLLER_AXIS_TRIGGERRIGHT},
        {"ButtonA",          TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_A},
        {"ButtonB",          TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_B},
        {"ButtonX",          TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_X},
        {"ButtonY",          TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_Y},
        {"ButtonBack",       TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_BACK},
        {"ButtonGuide",      TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_GUIDE},
        {"ButtonStart",      TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_START},
        {"ButtonLStick",     TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_LEFTSTICK},
        {"ButtonRStick",     TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_RIGHTSTICK},
        {"ButtonLShoulder",  TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
        {"ButtonRShoulder",  TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
        {"ButtonDPadUp",     TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_DPAD_UP},
        {"ButtonDPadDown",   TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_DPAD_DOWN},
        {"ButtonDPadLeft",   TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_DPAD_LEFT},
        {"ButtonDPadRight",  TYPE_BUTTON,  DIRECTION_NONE, SDL_CONTROLLER_BUTTON_DPAD_RIGHT}
    };

    static const SDL_GameControllerAxis opposing_axes[] = {
        SDL_CONTROLLER_AXIS_LEFTY,
        SDL_CONTROLLER_AXIS_LEFTX,
        SDL_CONTROLLER_AXIS_RIGHTY,
        SDL_CONTROLLER_AXIS_RIGHTX,
    };

    auto it = std::find_if(std::cbegin(infos), 
                  std::cend(infos), 
                  [&](const GamepadInfo &info) {return strcmp(label, info.label) == 0;}
              );
    if (it != std::cend(infos)) {
        controls.push_back({it->type, it->index, it->direction, it->label, cmd});

        // Add control stick for axis if it doesn't already exist
        if (it->type == TYPE_LSTICK || it->type == TYPE_RSTICK) {
            SDL_GameControllerAxis opposing_axis = opposing_axes[it->index];
            auto a = std::find_if(sticks.begin(),
                        sticks.end(),
                        [&](auto &stick) {return stick.axes[0] == it->index || stick.axes[1] == it->index;}
                     );
            if (a == sticks.end()) {
                sticks.push_back(
                    {
                        (ControlType) it->type,
                        {(SDL_GameControllerAxis) it->index, opposing_axis}
                    }
                );
            }
        }
    }
}

void Gamepad::poll()
{
    // Poll sticks
    if (!sticks.empty()) {
        AxisType axis_type;
        for (const auto &stick : sticks) {

            // Poll individual axes in stick
            for (auto axis : stick.axes) {
                axis_type = (axis == SDL_CONTROLLER_AXIS_LEFTX || axis == SDL_CONTROLLER_AXIS_RIGHTX) ? AXIS_X : AXIS_Y;
                axis_values[stick.type][axis_type] = SDL_GameControllerGetAxis(controller, axis);
            }
            AxisType max_axis = (abs(axis_values[stick.type][AXIS_X]) >= abs(axis_values[stick.type][AXIS_Y])) ? AXIS_X : AXIS_Y;
            if (abs(axis_values[stick.type][max_axis]) < GAMEPAD_DEADZONE)
                continue;
            
            // Determine direction of stick movement
            StickDirection direction;
            if (max_axis == AXIS_X) {
                if (axis_values[stick.type][AXIS_X] < 0) {
                    direction = DIRECTION_XM;
                }
                else {
                    direction = DIRECTION_XP;
                }
            }
            else if (max_axis == AXIS_Y) {
                if (axis_values[stick.type][AXIS_Y] < 0) {
                    direction = DIRECTION_YM;
                }
                else {
                    direction = DIRECTION_YP;
                }
            }

            // Save set selected axis if stick press is within range of a control
            auto it = std::find_if(controls.begin(),
                          controls.end(),
                          [&](auto control){return stick.type == control.type && direction == control.direction;}
                      );
            if (it != controls.end()) {
                AxisType min_axis = (max_axis == AXIS_X) ? AXIS_Y : AXIS_X;
                if (abs(axis_values[stick.type][min_axis]) < abs((int) std::round((float) axis_values[stick.type][max_axis] * max_opposing)))
                    selected_axis = &*it;
            }

        }
    }

    for (GamepadControl &control : controls) {
        if (control.type == TYPE_LSTICK || control.type == TYPE_RSTICK) {
            if (&control == selected_axis) {
                control.repeat++;
                selected_axis = NULL;
            }
            else {
                control.repeat = 0;
            }
        }

        // Poll buttons
        else {
            if ((control.type == TYPE_BUTTON && SDL_GameControllerGetButton(controller, (SDL_GameControllerButton) control.index)) ||
            (control.type == TYPE_TRIGGER && SDL_GameControllerGetAxis(controller, (SDL_GameControllerAxis) control.index) > GAMEPAD_DEADZONE)) {
                control.repeat++;
            }
            else {
                control.repeat = 0;
            }
        }

        if (control.repeat == 1) {
            spdlog::debug("Gamepad {} detected", control.label);
            execute_command(control.command);
        }
        else if (control.repeat == delay_period) {
            execute_command(control.command);
            control.repeat -= repeat_period;
        }
    }
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
    fmt::print("    -c p, --config=p     Load config file from path p.\n");
    fmt::print("    -l p, --layout=p     Load layout file from path p.\n");
    fmt::print("    -d,   --debug        Enable debug messages.\n");
    fmt::print("    -h,   --help         Show this help message.\n");
    fmt::print("    -v,   --version      Print version information.\n");
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

void execute_command(const std::string &command)
{
    // Special commands
    if (command.front() == ':') {
        if (command.starts_with(":fork")) {
            size_t space = command.find_first_of(' ');
            if (space != std::string::npos) {
                size_t cmd_begin = command.find_first_not_of(' ', space);
                if (cmd_begin != std::string::npos) {
                    start_process(command.substr(cmd_begin, command.size() - cmd_begin), false);
                }
            }
        }
        else if (command == ":left") {
            layout.move_left();
        }
        else if (command == ":right") {
            layout.move_right();
        }
        else if (command == ":up") {
            layout.move_up();
        }
        else if (command == ":down") {
            layout.move_down();
        }
        else if (command == ":select") {
            layout.select();
        }
        else if (command == ":shutdown") {
            scmd_shutdown();
        }
        else if (command == ":restart") {
            scmd_restart();
        }
        else if (command == ":sleep") {
            scmd_sleep();
        }
        else if (command == ":quit") {
            quit(EXIT_SUCCESS);
        }
    }

    // Application launching
    else {
        spdlog::debug("Executing command '{}'", command);
        state.application_launching = start_process(command, true);
        if (state.application_launching) {
            spdlog::debug("Successfully executed command");
            ticks.application_launch = ticks.main;
        }
        else {
            spdlog::error("Failed to execute command");
        }
    }
}

static inline void pre_launch()
{
    if (sound.connected)
        sound.disconnect();
    if (gamepad.connected)
        gamepad.disconnect();
}

static inline void post_launch()
{
    if (config.sound_enabled)
        sound.connect();
    if (config.gamepad_enabled)
        gamepad.connect(config.gamepad_index, false);
        
}

int main(int argc, char *argv[])
{
    SDL_Event event;
    std::string config_path;
    std::string layout_path;
    int c;
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
    if (!layout_path.empty()) {
        if (!std::filesystem::exists(layout_path)) {
            spdlog::critical("Layout file '{}' does not exist", layout_path);
            quit(EXIT_FAILURE);
        }
    }
    else if (!find_file(layout_path, LAYOUT_FILENAME, prefixes)) {
        spdlog::critical("Could not locate layout file");
        quit(EXIT_FAILURE);
    }

    if (!config_path.empty()) {
        if (!std::filesystem::exists(config_path)) {
            spdlog::critical("Config file '{}' does not exist", config_path);
            quit(EXIT_FAILURE);
        }
    }
    if (!find_file(config_path, CONFIG_FILENAME, prefixes)) {
        spdlog::critical("Could not locate config file");
        quit(EXIT_FAILURE);
    }


    // Parse files, initialize libraries
    layout.parse(layout_path);
    config.parse(config_path, gamepad);
    display.init();
    init_svg();
    if (config.sound_enabled && sound.init()) {
        config.sound_enabled = false;
    }
    if (config.gamepad_enabled && gamepad.init()) {
        config.gamepad_enabled = false;
    }

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

    // Render graphics
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
    spdlog::debug("");
    spdlog::debug("Begin main loop");
    while(1) {
        ticks.main = SDL_GetTicks();
        layout.update();
        while(SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_QUIT:
                    quit(EXIT_SUCCESS);
                    break;

                case SDL_KEYDOWN:
                    if (!state.application_launching) {
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
                        else if (event.key.keysym.sym == SDLK_RETURN) {
                            layout.select();
                        }
                        SDL_FlushEvent(SDL_KEYDOWN);
                    }
                    break;

                case SDL_JOYDEVICEADDED:
                    if (SDL_IsGameController(event.jdevice.which) == SDL_TRUE) {
                        if (config.debug) {
                            spdlog::debug("Detected gamepad '{}' at device index {}",
                                SDL_GameControllerNameForIndex(event.jdevice.which),
                                event.jdevice.which
                            );
                            //spdlog::debug("Mapping: {}", SDL_GameControllerMappingForIndex(event.jdevice.which));
                        }
                        if (event.jdevice.which == config.gamepad_index)
                            gamepad.connect(event.jdevice.which, true);
                    }
                    else if (config.debug) {
                        spdlog::debug("Unrecognized joystick detected at device index {}", event.jdevice.which);
                    }
                    break;

                case SDL_JOYDEVICEREMOVED:
                    if (event.jdevice.which == config.gamepad_index)
                        gamepad.disconnect();
                    break;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                        spdlog::debug("Lost window focus");
                        if (state.application_launching) {
                            pre_launch();
                            state.application_launching = false;
                            state.application_running = true;
                        }
                    }
                    else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                        spdlog::debug("Gained window focus");
                        if (state.application_running) {
                            post_launch();
                            state.application_running = false;
                        }
                    }
                    break;
            }
        }

        if (gamepad.connected && !state.application_launching)
            gamepad.poll();

        if (state.application_launching && 
        ticks.main - ticks.application_launch > APPLICATION_TIMEOUT) {
            state.application_launching = false;
        }
        if (state.application_running) {
            SDL_Delay(APPLICATION_WAIT_PERIOD);
        }
        else {
            layout.draw();
        }
    }
    return 0;
}
