#pragma once

#include <SDL_mixer.h>

#define MAX_VOLUME 10
#define RANGE_DB -40


class Sound {
    private:
        struct Chunk {
            Mix_Chunk *chunk = nullptr;
            int frequency = -1;
            int channels = -1;

            bool load(const std::string &path, int frequency, int channels);
            void free_chunk();
        };

        std::string click_path;
        std::string select_path;
        Chunk click;
        Chunk select;
        int frequency = -1;
        int channels = -1;

    public:
        bool connected = false;

        bool init();
        bool connect();
        void set_volume(int channel, int log_volume);
        void disconnect();
        void play_click();
        void play_select();

};
