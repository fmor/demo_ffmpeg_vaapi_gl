#ifndef INCLUDE_RECORDER
#define INCLUDE_RECORDER

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <va/va.h>
#include <pthread.h>
#include <GL/eglew.h>
#include <portaudio.h>



#include "queue.h"

typedef struct
{
    float               time_seconds_start;
    float               time_seconds_last_frame;
    AVFormatContext*    fc;

    Queue*              muxer_queue;
    pthread_t           muxer_thread;

    GLuint              video_fbo;
    GLuint              video_fbo_color;
    EGLImage            video_fbo_color_egl_image;
    VASurfaceID         video_fbo_color_va_surface;
    AVCodecContext*     video_cc;
    VAContextID         video_va_context;
    VADisplay           video_va_display;
    VABufferID          video_va_pipeline_parameters_buffer_id;
    Queue*              video_frame_queue;
    pthread_t           video_thread;
    uint                video_frame_count;

    AVCodecContext*     audio_cc;
    PaStream*           audio_stream;
    SwrContext*         audio_swr;
    pthread_t           audio_thread;
    Queue*              audio_queue;



} Recorder;


Recorder* recorder_create( VADisplay va_display, AVBufferRef* hw_context, int width, int height, int fps, const char* filename );
void recorder_free( Recorder** recorder );
void recorder_add_video_frame( Recorder* recorder, GLuint read_fbo );



#endif
