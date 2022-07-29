#include <SDL_mixer.h>

class SoundBite {
    public:
        Mix_Chunk *chunk;
        int frequency;
        int channels;

        SoundBite();
        int load(const std::string &path, int frequency, int channels);
        void free_chunk();
};


class Sound {
    private:
        std::string click_path;
        std::string select_path;
        SoundBite click;
        SoundBite select;
        int frequency;
        int channels;

    public:
        bool connected;

        Sound();
        int init();
        int connect();
        int set_volume(int channel, int log_volume);
        void disconnect();
        void play_click();
        void play_select();

};