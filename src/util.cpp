#include <filesystem>
#include <span>
#include <initializer_list>
#include <string.h>
#include <SDL.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <ini.h>
#include "sound.hpp"
#include "main.hpp"
#include "util.hpp"


extern "C" {
    static int handler(void* user, const char* section, const char* name, const char* value);
}

extern Config config;

Config::Config()
{
    debug = false;
    sidebar_highlight_color = {0x00, 0x00, 0x00, 0xFF};
    sidebar_text_color = {0xFF, 0xFF, 0xFF, 0xFF};
    sidebar_text_color_highlighted = {0xFF, 0xFF, 0xFF, 0xFF};
    mouse_select = false;
    sound_enabled = false;
    sound_volume = MAX_VOLUME;
}

void Config::parse(const std::string &file)
{
    spdlog::debug("Parsing config file '{}'", file);
    if (ini_parse(file.c_str(), handler, NULL) < 0) {
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
    std::string v = value;
    if (*(v.end() -1) != '%')
        return;
    float percent = atof(v.substr(0, v.size() - 1).c_str()) / 100.0f;
    if (percent == 0.0f && strcmp(value, "0%"))
        return;
    if (percent < min)
        percent = min;
    else if (percent > max)
        percent = max;
    
    out = static_cast<T>(percent * static_cast<float>(ref));
}


static int handler(void* user, const char* section, const char* name, const char* value)
{
    if (MATCH(section, "Settings")) {
        if (MATCH(name, "MouseSelect")) {
            config.add_bool(value, config.mouse_select);
        }
        else if (MATCH(name, "HighlightColor")) {
            hex_to_color(value, config.sidebar_highlight_color);
        }
        else if (MATCH(name, "TextColor")) {
            hex_to_color(value, config.sidebar_text_color);
        }
        else if (MATCH(name, "TextSelectedColor")) {
            hex_to_color(value, config.sidebar_text_color_highlighted);
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
        path /= *it;
    }
    out = path.string();
}

bool find_file(std::string &out, const char *filename, const std::initializer_list<const char*> &prefixes)
{
    //std::filesystem::path path;
    for (const char *prefix : prefixes) {
        join_paths(out, {prefix, filename});
        //path = out;
        if (std::filesystem::exists(out))
            return true;
    }
    out.clear();
    return false;
}