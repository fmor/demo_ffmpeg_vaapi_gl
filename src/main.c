#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>

#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include <va/va.h>
#include <va/va_drm.h>

#include <GL/eglew.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

#include "config.h"
#include "player.h"
#include "util.h"
#include "data.h"
#include "draw.h"
#include "stb.h"
#include "render.h"

#include <portaudio.h>

#include "resources.h"


static Context ctx;

static void cb_glfw_error(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
    assert(  0  );
}

static void cb_glfw_cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    static float yaw = -90;
    static float pitch = 0;
    float x_rel;
    float y_rel;

    Context* ctx;
    ctx = (Context*) glfwGetWindowUserPointer( window );
    static int pass_first = 1;
 //   fprintf( stdout, "Mouse : [%f,%f]\n", xpos, ypos );
    if( pass_first )
    {
        pass_first = 0;
        return;
    }




    x_rel = xpos - ctx->mouse_previous[0];
    y_rel = ypos - ctx->mouse_previous[1];

    ctx->mouse_previous[0] = xpos;
    ctx->mouse_previous[1] = ypos;



    float velocity_angular = ctx->camera_velocity_angular * ctx->delta_time;
    x_rel *= velocity_angular;
    y_rel *= velocity_angular;


    yaw += x_rel;
    pitch += y_rel;
    pitch = glm_clamp( pitch, -89.f, 89.f );

    float pitch_rad = ctx->camera_flip_pitch_direction ? -glm_rad(pitch) :  glm_rad(pitch);
    float yaw_rad =  glm_rad(yaw);

 //   printf( "delta=%f rel=(%f,%f) yaw=%f pitch=%f\n", ctx->delta_time, x_rel, y_rel, yaw, pitch );
 //   fflush( stdout );

    ctx->camera_direction[0] = cosf(yaw_rad) * cosf(pitch_rad);
    ctx->camera_direction[1] = sinf(pitch_rad);
    ctx->camera_direction[2] = sinf(yaw_rad) * cosf(pitch_rad);
    glm_normalize( ctx->camera_direction );



}
static void cb_glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Context* ctx;
    ctx = (Context*) glfwGetWindowUserPointer( window );

    switch( action )
    {
        case GLFW_PRESS:
            switch( key )
            {
                case GLFW_KEY_UP:
                case GLFW_KEY_DOWN:
                case GLFW_KEY_LEFT:
                case GLFW_KEY_RIGHT:
                    ctx->camera_translation_active = key;
                break;

                case GLFW_KEY_ESCAPE:
                {
                    int r;
                    r = glfwGetInputMode( ctx->glfw_window, GLFW_CURSOR );
                    if( r == GLFW_CURSOR_DISABLED )
                    {
                        glfwSetInputMode( ctx->glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                        ctx->camera_mode = CAMERA_MODE_FREE;
                    }
                    else
                    {
                        glfwSetInputMode( ctx->glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED );
                        ctx->camera_mode = CAMERA_MODE_NORMAL;
                        ctx->camera_position[1] = 1.0f;
                        ctx->camera_position[0] = glm_clamp( ctx->camera_position[0], 1.5f, ROOM_X - 1.5f );
                        ctx->camera_position[2] = glm_clamp( ctx->camera_position[2], 1.5f, ROOM_Z -1.5f );
                    }
                }
                break;

                case GLFW_KEY_R:
                    if( ctx->recorder )
                        recorder_free( &ctx->recorder );
                    else
                        ctx->recorder = recorder_create( ctx->video_va_display, ctx->hw_device_context, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, FPS_RECORD, RECORD_FILENAME  );
                break;

                case GLFW_KEY_A:
                case GLFW_KEY_Q:
                    ctx->flag_pending_quit = 1;
                break;
            }
        break;

        case GLFW_RELEASE:
            switch( key )
            {
                case GLFW_KEY_DOWN:
                case GLFW_KEY_UP:
                case GLFW_KEY_LEFT:
                case GLFW_KEY_RIGHT:
                    ctx->camera_translation_active = 0;
                break;
            }
        break;
    }
}

static void compute_volume( Context* ctx )
{
    float len;
    for( int i = 0; i < ctx->videos_count; ++i )
    {
        vec3 l;
        Video* video;
        video = &ctx->videos[i];
        if( video->player == NULL )
            continue;
        l[0] = ctx->camera_position[0] - video->transform[3][0];
        l[1] = ctx->camera_position[1] - video->transform[3][1];
        l[2] = ctx->camera_position[2] - video->transform[3][2];
        len = sqrtf( l[0]*l[0] + l[1]*l[1] + l[2]*l[2] );
        video->player->audio_volume = glm_clamp( 1. - len/10.f, 0.f, 1.0) ;
    }
}

int init()
{
    int r;
    char buffer[256];
    int va_version[2];
    VAStatus st;

    memset( &ctx, 0, sizeof(Context) );
    ctx.egl_context = EGL_NO_CONTEXT;

    for( unsigned i = 0; i < 10; ++i )
    {
        sprintf( buffer, "/dev/dri/renderD%d", 128 + i );
        ctx.fd = open( buffer, O_RDWR );
        if( ctx.fd == -1 )
        {
            fprintf( stderr, "Failed to open device %s", buffer );
            return -1;
        }
        ctx.video_va_display = vaGetDisplayDRM( ctx.fd );
        if( ctx.video_va_display != NULL )
            break;
        close( ctx.fd );
    }

    if( ctx.video_va_display == NULL )
    {
        fprintf( stderr, "Failed to find a vaapi device\n" );
        return -1;
    }

    // VA
    st = vaInitialize( ctx.video_va_display, &va_version[0], &va_version[1] );
    if( st != VA_STATUS_SUCCESS )
    {
        fprintf( stderr, "Failed to initialize libva\n" );
        return -1;
    }



    // Ffmpeg
    avdevice_register_all();
    r = avcodec_find_decoder( AV_CODEC_ID_H264 ) == NULL;
    r = r || ( avcodec_find_decoder( AV_CODEC_ID_MP3) == NULL );
    r = r || ( avcodec_find_encoder_by_name("h264_vaapi") == NULL );
    if( r )
    {
        fprintf( stderr, "Failed to find ffmpeg decoder/encoder h264, mp3 or h264_vaapi\n" );
        return -1;
    }
    ctx.hw_device_context = av_hwdevice_ctx_alloc( AV_HWDEVICE_TYPE_VAAPI );
    if( ctx.hw_device_context == NULL )
    {
        fprintf( stderr, "Failed to allocate ffmpeg vaapi context\n");
        return -1;
    }
    ((AVVAAPIDeviceContext*) (((AVHWDeviceContext*) ctx.hw_device_context->data)->hwctx))->display = ctx.video_va_display;
    r = av_hwdevice_ctx_init( ctx.hw_device_context );
    if( r != 0 )
    {
        fprintf( stderr, "Failed to initialize ffmpeg vaapi context\n");
        return -1;
    }
    ctx.recorder = NULL;

    // Portaudio
    r = Pa_Initialize();
    if( r != 0 )
    {
        fprintf( stderr, "Failed to initialize Port audio\n" );
        return -1;
    }
    r = Pa_GetDeviceCount();
    if( r < 0 )
    {
        fprintf( stderr, "No capture devives");
        return -1;
    }
    PaDeviceInfo* di;
    for( int i = 0; i < r; ++i )
    {
        di = Pa_GetDeviceInfo(i);
        fprintf( stdout, "Name=%s\n", di->name );
        fflush( stdout );
    }

    // stbi
    stbi_set_flip_vertically_on_load( true );
    return 0;
}

int main()
{
    int r;
    int window_width;
    int window_height;
    float window_aspect;


    r = init();
    if( r != 0 )
        return -1;


    ctx.lights_count = LIGHTS_COUNT;

    glfwSetErrorCallback( cb_glfw_error );
    r = glfwInit();
    if( r !=  GLFW_TRUE )
    {
        fprintf( stdout, "Failed to initialize glfw\n");
        return -1;
    }

    glfwWindowHint( GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint( GLFW_CLIENT_API, GLFW_OPENGL_API );
    glfwWindowHint( GLFW_CONTEXT_CREATION_API,  GLFW_EGL_CONTEXT_API );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, DEFAULT_GL_VERSION_MAJOR );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, DEFAULT_GL_VERSION_MINOR );
    glfwWindowHint( GLFW_SAMPLES, 4);

    ctx.glfw_window = glfwCreateWindow( DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL );
    if( ctx.glfw_window == NULL )
    {
        fprintf( stdout, "Failed to create glfw window\n");
        return -1;
    }
    glfwGetWindowSize( ctx.glfw_window, &window_width, &window_height );
    window_aspect = (float)window_width/ (float)window_height;
    ctx.mouse_previous[0] = window_width/2;
    ctx.mouse_previous[1] = window_height/2;
    glfwSetCursorPos( ctx.glfw_window, ctx.mouse_previous[0], ctx.mouse_previous[1] );
    glfwSetWindowUserPointer( ctx.glfw_window, &ctx);
    glfwSetKeyCallback( ctx.glfw_window, cb_glfw_key_callback );
    glfwSetCursorPosCallback( ctx.glfw_window, cb_glfw_cursor_position_callback );
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode( ctx.glfw_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

    glfwSetWindowPos( ctx.glfw_window, 200, 200 );
    glfwMakeContextCurrent( ctx.glfw_window );
    glfwSwapInterval(1);

    // glew
    glewExperimental = GL_TRUE;
    r = glewInit();
    if( r != GLEW_OK )
    {
        fprintf( stderr, "Failed to initialize glew\n");
        return -1;
    }


    // EGL
    glEGLImageTargetTexture2DOES =(PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) glfwGetProcAddress("glEGLImageTargetTexture2DOES");
    if( glEGLImageTargetTexture2DOES == NULL )
    {
        fprintf( stderr, "glEGLImageTargetTexture2DOES not found\n" );
        return -1;
    }

    int major;
    int minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor );
    fprintf( stdout, "OpenGL version : %d.%d\n", major, minor );
    fflush( stdout );


    // GL sutff

    // Load textures
    r = texture_2d_load_from_memory( &ctx.texture_ground, texture_ground, texture_ground_size );
    if( r != 0 )
        return -1;
    glBindTexture( GL_TEXTURE_2D, ctx.texture_ground );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture( GL_TEXTURE_2D, 0 );

    r = texture_2d_load_from_memory( &ctx.texture_lights[0], texture_light_0, (uint32_t) texture_light_0_size );
    if( r != 0 )
        return -1;
    r = texture_2d_load_from_memory( &ctx.texture_lights[1], texture_light_1, (uint32_t) texture_light_1_size );
    if( r != 0 )
        return -1;
    r = texture_2d_load_from_memory( &ctx.texture_lights[2], texture_light_2, (uint32_t) texture_light_2_size );
    if( r != 0 )
        return -1;

    // Load shapes
    load_fullscreen( &ctx.shape_fullscreen );
    load_quad( &ctx.shape_quad );
    load_cube( &ctx.shape_cube );

    Context* c = &ctx;
    // Light
    r = shader_load_from_string( &ctx.shader.id, shader_vs, shader_fs );
    if( r != 0 )
        return -1;
    ctx.shader.unif_mvp              = glGetUniformLocation( ctx.shader.id, "u_mvp" );
    ctx.shader.unif_model            = glGetUniformLocation( ctx.shader.id, "u_model" );
    ctx.shader.unif_texture          = glGetUniformLocation( ctx.shader.id, "u_texture" );
    ctx.shader.unif_camera_position  = glGetUniformLocation( ctx.shader.id, "u_camera_position");
    ctx.shader.unit_lights           = glGetUniformBlockIndex( ctx.shader.id, "LightsBlock");
    ctx.shader.unit_environment      = glGetUniformBlockIndex( ctx.shader.id, "EnvironmentBlock" );
    ctx.shader.unif_object_type      = glGetUniformLocation( ctx.shader.id, "u_object_type" );
    ctx.shader.unif_color            = glGetUniformLocation( ctx.shader.id, "u_color" );

    r = ctx.shader.unif_mvp == -1;
    r = r || ( ctx.shader.unif_texture == -1 );
    r = r || ( ctx.shader.unif_model == -1 );
    r = r || ( ctx.shader.unif_camera_position == -1 );
    r = r || ( ctx.shader.unit_lights == GL_INVALID_INDEX );
    r = r || ( ctx.shader.unit_environment == GL_INVALID_INDEX );
    r = r || ( ctx.shader.unif_object_type == -1 );
    r = r || ( ctx.shader.unif_color == -1 );
    if( r )
        return -1;

    glEnable( GL_CULL_FACE );
    glCullFace( GL_BACK );
    glFrontFace( GL_CCW );
    glEnable( GL_DEPTH_TEST );
    glClearColor( 0.f, 0.f, 0.f, 0.f );
    glEnable(GL_MULTISAMPLE);

    glGenBuffers( 1, &ctx.environment_buffer );
    glBindBuffer( GL_UNIFORM_BUFFER, ctx.environment_buffer );
    glBufferData( GL_UNIFORM_BUFFER, sizeof(Environment), NULL, GL_DYNAMIC_COPY );
    glBindBuffer( GL_UNIFORM_BUFFER, 0 );

    glGenBuffers( 1, &ctx.lights_buffer );
    glBindBuffer( GL_UNIFORM_BUFFER, ctx.lights_buffer );
    glBufferData( GL_UNIFORM_BUFFER, sizeof(Light) * ctx.lights_count, NULL, GL_DYNAMIC_COPY );
    glBindBuffer( GL_UNIFORM_BUFFER, 0 );

    // Shader
    glUseProgram( ctx.shader.id );
    glBindBufferBase( GL_UNIFORM_BUFFER, 2, ctx.lights_buffer  );
    glUniformBlockBinding( ctx.shader.id, ctx.shader.unit_lights , 2 );
    glBindBufferBase( GL_UNIFORM_BUFFER, 1, ctx.environment_buffer );
    glUniformBlockBinding( ctx.shader.id, ctx.shader.unit_environment, 1 );

    // Lights
    for( int i = 0; i < ctx.lights_count; ++i )
    {
        randomize_light( &ctx, i );
        ctx.lights_data[i].index = i;
    }

    // Videos
    const char* filenames[] = { VIDEO_0, VIDEO_1, VIDEO_2, VIDEO_3, VIDEO_4, VIDEO_5 };
    ctx.videos_count = 6;
    ctx.videos= (Video*) calloc( sizeof(Video), ctx.videos_count );
    for( int i = 0; i < ctx.videos_count; ++i )
    {
        Video* v = &ctx.videos[i];
        Player** p = &v->player;

        r = player_create( p,  ctx.hw_device_context,  filenames[i] );
        if( r != 0 )
        {
            fprintf( stderr, "Failed to load video %s\n", filenames[i] );
            fflush( stderr );
        }
        else
        {
            (*p)->parent_context = ctx.glfw_window;
            r = player_start( *p );
            if( r != 0 )
                return -1;
        }

        vec3 position;
        int left =  i % 2 == 0;

        float distance_to_wall = 1.1f;

        position[0] = left ? (-ROOM_X/2.f+distance_to_wall) : (ROOM_X/2.f - distance_to_wall);
        position[1] = 2.5f;
        position[2] = 5.f + i*ROOM_Z/ctx.videos_count - ROOM_Z/2.f;

        glm_mat4_identity( v->transform );
        glm_translate( v->transform, position );
        glm_rotate( v->transform, glm_rad(-90), VEC3_UNIT_Y );
        if( left )
            glm_rotate( v->transform, glm_rad(180), VEC3_UNIT_Y );
        glm_scale( v->transform, (vec3){ 8.0f, 4.5f, 1.f} );
    }

    glm_mat4_identity( ctx.mat_projection );
    glm_mat4_identity( ctx.mat_view );
    glm_mat4_identity( ctx.mat_vp );

    // Camera
    ctx.camera_flip_pitch_direction = 1;
    ctx.camera_position[0] = 0.f;
    ctx.camera_position[1] = 1.f;
    ctx.camera_position[2] = 0.f;
    ctx.camera_direction[0] = 0.f;
    ctx.camera_direction[1] = 0.f;
    ctx.camera_direction[2] = -1.f;
    ctx.camera_velocity = 8.0f;
    ctx.camera_velocity_angular = 4.0f;
    glfwSetInputMode( ctx.glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    compute_volume( &ctx );


    if( 0 )
    {
        glfwSetInputMode( ctx.glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        ctx.camera_mode = CAMERA_MODE_FREE;
    }

    // Environment
    memcpy( ctx.environment.ambient, (vec4) { 0.03f, 0.03f, 0.03f, 1.f}, sizeof(vec4) );
    memcpy( ctx.environment.fog_color, COLOR_BLADERUNNER_2049, sizeof(vec4) );
    ctx.environment.fog_density = 0.02f;

    // Projection
    glm_perspective( glm_rad(60), window_aspect, 0.1f, 1000.f, ctx.mat_projection  );
    glViewport( 0, 0, window_width, window_height );
    float last_time = (float) glfwGetTime();
    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    for(;;)
    {
        if( glfwWindowShouldClose(ctx.glfw_window) )
            ctx.flag_pending_quit = 1;
        if( ctx.flag_pending_quit )
        {
            ctx.flag_pending_quit = 0;
            for( int i = 0; i < ctx.videos_count; ++i )
            {
                Video* v = &ctx.videos[i];
                if( v->player)
                    v->player->flag_pending_quit = 1;
            }
            break;
        }

        ctx.time = (float) glfwGetTime();
        ctx.delta_time = ctx.time - last_time;
        ctx.environment.time = ctx.time;
        last_time = ctx.time;


        // Camera
        if( ctx.camera_translation_active )
        {
            float q = ctx.delta_time * ctx.camera_velocity;
            switch( ctx.camera_translation_active )
            {
                case GLFW_KEY_UP:
                    if( ctx.camera_mode == CAMERA_MODE_FREE )
                    {
                        ctx.camera_position[0] += ctx.camera_direction[0] * q;
                        ctx.camera_position[1] += ctx.camera_direction[1] * q;
                        ctx.camera_position[2] += ctx.camera_direction[2] * q;
                    }
                    else
                    {
                        vec3 front;
                        vec3 left;
                        glm_cross( VEC3_UNIT_Y, ctx.camera_direction, left );
                        glm_cross( left, VEC3_UNIT_Y, front );
                        glm_normalize( front );
                        ctx.camera_position[0] += front[0] * q;
                        ctx.camera_position[1] += front[1] * q;
                        ctx.camera_position[2] += front[2] * q;
                    }
                break;

                case GLFW_KEY_DOWN:
                    if( ctx.camera_mode == CAMERA_MODE_FREE )
                    {
                        ctx.camera_position[0] -= ctx.camera_direction[0] * q;
                        ctx.camera_position[1] -= ctx.camera_direction[1] * q;
                        ctx.camera_position[2] -= ctx.camera_direction[2] * q;
                    }
                    else
                    {
                        vec3 back;
                        vec3 right;
                        glm_cross( ctx.camera_direction, VEC3_UNIT_Y, right );
                        glm_cross( right, VEC3_UNIT_Y, back );
                        glm_normalize( back );
                        ctx.camera_position[0] += back[0] * q;
                        ctx.camera_position[1] += back[1] * q;
                        ctx.camera_position[2] += back[2] * q;

                    }
                break;

                case GLFW_KEY_LEFT:
                {
                    vec3 left;
                    glm_cross( VEC3_UNIT_Y, ctx.camera_direction, left );
                    ctx.camera_position[0] += left[0] * q;
                    ctx.camera_position[1] += left[1] * q;
                    ctx.camera_position[2] += left[2] * q;
                }
                break;

                case GLFW_KEY_RIGHT:
                {
                    vec3 right;
                    glm_cross( ctx.camera_direction, VEC3_UNIT_Y, right );
                    ctx.camera_position[0] += right[0] * q;
                    ctx.camera_position[1] += right[1] * q;
                    ctx.camera_position[2] += right[2] * q;
                }
                break;
            }

            if( ctx.camera_mode == CAMERA_MODE_NORMAL )
            {
                ctx.camera_position[0] = glm_clamp( ctx.camera_position[0], -ROOM_X/2.f+1.5f, ROOM_X/2.f-1.5f );
                ctx.camera_position[2] = glm_clamp( ctx.camera_position[2], -ROOM_Z/2.f+1.5f, ROOM_Z/2.f-1.5f );
            }

            compute_volume( &ctx );
        }
        vec3 center;
        center[0] = ctx.camera_position[0] + ctx.camera_direction[0];
        center[1] = ctx.camera_position[1] + ctx.camera_direction[1];
        center[2] = ctx.camera_position[2] + ctx.camera_direction[2];
        glm_lookat( ctx.camera_position, center, VEC3_UNIT_Y, ctx.mat_view );

        // vp
        glm_mat4_mul( ctx.mat_projection, ctx.mat_view, ctx.mat_vp  );

        // Shader uniforms
        GLvoid* p;

        glBindBuffer( GL_UNIFORM_BUFFER, ctx.lights_buffer );
        p = glMapBuffer( GL_UNIFORM_BUFFER, GL_WRITE_ONLY );
        memcpy( p, &ctx.lights, sizeof(Light) * ctx.lights_count );
        glUnmapBuffer( GL_UNIFORM_BUFFER );

        glBindBuffer( GL_UNIFORM_BUFFER, ctx.environment_buffer );
        p = glMapBuffer( GL_UNIFORM_BUFFER, GL_WRITE_ONLY );
        memcpy( p, &ctx.environment, sizeof(Environment) );
        glUnmapBuffer( GL_UNIFORM_BUFFER );

        glUniform3fv( ctx.shader.unif_camera_position, 1, ctx.camera_position );


        // Render scene
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        render_scene( &ctx );
        glFlush();

        if( ctx.recorder )
            recorder_add_video_frame( ctx.recorder, 0 );


        glfwSwapBuffers( ctx.glfw_window );
        glfwPollEvents();


        float duration = ( (float) glfwGetTime()) - ctx.time;


        static int cnt = 0 ;
        ++cnt;
        if( cnt == 200 )
        {
            cnt = 0;
            fprintf( stdout, "Theorical fps=%f\n", 1.f / duration );
            fflush( stdout );
        }
        float sleep_secs = 1.f/FPS_GL - duration;
        if( sleep_secs >  0.f )
            usleep( sleep_secs * 1000000.f);
        else
        {
            /*
            fprintf( stdout, "Underrun : duration = %f fps=%f\n",  duration*1000.f, 1.f/duration );
            fflush( stdout );
            */
        }
    }

    if( ctx.recorder )
        recorder_free( &ctx.recorder );

    for( int i = 0; i < ctx.videos_count; ++i )
    {
        Video* v = &ctx.videos[i];
        if( v->player )
        {
            player_stop( ctx.videos[i].player);
            player_free( &(ctx.videos[i].player) );
        }
    }
    free( ctx.videos );
    ctx.videos = NULL;

    glUseProgram(0);
    shader_delete( ctx.shader.id );
    glDeleteTextures( 1, &ctx.texture_ground );
    glDeleteTextures( 3, ctx.texture_lights );
    shape_unload( &ctx.shape_cube );
    shape_unload( &ctx.shape_quad );
    shape_unload( &ctx.shape_ground );
    shape_unload( &ctx.shape_fullscreen );
    glfwTerminate();
    vaTerminate( ctx.video_va_display );
    Pa_Terminate();
    close( ctx.fd );

    return 0;
}
