#pragma once

#include <string>
#include <vector>
#include <set>
#include <SDL.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "image.hpp"
#include "screensaver.hpp"

#define SIDEBAR_SHIFT_TIME 200.0f
#define ROW_SHIFT_TIME 120.0f
#define HIGHLIGHT_SHIFT_TIME 100.0f

#define ENTRY_PRESS_TIME 100
#define ENTRY_SHRINK_DISTANCE 0.04f

#define COLUMNS 3
#define TOP_MARGIN 0.2f
#define BOTTOM_MARGIN 1.0f

// Sidebar geometry
#define SIDEBAR_HIGHLIGHT_LEFT 0.08f
#define SIDEBAR_HIGHLIGHT_RIGHT 0.21f
#define SIDEBAR_HIGHLIGHT_WIDTH SIDEBAR_HIGHLIGHT_RIGHT - SIDEBAR_HIGHLIGHT_LEFT
#define SIDEBAR_HIGHLIGHT_HEIGHT 0.06f
#define SIDEBAR_CORNER_RADIUS 0.011f
#define SIDEBAR_FONT_SIZE 0.55
#define SIDEBAR_Y_ADVANCE 0.068f
#define SIDEBAR_TEXT_MARGIN 0.07f

// Menu card geometry
#define CARD_LEFT_MARGIN 0.42F
#define CARD_RIGHT_MARGIN 0.92F
#define CARD_SPACING 0.011F
#define CARD_WIDTH CARD_RIGHT_MARGIN - CARD_LEFT_MARGIN
#define CARD_ASPECT_RATIO 1.33333333F
#define CARD_ICON_MARGIN 0.12F
#define MAX_CARD_ICON_MARGIN 0.2f
#define ERROR_ICON_MARGIN 0.35F

// Menu highlight geometry
#define HIGHLIGHT_THICKNESS 0.5f
#define HIGHLIGHT_INNER_SPACING 0.25f
#define MENU_HIGHLIGHT_RX 0.02f
#define SHADOW_ALPHA_HIGHLIGHT 0.6f

#define set_texture_color(texture, color) SDL_SetTextureColorMod(texture, color.r, color.g, color.b)

#define HIGHLIGHT_FORMAT "<svg viewBox=\"0 0 {} {}\"><rect x=\"0\" width=\"{}\" height=\"{}\" rx=\"{}\" fill=\"#{:02x}{:02x}{:02x}\"/></svg>"
#define format_highlight(w, h, rx, color) fmt::format(HIGHLIGHT_FORMAT, w, h, w, h, rx, color.r, color.g, color.b)

#define MENU_HIGHLIGHT_FORMAT "<svg viewBox=\"0 0 {} {}\"><rect width=\"100%\" height=\"100%\" rx=\"{}\" fill=\"#{:02x}{:02x}{:02x}\" /><rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\" rx=\"{}\"  fill=\"#{:02x}{:02x}{:02x}\"/></svg>"
#define format_menu_highlight(w, h, color, mask_color, w_inner, h_inner, t, rx_outter, rx_inner) fmt::format(MENU_HIGHLIGHT_FORMAT, w, h, rx_outter, color.r, color.g, color.b, t, t, w_inner, h_inner, rx_inner, mask_color.r, mask_color.g, mask_color.b)

enum class Direction {
    UP,
    DOWN,
    LEFT,
    RIGHT
};

class Layout {
    private:
        enum SelectionMode {
            SIDEBAR,
            MENU
        };

        // Layout subclasses
        struct SidebarEntry {
            enum Type {
                MENU,
                COMMAND
            };
            Type type;
            std::string title;
            SDL_Surface *surface = nullptr;
            SDL_Texture *texture = nullptr;
            SDL_Rect src_rect;
            SDL_Rect dst_rect;
            
            SidebarEntry(const char *title, Type type) : title(title), type(type) {}
        };

        struct Menu : public SidebarEntry {
            struct Entry {
                enum CardType {
                    CUSTOM,
                    GENERATED
                };

                CardType card_type;
                std::string title;
                std::string command;
                SDL_Color background_color { 0xFF, 0xFF, 0xFF, 0xFF };
                std::string path; // doubles for both card path and background in generated mode
                std::string icon_path;

                SDL_Surface *surface = nullptr;
                SDL_Rect rect;
                SDL_Surface *icon_surface = nullptr;
                SDL_Rect icon_rect;
                float icon_margin = CARD_ICON_MARGIN;
                SDL_Texture *texture = nullptr;
                bool card_error = false;

                Entry(const char *title, const char *command) : title(title), command(command) {}
                void add_card(const char *path);
                void add_card(SDL_Color &background_color, const char *path);
                void add_card(const char *background_path, const char *icon_path);
                void add_margin(const char *value);
            };

            std::vector<Entry> entry_list;
            int y_offset = 0;
            int row = 0;
            int column = 0;
            int total_rows = 0;
            int max_columns = 0;
            int shift_count = 0;
            int height;

            std::vector<Entry>::iterator current_entry;
            Menu(const char *title) : SidebarEntry(title, MENU) {}
            int parse(xmlNodePtr node);
            void add_entry(xmlNodePtr node);
            size_t num_entries();
            bool render_surfaces(int shadow_offset, int w, int h, int x_start, int y_start, int spacing, int screen_height);
            void render_card_textures(SDL_Renderer *renderer, SDL_Texture *card_shadow_texture, int shadow_offset, int card_w, int card_h);
            void draw_entries(SDL_Renderer *renderer, int y_min, int y_max);
            void print_entries();
        };

        struct Command : public SidebarEntry {
            std::string command;
            Command(const char *title, const char *command) : SidebarEntry(title, COMMAND), command(command) {}
        };

        struct PressedEntry {
            Menu::Entry &entry;
            SDL_Rect original_rect;
            int total;
            Uint32 ticks;
            int current;
            float velocity;
            Direction direction;
            float aspect_ratio;

            PressedEntry(Menu::Entry &entry);
            bool update();
        };

        struct Shift {
            enum Type {
                SIDEBAR,
                MENU,
                HIGHLIGHT
            };

            Type type;
            Menu *menu = nullptr;
            Direction direction;
            Uint32 ticks;
            float velocity;
            int total;
            int target;
        };

        struct SidebarHighlight {
            SDL_Surface *surface = nullptr;
            SDL_Texture *texture = nullptr;
            SDL_Rect rect;
            int w;
            int h;
            int shadow_offset;

            void render_surface(int w, int h, int rx);
            void render_texture(SDL_Renderer *renderer);
                
        };

        struct MenuHighlight {
            SDL_Surface *surface = nullptr;
            SDL_Texture *texture = nullptr;
            SDL_Rect rect;

            void render_surface(int x, int y, int w, int h, int t, int shadow_offset);
            void render_texture(SDL_Renderer *renderer);

        };

        // Layout class members
        SDL_Renderer *renderer = nullptr;
        int screen_width;
        int screen_height;
        float f_screen_width;
        float f_screen_height;

        SDL_Surface *background_surface = nullptr;
        SDL_Texture *background_texture = nullptr;

        bool card_error = false;
        SDL_Surface *error_bg = nullptr;
        SDL_Surface *error_icon = nullptr;
        SDL_Rect error_icon_rect;
        SDL_Texture *error_texture = nullptr;

        // States
        std::vector<Shift> shift_queue;
        std::set<Menu*> visible_menus;
        SelectionMode selection_mode = SelectionMode::SIDEBAR;
        Menu *current_menu = nullptr;

        // Sidebar
        std::vector<SidebarEntry*> list;
        std::vector<SidebarEntry*>::iterator current_entry;
        Font sidebar_font;
        SidebarHighlight sidebar_highlight;
        int sidebar_pos = 0;
        int sidebar_y_advance;
        int y_min;
        int y_max;
        int max_sidebar_entries = -1;
        int sidebar_shift_count;
        int num_sidebar_entries = 0;

        // Menu entry cards
        int card_w;
        int card_h;
        int card_x0;
        int card_y0;
        int card_y_advance;
        int max_rows;
        SDL_Surface *card_shadow = nullptr;
        SDL_Texture *card_shadow_texture = nullptr;
        SDL_Rect cr;
        int card_shadow_offset;

        // Highlight
        MenuHighlight menu_highlight;
        int highlight_x0;
        int highlight_y0;
        int highlight_x_advance;
        int highlight_y_advance;

        PressedEntry *pressed_entry = nullptr;
        Screensaver screensaver;

    public:
        void parse(const std::string &file);
        void add_entry();
        void load_surfaces(int screen_width, int screen_height);
        void load_textures(SDL_Renderer *renderer);
        void render_error_surface();
        void render_error_texture();
        void update();
        void draw();
        void move_down();
        void move_up();
        void move_left();
        void move_right();
        void select();
        void add_shift(Shift::Type type, Direction direction, int target, float time, Menu *menu);
        void shift();
};
