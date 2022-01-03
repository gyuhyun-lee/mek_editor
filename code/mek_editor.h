#ifndef MEK_EDITOR_H
#define MEK_EDITOR_H

// TODO(joon) don't love this magic number.. any way to maintain '0 is initialization' policy?
#define Invalid_Index u32_max

// NOTE(joon) This model originally came from vim
enum editor_mode
{
    Editor_Mode_Normal, 
    Editor_Mode_Insert,
    Editor_Mode_Visual,
};

// TODO(joon) someday, replace platform read file result with this?
struct opended_source_file
{
    u8 *memory;
    u32 size;
};

struct editor_state
{
    u8 *font_bitmap;
    u32 font_bitmap_width;
    u32 font_bitmap_height;

    stbtt_bakedchar *glyph_infos;

    u32 cursor;
    u32 saved_cursor_offset_x; //offset from most the line start, changes only if the user makes a horizontal move

    u32 visible_line_start; // one past newline charcter of a line that comes before the visible line
    u32 visible_line_offset_x; // horizontal offset, starting from visible_line_start

    b32 is_initialized;
};


// NOTE(joon) Each recoreded entry looks like : <header> <editor_state> <file> "<header>"
struct recorded_file_header
{
    u32 this_entry_index;

    u32 next_entry_index;
    u32 previous_entry_index;
};


// NOTE(joon) transient_state does not get copied into recoreded file
struct editor_transient_state
{
    // NOTE(joon) This always should come first! Platform Layer will get this value
    // by checking the very first element of permanent memory(which is where this state resides.)
    b32 is_running; 

    //////////////////////////////////////////////////////////////////////////////////////////////////

    // TODO(joon) Eventually turn this into some sort of linked list, to open multiple files
    platform_read_source_file_result opened_file;

    editor_mode mode;

    memory_arena render_arena;

    memory_arena file_arena;

    // NOTE(joon) only used for recording files, 0th element is always an original file
    memory_arena record_arena;
    u32 one_entry_past_record_read_index; // one entry past read index

    // TODO(joon) Gets rid of unswanted memcpy, but introduces complexity while rendering.
    u8 scratch_buffer[256];
    u32 scratch_buffer_count;
    u32 backspace_start_index;

    // TODO(joon) be aware of the buffer overflow!!(make the clipboard as big as the file?)
    u8 clipboard[256];
    u32 clipboard_count;

    // NOTE(joon) : If the render grid cannot handle all the scratch buffer,
    // this will be adjusted to move the render grid position.
    i32 render_grid_y_offset;

    b32 is_initialized;
};

#endif
