#include "draw.h"

#include <stdlib.h>

void draw_fullscreen(Context* context)
{
    glBindVertexArray( context->shape_fullscreen.vao );
    glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
    glBindVertexArray( 0 );
}

void draw_quad( Context* ctx )
{
    glBindVertexArray( ctx->shape_quad.vao );
    glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
    glBindVertexArray( 0 );
}

void draw_cube(Context* ctx)
{
    glBindVertexArray( ctx->shape_cube.vao );
    glDrawArrays( GL_TRIANGLES, 0, 36 );
    glBindVertexArray( 0 );
    glBindTexture( GL_TEXTURE_2D, 0 );
}
