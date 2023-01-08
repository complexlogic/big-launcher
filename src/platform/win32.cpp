#include <windows.h>
#include <psapi.h>
#include <powrprof.h>
#include <string>
#include <algorithm>

#include <spdlog/spdlog.h>
#include <SDL.h>
#include <SDL_syswm.h>

#include "../main.hpp"
#include "platform.hpp"

static void parse_command(const std::string &command, std::string &file, std::string &params);
static bool get_shutdown_privilege();
static UINT sdl_to_win32_keycode(SDL_Keycode keycode);

struct KeycodeConversion {
    SDL_Keycode sdl;
    UINT win;
};

extern Display display;
HANDLE child_process            = nullptr;
bool has_shutdown_privilege     = false;
UINT exit_hotkey                = 0;

static void parse_command(const std::string &command, std::string &file, std::string &params)
{
    size_t begin = command.find_first_not_of(' ');
    if (begin == std::string::npos)
        return;
    size_t quote_begin = command.find_first_of('"');
    if (quote_begin != std::string::npos) {
        size_t quote_end = command.find_first_of('"', quote_begin + 1);
        if (quote_end != std::string::npos) {
            file = command.substr(quote_begin + 1, quote_end - quote_begin - 1);
            size_t params_begin = command.find_first_not_of(' ', quote_end + 1);
            if (params_begin != std::string::npos)
                params = command.substr(params_begin);
            return;
        }
    }
    size_t space = command.find_first_of(' ');
    if (space != std::string::npos) {
        file = command.substr(0, space);
        size_t params_begin = command.find_first_not_of(' ', space + 1);
        if (params_begin != std::string::npos)
            params = command.substr(params_begin);    
    }
    else
        file = command;
}

// A function to launch an application
bool start_process(const std::string &command, bool application)
{
    bool ret = false;
    std::string file;
    std::string params;
    
    // Parse command into file and parameters strings
    parse_command(command, file, params);

    // Set up info struct
    SHELLEXECUTEINFOA info = {
        .cbSize = sizeof(SHELLEXECUTEINFOA),
        .fMask = SEE_MASK_NOCLOSEPROCESS,
        .hwnd = nullptr,
        .lpVerb = "open",
        .lpFile = file.c_str(),
        .lpParameters = params.empty() ? nullptr : params.c_str(),
        .lpDirectory = nullptr,
        .nShow = application ? SW_SHOWMAXIMIZED : SW_HIDE,
        .lpIDList = nullptr,
        .lpClass = nullptr,
    };

    BOOL successful = ShellExecuteExA(&info);
    if (!application)
        ret = true;
    else {
        // Go down in the window stack so the launched application can take focus
        if (successful) {
            HWND hwnd = display.wm_info.info.win.window;
            SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOREDRAW | SWP_NOSIZE | SWP_NOMOVE);
            child_process = info.hProcess;
            ret = true;
        }
        else {
            spdlog::error("Failed to launch command");
            ret = false;
        }
    }
    return ret;
}

// A function to determine if the previously launched process is still running
bool process_running()
{
    DWORD status = WaitForSingleObject(child_process, 0);
    return status == WAIT_OBJECT_0 ? false : true;
}

void set_foreground_window()
{
    SetForegroundWindow(display.wm_info.info.win.window);
}

// A function to shutdown the computer
void scmd_shutdown()
{
    if (!has_shutdown_privilege) {
        bool successful = get_shutdown_privilege();
        if (!successful) 
            return;
    }
    InitiateShutdownA(nullptr, 
        nullptr, 
        0, 
        SHUTDOWN_FORCE_OTHERS | SHUTDOWN_POWEROFF | SHUTDOWN_HYBRID,
        SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER
    );
}

// A function to restart the computer
void scmd_restart()
{
    if (!has_shutdown_privilege) {
        bool successful = get_shutdown_privilege();
        if (!successful) 
            return;
    }
    InitiateShutdownA(nullptr,
        nullptr,
        0,
        SHUTDOWN_FORCE_OTHERS | SHUTDOWN_RESTART | SHUTDOWN_HYBRID,
        SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER
    );
}

// A function to put the computer to sleep
void scmd_sleep()
{
    if (!has_shutdown_privilege) {
        bool successful = get_shutdown_privilege();
        if (!successful) 
            return;
    }
    SetSuspendState(FALSE, FALSE, FALSE);
}

// A function to get the shutdown privilege from Windows
static bool get_shutdown_privilege()
{
    HANDLE token = nullptr;
    BOOL ret = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token);
    if (!ret) {
        spdlog::error("Could not open process token");
        return false;
    }
    LUID luid;
    ret = LookupPrivilegeValueA(nullptr, SE_SHUTDOWN_NAME, &luid);
    if (!ret) {
        spdlog::error("Failed to lookup privilege");
        CloseHandle(token);
        return false;
    }
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    ret = AdjustTokenPrivileges(token, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr);
    if (!ret) {
        spdlog::error("Failed to adjust token privileges");
        CloseHandle(token);
        return false;
    }
    has_shutdown_privilege = true;
    CloseHandle(token);
    return true;
}

// A function to check if there is an exit hotkey
bool has_exit_hotkey()
{
    return exit_hotkey ? true : false;
}

// A function to store an exit hotkey
void set_exit_hotkey(SDL_Keycode keycode)
{
    if (exit_hotkey) 
        return;
    exit_hotkey = sdl_to_win32_keycode(keycode);
    if (!exit_hotkey)
        spdlog::error("Invalid exit hotkey keycode %X", keycode);
}

// A function to register the exit hotkey with Windows
void register_exit_hotkey()
{
    BOOL ret = RegisterHotKey(display.wm_info.info.win.window, 1, 0, exit_hotkey);
    if (!ret) {
        exit_hotkey = 0;
        spdlog::error("Failed to register exit hotkey with Windows");
    }
}

// A function to check if the exit hotkey was pressed, and close the active window if so
void check_exit_hotkey(SDL_SysWMmsg *msg)
{
    if (msg->msg.win.msg == WM_HOTKEY) {
        spdlog::debug("Exit hotkey detected");
        HWND hwnd = GetForegroundWindow();
        if (hwnd == nullptr) {
            spdlog::error("Could not get top window");
            return;
        }
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }
}

// A function to convert an SDL keycode to a WIN32 virtual keycode
static UINT sdl_to_win32_keycode(SDL_Keycode keycode)
{
    static const KeycodeConversion table[] = {
        {SDLK_F1, VK_F1},
        {SDLK_F2, VK_F2},
        {SDLK_F3, VK_F3},
        {SDLK_F4, VK_F4},
        {SDLK_F5, VK_F5},
        {SDLK_F6, VK_F6},
        {SDLK_F7, VK_F7},
        {SDLK_F8, VK_F8},
        {SDLK_F9, VK_F9},
        {SDLK_F10, VK_F10},
        {SDLK_F11, VK_F11},
        {SDLK_F13, VK_F13},
        {SDLK_F14, VK_F14},
        {SDLK_F15, VK_F15},
        {SDLK_F16, VK_F16},
        {SDLK_F17, VK_F17},
        {SDLK_F18, VK_F18},
        {SDLK_F19, VK_F19},
        {SDLK_F20, VK_F20},
        {SDLK_F21, VK_F21},
        {SDLK_F22, VK_F22},
        {SDLK_F23, VK_F23},
        {SDLK_F24, VK_F24}
    };

    auto it = std::find_if(std::cbegin(table), std::cend(table), [=](const KeycodeConversion& i) {return i.sdl == keycode;});
    return it == std::cend(table) ? 0 : it->win;
}
