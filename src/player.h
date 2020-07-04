#ifndef INCLUDE_PLAYER
#define INCLUDE_PLAYER

#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <va/va.h>
#include <pthread.h>
#include <GL/eglew.h>
#include <GLFW/glfw3.h>
#include <portaudio.h>


#include "queue.h"

typedef struct
{
    AVFormatContext* fc;

    // Video
    GLuint           video_texture;
    VAContextID      video_va_context;
    VADisplay        video_va_display;
    GLFWwindow*      parent_context;
    Queue*    video_packet_queue;
    AVStream*        video_stream;
    AVCodecContext*  video_cc;
    pthread_t        video_thread;

    pthread_t        demuxer_thread;

    // Audio
    AVStream*       audio_stream;
    AVCodecContext* audio_cc;
    SwrContext*     audio_swr;
    Queue*   audio_packet_queue;
    pthread_t       audio_thread;
    volatile float  audio_volume;
 //   PaStream*       audio_pa_stream;


    double          clock_speed;
    int64_t         clock_start_time;


    uint8_t          flag_pending_quit;
} Player;


int  player_create( Player** data, AVBufferRef* hw_device_context, const char* url );
void player_free( Player** data );

int  player_start( Player* pipeline );
void player_stop( Player* pipeline );



#endif
