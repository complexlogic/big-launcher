#include <memory>
#include <string>
#include <algorithm>
#include <filesystem>
#include <unordered_map>
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
    renderer = nullptr;
    window = nullptr;
    width = 0;
    height = 0;
}

void Display::init()
{
#ifdef __unix__
    //SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    setenv("SDL_VIDEODRIVER", "wayland,x11", 0);
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
    if (aspect_ratio > DISPLAY_ASPECT_RATIO + DISPLAY_ASPECT_RATIO_TOLERANCE)
        width = (int) std::round((float) height * DISPLAY_ASPECT_RATIO);
    if (aspect_ratio < DISPLAY_ASPECT_RATIO - DISPLAY_ASPECT_RATIO_TOLERANCE)
        height = (int) std::round((float) width / DISPLAY_ASPECT_RATIO);

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
    spdlog::debug("Creating window...");
    window = SDL_CreateWindow(PROJECT_NAME, 
                 SDL_WINDOWPOS_UNDEFINED,
                 SDL_WINDOWPOS_UNDEFINED,
                 0,
                 0,
                 SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS
             );
    if (window == nullptr) {
        spdlog::critical("Could not create window");
        spdlog::critical("SDL Error: {}", SDL_GetError());
        quit(EXIT_FAILURE);
    }
    spdlog::debug("Sucessfully created window");
    SDL_ShowCursor(SDL_DISABLE);

    // Create HW accelerated renderer
    spdlog::debug("Creating renderer...");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        spdlog::critical("Could not create renderer");
        spdlog::critical("SDL Error: {}", SDL_GetError());
        quit(EXIT_FAILURE);
    }
    spdlog::debug("Sucessfully created renderer");

    // Make sure the renderer supports the required format
    SDL_GetRendererInfo(renderer, &ri);
    auto end = std::cbegin(ri.texture_formats) + ri.num_texture_formats;
    auto it = std::find(std::cbegin(ri.texture_formats), end, SDL_PIXELFORMAT_ARGB8888);
    if (it == end) {
        spdlog::critical("GPU does not support the required pixel format");
        SDL_DestroyWindow(window);
        window = nullptr;
        quit(EXIT_FAILURE);
    }

    // Make sure we can render to texture
    if (!(ri.flags & SDL_RENDERER_TARGETTEXTURE)) {
        spdlog::critical("GPU does not support rendering to texture");
        SDL_DestroyWindow(window);
        window = nullptr;
        quit(EXIT_FAILURE);
    }

    // Set renderer properties
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

#ifdef _WIN32
    SDL_VERSION(&wm_info.version);
    SDL_GetWindowWMInfo(window, &wm_info);
#endif

    if (config.debug)
        print_debug_info();
}

void Display::close()
{
    if (renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window != nullptr) {
        SDL_DestroyWindow(window);
        window = nullptr;
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
    if (device_index < 0) {
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
                Controller controller(i);
                controller.connect(raise_error);
                if (controller.connected)
                    controllers.push_back(controller);
            }
        }
    }
    else {
        Controller controller(device_index);
        controller.connect(raise_error);
        if (controller.connected)
            controllers.push_back(controller);
    }
    check_state();
}

void Gamepad::disconnect(int id)
{
    if (id < 0) {
        for (Controller &controller : controllers)
            controller.disconnect();
        controllers.clear();
    }
    else {
        auto it = std::find_if(controllers.begin(),
                      controllers.end(),
                      [&](const Controller &c){ return c.id == id; }
                  );
        if (it != controllers.end()) {
            if (it->connected)
                it->disconnect();
            controllers.erase(it);
        }
    }
    check_state();
}

void Gamepad::check_state()
{
    bool connected = false;
    for (const Controller &controller : controllers) {
        if (controller.connected) {
            connected = true;
            break;
        }
    }
    this->connected = connected;
}

void Gamepad::Controller::connect(bool raise_error)
{
    gc = SDL_GameControllerOpen(device_index);
    connected = gc != nullptr;
    if (!connected && raise_error) {
        spdlog::error("Could not connect to gamepad");
        spdlog::error("SDL Error: {}", SDL_GetError());
    }
    else if (connected && config.debug) {
        spdlog::debug("Sucessfully connected to gamepad");
        if (config.debug && raise_error) {
            char *mapping = SDL_GameControllerMappingForDeviceIndex(device_index);
            if (mapping == nullptr)
                spdlog::debug("Could not get mapping");
            else {
                spdlog::debug("Gamepad mapping: {}", mapping);
                SDL_free(mapping);
            }
        }
    }
}

void Gamepad::Controller::disconnect()
{
    SDL_GameControllerClose(gc);
    gc = nullptr;
    connected = false;
    spdlog::debug("Disconnected gamepad");
}

void Gamepad::add_control(const char *label, const char *cmd)
{
      static const std::unordered_map<std::string, GamepadInfo> infos = {
        {"LStickX-",         {GamepadControl::Type::LSTICK,  GamepadControl::Direction::XM,   SDL_CONTROLLER_AXIS_LEFTX}},
        {"LStickX+",         {GamepadControl::Type::LSTICK,  GamepadControl::Direction::XP,   SDL_CONTROLLER_AXIS_LEFTX}},
        {"LStickY-",         {GamepadControl::Type::LSTICK,  GamepadControl::Direction::YM,   SDL_CONTROLLER_AXIS_LEFTY}},
        {"LStickY+",         {GamepadControl::Type::LSTICK,  GamepadControl::Direction::YP,   SDL_CONTROLLER_AXIS_LEFTY}},
        {"RStickX-",         {GamepadControl::Type::RSTICK,  GamepadControl::Direction::XM,   SDL_CONTROLLER_AXIS_RIGHTX}},
        {"RStickX+",         {GamepadControl::Type::RSTICK,  GamepadControl::Direction::XP,   SDL_CONTROLLER_AXIS_RIGHTX}},
        {"RStickY-",         {GamepadControl::Type::RSTICK,  GamepadControl::Direction::YM,   SDL_CONTROLLER_AXIS_RIGHTY}},
        {"RStickY+",         {GamepadControl::Type::RSTICK,  GamepadControl::Direction::YP,   SDL_CONTROLLER_AXIS_RIGHTY}},
        {"LTrigger",         {GamepadControl::Type::TRIGGER, GamepadControl::Direction::NONE, SDL_CONTROLLER_AXIS_TRIGGERLEFT}},
        {"RTrigger",         {GamepadControl::Type::TRIGGER, GamepadControl::Direction::NONE, SDL_CONTROLLER_AXIS_TRIGGERRIGHT}},
        {"ButtonA",          {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_A}},
        {"ButtonB",          {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_B}},
        {"ButtonX",          {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_X}},
        {"ButtonY",          {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_Y}},
        {"ButtonBack",       {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_BACK}},
        {"ButtonGuide",      {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_GUIDE}},
        {"ButtonStart",      {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_START}},
        {"ButtonLStick",     {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_LEFTSTICK}},
        {"ButtonRStick",     {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_RIGHTSTICK}},
        {"ButtonLShoulder",  {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_LEFTSHOULDER}},
        {"ButtonRShoulder",  {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER}},
        {"ButtonDPadUp",     {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_DPAD_UP}},
        {"ButtonDPadDown",   {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_DPAD_DOWN}},
        {"ButtonDPadLeft",   {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_DPAD_LEFT}},
        {"ButtonDPadRight",  {GamepadControl::Type::BUTTON,  GamepadControl::Direction::NONE, SDL_CONTROLLER_BUTTON_DPAD_RIGHT}}
    };

    static const SDL_GameControllerAxis opposing_axes[] = {
        SDL_CONTROLLER_AXIS_LEFTY,
        SDL_CONTROLLER_AXIS_LEFTX,
        SDL_CONTROLLER_AXIS_RIGHTY,
        SDL_CONTROLLER_AXIS_RIGHTX,
    };

    auto it = infos.find(label);
    if (it != infos.end()) {
        const GamepadInfo &info = it->second;
        controls.push_back(GamepadControl(info.type, info.index, info.direction, it->first, cmd));

        // Add control stick for axis if it doesn't already exist
        if (info.type == GamepadControl::Type::LSTICK || info.type == GamepadControl::Type::RSTICK) {
            SDL_GameControllerAxis opposing_axis = opposing_axes[info.index];
            auto a = std::find_if(sticks.begin(),
                        sticks.end(),
                        [&](auto &stick) {return stick.axes[0] == info.index || stick.axes[1] == info.index;}
                     );
            if (a == sticks.end())
                sticks.push_back(Stick(info.type, {(SDL_GameControllerAxis) info.index, opposing_axis}));
        }
    }
}

void Gamepad::poll()
{
    Controller &controller = controllers.front();
    // Poll sticks
    if (!sticks.empty()) {
        AxisType axis_type;
        for (auto stick = sticks.begin(); stick != sticks.end() && !selected_axis; ++stick) {
            for (auto controller = controllers.begin(); controller != controllers.end() && !selected_axis; ++controller) {
                // Poll individual axes in stick
                for (auto axis : stick->axes) {
                    axis_type = (axis == SDL_CONTROLLER_AXIS_LEFTX || axis == SDL_CONTROLLER_AXIS_RIGHTX) ? AxisType::X : AxisType::Y;
                    axis_values[stick->type][axis_type] = SDL_GameControllerGetAxis(controller->gc, axis);
                }
                AxisType max_axis = (abs(axis_values[stick->type][static_cast<int>(AxisType::X)]) >= abs(axis_values[stick->type][static_cast<int>(AxisType::Y)])) ? AxisType::X : AxisType::Y;
                if (abs(axis_values[stick->type][max_axis]) < GAMEPAD_DEADZONE)
                    continue;
                
                // Determine direction of stick movement
                GamepadControl::Direction direction;
                if (max_axis == AxisType::X) {
                    if (axis_values[stick->type][static_cast<int>(AxisType::X)] < 0)
                        direction = GamepadControl::Direction::XM;
                    else
                        direction = GamepadControl::Direction::XP;
                }
                else if (max_axis == AxisType::Y) {
                    if (axis_values[stick->type][static_cast<int>(AxisType::Y)] < 0)
                        direction = GamepadControl::Direction::YM;
                    else
                        direction = GamepadControl::Direction::YP;
                }

                // Save set selected axis if stick press is within range of a control
                auto it = std::find_if(controls.begin(),
                            controls.end(),
                            [&](auto control){return stick->type == control.type && direction == control.direction;}
                        );
                if (it != controls.end()) {
                    AxisType min_axis = (max_axis == AxisType::X) ? AxisType::Y : AxisType::X;
                    if (abs(axis_values[stick->type][min_axis]) < abs((int) std::round((float) axis_values[stick->type][max_axis] * max_opposing)))
                        selected_axis = &*it;
                }
            }
        }
    }


    // Check each control
    for (GamepadControl &control : controls) {
        if (control.type == GamepadControl::Type::LSTICK || control.type == GamepadControl::Type::RSTICK) {
            if (&control == selected_axis) { // We already polled for the stick
                control.repeat++;
                selected_axis = nullptr;
            }
            else
                control.repeat = 0;
        }

        // Poll buttons
        else {
            bool pressed = false;
            for (Controller &controller : controllers) {
                if ((control.type == GamepadControl::Type::BUTTON && SDL_GameControllerGetButton(controller.gc, (SDL_GameControllerButton) control.index)) ||
                (control.type == GamepadControl::Type::TRIGGER && SDL_GameControllerGetAxis(controller.gc, (SDL_GameControllerAxis) control.index) > GAMEPAD_DEADZONE)) {
                    pressed = true;
                    break;
                }
            }
            control.repeat = pressed ? control.repeat + 1 : 0;
        }

        if (control.repeat == 1) {
            spdlog::debug("Gamepad {} detected", control.label);
            ticks.last_input = ticks.main;
            execute_command(control.command);

        }
        else if (control.repeat == delay_period) {
            execute_command(control.command);
            control.repeat -= repeat_period;
        }
    }
}

void HotkeyList::add(const char *value)
{
    std::string_view string = value;
    if (string.front() != '#')
        return;
    size_t pos = string.find_first_of(";");
    if (pos == std::string::npos || pos == (string.size() - 1))
        return;

    std::string keycode_s(string, 1, pos);
    SDL_Keycode keycode = (SDL_Keycode) strtol(keycode_s.c_str(), nullptr, 16);
    if (!keycode)
        return;
    std::string_view command((char*) value + pos + 1);
    
#ifdef _WIN32
    if (command == ":exit") {
        set_exit_hotkey(keycode);
        return;
    }
#endif

    list.push_back(Hotkey(keycode, (char*) value + pos + 1));
}

inline std::vector<Hotkey>::iterator HotkeyList::begin()
{
    return list.begin();
}

inline std::vector<Hotkey>::iterator HotkeyList::end()
{
    return list.end();
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
            nullptr
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
    if (!w || !h)
        spdlog::error("Invalid resolution argument '{}'", string);
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
                if (cmd_begin != std::string::npos)
                    start_process(command.substr(cmd_begin, command.size() - cmd_begin), false);
            }
        }
        else if (command == ":left")
            layout.move_left();
        else if (command == ":right")
            layout.move_right();
        else if (command == ":up")
            layout.move_up();
        else if (command == ":down")
            layout.move_down();
        else if (command == ":select")
            layout.select();
        else if (command == ":shutdown")
            scmd_shutdown();
        else if (command == ":restart")
            scmd_restart();
        else if (command == ":sleep")
            scmd_sleep();
        else if (command == ":quit")
            quit(EXIT_SUCCESS);
    }

    // Application launching
    else {
        spdlog::debug("Executing command '{}'", command);
        state.application_launching = start_process(command, true);
        if (state.application_launching) {
            spdlog::debug("Successfully executed command");
            ticks.application_launch = ticks.main;
        }
        else
            spdlog::error("Failed to execute command");
    }
}

static inline void pre_launch()
{
    if (sound.connected)
        sound.disconnect();
    if (gamepad.connected)
        gamepad.disconnect(-1);
#ifdef _WIN32
    if (has_exit_hotkey())
        SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#endif
}

static inline void post_launch()
{
    if (config.sound_enabled)
        sound.connect();
    if (config.gamepad_enabled)
        gamepad.connect(-1, false);
#ifdef _WIN32
    SDL_EventState(SDL_SYSWMEVENT, SDL_DISABLE);
#endif
}

int main(int argc, char *argv[])
{
    SDL_Event event;
    std::string config_path;
    std::string layout_path;
    int c;
    executable_dir = SDL_GetBasePath();
    HotkeyList hotkey_list;
    
    // Parse command line
    const char *short_opts = "+c:l:dhv";
    static struct option long_opts[] = {
        { "config",       required_argument, nullptr, 'c' },
        { "layout",       required_argument, nullptr, 'l' },
        { "debug",        no_argument,       nullptr, 'd' },
        { "help",         no_argument,       nullptr, 'h' },
        { "version",      no_argument,       nullptr, 'v' },
        { 0, 0, 0, 0 }
    };
  
    while ((c = getopt_long(argc, argv, short_opts, long_opts, nullptr)) !=-1) {
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
#ifdef _WIN32
    join_paths(log_path, {executable_dir, LOG_FILENAME});
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
    if (!layout_path.empty()) {
        if (!std::filesystem::exists(layout_path)) {
            spdlog::critical("Layout file '{}' does not exist", layout_path);
            quit(EXIT_FAILURE);
        }
    }
    else if (!find_file<FileType::CONFIG>(layout_path, LAYOUT_FILENAME)) {
        spdlog::critical("Could not locate layout file");
        quit(EXIT_FAILURE);
    }

    if (!config_path.empty()) {
        if (!std::filesystem::exists(config_path)) {
            spdlog::critical("Config file '{}' does not exist", config_path);
            quit(EXIT_FAILURE);
        }
    }
    if (!find_file<FileType::CONFIG>(config_path, CONFIG_FILENAME)) {
        spdlog::critical("Could not locate config file");
        quit(EXIT_FAILURE);
    }

    // Parse files, initialize libraries
    layout.parse(layout_path);
    config.parse(config_path, gamepad, hotkey_list);
    display.init();
    init_svg();
    if (config.sound_enabled && !sound.init())
        config.sound_enabled = false;
    if (config.gamepad_enabled && gamepad.init())
        config.gamepad_enabled = false;

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
    if (display.dm.w != display.width || display.dm.h != display.height)
        SDL_RenderSetLogicalSize(display.renderer, display.width, display.height);
#endif

#ifdef _WIN32
    if (has_exit_hotkey())
        register_exit_hotkey();
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
                        if (event.key.keysym.sym == SDLK_DOWN)
                            layout.move_down();
                        else if (event.key.keysym.sym == SDLK_UP)
                            layout.move_up();
                        else if (event.key.keysym.sym == SDLK_LEFT)
                            layout.move_left();
                        else if (event.key.keysym.sym == SDLK_RIGHT)
                            layout.move_right();
                        else if (event.key.keysym.sym == SDLK_RETURN)
                            layout.select();

                        // Check hotkeys
                        else {
                            for (Hotkey &hotkey : hotkey_list) {
                                if (hotkey.keycode == event.key.keysym.sym) {
                                    execute_command(hotkey.command);
                                    break;
                                }
                            }
                        }
                        ticks.last_input = ticks.main;
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
                        }
                        if (event.jdevice.which == config.gamepad_index || config.gamepad_index < 0)
                            gamepad.connect(event.jdevice.which, true);
                    }
                    else if (config.debug)
                        spdlog::debug("Unrecognized joystick detected at device index {}", event.jdevice.which);
                    break;

                case SDL_JOYDEVICEREMOVED:
                    spdlog::debug("Device {} disconnected", event.jdevice.which);
                    gamepad.disconnect(event.jdevice.which);
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
                case SDL_MOUSEBUTTONDOWN:
                    if (config.mouse_select && event.button.button == SDL_BUTTON_LEFT) {
                        ticks.last_input = ticks.main;
                        layout.select();
                    }
                    break;
#ifdef _WIN32
                case SDL_SYSWMEVENT:
                    check_exit_hotkey(event.syswm.msg);
                    break;
#endif
            }
        }

        if (gamepad.connected && !state.application_launching)
            gamepad.poll();

        if (state.application_launching && 
        ticks.main - ticks.application_launch > APPLICATION_TIMEOUT) {
            state.application_launching = false;
        }
        if (state.application_running)
            SDL_Delay(APPLICATION_WAIT_PERIOD);
        else
            layout.draw();
    }
    return 0;
}
