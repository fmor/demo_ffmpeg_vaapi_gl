#ifndef INCLUDE_CONFIG
#define INCLUDE_CONFIG

#define WINDOW_TITLE            "Demo ffmpeg vaapi GL"
#define DEFAULT_WINDOW_WIDTH    1280
#define DEFAULT_WINDOW_HEIGHT   720
#define FPS_GL                  50
#define FPS_RECORD              50

#define RECORD_FILENAME         "/tmp/video_%02d.mp4"

#define VIDEO_0                 "/home/fmor/video/lipsync2.mp4"
#define VIDEO_1                 "/home/fmor/video/savon.mp4"
#define VIDEO_2                 "/home/fmor/video/star_trek.mp4"
#define VIDEO_3                 "/home/fmor/video/prologue.mkv"
#define VIDEO_4                 "/home/fmor/video/titan.mp4"
#define VIDEO_5                 "/home/fmor/video/loups.mp4"

#define DEFAULT_GL_VERSION_MAJOR        3
#define DEFAULT_GL_VERSION_MINOR        3

#define ROOM_X          10
#define ROOM_Y          7
#define ROOM_Z          70
#define LIGHTS_COUNT    4

#define COLOR_WHITE (vec4){ 1.f, 1.f, 1.f, 1.f }
#define COLOR_BLACK (vec4){ 0.f, 0.f, 0.f, 1.f }
#define COLOR_BLADERUNNER_2049 (vec4) { 0xE2/255.f, 0x90/255.f, 0x06/255.f, 1.f}


#define VEC3_ZERO   (vec3) { 0.f, 0.f, 0.f }
#define VEC3_UNIT_X (vec3) { 1.f, 0.f, 0.f }
#define VEC3_UNIT_Y (vec3) { 0.f, 1.f, 0.f }
#define VEC3_UNIT_Z (vec3) { 0.f, 0.f, 1.f }
#define VEC3_NEGATIVE_UNIT_X (vec3) { -1.f,  0.f,  0.f }
#define VEC3_NEGATIVE_UNIT_Y (vec3) {  0.f, -1.f,  0.f }
#define VEC3_NEGATIVE_UNIT_Z (vec3) {  0.f,  0.f, -1.f }
#define VEC3_UNIT_SCALE (vec3) { 1.0f, 1.0f, 1.0f }

#endif
