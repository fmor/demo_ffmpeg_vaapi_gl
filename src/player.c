#include "player.h"

#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>
#include <libavutil/samplefmt.h>
#include <libavutil/time.h>



#include <va/va_drmcommon.h>

#include <samplerate.h>

#include <unistd.h>
#include <assert.h>

#include "config.h"
#include "util.h"

static int player_demux( Player* player);
static int player_decode_video( Player* player );
static int player_decode_audio( Player* player );
static double clock_seconds( int64_t start_time )
{
    return (av_gettime() - start_time) / 1000000.;
}
static double pts_seconds( AVFrame* frame, AVStream* stream )
{
    return frame->pts * av_q2d(stream->time_base);
}


typedef struct
{
    AVStream*       stream;
    SRC_STATE*      src_state;
    Queue*          frame_queue;
    AVFrame*        frame;
    float*          frame_data;
    uint            frame_nb_samples;
    double*         clock_speed;
    int64_t*        clock_start_time;
    float*          volume;
} cb_PastreamCallback_data;

static int cb_pa_stream( const void *input,
                         void *output,
                         unsigned long frame_count,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData )
{
    int r;
    cb_PastreamCallback_data* d;
    d = (cb_PastreamCallback_data*) userData;

    float* out = (float*) output;

    SRC_DATA data;


    while( frame_count)
    {
        if( d->frame == NULL )
        {
            d->frame  = queue_pop( d->frame_queue );
            d->frame_data = (float*) d->frame->data[0];
            d->frame_nb_samples = d->frame->nb_samples;

        }

        double clock = clock_seconds( *d->clock_start_time );
        double pts_secs = pts_seconds( d->frame, d->stream );
        double delta = pts_secs - clock;


        if( delta < -0.6 )
            data.src_ratio = 0.4;
        else
            data.src_ratio = (1  + delta) / *(d->clock_speed) ;

        data.data_in = d->frame_data;
        data.data_out = out;
        data.input_frames = d->frame_nb_samples;
        data.output_frames = frame_count;
        data.input_frames_used = 0;
        data.output_frames_gen = 0;
        data.end_of_input = 0;
        assert( src_is_valid_ratio(data.src_ratio) );
        if( src_is_valid_ratio(data.src_ratio) == 0 )
            assert( 0 );

        r = src_process( d->src_state, &data );
        assert( r == 0 );
        assert( data.output_frames_gen != 0 );


        r = data.output_frames_gen * 2;
        for( int i = 0; i < r; ++i )
            data.data_out[i] *= *d->volume;


        frame_count -= data.output_frames_gen;
        out += data.output_frames_gen * 2;
        d->frame_data += data.input_frames_used * 2;
        d->frame_nb_samples -= data.input_frames_used;

        if( d->frame_nb_samples == 0 )
            av_frame_free( &d->frame );
    }

    return 0;
}

static int codec_create( AVCodecContext** _cc, AVStream* stream, AVBufferRef* hw_device_context )
{
    AVCodec* c;
    AVCodecContext* cc;
    c = NULL;
    cc = NULL;
    int r;

    c = avcodec_find_decoder( stream->codecpar->codec_id );
    if( c == NULL )
        goto LBL_FAILED;

    cc = avcodec_alloc_context3( c );
    if( cc == NULL )
        goto LBL_FAILED;

    r = avcodec_parameters_to_context( cc, stream->codecpar );
    if( r != 0 )
        goto LBL_FAILED;
    if( hw_device_context )
        cc->hw_device_ctx = av_buffer_ref( hw_device_context );

    r = avcodec_open2( cc, c, NULL );
    if( r < 0 )
        goto LBL_FAILED;
    *_cc = cc;
    return 0;
LBL_FAILED:
    if( cc )
        avcodec_free_context( &cc );
    return -1;
}

int player_create( Player** data, AVBufferRef* hw_device_context, const char* url )
{
    int r;
    Player* player;
    VAStatus st;
    VAConfigAttrib va_config_attribs;
    VAConfigID va_config_id;
    AVCodec* c;

    va_config_id = VA_INVALID_ID;
    c = NULL;


    player = (Player*) malloc( sizeof(Player) );
    player->fc = NULL;
    player->flag_pending_quit = 0;
    player->clock_speed = 1.0;
    player->clock_start_time = AV_NOPTS_VALUE;

    player->demuxer_thread = 0;

    player->video_va_display =  ((AVVAAPIDeviceContext*) ((AVHWDeviceContext*) hw_device_context->data)->hwctx)->display;
    player->video_stream = NULL;
    player->video_cc = NULL;
    player->video_thread = 0;
    player->video_packet_queue = NULL;
    player->video_texture = 0;

    player->audio_stream = NULL;
    player->audio_cc = NULL;
    player->audio_swr = NULL;
    player->audio_packet_queue = NULL;
    player->audio_volume = 1.0f;
    player->audio_thread = 0;


    r = avformat_open_input( &player->fc, url, NULL, NULL );
    if( r != 0 )
    {
        fprintf( stderr, "Failed to open video file %s\n", url );
        goto LBL_FAILED;
    }
    r = avformat_find_stream_info( player->fc, NULL );
    if( r < 0 )
    {
        fprintf( stderr, "Failed to find stream info\n" );
        goto LBL_FAILED;
    }
    for( unsigned i = 0; i < player->fc->nb_streams; ++i )
    {
        AVStream* stream;
        stream = player->fc->streams[i];

        r = player->video_stream == NULL;
        //        r = r && ( stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO );
        r = r && ( stream->codecpar->codec_id == AV_CODEC_ID_H264);
        if( r )
            player->video_stream = stream;

        r = player->audio_stream == NULL;
        r = r && ( stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO );
        if( r )
            player->audio_stream = stream;

        if( player->audio_stream && player->video_stream )
            break;
    }

    if( player->video_stream == NULL )
    {
        fprintf( stderr, "No video/audio supported stream found\n");
        fprintf( stderr, "A H264 video stream and/or audio stream required\n");
        goto LBL_FAILED;
    }

    // Setup video
    r = codec_create( &player->video_cc, player->video_stream, hw_device_context );
    if( r != 0 )
        goto LBL_FAILED;


    // return Create( display, VA_RT_FORMAT_YUV420, width, height, VA_RT_FORMAT_RGB32, width, height  );
    // VAAPI
    va_config_id = VA_INVALID_ID;
    memset( &va_config_attribs, 0, sizeof(VAConfigAttrib) );
    va_config_attribs.type = VAConfigAttribRTFormat;
    st = vaGetConfigAttributes( player->video_va_display, VAProfileNone, VAEntrypointVideoProc, &va_config_attribs, 1 );
    if( st != VA_STATUS_SUCCESS )
        goto LBL_FAILED;
    if( (va_config_attribs.value & VA_RT_FORMAT_YUV420) == 0  )
    {
        fprintf( stderr, "H264 not supported by vaapi\n");
        goto LBL_FAILED;
    }

    st = vaCreateConfig( player->video_va_display, VAProfileNone, VAEntrypointVideoProc, &va_config_attribs, 1, &va_config_id );
    if( st != VA_STATUS_SUCCESS )
    {
        fprintf( stderr, "Failed to create vaapi config\n");
        goto LBL_FAILED;
    }

    st = vaCreateContext( player->video_va_display, va_config_id, player->video_stream->codecpar->width, player->video_stream->codecpar->height, VA_PROGRESSIVE, NULL, 0, &player->video_va_context );
    if( st != VA_STATUS_SUCCESS )
    {
        fprintf( stderr, "Failed to create vaapi context\n");
        goto LBL_FAILED;
    }

    player->video_packet_queue  = queue_alloc( 100, (Queue_free_function) free_packet);
    if( player->video_packet_queue == NULL  )
        goto LBL_FAILED;




    // Setup audio
    if( player->audio_stream   )
    {
        r = codec_create( &player->audio_cc, player->audio_stream, NULL );
        if( r != 0 )
            goto LBL_FAILED;

        player->audio_swr = swr_alloc_set_opts( NULL,
                                                  AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_FLT, 44100,
                                                  player->audio_stream->codecpar->channel_layout, ( enum AVSampleFormat) player->audio_stream->codecpar->format, player->audio_stream->codecpar->sample_rate,
                                                  0, NULL );
        if( player->audio_swr == NULL )
            goto LBL_FAILED;

        player->audio_packet_queue = queue_alloc( 100, (Queue_free_function)  free_packet );
        if( player->audio_packet_queue == NULL  )
            goto LBL_FAILED;


        r = swr_init( player->audio_swr );
        if( r < 0 )
            goto LBL_FAILED;
    }

    *data = player;

    return 0;
LBL_FAILED:
    if( player->audio_cc )
        avcodec_free_context( &player->audio_cc );
    if( player->audio_swr )
        swr_free( &player->audio_swr );
    if( player->audio_packet_queue )
        queue_free( &player->audio_packet_queue );

    if( player->video_cc )
        avcodec_free_context( &player->video_cc );
    if( player->fc )
        avformat_close_input( &player->fc );
    if( va_config_id != VA_INVALID_ID  )
        vaDestroyConfig( player->video_va_display, va_config_id );
    if( player->video_va_context != VA_INVALID_ID )
        vaDestroyContext( player->video_va_display, player->video_va_context );
    if( player->video_packet_queue )
        queue_free( &player->video_packet_queue );

    free( player );
    return -1;
}

void player_free( Player** player )
{
    Player* p = *player;
    avformat_close_input( &p->fc );

    if( p->video_stream )
    {
        queue_free( &p->video_packet_queue );
        avcodec_free_context( &p->video_cc );
        vaDestroyContext( p->video_va_display, p->video_va_context );
        p->video_va_context= VA_INVALID_ID;
        p->video_va_display = NULL;
    }

    if( p->audio_stream )
    {
        queue_free( &p->audio_packet_queue );
        avcodec_free_context( &p->audio_cc );
        swr_free( &p->audio_swr );
        p->audio_stream = NULL;
    }

    free( p );
    *player = NULL;
}


int player_start(Player* player)
{
    int r;
    r = pthread_create( &player->demuxer_thread, NULL, player_demux, player );
    if( r != 0 )
        goto LBL_FAILED;
    if( player->video_stream )
    {
        r = pthread_create( &player->video_thread, NULL, player_decode_video, player );
        if( r != 0 )
            goto LBL_FAILED;
    }
    if( player->audio_stream )
    {
        r = pthread_create( &player->audio_thread, NULL, player_decode_audio, player );
        if( r != 0 )
            goto LBL_FAILED;
    }
    return 0;
LBL_FAILED:
    assert(  0 && "Failed to start player" );
    return -1;
}

void player_stop(Player* player)
{
    intptr_t r;
    r = 0;
    player->flag_pending_quit = 1;
    if( player->video_thread )
    {
        queue_flush( player->video_packet_queue );
        queue_push( player->video_packet_queue, NULL );
        pthread_join( player->video_thread, (void*) &r );
        player->video_thread = 0;
    }

    if( player->audio_thread )
    {
        queue_flush( player->audio_packet_queue );
        queue_push( player->audio_packet_queue, NULL );
        pthread_join( player->audio_thread, (void*) &r );
        player->audio_thread = 0;
    }
    pthread_join( player->demuxer_thread, (void*) &r );
    player->demuxer_thread = 0;
    player->flag_pending_quit = 0;
}


static int player_demux( Player* player )
{
    AVPacket packet;
    int r;
    av_init_packet( &packet );
    int64_t video_offset;
    int64_t audio_offset;
    int64_t last_video_pts;
    int64_t last_audio_pts;

    video_offset = 0;
    audio_offset = 0;
    player->clock_start_time = av_gettime();
    for(;;)
    {
        if( player->flag_pending_quit )
            goto LBL_TERMINATE;
        r = av_read_frame( player->fc, &packet );
        switch( r )
        {
            case 0:

                if( player->video_stream && (packet.stream_index == player->video_stream->index) )
                {
                    AVPacket* clone = av_packet_clone( &packet );
                    clone->pts += video_offset;
                    clone->dts += video_offset;
                    last_video_pts = clone->pts;
                    queue_push( player->video_packet_queue, (void*) clone );
                }
                else if( player->audio_stream && (packet.stream_index == player->audio_stream->index) )
                {
                    AVPacket* clone = av_packet_clone( &packet );
                    clone->pts += audio_offset;
                    clone->dts += audio_offset;
                    last_audio_pts = clone->pts;
                    queue_push( player->audio_packet_queue, (void*) clone );
                }
            break;

            case AVERROR_EOF:
                // restart
                r = av_seek_frame( player->fc, player->video_stream->index, player->video_stream->start_time, AVSEEK_FLAG_BACKWARD );
                if( r < 0 )
                    goto LBL_FAILED;
                if( player->video_stream )
                    video_offset = last_video_pts;
                if( player->audio_stream )
                    audio_offset = last_audio_pts;
            break;

            default:
                goto LBL_FAILED;
        }
    }
LBL_TERMINATE:
    av_packet_unref( &packet );
    return 2;

LBL_FAILED:
    fprintf( stderr, "Failed av_read_frame\n");
    return -1;
}

static int player_decode_video(Player* player )
{
    int r;
    AVPacket*       packet;
    GLFWwindow*     context;
    VASurfaceID     surface;
    VADRMPRIMESurfaceDescriptor va_surface_descriptor;
    VAStatus        va_status;
    VABufferID      va_pipeline_param_buf_id;
    VAProcPipelineParameterBuffer va_pipeline_param_buffer;
    VARectangle     va_rectangle;
    EGLImageKHR     egl_image;
    AVFrame*        frame;

    double          vaapi_delay;
    vaapi_delay = 0.;


    packet = NULL;
    surface = VA_INVALID_SURFACE;
    context = NULL;
    memset( &va_surface_descriptor, 0, sizeof(VASurfaceAttrib) );
    memset( &va_surface_descriptor, 0, sizeof(VADRMPRIMESurfaceDescriptor) );
    memset( &va_pipeline_param_buffer, 0, sizeof(VAProcPipelineParameterBuffer) );
    va_pipeline_param_buf_id = VA_INVALID_ID;
    egl_image = EGL_NO_IMAGE;
    frame = NULL;

    glfwWindowHint( GLFW_CONTEXT_CREATION_API,  GLFW_EGL_CONTEXT_API );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, DEFAULT_GL_VERSION_MAJOR );
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, DEFAULT_GL_VERSION_MINOR );
    glfwWindowHint( GLFW_VISIBLE, 0 );
    context = glfwCreateWindow( 640, 480, "", NULL, player->parent_context );
    if( context == NULL )
        goto LBL_FAILED;
    glfwMakeContextCurrent( context );

    egl_image = egl_create_image_from_va( &surface, player->video_va_display, player->video_cc->width, player->video_cc->height );
    if( egl_image == EGL_NO_IMAGE )
        goto LBL_FAILED;

    glGenTextures( 1, &player->video_texture );
    glBindTexture( GL_TEXTURE_2D, player->video_texture );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, egl_image  );

    packet = NULL;
    frame = av_frame_alloc();
    for(;;)
    {
        packet = (AVPacket*) queue_pop( player->video_packet_queue );
        if( packet == NULL )
            goto LBL_TERMINATE;
LBL_SEND_PACKET:
        r = avcodec_send_packet( player->video_cc, packet );
        if( r == 0 )
        {
            av_packet_free( &packet );
            continue;
        }
        if( r != AVERROR(EAGAIN) )
            goto LBL_FAILED;
LBL_RECEIVE_FRAME:
        r = avcodec_receive_frame( player->video_cc, frame );
        if( r == AVERROR(EAGAIN) )
            goto LBL_SEND_PACKET;
        if( r != 0 )
            goto LBL_FAILED;

        frame->pts = frame->pts * 1./player->clock_speed;
        //   frame->pts = frame->pts * 0.5;

        double clock = clock_seconds( player->clock_start_time );
        double pts_secs = pts_seconds( frame, player->video_stream );
        double delta = pts_secs - clock - vaapi_delay;
        char skip;
        static int skipped = 0;

        if( fabs(delta) < 0.01 )
            skip = 0;
        else if( delta > 0. )
        {
            skip = 0;
            usleep( (uint32_t) (delta * 1000000.) );
        }
        else
            skip = 1;

        if( skip )
        {
            ++skipped;
            goto LBL_RECEIVE_FRAME;
        }
        vaapi_delay = clock_seconds( av_gettime() );
        va_rectangle.x = 0;
        va_rectangle.y = 0;
        va_rectangle.width = player->video_cc->width;
        va_rectangle.height = player->video_cc->height;
        va_pipeline_param_buffer.surface = (VASurfaceID) (uintptr_t) frame->data[3];
        va_pipeline_param_buffer.surface_region = &va_rectangle;
        va_pipeline_param_buffer.output_region = &va_rectangle;
        va_status = vaCreateBuffer( player->video_va_display, player->video_va_context, VAProcPipelineParameterBufferType, sizeof(VAProcPipelineParameterBuffer), 1, &va_pipeline_param_buffer, &va_pipeline_param_buf_id );
        if( va_status != VA_STATUS_SUCCESS )
            goto LBL_FAILED;

        va_status = vaBeginPicture( player->video_va_display, player->video_va_context, surface );
        if( va_status != VA_STATUS_SUCCESS )
            goto LBL_FAILED;
        va_status = vaRenderPicture( player->video_va_display, player->video_va_context, &va_pipeline_param_buf_id, 1 );
        if( va_status != VA_STATUS_SUCCESS )
            goto LBL_FAILED;
        va_status = vaEndPicture( player->video_va_display, player->video_va_context );
        if( va_status != VA_STATUS_SUCCESS )
            goto LBL_FAILED;

        vaDestroyBuffer( player->video_va_display, va_pipeline_param_buf_id );
        vaapi_delay = clock_seconds( av_gettime() ) - vaapi_delay;

        goto LBL_RECEIVE_FRAME;
    }
LBL_TERMINATE:
    av_frame_free( &frame );
    if( packet )
        av_packet_unref( packet );
    vaDestroySurfaces( player->video_va_display, &surface, 1 );
    eglDestroyImage( eglGetCurrentDisplay(), egl_image );
    glDeleteTextures( 1, &player->video_texture );
    player->video_texture = 0;
    glfwMakeContextCurrent( NULL );
    glfwDestroyWindow( context );
    return 0;

LBL_FAILED:
    if( packet )
        av_packet_free( &packet );
    if( frame )
        av_frame_free( &frame );
    if( surface != VA_INVALID_SURFACE )
        vaDestroySurfaces( player->video_va_display, &surface, 1 );
    if( egl_image != EGL_NO_IMAGE )
        eglDestroyImage( eglGetCurrentDisplay(), egl_image );
    if( player->video_texture )
        glDeleteTextures( 1, &player->video_texture );
    if( context )
    {
        glfwMakeContextCurrent( NULL );
        glfwDestroyWindow( context );
    }
    return -1;
}



static int player_decode_audio( Player* player )
{
    int r;
    AVPacket* packet;
    AVFrame* frame;
    AVFrame* frame_converted;

    cb_PastreamCallback_data cb_data;
    PaStream* pa_stream;

    pa_stream = NULL;
    packet = NULL;
    frame = NULL;
    frame_converted = NULL;
    cb_data.stream = player->audio_stream;
    cb_data.src_state = src_new( SRC_SINC_FASTEST, 2, NULL );
    cb_data.frame = NULL;
    cb_data.clock_speed = &player->clock_speed;
    cb_data.clock_start_time = &player->clock_start_time;
    cb_data.volume = &player->audio_volume;
    cb_data.frame_queue = queue_alloc( 10, (Queue_free_function) free_frame );
    if( cb_data.frame_queue == NULL )
        goto LBL_FAILED;

    r = Pa_OpenDefaultStream( &pa_stream, 0, 2, paFloat32, 44100, paFramesPerBufferUnspecified, cb_pa_stream, &cb_data );
    if( r != paNoError )
        goto LBL_FAILED;

    r = Pa_StartStream( pa_stream );
    if( r != paNoError )
        goto LBL_FAILED;

    frame = av_frame_alloc();
    for(;;)
    {
        packet = (AVPacket*) queue_pop( player->audio_packet_queue );
        if( packet == NULL )
            goto LBL_TERMINATE;
LBL_SEND_PACKET:
        r = avcodec_send_packet( player->audio_cc, packet );
        if( r == 0 )
        {
            av_packet_free( &packet );
            continue;
        }
        if( r != AVERROR(EAGAIN)  )
            goto LBL_FAILED;
LBL_RECEIVE_FRAME:
        r = avcodec_receive_frame( player->audio_cc, frame );
        if( r == AVERROR(EAGAIN) )
            goto LBL_SEND_PACKET;
        if( r != 0 )
            goto LBL_FAILED;
        // volume

        frame_converted = av_frame_alloc();
        frame_converted->channels = 2;
        frame_converted->channel_layout = AV_CH_LAYOUT_STEREO;
        frame_converted->format = AV_SAMPLE_FMT_FLT;
        frame_converted->sample_rate = 44100;
        frame_converted->pts = ((double) frame->pts) / player->clock_speed;
        r = swr_convert_frame( player->audio_swr, frame_converted, frame );
        if( r < 0 )
            goto LBL_FAILED;


        queue_push( cb_data.frame_queue, frame_converted );
        frame_converted = NULL;
        goto LBL_RECEIVE_FRAME;
    }
LBL_TERMINATE:
    Pa_StopStream( pa_stream );
    Pa_CloseStream( pa_stream );
    av_frame_free( &frame );

    queue_free( &cb_data.frame_queue );
    if( frame_converted )
        av_frame_free( &frame_converted );
    if( cb_data.frame )
        av_frame_free( &cb_data.frame );
    src_delete( cb_data.src_state );
    return 0;


LBL_FAILED:
    if( pa_stream )
        Pa_CloseStream( pa_stream );
    queue_flush( player->audio_packet_queue );
    if( frame_converted )
        av_frame_free( &frame_converted );
    if( frame )
        av_frame_free( &frame );
    if( packet )
        av_packet_free( &packet );
    if( cb_data.frame_queue )
        queue_free( &cb_data.frame_queue );
    if( cb_data.frame )
        av_frame_free( &cb_data.frame );
    if( cb_data.src_state )
        src_delete( cb_data.src_state );
    return -1;
}
