#pragma once


typedef struct font_cache_t {
    n_GFontInfo *font; // header
    uint16_t cached_glyph_count;
    uint32_t font_size;
    uint32_t cache_tick_stamp;  // cache time
    uint16_t resource_id;
    list_node node;
} font_cache_t;

typedef struct cache_glyph_info_t {
//     n_GFontInfo *font;
    uint8_t offset_entry_size;
    uint16_t font_info_size;
    uint8_t codepoint_size;
    uint8_t hash_table_size;
    uint8_t *hash_entry;
    uint8_t *offset_entry;
    uint8_t *glyph_entry;
} cache_glyph_info_t;

font_cache_t *font_load_system_font_by_resource_id(uint32_t resource_id);
font_cache_t *font_load_system_font(const char *font_key);

#define GFont font_cache_t *
