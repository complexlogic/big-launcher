#include <string>
#include <math.h>

#include <SDL.h>
#include <SDL_mixer.h>
#include <spdlog/spdlog.h>

#include <lconfig.h>
#include "sound.hpp"
#include "util.hpp"

extern char *executable_dir;
extern Config config;

SoundBite::SoundBite()
{
    chunk = NULL;
    frequency = -1;
    channels = -1;
}

void SoundBite::free_chunk()
{
    if (chunk != NULL) {
        if (!chunk->allocated)
            free(chunk->abuf);
        Mix_FreeChunk(chunk);
    }
}

int SoundBite::load(const std::string &path, int frequency, int channels)
{
    chunk = Mix_LoadWAV(path.c_str());
    if (chunk == NULL) {
        spdlog::error("Failed to load audio file '{}'", path);
        return 1;
    }
    this->frequency = frequency;
    this->channels = channels;
    return 0;
}


Sound::Sound()
{
    frequency = -1;
    channels = -1;
    connected = false;
}

int Sound::init()
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        spdlog::error("Failed to initialize audio");
        return 1;
    }
    spdlog::debug("Successfully initialized audio");

    // Locate sound files but don't load them yet
    std::string sound_dir_exe;
    join_paths(sound_dir_exe, {executable_dir, "assets", "sounds"});
    if (!find_file<TYPE_AUDIO>(click_path, CLICK_FILENAME)) {
        spdlog::error("Could not locate audio file '{}'", CLICK_FILENAME);
        return 1;
    }
    if (!find_file<TYPE_AUDIO>(select_path, SELECT_FILENAME)) {
        spdlog::error("Could not locate audio file '{}'", SELECT_FILENAME);
        return 1;
    }

    return this->connect();
}

int Sound::connect()
{
    spdlog::debug("Opening audio device...");
    int ret = Mix_OpenAudioDevice(48000,
                  AUDIO_S16SYS,
                  2,
                  1024,
                  NULL,
                  SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE
              );
    if (ret == -1) {
        spdlog::error("Could not connect to audio device");
        return 1;
    }
    connected = true;
    Mix_QuerySpec(&frequency, NULL, &channels);

    // Load audio if needed
    if (click.frequency != frequency || click.channels != channels) {
        click.free_chunk();
        if (click.load(click_path, frequency, channels))
            return 1;
    }
    if (select.frequency != frequency || select.channels != channels) {
        select.free_chunk();
        if (select.load(select_path, frequency, channels))
            return 1;
    }

    // Set volume
    if (config.sound_volume != MAX_VOLUME)
        this->set_volume(0, config.sound_volume);

    spdlog::debug("Successfully opened {} channel audio at {} Hz", channels, frequency);
    return 0;
}

int Sound::set_volume(int channel, int volume)
{
    if (volume > MAX_VOLUME || volume < 0)
        return 1;

    constexpr auto generate_array = []() {
        double a = pow(10.f, (double) RANGE_DB / 20.f);
        double b = log10(1.f / a) / (double) MAX_VOLUME;
        std::array<int, MAX_VOLUME + 1> arr;
        arr[0] = 0;
        arr[MAX_VOLUME] = MIX_MAX_VOLUME;
        for (int n = 1; n < MAX_VOLUME; n++)
            arr[n] = (int) std::round(a * pow(10.f, (double) n * b) * (double) MIX_MAX_VOLUME);
        return arr;
    };
    static constexpr std::array<int, MAX_VOLUME + 1> volume_array = generate_array();
    
    Mix_Volume(channel, volume_array[volume]);
    return 0;
}

void Sound::disconnect()
{
    Mix_CloseAudio();
    connected = false;
}

void Sound::play_click()
{
    Mix_PlayChannel(0, click.chunk, 0);
}

void Sound::play_select()
{
    Mix_PlayChannel(0, select.chunk, 0);
}