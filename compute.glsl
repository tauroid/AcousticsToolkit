#version 430

// CHANGE THE C DEFINE FIRST, THEN THE FRAGMENT SHADER
#define NUM_POINT_SOURCE 32
#define LOCAL_FIELD_SIZE_X 32
#define LOCAL_FIELD_SIZE_Y 32
#define PI 3.1415926535

layout(local_size_x = LOCAL_FIELD_SIZE_X,
       local_size_y = LOCAL_FIELD_SIZE_Y,
       local_size_z = 1) in;

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
layout(binding = 0) coherent buffer FieldData {
    int written;
    float field_max;
    float field_min;
};

vec2 cmult(vec2 c1, vec2 c2) {
    return vec2(c1.x*c2.x-c1.y*c2.y,c1.x*c2.y+c1.y*c2.x);
}

vec2 cexp(vec2 c) {
    return exp(c.x)*vec2(cos(c.y),sin(c.y));
}

void main() {
    if (gl_GlobalInvocationID.x >= fieldsize.x ||
        gl_GlobalInvocationID.y >= fieldsize.y) return;
    
    vec2 pos = fieldoffset
        +vec2(gl_GlobalInvocationID.xy)/vec2(fieldsize)*fielddims;
    ivec2 ipos = ivec2(gl_GlobalInvocationID.xy);

    vec2 fv = vec2(0.,0.); //imageLoad(field,ipos).rg; <- This way for fun!
    float r,k;
    for (int i = 0; i<psn && i<NUM_POINT_SOURCE; ++i) {
        r = length(pos-ps_loc[i]);
        k = ps_freq[i]*2.*PI/mat_c;
        fv += cmult( cexp( vec2(0.,ps_phase[i]) )/sqrt(r),
                     cexp( vec2(0.,k*r) ) );
    }

    imageStore(field,ivec2(ipos),vec4(fv,0.,1.));
    float m = length(fv);
    if (written == 0) {
        written = 1;
        field_min = m;
        field_max = m;
    } else if (written == 1) {
        if (m < field_min) field_min = m;
        else if (m > field_max) field_max = m;
    }
}
