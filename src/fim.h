// SET THIS IN THE FRAG AND COMPUTE SHADERS TOO
#define NUM_DIMS 2
#define MAX_POINT_SOURCE 32
#define FDM_DUMMY_BUFFER_SIZE 12

typedef struct FieldInfoMap {
    GLfloat* mat_c;
    
    GLint* psn;
    GLfloat* ps_loc;
    GLfloat* ps_freq;
    GLfloat* ps_phase;

/*  GLfloat* ref_loc;
    GLfloat* ref */

    GLfloat* fieldoffset;
    GLfloat* fielddims;
    GLuint* fieldsize;

    GLvoid* block_start;
} FieldInfoMap;

typedef struct {
    GLint* written;
    GLfloat* field_max;
    GLfloat* field_min;

    GLvoid* block_start;
} FieldDataMap;
