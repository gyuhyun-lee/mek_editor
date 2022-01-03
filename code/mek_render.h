#ifndef MEK_RENDER_H
#define MEK_RENDER_H

struct render_grid_element
{
    b32 highlighted;
    v3 highlight_color;

    u8 keycode;
};

struct render_grid
{
    render_grid_element *elements;

    // NOTE(joon) visible line start is (0, 0) in grid space, top down.
    i32 start_x;
    i32 start_y;

    i32 one_past_end_x;
    i32 one_past_end_y;

    i32 grid_width;
    i32 grid_height;

    i32 first_grid_offset_x;
    i32 first_grid_offset_y;
};


struct render_group
{
    temp_memory render_memory;

    u8 *font_bitmap; 
    u32 font_bitmap_width;
    u32 font_bitmap_height;

    stbtt_bakedchar *glyph_infos;
};

enum render_entry_type
{
    Render_Entry_Type_Glyph,
    Render_Entry_Type_Rect,
};

struct render_entry_header
{
    render_entry_type type;

    i32 x;
    i32 y;

    i32 width;
    i32 height;
};

struct render_entry_glyph
{
    render_entry_header header;
    u32 font_bitmap_x;
    u32 font_bitmap_y;

    u32 glyph_width;
    u32 glyph_height;

    v3 glyph_color; 
    v3 background_color;
};


struct render_entry_rect
{
    render_entry_header header;
    // TODO(joon) offset?
    v3 color;
};

#endif
