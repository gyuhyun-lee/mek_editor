#include <stdio.h>
// This is a very long string to test cursor movement. This is a very long string to test cursor movement.aaaaaaa

int main(void)
{
    printf("Hello World!\n");
    return 0;
}

// TODO(joon): we will eventually replace stb with our own glyph loader :)
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "mek_types.h"
#include "mek_platform.h"
#include "mek_intrinsic.h"

#include "mek_editor.h"

internal u8 *
get_preivous_one_past_newline(u8 *current, u8 *start)
{
    // NOTE(joon) no bound checking here, bound checking should be done externally
    u8 *result = current;
    while(result != start)
    {
        if(*result == '\r' || *result == '\n')
        {
            result++;
            break;
        }
        result--;
    }

    return result;
}

internal u8 *
move_to_next_one_past_newline(u8 *start, u8 *end)
{
    // NOTE(joon) no bound checking here, bound checking should be done externally
    u8 *result = start;
    while(result != end)
    {
        if(*result == '\r' && *result == '\n')
        {
            result += 2;
            break;
        }

        if(*result == '\r' || *result == 'n') 
        {
            result++;
            break;
        }
    }

    return start;
}
