#ifndef HAS_UTIL_H
#define HAS_UTIL_H

#include <string>
#include <span>
#include <SDL.h>
#include "main.hpp"
#define MATCH(a,b) !strcmp(a,b)

typedef enum {
    TYPE_CONFIG,
    TYPE_FONT,
    TYPE_AUDIO
} FileType;

typedef struct {
    Gamepad &gamepad;
    HotkeyList &hotkey_list;
} ConfigInfo;

class Config {
public:

    SDL_Color sidebar_highlight_color;
    SDL_Color sidebar_text_color;
    SDL_Color sidebar_text_color_highlighted;
    SDL_Color menu_highlight_color;

    std::string background_image_path;

    bool mouse_select;
    bool debug;

    bool sound_enabled;
    int sound_volume;

    bool screensaver_enabled;
    Uint32 screensaver_idle_time;
    Uint8 screensaver_intensity;

    bool gamepad_enabled;
    int gamepad_index;
    std::string gamepad_mappings_file;



    Config();
    void parse(const std::string &file, Gamepad &gamepad, HotkeyList &hotkey_list);
    void add_int(const char *value, int &out);
    void add_string(const char *value, std::string &out);
    void add_bool(const char *value, bool &out);
    void add_path(const char *value, std::string &out);
    void add_time(const char *value, Uint32 &out, Uint32 min, Uint32 max);
    
    template <typename T>
    void add_percent(const char *value, T &out, T ref, float min = 0.0f, float max = 1.0f);

};

int utf8_length(const char *string);
Uint16 get_unicode_code_point(const char *p, int &bytes);
void utf8_truncate(const char *string, std::string &truncated_text, int width, int max_width);
bool hex_to_color(const char *string, SDL_Color &color);
void join_paths(std::string &out, std::initializer_list<const char*> list);
template <FileType file_type>
extern bool find_file(std::string &out, const char *filename);


#endif