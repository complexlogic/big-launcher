#include <string>
#include <set>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <SDL.h>
#include <SDL_image.h>
#define NANOSVG_IMPLEMENTATION
#include "external/nanosvg.h"
#include <lconfig.h>
#include "layout.hpp"
#include "image.hpp"
#include "main.hpp"
#include "sound.hpp"
#include "util.hpp"

extern Config config;
extern Sound sound;

extern "C" void libxml2_error_handler(void *ctx, const char *msg, ...);


// Wrapper for libxml2 error messages
void libxml2_error_handler(void *ctx, const char *msg, ...)
{
    static char error[512];
    static int error_length = 0;
    if (!error_length)
        memset(error, '\0', sizeof(error));

    char message[512];
    va_list args;
    va_start(args, msg);
    vsnprintf(message, sizeof(message), msg, args);
    va_end(args);

    int message_length = strlen(message);
    size_t bytes = (error_length + message_length < sizeof(error)) ? message_length : sizeof(error) - error_length - 1;
    strncat(error, message, sizeof(error) - error_length - 1);
    error_length += bytes;

    if (error[error_length - 1] == '\n' || error_length == (sizeof(error) - 1)) {
        error[error_length - 1] = '\0';
        spdlog::error("{}", error);
        error_length = 0;
    }
}

Entry::Entry(const char *title, const char *command)
{
    this->title = title;
    this->command = command;
    surface = NULL;
    texture = NULL;
    icon_surface = NULL;
    background_color = {0xFF, 0xFF, 0xFF, 0xFF};
}

Entry::~Entry()
{

}

Entry *parse_entry(xmlChar *entry_title, xmlNodePtr node)
{
    Entry *entry = NULL;
    xmlNodePtr cmd_node = NULL;
    xmlNodePtr card_node = NULL;

    // Get location of command and card elements
    for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
        if (!xmlStrcmp(node->name, (const xmlChar*) "command") && cmd_node == NULL) {
            cmd_node = node;
        }
        else if (!xmlStrcmp(node->name, (const xmlChar*) "card") && card_node == NULL) {
            card_node = node;
        }
    }
    if (cmd_node == NULL || card_node == NULL) {
        return NULL;
    }

    // Parse command
    xmlChar *command = xmlNodeGetContent(cmd_node);
    if (command == NULL)
        return entry;
    entry = new Entry((const char*) entry_title, (const char*) command);
    xmlFree(command);

    // Parse card
    xmlChar *content = xmlNodeGetContent(card_node);
    unsigned long child_count = xmlChildElementCount(card_node);

    // Custom card
    if (content != NULL && !child_count) {
        entry->add_card((const char*) content);
    }

    // Generated card
    else if (child_count) {
        xmlNodePtr icon_node = NULL;
        xmlNodePtr background_node = NULL;

        // Get icon and background elements
        for (xmlNodePtr node = card_node->xmlChildrenNode; node != NULL; node = node->next) {
            if (!xmlStrcmp(node->name, (const xmlChar*) "icon")) {
                icon_node = node;
            }
            else if (!xmlStrcmp(node->name, (const xmlChar*) "background")) {
                background_node = node;
            }
        }

        if (icon_node == NULL || background_node == NULL || 
        xmlChildElementCount(icon_node) || xmlChildElementCount(background_node)) {
            delete entry;
            return NULL;
        }

        xmlChar *icon = xmlNodeGetContent(icon_node);
        xmlChar *background = xmlNodeGetContent(background_node);
        if (icon == NULL || background == NULL) {
            xmlFree(icon);
            xmlFree(background);
            delete entry;
            return NULL;
        }

        SDL_Color color;
        bool is_color = hex_to_color((char*) background, color);
        if (is_color) {
            entry->add_card(color, (const char*) icon);
        }
        else {
            entry->add_card((const char*) background, (const char*) icon);
        }
    }
    else {
        delete entry;
        entry = NULL;
    }
    return entry;
}

// Custom card
void Entry::add_card(const char *path)
{
    card_type = CUSTOM;
    this->path = path;
}

// Generated card, color background
void Entry::add_card(SDL_Color &background_color, const char *path)
{
    card_type = GENERATED;
    this->icon_path = path;
    this->background_color = background_color;
}

// Generated card, image background
void Entry::add_card(const char *background_path, const char *icon_path)
{
    card_type = GENERATED;
    this->path = background_path;
    this->icon_path = icon_path;
}

Menu *parse_menu(const char *menu_title,  xmlNodePtr node)
{
    xmlChar *entry_title;
    Entry *entry = NULL;
    Menu *menu = new Menu((const char*) menu_title);
    for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
        if (!xmlStrcmp(node->name, (const xmlChar*) "entry")) {
            entry_title = xmlGetProp(node, (const xmlChar*) "title");
            if (entry_title != NULL) {
                entry = parse_entry(entry_title, node);
                if (entry != NULL)
                    menu->add_entry(entry);
                xmlFree(entry_title);
            }
        }
    }
    int num_entries = menu->num_entries();
    if (!num_entries) {
        delete menu;
        menu = NULL;
    }
    else {
        menu->current_entry = menu->entry_list.begin();
        menu->max_columns = (num_entries < COLUMNS) ? num_entries : COLUMNS;
        menu->total_rows = num_entries / menu->max_columns;
        if (num_entries % menu->max_columns)
            menu->total_rows++;

    }

    return menu;
}

Menu::Menu(const char *title)
{
    this->title = title;
    type = MENU;
    y_offset = 0;
    row = 0;
    column = 0;
    max_columns = 0;
    shift_count = 0;
}

void Menu::add_entry(Entry *entry)
{
    if (entry != NULL) {
        entry_list.push_back(entry);
    }
}

size_t Menu::num_entries()
{
    return entry_list.size();
}


void Menu::render_surfaces(SDL_Surface *card_shadow, int shadow_offset, int w, int h, int x_start, int y_start, int spacing, int screen_height)
{
    int column = 0;
    int x = x_start;
    int y = y_start;
    int x_advance = w + spacing;
    int y_advance = h + spacing;
    SDL_Surface *bg;
    SDL_Surface *icon;

    for (Entry *entry : entry_list) {
        bg = NULL;

        if (entry->card_type == CUSTOM) {
            entry->surface = (entry->path.ends_with(".svg")) 
                                 ? rasterize_svg_from_file(entry->path, w, h)
                                 : load_surface(entry->path);
        }

        // Generated card
        else {
            icon = NULL;
            if (!entry->path.empty()) {
                bg = (entry->path.ends_with(".svg")) 
                        ? rasterize_svg_from_file(entry->path, w, h)
                        : load_surface(entry->path);
            }

            // Color background
            if (bg == NULL) {
                bg = SDL_CreateRGBSurfaceWithFormat(0, 
                          w + shadow_offset, 
                          h + shadow_offset, 
                          32,
                          SDL_PIXELFORMAT_ARGB8888
                      );
                Uint32 color = SDL_MapRGBA(bg->format, 
                                   entry->background_color.r, 
                                   entry->background_color.g, 
                                   entry->background_color.b, 
                                   entry->background_color.a
                               );
                SDL_FillRect(bg, NULL, color);
            }

            // Calculate aspect ratio, load surface if non-SVG
            float f_w, f_h, aspect_ratio;
            bool svg = entry->icon_path.ends_with(".svg");
            NSVGimage *image = NULL;
            if (svg) {
                image = nsvgParseFromFile((char*) entry->icon_path.c_str(), "px", 96.0f);
                f_w = image->width;
                f_h = image->height;
                aspect_ratio = image->width / image->height;
            }
            else {
                icon = load_surface(entry->icon_path);
                if (icon != NULL) {
                    f_w = (float) icon->w;
                    f_h = (float) icon->h;
                    aspect_ratio = (float) icon->w / (float) icon->h;
                }
            }

            if (image != NULL || icon != NULL) {
                float target_w, target_h;

                // Calculate icon dimensions
                if (aspect_ratio >  CARD_ASPECT_RATIO) {
                    target_w = (float) w  * (1.0f - 2.0f * CARD_ICON_MARGIN);
                    target_h = ((target_w / f_w)) * f_h;
                    entry->icon_rect =  {
                        (int) std::round(CARD_ICON_MARGIN * (float) w) + shadow_offset,
                        (h - (int) target_h) / 2 + shadow_offset,
                        (int) std::round(target_w),
                        (int) std::round(target_h)
                    };
                }
                else {
                    target_h = (float) h  * (1.0f - 2.0f * CARD_ICON_MARGIN);
                    target_w = (target_h / f_h) * f_w;
                    entry->icon_rect = {
                        (w - (int) target_w) / 2 + shadow_offset,
                        (int) std::round(CARD_ICON_MARGIN * (float) h) + shadow_offset,
                        (int) std::round(target_w),
                        (int) std::round(target_h)
                    };
                }
                if (svg) {
                    icon = rasterize_svg_image(image, 
                               entry->icon_rect.w, 
                               entry->icon_rect.h
                           );
                }
                entry->icon_surface = icon;
            }
            entry->surface = bg;
        }

        // Assign dimensions
        entry->rect = {
            x,
            y,
            w + 2*shadow_offset,
            h + 2*shadow_offset
        };
        x += x_advance;
        column++;
        if (column == COLUMNS) {
            x = x_start;
            y += y_advance;
            column = 0;
        }
    }

    height = (y > screen_height) ? y : screen_height;
}

void Menu::render_card_textures(SDL_Renderer *renderer, SDL_Texture *card_shadow_texture, int shadow_offset, int card_w, int card_h)
{
    SDL_Texture *texture = NULL;
    SDL_Rect rect = {shadow_offset, shadow_offset, card_w, card_h};
    
    for (Entry *entry : entry_list) {
        entry->texture = SDL_CreateTexture(renderer,
                             SDL_PIXELFORMAT_ARGB8888,
                             SDL_TEXTUREACCESS_TARGET,
                             entry->rect.w,
                             entry->rect.h
                         );
        SDL_SetTextureBlendMode(entry->texture, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(renderer, entry->texture);
        
        // Copy the shadow
        SDL_RenderCopy(renderer, card_shadow_texture, NULL, NULL);

        // Copy the background
        texture = SDL_CreateTextureFromSurface(renderer, entry->surface);
        SDL_RenderCopy(renderer, texture, NULL, &rect);
        free_surface(entry->surface);
        entry->surface = NULL;
        SDL_DestroyTexture(texture);
        
        // Copy the icon
        if (entry->card_type == GENERATED) {
            texture = SDL_CreateTextureFromSurface(renderer, entry->icon_surface);
            SDL_RenderCopy(renderer, texture, NULL, &entry->icon_rect);
            free_surface(entry->icon_surface);
            entry->icon_surface = NULL;
            SDL_DestroyTexture(texture);           
        }
    }
}

void Menu::draw_entries(SDL_Renderer *renderer, int y_min, int y_max)
{
    SDL_Rect src_rect;
    SDL_Rect dst_rect;
    int y, h;
    for (const Entry *entry : entry_list) {
        y = entry->rect.y + y_offset;
        h = entry->rect.h;
        if (y < y_min) {
            if ((y + h) > y_min) {
                src_rect = {
                    0, // x
                    y_min - y, // y
                    entry->rect.w, // w
                    entry->rect.h - (y_min - y) // h
                };
                dst_rect = {
                    entry->rect.x, // x
                    y_min, // y
                    entry->rect.w, // w
                    src_rect.h // h
                };
                SDL_RenderCopy(renderer, entry->texture, &src_rect, &dst_rect);
            }

        }
        else if ((y + h) > y_max) {
            if (y < y_max) {
                src_rect = {
                    0, // x
                    0, // y
                    entry->rect.w, // w
                    entry->rect.h - ((y + h) - y_max) // h
                };
                dst_rect = {
                    entry->rect.x, // x
                    y, // y
                    entry->rect.w, // w
                    src_rect.h // h
                };
                SDL_RenderCopy(renderer, entry->texture, &src_rect, &dst_rect);
            }
        }
        else {
            if (y_offset) {
                dst_rect = {
                    entry->rect.x, // x
                    entry->rect.y + y_offset, // y
                    entry->rect.w, // w
                    entry->rect.h // h
                };
                SDL_RenderCopy(renderer, entry->texture, NULL, &dst_rect);
            }
            else {
                SDL_RenderCopy(renderer, entry->texture, NULL, &entry->rect);
            }
        }
    }
}

void Menu::print_entries()
{
    for (Entry *entry : entry_list) {
        fmt::print("Entry {}:\n", entry - entry_list[0]);
        fmt::print("Title: {}\n", (char*) entry->title.c_str());
        fmt::print("Command: {}\n", (char*) entry->command.c_str());
    }
}

Command::Command(const char *title, const char *command)
{
    this->title = title;
    this->command = command;
    type = COMMAND;
}

Layout::Layout()
{
    sidebar_pos = 0;
    max_sidebar_entries = -1;
    renderer = NULL;
    current_menu = NULL;
    sidebar_shift_count = 0;
    num_sidebar_entries = 0;
    selection_mode = SELECTION_SIDEBAR;
    background_surface = NULL;
    background_texture = NULL;
    card_shadow = NULL;
}

void Layout::parse(const std::string &file)
{
    spdlog::debug("Parsing layout file '{}'", file);
    xmlDocPtr doc;
    xmlNodePtr node;
    Menu *menu = NULL;
    Command *command = NULL;

    xmlSetGenericErrorFunc(NULL, libxml2_error_handler);
    doc = xmlParseFile(file.c_str());
    if (doc == NULL) {
        spdlog::critical("Failed to parse layout file");
        quit(EXIT_FAILURE);
    }
    node = xmlDocGetRootElement(doc);
    if (node == NULL) {
        spdlog::critical("Could not get root element of layout file");
        xmlFreeDoc(doc);
        quit(EXIT_FAILURE);
    }

    if (xmlStrcmp(node->name, (const xmlChar*) "layout")) {
        spdlog::critical("Root element of layout file is not 'layout'");
        xmlFreeDoc(doc);
        quit(EXIT_FAILURE);
    }

    for (node = node->xmlChildrenNode; node != NULL; node = node->next) {

        // Menu detected
        if (!xmlStrcmp(node->name, (const xmlChar*) "menu")) {
            xmlChar *title = xmlGetProp(node, (const xmlChar*) "title");
            if (title != NULL) {
                menu = parse_menu((const char*) title, node);
                xmlFree(title);
                if (menu != NULL) {
                    list.push_back(menu);
                }
            }
        }

        // Command detected
        else if (!xmlStrcmp(node->name, (const xmlChar*) "command")) {
            xmlChar *title = xmlGetProp(node, (const xmlChar*) "title");
            if (title != NULL) {
                xmlChar *cmd = xmlNodeGetContent(node);
                if (cmd != NULL && !xmlChildElementCount(node)) {
                    command = new Command((const char*) title, (const char*) cmd);
                    list.push_back(command);
                    xmlFree(cmd);
                }
                xmlFree(title);
            }
        }
    }
    xmlFreeDoc(doc);
    
    /*
    for (const SidebarEntry *entry : list) {
        if (entry->type == MENU) {
            menu = (Menu*) entry;
            fmt::print("Menu '{}' has {} entries:\n", (char*) menu->title.c_str(), menu->num_entries());
            menu->print_entries();
            fmt::print("\n");
        }
        else if (entry->type == COMMAND) {
            command = (Command*) entry;
            fmt::print("Sidebar Command:\n");
            fmt::print("Title: {}\n", (char*) command->title.c_str());
            fmt::print("Command: {}\n", (char*) command->command.c_str());
            fmt::print("\n");
        }
    }
    */
    

    num_sidebar_entries = list.size();
    current_entry = list.begin();
    if ((*current_entry)->type == MENU) {
        current_menu = (Menu*) *current_entry;
        visible_menus.insert(current_menu);
    }
    spdlog::debug("Successfully parsed layout file");
}

#define SHADOW_ALPHA 0.45f
#define SHADOW_BLUR_SLOPE 0.0101212f
#define SHADOW_BLUR_INTERCEPT 8.93f
#define SHADOW_OFFSET_SLOPE 0.008f
#define SHADOW_OFFSET_INTERCEPT 3.15f


void SidebarHighlight::render_surface(int w, int h, int rx)
{
    this->w = w;
    this->h = h;

    std::string highlight_buffer = format_highlight(w, h, rx, config.sidebar_highlight_color);
    SDL_Surface *highlight = rasterize_svg(highlight_buffer, -1, -1);

    constexpr Uint8 alpha = (Uint8) std::round((float) 0xFF * SHADOW_ALPHA);
    float f_height = (float) highlight->h;

    float max_blur = SHADOW_BLUR_SLOPE*f_height + SHADOW_BLUR_INTERCEPT;
    int max_y_offset = SHADOW_OFFSET_SLOPE*f_height + SHADOW_OFFSET_INTERCEPT;
    std::vector<BoxShadow> box_shadows = {
        {0, max_y_offset / 2, max_blur / 2.0f, alpha},
        {0, max_y_offset,     max_blur,        alpha}
    };


    shadow_offset = (int) std::round(max_blur * 2.0f);
    surface = create_shadow(highlight, box_shadows, shadow_offset);
    SDL_Rect tmp = {shadow_offset, shadow_offset, highlight->w, highlight->h};
    SDL_BlitSurface(highlight, NULL, surface, &tmp);
    free_surface(highlight);

    rect.w = surface->w;
    rect.h = surface->h;
}


void SidebarHighlight::render_texture(SDL_Renderer *renderer)
{
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    free_surface(surface);
    surface = NULL;
}


MenuHighlight::MenuHighlight()
{
    surface = NULL;
    texture = NULL;
}


void MenuHighlight::render_surface(int x, int y, int w, int h, int t, int shadow_offset)
{
    int w_inner = w - 2*t;
    int h_inner = h - 2*t;
    int rx_outter = (int) std::round((float) w * MENU_HIGHLIGHT_RX);
    int rx_inner = rx_outter / 2;
    

    // Render highlight
    std::string format = format_menu_highlight(w, h, w_inner, h_inner, t, rx_outter, rx_inner);
    SDL_Surface *highlight = rasterize_svg(format, -1, -1);

    // Render shadow
    #define SHADOW_ALPHA_HIGHLIGHT 0.6f
    constexpr Uint8 alpha = (Uint8) std::round((float) 0xFF * SHADOW_ALPHA_HIGHLIGHT);
    float f_h = (float) h;
    float max_blur = SHADOW_BLUR_SLOPE*f_h+ SHADOW_BLUR_INTERCEPT;
    int max_y_offset = SHADOW_OFFSET_SLOPE*f_h + SHADOW_OFFSET_INTERCEPT;
    std::vector<BoxShadow> box_shadows = {
        {0, max_y_offset / 2, max_blur / 2.0f, alpha},
        {0, max_y_offset,     max_blur,        alpha}
    };


    surface = create_shadow(highlight, box_shadows, shadow_offset);
    SDL_Rect blit_rect = {shadow_offset, shadow_offset, highlight->w, highlight->h};
    SDL_BlitSurface(highlight, NULL, surface, &blit_rect);
    free_surface(highlight);
    Uint32 key = SDL_MapRGBA(surface->format, 0x00, 0x00, 0x01, 0xFF);
    SDL_SetColorKey(surface, SDL_TRUE, key);
    rect = {x, y, surface->w, surface->h};
}


void MenuHighlight::render_texture(SDL_Renderer *renderer)
{
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    free_surface(surface);
}


void Layout::load_surfaces(int screen_width, int screen_height)
{
    this->screen_width = screen_width;
    this->screen_height = screen_height;
    f_screen_width = (float) screen_width;
    f_screen_height = (float) screen_height;
    y_min = (int) std::round(f_screen_height * TOP_MARGIN);
    y_max = (int) std::round(f_screen_height * BOTTOM_MARGIN);

    // Background
    if (!config.background_image_path.empty()) {
        background_surface = (config.background_image_path.ends_with(".svg")) 
                             ? rasterize_svg_from_file(config.background_image_path, screen_width/4, screen_height/4) 
                             : load_surface(config.background_image_path);
    }


    // Sidebar highlight geometry calculations and rendering
    float f_sidebar_width = std::round(f_screen_width * SIDEBAR_HIGHLIGHT_WIDTH);
    int sidebar_width = (int) f_sidebar_width;
    float f_sidebar_height = std::round(f_screen_height * SIDEBAR_HIGHLIGHT_HEIGHT);
    int sidebar_height = (int) f_sidebar_height;
    int sidebar_cx = (int) std::round(f_sidebar_width * SIDEBAR_CORNER_RADIUS);
    int sidebar_font_size = (int) std::round(f_sidebar_height * SIDEBAR_FONT_SIZE);
    sidebar_y_advance = (int) std::round(f_screen_height * SIDEBAR_Y_ADVANCE);
    
    sidebar_highlight.render_surface(sidebar_width, sidebar_height, sidebar_cx);
    sidebar_highlight.rect.x = (int) std::round(f_screen_width * SIDEBAR_HIGHLIGHT_LEFT) - sidebar_highlight.shadow_offset;
    sidebar_highlight.rect.y = y_min - sidebar_highlight.shadow_offset;

    // Find and load sidebar font
    sidebar_font.load(SIDEBAR_FONT, sidebar_font_size);

    // Sidebar entry text geometry calculations and rendering
    int sidebar_text_margin = (int) std::round(f_sidebar_width * SIDEBAR_TEXT_MARGIN);
    int sidebar_text_x = sidebar_highlight.rect.x + sidebar_highlight.shadow_offset + sidebar_text_margin;
    int max_sidebar_text_width = sidebar_width - 2 * sidebar_text_margin;
    int y = sidebar_highlight.rect.y + sidebar_highlight.h / 2 + sidebar_highlight.shadow_offset;
    for (int i = 0; SidebarEntry *entry : list) {
        entry->surface = sidebar_font.render_text(entry->title.c_str(), 
                             &entry->src_rect, 
                             &entry->dst_rect,
                             max_sidebar_text_width
                         );
        entry->dst_rect.x = sidebar_text_x;
        entry->dst_rect.y = y - entry->dst_rect.h / 2;
        if (max_sidebar_entries == -1 && 
        entry->dst_rect.y + entry->dst_rect.h > y_max) {
            max_sidebar_entries = i - 1;
        }
        y += sidebar_y_advance;
        i++;
    }

    // Menu card geometry calculations
    card_x0 = (int) std::round(f_screen_width * CARD_LEFT_MARGIN);
    card_y0 = y_min;
    int card_spacing = std::round(f_screen_width * CARD_SPACING);
    card_w = (((int) std::round(f_screen_width * (CARD_WIDTH))) - (COLUMNS - 1)*card_spacing) / COLUMNS;
    card_h = (int) std::round((float) card_w / CARD_ASPECT_RATIO);
    float f_card_h = (float) card_h;

    // Render card shadow
    constexpr Uint8 alpha = (Uint8) std::round((float) 0xFF * SHADOW_ALPHA);
    float max_blur = SHADOW_BLUR_SLOPE*f_card_h + SHADOW_BLUR_INTERCEPT;
    int max_y_offset = SHADOW_OFFSET_SLOPE*f_card_h + SHADOW_OFFSET_INTERCEPT;
    std::vector<BoxShadow> box_shadows = {
        {0, max_y_offset / 2, max_blur / 2.0f, alpha},
        {0, max_y_offset,     max_blur,        alpha}
    };
    card_shadow_offset = (int) std::round(max_blur * 2.0f);

    SDL_Surface *shadow_box = SDL_CreateRGBSurfaceWithFormat(0, 
                                  card_w, 
                                  card_h, 
                                  32,
                                  SDL_PIXELFORMAT_ARGB8888
                              );
    Uint32 white = SDL_MapRGBA(shadow_box->format, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_FillRect(shadow_box, NULL, white);
    card_shadow = create_shadow(shadow_box, box_shadows, card_shadow_offset);
    free_surface(shadow_box);

    // Menu card rendering
    card_y_advance = card_h + card_spacing;
    max_rows = (y_max - y_min) / card_y_advance;
    Menu *menu = NULL;
    for (const SidebarEntry *entry : list) {
        if (entry->type == MENU) {
            menu = (Menu*) entry;
            menu->render_surfaces(card_shadow, 
                card_shadow_offset, 
                card_w, 
                card_h, 
                card_x0 - card_shadow_offset, 
                card_y0 - card_shadow_offset, 
                card_spacing, 
                screen_height
            );
        }
    }

    // Menu highlight geometry calculations and rendering
    float f_card_spacing = (float) card_spacing;
    int t = (int) std::round(f_card_spacing * HIGHLIGHT_THICKNESS);
    int inner_spacing = (int) std::round(f_card_spacing * HIGHLIGHT_INNER_SPACING);
    int mh_shadow_offset = (int) std::round(max_blur * 2.0f);
    highlight_x0 = card_x0 - (inner_spacing + t) - mh_shadow_offset;
    highlight_y0 = card_y0 - (inner_spacing + t) - mh_shadow_offset;
    int padding = 2*(inner_spacing + t);
    int highlight_w = card_w + padding;
    int highlight_h = card_h + padding; 

    menu_highlight.render_surface(highlight_x0, 
        highlight_y0, 
        highlight_w, 
        highlight_h, 
        t,
        mh_shadow_offset
    );
    highlight_x_advance = card_w + card_spacing;
    highlight_y_advance = card_h + card_spacing;

    spdlog::debug("Successfully rendered surfaces");
}


void Layout::load_textures(SDL_Renderer *renderer)
{
    this->renderer = renderer;

    // Background texture
    if (background_surface != NULL) {
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, background_surface);
        if (background_surface->w != screen_width || background_surface->h != screen_height) {
            background_texture = SDL_CreateTexture(renderer,
                                     SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_TARGET,
                                     screen_width,
                                     screen_height
                                 );
            SDL_SetRenderTarget(renderer, background_texture);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_DestroyTexture(texture);
        }
        else {
            background_texture = texture;
        }
        free_surface(background_surface);
    }
    sidebar_highlight.render_texture(renderer);

    card_shadow_texture = SDL_CreateTextureFromSurface(renderer, card_shadow);
    free_surface(card_shadow);
    card_shadow = NULL;
    SDL_SetTextureBlendMode(card_shadow_texture, SDL_BLENDMODE_NONE);

    Menu *menu;
    for (SidebarEntry *entry : list) {
        entry->texture = SDL_CreateTextureFromSurface(renderer, entry->surface);
        (entry == *current_entry) ? set_texture_color(entry->texture, config.sidebar_text_color_highlighted) : set_texture_color(entry->texture, config.sidebar_text_color);
        free_surface(entry->surface);
        if (entry->type == MENU) {
            menu = (Menu*) entry;
            menu->render_card_textures(renderer, card_shadow_texture, card_shadow_offset, card_w, card_h);
        }
    }
    SDL_DestroyTexture(card_shadow_texture);
    card_shadow_texture = NULL;

    menu_highlight.render_texture(renderer);
    SDL_SetRenderTarget(renderer, NULL);
    spdlog::debug("Sucessfully rendered textures");
}


void Layout::move_up()
{
    if (selection_mode == SELECTION_SIDEBAR) {
        if (sidebar_pos) {
            if (sidebar_shift_count && sidebar_pos == sidebar_shift_count){
                this->add_shift(SHIFT_SIDEBAR, DIRECTION_DOWN, sidebar_y_advance, SIDEBAR_SHIFT_TIME, NULL);
                sidebar_shift_count--;
            }

            // Shift menus if necessary
            if (current_menu != NULL) {
                this->add_shift(SHIFT_MENU, DIRECTION_DOWN, screen_height, SIDEBAR_SHIFT_TIME, current_menu);
            }
            if ((*(current_entry - 1))->type == MENU) {
                current_menu = (Menu*) *(current_entry - 1);
                if (!current_menu->y_offset)
                    current_menu->y_offset = -1 * current_menu->height;
                visible_menus.insert(current_menu);
                this->add_shift(SHIFT_MENU, DIRECTION_DOWN, screen_height, SIDEBAR_SHIFT_TIME, current_menu);
            }
            else {
                current_menu = NULL;
            }

            // Adjust texture color
            set_texture_color((*current_entry)->texture, config.sidebar_text_color);
            set_texture_color((*(current_entry  -1))->texture, config.sidebar_text_color_highlighted); 
            sidebar_highlight.rect.y -= sidebar_y_advance;
            current_entry--;
            sidebar_pos--;
            if (sound.connected)
                sound.play_click();
        }
    }

    else if (selection_mode == SELECTION_MENU && current_menu->row) {
        if (current_menu->shift_count && current_menu->row == current_menu->shift_count) {
            
            // Shift rows down
            this->add_shift(SHIFT_MENU, DIRECTION_DOWN, card_y_advance, ROW_SHIFT_TIME, current_menu);
            current_menu->shift_count--;
        }
        
        else {
            this->add_shift(SHIFT_HIGHLIGHT, DIRECTION_UP, highlight_y_advance, HIGHLIGHT_SHIFT_TIME, NULL);
        }
        current_menu->current_entry -= 3;
        current_menu->row--;
        if (sound.connected)
            sound.play_click();
    }
}


void Layout::move_down()
{
    if (selection_mode == SELECTION_SIDEBAR) {
        if (sidebar_pos < (num_sidebar_entries - 1)) {

            // Shift menus if necessary
            if (current_menu != NULL) {
                this->add_shift(SHIFT_MENU, DIRECTION_UP, screen_height, SIDEBAR_SHIFT_TIME, current_menu);
            }
            if ((*(current_entry + 1))->type == MENU) {
                current_menu = (Menu*) *(current_entry + 1);
                if (!current_menu->y_offset)
                    current_menu->y_offset = current_menu->height;
                visible_menus.insert(current_menu);
                this->add_shift(SHIFT_MENU, DIRECTION_UP, screen_height, SIDEBAR_SHIFT_TIME, current_menu);
            }
            else {
                current_menu = NULL;
            }

            // Adjust text color
            set_texture_color((*current_entry)->texture, config.sidebar_text_color);
            set_texture_color((*(current_entry + 1))->texture, config.sidebar_text_color_highlighted); 
            sidebar_highlight.rect.y += sidebar_y_advance;
            current_entry++;
            sidebar_pos++;
            if (sound.connected)
                sound.play_click();
        }

        // Shift sidebar
        if (max_sidebar_entries != -1 && sidebar_pos < (num_sidebar_entries - max_sidebar_entries)) {
            this->add_shift(SHIFT_SIDEBAR, DIRECTION_UP, sidebar_y_advance, SIDEBAR_SHIFT_TIME, NULL);
            sidebar_shift_count++;
        }
    }

    // We are in menu mode
    else if (selection_mode == SELECTION_MENU) {
        if (current_menu->row * COLUMNS + current_menu->column + COLUMNS < current_menu->num_entries()) {

            if (current_menu->total_rows > max_rows && 
            current_menu->row + current_menu->shift_count < (current_menu->total_rows - 2)) {
                this->add_shift(SHIFT_MENU, DIRECTION_UP, card_y_advance, ROW_SHIFT_TIME, current_menu);
                current_menu->shift_count++;
            }
            else {
                this->add_shift(SHIFT_HIGHLIGHT, DIRECTION_DOWN, highlight_y_advance, HIGHLIGHT_SHIFT_TIME, NULL);
            }

            current_menu->current_entry += COLUMNS;
            current_menu->row++;
            if (sound.connected)
                sound.play_click();
        }
    }
}


void Layout::move_left()
{
    if (selection_mode == SELECTION_MENU) {
        if (current_menu->column == 0) {
            selection_mode = SELECTION_SIDEBAR;
            set_texture_color((*current_entry)->texture, config.sidebar_text_color_highlighted);
            current_menu->row = 0;
            menu_highlight.rect.y = highlight_y0;
            current_menu->current_entry = current_menu->entry_list.begin();
            
            // Reset menu shift
            if (current_menu->shift_count) {
                int shift_amount = current_menu->shift_count*card_y_advance;
                this->add_shift(SHIFT_MENU, DIRECTION_DOWN, shift_amount, HIGHLIGHT_SHIFT_TIME, current_menu);
                current_menu->shift_count = 0;
            }
        }
        else {
            // Move highlight left
            this->add_shift(SHIFT_HIGHLIGHT, DIRECTION_LEFT, highlight_x_advance, HIGHLIGHT_SHIFT_TIME, NULL);

            current_menu->column--;
            current_menu->current_entry--;
        }
        if (sound.connected)
            sound.play_click();
    }
}


void Layout::move_right()
{
    if (selection_mode == SELECTION_SIDEBAR && current_menu != NULL) {
        selection_mode = SELECTION_MENU;
        set_texture_color((*current_entry)->texture, config.sidebar_text_color);
        if (sound.connected)
            sound.play_click();
    }

    else if (selection_mode == SELECTION_MENU) {
        int max_columns = current_menu->num_entries() - current_menu->row * current_menu->max_columns;
        if (max_columns > current_menu->max_columns)
            max_columns = current_menu->max_columns;
        if(current_menu->column < max_columns - 1) {
            
            // Shift highlight right
            this->add_shift(SHIFT_HIGHLIGHT, DIRECTION_RIGHT, highlight_x_advance, HIGHLIGHT_SHIFT_TIME, NULL);

            current_menu->column++;
            current_menu->current_entry++;
            if (sound.connected)
                sound.play_click();
        }
    }
}

void Layout::add_shift(ShiftType type, Direction direction, int target, float time, Menu *menu)
{
    static const Direction opposites[] = {
        DIRECTION_DOWN,
        DIRECTION_UP,
        DIRECTION_RIGHT,
        DIRECTION_LEFT
    };
    Direction opposite = opposites[direction];
    float velocity;

    // Interrupt if opposite direction shift is in progress
    for(auto shift = shift_queue.begin(); shift != shift_queue.end(); ++shift) {
        if (shift->direction == opposite && shift->type == type && 
        (shift->type != SHIFT_MENU || shift->menu == menu)) {
            int new_target = target - (shift->target - shift->total);
            velocity = shift->velocity;
            shift_queue[shift - shift_queue.begin()] = {
                type,
                menu,
                direction,
                SDL_GetTicks(),
                velocity,
                0,
                new_target
            };
            return;
        }
    }

    // Add new shift to queue
    velocity = (float) target / time;
    shift_queue.push_back(
        {
            type,
            menu,
            direction,
            SDL_GetTicks(),
            velocity,
            0,
            target
        }
    );
}


void Layout::shift()
{
    Uint32 ticks = SDL_GetTicks();
    for (auto shift = shift_queue.begin(); shift != shift_queue.end();) {
        
        // Calculate position change based on velocity and time elapsed
        int current = (int) ((float) (ticks - shift->ticks) * shift->velocity);
        if (shift->total + current > shift->target) {
            current = shift->target - shift->total;
        }
        shift->total += current;
        if (shift->direction == DIRECTION_UP || shift->direction == DIRECTION_LEFT)
            current *= -1;

        // Apply shift
        if (shift->type == SHIFT_SIDEBAR) {
            sidebar_highlight.rect.y += current;
            for (SidebarEntry *entry : list) {
                entry->dst_rect.y += current;
            }
        }
        else if (shift->type == SHIFT_MENU) {
            shift->menu->y_offset += current;
            if (shift->target == shift->total && shift->menu != current_menu) {
                shift->menu->y_offset = 0;
                visible_menus.erase(shift->menu);
            }
        }
        else if (shift->type == SHIFT_HIGHLIGHT) {
            if (shift->direction == DIRECTION_LEFT || shift->direction == DIRECTION_RIGHT) {
                menu_highlight.rect.x += current;
            }
            else {
                menu_highlight.rect.y += current;
            }
        }

        shift->ticks = ticks;
        (shift->target == shift->total) ? shift_queue.erase(shift) : ++shift;
    }
}

void Layout::draw()
{
    int y, h;
    SDL_Rect src_rect;
    SDL_Rect dst_rect;

    SDL_RenderClear(renderer);

    // Draw background
    if (background_texture != NULL) {
        SDL_RenderCopy(renderer, background_texture, NULL, NULL);
    }


    // Draw sidebar highlight
    if (selection_mode == SELECTION_SIDEBAR) {
        y = sidebar_highlight.rect.y;
        h = sidebar_highlight.rect.h;
        int sidebar_y_min = y_min - sidebar_highlight.shadow_offset;

        // Intersecting top bound
        if (y < sidebar_y_min) {
            if (y + h > sidebar_y_min) {
                src_rect = {
                    0, // x
                    sidebar_y_min - y, // y
                    sidebar_highlight.rect.w, // w
                    sidebar_highlight.rect.h - (sidebar_y_min - y) // h
                };
                dst_rect = {
                    sidebar_highlight.rect.x, // x
                    sidebar_y_min, // y
                    src_rect.w, // w
                    src_rect.h // h
                };
                SDL_RenderCopy(renderer, sidebar_highlight.texture, &src_rect, &dst_rect);
            }
        }

        // Default, fully visible case
        else {
            SDL_RenderCopy(renderer, sidebar_highlight.texture, NULL, &sidebar_highlight.rect);
        }
    }
    
    // Draw sidebar texts
    for (const SidebarEntry *entry : list) {
        y = entry->dst_rect.y;
        h = entry->dst_rect.h;

        // Intersecting top bound
        if (y < y_min) {
            if (y + h > y_min) {
                src_rect = {
                    entry->src_rect.x, // x
                    entry->src_rect.y + (y_min - y), // y
                    entry->src_rect.w, // w
                    entry->src_rect.h - (y_min - h) // h
                };
                dst_rect = {
                    entry->dst_rect.x, // x
                    y_min, // y
                    entry->dst_rect.w, // w
                    src_rect.h // h
                };
                SDL_RenderCopy(renderer, entry->texture, &src_rect, &dst_rect);
            }
        }

        // Intersecting bottom bound
        else if ((y + h) > y_max) {
            if (y < y_max) {
                src_rect = {
                    entry->src_rect.x, //x
                    entry->src_rect.y, // y
                    entry->src_rect.w, // w
                    entry->src_rect.h - ((y + h) - y_max) // h
                };
                dst_rect = {
                    entry->dst_rect.x, // x
                    entry->dst_rect.y, // y
                    entry->dst_rect.w, // w
                    src_rect.h // h
                };
                SDL_RenderCopy(renderer, entry->texture, &src_rect, &dst_rect);
            }
        }

        // Default, fully visible case
        else {
            SDL_RenderCopy(renderer, entry->texture, &entry->src_rect, &entry->dst_rect);
        }
    }

    // Draw menu entries
    for (Menu *menu : visible_menus) {
        menu->draw_entries(renderer, y_min - card_shadow_offset, y_max);
    }

    // Draw menu highlight
    if (selection_mode == SELECTION_MENU) {
        SDL_RenderCopy(renderer, menu_highlight.texture, NULL, &menu_highlight.rect);
    }

    // Output to screen
    SDL_RenderPresent(renderer);
}