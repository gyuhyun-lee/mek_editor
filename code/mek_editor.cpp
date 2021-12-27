// TODO(joon): we will eventually replace stb with our own glyph loader :)
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "mek_types.h"
#include "mek_platform.h"
#include "mek_intrinsic.h"
#include "mek_math.h"

#include "mek_editor.h"

internal v3
invert_color(v3 color)
{
    v3 result = {};
    // TODO(joon) clamp?
    result.r = 1.0f - color.r;
    result.g = 1.0f - color.g;
    result.b = 1.0f - color.b;

    return result;
}


internal u32
get_one_past_previous_newline(u8 *file, u32 current, u32 count = 1)
{
    assert(file);

    u32 result = current;
    u32 newline_count = 0;
    while(result > 0)
    {
        u8 one_previous_char = file[result - 1];
        if(one_previous_char == '\n')
        {
            newline_count++;

            if(newline_count == count)
            {
                break;
            }
        }

        result--;
    }

    return result;
}

internal u32
get_diff_from_one_past_previous_newline(u8 *file, u32 current)
{
    u32 result = current - get_one_past_previous_newline(file, current);

    return result;
}

internal u32
get_one_past_next_newline(u8 *file, u32 file_size, u32 current, u32 count = 1)
{
    assert(count != 0);

    u32 result = current;
    u32 newline_count = 0;

    while(result < file_size - 1)
    {
        u8 c = file[result];
        if(c == '\n') 
        {
            result++;
            newline_count++;
            if(newline_count == count)
            {
                break;
            }
        }

        result++;
    }

    return result;
}

internal i32
get_line_diff_between(u8 *file, u32 file_size, u32 index0, u32 index1)
{
    i32 sign_coeff = 1;
    u32 start = index0;
    u32 end = index1;
    if(index0 > index1)
    {
        start = index1;
        end = index0;
        sign_coeff = -1;
    }

    assert(end < file_size);

    i32 result = 0;
    while(start < end)
    {
        u8 c = file[start];

        if(c == '\n' || c == '\r')
        {
            result++;
            start = get_one_past_next_newline(file, file_size, start);
        }
        else
        {
            start++;
        }
    }

    result *= sign_coeff;

    return result;
}

struct file_line_boundary
{
    u32 start;
    u32 one_past_end;
};

internal file_line_boundary
get_line_boundary(u8 *file, u32 file_size, u32 current)
{
    file_line_boundary result = {};

    result.start = get_one_past_previous_newline(file, current);
    result.one_past_end = get_one_past_next_newline(file, file_size, current);

    return result;
}

internal u32
pack_color(v4 color)
{
    u32 result = round_f32_to_u32(color.a * 255.0f) << 24 |
                round_f32_to_u32(color.r * 255.0f) << 16 |
                round_f32_to_u32(color.g * 255.0f) << 8 |
                round_f32_to_u32(color.b * 255.0f) << 0;

    return result;
}

internal void
clear(offscreen_buffer *offscreen_buffer, v3 color)
{
    u32 color_255 = pack_color({color.r, color.g, color.b, 1.0f});

    u8 *row = offscreen_buffer->memory;

    for(u32 y = 0;
            y < offscreen_buffer->height;
            ++y)
    {
        u32 *pixel = (u32 *)row;
        for(u32 x = 0;
                x < offscreen_buffer->width;
                ++x)
        {
            *pixel++ = color_255;
        }

        row += offscreen_buffer->stride;
    }
}

internal void
render_grid(offscreen_buffer *offscreen_buffer, 
            u32 grid_x, u32 grid_y, 
            u32 grid_width, u32 grid_height,
            u32 first_grid_offset_x, u32 first_grid_offset_y, v3 background_color)
{
    u32 min_x = first_grid_offset_x + grid_width * grid_x;
    u32 min_y = first_grid_offset_y + grid_height * grid_y;
    u32 one_past_max_x = min_x + grid_width;
    u32 one_past_max_y = min_y + grid_height;

    if(one_past_max_x > offscreen_buffer->width)
    {
        one_past_max_x = offscreen_buffer->width;
    }
    if(one_past_max_y > offscreen_buffer->height)
    {
        one_past_max_y = offscreen_buffer->height;
    }

    u32 draw_width = one_past_max_x - min_x;
    u32 draw_height = one_past_max_y - min_y;

    u32 background_color_rgb = round_f32_to_u32(background_color.r * 255.0f) << 16 |
                            round_f32_to_u32(background_color.g * 255.0f) << 8 |
                            round_f32_to_u32(background_color.b * 255.0f) << 0;

    u8 *row = offscreen_buffer->memory + 
                offscreen_buffer->stride * min_y + 
                sizeof(u32) * min_x;
    for(u32 y = 0;
            y < draw_height;
            ++y)
    {
        u32 *pixel = (u32 *)row;
        for(u32 x = 0;
                x < draw_width;
                ++x)
        {
            *pixel++ = 0xff000000 | background_color_rgb;
        }

        row += offscreen_buffer->stride;
    }
}

internal void
render_glyph_inside_grid(offscreen_buffer *offscreen_buffer, 
                        u8 *font_bitmap, u32 font_bitmap_width,
                        stbtt_bakedchar *glyph_info,
                        u32 grid_x, u32 grid_y, 
                        u32 grid_width, u32 grid_height,
                        u32 first_grid_offset_x, u32 first_grid_offset_y, v3 glyph_color, v3 background_color)
{
    u32 glyph_width = glyph_info->x1 - glyph_info->x0;
    u32 glyph_height = glyph_info->y1 - glyph_info->y0;

    u32 min_x = first_grid_offset_x + grid_width * grid_x;
    u32 min_y = first_grid_offset_y + grid_height * grid_y;

    u32 one_past_max_x = min_x + grid_width;
    if(one_past_max_x > offscreen_buffer->width)
    {
        one_past_max_x = offscreen_buffer->width;
    }
    u32 one_past_max_y = min_y + grid_height;
    if(one_past_max_y > offscreen_buffer->height)
    {
        one_past_max_y = offscreen_buffer->height;
    }

    u32 draw_width = one_past_max_x - min_x;
    u32 draw_height = one_past_max_y - min_y;

    u32 glyph_color_255 = (round_f32_to_u32(glyph_color.r * 255.0f) << 16) |
                        (round_f32_to_u32(glyph_color.g * 255.0f) << 8) |
                        (round_f32_to_u32(glyph_color.b * 255.0f) << 0);

    u32 background_color_255 = round_f32_to_u32(background_color.r * 255.0f) << 16 |
                            round_f32_to_u32(background_color.g * 255.0f) << 8 |
                            round_f32_to_u32(background_color.b * 255.0f) << 0;

    u8 *row = offscreen_buffer->memory + 
                offscreen_buffer->stride * min_y + 
                sizeof(u32) * min_x;
    for(u32 y = 0;
            y < draw_height;
            ++y)
    {
        u32 *pixel = (u32 *)row;
        for(u32 x = 0;
                x < draw_width;
                ++x)
        {
            *pixel = 0xff000000 | background_color_255;

            // TODO(joon) We won't be having this problem, if we have evenly sized font bitmap
            // TODO(joon) This does seem like a hack, and does not properly work with y,p... etc
            u32 glyph_and_grid_height_diff = grid_height - glyph_height;
            f32 tex_x = (f32)x/(f32)glyph_width;
            // TODO(joon) Out of bounds, is y is smaller than the diff
            f32 tex_y = (f32)(y - glyph_and_grid_height_diff)/(f32)glyph_height;

            if(tex_x >= 0.0f && tex_x <= 1.0f &&
                tex_y >= 0.0f && tex_y <= 1.0f)
            {
                u32 sample_x = glyph_info->x0 + round_f32_to_u32(glyph_width * tex_x);
                u32 sample_y = glyph_info->y0 + round_f32_to_u32(glyph_height * tex_y);

                // TODO(joon) use font bitmap value as a alpha value to achieve anti-aliasing!
                if(font_bitmap[sample_y * font_bitmap_width + sample_x])
                {
                    *pixel = 0xff000000 | glyph_color_255;
                    int a = 1;
                }
                else
                {
                }
            }

            pixel++;
        }


        row += offscreen_buffer->stride;
    }
}


internal u8
peek_previous_character(u8 *file, u32 index)
{
    u8 result = 0;
    if(index > 0)
    {
        result = file[index-1];
    }

    return result;
}
internal u8
peek_one_past_charcter(u8 *file, u32 file_size, u32 index)
{
    u8 result = 0;
    if(index < file_size - 1)
    {
        result = file[index + 1];
    }

    return result;
}


UPDATE_AND_RENDER(update_and_render)
{
    editor_state *state = (editor_state *)memory->permanent_memory;
    if(!state->is_initialized)
    {
        state->font_bitmap_width = 512;
        state->font_bitmap_height = 512;
        // TODO(joon) Replace malloc with our own memory arena
        state->font_bitmap = (u8 *)malloc(sizeof(u8) * state->font_bitmap_width * state->font_bitmap_height);
        u32 glyph_count = 256; // TODO(joon) Only supports ascii for now

        // TODO(joon) replace stb with my own font loader?
        state->glyph_infos = (stbtt_bakedchar *)malloc(sizeof(stbtt_bakedchar) * glyph_count);
        platform_read_file_result font = platform_api->read_file("/Users/mekalopo/Library/Fonts/InputMonoCompressed-Light.ttf");
        int result = stbtt_BakeFontBitmap((unsigned char *)font.memory, 0,
                                            50.0f, // TODO(joon) This does not correspond to the actual pixel size, but to get higher pixel density, we need to crank this up
                                            (unsigned char *)state->font_bitmap, state->font_bitmap_width, state->font_bitmap_height,
                                            0, glyph_count,
                                            state->glyph_infos);

        state->opened_file = platform_api->read_file_with_extra_memory("/Volumes/meka/mek_editor/data/test.cpp");

        state->is_initialized = true;
    }

    // NOTE(joon) Process inputs
    // right
    if(input->is_l_down)
    {
        if(state->cursor != state->opened_file.size - 1)
        {
            file_line_boundary current_line = get_line_boundary(state->opened_file.memory, state->opened_file.size, state->cursor);

            state->cursor = clamp(current_line.start, state->cursor+1, current_line.one_past_end - 1);
            state->cursor_offset_from_line_start = state->cursor - current_line.start;
        }
    }

    // left
    if(input->is_h_down)
    {
        // TODO(joon) This check is needed because the cursor is u32.. any way to make this cleaner?
        if(state->cursor != 0)
        {
            file_line_boundary current_line = get_line_boundary(state->opened_file.memory, state->opened_file.size, state->cursor);

            state->cursor = clamp(current_line.start, state->cursor-1, current_line.one_past_end - 1);
            state->cursor_offset_from_line_start = state->cursor - current_line.start;
        }
    }

    // up
    if(input->is_k_down)
    {
        u32 previous_newline = get_one_past_previous_newline(state->opened_file.memory, state->cursor);
        if(previous_newline != 0)
        {
            file_line_boundary target_line = get_line_boundary(state->opened_file.memory, state->opened_file.size, previous_newline - 1);

            state->cursor = clamp(target_line.start, target_line.start + state->cursor_offset_from_line_start, target_line.one_past_end - 1);
        }
    }

    // down
    if(input->is_j_down)
    {
        u32 one_past_next_newline = get_one_past_next_newline(state->opened_file.memory, state->opened_file.size, state->cursor);
        file_line_boundary target_line = get_line_boundary(state->opened_file.memory, state->opened_file.size, one_past_next_newline);

        state->cursor = clamp(target_line.start, target_line.start + state->cursor_offset_from_line_start, target_line.one_past_end - 1);
    }

    u32 first_grid_offset_x = 0;
    u32 first_grid_offset_y = 0;

    // TODO(joon) how to adjust width based on height?
    u32 grid_height = 40;
    u32 grid_width = grid_height/2;

    u32 max_grid_x = (offscreen_buffer->width - first_grid_offset_x) / grid_width;
    u32 max_grid_y = (offscreen_buffer->height - first_grid_offset_y) / grid_height;

#if 1
    if(state->offset_from_visible_line_start + max_grid_x < state->cursor_offset_from_line_start)
    {
        state->offset_from_visible_line_start += round_f32_to_u32(0.5f*max_grid_x);
    }
    else if(state->offset_from_visible_line_start > state->cursor_offset_from_line_start)
    {
        u32 delta = round_f32_to_u32(0.5f*max_grid_x);

        if(state->offset_from_visible_line_start < delta)
        {
            state->offset_from_visible_line_start = 0;
        }
        else
        {
            state->offset_from_visible_line_start -= delta;
        }
    }
#endif

    i32 line_diff_between_cursor_and_start_of_visible_line = get_line_diff_between(state->opened_file.memory, 
                                                                                    state->opened_file.size, 
                                                                                    state->visible_line_start, state->cursor);

    if(line_diff_between_cursor_and_start_of_visible_line > 0)
    {
        if(line_diff_between_cursor_and_start_of_visible_line >= max_grid_y)
        {
            // TODO(joon) This only support one line movement, what if we move multiple lines at once?
            state->visible_line_start = get_one_past_next_newline(state->opened_file.memory, state->opened_file.size, state->visible_line_start);
        }
    }
    else if(line_diff_between_cursor_and_start_of_visible_line < 0)
    {
        state->visible_line_start = get_one_past_previous_newline(state->opened_file.memory, state->visible_line_start, 2);
    }

#if 1

    u32 grid_x = 0;
    u32 grid_y = 0;

    clear(offscreen_buffer, {0.0f, 0.0f, 0.0f});

    // NOTE(joon) one character should be one byte
    u32 current = state->visible_line_start;
    while(current < state->opened_file.size)
    {
        v3 glyph_color = {0, 1, 1};
        v3 background_color = {0, 0, 0};

        u8 current_char = state->opened_file.memory[current];
        if(current_char == '\n' || current_char == '\r')
        {
            if(current == state->cursor)
            {
                render_grid(offscreen_buffer, 
                            grid_x, grid_y, 
                            grid_width, grid_height, 
                            first_grid_offset_x, first_grid_offset_y, {0, 1, 1});
            }

            grid_x = 0;
            grid_y++;
        }
        else
        {
            file_line_boundary current_line = get_line_boundary(state->opened_file.memory, state->opened_file.size, current);
            if(current - current_line.start >= state->offset_from_visible_line_start)
            {

                if(current_char != 0)
                {
                    stbtt_bakedchar *glyph_info = state->glyph_infos + current_char;

                    if(current == state->cursor)
                    {
                        // TODO(joon) reverse the glyph color?
                        glyph_color = invert_color(glyph_color);
                        background_color.r = 0;
                        background_color.g = 1;
                        background_color.b = 1;
                    }

                    // TODO(joon) Also take account of kernel(bearing) to decide where to start
                    render_glyph_inside_grid(offscreen_buffer, 
                                        state->font_bitmap, state->font_bitmap_width,
                                        glyph_info,
                                        grid_x, grid_y, 
                                        grid_width, grid_height,
                                        first_grid_offset_x, first_grid_offset_y, glyph_color, background_color);
                }

                ++grid_x;
            }
        }

        if(grid_x == max_grid_x)
        {
            grid_x = 0;
            grid_y++;

            // NOTE(joon) wrapping
            current = get_one_past_next_newline(state->opened_file.memory, state->opened_file.size, current);
        }
        else
        {
            current++;
        }
        if(grid_y == max_grid_y + 1)
        {
            break;
        }

    }
#endif

#if 1
    // NOTE(joon) draw pixel grid
    {
        u8 *row = offscreen_buffer->memory + 
                        offscreen_buffer->stride * first_grid_offset_y + 
                        sizeof(u32) * first_grid_offset_x;
        for(u32 y = 0;
                y < offscreen_buffer->height;
                ++y)
        {
            u32 *pixel = (u32 *)row;
            for(u32 x = 0;
                x < offscreen_buffer->width;
                ++x)
            {
                if(x % grid_width == 0 || x % grid_width == grid_width - 1 ||
                    y % grid_height == 0 || y % grid_height == grid_height - 1)
                {
                    *pixel = 0x701654;
                }

                pixel++;
            }

            row += offscreen_buffer->stride;
        }
    }
#endif




}
