#pragma once

#include <SDL.h>
#ifdef _WIN32
#include <SDL_syswm.h>
#endif
#include <SDL_mixer.h>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <set>
#include <cmath>

#define DISPLAY_ASPECT_RATIO 1.77777778
#define DISPLAY_ASPECT_RATIO_TOLERANCE 0.01f
#define APPLICATION_WAIT_PERIOD 100
#define APPLICATION_TIMEOUT 10000

#define GAMEPAD_DEADZONE 15000
#define GAMEPAD_REPEAT_DELAY 500
#define GAMEPAD_REPEAT_INTERVAL 25
#define PI 3.14159f
#define GAMEPAD_AXIS_RANGE 60.f // degrees

class Display {
    public:
        SDL_Renderer *renderer = nullptr;
        SDL_Window *window = nullptr;
        SDL_DisplayMode dm;
        SDL_RendererInfo ri;
#ifdef _WIN32
        SDL_SysWMinfo wm_info;
#endif
        int width = 0;
        int height = 0;

        void init();
        void create_window();
        void close();
        void print_debug_info();
};

class Gamepad {
    private:
        struct Controller {
            SDL_GameController *gc = nullptr;
            int device_index;
            int id;
            bool connected = false;

            Controller(int device_index) : device_index(device_index), id((int) SDL_JoystickGetDeviceInstanceID(device_index)) {}
            void connect(bool raise_error);
            void disconnect();
        };

        enum AxisType{
            X,
            Y,
        };

        struct GamepadControl {
            enum Type {
                LSTICK,
                RSTICK,
                BUTTON,
                TRIGGER
            };

            enum Direction {
                NONE = -1,
                XM,
                XP,
                YM,
                YP
            };
            
            Type                       type;
            int                        index;
            GamepadControl::Direction  direction;
            int                        repeat = 0;
            std::string                label;
            std::string                command;
            GamepadControl(Type type, int index, GamepadControl::Direction direction, const std::string &label, const char *cmd) 
            : type(type), index(index), direction(direction), label(label), command(cmd) {}
        };

        struct Stick {
            GamepadControl::Type type;
            std::array<SDL_GameControllerAxis, 2> axes;
            Stick(GamepadControl::Type type, std::array<SDL_GameControllerAxis, 2> axes) : type(type), axes(axes) {}
        };

        struct GamepadInfo {
            GamepadControl::Type type;
            GamepadControl::Direction direction;
            int index;
        };

        std::vector<Controller> controllers;
        std::vector<GamepadControl> controls;
        std::vector<Stick> sticks;
        std::array<std::array<int, 2>, 2> axis_values;
        GamepadControl *selected_axis = nullptr;
#ifdef __unix__
        constexpr static float max_opposing = sin((GAMEPAD_AXIS_RANGE / 2.f) * PI / 180.f);
#else
        float max_opposing = sin((GAMEPAD_AXIS_RANGE / 2.f) * PI / 180.f);
#endif
        int delay_period;
        int repeat_period;

    public:
        bool connected;
        int init();
        void connect(int device_index, bool raise_error);
        void disconnect(int id);
        void add_control(const char *label, const char *cmd);
        void check_state();
        void poll();
};

struct Hotkey {
    SDL_Keycode keycode;
    std::string command;
    Hotkey(SDL_Keycode keycode, std::string_view command) : keycode(keycode), command(command) {};
};

class HotkeyList {
    private:
        std::vector<Hotkey> list;

    public:
        void add(const char *value);
        std::vector<Hotkey>::iterator begin(void);
        std::vector<Hotkey>::iterator end(void);
};

struct Ticks {
    Uint32 main;
    Uint32 application_launch;
    Uint32 last_input;
};


struct State {
    bool application_launching = false;
    bool application_running = false;
};


#ifdef __GNUC__
#define COMPILER_INFO(f, end) f("Compiler:   GCC {}.{}" end, __GNUC__, __GNUC_MINOR__);
#endif

#ifdef _MSC_VER
#define COMPILER_INFO(f, end) f("Compiler:   Microsoft C/C++ {:.2f}" end, (float) _MSC_VER / 100.0f);
#endif


#define VERSION(name, f, end)                                                                       \
static void name##_version() {                                                                      \
    SDL_version sdl_version;                                                                        \
    SDL_GetVersion(&sdl_version);                                                                   \
    const SDL_version *img_version = IMG_Linked_Version();                                          \
    const SDL_version *ttf_version = TTF_Linked_Version();                                          \
    const SDL_version *mix_version = Mix_Linked_Version();                                          \
    f(PROJECT_NAME " version " PROJECT_VERSION ", using:" end);                                     \
    f("    SDL        {}.{}.{}" end, sdl_version.major, sdl_version.minor, sdl_version.patch);      \
    f("    SDL_image  {}.{}.{}" end, img_version->major, img_version->minor, img_version->patch);   \
    f("    SDL_ttf    {}.{}.{}" end, ttf_version->major, ttf_version->minor, ttf_version->patch);   \
    f("    SDL_mixer  {}.{}.{}" end, mix_version->major, mix_version->minor, mix_version->patch);   \
    f(end);                                                                                         \
    f("Build date: " __DATE__ end);                                                                 \
    COMPILER_INFO(f, end);                                                                          \
}

void execute_command(const std::string &command);
void quit(int status);
