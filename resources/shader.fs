#version 330 core

#define OBJECT_TYPE_LIGHT       0
#define OBJECT_TYPE_VIDEO       1
#define OBJECT_TYPE_CUBE        2
#define OBJECT_TYPE_PLANE       3
#define OBJECT_TYPE_TV_SNOW     4
#define OBJECT_TYPE_GROUND      5

#define LIGHTS_COUNT            4

struct Environment
{
    vec4  ambient;
    vec4  fog_color;
    float fog_density;
    float time;
};

struct Light
{
    vec4    position;
    vec4    color;
    float   power;
    float   constant;
    float   linear;
    float   quadratic;
};

layout ( location = 0 ) out vec4 FragColor;


uniform sampler2D u_texture;
uniform vec3      u_camera_position;
uniform int       u_object_type;
uniform vec4      u_color;
uniform float     u_time;




varying vec3 v_position;
varying vec3 v_normal;
varying vec2 v_uv;


layout (std140) uniform EnvironmentBlock
{
    Environment environment;
};
layout (std140) uniform LightsBlock
{
    Light lights[LIGHTS_COUNT];
};

vec4 compute_fog( vec4 color, float fog_density, vec4 fog_color, float z );
vec3 compute_light(  Light l, vec3 position, vec3 normal, vec3 camera );
float rand(vec2 co);

void render_object_type_cube();
void render_object_type_light();
void render_object_type_video();
void render_object_type_plane();
void render_tv_snow();
void render_object_type_ground();

void main()
{
    switch( u_object_type )
    {
        case OBJECT_TYPE_LIGHT:     render_object_type_light(); break;
        case OBJECT_TYPE_VIDEO:     render_object_type_video(); break;
        case OBJECT_TYPE_CUBE:      render_object_type_cube();  break;
        case OBJECT_TYPE_PLANE:     render_object_type_plane(); break;
        case OBJECT_TYPE_TV_SNOW:   render_tv_snow();           break;
        case OBJECT_TYPE_GROUND:    render_object_type_ground();break;
        default:
            FragColor = vec4( 1.0, 0.0, 1.0, 1.0 );
    }

}

void render_object_type_ground()
{
    vec4  color;
    vec3  light_color;
    float distance_to_camera;
    vec2  uv;


    uv = v_uv;
    uv.x *= 4.0;
    uv.y *= 16.0;


    color =   texture( u_texture, uv );

    light_color = vec3(0);
    for( int i = 0; i < LIGHTS_COUNT; ++i )
        light_color += compute_light( lights[i], v_position, v_normal, u_camera_position );

    color = (environment.ambient + vec4(light_color,1.0)) * color ;

    distance_to_camera = length( u_camera_position - v_position );
    color = compute_fog( color, environment.fog_density, environment.fog_color, distance_to_camera );

    FragColor = color;
}


void render_tv_snow()
{
    float r;
    vec2 co;
    vec4 color;

    co.x = v_position.x + environment.time;
    co.y = v_position.y + environment.time;

    r = rand( co );
    if( r > 0.5 )
        r = 1.0;
    else
        r = 0.0;
    color = vec4( r, r, r, 1.0 );

    float distance_to_camera = length( u_camera_position - v_position);
    FragColor =  compute_fog( color, environment.fog_density, environment.fog_color, distance_to_camera );
}

void render_object_type_light()
{
    vec3 color = texture( u_texture, v_uv ).rgb;
    float alpha = (color.r + color.g + color.b )/ 3.0;
    if( alpha < 0.4 )
        alpha = alpha;
    color *= u_color.rgb;
    FragColor  = vec4( color, alpha);
}
void render_object_type_video()
{
    float distance_to_camera;
    distance_to_camera = length( u_camera_position - v_position );
    FragColor = texture2D( u_texture, v_uv );
    FragColor = compute_fog( FragColor, environment.fog_density, environment.fog_color, distance_to_camera );
}

void render_object_type_cube()
{
    float distance_to_camera;
    vec3 light_color = vec3(0);

    for( int i = 0; i < LIGHTS_COUNT; ++i )
        light_color += compute_light( lights[i], v_position, v_normal, u_camera_position ) ;


    FragColor = (environment.ambient + vec4(light_color,1.0) )  * u_color;

    distance_to_camera = length( u_camera_position - v_position );
    FragColor = compute_fog( FragColor, environment.fog_density, environment.fog_color, distance_to_camera );
}

void render_object_type_plane()
{
    vec4  color;
    vec3  light_color;
    float distance_to_camera;

    light_color = vec3(0);
    for( int i = 0; i < LIGHTS_COUNT; ++i )
        light_color += compute_light( lights[i], v_position, v_normal, u_camera_position );

    color = (environment.ambient + vec4(light_color,1.0)) * u_color;

    distance_to_camera = length( u_camera_position - v_position );
    color = compute_fog( color, environment.fog_density, environment.fog_color, distance_to_camera );

    FragColor = color;
}

//////////////////////////////////////////////////////////
///




vec3 compute_light( Light l, vec3 position, vec3 normal, vec3 camera )
{
    vec3 light_direction;
    float light_distance;
    float attenuation;
    vec3 diffuse;
    vec3 specular;

    light_direction = l.position.xyz - position;
    light_distance = length(light_direction);
    light_direction = normalize(light_direction);
    attenuation = 1.0 / (1.0 + l.constant + l.linear*light_distance +  l.quadratic*light_distance*light_distance );

    // Diffuse
    diffuse = l.color.rgb * max( dot(normal, light_direction), 0.0 );
    diffuse *= attenuation;
    return diffuse;

    // Specular
    vec3 viewDir = normalize( camera - position );
    vec3 reflectDir = reflect(-light_direction, normal );
    specular = l.color.rgb * pow( max( dot(viewDir,reflectDir), 0.0), 256 );
    specular *= attenuation;
    return diffuse + specular;
}
vec4 compute_fog( vec4 color, float fog_density, vec4 fog_color, float z )
{
    const float LOG2 = 1.442695;
    float factor = exp2( -fog_density * fog_density * pow(z,2) * LOG2  );
    //   float factor = exp2( -fog_density * fog_density * pow(z,2) * LOG2  * 1.0 / ( 1.0 + pow(v_position.y,2)) );
    factor = clamp(factor, 0.0, 1.0);

    return mix( fog_color, color, factor );
}

// return 0.0 -> 1.0
float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}
