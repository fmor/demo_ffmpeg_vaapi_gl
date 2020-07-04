cmake_minimum_required(VERSION 3.8.2)

project( demo C ASM )

set( CMAKE_C_STANDARD_REQUIRED   YES)
set( CMAKE_C_FLAGS "-D__STDC_CONSTANT_MACROS -Wno-zero-as-null-pointer-constant -Wno-old-style-cast -L/usr/local/lib" )
set( CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g -Wall -pg " )
set( CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )
set( CMAKE_VERBOSE_MAKEFILE 1 )


# cglm
option(CGLM_SHARED "Shared build" OFF)
option(CGLM_STATIC "Static build" ON)
option(CGLM_USE_C99 "" OFF)
option(CGLM_USE_TEST "Enable Tests" OFF)
add_subdirectory( externals/cglm-0.7.6 )
link_libraries( cglm)

# glew
include_directories( externals/glew-2.1.0/include )
add_compile_definitions( GLEW_STATIC GLEW_EGL )
link_libraries( EGL )

# glfw
set( GLFW_BUILD_DOCS        OFF CACHE BOOL "" FORCE)
set( GLFW_BUILD_TESTS       OFF CACHE BOOL "" FORCE)
set( GLFW_BUILD_EXAMPLES    OFF CACHE BOOL "" FORCE)
set( GLFW_INSTALL           OFF CACHE BOOL "" FORCE)
add_definitions( -DGLFW_USE_EGL )
include_directories( externals/glfw-3.3.2/include )
add_subdirectory( externals/glfw-3.3.2 )
link_libraries( glfw )

# ffmpeg
#find_package( ffmpeg  REQUIRED )
#link_libraries( ${FFMPEG_LIBRARIES} )
#link_libraries( avformat avcodec swresample swscale avfilter avdevice avutil  )
link_libraries( avfilter avformat avcodec swresample swscale avdevice avutil v4l2 )

# vaapi
link_libraries( va va-glx va-drm  )

# drm
include_directories( "/usr/x86_64-linux-gnu/include" )
# x11
#link_libraries( x11 )


# portaudio
link_libraries( portaudio )

# samplerate
link_libraries( samplerate )

# stb
include_directories( "externals/stb" )

link_libraries( pthread )
link_libraries( GL)
link_libraries( z lzma)


set( SOURCES
    externals/glew-2.1.0/src/glew.c
    externals/stb/stb.c

    src/draw.c
    src/main.c
    src/player.c
    src/queue.c
    src/render.c
    src/recorder.c
    src/resources.s
    src/shapes.c
    src/util.c
)


add_executable( ${PROJECT_NAME} ${SOURCES} )
add_custom_target( copy_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources ${CMAKE_CURRENT_BINARY_DIR}/resources
)
add_dependencies( ${PROJECT_NAME} copy_resources )

