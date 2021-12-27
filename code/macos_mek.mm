#include <Cocoa/Cocoa.h> // APPKIT
#include <CoreGraphics/CoreGraphics.h> 
#include <metal/metal.h>
#include <metalkit/metalkit.h>
#include <mach/mach_time.h> // mach_absolute_time
#include <stdio.h> // printf for debugging purpose
#include <sys/stat.h>
#include <Carbon/Carbon.h>
#include <arm_neon.h>
#include <dlfcn.h> // dlsym

#include "mek_editor.cpp"

global b32 is_running;

internal u64 
mach_time_diff_in_nano_seconds(u64 begin, u64 end, r32 nano_seconds_per_tick)
{
    return (u64)(((end - begin)*nano_seconds_per_tick));
}

internal 
PLATFORM_GET_FILE_SIZE(macos_get_file_size) 
{
    u64 result = 0;

    int file = open(filename, O_RDONLY);
    struct stat file_stat;
    fstat(file , &file_stat); 
    result = file_stat.st_size;
    close(file);

    return result;
}

PLATFORM_READ_FILE(debug_macos_read_file)
{
    platform_read_file_result Result = {};

    int File = open(filename, O_RDONLY);
    int Error = errno;
    if(File >= 0) // NOTE : If the open() succeded, the return value is non-negative value.
    {
        struct stat file_stat;
        fstat(File , &file_stat); 
        off_t fileSize = file_stat.st_size;

        if(fileSize > 0)
        {
            // TODO/Joon : no more os level memory allocation?
            Result.size = fileSize;
            Result.memory = (u8 *)malloc(Result.size);
            if(read(File, Result.memory, fileSize) == -1)
            {
                free(Result.memory);
                Result.size = 0;
            }
        }

        close(File);
    }

    return Result;
}

PLATFORM_WRITE_ENTIRE_FILE(debug_macos_write_entire_file)
{
    int file = open(file_name, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);

    if(file >= 0) 
    {
        if(write(file, memory_to_write, size) == -1)
        {
            // TODO(joon) : LOG here
        }

        close(file);
    }
    else
    {
        // TODO(joon) :LOG
        printf("Failed to create file\n");
    }
}

PLATFORM_FREE_FILE_MEMORY(debug_macos_free_file_memory)
{
    free(memory);
}

@interface 
app_delegate : NSObject<NSApplicationDelegate>
@end
@implementation app_delegate
- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    [NSApp stop:nil];

    // Post empty event: without it we can't put application to front
    // for some reason (I get this technique from GLFW source).
    NSAutoreleasePool* pool = [NSAutoreleasePool new];
    NSEvent* event =
        [NSEvent otherEventWithType: NSApplicationDefined
                 location: NSMakePoint(0, 0)
                 modifierFlags: 0
                 timestamp: 0
                 windowNumber: 0
                 context: nil
                 subtype: 0
                 data1: 0
                 data2: 0];
    [NSApp postEvent: event atStart: YES];
    [pool drain];
}

@end

#define check_ns_error(error)\
{\
    if(error)\
    {\
        printf("check_metal_error failed inside the domain: %s code: %ld\n", [error.domain UTF8String], (long)error.code);\
        assert(0);\
    }\
}\

internal void
macos_get_file_time(char *file_name)
{
    int file = open(file_name, O_RDONLY);

    struct stat file_stat;
    fstat(file , &file_stat); 

    close(file);
}

struct macos_code
{
    void *library;
    update_and_render_ *update_and_render;
};

internal void
macos_get_code(macos_code *code, char *file_name)
{
    if(code->library)
    {
        dlclose(code->library);
    }

    code->library = dlopen(file_name, RTLD_LAZY);
    if(code->library)
    {
        code->update_and_render = (update_and_render_ *)dlsym(code->library, "update_and_render");
    }
}


internal void
macos_handle_event(NSApplication *app, NSWindow *window, platform_input *input)
{
#if 0
    NSPoint mouse_location = [NSEvent mouseLocation];
    NSRect frame_rect = [window frame];
    NSRect content_rect = [window contentLayoutRect];

    v2 bottom_left_p = {};
    bottom_left_p.x = frame_rect.origin.x;
    bottom_left_p.y = frame_rect.origin.y;

    v2 content_rect_dim = {}; 
    content_rect_dim.x = content_rect.size.width; 
    content_rect_dim.y = content_rect.size.height;

    v2 rel_mouse_location = {};
    rel_mouse_location.x = mouse_location.x - bottom_left_p.x;
    rel_mouse_location.y = mouse_location.y - bottom_left_p.y;

    r32 mouse_speed_when_clipped = 0.08f;
    if(rel_mouse_location.x >= 0.0f && rel_mouse_location.x < content_rect_dim.x)
    {
        mouse_diff.x = mouse_location.x - last_mouse_p.x;
    }
    else if(rel_mouse_location.x < 0.0f)
    {
        mouse_diff.x = -mouse_speed_when_clipped;
    }
    else
    {
        mouse_diff.x = mouse_speed_when_clipped;
    }

    if(rel_mouse_location.y >= 0.0f && rel_mouse_location.y < content_rect_dim.y)
    {
        mouse_diff.y = mouse_location.y - last_mouse_p.y;
    }
    else if(rel_mouse_location.y < 0.0f)
    {
        mouse_diff.y = -mouse_speed_when_clipped;
    }
    else
    {
        mouse_diff.y = mouse_speed_when_clipped;
    }

    // NOTE(joon) : MacOS screen coordinate is bottom-up, so just for the convenience, make y to be bottom-up
    mouse_diff.y *= -1.0f;

    last_mouse_p.x = mouse_location.x;
    last_mouse_p.y = mouse_location.y;

    //printf("%f, %f\n", mouse_diff.x, mouse_diff.y);
#endif

    // TODO : Check if this loop has memory leak.
    while(1)
    {
        NSEvent *event = [app nextEventMatchingMask:NSAnyEventMask
                         untilDate:nil
                            inMode:NSDefaultRunLoopMode
                           dequeue:YES];
        if(event)
        {
            switch([event type])
            {
                case NSEventTypeKeyUp:
                case NSEventTypeKeyDown:
                {
                    b32 was_down = event.ARepeat;
                    b32 is_down = ([event type] == NSEventTypeKeyDown);

                    if((is_down != was_down) || !is_down)
                    {
                        //printf("isDown : %d, WasDown : %d", is_down, was_down);
                        u16 key_code = [event keyCode];
                        if(key_code == kVK_Escape)
                        {
                            is_running = false;
                        }
                        if(key_code == kVK_Delete)
                        {
                            input->is_backspace_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_A)
                        {
                            input->is_a_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_B)
                        {
                            input->is_b_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_C)
                        {
                            input->is_c_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_D)
                        {
                            input->is_d_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_E)
                        {
                            input->is_e_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_F)
                        {
                            input->is_f_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_G)
                        {
                            input->is_g_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_H)
                        {
                            input->is_h_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_I)
                        {
                            input->is_i_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_J)
                        {
                            input->is_j_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_K)
                        {
                            input->is_k_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_L)
                        {
                            input->is_l_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_M)
                        {
                            input->is_m_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_N)
                        {
                            input->is_n_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_O)
                        {
                            input->is_o_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_P)
                        {
                            input->is_p_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_Q)
                        {
                            input->is_q_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_R)
                        {
                            input->is_r_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_S)
                        {
                            input->is_s_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_T)
                        {
                            input->is_t_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_U)
                        {
                            input->is_u_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_V)
                        {
                            input->is_v_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_W)
                        {
                            input->is_w_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_X)
                        {
                            input->is_w_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_Y)
                        {
                            input->is_y_down = is_down;
                        }
                        else if(key_code == kVK_ANSI_Z)
                        {
                            input->is_z_down = is_down;
                        }

                        else if(key_code == kVK_LeftArrow)
                        {
                            input->is_arrow_left_down = is_down;
                        }
                        else if(key_code == kVK_RightArrow)
                        {
                            input->is_arrow_right_down = is_down;
                        }
                        else if(key_code == kVK_UpArrow)
                        {
                            input->is_arrow_up_down = is_down;
                        }
                        else if(key_code == kVK_DownArrow)
                        {
                            input->is_arrow_down_down = is_down;
                        }
                        else if(key_code == kVK_Return)
                        {
                            if(is_down)
                            {
                                NSWindow *window = [event window];
                                // TODO : proper buffer resize here!
                                [window toggleFullScreen:0];
                            }
                        }
                    }
                }break;

                default:
                {
                    [app sendEvent : event];
                }
            }
        }
        else
        {
            break;
        }
    }
} 

#pragma pack(push, 1)
struct bmp_file_header
{
    u16 file_header;
    u32 file_size;
    u16 reserved_1;
    u16 reserved_2;
    u32 pixel_offset;

    u32 header_size;
    u32 width;
    u32 height;
    u16 color_plane_count;
    u16 bits_per_pixel;
    u32 compression;

    u32 image_size;
    u32 pixels_in_meter_x;
    u32 pixels_in_meter_y;
    u32 colors;
    u32 important_color_count;
    u32 red_mask;
    u32 green_mask;
    u32 blue_mask;
    u32 alpha_mask;
};
#pragma pack(pop)

internal 
PLATFORM_READ_FILE_WITH_EXTRA_MEMORY(debug_macos_read_file_with_extra_memory)
{
    platform_read_file_with_extra_memory_result result = {};

    // TODO(joon) Research more about the options for more smoooooth experience
    int file = open(file_name, O_RDWR, S_IRWXU);
    int error = errno;
    if(file >= 0) // NOTE(joon) If the open() succeded, the return value is non-negative value.
    {
        struct stat file_stat;
        fstat(file , &file_stat); 
        off_t file_size = file_stat.st_size;

        if(file_size > 0)
        {
            // TODO/Joon : no more os level memory allocation?
            // TODO(joon): how much extra bytes? What should happen if the file size is really big?
            result.size = 2.0f*file_size;
            result.memory = (u8 *)malloc(result.size);
            zero_memory(result.memory, result.size);

            if(read(file, result.memory, file_size) == -1)
            {
                free(result.memory);
                result.size = 0;
            }
        }

        close(file);
    }

    return result;
}

internal void
macos_resize_window(offscreen_buffer *offscreen_buffer, u32 width, u32 height)
{
    if(offscreen_buffer->memory)
    {
        free(offscreen_buffer->memory);
    }

    offscreen_buffer->width = width;
    offscreen_buffer->height = height;
    offscreen_buffer->stride = sizeof(u32)*offscreen_buffer->width;

    offscreen_buffer->memory =  (u8 *)malloc(offscreen_buffer->height * offscreen_buffer->stride);
}
    
int 
main(void)
{ 
    platform_api platform_api = {};
    platform_api.read_file = debug_macos_read_file;
    platform_api.write_entire_file = debug_macos_write_entire_file;
    platform_api.read_file_with_extra_memory = debug_macos_read_file_with_extra_memory;
    srand((u32)time(NULL));

    struct mach_timebase_info mach_time_info;
    mach_timebase_info(&mach_time_info);
    r32 nano_seconds_per_tick = ((r32)mach_time_info.numer/(r32)mach_time_info.denom);

	i32 window_width = 1920;
	i32 window_height = 1080;
    r32 window_width_over_height = (r32)window_width/(r32)window_height;

    offscreen_buffer offscreen_buffer = {};
    // TODO(joon) Use content rect size instead of the window size?
    macos_resize_window(&offscreen_buffer, window_width, window_height);

    platform_memory platform_memory = {};

    platform_memory.permanent_memory_size = gigabytes(1);
    platform_memory.transient_memory_size = gigabytes(3);
    u64 total_size = platform_memory.permanent_memory_size + platform_memory.transient_memory_size;
    // NOTE(joon) automatically zeroed out
    vm_allocate(mach_task_self(), 
                (vm_address_t *)&platform_memory.permanent_memory,
                total_size, 
                VM_FLAGS_ANYWHERE);

    u32 target_frames_per_second = 30;
    r32 target_seconds_per_frame = 1.0f/(r32)target_frames_per_second;
    u32 target_nano_seconds_per_frame = (u32)(target_seconds_per_frame*sec_to_nano_sec);

    platform_memory.transient_memory = (u8 *)platform_memory.permanent_memory + platform_memory.permanent_memory_size;

    //macos_code code = {};
    //macos_get_code(code, "/Volumes/meka/mek_editor/data/test.bmp");


#if 0
    u32 result_bitmap_size = sizeof(bmp_file_header) + 
                                    sizeof(u32) * font_bitmap_width * font_bitmap_height;
    u8 *result_bitmap = (u8 *)malloc(result_bitmap_size);

    bmp_file_header *bmp_header = (bmp_file_header *)result_bitmap;  
    bmp_header->file_header = 19778;
    bmp_header->file_size = result_bitmap_size;
    bmp_header->pixel_offset = sizeof(bmp_file_header);

    bmp_header->header_size = sizeof(bmp_file_header) - 14;
    bmp_header->width = font_bitmap_width;
    bmp_header->height = font_bitmap_height;
    bmp_header->color_plane_count = 1;
    bmp_header->bits_per_pixel = 32;
    bmp_header->compression = 3;

    bmp_header->image_size = result_bitmap_size - sizeof(bmp_file_header);
    bmp_header->pixels_in_meter_x = 11811;
    bmp_header->pixels_in_meter_y = 11811;
    bmp_header->red_mask = 0x00ff0000;
    bmp_header->green_mask = 0x0000ff00;
    bmp_header->blue_mask = 0x000000ff;
    bmp_header->alpha_mask = 0xff000000;

    u8 *row = result_bitmap + sizeof(bmp_file_header);
    u32 stride = sizeof(u32) * font_bitmap_width;
    for(u32 y = 0;
            y < font_bitmap_height;
            ++y)
    {
        u32 *pixel = (u32 *)row;
        for(u32 x = 0;
                x < font_bitmap_width;
                ++x)
        {
            if(font_bitmap[y * font_bitmap_width + x])
            {
                *pixel = 0xffff0000;
            }
            pixel++;
        }

        row += stride;
    }

    debug_macos_write_entire_file("/Volumes/meka/mek_editor/data/test.bmp", (void *)result_bitmap, result_bitmap_size);
#endif

    NSApplication *app = [NSApplication sharedApplication];
    [app setActivationPolicy :NSApplicationActivationPolicyRegular];
    app_delegate *delegate = [app_delegate new];
    [app setDelegate: delegate];

    NSMenu *app_main_menu = [NSMenu alloc];
    NSMenuItem *menu_item_with_item_name = [NSMenuItem new];
    [app_main_menu addItem : menu_item_with_item_name];
    [NSApp setMainMenu:app_main_menu];

    NSMenu *SubMenuOfMenuItemWithAppName = [NSMenu alloc];
    NSMenuItem *quitMenuItem = [[NSMenuItem alloc] initWithTitle:@"Quit" 
                                                    action:@selector(terminate:)  // Decides what will happen when the menu is clicked or selected
                                                    keyEquivalent:@"q"];
    [SubMenuOfMenuItemWithAppName addItem:quitMenuItem];
    [menu_item_with_item_name setSubmenu:SubMenuOfMenuItemWithAppName];

    // TODO(joon): when connected to the external display, this should be window_width and window_height
    // but if not, this should be window_width/2 and window_height/2. Why?
    NSRect window_rect = NSMakeRect(100.0f, 100.0f, (r32)window_width, (r32)window_height);
    //NSRect window_rect = NSMakeRect(100.0f, 100.0f, (r32)window_width/2.0f, (r32)window_height/2.0f);

    NSWindow *window = [[NSWindow alloc] initWithContentRect : window_rect
                                        // Apple window styles : https://developer.apple.com/documentation/appkit/nswindow/stylemask
                                        styleMask : NSTitledWindowMask|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable
                                        backing : NSBackingStoreBuffered
                                        defer : NO];

    NSString *app_name = [[NSProcessInfo processInfo] processName];
    [window setTitle:app_name];
    [window makeKeyAndOrderFront:0];
    [window makeKeyWindow];
    [window makeMainWindow];

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    MTKView *view = [[MTKView alloc] initWithFrame : window_rect
                                    device:device];
    [window setContentView:view];

    NSError *error;
    // TODO(joon) : Put the metallib file inside the app
    NSString *file_path = @"/Volumes/meka/mek_editor/code/shader/shader.metallib";
    id<MTLLibrary> shader_library = [device newLibraryWithFile:(NSString *)file_path 
                                            error: &error];
    check_ns_error(error);

    id<MTLFunction> simple_vertex_function = [shader_library newFunctionWithName:@"vertex_function"];
    id<MTLFunction> simple_frag_function = [shader_library newFunctionWithName:@"frag_function"];
    MTLRenderPipelineDescriptor *simple_pipeline_descriptor = [MTLRenderPipelineDescriptor new];
    simple_pipeline_descriptor.label = @"simple_pipeline";
    simple_pipeline_descriptor.vertexFunction = simple_vertex_function;
    simple_pipeline_descriptor.fragmentFunction = simple_frag_function;
    simple_pipeline_descriptor.sampleCount = 1;
    simple_pipeline_descriptor.rasterSampleCount = simple_pipeline_descriptor.sampleCount;
    simple_pipeline_descriptor.rasterizationEnabled = true;
    simple_pipeline_descriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
    simple_pipeline_descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    simple_pipeline_descriptor.colorAttachments[0].writeMask = MTLColorWriteMaskAll;

    id<MTLRenderPipelineState> simple_pipeline_state = [device newRenderPipelineStateWithDescriptor:simple_pipeline_descriptor
                                                                 error:&error];
    id<MTLCommandQueue> command_queue = [device newCommandQueue];

    //TODO(joon) Use this to output offscreen buffer
    MTLTextureDescriptor *texture_desc = [[MTLTextureDescriptor alloc] init];
    texture_desc.pixelFormat = MTLPixelFormatBGRA8Unorm;
    texture_desc.width = offscreen_buffer.width;
    texture_desc.height = offscreen_buffer.height;

    id<MTLTexture> output_texture = [device newTextureWithDescriptor:texture_desc];

#if 0
    CVDisplayLinkRef display_link;
    if(CVDisplayLinkCreateWithActiveCGDisplays(&display_link)== kCVReturnSuccess)
    {
        CVDisplayLinkSetOutputCallback(display_link, display_link_callback, 0); 
        CVDisplayLinkStart(display_link);
    }
#endif

    [app activateIgnoringOtherApps:YES];
    [app run];

    editor_state editor_state = {};

    u64 last_time = mach_absolute_time();
    is_running = true;

    // TODO(joon) works for now, but might need more robust input system
    platform_input input = {};
    while(is_running)
    {
        macos_handle_event(app, window, &input);

        //if(code->file_name)
        update_and_render(&platform_memory, &offscreen_buffer, &platform_api, &input);

        // NOTE(joon) update the output buffer with offscreen buffer
        MTLRegion region = 
        {
            { 0, 0, 0 },
            {offscreen_buffer.width, offscreen_buffer.height, 1}
        };
        [output_texture replaceRegion:region
                        mipmapLevel:0
                        withBytes:offscreen_buffer.memory
                        bytesPerRow:offscreen_buffer.stride];

        @autoreleasepool
        {
            id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];

            MTLViewport viewport = {};
            viewport.originX = 0;
            viewport.originY = 0;
            viewport.width = (r32)window_width;
            viewport.height = (r32)window_height;
            viewport.znear = 0.0;
            viewport.zfar = 1.0;

            // NOTE(joon): renderpass descriptor is already configured for this frame, obtain it as late as possible
            MTLRenderPassDescriptor *this_frame_descriptor = view.currentRenderPassDescriptor;
            this_frame_descriptor.colorAttachments[0].clearColor = {0, 1, 1, 1};
            this_frame_descriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
            this_frame_descriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

            assert(view.currentDrawable);

            if(this_frame_descriptor)
            {
                id<MTLRenderCommandEncoder> render_encoder = [command_buffer renderCommandEncoderWithDescriptor : this_frame_descriptor];
                render_encoder.label = @"render_encoder";
                [render_encoder setViewport:viewport];
                [render_encoder setRenderPipelineState:simple_pipeline_state];

                [render_encoder setTriangleFillMode: MTLTriangleFillModeFill];
                [render_encoder setCullMode: MTLCullModeNone];

                struct v2
                {
                    f32 x;
                    f32 y;
                };

                struct vertex
                {
                    v2 p;
                    v2 tex_coord;
                };

#if 1
                vertex quad_vertices[] = 
                {
                    {{-1.0f, -1.0f}, {0.0f, 1.0f}},
                    {{1.0f, -1.0f}, {1.0f, 1.0f}},
                    {{1.0f, 1.0f}, {1.0f, 0.0f}},

                    {{-1.0f, -1.0f}, {0.0f, 1.0f}},
                    {{1.0f, 1.0f}, {1.0f, 0.0f}},
                    {{-1.0f, 1.0f}, {0.0f, 0.0f}},
                };
#else
                v2 quad_vertices[] = 
                {
                    {0.0f, 0.0f}, 
                    {100.0f, 0.0f}, 
                    {100.0f, 100.0f}
                };
#endif

                [render_encoder setVertexBytes: quad_vertices
                                length: sizeof(quad_vertices[0]) * array_count(quad_vertices) 
                                atIndex: 0]; 

                [render_encoder setFragmentTexture:output_texture
                                atIndex:0];

                [render_encoder drawPrimitives:MTLPrimitiveTypeTriangle
                                vertexStart:0 
                                vertexCount:array_count(quad_vertices)];

                [render_encoder endEncoding];
            
#if 0
                u64 time_before_presenting = mach_absolute_time();
                u64 time_passed = mach_time_diff_in_nano_seconds(last_time, time_before_presenting, nano_seconds_per_tick);
                u64 time_left_until_present_in_nano_seconds = target_nano_seconds_per_frame - time_passed;
                                                            
                double time_left_until_present_in_sec = time_left_until_present_in_nano_seconds/(double)sec_to_nano_sec;
#endif

                // NOTE(joon): This will work find, as long as we match our display refresh rate with our game
                [command_buffer presentDrawable: view.currentDrawable];
            }

            // NOTE : equivalent to vkQueueSubmit,
            // TODO(joon): Sync with the swap buffer!
            [command_buffer commit];
        }

        u64 time_passed_in_nano_seconds = mach_time_diff_in_nano_seconds(last_time, mach_absolute_time(), nano_seconds_per_tick);

        // NOTE(joon): Because nanosleep is such a high resolution sleep method, for precise timing,
        // we need to undersleep and spend time in a loop for the precise page flip
        u64 undersleep_nano_seconds = 1000000;
        if(time_passed_in_nano_seconds < target_nano_seconds_per_frame)
        {
            timespec time_spec = {};
            time_spec.tv_nsec = target_nano_seconds_per_frame - time_passed_in_nano_seconds -  undersleep_nano_seconds;

            nanosleep(&time_spec, 0);
        }
        else
        {
            // TODO : Missed Frame!
            // TODO(joon) : Whenever we miss the frame re-sync with the display link
        }

        // For a short period of time, loop
        time_passed_in_nano_seconds = mach_time_diff_in_nano_seconds(last_time, mach_absolute_time(), nano_seconds_per_tick);
        while(time_passed_in_nano_seconds < target_nano_seconds_per_frame)
        {
            time_passed_in_nano_seconds = mach_time_diff_in_nano_seconds(last_time, mach_absolute_time(), nano_seconds_per_tick);
        }

        // update the time stamp
        last_time = mach_absolute_time();
    }

    return 0;
}

