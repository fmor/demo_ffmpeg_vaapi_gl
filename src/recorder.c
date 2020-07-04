#include "recorder.h"
#include <libavutil/hwcontext.h>

#include "util.h"
#include "config.h"
#include "time.h"

#define SAMPLE_RATE 44100
#define HW_FRAME_BUFFER_POOL_SIZE   30

static void* recorder_mux( void* data );
static void* recorder_encode_video( void* data );
static void* recorder_encode_audio( void* data );

static int cb_pa_stream( const void *input, void *output, unsigned long frame_count, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData );

Recorder* recorder_create( VADisplay va_display, AVBufferRef* hw_device_context, int width, int height, int fps, const char* filename )
{
    Recorder* rec;
    VAConfigAttrib va_config_attrib;
    VAConfigID va_config_id;
    int r;
    AVCodec* c;
    AVBufferRef* hw_frames_ctx;
    AVDictionary* options;
    AVStream* stream;
    char buffer[1024];
    VAProcPipelineParameterBuffer*  pipeline_parameter_buffer;
    struct timespec ts;
    GLint old_fbo_draw;

    va_config_id = VA_INVALID_ID;
    c = NULL;
    hw_frames_ctx = NULL;
    options = NULL;
    stream = NULL;
    pipeline_parameter_buffer = NULL;
    old_fbo_draw = 0;

    rec = (Recorder*) calloc( 1, sizeof(Recorder) );
    if( rec == NULL )
        goto LBL_FAILED;



    // GL
    rec->video_fbo_color_egl_image = egl_create_image_from_va( &rec->video_fbo_color_va_surface, va_display, width, height  );
    if( rec->video_fbo_color_egl_image == EGL_NO_IMAGE )
        goto LBL_FAILED;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_fbo_draw);
    glGenTextures( 1, &rec->video_fbo_color );
    glBindTexture( GL_TEXTURE_2D, rec->video_fbo_color );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, rec->video_fbo_color_egl_image );
    glBindTexture( GL_TEXTURE_2D, 0 );

    glGenFramebuffers( 1, &rec->video_fbo );
    glBindFramebuffer( GL_FRAMEBUFFER, rec->video_fbo );
    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rec->video_fbo_color, 0 );
    GLuint attachments[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers( 1, attachments  );
    if( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
        goto LBL_FAILED;
    glBindFramebuffer( GL_FRAMEBUFFER, old_fbo_draw );


    // Video
    rec->video_va_display = va_display;
    memset( &va_config_attrib, 0, sizeof(VAConfigAttrib) );
    va_config_attrib.type = VAConfigAttribRTFormat;
    r = vaGetConfigAttributes( va_display, VAProfileNone, VAEntrypointVideoProc, &va_config_attrib, 1 );
    if( r != VA_STATUS_SUCCESS )
        goto LBL_FAILED;
    r = (va_config_attrib.value & VA_RT_FORMAT_RGB32) == 0;
    if( r )
        goto LBL_FAILED;

    r = vaCreateConfig( va_display, VAProfileNone, VAEntrypointVideoProc, &va_config_attrib, 1, &va_config_id );
    if( r != VA_STATUS_SUCCESS )
        goto LBL_FAILED;

    r = vaCreateContext( va_display, va_config_id, width, height, VA_PROGRESSIVE, NULL, 0, &rec->video_va_context );
    if( r != VA_STATUS_SUCCESS )
        goto LBL_FAILED;

    r = vaCreateBuffer( va_display, va_config_id,  VAProcPipelineParameterBufferType, sizeof(VAProcPipelineParameterBuffer), 1, NULL, &rec->video_va_pipeline_parameters_buffer_id );
    if( r != VA_STATUS_SUCCESS )
        goto LBL_FAILED;
    r = vaMapBuffer( rec->video_va_display, rec->video_va_pipeline_parameters_buffer_id, (void**) &pipeline_parameter_buffer );
    pipeline_parameter_buffer->surface = rec->video_fbo_color_va_surface;
    pipeline_parameter_buffer->surface_region = NULL;
    pipeline_parameter_buffer->output_region = NULL;
    r = vaUnmapBuffer( rec->video_va_display, rec->video_va_pipeline_parameters_buffer_id );


    hw_frames_ctx = av_hwframe_ctx_alloc( hw_device_context );
    if( hw_frames_ctx == NULL )
        goto LBL_FAILED;
    ( (AVHWFramesContext*) hw_frames_ctx->data)->format = AV_PIX_FMT_VAAPI;
    ( (AVHWFramesContext*) hw_frames_ctx->data)->sw_format = AV_PIX_FMT_NV12;
    ( (AVHWFramesContext*) hw_frames_ctx->data)->width = width;
    ( (AVHWFramesContext*) hw_frames_ctx->data)->height = height;
    ( (AVHWFramesContext*) hw_frames_ctx->data)->initial_pool_size = HW_FRAME_BUFFER_POOL_SIZE;
    r = av_hwframe_ctx_init( hw_frames_ctx );
    if( r < 0 )
        goto LBL_FAILED;
    c = avcodec_find_encoder_by_name("h264_vaapi");
    if( c == NULL )
        goto LBL_FAILED;
    rec->video_cc = avcodec_alloc_context3( c );
    if( rec->video_cc == NULL )
        goto LBL_FAILED;
    rec->video_cc->hw_frames_ctx = av_buffer_ref(hw_frames_ctx );
    av_buffer_unref( &hw_frames_ctx );
    rec->video_cc->pix_fmt = AV_PIX_FMT_VAAPI;
    rec->video_cc->width = width;
    rec->video_cc->height = height;
    rec->video_cc->time_base = (AVRational) {1,fps};
    rec->video_cc->framerate = (AVRational) {fps, 1};
    rec->video_cc->gop_size = 4*fps;
    rec->video_cc->max_b_frames = 16;
    rec->video_cc->global_quality = 20;
    rec->video_cc->profile = FF_PROFILE_H264_HIGH;
    rec->video_cc->compression_level = 0;
    rec->video_cc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    r = avcodec_open2( rec->video_cc, c,  &options );
    av_dict_free( &options );
    if( r < 0 )
        goto LBL_FAILED;
    rec->video_frame_queue = queue_alloc( HW_FRAME_BUFFER_POOL_SIZE, (Queue_free_function) free_frame );
    if( rec->video_frame_queue == NULL )
        goto LBL_FAILED;

    // Audio
    c = avcodec_find_encoder( AV_CODEC_ID_MP3 );
    if( c == NULL )
        goto LBL_FAILED;
    rec->audio_cc = avcodec_alloc_context3( c );
    if( rec->audio_cc == NULL )
        goto LBL_FAILED;
    rec->audio_cc->channels = 2;
    rec->audio_cc->channel_layout = AV_CH_LAYOUT_STEREO;
    rec->audio_cc->sample_fmt = AV_SAMPLE_FMT_FLTP;
    rec->audio_cc->sample_rate = SAMPLE_RATE;
    rec->audio_cc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    r = avcodec_open2( rec->audio_cc, c, &options );
    av_dict_free( &options );
    if( r < 0 )
        goto LBL_FAILED;
    rec->audio_swr = swr_alloc_set_opts( NULL, rec->audio_cc->channel_layout, rec->audio_cc->sample_fmt, rec->audio_cc->sample_rate, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_FLT, SAMPLE_RATE, 0 , NULL );
    if( rec->audio_swr == NULL )
        goto LBL_FAILED;
    r = swr_init( rec->audio_swr );
    if( r < 0 )
        goto LBL_FAILED;
    rec->audio_queue = queue_alloc( 0, (Queue_free_function) free_packet );
    if( r != 0 )
        goto LBL_FAILED;



    // Muxer
    rec->muxer_queue = queue_alloc(  0, (Queue_free_function) free_packet );
    if( rec->muxer_queue == NULL )
        goto LBL_FAILED;
    static int record_count = 0;
    ++record_count;
    snprintf( buffer, 1024, filename, record_count );
    fprintf( stdout, "Recording to filename %s\n", buffer );
    fflush( stdout );
    r = avformat_alloc_output_context2( &rec->fc, NULL, NULL, buffer );
    if( r < 0 )
        goto LBL_FAILED;
    // Muxer / video
    stream = avformat_new_stream( rec->fc, rec->video_cc->codec );
    if( stream == NULL )
        goto LBL_FAILED;
    r = avcodec_parameters_from_context( stream->codecpar, rec->video_cc );
    if( r < 0 )
        goto LBL_FAILED;
    // Muxer / audio
    stream = avformat_new_stream( rec->fc, rec->audio_cc->codec );
    if( stream == NULL )
        goto LBL_FAILED;
    r = avcodec_parameters_from_context( stream->codecpar, rec->audio_cc );
    if( r < 0 )
        goto LBL_FAILED;

    r = avio_open( &rec->fc->pb, rec->fc->url, AVIO_FLAG_WRITE );
    if( r < 0 )
        goto LBL_FAILED;
    r = avformat_write_header( rec->fc, NULL );
    if( r < 0 )
        goto LBL_FAILED;




    // Start
    r = Pa_OpenDefaultStream( &rec->audio_stream, 2, 0, paFloat32, SAMPLE_RATE, paFramesPerBufferUnspecified, cb_pa_stream, rec );
    if( r != paNoError )
        goto LBL_FAILED;

    r = Pa_StartStream( rec->audio_stream );
    if( r != paNoError )
        goto LBL_FAILED;

    clock_gettime( CLOCK_MONOTONIC_RAW, &ts );
    rec->time_seconds_start = ts.tv_sec + (ts.tv_nsec / 1E9);

    r = pthread_create( &rec->muxer_thread, NULL, recorder_mux, rec );
    if( r != 0 )
        goto LBL_FAILED;
    r = pthread_create( &rec->video_thread, NULL, recorder_encode_video, rec );
    if( r != 0 )
        goto LBL_FAILED;
    r = pthread_create( &rec->audio_thread, NULL, recorder_encode_audio, rec );
    if( r != 0 )
        goto LBL_FAILED;
    return rec;

LBL_FAILED:
    if( va_config_id )
        vaDestroyConfig( va_display, va_config_id );

    if( rec )
    {
        // Video
        if( rec->video_fbo )
            glDeleteFramebuffers( 1, &rec->video_fbo );
        if( rec->video_fbo_color )
            glDeleteTextures( 1, &rec->video_fbo_color );
        if( rec->video_fbo_color_egl_image != EGL_NO_IMAGE )
            eglDestroyImage( eglGetCurrentDisplay(), rec->video_fbo_color_egl_image );
        if( rec->video_fbo_color_va_surface != VA_INVALID_SURFACE )
            vaDestroySurfaces( va_display, &rec->video_fbo_color_va_surface, 1 );
        if( rec->video_frame_queue )
            queue_free( &rec->video_frame_queue );
        if( rec->muxer_queue )
            queue_free( &rec->muxer_queue );
        if( rec->video_va_context )
            vaDestroyContext( va_display, rec->video_va_context );
        if( rec->video_va_pipeline_parameters_buffer_id )
            vaDestroyBuffer( va_display, rec->video_va_pipeline_parameters_buffer_id );
        if( rec->video_cc )
            avcodec_free_context( &rec->video_cc );

        // Audio
        if( rec->audio_cc )
            avcodec_free_context( &rec->audio_cc );
        if( rec->audio_swr )
            swr_free( &rec->audio_swr );
        if( rec->audio_queue )
            queue_free( &rec->audio_queue );
        if( rec->audio_stream );
            Pa_CloseStream( rec->audio_stream );

        free( rec );
    }
    if( hw_frames_ctx )
        av_buffer_unref( hw_frames_ctx );
    if( old_fbo_draw )
        glBindFramebuffer( GL_FRAMEBUFFER, old_fbo_draw );
    return NULL;
}
void recorder_free(Recorder** recorder)
{
    intptr_t r;
    Recorder* rec = *recorder;

    queue_push( rec->video_frame_queue, NULL );
    Pa_StopStream( rec->audio_stream );
    Pa_CloseStream( rec->audio_stream );
    queue_push( rec->audio_queue, NULL );

    // Video
    pthread_join( rec->video_thread, (void**) &r );
    rec->video_thread = 0;
    avcodec_free_context( &rec->video_cc );
    vaDestroyBuffer( rec->video_va_display, rec->video_va_pipeline_parameters_buffer_id );
    rec->video_va_pipeline_parameters_buffer_id = 0;
    vaDestroyContext( rec->video_va_display, rec->video_va_context );
    rec->video_va_context = 0;
    queue_free( &rec->video_frame_queue );
    vaDestroyBuffer( rec->video_va_display, rec->video_va_pipeline_parameters_buffer_id );
    rec->video_va_pipeline_parameters_buffer_id = 0;
    glDeleteFramebuffers( 1, &rec->video_fbo );
    rec->video_fbo = 0;
    glDeleteTextures( 1, &rec->video_fbo_color );
    rec->video_fbo_color = 0;
    eglDestroyImage( eglGetCurrentDisplay(), rec->video_fbo_color_egl_image );
    rec->video_fbo_color_egl_image = 0;
    vaDestroySurfaces( rec->video_va_display, &rec->video_fbo_color_va_surface, 1 );
    rec->video_fbo_color_va_surface = 0;

    // Audio
    rec->audio_stream = NULL;
    pthread_join( rec->audio_thread, (void**) &r );
    avcodec_free_context( &rec->audio_cc );
    swr_free( &rec->audio_swr );
    queue_free( &rec->audio_queue );

    // Muxer
    pthread_join( rec->muxer_thread, (void**) &r );
    rec->muxer_thread = 0;
    av_write_trailer( rec->fc );
    avio_close( rec->fc->pb );
    avformat_free_context( rec->fc );
    rec->fc = NULL;
    queue_free( &rec->muxer_queue );

    free( rec );
    *recorder = NULL;
}

void recorder_add_video_frame(Recorder* rec, GLuint read_fbo)
{
    int r;
    AVFrame* frame;
    int w;
    int h;
    GLint old_fbo_draw;
    GLint old_fbo_read;
    struct timespec ts;
    float time_in_seconds;

    clock_gettime( CLOCK_MONOTONIC_RAW, &ts );
    time_in_seconds = ts.tv_sec + ts.tv_nsec/1E9 - rec->time_seconds_start;
    float time_next_frame = ((float) rec->video_frame_count) / ( (float) rec->video_cc->framerate.num);
    if( time_in_seconds < time_next_frame )
            return;
    ++rec->video_frame_count;

    w = rec->video_cc->width;
    h = rec->video_cc->height;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_fbo_read);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_fbo_draw);
    glBindFramebuffer( GL_READ_FRAMEBUFFER, read_fbo );
    glBindFramebuffer( GL_DRAW_FRAMEBUFFER, rec->video_fbo );
    glBlitFramebuffer(  0,0,w,h,  0,h,w,0,  GL_COLOR_BUFFER_BIT, GL_LINEAR );
    glBindFramebuffer( GL_READ_FRAMEBUFFER, old_fbo_read );
    glBindFramebuffer( GL_DRAW_FRAMEBUFFER, old_fbo_draw );

    frame = av_frame_alloc();
    r = av_hwframe_get_buffer( rec->video_cc->hw_frames_ctx, frame, 0 );
    if( r < 0 )
        goto LBL_FAILED;

    r = vaBeginPicture( rec->video_va_display, rec->video_va_context, (VASurfaceID)(uintptr_t) frame->data[3] );
    if( r != VA_STATUS_SUCCESS )
        goto LBL_FAILED;

    r = vaRenderPicture( rec->video_va_display, rec->video_va_context, &rec->video_va_pipeline_parameters_buffer_id, 1 );
    if( r != VA_STATUS_SUCCESS )
        goto LBL_FAILED;

    r = vaEndPicture( rec->video_va_display, rec->video_va_context );
    if( r != VA_STATUS_SUCCESS )
        goto LBL_FAILED;

    time_in_seconds *= rec->fc->streams[0]->time_base.den;
    modff( time_in_seconds, &time_in_seconds );
    frame->pts = time_in_seconds;
    queue_push( rec->video_frame_queue, frame );
    return;

LBL_FAILED:
    if( frame )
        av_frame_free( &frame );
    fprintf( stderr, "Recorder failed to push surface\n");
}
static void* recorder_mux( void* data )
{
    int r;
    Recorder* rec;
    AVPacket* packet;
    unsigned null_count;
    rec = (Recorder*) data;
    packet = NULL;
    null_count = 0;
    for(;;)
    {
        packet = (AVPacket*) queue_pop( rec->muxer_queue );
        if( packet == NULL )
        {
            ++null_count;
            if( null_count == rec->fc->nb_streams )
                break;
            else
                continue;
        }

        r = av_interleaved_write_frame( rec->fc, packet );
        if( r != 0 )
        {
            fputs("Failed to write packet\n", stderr );
            fflush( stderr );
            exit(-1);
        }
    }
    return (void*) 0;

LBL_FAILED:
    if( packet )
        av_packet_free( &packet );
    if( rec->fc )
    {
        if( rec->fc->pb )
            avio_close( rec->fc->pb );
        avformat_free_context( rec->fc );
        rec->fc  = NULL;
    }
    exit(1);
    return (void*) -1;

}
static void* recorder_encode_video( void* data )
{
    int r;
    AVFrame* frame;
    AVPacket packet;
    Recorder* rec;


    frame = NULL;
    rec = (Recorder*) data;
    av_init_packet( &packet );
    for( ;; )
    {
        frame = queue_pop( rec->video_frame_queue );
        r = avcodec_send_frame( rec->video_cc, frame );
        if( frame )
            av_frame_free( &frame );
        if( r < 0 )
            goto LBL_FAILED;
        for(;;)
        {
            r = avcodec_receive_packet( rec->video_cc, &packet );
            if( r == 0 )
            {
                packet.stream_index = 0;
                queue_push( rec->muxer_queue,  av_packet_clone(&packet)  );
                av_packet_unref( &packet );
            }
            else if( r == AVERROR(EAGAIN) )
                break;
            else if( r == AVERROR_EOF )
                goto LBL_EOF;
            else
                goto LBL_FAILED;
        }
    }
LBL_EOF:
    queue_push( rec->muxer_queue, NULL );
    return (void*) 0;
LBL_FAILED:
    av_packet_unref( &packet );
    return (void*) -1;
}


static void* recorder_encode_audio( void* data )
{
    int r;
    Recorder* rec;
    AVPacket packet;
    AVFrame* frame;

    rec = (Recorder*) data;
    av_init_packet( &packet );

    for(;;)
    {
        frame = queue_pop( rec->audio_queue );
        r = avcodec_send_frame( rec->audio_cc, frame );
        if( frame )
            av_frame_free( &frame );
        if( r < 0  )
            goto LBL_FAILED;
        for(;;)
        {
            r = avcodec_receive_packet( rec->audio_cc, &packet );
            if( r == 0 )
            {
                packet.stream_index = 1;
                queue_push( rec->muxer_queue, av_packet_clone(&packet) );
                av_packet_unref( &packet );
            }
            else if( r == AVERROR(EAGAIN) )
                break;
            else if( r == AVERROR_EOF )
                goto LBL_EOF;
            else
                goto LBL_FAILED;
        }
    }
LBL_EOF:
    queue_push( rec->muxer_queue, NULL );
    return (void*) 0;
LBL_FAILED:
    av_packet_unref( &packet );
    return (void*) -1;
}


static int cb_pa_stream( const void *input, void *output, unsigned long frame_count, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData )
{
    int r;
    Recorder* rec;
    AVFrame* frame_input;
    AVFrame* frame_output;
    struct timespec ts;

    rec = (Recorder*) userData;
    clock_gettime( CLOCK_MONOTONIC_RAW, &ts  );
    float t = ts.tv_sec + ts.tv_nsec/1E9 - rec->time_seconds_start;

    frame_input = av_frame_alloc();
    frame_input->channel_layout = AV_CH_LAYOUT_STEREO;
    frame_input->sample_rate = SAMPLE_RATE;
    frame_input->format = AV_SAMPLE_FMT_FLT;
    frame_input->data[0] = input;
    frame_input->nb_samples = frame_count;

    frame_output = av_frame_alloc();
    frame_output->channel_layout = rec->audio_cc->channel_layout;
    frame_output->sample_rate = rec->audio_cc->sample_rate;
    frame_output->format = rec->audio_cc->sample_fmt;


    static int pts = 0;
    frame_output->pts = pts;
    pts += frame_count;

    /*
    fprintf( stdout, "pts=%d\n", pts );
    fflush( stdout );
    */

    r = swr_convert_frame( rec->audio_swr, frame_output, frame_input );
    if( r < 0 )
        exit(-1);

    av_frame_free( &frame_input );

    queue_push( rec->audio_queue, frame_output );
    return paContinue;
}

