#ifndef MEK_EDITOR_H
#define MEK_EDITOR_H

struct editor_state
{
    // NOTE(joon) Eventually turn into some kind of linked list, to open multiple files
    platform_read_file_with_extra_memory_result opened_file;

    // NOTE(joon): Where are we at the file? 

    u32 cursor;
    u32 cursor_offset_from_line_start; //offset from most the line start

    // NOTE(joon) mostly used inside rendering
    u32 visible_line_start;
    u32 offset_from_visible_line_start;

    u8 *font_bitmap;
    u32 font_bitmap_width;
    u32 font_bitmap_height;

    stbtt_bakedchar *glyph_infos;

    b32 is_initialized;
};

#endif
