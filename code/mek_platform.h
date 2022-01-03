#ifndef MEK_PLATFORM_H
#define MEK_PLATFORM_H

#ifdef __cplusplus
extern "C"{
#endif

#undef internal
#undef assert

#define assert(Expression) if(!(Expression)) {int *a = 0; *a = 0;}
#define array_count(Array) (sizeof(Array) / sizeof(Array[0]))
#define maximum(a, b) ((a > b) ? a : b)
#define minimum(a, b) ((a < b) ? a : b)

#define global static
#define local_persist static
#define internal static

#define kilobytes(value) value*1024LL
#define megabytes(value) 1024LL*kilobytes(value)
#define gigabytes(value) 1024LL*megabytes(value)
#define terabytes(value) 1024LL*gigabytes(value)

#define Sec_To_Nano_Sec 1.0e+9f
#define Sec_To_Micro_Sec 1000.0f

struct platform_read_file_result
{
    u8 *memory;
    u64 size; // TOOD/joon : make this to be at least 64bit
};

struct platform_read_source_file_result
{
    u8 *memory;
    // TODO(joon) This needs to be adjusted, if the user writes more than the buffer size
    u32 size; // size that we allocate
    char file_name[1024]; //TODO(joon) out of bounds?
};

#define PLATFORM_GET_FILE_SIZE(name) u64 (name)(char *filename)
typedef PLATFORM_GET_FILE_SIZE(platform_get_file_size);

#define PLATFORM_READ_FILE(name) platform_read_file_result (name)(char *filename)
typedef PLATFORM_READ_FILE(platform_read_file);

#define PLATFORM_READ_FILE_FOR_EDITOR(name) platform_read_source_file_result (name)(char *file_name, u8 *dest, u32 size)
typedef PLATFORM_READ_FILE_FOR_EDITOR(platform_read_file_for_editor);

#define PLATFORM_WRITE_ENTIRE_FILE(name) void (name)(char *file_name, void *memory_to_write, u32 size)
typedef PLATFORM_WRITE_ENTIRE_FILE(platform_write_entire_file);

#define PLATFORM_FREE_FILE_MEMORY(name) void (name)(void *memory)
typedef PLATFORM_FREE_FILE_MEMORY(platform_free_file_memory);

struct platform_api
{
    platform_read_file *read_file;
    platform_write_entire_file *write_entire_file;
    platform_free_file_memory *free_file_memory;

    platform_get_file_size *get_file_size;

    platform_read_file_for_editor *read_source_file;
};


struct platform_memory
{
    void *permanent_memory;
    u64 permanent_memory_size;
    u64 permanent_memory_used;

    void *transient_memory;
    u64 transient_memory_size;
    u64 transient_memory_used;
};

struct memory_arena
{
    u8 *base;
    size_t total_size;
    size_t used;

    u32 temp_memory_count;
};

// NOTE(joon): This is where we are rendering at off-screen
struct offscreen_buffer
{
    u32 width;
    u32 height;
    u32 stride;

    u8 *memory;
};

/*
   NOTE(joon) How we will implement backspace
   Instead of actually moving the characters(which includes a ton of copies per each delete), 
   we will replace them with \b (or anything that we can agree with), and only when we output the file,
   we will delete those \b.
*/

struct virtual_key
{
    b32 ended_down;

    u32 state_change_count; // Per each frame
    r32 time_passed;
};

struct input_stream_key
{
    u8 keycode;
    //f32 hold_time;
};

// TODO(joon) repeat count / hold time?
struct platform_input
{
    r32 time_until_repeat;
    r32 dt;

    input_stream_key stream[256];
    u32 stream_read_index;
    u32 stream_write_index;

    union
    {
        struct
        {
            // NOTE(joon) 94 printable inputs, in ASCII order
            virtual_key key_exclamation_mark;
            virtual_key key_double_quote;
            virtual_key key_hash;
            virtual_key key_dollar;
            virtual_key key_percent;
            virtual_key key_and;
            virtual_key key_single_quote;
            virtual_key key_left_parenthesis;
            virtual_key key_right_parenthesis;
            virtual_key key_asterisk;
            virtual_key key_plus;
            virtual_key key_comma;
            virtual_key key_minus;
            virtual_key key_period; // .
            virtual_key key_slash; 

            // numeric
            virtual_key key_0;
            virtual_key key_1;
            virtual_key key_2;
            virtual_key key_3;
            virtual_key key_4;
            virtual_key key_5;
            virtual_key key_6;
            virtual_key key_7;
            virtual_key key_8;
            virtual_key key_9;

            virtual_key key_colon; // :
            virtual_key key_semicolon; // ;
            virtual_key key_less_than; // <
            virtual_key key_equal;
            virtual_key key_greater_than; // >
            virtual_key key_question_mark; // ?
            virtual_key key_at; // @

            // Capital letters
            virtual_key key_A;
            virtual_key key_B;
            virtual_key key_C;
            virtual_key key_D;
            virtual_key key_E;
            virtual_key key_F;
            virtual_key key_G;
            virtual_key key_H;
            virtual_key key_I;
            virtual_key key_J;
            virtual_key key_K;
            virtual_key key_L;
            virtual_key key_M;
            virtual_key key_N;
            virtual_key key_O;
            virtual_key key_P;
            virtual_key key_Q;
            virtual_key key_R;
            virtual_key key_S;
            virtual_key key_T;
            virtual_key key_U;
            virtual_key key_V;
            virtual_key key_W;
            virtual_key key_X;
            virtual_key key_Y;
            virtual_key key_Z;

            virtual_key key_left_bracket; // [
            virtual_key key_backslash; // 
            virtual_key key_right_bracket; // ]
            virtual_key key_caret; // ^
            virtual_key key_underscore; // _
            virtual_key key_grave; // `
 
            // letters
            virtual_key key_a;
            virtual_key key_b;
            virtual_key key_c;
            virtual_key key_d;
            virtual_key key_e;
            virtual_key key_f;
            virtual_key key_g;
            virtual_key key_h;
            virtual_key key_i;
            virtual_key key_j;
            virtual_key key_k;
            virtual_key key_l;
            virtual_key key_m;
            virtual_key key_n;
            virtual_key key_o;
            virtual_key key_p;
            virtual_key key_q;
            virtual_key key_r;
            virtual_key key_s;
            virtual_key key_t;
            virtual_key key_u;
            virtual_key key_v;
            virtual_key key_w;
            virtual_key key_x;
            virtual_key key_y;
            virtual_key key_z;

            virtual_key key_left_brace;
            virtual_key key_vertical; // |
            virtual_key key_right_brace;
            virtual_key key_tilde; // ~

            // NOTE(joon) non-printable inputs
            // arrows
            virtual_key key_right_arrow;
            virtual_key key_left_arrow;
            virtual_key key_up_arrow;
            virtual_key key_down_arrow;

            // functional
            virtual_key key_backspace;
            virtual_key key_escape;
            virtual_key key_space;
            virtual_key key_enter;
        };

        virtual_key virtual_keys[102];
    };
};

#define UPDATE_AND_RENDER(name) void (name)(platform_memory *memory, offscreen_buffer *offscreen_buffer, platform_api *platform_api, platform_input *input)
typedef UPDATE_AND_RENDER(update_and_render_);

// TODO/Joon: intrinsic zero memory?
// TODO(joon): can be faster using wider vectors
internal void
zero_memory(void *memory, u64 size)
{
    // TODO/joon: What if there's no neon support
#if MEKA_ARM
    u8 *byte = (u8 *)memory;
    uint8x16_t zero_128 = vdupq_n_u8(0);

    while(size > 16)
    {
        vst1q_u8(byte, zero_128);

        byte += 16;
        size -= 16;
    }

    if(size > 0)
    {
        while(size--)
        {
            *byte++ = 0;
        }
    }
#else
    // TODO(joon): support for x86/64 simd, too!
    memset (memory, 0, size);
#endif
}

// NOTE(joon): string should be null terminated
internal u32
length_of_string(char *string)
{
    u32 result = 0;
    while(*string++ != 0)
    {
        result++;
    }

    return result;
}

// TODO(joon) lacks any kind of sercurity
internal void
append_string(char *string, u32 max_string_size, char *add)
{
    u32 string_size =  length_of_string(string);
    u32 add_size = length_of_string(add);
    
    char *end = string + string_size;
    for(u32 i = 0;
            i < add_size;
            ++i)
    {
        *end++ = *add++;
    }
}

// NOTE(joon) only works if the source is null terminated & dest has enough space
internal void
copy_string(char *dest, char *source)
{
    u32 source_size = length_of_string(source);

    for(u32 i = 0;
            i < source_size;
            ++i)
    {
        *dest++ = *source++;
    }
}

internal void
copy_memory_and_zero_out_the_rest(u8 *dest, u32 dest_size, u8 *source, u32 copy_size)
{
    assert(dest_size >= copy_size);

    for(u32 index = 0;
            index < copy_size;
            ++index)
    {
        *dest++ = *source++;
    }

    if(dest_size > copy_size)
    {
        zero_memory(dest + copy_size, dest_size - copy_size);
    }
}

internal memory_arena
start_memory_arena(void *base, size_t size, u64 *used, b32 should_be_zero = true)
{
    memory_arena result = {};

    result.base = (u8 *)base + *used;
    result.total_size = size;

    *used += result.total_size;

    if(should_be_zero)
    {
        zero_memory(result.base, result.total_size);
    }


    return result;
}

// NOTE(joon) This should be only used for special cases, such as when we know the exact fallback point
// otherwise, this might lead to memory corruption
internal void
fallback_memory_arena(memory_arena *arena, u64 fallback_index)
{
    arena->used = fallback_index;
}

// NOTE(joon): Works for both platform memory(world arena) & temp memory
#define push_array(memory, type, count) (type *)push_size(memory, count * sizeof(type))
#define push_struct(memory, type) (type *)push_size(memory, sizeof(type))

// TODO(joon) : Alignment might be an issue, always take account of that
internal void *
push_size(memory_arena *memory_arena, size_t size, b32 should_be_zeroed_out = false, size_t alignment = 0)
{
    assert(size != 0);
    assert(memory_arena->temp_memory_count == 0);
    assert(memory_arena->used <= memory_arena->total_size);

    void *result = (u8 *)memory_arena->base + memory_arena->used;
    memory_arena->used += size;

    if(should_be_zeroed_out)
    {
        zero_memory(result, size);
    }

    return result;
}


struct temp_memory
{
    memory_arena *memory_arena;

    // TODO/Joon: temp memory is for arrays only, so dont need to keep track of 'used'?
    void *base;
    size_t total_size;
    size_t used;
};

// TODO(joon) : Alignment might be an issue, always take account of that
internal void *
push_size(temp_memory *temp_memory, size_t size, b32 should_be_zeroed_out = false, size_t alignment = 0)
{
    assert(size != 0);

    void *result = (u8 *)temp_memory->base + temp_memory->used;
    temp_memory->used += size;

    if(should_be_zeroed_out)
    {
        zero_memory(result, size);
    }

    assert(temp_memory->used <= temp_memory->total_size);

    return result;
}

internal temp_memory
start_temp_memory(memory_arena *memory_arena, size_t size, b32 should_be_zero = true)
{
    temp_memory result = {};
    result.base = (u8 *)memory_arena->base + memory_arena->used;
    result.total_size = size;
    result.memory_arena = memory_arena;

    // TODO(joon) might be unsafe
    memory_arena->used += size;

    memory_arena->temp_memory_count++;

    if(should_be_zero)
    {
        zero_memory(result.base, result.total_size);
    }

    return result;
}

internal void
end_temp_memory(temp_memory temp_memory)
{
    memory_arena *memory_arena = temp_memory.memory_arena;

    memory_arena->temp_memory_count--;
    // IMPORTANT(joon) all temp memories should be cleared at once
    memory_arena->used -= temp_memory.total_size;
}


#ifdef __cplusplus
}
#endif

#endif
