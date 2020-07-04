#ifndef INCLUDE_RENDER
#define INCLUDE_RENDER

#include "data.h"

void randomize_light( Context* ctx, int id );

void render_scene( Context* ctx );
void render_room( Context* context );
void render_wall( Context* ctx, mat4 parent_transform );
void render_light( Context* context, mat4 mvp, mat4 model, int id );
void render_video( Context* context, mat4 mvp, mat4 model, GLint texture );
void render_tv_snow( Context* ctx, mat4 mvp, mat4 model );
void render_plane( Context* ctx, mat4 mvp, mat4 model, vec4 color );
void render_ground( Context* ctx, mat4 mvp, mat4 model );
void render_cube_color( Context* context, mat4 mvp, mat4 model, vec4 color );

#endif
