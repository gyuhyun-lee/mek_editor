#include <metal_stdlib>

using namespace metal;

typedef float float32x4_t __attribute__((ext_vector_type(4)));

struct v2
{
    float x;
    float y;
};

struct vertex_output
{
    // @NOTE/Joon: [[position]] -> indicator that this is clip space position
    // This is mandatory to be inside the vertex output struct
    float32x4_t clip_position [[position]];

    v2 tex_coord;
};


struct temp_vertex
{
    v2 p;
    v2 tex_coord;
};

// NOTE(joon) : vertex is a prefix for vertex shader
vertex vertex_output
vertex_function(uint vertex_ID [[vertex_id]],
             constant temp_vertex *vertices [[buffer(0)]])
{
    vertex_output output = {};

    output.clip_position.x = vertices[vertex_ID].p.x;
    output.clip_position.y = vertices[vertex_ID].p.y;
    output.clip_position.z = 0;
    output.clip_position.w = 1;

    output.tex_coord = vertices[vertex_ID].tex_coord;

    return output;
}

fragment float32x4_t 
frag_function(vertex_output vertex_output [[stage_in]], 
            texture2d<float, access::sample> colorTexture [[texture(0)]])
{
    // TODO(joon) pass the pre made sampler?
    constexpr sampler texture_sampler(mag_filter::linear, min_filter::linear);
    float32x4_t out_color = colorTexture.sample(texture_sampler, {vertex_output.tex_coord.x, vertex_output.tex_coord.y});
    //float32x4_t out_color = {48/255.0f, 10/255.0f, 36/255.0f, 1};

    return out_color;
}


