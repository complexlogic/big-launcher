#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <vector>
#include <SDL.h>
#include <spdlog/spdlog.h>
#include "platform.hpp"


// A function to launch an external application
bool start_process(const std::string &command, bool application)
{
    pid_t child_pid = fork();
    switch(child_pid) {
        case -1:
            spdlog::error("Could not fork new process");
            return false;

        // Child process
        case 0:
            {
                const char *file = "/bin/sh";
                std::array<char*, 4> args = {
                    (char*) "sh",
                    (char*) "-c", 
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