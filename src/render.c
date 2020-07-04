//
#include "render.h"
#include "config.h"

#include <stdlib.h>

#include "draw.h"
#include "util.h"

void randomize_light( Context* ctx, int id )
{

    float power = (rand() % 101 ) / 100.f;
    float hue = (rand() % 360 );
    float sat = 0.5f  + 0.5f*power;
    float val = 0.5f + 0.5f * power;
    sat = 1.f;
    float r;
    float g;
    float b;
    hsv_2_rgb( hue, sat, val,  &r, &g, &b  );


    ctx->lights[id].color[0] = r;
    ctx->lights[id].color[1] = g;
    ctx->lights[id].color[2] = b;
    ctx->lights[id].power = 32.f + 32.f * power ;
    ctx->lights[id].linear = 0.01f + 0.02f / power ;
    ctx->lights[id].quadratic = 0.01f + 0.01f / power ;
    ctx->lights[id].position[0] = (rand() % (ROOM_X-3) ) - ROOM_X/2.f + 2.0f;
    ctx->lights[id].position[1] = (rand() % (ROOM_Y-3) ) + 1.0f;
    ctx->lights[id].position[2] =  -ROOM_Z/2.f -1.f;
    ctx->lights_data[id].scale = power*2.0f + 2.0f;
    ctx->lights_data[id].velocity = (rand() % 61 )*power + 7.f;
    ctx->lights_data[id].texture = ctx->texture_lights[ ( rand() % 3 ) ];


    /*
    ctx->lights[id].position[0] = 0;
    ctx->lights[id].position[1] = 1;
    ctx->lights[id].position[2] = 15.0f;
    ctx->lights_velocity[id] =  0.000001f;
    */
}
static int sort_by_distance_inv( const void* a, const void* b )
{
    Light_data* la = *((Light_data**) a);
    Light_data* lb = *((Light_data**) b);
    return (int) lb->distance_to_camera - la->distance_to_camera;
}


void render_scene(Context* ctx)
{
    mat4 mat_mvp;

    // Draw videos
    for( int i = 0; i < ctx->videos_count; ++i )
    {
        Video* video;
        video = &ctx->videos[i];
        glm_mat4_mul( ctx->mat_vp, video->transform, mat_mvp );
        if( video->player )
            render_video( ctx, mat_mvp, video->transform, video->player->video_texture );
        else
            render_tv_snow( ctx, mat_mvp, video->transform );

    }

    // Draw room
    render_room( ctx );


    // Draw lights
    for( int i = 0; i < ctx->lights_count; ++i )
    {
        Light* l;
        Light_data* la;
        vec3 scale;
        l = &ctx->lights[i];
        la = &ctx->lights_data[i];
        glm_mat4_identity( la->transform);

        l->position[2] += ctx->delta_time *  la->velocity;
        if( l->position[2] > (ROOM_Z/2.f+1.f) )
            randomize_light( ctx, i );

        scale[0] = ctx->lights_data[i].scale;
        scale[1] = ctx->lights_data[i].scale;
        scale[2] = ctx->lights_data[i].scale;

        la->distance_to_camera = mat4_billboard( la->transform, ctx->camera_position, l->position );
        glm_scale( la->transform, scale );
        glm_rotate( la->transform, glm_rad( (rand() % 360))  ,  VEC3_UNIT_Z);


    }

    Light_data* id_list[LIGHTS_COUNT];
    for( int i = 0; i < LIGHTS_COUNT; ++i )
        id_list[i] = &ctx->lights_data[i];
    qsort( &id_list[0], LIGHTS_COUNT, sizeof(Light_data*), sort_by_distance_inv );
    glDisable( GL_DEPTH_TEST );
    glEnable( GL_BLEND );
    glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
    for( int i = 0;  i < LIGHTS_COUNT; ++i )
    {
        Light_data* la = id_list[i];
        glm_mat4_mul( ctx->mat_vp, la->transform, mat_mvp );
        render_light( ctx, mat_mvp, la->transform, la->index );
    }
    glEnable( GL_DEPTH_TEST );
    glDisable( GL_BLEND );


}
void render_wall( Context* ctx, mat4 parent_transform )
{

    mat4 m;
    mat4 mvp;
    mat4 tmp;
    vec3 position;
    static float velocity[ROOM_Y*ROOM_Z];
    static char b = 1;
    if( b )
    {
        b=  0;
        for( int i = 0; i < ROOM_Y*ROOM_Z; ++i )
        {
            velocity[i] = (rand() % 16) * 0.1f;
        }

    }
    vec4 color;
    memcpy( color, COLOR_WHITE, sizeof(vec4) );


    for( int y = 0; y < ROOM_Y; ++y )
    {
        for( int z = 0; z < ROOM_Z; ++z )
        {
            position[0] = ( sinf(ctx->time*velocity[y*ROOM_Z + z]) + 1.0f) / 4.f;
            position[1] = y;
            position[2] = z;

            glm_mat4_identity( m );
            glm_translate( m, position );

            glm_mat4_mul( parent_transform, m, tmp );
            glm_mat4_mul( ctx->mat_vp, tmp, mvp );

            render_cube_color( ctx, mvp, tmp, color );
        }
    }

}


void render_room(Context* ctx )
{
    mat4 parent;

    mat4 mvp;
    mat4 model;

    glm_mat4_identity( parent );
    glm_translate( parent, (vec3) {0, 0, -ROOM_Z/2.f} );

    // Ground
    glm_mat4_identity(model);
    glm_rotate( model, glm_rad(-90), VEC3_UNIT_X );
    glm_scale( model, (vec3){ ROOM_X, ROOM_Z, 1.0f} );
    glm_mat4_mul( ctx->mat_vp, model, mvp );
    render_ground( ctx, mvp, model );

    // Top
    glm_mat4_identity(model);
    glm_translate( model, (vec3) { 0.f, ROOM_Y, 0.f} );
    glm_rotate( model, glm_rad(90), VEC3_UNIT_X );
    glm_scale( model, (vec3){ ROOM_X, ROOM_Z, 1.f} );
    glm_mat4_mul( ctx->mat_vp, model, mvp );
    render_ground( ctx, mvp, model );

    // Front
    glm_mat4_identity(model);
    glm_translate( model, (vec3) { 0.f, ROOM_Y/2.f, -ROOM_Z/2.0f} );
    glm_scale( model, (vec3){ ROOM_X, ROOM_Y, 1.0f} );
    glm_mat4_mul( ctx->mat_vp, model, mvp );
    render_plane( ctx, mvp, model, (vec4){ 1.0, 1.0, 1.0, 1.0} );

    // Back
    glm_mat4_identity(model);
    glm_translate( model, (vec3) { 0.f, ROOM_Y/2.f, ROOM_Z/2.f}  );
    glm_rotate( model, glm_rad(180), VEC3_UNIT_X );
    glm_scale( model, (vec3){ ROOM_X, ROOM_Y, 1.0f} );
    glm_mat4_mul( ctx->mat_vp, model, mvp );
    render_plane( ctx, mvp, model, (vec4){ 1.0, 1.0, 1.0, 1.0} );

    // Left
    glm_mat4_identity(model);
    glm_translate( model, (vec3){ ROOM_X/2.f, 0.5f, -ROOM_Z/2.0f + 0.5f} );
    render_wall( ctx, model );

    // Right
    glm_mat4_identity(model);
    glm_translate( model, (vec3){ -ROOM_X/2.f, 0.5f, -ROOM_Z/2.0f + 0.5f} );
    render_wall( ctx, model );
}

void render_light(Context* ctx, mat4 mvp, mat4 model, int id)
{
    glUniform1i( ctx->shader.unif_object_type, OBJECT_TYPE_LIGHT);
    glUniformMatrix4fv( ctx->shader.unif_mvp, 1, GL_FALSE, (const GLfloat*) mvp );
    glUniformMatrix4fv( ctx->shader.unif_model, 1, GL_FALSE, (const GLfloat*) model );
    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, ctx->lights_data[id].texture );
    glUniform1i( ctx->shader.unif_texture, 0 );
    glUniform4fv( ctx->shader.unif_color, 1, ctx->lights[id].color );
    draw_quad( ctx );
}

void render_video(Context* context, mat4 mvp, mat4 model, GLint texture)
{
    glUniform1i( context->shader.unif_object_type, OBJECT_TYPE_VIDEO );
    glUniformMatrix4fv( context->shader.unif_mvp, 1, GL_FALSE, (const GLfloat*) mvp );
    glUniformMatrix4fv( context->shader.unif_model, 1, GL_FALSE, (const GLfloat*) model );
    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, texture );
    glUniform1i( context->shader.unif_texture, 0 );
    draw_quad( context );
}

void render_tv_snow(Context* ctx, mat4 mvp, mat4 model)
{
    glUniform1i( ctx->shader.unif_object_type, OBJECT_TYPE_TV_SNOW);
    glUniformMatrix4fv( ctx->shader.unif_mvp, 1, GL_FALSE, (const GLfloat*) mvp );
    glUniformMatrix4fv( ctx->shader.unif_model, 1, GL_FALSE, (const GLfloat*) model );
    draw_quad( ctx );
}

void render_plane(Context* ctx, mat4 mvp, mat4 model, vec4 color)
{
    glUniform1i( ctx->shader.unif_object_type, OBJECT_TYPE_PLANE);
    glUniformMatrix4fv( ctx->shader.unif_mvp,1 , GL_FALSE, (const GLfloat*) mvp );
    glUniformMatrix4fv( ctx->shader.unif_model, 1, GL_FALSE, (const GLfloat*) model );
    glUniform4fv( ctx->shader.unif_color, 1, color );
    draw_quad( ctx );
}

void render_ground(Context* ctx, mat4 mvp, mat4 model)
{
    glUniform1i( ctx->shader.unif_object_type, OBJECT_TYPE_GROUND );
    glUniformMatrix4fv( ctx->shader.unif_mvp,1 , GL_FALSE, (const GLfloat*) mvp );
    glUniformMatrix4fv( ctx->shader.unif_model, 1, GL_FALSE, (const GLfloat*) model );
    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, ctx->texture_ground );
    glUniform1i( ctx->shader.unif_texture, 0 );
    draw_quad( ctx );
}

void render_cube_color(Context* context, mat4 mvp, mat4 model,  vec4 color )
{
    glUniform1i( context->shader.unif_object_type, OBJECT_TYPE_CUBE );
    glUniformMatrix4fv( context->shader.unif_mvp, 1, GL_FALSE, (const GLfloat*) mvp );
    glUniformMatrix4fv( context->shader.unif_model, 1, GL_FALSE, (const GLfloat*) model );
    glUniform4fv( context->shader.unif_color, 1, color );
    draw_cube( context );
}




