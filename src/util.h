#ifndef INCLUDE_UTIL
#define INCLUDE_UTIL

#include <GL/eglew.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>
#include <va/va.h>

#include "stb_image_write.h"
#include <libavcodec/avcodec.h>

typedef void *GLeglImageOES;
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum target, GLeglImageOES image);
extern PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

int texture2d_to_jpeg( GLuint tex, int level, const char* filename );
int texture_2d_load_from_file( GLuint* tex, const char* filename );
int texture_2d_load_from_memory( GLuint* tex, const uint8_t* data, uint32_t data_len);

int texture_cubemap_load( GLuint* tex, const char* filename );

int shader_load_from_file( GLuint* id, const char* vertex_filename, const char* fragment_filename );
int shader_load_from_string( GLuint* id, const char* vertex, const char* fragment );
void shader_delete( GLuint program );

void hsv_2_rgb( float h, float s, float v, float* r, float* g, float* b );

float mat4_billboard( mat4 result, vec3 camera, vec3 position );

EGLImage egl_create_image_from_va( VASurfaceID* va_surface, VADisplay va_display, int width, int height );

void free_packet( AVPacket* packet );
void free_frame( AVFrame* frame );

#endif
