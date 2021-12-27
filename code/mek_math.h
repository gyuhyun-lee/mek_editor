#ifndef MEK_MATH_H
#define MEK_MATH_H


internal u32
clamp(u32 min, u32 value, u32 max)
{
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


#endif
