// TODO(joon): we will eventually replace stb with our own glyph loader :)
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
include "mek_types_h"
#include "stb_truetype.h"
"
this is really fun!
'mef

#include "mek_types.h"
#include "mek_platform.h"
#include "mek_intrinsic.h"
#include "mek_math.h"

#include "mek_editor.h"

/* TODO(joon) Here are the minimum requirements as an source editor
   - save file
   - allocate more memory(contiguous?) when we append the source(by allocating a new memory block & copying the original buffer to it), 
    with failsafe features(what happens if the user writes a _giant_ file?)
    - For now, backspaced spaces are just getting wasted. Instead of allocating new buffer, pack the file without backspace characters?
    - enter
    - canonicalize visible line start
    - canonicalize the cursor only before the rendering?
    - redo / undo
    - move vertically based on tab ? 
    - auto indent(when we also do the enter)
   - input stream, so that we don't miss any inputs & handle input in a sane order

   Nice to have features
   - normal mode / visual mode / input mode for vim-like features
   - easy commands
   - fast move between characters
   - copy & paste
   - line specific scrolling
   - config file
   - syntax highlighting
*/

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
string_length(char *string)
{
    u32 result = 0;
    char *c = string;

    while(*c)
    {
        result++;
    }

    return result;
}

struct search_result
{
    b32 found;
    u32 index;
};

// TODO(joon) we can expand this later to search for certain string or token
internal search_result
backward_search(search_result)
{
    search_result result = {};
    result.index = current;

    while(result.index >= 0)
    {
        if(file[result.index] == search)
        {
            result.found = true;
            break;
        }
        
        result.index--;
    }

    return result;
}

// TODO(joon) we can expand this later to search for certain string or token
internal search_result
backward_search_not(u8 *file, u32 current, char search)
{
    search_result result = {};
    result.index = current;

    while(result.index >= 0)
    {
        if(file[result.index] != search)
        {
            result.found = true;
            break;
        }
        
        result.index--;
    }

    return result;
}

internal u32
get_one_past_last_character(u8 *file, u32 file_size, u32 current = 0)
{
    u32 result = current;

    while(file[result] != 0 && result < file_size)
    {
        result++;
    }

    assert(current <= result);

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
            newline_count++;
            if(newline_count == count)
            {
                result++;
                break;
            }
        }
        else if(c == 0)
        {
            break;
        }

        result++;
    }

    return result;
}

internal u32
get_one_previous_next_newline_character(u8 *file, u32 current, u32 count = 1)
{
    assert(count != 0);

    u32 result = current;
    u32 newline_count = 0;

    while(result > 0)
    {
        u8 c = file[result];
        if(c == '\n') 
        {
            newline_count++;
            if(newline_count == count)
            {
                result--;
                break;
            }
        }
        else if(c == 0)
        {
            break;
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
    // TODO(joon) just store end? Noone actually uses one_past_end...
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
get_th_charcter_in_line(u8 *file, u32 file_size, u32 search_index, u32 th_count)
{
    file_line_boundary search_line_boundary = get_line_boundary(file, file_size, search_index);
    u32 result = search_line_boundary.start;

    u32 found_character_count = 0;
    while(found_character_count < th_count && 
        result < (search_line_boundary.one_past_end - 1))
    {
        if(file[result] != '\b')
        {
           found_character_count++;
        }

        result++;
    }

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

internal u32
get_actual_file_size(platform_read_source_file_result file)
{
    u32 result = 0;

    u8 *c = file.memory;
    u8 *end = file.memory + file.size;
    while(*c++ != 0 && c < end)
    {
        // NOTE(joon) Add whatever character that you want to ignore
        if(*c != '\b')
        {
            result++;
        }
    }

    return result;
}

// TODO(joon) This save method traverses the memory twice - 
// first time to get the actual size, and second time to actually save the memory.
// Can we do something better here?
internal u32
save_file(u8 *dest, u32 size_to_save, platform_read_source_file_result opened_file)
{
    u8 *source = opened_file.memory;

    for(u32 byte_index = 0;
            byte_index < size_to_save;
            ++byte_index)
    {
        if(*source != '\b')
        {
            *dest++ = *source;
        }
        source++;

        assert(source < opened_file.memory + opened_file.size);
    }

    return size_to_save;
}

internal void
output_file(platform_api *platform_api, memory_arena *arena, 
            platform_read_source_file_result file)
{
    temp_memory temp_memory = start_temp_memory(arena, file.size, false);
    u8 *temp_dest = (u8 *)push_size(&temp_memory, file.size);
    
    u32 size_to_save = get_actual_file_size(file);

    save_file(temp_dest, size_to_save, file);
    platform_api->write_entire_file(file.file_name, temp_dest, size_to_save);

    end_temp_memory(temp_memory);
}

internal void
render_grid(offscreen_buffer *offscreen_buffer, 
            u32 grid_x, u32 grid_y, 
            u32 first_grid_offset_x, u32 first_grid_offset_y, 
            u32 grid_width, u32 grid_height,
            v3 background_color)
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
                        u32 first_grid_offset_x, u32 first_grid_offset_y, 
                        v3 glyph_color, v3 background_color)
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

#if 0
internal b32
get_key_down(virtual_key key, r32 time_until_repeat, r32 dt)
{
    b32 result = 0;

    // TODO(joon) any way to get more robust single press timeout?
    r32 single_press_timeout = 0.3f*dt; 

    // NOTE(joon) state change also contributes to the input, because the user might have been pressed and released
    // the key in one frame
    if((key.ended_down && (key.time_passed < single_press_timeout || key.time_passed >= time_until_repeat)) || 
        (!key.ended_down && key.state_change_count >= 2))
    {
        result = true;
    }

    return result;
}
#endif

internal void
insert_new_character(editor_transient_state *tran_state, u8 *file, u32 file_size, u32 cursor, u32 c_to_insert)
{
    tran_state->changed_anything_in_insert_mode = true;
    u8 cursor_c = file[cursor];

    // NOTE(joon) If the character was \b, we can just replace it without moving the memory
    if(cursor_c != '\b')
    {
        u32 one_past_last_character = get_one_past_last_character(file, file_size, cursor);
        assert(one_past_last_character != file_size);

        u32 move_region = one_past_last_character - cursor;
        // TODO(joon) how do we add characters without copying the memory?
        // TODO(joon) we can at least use our own scratch buffer to store the original memory
        // TODO(joon) Instead of moving the data for each input, mark the memory with a special character(or just remember the position) &
        // write into a seperate scratch buffer, and then when we press esc, write into memory?
        memmove(file + cursor + 1, file + cursor, move_region);
    }

    file[cursor] = c_to_insert; // printable 
}

inline u32
get_left_closest_character(u8 *file, u32 current)
{
    assert(file);

    u32 result = 0;
    if(current != 0)
    {
        result = current - 1;
        while(result > 0)
        {
            if(file[result] != '\b')
            {
                break;
            }

            result--;
        }
    }

    return result;
}

internal u32
get_right_closest_charcter(u8 *file, u32 file_size, u32 current)
{
    assert(file);
    u32 result = current + 1;

    while(result < file_size)
    {
        u8 c = file[result];

        if(c == 0 ||  c != '\b')
        {
            break;
        }

        result++;
    }

    return result;
}

internal void
record_file(editor_state *state, editor_transient_state *tran_state)
{
    // TODO(joon) This is not a precise assert
    //assert(tran_state->one_pastrecord_read_index <= tran_state->record_arena.used);

    memory_arena *record_arena = &tran_state->record_arena;

    recorded_file_header *one_past_read_header = (recorded_file_header *)(record_arena->base + tran_state->one_entry_past_record_read_index);
    fallback_memory_arena(record_arena, one_past_read_header->this_entry_index);

    u32 file_size_to_save = get_actual_file_size(tran_state->opened_file);
    u32 total_required_size = sizeof(recorded_file_header) + sizeof(editor_state) + file_size_to_save;

    // NOTE(joon) DO NOT ZERO OUT THE MEMORY! This header will contain the previous header index
    recorded_file_header *header = (recorded_file_header *)push_size(&tran_state->record_arena, total_required_size, false);
    header->next_entry_index = tran_state->record_arena.used;

    // save editor state
    editor_state *recorded_state = (editor_state *)((u8 *)header + sizeof(recorded_file_header));
    *recorded_state = *state;

    // save file
    u8 *recorded_file = (u8 *)recorded_state + sizeof(editor_state); 
    save_file(recorded_file, file_size_to_save, tran_state->opened_file);

    // secretly add another header, without changing the memory arena
    recorded_file_header *next_header = (recorded_file_header *)((u8 *)tran_state->record_arena.base + tran_state->record_arena.used);
    next_header->previous_entry_index = header->this_entry_index;
    // TODO(joon) This does not work if the memory arena is aligned?
    next_header->this_entry_index = header->next_entry_index;

    tran_state->one_entry_past_record_read_index = header->next_entry_index;
}

internal void
enter_insert_mode(editor_state *state, editor_transient_state *tran_state)
{
    //if(tran_state->changed_anything_in_insert_mode)
    {
        record_file(state, tran_state);
    }
    tran_state->mode = Editor_Mode_Insert;
}

// TODO(joon) Just allow wrapping?
internal void
handle_input_normal_mode(editor_state *state, editor_transient_state *tran_state, platform_input *input, platform_api *platform_api, u8 keycode)
{
    switch(keycode)
    {
        // left
        case 'h':
        {
            // TODO(joon) This check is needed because the cursor is u32.. any way to make this cleaner(i.e  by allowing wrapping?)
            if(state->cursor != 0)
            {
                file_line_boundary current_line = get_line_boundary(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);

                state->cursor = clamp_exclude_max(current_line.start, 
                                                get_left_closest_character(tran_state->opened_file.memory, state->cursor), 
                                                current_line.one_past_end);
                state->saved_cursor_offset_from_line_start = state->cursor - current_line.start;
            }
        }break;
        // right
        case 'l':
        {
            if(state->cursor != tran_state->opened_file.size - 1)
            {
                file_line_boundary current_line = get_line_boundary(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);

                state->cursor = clamp_exclude_max(current_line.start, 
                                                    get_right_closest_charcter(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor), 
                                                    current_line.one_past_end);
                state->saved_cursor_offset_from_line_start = state->cursor - current_line.start;
            }
        }break;

        // up
        case 'k':
        {
            u32 one_past_previous_newline = get_one_past_previous_newline(tran_state->opened_file.memory, state->cursor);
            if(one_past_previous_newline != 0)
            {
                state->cursor = get_th_charcter_in_line(tran_state->opened_file.memory, tran_state->opened_file.size, one_past_previous_newline - 1, 
                                                        state->saved_cursor_offset_from_line_start);
            }
        }break;
        
        // down
        case 'j':
        {
            u32 one_past_next_newline = get_one_past_next_newline(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);
            state->cursor = get_th_charcter_in_line(tran_state->opened_file.memory, tran_state->opened_file.size, one_past_next_newline, 
                                                        state->saved_cursor_offset_from_line_start);
        }break;

        case 'G':
        {
            u32 one_past_last_character = get_one_past_last_character(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);
            state->cursor = get_th_charcter_in_line(tran_state->opened_file.memory, tran_state->opened_file.size, one_past_last_character, state->saved_cursor_offset_from_line_start);
        }break;
#if 0
        case '\b':
        {
            search_result first_non_backspace_search_result = backward_search_not(tran_state->opened_file.memory, state->cursor, '\b');
            if(first_non_backspace_search_result.found)
            {
                tran_state->opened_file.memory[first_non_backspace_search_result.index] = '\b';
            }

            tran_state->mode = Editor_Mode_Insert;
        }break;
#endif
        case '$':
        {
            state->cursor = get_one_previous_next_newline_character(tran_state->opened_file.memory, state->cursor);
        }break;

        case '0':
        {
            state->cursor = get_one_past_previous_newline(tran_state->opened_file.memory, state->cursor);
            state->saved_cursor_offset_from_line_start = 0;
        }break;

        case 'i':
        {
            enter_insert_mode(state, tran_state);
        }break;

        case 'a':
        {
            file_line_boundary target_line = get_line_boundary(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);
            state->cursor = clamp(target_line.start, state->cursor + 1, target_line.one_past_end);

            enter_insert_mode(state, tran_state);
        }break;

        case 'o':
        {
            // TODO(joon) see how this performs with the last line
            u32 one_past_next_newline = get_one_past_next_newline(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);
            file_line_boundary target_line = get_line_boundary(tran_state->opened_file.memory, tran_state->opened_file.size, one_past_next_newline);
            state->cursor = target_line.start;
            enter_insert_mode(state, tran_state);

            insert_new_character(tran_state, tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor, '\n');
        }break;
        case 'u':
        {
            u8 *base = (u8 *)tran_state->record_arena.base;

            recorded_file_header *one_past_header = (recorded_file_header *)(base + tran_state->one_entry_past_record_read_index);
            recorded_file_header *header = (recorded_file_header *)(base + one_past_header->previous_entry_index);
            u32 block_size = header->next_entry_index - header->this_entry_index;
            tran_state->one_entry_past_record_read_index = header->this_entry_index;

            // save editor state
            editor_state *recorded_state = (editor_state *)((u8 *)header + sizeof(recorded_file_header));
            *state = *recorded_state; 

            // TODO(joon) Safety check for recorded file bigger than our buffer!
            copy_memory_and_zero_out_the_rest(tran_state->opened_file.memory, 
                                            tran_state->opened_file.size, 
                                            (u8 *)recorded_state + sizeof(editor_state), 
                                            block_size - sizeof(recorded_file_header) - sizeof(editor_state));
        }break;
        case 's':
        {
            output_file(platform_api, &tran_state->file_arena, tran_state->opened_file);
        }break;
        case 'q':
        {
            tran_state->is_running = false;
        }break;
    }
}

internal void
handle_input_insert_mode(editor_state *state, editor_transient_state *tran_state, platform_input *input, u8 keycode)
{
    switch(keycode)
    {
        case 27: //esc
        {
            tran_state->changed_anything_in_insert_mode = false;
            tran_state->mode = Editor_Mode_Normal;
        }break;
        case '\b':
        {
            search_result first_non_backspace_search_result = backward_search_not(tran_state->opened_file.memory, state->cursor-1, '\b');
            if(first_non_backspace_search_result.found)
            {
                tran_state->opened_file.memory[first_non_backspace_search_result.index] = '\b';
            }           
        }break;
        case 32: // space
        {
            insert_new_character(tran_state, tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor, ' ');
            state->cursor++; // We know that the next chacter is space, so no need to check
        }break;
        case 13: // enter
        {
            insert_new_character(tran_state, tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor, '\n');
            state->cursor++;
        }break;
        default:
        {
            if(keycode >= 33 && keycode <= 126)
            {
                insert_new_character(tran_state, tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor, keycode);
                state->cursor++;
            }
        }break;
    }
}

extern "C" UPDATE_AND_RENDER(update_and_render)
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

        state->is_initialized = true;
    }

    if(!state->is_initialized)
    {
        state->font_bitmap_width = 512;
        state->font_bitmap_height = 512;
    }

    editor_transient_state *tran_state = (editor_transient_state *)memory->transient_memory;
    if(!tran_state->is_initialized)
    {
        tran_state->file_arena = start_memory_arena((u8 *)memory->transient_memory + sizeof(*tran_state) + memory->transient_memory_used, megabytes(16), 
                                                &memory->transient_memory_used, true);

        u32 file_size = platform_api->get_file_size("/Volumes/meka/mek_editor/data/test.cpp");

        copy_string(tran_state->opened_file.file_name, "/Volumes/meka/mek_editor/data/test.cpp");
        tran_state->opened_file.size = 2.0f * file_size; 
        tran_state->opened_file.memory = (u8 *)push_size(&tran_state->file_arena, tran_state->opened_file.size, true);
        platform_api->read_source_file("/Volumes/meka/mek_editor/data/test.cpp", tran_state->opened_file.memory, file_size);

        tran_state->record_arena = start_memory_arena((u8 *)memory->transient_memory + sizeof(*tran_state) + memory->transient_memory_used, megabytes(64), 
                                                &memory->transient_memory_used, true);

        // NOTE(joon) record the original file
        record_file(state, tran_state);

        tran_state->is_initialized = true;
    }

    input->time_until_repeat = 0.2f;

    while(input->stream_read_index != input->stream_write_index)
    {
        input_stream_key *key = input->stream + input->stream_read_index;

        input->stream_read_index = (input->stream_read_index + 1) % array_count(input->stream);
        switch(tran_state->mode)
        {
            case Editor_Mode_Normal:
            {
                handle_input_normal_mode(state, tran_state, input, platform_api, key->keycode);
            }break;
     
            case Editor_Mode_Insert:
            {
                handle_input_insert_mode(state, tran_state, input, key->keycode);
            }break;
     
            case Editor_Mode_Visual:
            {
                //handle_input_insert_mode(state, input);
            }break;
        }
    }


    u32 first_grid_offset_x = 0;
    u32 first_grid_offset_y = 0;

    // TODO(joon) how to adjust width based on height?
    u32 grid_height = 40;
    u32 grid_width = grid_height/2;

    u32 max_grid_x = (offscreen_buffer->width - first_grid_offset_x) / grid_width;
    u32 max_grid_y = (offscreen_buffer->height - first_grid_offset_y) / grid_height;

    file_line_boundary current_line = get_line_boundary(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);
    u32 current_cursor_offset_from_line_start = state->cursor - current_line.start;
    u32 screen_delta = round_f32_to_u32(0.5f*max_grid_x);

    // NOTE(joon) move visible line horizontally
    if(state->offset_from_visible_line_start + max_grid_x < current_cursor_offset_from_line_start)
    {
        state->offset_from_visible_line_start += screen_delta;
    }
    else if(state->offset_from_visible_line_start > current_cursor_offset_from_line_start)
    {
        if(state->offset_from_visible_line_start < screen_delta)
        {
            state->offset_from_visible_line_start = 0;
        }
        else
        {
            state->offset_from_visible_line_start -= screen_delta;
        }
    }

    // NOTE(joon) move visible line vertically
    i32 line_diff_between_cursor_and_start_of_visible_line = get_line_diff_between(tran_state->opened_file.memory, 
                                                                                    tran_state->opened_file.size, 
                                                                                    state->visible_line_start, state->cursor);
    if(line_diff_between_cursor_and_start_of_visible_line > 0)
    {
        if(line_diff_between_cursor_and_start_of_visible_line >= max_grid_y)
        {
            u32 vertical_move_count = line_diff_between_cursor_and_start_of_visible_line - max_grid_y + 1;
            state->visible_line_start = 
                get_one_past_next_newline(tran_state->opened_file.memory, tran_state->opened_file.size, 
                                            state->visible_line_start, vertical_move_count);
        }
    }
    else if(line_diff_between_cursor_and_start_of_visible_line < 0)
    {
        state->visible_line_start = get_one_past_previous_newline(tran_state->opened_file.memory, state->cursor);
    }

#if 1

    u32 grid_x = 0;
    u32 grid_y = 0;

    // TODO(joon) We are drawing every grid anyway, so is this necessary?
    clear(offscreen_buffer, {0.0f, 0.0f, 0.0f});

    // NOTE(joon) one character should be one byte
    u32 current = state->visible_line_start;
    while(current < tran_state->opened_file.size)
    {
        v3 glyph_color = {1, 1, 1};
        v3 background_color = {0, 0, 0};
        v3 cursor_color = {0, 1, 1};

        if(current == state->cursor)
        {
            if(tran_state->mode == Editor_Mode_Insert)
            {
                cursor_color = invert_color(cursor_color);
            }

            glyph_color = invert_color(glyph_color);
            background_color = cursor_color;
        }

        u8 current_char = tran_state->opened_file.memory[current];
        file_line_boundary current_line = get_line_boundary(tran_state->opened_file.memory, tran_state->opened_file.size, current);
        if(current - current_line.start >= state->offset_from_visible_line_start)
        {
            switch(current_char)
            {
                case '\n': // for modern computers, newline always ends with \n
                {
#if 1
    // NOTE(joon) Render newline grid
                    render_grid(offscreen_buffer, 
                                grid_x, grid_y, 
                                first_grid_offset_x, first_grid_offset_y, 
                                grid_width, grid_height, 
                                {0, 0.4f, 0});
#endif
                    if(current == state->cursor)
                    {

                        render_grid(offscreen_buffer, 
                                    grid_x, grid_y, 
                                    first_grid_offset_x, first_grid_offset_y, 
                                    grid_width, grid_height, 
                                    background_color);
                    }

                    grid_x = 0;
                    grid_y++;
                }break;

                case '\b':
                {
                }break;

                case 0:
                {
                    render_grid(offscreen_buffer, 
                                grid_x, grid_y, 
                                first_grid_offset_x, first_grid_offset_y, 
                                grid_width, grid_height, 
                                {1, 0, 0});

                    ++grid_x;
                }break;

                default:
                {
                    stbtt_bakedchar *glyph_info = state->glyph_infos + current_char;


                    // TODO(joon) Also take account of kernel(bearing) to decide where to start
                    render_glyph_inside_grid(offscreen_buffer, 
                                            state->font_bitmap, state->font_bitmap_width,
                                            glyph_info,
                                            grid_x, grid_y, 
                                            grid_width, grid_height,
                                            first_grid_offset_x, first_grid_offset_y, glyph_color, background_color);
                    ++grid_x;

                }break;
            }

        }
        
        if(grid_x == max_grid_x)
        {
            grid_x = 0;
            grid_y++;

            // NOTE(joon) wrapping
            current = get_one_past_next_newline(tran_state->opened_file.memory, tran_state->opened_file.size, current);
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

#if 0
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
if
}
f
}
f
}
if
}
f
}
ffdaf
asdfasdf
asdfasdfasdfasd
asdfasdfasdfasdf
bbasdfasdfasdfasdf
adfasdfa
dfasdfa
fa
sdf
adfasdfa
fa
dfasdf
adfasdfa
fa

fa

sdfasdfasdf
adfasdfa
fa

fa

fasdfa
fa

fa

fa
fa

fa

a

fa

fa
fa

fa

a








fa

a





a





a

fa

a





a
fa

fa

fa
fa

fa

a








fa

a










dfasdfasdfasdf
adfasdfa
fa

fa

fa
fa

fa

a





















aaa
aa

aa
a
