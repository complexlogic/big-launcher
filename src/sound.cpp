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
        if (!chunk->allocated) {
            free(chunk->abuf);
        }
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
#ifdef __unix__
    std::initializer_list<const char*> prefixes = {
        sound_dir_exe.c_str(),
        SYSTEM_SOUNDS_DIR
    };
#endif
    if (!find_file(click_path, CLICK_FILENAME, prefixes)) {
        spdlog::error("Could not locate audio file '{}'", CLICK_FILENAME);
        return 1;
    }
    if (!find_file(select_path, SELECT_FILENAME, prefixes)) {
        spdlog::error("Could not locate audio file '{}'", SELECT_FILENAME);
        return 1;
    }

    return this->connect();
}

int Sound::connect()
{
    int ret = Mix_OpenAudioDevice(44100,
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
    spdlog::debug("Opened {} channel audio at {} Hz", channels, frequency);

    // Load audio if needed
    if (click.frequency != frequency || click.channels != channels) {
        click.free_chunk();
        if (click.load(click_path, frequency, channels)) {
            return 1;
        }
    }
    if (select.frequency != frequency || select.channels != channels) {
        select.free_chunk();
        if (select.load(select_path, frequency, channels)) {
            return 1;
        }
    }

    // Set volume
    if (config.sound_volume != 100) {
        this->set_volume(0, config.sound_volume);
    }

    return 0;
}


int Sound::set_volume(int channel, int log_volume)
{
    if (log_volume > 100 || log_volume < 0)
        return 1;

    int volume;
    if (log_volume == 0) {
        volume = 0;
    }
    else if (log_volume == 100) {
        volume = MIX_MAX_VOLUME;
    }
    else {
        volume = (int) std::round(0.01f * pow(10.f, (double) log_volume * 0.02f) * (double) MIX_MAX_VOLUME);
    }

    Mix_Volume(channel, volume);
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