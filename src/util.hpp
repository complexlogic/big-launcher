#pragma once

#include <string>
#include <span>
#include <SDL.h>
#include "main.hpp"
#include "sound.hpp"

#define MATCH(a,b) !strcmp(a,b)

enum FileType{
    TYPE_CONFIG,
    TYPE_FONT,
    TYPE_AUDIO
};

struct ConfigInfo {
    Gamepad &gamepad;
    HotkeyList &hotkey_list;
};

struct Config {
    SDL_Color sidebar_highlight_color = {0x00, 0x00, 0xFF, 0xFF};
    SDL_Color sidebar_text_color = {0xFF, 0xFF, 0xFF, 0xFF};
    SDL_Color sidebar_text_color_highlighted = {0xFF, 0xFF, 0xFF, 0xFF};
    SDL_Color menu_highlight_color = {0xFF, 0xFF, 0xFF, 0xFF};
    std::string background_image_path;
    bool mouse_select = false;
    bool debug = false;
    bool sound_enabled = false;
    int sound_volume = MAX_VOLUME;
    bool screensaver_enabled = false;
    Uint32 screensaver_idle_time = 900000;
    Uint8 screensaver_intensity = 170;
    bool gamepad_enabled;
    int gamepad_index;
    std::string gamepad_mappings_file;

    void parse(const std::string &file, Gamepad &gamepad, HotkeyList &hotkey_list);
    void add_int(const char *value, int &out);
    void add_string(const char *value, std::string &out);
    void add_bool(const char *value, bool &out);
    void add_path(const char *value, std::string &out);
    void add_time(const char *value, Uint32 &out, Uint32 min, Uint32 max);

    template <typename T>
    void add_percent(const char *value, T &out, T ref, float min = 0.0f, float max = 1.0f);

};

int utf8_length(std::string_view string);
Uint16 get_unicode_code_point(std::string_view string, int &bytes);
void utf8_truncate(const char *string, std::string &truncated_text, int width, int max_width);
bool hex_to_color(std::string_view string, SDL_Color &color);
void join_paths(std::string &out, std::initializer_list<const char*> list);
template <FileType file_type>
extern bool find_file(std::string &out, const char *filename);
