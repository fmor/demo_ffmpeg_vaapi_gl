#version 330 core

attribute vec3 a_position;
attribute vec2 a_uv;
attribute vec3 a_normal;



uniform mat4 u_mvp;
uniform mat4 u_model;

varying vec3 v_position;
varying vec3 v_normal;
varying vec2 v_uv;

void main()
{
    vec4 position;
    position = vec4( a_position, 1.0 );

    v_position = (u_model * position).xyz;
    v_normal   = mat3(transpose(inverse(u_model))) * a_normal;
    v_uv =   a_uv;

    gl_Position = u_mvp * position;
}


