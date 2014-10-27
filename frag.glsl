#version 430

// FOR FUCKS SAKE DONT CHANGE THIS WITHOUT CHECKING THE C CODE AND THE COMPUTE SHADER
#define NUM_POINT_SOURCE 32
#define PI 3.1415926535

out vec4 c;

uniform float window_width;
uniform float window_height;
uniform float time;

uniform FieldInfo {
    float mat_c;

    int psn;
    vec2 ps_loc[NUM_POINT_SOURCE];
    float ps_freq[NUM_POINT_SOURCE];
    float ps_phase[NUM_POINT_SOURCE];

    vec2 fieldoffset;
    vec2 fielddims;
    uvec2 fieldsize;
};

layout(rg32f,binding = 0) uniform image2D field;
uniform sampler2D fieldsampler;
layout(binding = 0) coherent buffer FieldData {
    int written;
    float field_max;
    float field_min;
};

vec4 mapStoC(float scalar) {
    /*return vec4(min(max(vec3(2.*(scalar-.5),
                         1.-2.*abs(scalar-.5),
                         1.-2.*scalar),
                        vec3(0.)),vec3(1.)),1.);*/
    return vec4(vec3(
                     cos(PI*(scalar-1.)),
                     cos(PI*(scalar-1./2.)),
                     cos(PI*(scalar))
                    ),1.);
}

void main(void) {
    vec2 pos = gl_FragCoord.xy/vec2(window_width,window_height);
    c = mapStoC( clamp((length(texture(fieldsampler,pos).rg)-field_min)/
                       (field_max-field_min),0.,1.) );
    /*if (written == 2) c = vec4(1.,0.,0.,1.);
    else c = vec4(0.,1.,0.,1.);*/
}
