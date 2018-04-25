#pragma once
/* font_cache.c
 * 
 * See .c file for full description
 * 
 * Author: Barry Carter <barry.carter@gmail.com>
 */

typedef struct font_cache_t {
    n_GFontInfo *font; // header
    uint16_t cached_glyph_count;
    uint32_t font_size;
    uint16_t resource_id;
    list_node node;
} font_cache_t;

#define GFont font_cache_t *

/**
 * @brief remove all fonts from the current threads cache 
 */
void font_cache_remove_all(void);

font_cache_t *font_load_system_font_by_resource_id(uint32_t resource_id);
font_cache_t *font_load_system_font(const char *font_key);
void font_draw_text(n_GContext * ctx, const char * text, GFont cached_font, const n_GRect box,
    const n_GTextOverflowMode overflow_mode, const n_GTextAlignment alignment,
    n_GTextAttributes * text_attributes);

