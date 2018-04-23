/* font_loader.c
 * routines for [...]
 * libRebbleOS
 *
 * Author: Barry Carter <barry.carter@gmail.com>
 */

#include "rebbleos.h"
#include "librebble.h"
#include "platform_res.h"


uint16_t _fonts_get_resource_id_for_key(const char *key);
GFont fonts_get_system_font_by_resource_id(uint32_t resource_id);


void fonts_init(void)
{
//     KERN_LOG("font", APP_LOG_LEVEL_ERROR, "font init");
}

void fonts_resetcache()
{

}

GFont fonts_get_system_font(const char *font_key)
{
    return font_load_system_font(font_key);
}

/*
 * Load a system font from the resource table
 * Will save into a cheesey cache so it isn't loaded over and over.
 */
GFont fonts_get_system_font_by_resource_id(uint32_t resource_id)
{
    return font_load_system_font_by_resource_id(resource_id);
}

/*
 * Load a custom font
 */
GFont *fonts_load_custom_font(ResHandle *handle, const struct file* file)
{   
    uint8_t *buffer = resource_fully_load_res_app(*handle, file);

    return (GFont *)buffer;
}

/*
 * Unload a custom font
 */
void fonts_unload_custom_font(GFont font)
{
    app_free(font);
}

#define EQ_FONT(font) (strncmp(key, "RESOURCE_ID_" #font, strlen(key)) == 0) return RESOURCE_ID_ ## font;

/*
 * Load a font by a string key
 */
uint16_t _fonts_get_resource_id_for_key(const char *key)
{   
    // this seems kinda.... messy and bad. Why a char * Pebble? why pass strings around?
    // I got my answer from Heiko:
    /*
     @Heiko 
     That API has been around forever. Strings are an easy way to maintain an ABI contract between app and firmware compiled at different times. Was helpful as different SDK versions and models introduced various new fonts over time. It also allows for "secret fonts" that were not (yet) published. While one could accomplish the same with enums that have gaps and vary over time we already had those names in the firmware. And of course we had to maintain backwards compatibility when one of the fonts was renamed... again old API :wink:
      
     */
    // so still seems like a bad choice, but backward compat.
         if EQ_FONT(AGENCY_FB_60_THIN_NUMBERS_AM_PM)
    else if EQ_FONT(AGENCY_FB_60_NUMBERS_AM_PM)
    else if EQ_FONT(AGENCY_FB_36_NUMBERS_AM_PM)
    else if EQ_FONT(GOTHIC_09)
    else if EQ_FONT(GOTHIC_14)
    else if EQ_FONT(GOTHIC_14_BOLD)
    else if EQ_FONT(GOTHIC_18)
    else if EQ_FONT(GOTHIC_18_BOLD)
    else if EQ_FONT(GOTHIC_24)
    else if EQ_FONT(GOTHIC_24_BOLD)
    else if EQ_FONT(GOTHIC_28)
    else if EQ_FONT(GOTHIC_28_BOLD)
    else if EQ_FONT(GOTHIC_36)
    else if EQ_FONT(BITHAM_18_LIGHT_SUBSET)
    else if EQ_FONT(BITHAM_34_LIGHT_SUBSET)
    else if EQ_FONT(BITHAM_30_BLACK)
    else if EQ_FONT(BITHAM_42_BOLD)
    else if EQ_FONT(BITHAM_42_LIGHT)
    else if EQ_FONT(BITHAM_34_MEDIUM_NUMBERS)
    else if EQ_FONT(BITHAM_42_MEDIUM_NUMBERS)
    else if EQ_FONT(ROBOTO_CONDENSED_21)
    else if EQ_FONT(ROBOTO_BOLD_SUBSET_49)
    else if EQ_FONT(DROID_SERIF_28_BOLD)
    else if EQ_FONT(LECO_20_BOLD_NUMBERS)
    else if EQ_FONT(LECO_26_BOLD_NUMBERS_AM_PM)
    else if EQ_FONT(LECO_32_BOLD_NUMBERS)
    else if EQ_FONT(LECO_36_BOLD_NUMBERS)
    else if EQ_FONT(LECO_38_BOLD_NUMBERS)
    else if EQ_FONT(LECO_28_LIGHT_NUMBERS)
    else if EQ_FONT(LECO_42_NUMBERS)
    else if EQ_FONT(FONT_FALLBACK)
                                                                                                                                
    return RESOURCE_ID_FONT_FALLBACK;
}
