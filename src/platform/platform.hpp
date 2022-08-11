bool start_process(const std::string &command, bool application);

#ifdef __unix__
#define scmd_shutdown() start_process("systemctl poweroff", false)
#define scmd_restart()  start_process("systemctl reboot", false)
#define scmd_sleep()    start_process("systemctl suspend", false)
#endif