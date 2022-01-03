// TODO(joon): we will eventually replace stb with our own glyph loader :)
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "mek_types.h"
#include "mek_platform.h"
#include "mek_intrinsic.h"
#include "mek_math.h"

#include "mek_editor.h"

#include "mek_render.cpp"

/* TODO(joon) Here are the minimum requirements as an source editor
   - allocate more memory(contiguous?) when we append the source(by allocating a new memory block & copying the original buffer to it), 
    with failsafe features(what happens if the user writes a _giant_ file?)
    - redo
    - auto indent(tab, enter)
   - copy & paste
   - normal mode / visual mode / input mode for vim-like features
   - only update when focused
 
   Nice to have features
   - easy commands
   - fast move between characters
   - syntax highlighting
   - redraw only the changed region
   - one key for 'per keyword deletion'
   - smooth scrolling(sublime, 4coder)

   Things to think about
   - Wrapping inside the file
   - config file
   - line specific scrolling
   - file struct
*/

/*
   NOTE(joon) : possible command tokens
    del (delete)
    is (inside)
    pas (paste)
    copy (copy)
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

internal u32
get_logical_cursor(editor_transient_state *tran_state, u32 cursor)
{
    u32 result = cursor;

    if(tran_state->backspace_start_index != Invalid_Index)
    {
        result = tran_state->backspace_start_index;
    }

    result += tran_state->scratch_buffer_count;

    return result;
}

// TODO(joon) we can expand this later to search for certain string or token
internal search_result
backward_search(u8 *file, u32 current, char search)
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

// TODO(joon)
internal u32
get_one_past_previous_newline(u8 *file, u32 file_size, u32 current, u32 count = 1)
{
    assert(file && count != 0);

    u32 result = current;
    if(file_size != 0)
    {
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
    }
    else
    {
        result = 0;
    }

    return result;
}

internal u32
get_one_past_next_newline(u8 *file, u32 file_size, u32 current, u32 count = 1)
{
    assert(file && count != 0);

    u32 result = current;
    if(file_size > 0)
    {
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
    }
    else
    {
        result = 0;
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
get_line_number(u8 *file, u32 file_size, u32 index)
{
    assert(file && index <= file_size);

    i32 result = 0;
    u32 start = 0;
    while(start < index)
    {
        if(file[start] == '\n')
        {
            result++;
        }

        start++;
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

    result.start = get_one_past_previous_newline(file, file_size, current);
    result.one_past_end = get_one_past_next_newline(file, file_size, current);

    return result;
}

internal u32
get_th_charcter_in_line(u8 *file, u32 file_size, u32 somewhere_in_line, u32 th_count)
{
    file_line_boundary search_line_boundary = get_line_boundary(file, file_size, somewhere_in_line);

    u32 result = clamp_exclude_max(search_line_boundary.start, search_line_boundary.start + th_count, search_line_boundary.one_past_end);

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
        *dest++ = *source++;

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
    
    u32 size_to_save = get_one_past_last_character(file.memory, file.size);

    save_file(temp_dest, size_to_save, file);
    platform_api->write_entire_file(file.file_name, temp_dest, size_to_save);

    end_temp_memory(temp_memory);
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
delete_range(u8 *file, u32 file_size, u32 start, u32 one_past_end)
{
    if(start < one_past_end)
    {
        u32 one_past_last_character = get_one_past_last_character(file, file_size, start);
        assert(one_past_last_character < file_size);

        u32 move_region_size = one_past_last_character - one_past_end;
        // TODO(joon) how do we add characters without copying the memory?
        // TODO(joon) we can at least use our own scratch buffer to store the original memory
        // TODO(joon) Instead of moving the data for each input, mark the memory with a special character(or just remember the position) &
        // write into a seperate scratch buffer, and then when we press esc, write into memory?
        memmove(file + start, file + one_past_end, move_region_size);
    }
}


// TODO(joon) just like delete function, pass in start & end instead of making a new function(insert_string)?
internal void
insert_string(u8 *file, u32 file_size, u32 index_to_insert, u8 *string_to_insert, u32 string_to_insert_size)
{
    u32 one_past_end_of_file_index = get_one_past_last_character(file, file_size);
    // TODO(joon) This should not be an assert, make the file bigger!!
    assert(one_past_end_of_file_index + string_to_insert_size <= file_size);

    u32 move_region = one_past_end_of_file_index - index_to_insert;
    // TODO(joon) we can use our own buffer to store the original memory, instead of memmove doing memory allocation
    memmove(file + index_to_insert + string_to_insert_size, file + index_to_insert, move_region);
    memcpy(file + index_to_insert, string_to_insert, string_to_insert_size);
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

    u32 file_size_to_save = get_one_past_last_character(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);
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
    // TODO(joon) only record if we changed anything!
    //if(tran_state->changed_anything)
    {
        record_file(state, tran_state);
    }
    tran_state->mode = Editor_Mode_Insert;
}

internal void
enter_normal_mode(editor_state *state, editor_transient_state *tran_state)
{
    if(tran_state->backspace_start_index != Invalid_Index)
    {
        delete_range(tran_state->opened_file.memory, tran_state->opened_file.size, 
                    tran_state->backspace_start_index, state->cursor);

        state->cursor = tran_state->backspace_start_index;
    }
    
    if(tran_state->scratch_buffer_count != 0)
    {
        insert_string(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor, 
                    tran_state->scratch_buffer, tran_state->scratch_buffer_count);  
        // TOOD(joon) safe?
        state->cursor += tran_state->scratch_buffer_count;
    }
    
    u32 cursor_line_start = get_one_past_previous_newline(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);
    state->saved_cursor_offset_x = state->cursor - cursor_line_start;

    tran_state->backspace_start_index = Invalid_Index;

    tran_state->scratch_buffer_count = 0;

    tran_state->mode = Editor_Mode_Normal;
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
                state->saved_cursor_offset_x = state->cursor - current_line.start;
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
                state->saved_cursor_offset_x = state->cursor - current_line.start;
            }
        }break;

        // up
        case 'k':
        {
            u32 one_past_previous_newline = get_one_past_previous_newline(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);
            if(one_past_previous_newline != 0)
            {
                state->cursor = get_th_charcter_in_line(tran_state->opened_file.memory, tran_state->opened_file.size, one_past_previous_newline - 1, 
                                                        state->saved_cursor_offset_x);
            }
        }break;
        
        // down
        case 'j':
        {
            u32 one_past_next_newline = get_one_past_next_newline(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);
            state->cursor = get_th_charcter_in_line(tran_state->opened_file.memory, tran_state->opened_file.size, one_past_next_newline, 
                                                        state->saved_cursor_offset_x);
        }break;

        case 'G':
        {
            u32 one_past_last_character = get_one_past_last_character(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);
            state->cursor = get_th_charcter_in_line(tran_state->opened_file.memory, tran_state->opened_file.size, one_past_last_character, state->saved_cursor_offset_x);
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
            file_line_boundary current_line = get_line_boundary(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);
            state->cursor = clamp_exclude_max(current_line.start, current_line.one_past_end - 1, current_line.one_past_end);
            state->saved_cursor_offset_x = state->cursor - current_line.start;
        }break;

        case '0':
        {
            state->cursor = get_one_past_previous_newline(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);
            state->saved_cursor_offset_x = 0;
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

        // TODO(joon) change this to use the scratch buffer by inserting \n at the start of the scratch buffer
        case 'o':
        {
            // TODO(joon) see how this performs with the last line
            u32 one_past_next_newline = get_one_past_next_newline(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor);
            state->cursor = one_past_next_newline-1;
            enter_insert_mode(state, tran_state);

            tran_state->scratch_buffer[tran_state->scratch_buffer_count++] = '\n';
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
            enter_normal_mode(state, tran_state);
        }break;
        case '\b':
        {
            if(tran_state->scratch_buffer_count > 0)
            {
                tran_state->scratch_buffer_count--;
            }
            else
            {
                if(tran_state->backspace_start_index == Invalid_Index)
                {
                    tran_state->backspace_start_index = state->cursor;
                }

                if(tran_state->backspace_start_index > 0)
                {
                    tran_state->backspace_start_index--;
                }
            }
#if 0
            search_result first_non_backspace_search_result = backward_search_not(tran_state->opened_file.memory, state->cursor-1, '\b');
            if(first_non_backspace_search_result.found)
            {
                tran_state->opened_file.memory[first_non_backspace_search_result.index] = '\b';
            }           
#endif
        }break;
        case 32: // space
        {
            assert(tran_state->scratch_buffer_count < array_count(tran_state->scratch_buffer));
            tran_state->scratch_buffer[tran_state->scratch_buffer_count++] = ' ';

            //insert_single_character(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor, '\n');

            // TODO(joon) What should we do with the cursor?
            //state->cursor++; // We know that the next chacter is space, so no need to check
        }break;
        case 13: // enter
        {
            assert(tran_state->scratch_buffer_count < array_count(tran_state->scratch_buffer));
            tran_state->scratch_buffer[tran_state->scratch_buffer_count++] = '\n';

            //insert_single_character(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor, '\n');
            //state->cursor++;
        }break;
        default:
        {
            if(keycode >= 33 && keycode <= 126)
            {
                assert(tran_state->scratch_buffer_count < array_count(tran_state->scratch_buffer));
                tran_state->scratch_buffer[tran_state->scratch_buffer_count++] = keycode;
                //insert_single_character(tran_state->opened_file.memory, tran_state->opened_file.size, state->cursor, keycode);
                //state->cursor++;
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

        tran_state->render_arena = start_memory_arena((u8 *)memory->transient_memory + sizeof(*tran_state) + memory->transient_memory_used, megabytes(512), 
                                                &memory->transient_memory_used, true);
        
        tran_state->backspace_start_index = Invalid_Index;

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

    // NOTE(joon) order is important!
    // TODO(joon) how to adjust width based on height?
    render_grid render_grid = {};
    render_grid.start_x = state->visible_line_offset_x;
    render_grid.start_y = tran_state->render_grid_y_offset;
    render_grid.grid_height = 40;
    render_grid.grid_width = render_grid.grid_height/2;
    render_grid.first_grid_offset_x = 0;
    render_grid.first_grid_offset_y = 0;

    i32 grid_count_x = (offscreen_buffer->width - render_grid.first_grid_offset_x) / render_grid.grid_width;
    i32 grid_count_y = (offscreen_buffer->height - render_grid.first_grid_offset_y) / render_grid.grid_height;

    render_grid.one_past_end_x = render_grid.start_x + grid_count_x;
    render_grid.one_past_end_y = render_grid.start_y + grid_count_y;

    temp_memory grid_temp_memory = start_temp_memory(&tran_state->render_arena, sizeof(render_grid_element) * render_grid.one_past_end_x * render_grid.one_past_end_y, true);
    render_grid.elements = (render_grid_element *)grid_temp_memory.base;

    i32 line_diff = get_line_diff_between(tran_state->opened_file.memory, tran_state->opened_file.size, state->visible_line_start, minimum(tran_state->backspace_start_index, state->cursor));
    if(line_diff >= grid_count_y)
    {
        state->visible_line_start = get_one_past_next_newline(tran_state->opened_file.memory, tran_state->opened_file.size, state->visible_line_start, line_diff - grid_count_y + 1);
    }
    else if(line_diff < 0)
    {
        state->visible_line_start = get_one_past_previous_newline(tran_state->opened_file.memory, tran_state->opened_file.size, minimum(tran_state->backspace_start_index, state->cursor));
    }

    i32 logical_cursor_grid_x = 0;

    i32 x = 0;
    i32 y = 0;
    u32 current = state->visible_line_start;
    while(current < tran_state->opened_file.size)
    {
        if(current >= tran_state->backspace_start_index && current < state->cursor)
        {
            current = state->cursor;
        }

        if(current == state->cursor)
        {
            highlight_render_grid(&render_grid, x, y, {1, 0, 0});

            for(u32 scratch_buffer_index = 0;
                    scratch_buffer_index < tran_state->scratch_buffer_count;
                    ++scratch_buffer_index)
            {
                fill_render_grid(&render_grid, &x, &y, tran_state->scratch_buffer[scratch_buffer_index]);
            }

            logical_cursor_grid_x = x;

            if(tran_state->mode == Editor_Mode_Insert)
            {
                highlight_render_grid(&render_grid, x, y, {0, 1, 0});
            }
            else
            {
                highlight_render_grid(&render_grid, x, y, {0, 1, 1});
            }
        }

        fill_render_grid(&render_grid, &x, &y, tran_state->opened_file.memory[current]);

        current++;

        if(y >= grid_count_y)
        {
            tran_state->render_grid_y_offset = y - grid_count_y;
            break;
        }

    }

    // NOTE(joon) rendering codes
    clear(offscreen_buffer, {0 ,0, 0});

    render_group render_group = {};

    render_group.render_memory = start_temp_memory(&tran_state->render_arena, megabytes(64), false);
    render_group.font_bitmap = state->font_bitmap;
    render_group.font_bitmap_width = state->font_bitmap_width;
    render_group.font_bitmap_height = state->font_bitmap_height;
    render_group.glyph_infos = state->glyph_infos;

    for(u32 grid_y = 0;
            grid_y < render_grid.one_past_end_y;
            ++grid_y)
    {
        for(u32 grid_x = 0;
                grid_x < render_grid.one_past_end_x;
                ++grid_x)
        {
            render_grid_element *element = render_grid.elements + grid_y * render_grid.one_past_end_x + grid_x;

            // TODO(joon) load these values from the config file
            v3 background_color = {0, 0, 0};
            v3 glyph_color = {1, 1, 1};
            v3 highlighted_glyph_color = {0, 0, 0};

            if(element->keycode >= '!' && element->keycode <= '~')
            {
                if(element->highlighted)
                {
                    push_glyph(&render_group, 
                                &render_grid,
                                element->keycode,
                                grid_x, grid_y, 
                                render_grid.grid_width, render_grid.grid_height, 
                                highlighted_glyph_color, element->highlight_color);
                }
                else
                {
                    push_glyph(&render_group, 
                                &render_grid,
                                element->keycode,
                                grid_x, grid_y, 
                                render_grid.grid_width, render_grid.grid_height, 
                                glyph_color, background_color);
                }
            }
            else
            {
                push_rect(&render_group, 
                            &render_grid,
                            grid_x, grid_y,
                            render_grid.grid_width, render_grid.grid_height,
                            element->highlight_color);
            }
        }
    }
    render_render_group(&render_group, offscreen_buffer);

    // NOTE(joon) ideally we should adjust visible line before rendering thsi frame,
    // but it causes a lot of unwanted overhead while traversing the file, 
    // as the scratch buffer is not searched while doing so.
    // This will cause 1 frame delay while rendering the source code.

    // NOTE(joon) adjust visible line horizontally
    u32 screen_delta = round_f32_to_u32(0.5f*grid_count_x);
    i32 offset_x_to_move = 0;
    // TODO(joon) Replace this with proper floor function
    f32 diff = (f32)(logical_cursor_grid_x - (i32)render_grid.start_x) / (f32)grid_count_x;
    if(diff >= 0)
    {
        offset_x_to_move = (i32)diff;
    }
    else
    {
        offset_x_to_move = (i32)(diff - 1.0f);
    }
    state->visible_line_offset_x += offset_x_to_move * screen_delta;

    

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

    end_temp_memory(grid_temp_memory);
    end_temp_memory(render_group.render_memory);
}
