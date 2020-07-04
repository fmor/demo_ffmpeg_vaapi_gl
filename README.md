# demo_ffmpeg_vaapi_gl

This is a demo project for demonstrate integration of hardware decoding and rendering via VAAPI ( libva )
to OpenGL texture and recording at the same time. It is based of the use of EGLImage that act as an handle
between memory space of different api on the gpu, and in this case opengl and vaapi. This result as an zero
copy procedure, if we exclude the conversion to RGBA ( that simplify shader dev ). Rapid explanation of how it
works below.


[![Demo](doc/video.gif)](https://www.youtube.com/watch?v=xNobcTj9Txk)

# Configuration
You may to change the following defines in config.h :

    #define RECORD_FILENAME         "/tmp/video_%02d.mp4"

    #define VIDEO_0                 "/home/fmor/video/lipsync2.mp4"
    #define VIDEO_1                 "/home/fmor/video/savon.mp4"
    #define VIDEO_2                 "/home/fmor/video/star_trek.mp4"
    #define VIDEO_3                 "/home/fmor/video/prologue.mkv"
    #define VIDEO_4                 "/home/fmor/video/titan.mp4"
    #define VIDEO_5                 "/home/fmor/video/loups.mp4"

# Control

* Arrow keys for moving, mouse for camera
* R to start and stop recording, recorded video will be named according RECORD_FILENAME.  The video file will have a h264 video stream and a mp3 audio stream.
* A/Q to quit
* ESC to focus/unfocus mouse


# How it works

## Setup:
* Initialize the libva lib with /dev/dri/renderDxxx device.
* Create a ffmpeg HW device context for vaapi
* Set the ffmpeg HW device vaapi created with the the va display obtainned by vaGetDisplayDRM
* Create and EGL context


## Decode and render to texture :
* Create an AVCodecContext that and set the hw device context
* Create a va surface of pixel format BGRX
* Create an egl image with the va_surface as target ( util.c )
* Create a GL_TEXTURE_2D and bind it with the EGL Image
* Create a va postprocess pipeline in order convert  ( H264 -> GL_RGBA )
* when a ffmpeg AVCodecContext decode a frame  use postprocess pipeline to render to the va_surface BGRX
* Your GL_RGBA texture is ready to be used.

## Encode GL window :
* Create a va surface of pixel format BGRX
* Create an egl image with the va_surface as target ( util.c )
* Create a GL_TEXTURE_2D and bind it with the EGL Image
* Create a dst FrameBuffer and attach it the texture as color attachment
* Create an pool of hw frame context from the hw device context
* Create an AVCodecContext and set the hw device context and the pool of hw frame
* Create a va postprocess pipeline GL_RGBA -> H264/NV12
* for each frame you want to add to the video, pass the source framebuffer you want to render
* Blit the source framebuffer to the dst framebuffer
* Pick a hw frame and render the va surface of the texture to the va surface of the picked avframe using postprocess va pipeline
* Your H264 frame is ready to be send to the encoder.


# Preriquisitorites

* Video file with a least one H264 video stream
* Intel processor that support H264 hardware decoding/encoding
* Linux based operating system
* Ffmpeg must support vaapi en mp3 encoding

# Libraries used

* cglm <https://github.com/recp/cglm>
* ffmpeg <https://ffmpeg.org>
* glew   <http://glew.sourceforge.net>
* glfw   <https://www.glfw.org>
* libva  <https://github.com/intel/libva>
* portaudio <http://portaudio.com>
* samplerate <http://www.mega-nerd.com/SRC>
