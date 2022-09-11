#include <filesystem>
#include <span>
#include <initializer_list>
#include <string.h>
#include <SDL.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <ini.h>
#include <lconfig.h>
#include "sound.hpp"
#include "main.hpp"
#include "util.hpp"
#include "screensaver.hpp"


extern "C" int handler(void* user, const char* section, const char* name, const char* value);
extern Config config;
extern char *executable_dir;

Config::Config()
{
    debug = false;
    sidebar_highlight_color = {0x00, 0x00, 0xFF, 0xFF};
    sidebar_text_color = {0xFF, 0xFF, 0xFF, 0xFF};
    sidebar_text_color_highlighted = {0xFF, 0xFF, 0xFF, 0xFF};
    menu_highlight_color = {0xFF, 0xFF, 0xFF, 0xFF};
    mouse_select = false;
    sound_enabled = false;
    sound_volume = MAX_VOLUME;

    screensaver_enabled = false;
    screensaver_idle_time = 900000;
    screensaver_intensity = 170;
}

void Config::parse(const std::string &file, Gamepad &gamepad, HotkeyList &hotkey_list)
{
    ConfigInfo info = {gamepad, hotkey_list};
    spdlog::debug("Parsing config file '{}'", file);
    if (ini_parse(file.c_str(), handler, (void*) &info) < 0) {
        spdlog::critical("Failed to parse config file");
        quit(EXIT_FAILURE);
    }
    spdlog::debug("Sucessfully parsed config file");
}

void Config::add_bool(const char *value, bool &out)
{
    if (MATCH(value, "true") || MATCH(value, "True")) {
        out = true;
    }
    else if (MATCH(value, "false") || MATCH(value, "False")) {
        out = false;
    }
}

void Config::add_int(const char *value, int &out)
{
    int x = std::stoi(value);
    if (x || *value == '0') {
        out = x;
    }
}

void Config::add_time(const char *value, Uint32 &out, Uint32 min, Uint32 max)
{
    int x = std::stoi(value) * 1000;
    if ((x || *value == '0') && x >= min && x <=max) {
        out = x;
    }
}

void Config::add_path(const char *value, std::string &out)
{
    out = value;

    // Remove double quotes
    if (*out.begin() == '"' && *(out.end() - 1) == '"') {
        out = out.substr(1, out.size() - 2);
    }
}

template <typename T>
void Config::add_percent(const char *value, T &out, T ref, float min, float max)
{
    std::string_view string = value;
    if (string.back() != '%')
        return;
    float percent = atof(std::string(string, 0, string.size() - 1).c_str()) / 100.0f;
    if (percent == 0.0f && strcmp(value, "0%"))
        return;
    if (percent < min)
        percent = min;
    else if (percent > max)
        percent = max;
    
    out = static_cast<T>(percent * static_cast<float>(ref));
}

int handler(void* user, const char* section, const char* name, const char* value)
{
    if (MATCH(section, "Settings")) {
        if (MATCH(name, "MouseSelect")) {
            config.add_bool(value, config.mouse_select);
        }
        else if (MATCH(name, "SidebarHighlightColor")) {
            hex_to_color(value, config.sidebar_highlight_color);
        }
        else if (MATCH(name, "SidebarTextColor")) {
            hex_to_color(value, config.sidebar_text_color);
        }
        else if (MATCH(name, "SidebarTextSelectedColor")) {
            hex_to_color(value, config.sidebar_text_color_highlighted);
        }
        else if (MATCH(name, "MenuHighlightColor")) {
            hex_to_color(value, config.menu_highlight_color);
        }

        else if (MATCH(name, "BackgroundImage")) {
            config.add_path(value, config.background_image_path);
        }
    }

    else if (MATCH(section, "Sound")) {
        if (MATCH(name, "Enabled")) {
            config.add_bool(value, config.sound_enabled);
        }
        else if (MATCH(name, "Volume")) {
            config.add_int(value, config.sound_volume);
        }
    }

    else if (MATCH(section, "Screensaver")) {
        if (MATCH(name, "Enabled")) {
            config.add_bool(value, config.screensaver_enabled);
        }
        else if (MATCH(name, "IdleTime")) {
            config.add_time(value, config.screensaver_idle_time, MIN_SCREENSAVER_IDLE_TIME, MAX_SCREENSAVER_IDLE_TIME);
        }
        else if (MATCH(name, "Intensity")) {
            config.add_percent<Uint8>(value, config.screensaver_intensity, 255, 0.1f, 1.f);
        }
    }

    else if (MATCH(section, "Hotkeys")) {
        ConfigInfo *info = (ConfigInfo*) user;
        info->hotkey_list.add(value);
    }

    else if (MATCH(section, "Gamepad")) {
        if (MATCH(name, "Enabled")) {
            config.add_bool(value, config.gamepad_enabled);
        }
        else if (MATCH(name, "DeviceIndex")) {
            config.add_int(value, config.gamepad_index);
        }
        else if (MATCH(name, "MappingsFile")) {
            config.add_path(value, config.gamepad_mappings_file);
        }
        else {
            ConfigInfo *info = (ConfigInfo*) user;
            info->gamepad.add_control(name, value);
        }
    }
    return 0;
}


// Calculates the length of a utf-8 encoded C-style string
int utf8_length(const char *string)
{
    int length = 0;
    char *ptr = (char*) string;
    while (*ptr != '\0') {
        // If byte is 0xxxxxxx, then it's a 1 byte (ASCII) char
        if ((*ptr & 0x80) == 0) {
            ptr++;
        }
        // If byte is 110xxxxx, then it's a 2 byte char
        else if ((*ptr & 0xE0) == 0xC0) {
            ptr +=2;
        }
        // If byte is 1110xxxx, then it's a 3 byte char
        else if ((*ptr & 0xF0) == 0xE0) {
            ptr +=3;
        }
        // If byte is 11110xxx, then it's a 4 byte char
        else if ((*ptr & 0xF8) == 0xF0) {
            ptr+=4;
        }
    length++;
    }
    return length;
}

// A function to extract the Unicode code point from the first character in a UTF-8 encoded C-style string
Uint16 get_unicode_code_point(const char *p, int &bytes)
{
    Uint16 result;

    // 1 byte ASCII char
    if ((*p & 0x80) == 0) {
        result = (Uint16) *p;
        bytes = 1;
    }

    // If byte is 110xxxxx, then it's a 2 byte char
    else if ((*p & 0xE0) == 0xC0) {
        Uint8 byte1 = *p & 0x1F;
        Uint8 byte2 = *(p + 1) & 0x3F;
        result = (Uint16) ((byte1 << 6) + byte2);
        bytes = 2;
    }

    // If byte is 1110xxxx, then it's a 3 byte char
    else if ((*p & 0xF0) == 0xE0) {
        Uint8 byte1 = *p & 0x0F;
        Uint8 byte2 = *(p + 1) & 0x3F;
        Uint8 byte3 = *(p + 2) & 0x3F;
        result = (Uint16) ((byte1 << 12) + (byte2 << 6) + byte3);
        bytes = 3;
    }
    else {
        result = 0;
        bytes = 1;
    }
    return result;
}

// A function to truncate a utf-8 encoded string to max number of pixels
void utf8_truncate(char *string, int width, int max_width)
{
    int string_length = utf8_length(string);
    int avg_width = width / string_length;
    int num_chars = max_width / avg_width;
    int spaces = (string_length - num_chars) + 3; // Number of spaces to go back
    char *ptr = string + strlen(string); // Change to null character of string
    int chars = 0;

    // Go back required number of spaces
    do {
        ptr--;
        if (!(*ptr & 0x80)) { // ASCII characters have 0 as most significant bit
            chars++;
        }
        else { // Non-ASCII character detected
            do {
                ptr--;
            } while (ptr > string && (*ptr & 0xC0) == 0x80); // Non-ASCII most significant byte begins with 0b11
            chars++;
        }
    } while (chars < spaces);

    // Add "..." to end of string to inform user of truncation
    if (strlen(ptr) > 2) {
        *ptr = '.';
        *(ptr + 1) = '.';
        *(ptr + 2) = '.';
        *(ptr + 3) = '\0';
    }
}

// Allocates memory and copies a variable length C-style string
void copy_string_alloc(char **dest, const char *string)
{
    int length = strlen(string);
    if (length) {
        *dest = (char*) malloc(sizeof(char)*(length + 1));
        strcpy(*dest, string);
    }
    else {
        *dest = NULL;
    }
}

// A function to convert a hex-formatted string into a color struct
bool hex_to_color(const char *string, SDL_Color &color)
{
    if (*string != '#')
        return false;
    char *p = (char*) string + 1;

    // If strtoul returned 0, and the hex string wasn't 000..., then there was an error
    int length = strlen(p);
    if (length != 6)
        return false;
    Uint32 hex = (Uint32) strtoul(p, NULL, 16);
    if (!hex && strcmp(p,"000000")) {
        return false;
    }

    color.r = (Uint8) (hex >> 16);
    color.g = (Uint8) ((hex & 0x0000ff00) >> 8);
    color.b = (Uint8) (hex & 0x000000ff);
    color.a = 0xFF;
    return true;
}

void join_paths(std::string &out, std::initializer_list<const char*> list)
{
    if (list.size() < 2)
        return;
    std::filesystem::path path;
    auto it = list.begin();
    path = *it;
    it++;
    for(it; it != list.end(); ++it) {
        if (*it == NULL)
            return;
        path /= *it;
    }
    out = path.string();
}

template <FileType file_type>
bool find_file(std::string &out, const char *filename)
{
    static std::vector<const char*> prefixes;
    static std::string str;
    if constexpr(file_type == TYPE_CONFIG) {
        if (!prefixes.size()) {
#ifdef __unix__
            prefixes.resize(4);
            join_paths(str, {getenv("HOME"), ".config", EXECUTABLE_TITLE});
            prefixes[0] = CURRENT_DIRECTORY;
            prefixes[1] = executable_dir;
            prefixes[2] = str.c_str();
            prefixes[3] = SYSTEM_SHARE_DIR;
#endif
        }
    }

    if constexpr(file_type == TYPE_FONT) {
        if (!prefixes.size()) {
#ifdef __unix__
            prefixes.resize(2);
            join_paths(str, {executable_dir, "assets", "fonts"});
            prefixes[0] = str.c_str();
            prefixes[1] = SYSTEM_FONTS_DIR;

#endif
        }
    }

    if constexpr(file_type == TYPE_AUDIO) {
        if (!prefixes.size()) {
#ifdef __unix__
            prefixes.resize(2);
            join_paths(str, {executable_dir, "assets", "sounds"});
            prefixes[0] = str.c_str();
            prefixes[1] = SYSTEM_SOUNDS_DIR;

#endif
        }
    }
    for (const char *prefix : prefixes) {
        join_paths(out, {prefix, filename});
        if (std::filesystem::exists(out))
            return true;
    }
    out.clear();
    return false;
}
template bool find_file<TYPE_CONFIG>(std::string &out, const char *filename);
template bool find_file<TYPE_FONT>(std::string &out, const char *filename);
template bool find_file<TYPE_AUDIO>(std::string &out, const char *filename);