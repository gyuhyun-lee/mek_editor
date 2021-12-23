#ifndef MEK_INTRINSIC_H
#define MEK_INTRINSIC_H

inline u32
round_f32_to_u32(f32 value)
{
    u32 result = (u32)(value + 0.5f);

    return result;
}

#endif
