#ifndef MEK_PLATFORM_H
#define MEK_PLATFORM_H

#undef internal
#undef assert

#define assert(Expression) if(!(Expression)) {int *a = 0; *a = 0;}
#define array_count(Array) (sizeof(Array) / sizeof(Array[0]))

#define global static
#define local_persist static
#define internal static

#define kilobytes(value) value*1024LL
#define megabytes(value) 1024LL*kilobytes(value)
#define gigabytes(value) 1024LL*megabytes(value)
#define terabytes(value) 1024LL*gigabytes(value)

#define sec_to_nano_sec 1.0e+9f
#define sec_to_micro_sec 1000.0f

struct platform_read_file_result
{
    u8 *memory;
    u64 size; // TOOD/joon : make this to be at least 64bit
};

struct platform_read_file_with_extra_memory_result
{
    // TODO(joon) This needs to be adjusted, if the user writes more than the buffer size
    u32 size; // size that we allocate
    u8 *memory;
};

#define PLATFORM_GET_FILE_SIZE(name) u64 (name)(char *filename)
typedef PLATFORM_GET_FILE_SIZE(platform_get_file_size);

#define PLATFORM_READ_FILE(name) platform_read_file_result (name)(char *filename)
typedef PLATFORM_READ_FILE(platform_read_file);

#define PLATFORM_READ_FILE_WITH_EXTRA_MEMORY(name) platform_read_file_with_extra_memory_result (name)(char *file_name)
typedef PLATFORM_READ_FILE_WITH_EXTRA_MEMORY(platform_read_file_with_extra_memory);

#define PLATFORM_WRITE_ENTIRE_FILE(name) void (name)(char *file_name, void *memory_to_write, u32 size)
typedef PLATFORM_WRITE_ENTIRE_FILE(platform_write_entire_file);

#define PLATFORM_FREE_FILE_MEMORY(name) void (name)(void *memory)
typedef PLATFORM_FREE_FILE_MEMORY(platform_free_file_memory);

struct platform_api
{
    platform_read_file *read_file;
    platform_write_entire_file *write_entire_file;
    platform_free_file_memory *free_file_memory;

    platform_read_file_with_extra_memory *read_file_with_extra_memory;
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
    void *base;
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
   NOTE(joon)
   How we wil l
*/

struct platform_input
{
    b32 is_a_down;
    b32 is_b_down;
    b32 is_c_down;
    b32 is_d_down;
    b32 is_e_down;
    b32 is_f_down;
    b32 is_g_down;
    b32 is_h_down;
    b32 is_i_down;
    b32 is_j_down;
    b32 is_k_down;
    b32 is_l_down;
    b32 is_m_down;
    b32 is_n_down;
    b32 is_o_down;
    b32 is_p_down;
    b32 is_q_down;
    b32 is_r_down;
    b32 is_s_down;
    b32 is_t_down;
    b32 is_u_down;
    b32 is_v_down;
    b32 is_w_down;
    b32 is_x_down;
    b32 is_y_down;
    b32 is_z_down;

    b32 is_arrow_right_down;
    b32 is_arrow_left_down;
    b32 is_arrow_up_down;
    b32 is_arrow_down_down;


    b32 is_backspace_down;
};

#define UPDATE_AND_RENDER(name) void (name)(platform_memory *memory, offscreen_buffer *offscreen_buffer, platform_api *platform_api, platform_input *input)
typedef UPDATE_AND_RENDER(update_and_render_);

// TODO/Joon: intrinsic zero memory?
// TODO(joon): can be faster using wider vectors
inline void
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
    // TODO(joon): support for intel simd, too!
    memset (memory, 0, size);
#endif
}


internal memory_arena
start_memory_arena(void *base, size_t size, u64 *used, b32 should_be_zero = true)
{
    memory_arena result = {};

    result.base = (u8 *)base + *used;
    result.total_size = size;

    *used += result.total_size;

    // TODO/joon :zeroing memory every time might not be a best idea
    if(should_be_zero)
    {
        zero_memory(result.base, result.total_size);
    }

    return result;
}

// NOTE(joon): Works for both platform memory(world arena) & temp memory
#define push_array(memory, type, count) (type *)push_size(memory, count * sizeof(type))
#define push_struct(memory, type) (type *)push_size(memory, sizeof(type))

// TODO(joon) : Alignment might be an issue, always take account of that
internal void *
push_size(memory_arena *memory_arena, size_t size, size_t alignment = 0)
{
    assert(size != 0);
    assert(memory_arena->temp_memory_count == 0);
    assert(memory_arena->used < memory_arena->total_size);

    void *result = (u8 *)memory_arena->base + memory_arena->used;
    memory_arena->used += size;

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
push_size(temp_memory *temp_memory, size_t size, size_t alignment = 0)
{
    assert(size != 0);

    void *result = (u8 *)temp_memory->base + temp_memory->used;
    temp_memory->used += size;

    assert(temp_memory->used < temp_memory->total_size);

    return result;
}

internal temp_memory
start_temp_memory(memory_arena *memory_arena, size_t size, b32 should_be_zero = true)
{
    temp_memory result = {};
    result.base = (u8 *)memory_arena->base + memory_arena->used;
    result.total_size = size;
    result.memory_arena = memory_arena;

    push_size(memory_arena, size);

    memory_arena->temp_memory_count++;

    if(should_be_zero)
    {
        zero_memory(result.base, result.total_size);
    }

    return result;
}

internal void
end_temp_memory(temp_memory *temp_memory)
{
    memory_arena *memory_arena = temp_memory->memory_arena;
    // NOTE(joon) : safe guard for using this temp memory after ending it 
    temp_memory->base = 0;

    memory_arena->temp_memory_count--;
    // IMPORTANT(joon) : As the nature of this, all temp memories should be cleared at once
    memory_arena->used -= temp_memory->total_size;
}


#endif
