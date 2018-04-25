/* font_cache.c
 * 
 * A glyph cache for fonts to allow only saving glyphs required 
 * without loading the font over and over, using lots of memory.
 * 
 * Author: Barry Carter <barry.carter@gmail.com>
 */
/*
 * This is a glyph cache. It saves each used character in a font 
 * inside a cache. This allows us to load the font once, and use 
 * it over and over without wasting massive amounts of memory.
 * This allows us to load several fonts at the same time.
 * 
 * How does it work?
 * 
 * The below spaghetti is a carefully hacked font builder.
 * We start with an empty font. it has a header, hash, and offset table
 * No glyphs. This keeps it small. (we could go smaller with a custom font 
 * with say, 32 hash entries)
 * When a font is rendered to the screen for the given text (codepoints)
 * each character (glyph) needed is
 *   timestamp added after the last font (or offset table)
 *   font glyph copied in
 *   offset table entry added that points to this offset (not the timestamp)
 *   
 * If there are more characters than the font cache size, we load and return the 
 * font unadulterated.
 *
 * We are constructing a font so we don't need any changed to ngfx. 
 * The generated font is in near native format, and is very fast 
 * to render once cached.
 * The speed is ok on cache checks (more can be done) and returning pre 
 * cached fonts for render doesn't change the speed of draw from 
 * previous behaviour.
 * 
 * Cache expiry takes the oldest n items to free, and removes them from 
 * the cache. Then are then re-added to a new font, along with the new glyphs 
 * 
 */

#include "librebble.h"
#include "node_list.h"
#include "font_cache.h"
#include "../lib/musl/stdlib/musl_stdlib.h"

#define CACHE_COUNT 22

/* comment me out to disable font debug */
//#define DEBUG_FONT

#ifdef DEBUG_FONT
    #define FONT_LOG SYS_LOG
#else
    #define FONT_LOG 
#endif

typedef struct cache_glyph_info_t {
    uint8_t offset_entry_size;
    uint16_t font_info_size;
    uint8_t codepoint_size;
    uint8_t hash_table_size;
    uint8_t *hash_entry;
    uint8_t *offset_entry;
    uint8_t *glyph_entry;
} cache_glyph_info_t;

static list_head _font_cache_list_head_app = LIST_HEAD(_font_cache_list_head_app);
static list_head _font_cache_list_head_ovl = LIST_HEAD(_font_cache_list_head_ovl);

static uint8_t _offset_table_entry_size(n_GFontInfo *font);
static bool _is_codepoint_in_cache(font_cache_t *font, uint32_t codepoint);
static bool _offset_entry_value_equal(uint8_t *offset_entry, uint8_t codepoint_bytes, uint32_t codepoint);
static list_head *_head_for_thread(void);
static void _expire_cache_items(font_cache_t *cached_font, uint32_t exclude_codepoints[], uint16_t exclude_count, uint16_t remove_count);
static uint32_t _create_empty_font(n_GFontInfo *font, n_GFontInfo **font_to_create);
static font_cache_t *_get_cache_entry_resource(uint16_t resource_id);
static font_cache_t *_add_font_cache_entry(n_GFontInfo *font, uint16_t resource_id, uint32_t font_size);
static inline void _offset_entry_set_offset(uint8_t *offset_entry, uint8_t codepoint_bytes, uint32_t offset);
static font_cache_t *_remove_font_cache_entry(uint16_t resource_id);

font_cache_t *font_load_system_font(const char *font_key)
{
    /* XXX consider storing the font key to save this resource header load */
    uint16_t res_id = _fonts_get_resource_id_for_key(font_key);
    font_cache_t *fc = _get_cache_entry_resource(res_id);
    
    /* see if we are already caching this */
    if (fc)
        return fc;
    
    return font_load_system_font_by_resource_id(res_id);
}

font_cache_t *font_load_system_font_by_resource_id(uint32_t resource_id)
{
    font_cache_t *fc = _get_cache_entry_resource(resource_id);
    
    /* see if we are already caching this */
    if (fc) {
        FONT_LOG("Font", APP_LOG_LEVEL_INFO, "Already cached res:%d", resource_id);
        return fc;
    }
    
    /* we could consider lazy loading the font as required. 
     * For now, just immediately load */
    uint8_t *buffer = resource_fully_load_id_system(resource_id);
    n_GFontInfo *font = (n_GFontInfo *)buffer;
    n_GFontInfo *new_font;
    uint32_t font_size = _create_empty_font(font, &new_font);

    font_cache_t *c = _add_font_cache_entry(new_font, resource_id, font_size);

    app_free(buffer);
    
    FONT_LOG("Font", APP_LOG_LEVEL_INFO, "Loaded font res:%d", resource_id);
    return c;
}

void font_draw_text(n_GContext * ctx, const char * text, GFont cached_font, const n_GRect box,
    const n_GTextOverflowMode overflow_mode, const n_GTextAlignment alignment,
    n_GTextAttributes * text_attributes)
{
    /* are we even caching this resource? */
    uint16_t i = 0, cp_count = 0, total_count = 0;
    
    uint32_t cp_to_load[CACHE_COUNT];
    uint32_t cp_to_exclude[CACHE_COUNT];
    uint32_t next_codepoint = 0, next_i = 0;
    
    FONT_LOG("Font", APP_LOG_LEVEL_INFO, "Text: %s", text);
    /* see if we need to cache any glyphs */
    while(text[i] != '\0') {
        
        if (text[i] & 0b10000000) { // begin of multibyte character
            if ((text[i] & 0b11100000) == 0b11000000) {
                next_codepoint = ((text[i  ] &  0b11111) << 6)
                               +  (text[i+1] & 0b111111);
                next_i += 2;
            } else if ((text[i] & 0b11110000) == 0b11100000) {
                next_codepoint = ((text[i  ] &   0b1111) << 12)
                               + ((text[i+1] & 0b111111) << 6)
                               +  (text[i+2] & 0b111111);
                next_i += 3;
            } else if ((text[i] & 0b11111000) == 0b11110000) {
                next_codepoint = ((text[i  ] &    0b111) << 18)
                               + ((text[i+1] & 0b111111) << 12)
                               + ((text[i+2] & 0b111111) << 6)
                               +  (text[i+3] & 0b111111);
                next_i += 4;
            } else {
                next_codepoint = 0;
                next_i += 1;
            }
        } else {
            next_codepoint = text[i];
            next_i += 1;
        }
        
        i = next_i;
        
        /* check not already staged */
        bool found = false;
        for (uint16_t j = 0; j < cp_count; j++) {
            if (cp_to_load[j] == next_codepoint) {
                found = true;
                break;
            }
        }
        
        for (uint16_t j = 0; j < total_count; j++) {
            if (cp_to_exclude[j] == next_codepoint) {
                found = true;
                break;
            }
        }
        
        if (found) continue;
        
        /* go through the text and see if we are caching the codepoint */
        if (!_is_codepoint_in_cache(cached_font, next_codepoint)) {            
            cp_to_load[cp_count] = next_codepoint;
            cp_count++;
        } 
        else {
            cp_to_exclude[total_count] = next_codepoint;
            total_count++;
        }
        
        if (total_count + cp_count > CACHE_COUNT || 
            total_count > CACHE_COUNT || 
            cp_count > CACHE_COUNT) {
            /* we are now too big to fit into cache. Give up */
            FONT_LOG("Font", APP_LOG_LEVEL_INFO, "Serving raw font for dinner. font: 0x%x", cached_font->font);
            uint8_t *buffer = resource_fully_load_id_system(cached_font->resource_id);

            n_graphics_draw_text(ctx, text, (n_GFontInfo *)buffer, box,
                            overflow_mode, alignment,
                            text_attributes);
            
            app_free(buffer);
            return;
        }
    }
    
    FONT_LOG("Font", APP_LOG_LEVEL_INFO, "Adding %d to cache. font: 0x%x res: %d", cp_count, cached_font->font, cached_font->resource_id);
    
    if (cp_count)
    {
        if (cached_font->cached_glyph_count + cp_count > CACHE_COUNT)
        {
            _expire_cache_items(cached_font, cp_to_exclude, total_count, 
                                cached_font->cached_glyph_count + cp_count - CACHE_COUNT);
        }
        
        _add_glyphs_to_cache(cached_font, cp_to_load, cp_count);
    }
    else
    {
        FONT_LOG("Font", APP_LOG_LEVEL_INFO, "Font already fully cached. font: 0x%x res: %d", cached_font->font, cached_font->resource_id);        
    }

    n_graphics_draw_text(ctx, text, cached_font->font, box,
                            overflow_mode, alignment,
                            text_attributes);
}

void font_cache_remove_all(void)
{
    list_head *lh = _head_for_thread();
    list_node *ln = list_get_head(lh);
    uint16_t n = 0;
    
    if (!ln)
        return;
   
    while(ln && &lh->node != ln)
    {
        font_cache_t *elem = list_elem(ln, font_cache_t, node);

        list_node *lnext = ln->next;
        list_remove(lh, ln);
        app_free(elem->font);
        app_free(elem);
        ln = lnext;
        n++;
    }
    
    FONT_LOG("Font", APP_LOG_LEVEL_INFO, "Removed %d fonts from the cache", n);
}

void font_cache_remove_by_resource_id(uint16_t resource_id)
{
    return _remove_font_cache_entry(resource_id);
}

/* A utility to fill cach_info with all known offsets
 * sizes and all other generally useful font info
 */
static void _create_cache_info(cache_glyph_info_t *cache_info, n_GFontInfo *font)
{
    assert(cache_info);
    uint8_t *data;
    
    switch (font->version) {
        case 1:
            data = (uint8_t *) font + __FONT_INFO_V1_LENGTH;    
            break;
        case 2:
            data = (uint8_t *) font + __FONT_INFO_V2_LENGTH;
            break;
        default:
            data = (uint8_t *) font + font->fontinfo_size;
    
    }
    
    cache_info->hash_table_size = font->version >= 2 ? font->hash_table_size : 255;
    
    uint8_t *hash_entry = data;
    uint8_t *offset_start = data + (cache_info->hash_table_size * sizeof(n_GFontHashTableEntry));
    
    cache_info->codepoint_size = font->version >= 2 ? font->codepoint_bytes : 4;
    cache_info->offset_entry_size =  _offset_table_entry_size(font);
    cache_info->font_info_size = (uint16_t)(hash_entry - (uint8_t *)font);
    cache_info->hash_entry = hash_entry;
    cache_info->offset_entry = offset_start;
    cache_info->glyph_entry = offset_start + (cache_info->offset_entry_size * font->glyph_amount);
   
    /* We should check more here */
    assert(cache_info->hash_entry - (uint8_t*)font == cache_info->font_info_size);
}

/* Add a font to the cache */
static font_cache_t *_add_font_cache_entry(n_GFontInfo *font, uint16_t resource_id, uint32_t font_size)
{   
    /* TODO XXX check it's not already there! */
    font_cache_t *cfont = app_calloc(1, sizeof(font_cache_t));
    list_init_node(&cfont->node);

    cfont->font = font;
    cfont->resource_id = resource_id;
    
    cache_glyph_info_t fc;
    _create_cache_info(&fc, font);
    cfont->font_size = font_size;
    
    list_insert_head(_head_for_thread(), &cfont->node);
    
    return cfont;
}

/* remove a font to the cache */
static font_cache_t *_remove_font_cache_entry(uint16_t resource_id)
{
    font_cache_t *fc = _get_cache_entry_resource(resource_id);
    if (!fc)
        return;
    
    app_free(fc->font);
    
    list_head *lh = _head_for_thread();
    list_remove(lh, &fc->node);
    
    app_free(fc);
    
    return fc;
}

/* Get a cache entry for a resource Id */
static font_cache_t *_get_cache_entry_resource(uint16_t resource_id)
{
    font_cache_t *f;
    list_foreach(f, _head_for_thread(), font_cache_t, node)
    {
        if (f->resource_id == resource_id) 
            return f;
    }
    
    return NULL;
}

/* Given a thread type, get the font cache for it */
static list_head *_head_for_thread(void)
{
    AppThreadType thread_type = appmanager_get_thread_type();
    
    if (thread_type == AppThreadMainApp)
    {
        return &_font_cache_list_head_app;
    }
    else if (thread_type == AppThreadOverlay)
    {
        return &_font_cache_list_head_ovl;
    }
    
    KERN_LOG("font", APP_LOG_LEVEL_ERROR, "Why you need fonts?");
    return NULL;
}

/* Create a new, empty font given a template. 
 * We allocate copy, reset and return the new font
 */
static uint32_t _create_empty_font(n_GFontInfo *font, n_GFontInfo **font_to_create)
{
    cache_glyph_info_t fi;
    _create_cache_info(&fi, font);
    
    /* create a font with just the font info, hash table and offset. 
     * We only copy the null glyph and the tofu "default" glyph */
    n_GGlyphInfo *tofu = fi.glyph_entry + 4;
    uint32_t font_and_tofu_size = (fi.glyph_entry - (uint8_t *)font)
            + tofu->width * tofu->height;

    n_GFontInfo *new_font = app_calloc(1, font_and_tofu_size);
    /* copy the font into the new font-cache */
    memcpy(new_font, font, font_and_tofu_size);
    _create_cache_info(&fi, new_font);
    uint8_t *offset_entry = fi.offset_entry;
    
    /* reset all of the offsets to FFFF */
    for(uint16_t i = 0; i < font->glyph_amount; i++)
    {        
        /* ignore any codepoints that refer to tofu */
        if (_offset_entry_value_equal(offset_entry + fi.codepoint_size, fi.codepoint_size, 4))
        {
            offset_entry += fi.offset_entry_size;
            continue;
        }
        _offset_entry_set_offset(offset_entry, fi.codepoint_size, 0xFFFF);
        offset_entry += fi.offset_entry_size;
    }
    
    *font_to_create = new_font;
    
    return font_and_tofu_size;
}




/* Calculate the size of each entry in the offset table */
static inline uint8_t _offset_table_entry_size(n_GFontInfo *font)
{
    uint8_t codepoint_bytes = 4, features = 0;
    
    switch (font->version) {
        /* switch trickery! Default first is valid. */
        default:
            features = font->features;
        case 2:
            codepoint_bytes = font->codepoint_bytes;
        case 1:
            break;
    }
    
    return codepoint_bytes +
        (features & n_GFontFeature2ByteGlyphOffset ? 2 : 4);
}

/* Given a codepoint value, return the offset entry table location */
static uint8_t *_font_cache_offset_for_cp(n_GFontInfo *font, uint32_t codepoint, cache_glyph_info_t *fi)
{
    /* we could fast-forward through the while offset table, 
       but jumping through hash is optimal */
    n_GFontHashTableEntry * hash_data =
        (n_GFontHashTableEntry *) (fi->hash_entry +
            (codepoint % fi->hash_table_size) * sizeof(n_GFontHashTableEntry));
    
    /* Now we have the have entry value, get the offset from the hash */
    uint8_t *offset_entry = fi->offset_entry + hash_data->offset_table_offset;

    uint16_t iters = 0; 
    /* fast forward to the codepoint in question */
    while (!_offset_entry_value_equal(offset_entry, fi->codepoint_size, codepoint) &&
            iters < hash_data->offset_table_size) 
    {
        offset_entry += fi->offset_entry_size;
        iters++;
    }

    if (!_offset_entry_value_equal(offset_entry, fi->codepoint_size, codepoint))
        return NULL;
 
    return offset_entry;
}

/* given an entry in the offset table, check the value is the same as the passed */
static inline bool _offset_entry_value_equal(uint8_t *offset_entry, uint8_t codepoint_bytes, uint32_t value)
{
    return ((codepoint_bytes == 2
            ? *((uint16_t *) offset_entry)
            : *((uint32_t *) offset_entry)) == value);
}


/* given an entry in the offset table, check the value is known valid */
static inline bool _offset_entry_offset_valid(uint8_t *offset_entry, uint8_t codepoint_bytes)
{
    uint32_t val = codepoint_bytes == 2
            ? *((uint16_t *)(offset_entry + codepoint_bytes))
            : *((uint32_t *)(offset_entry + codepoint_bytes));

    return (val != 0xFFFF && val != 4 && val != 0);
}

/* Set the offset from the glyph start to the new font entry */
static inline void _offset_entry_set_offset(uint8_t *offset_entry, uint8_t codepoint_bytes, uint32_t offset)
{
    if (codepoint_bytes == 2)
        *((uint16_t *)(offset_entry + codepoint_bytes)) = (uint16_t)offset;
    else
        *((uint32_t *)(offset_entry + codepoint_bytes)) = (uint32_t)offset;
}

/* Runs through the cache and returns true if the codepoint is there */
static bool _is_codepoint_in_cache(font_cache_t *font, uint32_t codepoint)
{
    /* shortcut tofu */
    if (codepoint == 4)
        return true;
    
    cache_glyph_info_t fi;
    _create_cache_info(&fi, font->font);
    uint8_t *off = _font_cache_offset_for_cp(font->font, codepoint, &fi);
    assert(off);
    
    return !_offset_entry_value_equal(off + fi.codepoint_size, fi.codepoint_size, 0xFFFF);
}

/* Given a list of codepoints and a cached font, add the glpyhs to the cache 
 * Glyphs are not stored back to back like a regular font. There is a uint32 timestamp
 * between the glyphs. We use this later for sorting. It is transparent to the font renderer
 */
void _add_glyphs_to_cache(font_cache_t *cached_font, uint32_t codepoints[], uint16_t codepoint_count)
{
    cache_glyph_info_t fi;
    
#ifdef DEBUG_FONT
    printf("Font Caching Glyphs: ");    
    for (uint16_t i = 0; i < codepoint_count; i++) {
        printf(" %c", codepoints[i]);
    }
    printf("\n");
#endif
    
    /* we need to load this font now regardless */
    uint8_t *buffer = resource_fully_load_id_system(cached_font->resource_id);
    n_GFontInfo *loaded_font = (n_GFontInfo *)buffer;
    uint32_t total_size = 0;
    
    /* go through all of the uncached elements and calculate the total 
     * size of the new elements to cache */
    for (uint16_t i = 0; i < codepoint_count; i++) {
        /* TODO could optimise by not calling this and making a shortcut version... */
        n_GGlyphInfo *gi = n_graphics_font_get_glyph_info(loaded_font, codepoints[i]);
        assert(gi && "Invalid Glyph");
        
        total_size += sizeof(n_GGlyphInfo) + (gi->width * gi->height) + 4;
    }

    /* create the new font and copy the glyphs in */
    n_GFontInfo *nfont = app_calloc(1, cached_font->font_size + total_size);
    assert(nfont && "Out of memory pal");
    
    /* free old font and swap in the new one */
    memcpy(nfont, cached_font->font, cached_font->font_size);
    app_free(cached_font->font);
    cached_font->font = nfont;
    _create_cache_info(&fi, nfont);
    
    /* now we have a new font, add each to the cache */
    for (uint16_t i = 0; i < codepoint_count; i++) {
        uint8_t *off = _font_cache_offset_for_cp(cached_font->font, codepoints[i], &fi);
        assert(off && "Codepoint not in font");

        /* offset is NOT tofu or ffff? */
        if (_offset_entry_offset_valid(off, fi.codepoint_size))
        {
            FONT_LOG("Font", APP_LOG_LEVEL_INFO, "Codepoint %d already in cache. font: 0x%x res: %d", codepoints[i], cached_font->font, cached_font->resource_id);
            continue;
        }
        n_GGlyphInfo *gi = n_graphics_font_get_glyph_info(loaded_font, codepoints[i]);
        uint32_t glyph_size = sizeof(n_GGlyphInfo) + (gi->width * gi->height);
        uint32_t newoffset = ((uint8_t *)nfont + cached_font->font_size) - fi.glyph_entry + 4;

        /* update offset to the new entry point 
         * With an offset of +4. We use 4 bytes to store the timestamp */
        _offset_entry_set_offset(off, fi.codepoint_size, newoffset);
        memcpy((uint8_t *)nfont + cached_font->font_size + 4, gi, glyph_size);
        
        /* set the timestamp */
        *((uint32_t *)((uint8_t *)nfont + cached_font->font_size)) = xTaskGetTickCount(); //0xABCD + i;
        cached_font->font_size += glyph_size + 4;
        
        cached_font->cached_glyph_count++;
    }

    app_free(buffer);
}

/***** expiry ******/

typedef struct _cp_sort_tmp {
        uint32_t codepoint;
        uint32_t timestamp;
    } _cp_sort_tmp;
    
static int _cache_compare(const void *a, const void *b)
{    
    return (*(_cp_sort_tmp *)a).timestamp - (*(_cp_sort_tmp *)b).timestamp;
}

/* Given a font and a list of of codepoints to keep, and amount to purge, do:
 * Sort the glyphs by timestamp (stored before the glyph)
 * decide what to keep.
 * Expire n items that are not required
 * drop the existing font, create a new one and then cache the glyphs
 */
static void _expire_cache_items(font_cache_t *cached_font, uint32_t exclude_codepoints[], uint16_t exclude_count, uint16_t remove_count)
{
    
    _cp_sort_tmp cps[CACHE_COUNT];
    uint16_t codepoint_count = 0;
    cache_glyph_info_t fi;
    FONT_LOG("Font", APP_LOG_LEVEL_INFO, "Expiring %d glyphs of %d cached. Total font glyphs: %d. font: 0x%x res: %d", remove_count, cached_font->cached_glyph_count, cached_font->font->glyph_amount, cached_font->font, cached_font->resource_id);
    
    if (!remove_count || !cached_font->font)
        return;
    
    _create_cache_info(&fi, cached_font->font);
        
    /* hop through each offset */
    uint8_t *off = fi.offset_entry;
    
#ifdef DEBUG_FONT
    printf("Font Cache Contents: ");
#endif
    for (uint32_t i = 0; i < cached_font->font->glyph_amount; i++)
    {
        if (!_offset_entry_offset_valid(off, fi.codepoint_size))
        {
            off += fi.offset_entry_size;
            continue;
        }
        off += fi.offset_entry_size;
#ifdef DEBUG_FONT
        printf(" %c", *off);
    }
    printf(" \n");
#else
    }
#endif
        
    off = fi.offset_entry;

    for (uint32_t i = 0; i < cached_font->font->glyph_amount; i++)
    {
        /* ignore tofu and ffff */
        if (!_offset_entry_offset_valid(off, fi.codepoint_size))
        {
            off += fi.offset_entry_size;
            continue;
        }
        bool found = false;
        for (uint16_t exc = 0; exc < exclude_count; exc++)
        {
            if (exclude_codepoints[exc] == *off)
            {
                found = true;
                break;
            }
        }
        
        if (found)
        {
            off += fi.offset_entry_size;
            continue;
        }
        
        cps[codepoint_count].codepoint = *off;
        cps[codepoint_count].timestamp = *((uint32_t *)(fi.glyph_entry + *((uint16_t *)(off + fi.codepoint_size)) - 4));
        
        codepoint_count++;
        off += fi.offset_entry_size;
        
        /* just in case, quick sanity check. (nope, i'm still insane) */
        if (codepoint_count > CACHE_COUNT)
        {
            FONT_LOG("Font", APP_LOG_LEVEL_WARNING, "Warning, more valid glyphs found than expected found: %d expected %d. font: 0x%x res: %d", codepoint_count, CACHE_COUNT, cached_font->font, cached_font->resource_id);
            break;
        }
    }
    
    qsort(cps, codepoint_count, sizeof(_cp_sort_tmp), _cache_compare);
    
    if (remove_count > codepoint_count) 
        remove_count = codepoint_count;
    
    /* we have an array of pointers to table offsets, sorted by last used.
     * we will be putting them back into the cache. */
    uint32_t codepoints[exclude_count + codepoint_count];
    /* add the ones we want to keep */
    uint16_t tcnt = 0;
    for (tcnt = 0; tcnt < exclude_count; tcnt++)
        codepoints[tcnt] = exclude_codepoints[tcnt];

    for (uint16_t i = 0; i < codepoint_count - remove_count; i++)
        codepoints[i + tcnt] = cps[remove_count + i].codepoint;

    /* burn the items from the cache */
    n_GFontInfo *new_font;
    uint32_t font_size = _create_empty_font(cached_font->font, &new_font);
    app_free(cached_font->font);
    cached_font->font = new_font;
    cached_font->font_size = font_size;
    cached_font->cached_glyph_count = 0;
    _add_glyphs_to_cache(cached_font, codepoints, codepoint_count - remove_count + exclude_count);
}
