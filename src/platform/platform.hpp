#pragma once

bool start_process(const std::string &command, bool application);
bool process_running();

#ifdef __unix__
#define scmd_shutdown() start_process("systemctl poweroff", false)
#define scmd_restart()  start_process("systemctl reboot", false)
#define scmd_sleep()    start_process("systemctl suspend", false)
#endif

#ifdef _WIN32
void scmd_shutdown();
void scmd_restart();
void scmd_sleep();
bool has_exit_hotkey();
void set_exit_hotkey(SDL_Keycode keycode);
void register_exit_hotkey();
void check_exit_hotkey(SDL_SysWMmsg *msg);
void set_foreground_window();
#endif