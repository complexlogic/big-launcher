#pragma once

#include <SDL.h>
#include <SDL_mixer.h>
#include <vector>
#include <set>

#define DISPLAY_ASPECT_RATIO 1.77777778
#define DISPLAY_ASPECT_RATIO_TOLERANCE 0.01f
#define APPLICATION_WAIT_PERIOD 100
#define APPLICATION_TIMEOUT 10000

#define GAMEPAD_DEADZONE 10000
#define GAMEPAD_REPEAT_DELAY 500
#define GAMEPAD_REPEAT_INTERVAL 25
#define PI 3.14159f
#define GAMEPAD_AXIS_RANGE 60 // degrees

class Display {
    public:
        SDL_Renderer *renderer;
        SDL_Window *window;
        SDL_DisplayMode dm;
        SDL_RendererInfo ri;
        int width;
        int height;

        Display();
        void init();
        void create_window();
        void close();
        void print_debug_info();
};

enum ControlType {
    TYPE_LSTICK,
    TYPE_RSTICK,
    TYPE_BUTTON,
    TYPE_TRIGGER
};

enum StickDirection {
    DIRECTION_NONE = -1,
    DIRECTION_XM,
    DIRECTION_XP,
    DIRECTION_YM,
    DIRECTION_YP,
};

struct Stick {
    ControlType type;
    std::array<SDL_GameControllerAxis, 2> axes;
    Stick(ControlType type, std::array<SDL_GameControllerAxis, 2> axes) : type(type), axes(axes){};
};

struct GamepadInfo{
    const char *label;
    ControlType type;
    StickDirection direction;
    int index;
};

enum AxisType{
    AXIS_X,
    AXIS_Y,
};

struct GamepadControl {
    ControlType    type;
    int            index;
    StickDirection direction;
    int            repeat;
    const char     *label;
    std::string    command;
    GamepadControl(ControlType type, int index, StickDirection direction, const char *label, const char *cmd) 
    : type(type), index(index), direction(direction), repeat(0), label(label), command(cmd){};
};


class Gamepad {
    private:
        SDL_GameController *controller;
        std::vector<GamepadControl> controls;
        std::vector<Stick> sticks;
        std::array<std::array<int, 2>, 2> axis_values;
        GamepadControl *selected_axis;
        static constexpr float max_opposing = sin((GAMEPAD_AXIS_RANGE / 2.f) * PI / 180.f);
        int delay_period;
        int repeat_period;

    public:
        bool connected;
        Gamepad();
        int init();
        void connect(int device_index, bool raise_error);
        void disconnect(void);
        void add_control(const char *label, const char *cmd);
        void poll();
};

struct Hotkey {
    SDL_Keycode keycode;
    std::string command;
    Hotkey(SDL_Keycode keycode, char *command) : keycode(keycode), command(command) {};
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
    bool application_launching;
    bool application_running;
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
