#include <string>
#include <vector>
#include <set>
#include <SDL.h>
#include "image.hpp"
#define SIDEBAR_SHIFT_TIME 200.0f
#define ROW_SHIFT_TIME 120.0f
#define HIGHLIGHT_SHIFT_TIME 100.0f

#define COLUMNS 3
#define TOP_MARGIN 0.2f
//#define BOTTOM_MARGIN 0.85f
#define BOTTOM_MARGIN 1.0f
// Sidebar geometry
#define SIDEBAR_HIGHLIGHT_LEFT 0.08f
#define SIDEBAR_HIGHLIGHT_RIGHT 0.22f
#define SIDEBAR_HIGHLIGHT_WIDTH SIDEBAR_HIGHLIGHT_RIGHT - SIDEBAR_HIGHLIGHT_LEFT
#define SIDEBAR_HIGHLIGHT_HEIGHT 0.065f
#define SIDEBAR_CORNER_RADIUS 0.012f
#define SIDEBAR_FONT_SIZE 0.55
#define SIDEBAR_Y_ADVANCE 0.068f
#define SIDEBAR_TEXT_MARGIN 0.07f

// Menu card geometry
#define CARD_LEFT_MARGIN 0.4F
#define CARD_RIGHT_MARGIN 0.92F
#define CARD_SPACING 0.01F
#define CARD_WIDTH CARD_RIGHT_MARGIN - CARD_LEFT_MARGIN
#define CARD_ASPECT_RATIO 1.33333333F
#define CARD_ICON_MARGIN 0.10F

// Menu highlight geometry
#define HIGHLIGHT_THICKNESS 0.45f
#define HIGHLIGHT_INNER_SPACING 0.33f
#define MENU_HIGHLIGHT_RX 0.02f

#define set_texture_color(texture, color) SDL_SetTextureColorMod(texture, color.r, color.g, color.b)

#define HIGHLIGHT_FORMAT "<svg viewBox=\"0 0 {} {}\"><rect x=\"0\" width=\"{}\" height=\"{}\" rx=\"{}\" fill=\"#{:02x}{:02x}{:02x}\"/></svg>"
#define format_highlight(w, h, rx, color) fmt::format(HIGHLIGHT_FORMAT, w, h, w, h, rx, color.r, color.g, color.b)

#define MENU_HIGHLIGHT_FORMAT "<svg viewBox=\"0 0 {} {}\"><rect width=\"100%\" height=\"100%\" rx=\"{}\" fill=\"#ffffff\" /><rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\" rx=\"{}\"  fill=\"#0000ff\"/></svg>"
#define format_menu_highlight(w, h, w_inner, h_inner, t, rx_outter, rx_inner) fmt::format(MENU_HIGHLIGHT_FORMAT, w, h, rx_outter, t, t, w_inner, h_inner, rx_inner)

typedef enum  {
    MENU,
    COMMAND
} SidebarEntryType;

typedef enum {
    CUSTOM,
    GENERATED
} CardType;

typedef enum {
    DIRECTION_UP,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
    DIRECTION_RIGHT
} Direction;

typedef enum {
    SHIFT_SIDEBAR,
    SHIFT_MENU,
    SHIFT_HIGHLIGHT
} ShiftType;

typedef enum {
    SELECTION_SIDEBAR,
    SELECTION_MENU
} SelectionMode;

class Entry {
    public:
        CardType card_type;
        std::string title;
        std::string command;
        SDL_Color background_color;
        std::string path; // doubles for both card path and background in generated mode
        std::string icon_path;

        SDL_Surface *surface;
        SDL_Rect rect;
        SDL_Surface *icon_surface;
        SDL_Rect icon_rect;
        SDL_Texture *texture;

        Entry(const char *title, const char *command);
        ~Entry();
        void add_card(const char *path);
        void add_card(SDL_Color &background_color, const char *path);
        void add_card(const char *background_path, const char *icon_path);


};

class SidebarEntry {
    public:
        SidebarEntryType type;
        std::string title;
        SDL_Surface *surface;
        SDL_Texture *texture;
        SDL_Rect src_rect;
        SDL_Rect dst_rect;
};


class Menu : public SidebarEntry{
    private:
        

    public:
        std::vector<Entry*> entry_list;
        int y_offset;
        int row;
        int column;
        int total_rows;
        int max_columns;
        int shift_count;
        int height;

        std::vector<Entry*>::iterator current_entry;
        Menu(const char *title);
        //~Menu();
        void add_entry(Entry *entry);
        size_t num_entries();
        void render_surfaces(int w, int h, int x_start, int y_start, int spacing, int screen_height);
        void render_card_textures(SDL_Renderer *renderer);
        void draw_entries(SDL_Renderer *renderer, int y_min, int y_max);
        void print_entries();
};

typedef struct {
    ShiftType type;
    Menu *menu;
    Direction direction;
    Uint32 ticks;
    float velocity;
    int total;
    int target;

} Shift;

class Command : public SidebarEntry {
    public:
        std::string command;
        Command(const char *title, const char *command);
};

class SidebarHighlight {
    public:
        SDL_Surface *surface;
        SDL_Texture *texture;
        SDL_Rect rect;
        int w;
        int h;

        void render_surface(int w, int h, int rx);
        void render_texture(SDL_Renderer *renderer);
        
};

class MenuHighlight {
    public:
        SDL_Surface *surface;
        SDL_Texture *texture;
        SDL_Rect rect;

        MenuHighlight();
        void render_surface(int x, int y, int w, int h, int t);
        void render_texture(SDL_Renderer *renderer);

};

class Layout {
    private:
        SDL_Renderer *renderer;
        int screen_width;
        int screen_height;
        float f_screen_width;
        float f_screen_height;

        // States
        std::vector<Shift> shift_queue;
        std::set<Menu*> visible_menus;
        SelectionMode selection_mode;
        Menu *current_menu;

        // Sidebar
        std::vector<SidebarEntry*> list;
        std::vector<SidebarEntry*>::iterator current_entry;
        Font sidebar_font;
        SidebarHighlight sidebar_highlight;
        int sidebar_pos;
        int sidebar_y_advance;
        int y_min;
        int y_max;
        int max_sidebar_entries;
        int sidebar_shift_count;
        int num_sidebar_entries;

        // Menu entry cards
        int card_w;
        int card_h;
        int card_x0;
        int card_y0;
        int card_y_advance;
        int max_rows;

        // Highlight
        MenuHighlight menu_highlight;
        int highlight_x0;
        int highlight_y0;
        int highlight_x_advance;
        int highlight_y_advance;

    public:
        Layout();
        void parse(const std::string &file);
        void add_entry();
        void load_surfaces(int screen_width, int screen_height);
        void load_textures(SDL_Renderer *renderer);
        void draw();
        void move_down();
        void move_up();
        void move_left();
        void move_right();
        void add_shift(ShiftType type, Direction direction, int target, float time, Menu *menu);
        void shift();
};