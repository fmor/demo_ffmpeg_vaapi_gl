#include "util.h"


#include <string.h>
#include <GL/eglew.h>

#include "stb.h"
#include <va/va_drm.h>
#include <va/va_drmcommon.h>


PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

int texture2d_to_jpeg(GLuint tex, int level, const char* filename)
{
    GLint old_tex;
    GLint w;
    GLint h;
    void* pixels;
    int r;

    old_tex = 0;
    w = 0;
    h = 0;
    pixels = NULL;


    glGetIntegerv( GL_TEXTURE_BINDING_2D, &old_tex );
    glBindTexture( GL_TEXTURE_2D, tex );
    glGetTexLevelParameteriv( GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &w );
    glGetTexLevelParameteriv( GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &h );

    pixels = malloc( w * h * 3 );
    if( pixels == NULL )
        goto LBL_FAILED;

    glGetTexImage( GL_TEXTURE_2D, level, GL_RGB, GL_UNSIGNED_BYTE, pixels );
    r = stbi_write_jpg( filename, w, h, 3, pixels, 90 );
    if( r != 1 )
        goto LBL_FAILED;
    free( pixels );
    glBindTexture( GL_TEXTURE_2D, old_tex );
    return 0;

LBL_FAILED:
    if( pixels )
        free( pixels );
    glBindTexture( GL_TEXTURE_2D, old_tex );
    return -1;
}


int texture_2d_load_from_file(GLuint* tex, const char* filename)
{
    int w;
    int h;
    int channels;
    stbi_uc* pixels;

    w = 0;
    h = 0;
    channels = 0;
    pixels =  NULL;

    pixels = stbi_load( filename, &w, &h, &channels, STBI_rgb_alpha );
    if( pixels == NULL )
        goto LBL_FAILED;
    if( (channels < 1) || (channels > 4) )
        goto LBL_FAILED;
    glGenTextures( 1, tex );
    glBindTexture( GL_TEXTURE_2D, *tex);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels );
    stbi_image_free( pixels );
    return 0;
LBL_FAILED:
    if( pixels )
        stbi_image_free( pixels );
    return -1;
}
int texture_2d_load_from_memory( GLuint* tex, const uint8_t* data, uint32_t data_len )
{
    int w;
    int h;
    int channels;
    stbi_uc* pixels;

    w = 0;
    h = 0;
    channels = 0;
    pixels =  NULL;

    pixels = stbi_load_from_memory( data , data_len, &w, &h, &channels, STBI_rgb_alpha );
    if( pixels == NULL )
        goto LBL_FAILED;
    if( (channels < 1) || (channels > 4) )
        goto LBL_FAILED;
    glGenTextures( 1, tex );
    glBindTexture( GL_TEXTURE_2D, *tex);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels );
    stbi_image_free( pixels );
    return 0;
LBL_FAILED:
    if( pixels )
        stbi_image_free( pixels );
    return -1;
}
int texture_cubemap_load(GLuint* tex, const char* filename)
{
    GLuint t;
    void* data;
    int width;
    int height;
    int channels;
#define HOME_RESOURCE   "/projects/fmor/demo_ffmpeg_vaapi_gl/resources/"
#define SKYBOX  "test"

    data = NULL;
    t = 0;

    glGenTextures( 1, &t );
    glBindTexture( GL_TEXTURE_CUBE_MAP, t );

    // Right
    data = stbi_load( HOME_RESOURCE "/" SKYBOX "_right.jpg", &width, &height, &channels, 3 );
    if( data == NULL )
        goto LBL_FAILED;
    glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data );
    stbi_image_free( data );

    // Left
    data = stbi_load( HOME_RESOURCE "/" SKYBOX "_left.jpg", &width, &height, &channels, 0 );
    if( data == NULL )
        goto LBL_FAILED;
    glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data );
    stbi_image_free( data );

    // Top
    data = stbi_load( HOME_RESOURCE "/" SKYBOX "_up.jpg", &width, &height, &channels, 0 );
    if( data == NULL )
        goto LBL_FAILED;
    glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data );
    stbi_image_free( data );

    // Bottom
    data = stbi_load( HOME_RESOURCE "/" SKYBOX "_down.jpg", &width, &height, &channels, 0 );
    if( data == NULL )
        goto LBL_FAILED;
    glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data );
    stbi_image_free( data );

    // Back
    data = stbi_load( HOME_RESOURCE "/" SKYBOX "_back.jpg", &width, &height, &channels, 0 );
    if( data == NULL )
        goto LBL_FAILED;
    glTexImage2D( GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data );
    stbi_image_free( data );

    // Front
    data = stbi_load( HOME_RESOURCE "/" SKYBOX "_front.jpg", &width, &height, &channels, 0 );
    if( data == NULL )
        goto LBL_FAILED;
    glTexImage2D( GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data );


    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
    *tex = t;
    return 0;
LBL_FAILED:
    if( data )
        stbi_image_free( data );
    glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
    glDeleteTextures( 1, &t );
    return -1;

}
int load_text_file( char** str, const char* filename )
{
    FILE* f;
    int r;
    int sz;
    char* s;

    s = NULL;

    f = fopen( filename, "r" );
    if( f == NULL )
        goto LBL_FAILED;
    r = fseek( f, 0, SEEK_END );
    if( r != 0 )
        goto LBL_FAILED;
    sz = ftell(f);
    r = fseek( f, 0, SEEK_SET );
    if( r != 0 )
        goto LBL_FAILED;
    s = (char*) malloc( sz + 1 );
    if( s == NULL )
        goto LBL_FAILED;

    r = fread( s, sz, 1, f );
    if( r != 1 )
        goto LBL_FAILED;

    fclose(f);
    s[sz] = 0;
    *str = s;
    return 0;
LBL_FAILED:
    if( f )
        fclose(f);
    if( s )
        free(s);
    return -1;
}

int shader_load_from_file(GLuint* id, const char* vertex_filename, const char* fragment_filename)
{
    int r;
    char* vertex;
    char* fragment;

    vertex = NULL;
    fragment = NULL;

    r = load_text_file( &vertex, vertex_filename );
    if( r != 0 )
        goto LBL_FAILED;
    r = load_text_file( &fragment, fragment_filename );
    if( r != 0 )
        goto LBL_FAILED;
    r = shader_load_from_string( id, vertex, fragment );
    if( r != 0 )
        goto LBL_FAILED;
    free( vertex );
    free( fragment );
    return 0;
LBL_FAILED:
    if( vertex )
        free( vertex );
    if( fragment )
        free( fragment );
    return -1;
}


int shader_load_from_string(GLuint* id, const char* vertex, const char* fragment)
{
    GLint sz;
    GLint compile_status;
    GLuint ids[3];
    char   err[1024];
    GLsizei err_length;

    memset( &ids, 0, sizeof(GLuint) * 3 );
    ids[0] = glCreateProgram();
    ids[1] = glCreateShader( GL_VERTEX_SHADER );
    ids[2] = glCreateShader( GL_FRAGMENT_SHADER );

    err[0] = 0;
    err_length = 0;

    glShaderSource( ids[1], 1, &vertex, NULL);
    glCompileShader( ids[1] );
    glGetShaderiv( ids[1], GL_COMPILE_STATUS, &compile_status );
    if( compile_status == GL_FALSE )
    {
        glGetShaderInfoLog( ids[1], 1024, &err_length, err );
        fprintf( stderr, "Failed to load vertex shader :\n\n%s\n", err );
        goto LBL_FAILED;
    }

    glShaderSource( ids[2], 1, &fragment, NULL );
    glCompileShader( ids[2] );
    glGetShaderiv( ids[2], GL_COMPILE_STATUS, &compile_status );
    if( compile_status == GL_FALSE )
    {
        glGetShaderInfoLog( ids[2], 1024, &err_length, err );
        fprintf( stderr, "Failed to load fragment shader :\n\n%s\n", err );
        goto LBL_FAILED;
    }

    glAttachShader( ids[0], ids[1] );
    glAttachShader( ids[0], ids[2] );
    glLinkProgram( ids[0] );
    glGetProgramiv( ids[0], GL_LINK_STATUS, &compile_status );
    if( compile_status == GL_FALSE )
    {
        glGetProgramInfoLog( ids[0], 1024, &err_length, err );
        goto LBL_FAILED;
    }

    *id = ids[0];
    return 0;
LBL_FAILED:
    glDeleteProgram( ids[0] );
    glDeleteShader( ids[1] );
    glDeleteShader( ids[2] );
    return -1;
}







void hsv_2_rgb(float h, float s, float v, float* r, float* g, float* b)
{
    float      hh, p, q, t, ff;
    long        i;

    if( s <= 0.f)
    {       // < is bogus, just shuts up warnings
        *r = v;
        *g = v;
        *b = v;
    }
    hh = h;
    if(hh >= 360.f)
        hh = 0.0;
    hh /= 60.f;
    i = (long)hh;
    ff = hh - i;
    p = v * (1.f - s);
    q = v * (1.f - (s * ff));
    t = v * (1.f - (s * (1.f - ff)));

    switch(i) {
        case 0:
            *r = v;
            *g = t;
            *b = p;
        break;
        case 1:
            *r = q;
            *g = v;
            *b = p;
        break;
        case 2:
            *r = p;
            *g = v;
            *b = t;
        break;

        case 3:
            *r = p;
            *g = q;
            *b = v;
        break;
        case 4:
            *r = t;
            *g = p;
            *b = v;
        break;
        case 5:
        default:
            *r = v;
            *g = p;
            *b = q;
        break;
    }
    return;
}

float mat4_billboard(mat4 mat, vec3 camera, vec3 position)
{
    vec3 front;
    vec3 left;
    vec3 up;
    float distance_to_camera;

    glm_vec3_sub( camera, position, front );
    distance_to_camera = sqrtf( front[0]*front[0] + front[1]*front[1] + front[2]*front[2] );
    glm_normalize( front );
    glm_vec3_cross( (vec3) {0.f, 1.f, 0.f}, front, left );
    glm_vec3_cross( front, left, up );


    mat[0][0] = left[0];
    mat[0][1] = left[1];
    mat[0][2] = left[2];
    mat[0][3] = 0;

    mat[1][0] = up[0];
    mat[1][1] = up[1];
    mat[1][2] = up[2];
    mat[1][3] = 0;

    mat[2][0] = front[0];
    mat[2][1] = front[1];
    mat[2][2] = front[2];
    mat[2][3] = 0;


    mat[3][0] = position[0];
    mat[3][1] = position[1];
    mat[3][2] = position[2];
    mat[3][3] = 1;

    return distance_to_camera;

}

EGLImage egl_create_image_from_va(VASurfaceID* _va_surface, VADisplay va_display, int width, int height)
{
    EGLImage egl_image;
    VASurfaceID va_surface;
    VASurfaceAttrib va_surface_attrib;
    VADRMPRIMESurfaceDescriptor va_surface_descriptor;
    int r;

    egl_image = EGL_NO_IMAGE;
    va_surface = VA_INVALID_SURFACE;

    va_surface_attrib.type = VASurfaceAttribPixelFormat;
    va_surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    va_surface_attrib.value.type = VAGenericValueTypeInteger;
    va_surface_attrib.value.value.i = VA_FOURCC_BGRA;

    r = vaCreateSurfaces( va_display, VA_RT_FORMAT_RGB32, width, height, &va_surface, 1, &va_surface_attrib, 1 );
    if( r != VA_STATUS_SUCCESS )
        goto LBL_FAILED;

    r = vaExportSurfaceHandle( va_display, va_surface, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2, VA_EXPORT_SURFACE_READ_ONLY, &va_surface_descriptor );
    if( r != 0 )
        goto LBL_FAILED;

    EGLAttrib egl_img_attributes[] = {
        EGL_LINUX_DRM_FOURCC_EXT, va_surface_descriptor.layers[0].drm_format,
        EGL_WIDTH, va_surface_descriptor.width,
        EGL_HEIGHT, va_surface_descriptor.height,
        EGL_DMA_BUF_PLANE0_FD_EXT, va_surface_descriptor.objects[va_surface_descriptor.layers[0].object_index[0]].fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, va_surface_descriptor.layers[0].offset[0],
        EGL_DMA_BUF_PLANE0_PITCH_EXT, va_surface_descriptor.layers[0].pitch[0],
        EGL_NONE
    };
    egl_image = eglCreateImage( eglGetCurrentDisplay(), EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, egl_img_attributes );
    if( egl_image == EGL_NO_IMAGE )
        goto LBL_FAILED;


    *_va_surface = va_surface;
    return egl_image;
LBL_FAILED:
    if( va_surface != VA_INVALID_SURFACE )
        vaDestroySurfaces( va_display, &va_surface, 1 );
    return EGL_NO_IMAGE;
}

void free_packet( AVPacket* packet )
{
    av_packet_free( &packet );
}
void free_frame( AVFrame* frame )
{
    av_frame_free( &frame );
}

void shader_delete(GLuint program)
{
    GLuint shaders[2];
    GLsizei count;
    shaders[0] = 0;
    shaders[1] = 0;
    glGetAttachedShaders( program, 2, &count, shaders );
    glDeleteProgram( program );
    if( shaders[0] )
        glDeleteShader( shaders[0] );
    if( shaders[1] )
        glDeleteShader( shaders[1] );
}


