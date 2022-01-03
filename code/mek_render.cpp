#include "mek_render.h"

internal void
highlight_render_grid(render_grid *grid, i32 grid_x, i32 grid_y, v3 color)
{
    if(grid_x >= grid->start_x && grid_x < grid->one_past_end_x && 
        grid_y >= grid->start_y && grid_y < grid->one_past_end_y)
    {
        render_grid_element *element = grid->elements + (grid_y - grid->start_y) * grid->one_past_end_x + 
                                        (grid_x - grid->start_x);

        element->highlighted = true;
        element->highlight_color = color;
    }
}

internal void
set_render_grid_keycode(render_grid *grid,
                i32 grid_x, i32 grid_y, u8 keycode)
{
    if(grid_x >= grid->start_x && grid_x < grid->one_past_end_x && 
        grid_y >= grid->start_y && grid_y < grid->one_past_end_y)
    {
        render_grid_element *element = grid->elements + (grid_y - grid->start_y) * grid->one_past_end_x + 
                                        (grid_x - grid->start_x);

        element->keycode = keycode;
    }
}

internal void
fill_render_grid(render_grid *grid, i32 *x, i32 *y, u8 keycode)
{
    switch(keycode)
    {
        case '\n': // for modern computers, newline always ends with \n
        {
            //highlight_grid(grid, one_past_max_grid_x, one_past_max_grid_y, *x, *y, {0, 0.4f, 0});
            *x = 0;
            *y += 1;
        }break;

        case ' ':
        {
            *x += 1;
        }break;

        case 0: 
        {
            highlight_render_grid(grid, *x, *y, {1, 0, 0});
            *x += 1;
        }break;

        default:
        {
            if(keycode >= '!' && keycode <= '~')
            {
                set_render_grid_keycode(grid, *x, *y, keycode);
                *x += 1;
            }
        }break;
    }
}

internal void
render_grid_rect(offscreen_buffer *offscreen_buffer, 
            i32 x, i32 y, 
            i32 width, i32 height,
            v3 background_color)
{

    i32 min_x = maximum(x, 0);
    i32 min_y = maximum(y, 0);
    i32 one_past_max_x = minimum(min_x + width, offscreen_buffer->width);
    i32 one_past_max_y = minimum(min_y + height, offscreen_buffer->height);

    u32 background_color_rgb = round_f32_to_u32(background_color.r * 255.0f) << 16 |
                            round_f32_to_u32(background_color.g * 255.0f) << 8 |
                            round_f32_to_u32(background_color.b * 255.0f) << 0;

    u8 *row = offscreen_buffer->memory + 
                offscreen_buffer->stride * min_y + 
                sizeof(u32) * min_x;

    for(i32 y = min_y;
            y < one_past_max_y;
            ++y)
    {
        u32 *pixel = (u32 *)row;
        for(i32 x = min_x;
                x < one_past_max_x;
                ++x)
        {
            *pixel++ = 0xff000000 | background_color_rgb;
        }

        row += offscreen_buffer->stride;
    }
}

internal void
render_glyph(offscreen_buffer *offscreen_buffer, 
                        u8 *font_bitmap, u32 font_bitmap_width,
                        u32 font_bitmap_x, u32 font_bitmap_y,
                        i32 glyph_width, i32 glyph_height,

                        i32 x, i32 y, 
                        i32 width, i32 height,

                        v3 glyph_color, v3 background_color)
{
    i32 min_x = maximum(x, 0);
    i32 min_y = maximum(y, 0);
    i32 one_past_max_x = minimum(min_x + width, offscreen_buffer->width);
    i32 one_past_max_y = minimum(min_y + height, offscreen_buffer->height);

    u32 glyph_color_255 = (round_f32_to_u32(glyph_color.r * 255.0f) << 16) |
                        (round_f32_to_u32(glyph_color.g * 255.0f) << 8) |
                        (round_f32_to_u32(glyph_color.b * 255.0f) << 0);

    u32 background_color_255 = round_f32_to_u32(background_color.r * 255.0f) << 16 |
                            round_f32_to_u32(background_color.g * 255.0f) << 8 |
                            round_f32_to_u32(background_color.b * 255.0f) << 0;

    u8 *row = offscreen_buffer->memory + 
                offscreen_buffer->stride * min_y + 
                sizeof(u32) * min_x;
    for(i32 y = min_y;
            y < one_past_max_y;
            ++y)
    {
        u32 *pixel = (u32 *)row;
        for(i32 x = min_x;
                x < one_past_max_x;
                ++x)
        {
            *pixel = 0xff000000 | background_color_255;

            // TODO(joon) We won't be having this problem, if we have evenly sized font bitmap
            // TODO(joon) properly get the texcoord, based on bearing & kerning
            u32 glyph_height_diff = height - glyph_height;
            f32 tex_x = (f32)(x - min_x)/(f32)glyph_width;
            f32 tex_y = (f32)(y - glyph_height_diff - min_y)/(f32)glyph_height;

            if(tex_x >= 0.0f && tex_x <= 1.0f &&
                tex_y >= 0.0f && tex_y <= 1.0f)
            {
                u32 sample_x = font_bitmap_x + round_f32_to_u32(glyph_width * tex_x);
                u32 sample_y = font_bitmap_y + round_f32_to_u32(glyph_height * tex_y);

                // TODO(joon) use font bitmap value as a alpha value to achieve anti-aliasing!
                r32 t = (r32)font_bitmap[sample_y * font_bitmap_width + sample_x] / 255.0f;
                r32 r = lerp(background_color.r, t, glyph_color.r);
                r32 g = lerp(background_color.g, t, glyph_color.g);
                r32 b = lerp(background_color.b, t, glyph_color.b);
                
                u32 result = 0xff000000 |
                            (round_f32_to_u32(r * 255.0f) << 16) |
                            (round_f32_to_u32(g * 255.0f) << 8) |
                            (round_f32_to_u32(b * 255.0f) << 0);

                *pixel = result;
            }

            pixel++;
        }

        row += offscreen_buffer->stride;
    }
}

struct render_basis
{
    // NOTE(joon) based on the top left corner of the render grid, top down.
    i32 x;
    i32 y;
};

internal render_basis
get_grid_based_render_basis(render_grid *grid, i32 grid_x, i32 grid_y)
                            
{
    render_basis result = {};

    // NOTE(joon) grid x and y are already relative to the render grid, so no need to subtract by the start x and y
    result.x = grid_x * grid->grid_width + (i32)grid->first_grid_offset_x;
    result.y = grid_y * grid->grid_height + (i32)grid->first_grid_offset_y;

    return result;
}

internal void
push_glyph(render_group *group, 
            render_grid *grid,
            u8 keycode,
            i32 grid_x, i32 grid_y, 
            i32 width, i32 height,
            v3 glyph_color, v3 background_color)
{

    render_basis basis = get_grid_based_render_basis(grid, grid_x, grid_y); 
    render_entry_header *header = (render_entry_header *)push_struct(&group->render_memory, render_entry_glyph);

    header->x = basis.x;
    header->y = basis.y;
    header->width = width;
    header->height = height;
    header->type = Render_Entry_Type_Glyph;

    render_entry_glyph *entry = (render_entry_glyph *)header;

    stbtt_bakedchar *glyph_info = group->glyph_infos + keycode;

    entry->font_bitmap_x = glyph_info->x0;
    entry->font_bitmap_y = glyph_info->y0;
    entry->glyph_width = glyph_info->x1 - glyph_info->x0;
    entry->glyph_height = glyph_info->y1 - glyph_info->y0;

    entry->glyph_color = glyph_color; 
    entry->background_color = background_color;
}

internal void 
push_rect(render_group *group, 
          render_grid *grid,
          i32 grid_x, i32 grid_y,
          i32 width, i32 height,
          v3 color)
{
    render_entry_header *header = (render_entry_header *)push_struct(&group->render_memory, render_entry_rect);
    render_basis basis = get_grid_based_render_basis(grid, grid_x, grid_y); 
    header->x = basis.x;
    header->y = basis.y;
    header->width = width;
    header->height = height;
    header->type = Render_Entry_Type_Rect;

    render_entry_rect *entry = (render_entry_rect *)header;

    entry->color = color;
}


internal void
render_render_group(render_group *group, offscreen_buffer *buffer)
{
    for(u32 base_address = 0;
            base_address < group->render_memory.used;
            )
    {
        render_entry_header *header = (render_entry_header *)((u8 *)group->render_memory.base + base_address);

        switch(header->type)
        {
            case Render_Entry_Type_Rect:
            {
                render_entry_rect *entry = (render_entry_rect *)header;

                render_grid_rect(buffer, 
                            header->x, header->y, 
                            header->width, header->height,
                            entry->color);

                base_address += sizeof(render_entry_rect);
            }break;

            case Render_Entry_Type_Glyph:
            {
                render_entry_glyph *entry = (render_entry_glyph *)header;

                render_glyph(buffer, 
                            group->font_bitmap, group->font_bitmap_width,
                            entry->font_bitmap_x, entry->font_bitmap_y,
                            entry->glyph_width, entry->glyph_height,

                            header->x, header->y, 
                            header->width, header->height,
                            entry->glyph_color, entry->background_color);

                base_address += sizeof(render_entry_glyph);
            }break;
        };
    }
}








