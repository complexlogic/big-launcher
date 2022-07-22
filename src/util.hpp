#include <string>
#include <span>
#include <SDL.h>
#define MATCH(a,b) !strcmp(a,b)

class Config {
public:

    SDL_Color sidebar_highlight_color;
    SDL_Color sidebar_text_color;
    SDL_Color sidebar_text_color_highlighted;

    std::string background_image_path;

    bool mouse_select;
    bool debug;


    Config();
    void parse(const std::string &file);
    void add_int(const char *value, int &out);
    void add_string(const char *value, std::string &out);
    void add_bool(const char *value, bool &out);
    void add_path(const char *value, std::string &out);
    //void add_color(const char *value, SDL_Color &color);


};




int utf8_length(const char *string);
Uint16 get_unicode_code_point(const char *p, int &bytes);
void utf8_truncate(char *string, int width, int max_width);
void copy_string_alloc(char **dest, const char *string);
bool hex_to_color(const char *string, SDL_Color &color);
void join_paths(std::string &out, std::initializer_list<const char*> list);
bool find_file(std::string &out, const char *filename, const std::initializer_list<const char*> &prefixes);