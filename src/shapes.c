#include "shapes.h"

#include <memory.h>

void load_fullscreen(Shape* shape)
{
    GLfloat vertices[] = {
        -1.f,  1.f, 0.f,   0.f, 1.f,  // Top left
         1.f,  1.f, 0.f,   1.f, 1.f,  // Top right
        -1.f, -1.f, 0.f,   0.f, 0.f,  // Bottom left
         1.f, -1.f, 0.f,  1.0f, 0.f   // Bottom right
    };
    GLsizei sz;
    sz = 5 * sizeof(GLfloat);

    glGenBuffers( 1, &shape->vbo );
    glBindBuffer( GL_ARRAY_BUFFER, shape->vbo );
    glBufferData( GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );

    glGenVertexArrays( 1, &shape->vao );
    glBindVertexArray( shape->vao );
    glBindBuffer( GL_ARRAY_BUFFER, shape->vbo );
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sz, (const void*) (0*sizeof(GLfloat)) );
    glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, sz, (const void*) (3*sizeof(GLfloat)) );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glBindVertexArray( 0 );

    shape->ebo = 0;
    shape->nb_elements = 0;
}

void load_quad( Shape* shape )
{

    GLfloat vertices[] = {
        -0.5f,  0.5f,  0.f,   0.0f, 0.0f,   0.f, 0.f, 1.f,  // Top left
        -0.5f, -0.5f,  0.f,   0.0f, 1.0f,   0.f, 0.f, 1.f,  // Bottom left
         0.5f,  0.5f,  0.f,   1.0f, 0.0f,   0.f, 0.f, 1.f,  // Top right
         0.5f, -0.5f,  0.f,   1.0f, 1.0f,   0.f, 0.f, 1.f   // Bottom right
    };

    GLsizei sz;
    sz = 8 * sizeof(GLfloat);

    glGenBuffers( 1, &shape->vbo );
    glBindBuffer( GL_ARRAY_BUFFER, shape->vbo );
    glBufferData( GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );

    glGenVertexArrays( 1, &shape->vao );
    glBindVertexArray( shape->vao );
    glBindBuffer( GL_ARRAY_BUFFER, shape->vbo );
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sz, (const void*) (0*sizeof(GLfloat)) );
    glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, sz, (const void*) (3*sizeof(GLfloat)) );
    glVertexAttribPointer( 2, 3, GL_FLOAT, GL_FALSE, sz, (const void*) (5*sizeof(GLfloat)) );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glBindVertexArray( 0 );

    shape->ebo = 0;
    shape->nb_elements = 0;
}

void load_cube( Shape* shape )
{
    float vertices[] = {
        // positions / uv / normal
        // back // NEGATIVE_Z
         0.5f,  0.5f, -0.5f,  1.f, 0.f,  0.f, 0.f, -1.f,
         0.5f, -0.5f, -0.5f,  1.f, 1.f,  0.f, 0.f, -1.f,
        -0.5f,  0.5f, -0.5f,  0.f, 0.f,  0.f, 0.f, -1.f,
        -0.5f, -0.5f, -0.5f,  0.f, 1.f,  0.f, 0.f, -1.f,
        -0.5f,  0.5f, -0.5f,  0.f, 0.f,  0.f, 0.f, -1.f,
         0.5f, -0.5f, -0.5f,  1.f, 1.f,  0.f, 0.f, -1.f,

        // Left // NEGATIVE_X
        -0.5f,  0.5f, -0.5f,  0.f, 0.f,  -1.f, 0.f, 0.f,
        -0.5f, -0.5f, -0.5f,  0.f, 1.f,  -1.f, 0.f, 0.f,
        -0.5f, -0.5f,  0.5f,  1.f, 1.f,  -1.f, 0.f, 0.f,
        -0.5f,  0.5f,  0.5f,  1.f, 0.f,  -1.f, 0.f, 0.f,
        -0.5f,  0.5f, -0.5f,  0.f, 0.f,  -1.f, 0.f, 0.f,
        -0.5f, -0.5f,  0.5f,  1.f, 1.f,  -1.f, 0.f, 0.f,

        // Right POSITIVE_X
         0.5f,  0.5f,  0.5f,  0.f, 0.f,  1.f, 0.f, 0.f,
         0.5f, -0.5f,  0.5f,  0.f, 1.f,  1.f, 0.f, 0.f,
         0.5f, -0.5f, -0.5f,  1.f, 1.f,  1.f, 0.f, 0.f,
         0.5f,  0.5f, -0.5f,  1.f, 0.f,  1.f, 0.f, 0.f,
         0.5f,  0.5f,  0.5f,  0.f, 0.f,  1.f, 0.f, 0.f,
         0.5f, -0.5f, -0.5f,  1.f, 1.f,  1.f, 0.f, 0.f,

        // Front // POSITIVE_Z
        -0.5f,  0.5f,  0.5f,  0.f, 0.f,  0.f, 0.f, 1.f,
        -0.5f, -0.5f,  0.5f,  0.f, 1.f,  0.f, 0.f, 1.f,
         0.5f,  0.5f,  0.5f,  1.f, 0.f,  0.f, 0.f, 1.f,
         0.5f,  0.5f,  0.5f,  1.f, 0.f,  0.f, 0.f, 1.f,
        -0.5f, -0.5f,  0.5f,  0.f, 1.f,  0.f, 0.f, 1.f,
         0.5f, -0.5f,  0.5f,  1.f, 1.f,  0.f, 0.f, 1.f,

        //  Top POSITIIVE Y
        -0.5f,  0.5f, -0.5f,  0.f, 0.f,  0.f, 1.f, 0.f,
        -0.5f,  0.5f,  0.5f,  0.f, 1.f,  0.f, 1.f, 0.f,
         0.5f,  0.5f, -0.5f,  1.f, 0.f,  0.f, 1.f, 0.f,
         0.5f,  0.5f,  0.5f,  1.f, 1.f,  0.f, 1.f, 0.f,
         0.5f,  0.5f, -0.5f,  1.f, 0.f,  0.f, 1.f, 0.f,
        -0.5f,  0.5f,  0.5f,  0.f, 1.f,  0.f, 1.f, 0.f,

        // Bottom NEGATIVE_Y
         0.5f, -0.5f, -0.5f,  1.f, 0.f,  0.f, -1.f, 0.f,
        -0.5f, -0.5f,  0.5f,  0.f, 1.f,  0.f, -1.f, 0.f,
        -0.5f, -0.5f, -0.5f,  0.f, 0.f,  0.f, -1.f, 0.f,
        -0.5f, -0.5f,  0.5f,  0.f, 1.f,  0.f, -1.f, 0.f,
         0.5f, -0.5f, -0.5f,  1.f, 0.f,  0.f, -1.f, 0.f,
         0.5f, -0.5f,  0.5f,  1.f, 1.f,  0.f, -1.f, 0.f
    };

    glGenBuffers( 1, &shape->vbo );
    glBindBuffer( GL_ARRAY_BUFFER, shape->vbo );
    glBufferData( GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );

    GLsizei sz = 8 * sizeof (GLfloat);
    glGenVertexArrays( 1, &shape->vao );
    glBindVertexArray( shape->vao );
    glBindBuffer( GL_ARRAY_BUFFER, shape->vbo );
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sz, (const void*) (0*sizeof(GLfloat)) );
    glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, sz, (const void*) (3*sizeof(GLfloat)) );
    glVertexAttribPointer( 2, 3, GL_FLOAT, GL_FALSE, sz, (const void*) (5*sizeof(GLfloat)) );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glBindVertexArray( 0 );

    shape->ebo = 0;
    shape->nb_elements = 0;
}

void load_skybox( Shape* shape )
{

    float vertices[] = {
        // positions
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };
    glGenVertexArrays( 1, &shape->vao );
    glBindVertexArray( shape->vao );

    glGenBuffers( 1, &shape->vbo );
    glBindBuffer( GL_ARRAY_BUFFER, shape->vbo );
    glBufferData( GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW );
    glEnableVertexAttribArray(0);
    glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 );

    shape->ebo = 0;
    shape->nb_elements = 0;
}

void shape_unload(Shape* shape)
{
    if( shape->vao )
        glDeleteBuffers( 1, &shape->vao );
    if( shape->vbo )
        glDeleteBuffers( 1, &shape->vbo );
    if( shape->ebo )
        glDeleteBuffers( 1, &shape->ebo );
    memset( shape, 0, sizeof(shape) );
}
