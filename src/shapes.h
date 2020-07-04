#ifndef INCLUDE_SHAPES
#define INCLUDE_SHAPES

#include <GL/eglew.h>

typedef struct
{
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    int    nb_elements;
} Shape;


void load_fullscreen( Shape* shape );
void load_quad( Shape* shape );
void load_cube( Shape* shape );
void load_skybox( Shape* shape );

void shape_unload( Shape* shape );

#endif
