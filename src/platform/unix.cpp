#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <vector>
#include <SDL.h>
#include <spdlog/spdlog.h>
#include "platform.hpp"
#include "unix.hpp"


// A function to launch an external application
bool start_process(const std::string &command, bool application)
{
    pid_t child_pid = fork();
    switch(child_pid) {
        case -1:
            spdlog::error("Error: Could not fork new process for application");
            return false;

        // Child process
        case 0:
            {
                const char *file = "/bin/sh";
                std::array<char*, 4> args = {
                    "sh",
                    "-c", 
                    (char*) command.c_str(), 
                    NULL
                };
                execvp(file, args.data());
            }
            break;

        // Parent process
        default:
            if (!application) 
                return true;
            int status;

            // Check to see if the shell successfully launched
            SDL_Delay(50);
            pid_t pid = waitpid(child_pid, &status, WNOHANG);
            if (WIFEXITED(status) && WEXITSTATUS(status) > 126)
                return false;
            break;
    }
    return true;
}

// A function to shutdown the computer
void scmd_shutdown()
{
    std::string string = CMD_SHUTDOWN;
    start_process(string, false);
}

// A function to restart the computer
void scmd_restart()
{
    std::string string = CMD_RESTART;
    start_process(string, false);
}

// A function to put the computer to sleep
void scmd_sleep()
{
    std::string string = CMD_SLEEP;
    start_process(string, false);
}