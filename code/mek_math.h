#ifndef MEK_MATH_H
#define MEK_MATH_H

internal u32
clamp(u32 min, u32 value, u32 max)
{
    assert(min <= max);

    u32 result = value;

    if(result < min)
    {
        result = min;
    }
    else if(result > max)
    {
        result = max;
    }

    return result;
}

internal u32
clamp_exclude_max(u32 min, u32 value, u32 max)
{
    assert(min <= max);

    u32 result = value;

    if(result < min)
    {
        result = min;
    }
    else if(result > max-1)
    {
        result = max-1;
    }

    return result;
}

internal r32
lerp(r32 min, r32 t, r32 max)
{
    r32 result = (max - min) * t + min;

    return result;
}

#endif
