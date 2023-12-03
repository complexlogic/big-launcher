#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <vector>
#include <SDL.h>
#include <spdlog/spdlog.h>
#include "platform.hpp"

pid_t child_pid;

// A function to launch an external application
bool start_process(const std::string &command, bool application)
{
    child_pid = fork();
    switch(child_pid) {
        case -1:
            spdlog::error("Could not fork new process");
            return false;

        // Child process
        case 0:
            {
                const char *file = "/bin/sh";
                const char *args[] = {
                    "sh",
                    "-c", 
                    command.c_str(), 
                    nullptr
                };
                execvp(file, (char* const*) args);
            }
            break;

        // Parent process
        default:
            if (!application) 
                return true;
            int status;

            // Check to see if the shell successfully launched
            SDL_Delay(50);
            waitpid(child_pid, &status, WNOHANG);
            if (WIFEXITED(status) && WEXITSTATUS(status) > 126)
                return false;
            break;
    }
    return true;
}

bool process_running()
{
    pid_t pid = waitpid(-1*child_pid, nullptr, WNOHANG);
    if (pid > 0) {
        if (waitpid(-1*child_pid, nullptr, WNOHANG) == -1)
            return false;
    } 
    else if (pid == -1)
        return false;
    return true;
}
