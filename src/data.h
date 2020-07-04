#ifndef INCLUDE_DATA
#define INCLUDE_DATA

#include <GL/eglew.h>
#include <GLFW/glfw3.h>
#include <va/va.h>
#include <libavutil/buffer.h>
#include <cglm/cglm.h>

#include "config.h"
#include "player.h"
#include "shapes.h"
#include "recorder.h"


typedef enum
{
    OBJECT_TYPE_LIGHT = 0,
    OBJECT_TYPE_VIDEO,
    OBJECT_TYPE_CUBE,
    OBJECT_TYPE_PLANE,
    OBJECT_TYPE_TV_SNOW,
    OBJECT_TYPE_GROUND
} Object_type;

typedef struct
{
    GLuint  id;

    GLint   unif_mvp;
    GLint   unif_model;
    GLint   unif_camera_position;
    GLint   unif_texture;
    GLuint  unit_lights;
    GLuint  unit_environment;
    GLint   unif_object_type;
    GLint   unif_color;
} Shader;




typedef struct
{
    Player*   player;
    mat4      transform;
} Video;


typedef struct
{
    float position[4];
    float color[4];
    float power;
    float constant;
    float linear;
    float quadratic;
} Light;

typedef struct
{
    GLuint  texture;
    float   velocity;
    float   scale;
    float   distance_to_camera;
    mat4    transform;
    int     index;
} Light_data;

typedef struct
{
    vec4    ambient;
    vec4    fog_color;
    float   fog_density;
    float   time;
} Environment;


typedef enum
{
    CAMERA_MODE_NORMAL,
    CAMERA_MODE_FREE
} Camera_mode;



typedef struct
{
    int                 fd;
    AVBufferRef*        hw_device_context;
    VADisplay           video_va_display;
    uint8_t             flag_pending_quit;

    GLFWwindow*         glfw_window;
    EGLContext          egl_context;

    Recorder*           recorder;

    GLuint              texture_ground;
    GLuint              texture_lights[3];


    Shape               shape_fullscreen;
    Shape               shape_quad;
    Shape               shape_ground;
    Shape               shape_cube;

    Shader              shader;


    Environment         environment;
    GLuint              environment_buffer;

    Light               lights[LIGHTS_COUNT];
    Light_data          lights_data[LIGHTS_COUNT];

    int                 lights_count;
    GLuint              lights_buffer;

    mat4                mat_projection;

    // Camera
    mat4                mat_view;

    mat4                mat_vp;

    uint8_t             camera_flip_pitch_direction;
    float               camera_velocity;
    float               camera_velocity_angular;
    vec3                camera_position;
    vec3                camera_direction;
    int                 camera_translation_active;
    Camera_mode         camera_mode;

    float               time;
    float               delta_time;

    vec2                mouse_previous;
    vec2                mouse_rel;


    Video*              videos;
    int                 videos_count;
} Context;


#endif
