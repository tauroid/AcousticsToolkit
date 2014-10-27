#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "common.h"
#include "fim.h"
#include "fi-parser.h"

#define MAX_FILE_BUF_SIZE 2048
#define WINDOW_WIDTH 800.0
#define WINDOW_HEIGHT 600.0
#define MAX_FIELDINFO_UNIFORM_NAME_LENGTH 16
#define FIELDINFO_UBO_BINDING 0
#define FIELDDATA_SSBO_BINDING 0
#define FIELD_IMAGE_UNIT 0
#define FIELD_TEX_UNIT 0

#define COMPUTE_LOCAL_FIELD_SIZE_X 32
#define COMPUTE_LOCAL_FIELD_SIZE_Y 32

typedef struct {
    GLuint vao;
    GLuint vbuf;
    GLuint ibuf;
} Renderable;

Renderable createCanvas() {
    Renderable canvas;

    GLfloat ratio = WINDOW_WIDTH/WINDOW_HEIGHT;
    GLfloat vbuf1data[] = { 0.0f,0.0f,
                           0.0f,1.0f,
                           ratio,1.0f,
                           ratio,0.0f };
    GLuint ibuf1data[] = { 0,1,2, 0,2,3 };

    glGenBuffers(1,&canvas.vbuf);
    glBindBuffer(GL_ARRAY_BUFFER,canvas.vbuf);
    glBufferData(GL_ARRAY_BUFFER,8*sizeof(GLfloat),vbuf1data,GL_STATIC_DRAW);

    glGenVertexArrays(1,&canvas.vao);
    glBindVertexArray(canvas.vao);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_TRUE,0,0);

    glGenBuffers(1,&canvas.ibuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,canvas.ibuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,6*sizeof(GLuint),ibuf1data,GL_STATIC_DRAW);

    glBindVertexArray(0);

    return canvas;
}

void recordInfoLog(unsigned int level,GLuint shader) {
    GLchar* infobuf;

    GLint infobuflen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infobuflen);

    infobuf = (GLchar*) malloc(sizeof(GLchar)*infobuflen);
    glGetShaderInfoLog(shader,infobuflen,NULL,infobuf);
    record(level,infobuf);
    fflush(stdout);

    free(infobuf);
}

int readFile(const char* filename, char* const buffer, int bufsize) {
    FILE* fp = fopen(filename,"r");
    if (fp == NULL) {
        record(1,"Failed to open file %s\n",filename);
        return 1;
    }

    long fl;

    fseek(fp,0,SEEK_END);
    fl = ftell(fp);
    fseek(fp,0,SEEK_SET);

    if (fl > bufsize) {
        record(1,"File %s too big for buffer\n",filename);
        return 1;
    }

    fread(buffer,sizeof(char),fl,fp);
    fclose(fp);
    return 0;
}

GLuint createShaderProgram() {
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

    GLchar ss[MAX_FILE_BUF_SIZE];
    memset(ss,0,MAX_FILE_BUF_SIZE);
    const GLchar* ssptr = (const GLchar*)ss;
    
    if (readFile("vert.glsl", ss, MAX_FILE_BUF_SIZE)) {
        record(1,"Could not read vertex shader\n");
        return 0;
    }
    glShaderSource(vert,1,&ssptr,0);
    glCompileShader(vert);

    GLint compiled = 0;
    glGetShaderiv(vert, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        record(1,"Compilation of vert.glsl failed:\n");
        recordInfoLog(1,vert);
        glDeleteShader(vert);
        return 0;
    }

    memset(ss,0,MAX_FILE_BUF_SIZE);
    if (readFile("frag.glsl", ss, MAX_FILE_BUF_SIZE)) {
        record(1,"Could not read fragment shader\n");
        return 0;
    }
    glShaderSource(frag,1,&ssptr,NULL);
    glCompileShader(frag);

    compiled = 0;
    glGetShaderiv(frag, GL_COMPILE_STATUS, &compiled);
    if(compiled == GL_FALSE) {
        record(1,"Compilation of frag.glsl failed:\n");
        recordInfoLog(1,frag);
        glDeleteShader(vert);
        glDeleteShader(frag);
        return 0;
    }
    
    GLuint prog = glCreateProgram();

    glAttachShader(prog,vert);
    glAttachShader(prog,frag);

    glLinkProgram(prog);

    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        GLint infobuflen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &infobuflen);

        GLchar* infobuf = (GLchar*) malloc(sizeof(GLchar)*infobuflen);
        glGetProgramInfoLog(prog,infobuflen,NULL,infobuf);

        record(1,infobuf);
        fflush(stdout);

        free(infobuf);

        glDeleteProgram(prog);
        glDeleteShader(vert);
        glDeleteShader(frag);
        return 0;
    }

    glDetachShader(prog,vert);
    glDetachShader(prog,frag);

    return prog;
}

GLuint createComputeProgram() {
    GLuint compute = glCreateShader(GL_COMPUTE_SHADER);

    GLchar ss[MAX_FILE_BUF_SIZE];
    const GLchar* ssptr = (const GLchar*)ss;

    memset(ss,0,MAX_FILE_BUF_SIZE);
    readFile("compute.glsl", ss, MAX_FILE_BUF_SIZE);
    glShaderSource(compute,1,&ssptr,NULL);
    glCompileShader(compute);

    GLint compiled = 0;
    glGetShaderiv(compute, GL_COMPILE_STATUS, &compiled);
    if(compiled == GL_FALSE) {
        record(1,"Compilation of compute.glsl failed:\n");
        recordInfoLog(1,compute);
        glDeleteShader(compute);
        return 0;
    }

    GLuint prog = glCreateProgram();

    glAttachShader(prog,compute);
    
    glLinkProgram(prog);

    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        GLint infobuflen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &infobuflen);

        GLchar* infobuf = (GLchar*) malloc(sizeof(GLchar)*infobuflen);
        glGetProgramInfoLog(prog,infobuflen,NULL,infobuf);

        record(1,infobuf);
        fflush(stdout);

        free(infobuf);

        glDeleteProgram(prog);
        glDeleteShader(compute);
        return 0;
    }

    glDetachShader(prog,compute);

    return prog;
}

unsigned int initFieldInfoMap(GLuint program, GLuint blockIndex, FieldInfoMap* fim, GLvoid* const buffer) {
    // Make sure buffer is big enough, because I will not check. All I care about is the pointer address.
    record(0,"Initialising FieldInfo UBO memory map\n");

    int isset_mat_c = 0,isset_psn = 0,isset_ps_loc = 0,isset_ps_freq = 0,isset_ps_phase = 0,
        isset_fieldoffset = 0,isset_fielddims = 0,isset_fieldsize = 0;

    GLchar* const cbuffer = (GLchar* const)buffer;

    GLint numuniforms;
    glGetActiveUniformBlockiv(program,blockIndex,GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS,&numuniforms);
    
    GLint* const uniformindices = (GLint* const)malloc(sizeof(GLint)*numuniforms);
    glGetActiveUniformBlockiv(program,blockIndex,GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,uniformindices);

    GLint* const uniformoffsets = (GLint* const)malloc(sizeof(GLint)*numuniforms);
    glGetActiveUniformsiv(program,numuniforms,uniformindices,GL_UNIFORM_OFFSET,uniformoffsets);

    char uniformname[MAX_FIELDINFO_UNIFORM_NAME_LENGTH];
    GLchar* uniformlocationptr;

    int i;
    for (i=0; i<numuniforms; i++) {
        memset(uniformname,0,MAX_FIELDINFO_UNIFORM_NAME_LENGTH);
        glGetActiveUniformName(program,uniformindices[i],
                               MAX_FIELDINFO_UNIFORM_NAME_LENGTH,NULL,
                               (char* const)uniformname);
        
        record(0,"> Found uniform name \"%s\" at offset %d\n",uniformname,uniformoffsets[i]);

        uniformlocationptr = cbuffer+uniformoffsets[i];
        if (!strcmp(uniformname,"mat_c")) {
            fim->mat_c = (GLfloat*)uniformlocationptr;
            isset_mat_c = 1;

        } else if (!strcmp(uniformname,"psn")) {
            fim->psn = (GLint*)uniformlocationptr;
            isset_psn = 1;
        } else if (!strcmp(uniformname,"ps_loc[0]")) {
            fim->ps_loc = (GLfloat*)uniformlocationptr;
            isset_ps_loc = 1;
        } else if (!strcmp(uniformname,"ps_freq[0]")) {
            fim->ps_freq = (GLfloat*)uniformlocationptr;
            isset_ps_freq = 1;
        } else if (!strcmp(uniformname,"ps_phase[0]")) {
            fim->ps_phase = (GLfloat*)uniformlocationptr;
            isset_ps_phase = 1;

        } else if (!strcmp(uniformname,"fieldoffset")) {
            fim->fieldoffset = (GLfloat*)uniformlocationptr;
            isset_fieldoffset = 1;
        } else if (!strcmp(uniformname,"fielddims")) {
            fim->fielddims = (GLfloat*)uniformlocationptr;
            isset_fielddims = 1;
        } else if (!strcmp(uniformname,"fieldsize")) {
            fim->fieldsize = (GLuint*)uniformlocationptr;
            isset_fieldsize = 1;
        } else {
            record(1,"Uniform %s is not expected in FieldInfo block\n",uniformname);
        }
    }

    free(uniformindices);
    free(uniformoffsets);

    if (!isset_mat_c || !isset_psn || !isset_ps_loc || !isset_ps_freq || !isset_ps_phase ||
            !isset_fielddims || !isset_fieldsize) {
        record(1,"Missing field in uniform block; memory map incomplete\n");
        return 1;
    } else {
        record(0,"All required uniforms found and offsets stored in client-side buffer map\n\n");
    }

    fim->block_start = buffer;

    fim->fielddims[0] = 1.0;
    fim->fielddims[1] = 1.0;
    fim->fieldsize[0] = 128;
    fim->fieldsize[1] = 128;

    return 0;
}

void setupDemoFieldInfo(FieldInfoMap* const fim) {
    *(fim->mat_c) = 500.0;

    const GLfloat ps_loc[] = { 0.0, 0.2,
                               0.0, 0.5,
                               0.0, 0.8  };

    const GLfloat ps_freq[] = { 20000.0, 20000.0, 20000.0 };
    const GLfloat ps_phase[] = { 0.0, 0.0, 0.0 };

    *(fim->psn) = 3;

    const GLfloat fieldoffset[] = { 0.0, 0.0 };
    const GLfloat fielddims[] = { WINDOW_WIDTH/WINDOW_HEIGHT, 1.0 };
    const GLuint fieldsize[] = { 640, 480 };

    memcpy(fim->ps_loc,ps_loc,sizeof(ps_loc));
    memcpy(fim->ps_freq,ps_freq,sizeof(ps_freq));
    memcpy(fim->ps_phase,ps_phase,sizeof(ps_phase));

    memcpy(fim->fieldoffset,fieldoffset,sizeof(fieldoffset));
    memcpy(fim->fielddims,fielddims,sizeof(fielddims));
    memcpy(fim->fieldsize,fieldsize,sizeof(fieldsize));
}

int loadFieldInfoFile(const char* const filename,
                      FieldInfoMap* const fim,
                      FieldDataMap* const fdm)
{
    GLchar fibuf[MAX_FILE_BUF_SIZE];
    memset(fibuf,0,MAX_FILE_BUF_SIZE);

    int readfailed = readFile(filename,fibuf,MAX_FILE_BUF_SIZE);
    if (readfailed) return 1;
    return parseFieldInfo(fibuf, MAX_FILE_BUF_SIZE, fim, fdm);
}

int initFieldDataMap(GLuint program, GLuint blockIndex, FieldDataMap* fdm, void* const buffer) {
    record(0,"Initialising FieldData SSBO memory map\n");

    GLchar* const cbuffer = (GLchar* const)buffer;

    GLint index_written = glGetProgramResourceIndex(program,GL_BUFFER_VARIABLE,"written");
    if (index_written == GL_INVALID_INDEX) {
        record(1,"Variable \"written\" not found; aborting\n");
        return 1;
    }
    GLint index_field_max = glGetProgramResourceIndex(program,GL_BUFFER_VARIABLE,"field_max");
    if (index_field_max == GL_INVALID_INDEX) {
        record(1,"Variable \"field_max\" not found; aborting\n");
        return 1;
    }
    GLint index_field_min = glGetProgramResourceIndex(program,GL_BUFFER_VARIABLE,"field_min");
    if (index_field_min == GL_INVALID_INDEX) {
        record(1,"Variable \"field_min\" not found; aborting\n");
        return 1;
    }

    GLint offset_written, offset_field_max, offset_field_min;
    
    GLenum gl_offset = GL_OFFSET;
    glGetProgramResourceiv(program,GL_BUFFER_VARIABLE,index_written,1,&gl_offset,1,NULL,&offset_written);
    glGetProgramResourceiv(program,GL_BUFFER_VARIABLE,index_field_max,1,&gl_offset,1,NULL,&offset_field_max);
    glGetProgramResourceiv(program,GL_BUFFER_VARIABLE,index_field_min,1,&gl_offset,1,NULL,&offset_field_min);

    record(0,"> Found variable name \"written\" at offset %d\n",offset_written);
    record(0,"> Found variable name \"field_max\" at offset %d\n",offset_field_max);
    record(0,"> Found variable name \"field_min\" at offset %d\n",offset_field_min);

    fdm->written = (GLint*)(cbuffer+offset_written);
    fdm->field_max = (GLfloat*)(cbuffer+offset_field_max);
    fdm->field_min = (GLfloat*)(cbuffer+offset_field_min);

    fdm->block_start = buffer;
    
    record(0,"All required variables found and offsets stored in client-side buffer map\n\n");

    return 0;
}

int main(int argc, char** argv) {
    setbuf(stdout,NULL);
    if (argc > 1 && strcmp(argv[1],"-v") == 0){
        setVerbose(1);
        record(0,"Verbose mode switched on\n");
    }
    
    if(!glfwInit()) return 1;
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH,WINDOW_HEIGHT,"Acoustics Toolkit",NULL,NULL);
    if (!window) {
        record(1,"Window creation failed; terminating\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    if (glewInit() != GLEW_OK) {
        record(1,"Failed to initialise GL extensions; terminating\n");
        glfwTerminate();
        return 1;
    }

    Renderable canvas = createCanvas();
    GLuint shaderprogram = createShaderProgram();
    if (!shaderprogram) {
        record(1,"Failed to create shader program; terminating\n");
        glfwTerminate();
        return 1;
    }

    GLuint computeprogram = createComputeProgram();
    if (!computeprogram) {
        record(1,"Failed to create compute program; terminating\n");
        glfwTerminate();
        return 1;
    }
/*
 * ----------------------------------------------------------------------------
 *  Prepare a client-side buffer for upload to the FieldInfo UBO
 * ----------------------------------------------------------------------------
 */
    FieldInfoMap fieldinfomap;
    GLuint fieldinfoubo;
    GLint fibstoragesize;
    
    {
        GLuint computeBlockIndex = glGetUniformBlockIndex(computeprogram,"FieldInfo");
        if (computeBlockIndex == GL_INVALID_INDEX) {
            glfwTerminate();
            return 1;
        }

        glGetActiveUniformBlockiv(computeprogram,computeBlockIndex,GL_UNIFORM_BLOCK_DATA_SIZE,&fibstoragesize);
        GLvoid* const ubobuffer = malloc(sizeof(char)*fibstoragesize);
        memset(ubobuffer,0,fibstoragesize);

        record(0,"------------------------------------------------------------\n"
                 " Getting FieldInfo block information from shaders\n"
                 "------------------------------------------------------------\n\n");
        record(0,"Buffer created at %p\n",ubobuffer);
        if (initFieldInfoMap(computeprogram,computeBlockIndex,&fieldinfomap,ubobuffer)) {
            free(ubobuffer);
            glfwTerminate();
            return 1;
        } 
   }
/*
 * ----------------------------------------------------------------------------
 *  Prepare an initial client-side buffer and buffer map for the FieldData SSBO
 * ----------------------------------------------------------------------------
 */
    FieldDataMap fielddatamap;
    GLuint fielddatassbo;
    GLint fdbstoragesize;

    {
        GLuint computeBlockIndex = glGetProgramResourceIndex(computeprogram,GL_SHADER_STORAGE_BLOCK,"FieldData");
        if (computeBlockIndex == GL_INVALID_INDEX) {
            record(1,"FieldData block not found; no metadata will be available to the fragment shader\n\n");
            fdbstoragesize = 0;
        } else {
            GLenum gl_buffer_data_size = GL_BUFFER_DATA_SIZE;
            glGetProgramResourceiv(computeprogram,GL_SHADER_STORAGE_BLOCK,
                                   computeBlockIndex,1,&gl_buffer_data_size,
                                   1,NULL,&fdbstoragesize);

            GLvoid* const ssbobuffer = malloc(sizeof(char)*fdbstoragesize);
            memset(ssbobuffer,0,fdbstoragesize);

            record(0,"------------------------------------------------------------\n"
                     " Getting FieldData block information from shaders\n"
                     "------------------------------------------------------------\n\n");
            record(0,"Buffer created at %p\n",ssbobuffer);
            if (initFieldDataMap(computeprogram,computeBlockIndex,&fielddatamap,ssbobuffer)) {
                record(1,"FieldData block is missing required elements;"
                         " no metadata will be available to the fragment shader\n\n");
                fdbstoragesize = 0;
                free(ssbobuffer);
            }
        }
        // If buffer creation fucked up, we'll make a dummy for the parser
        // but not upload anything
        if (fdbstoragesize == 0) {
            fielddatamap.block_start = malloc(FDM_DUMMY_BUFFER_SIZE);
            fielddatamap.written = (GLint*)fielddatamap.block_start;
            fielddatamap.field_max = (GLfloat*)(fielddatamap.written+1);
            fielddatamap.field_min = fielddatamap.field_max+1;
        }
    }
/*
 * ----------------------------------------------------------------------------
 *  Load data into client-side buffers, hopefully from input file *crossed*
 * ----------------------------------------------------------------------------
 */
    record(0,"------------------------------------------------------------\n"
             " Loading FieldInfo file field1.fi\n"
             "------------------------------------------------------------\n\n");
    if (loadFieldInfoFile("field1.fi",&fieldinfomap,&fielddatamap)) {
        record(1,"Failed to load input file; falling back to demo\n\n");
        setupDemoFieldInfo(&fieldinfomap);
    }
/*
 * ----------------------------------------------------------------------------
 *  Create, fill and bind the FieldInfo uniform buffer
 * ----------------------------------------------------------------------------
 */
    glGenBuffers(1,&fieldinfoubo);
    glBindBuffer(GL_UNIFORM_BUFFER,fieldinfoubo);
    glBufferStorage(GL_UNIFORM_BUFFER,fibstoragesize,fieldinfomap.block_start,GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER,FIELDINFO_UBO_BINDING,fieldinfoubo);

    {
        GLuint computeBlockIndex = glGetUniformBlockIndex(computeprogram,"FieldInfo");
        glUniformBlockBinding(computeprogram,computeBlockIndex,FIELDINFO_UBO_BINDING);

        GLuint shaderBlockIndex = glGetUniformBlockIndex(shaderprogram,"FieldInfo");
        glUniformBlockBinding(shaderprogram,shaderBlockIndex,FIELDINFO_UBO_BINDING);
    }
/*
 * ----------------------------------------------------------------------------
 *  Create, fill and bind the FieldData SSBO
 * ----------------------------------------------------------------------------
 */
    if (fdbstoragesize > 0) {
        glGenBuffers(1,&fielddatassbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER,fielddatassbo);
        glBufferStorage(GL_SHADER_STORAGE_BUFFER,fdbstoragesize,fielddatamap.block_start,0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER,FIELDDATA_SSBO_BINDING,fielddatassbo);
    }
/*
 * ----------------------------------------------------------------------------
 *  Create and bind texture buffer for storage of amplitude field
 * ----------------------------------------------------------------------------
 */
    GLuint fieldtexture;
    glGenTextures(1,&fieldtexture);
    glBindTexture(GL_TEXTURE_2D,fieldtexture);
    glTexStorage2D(GL_TEXTURE_2D,1,GL_RG32F,fieldinfomap.fieldsize[0],fieldinfomap.fieldsize[1]);
    GLfloat fillColour[] = { 0.0, 1.0 };
    glClearTexImage(fieldtexture,0,GL_RG,GL_FLOAT,(const void*)fillColour);
    glBindImageTexture(FIELD_IMAGE_UNIT,fieldtexture,0,GL_TRUE,0,GL_READ_WRITE,GL_RG32F);

    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D,0);
/*
 * ----------------------------------------------------------------------------
 *  Assign shader program non-block uniforms
 * ----------------------------------------------------------------------------
 */
    GLint fieldsamplerloc = glGetUniformLocation(shaderprogram,"fieldsampler");

    GLint ortho = glGetUniformLocation(shaderprogram,"ortho");
    const GLfloat orthomat[] = { 2/(fieldinfomap.fieldsize[0]/fieldinfomap.fieldsize[1]),0,0,-1,
                                 0,2,0,-1,
                                 0,0,1,0,
                                 0,0,0,1 };

    GLint wwidth = glGetUniformLocation(shaderprogram,"window_width");
    GLint wheight = glGetUniformLocation(shaderprogram,"window_height");
    GLint stime = glGetUniformLocation(shaderprogram,"time");

    glUseProgram(shaderprogram);

    glUniform1i(fieldsamplerloc, FIELD_TEX_UNIT);
    glActiveTexture(GL_TEXTURE0 + FIELD_TEX_UNIT);
    glBindTexture(GL_TEXTURE_2D,fieldtexture);

    glUniformMatrix4fv(ortho,1,GL_TRUE,orthomat);
    glUniform1f(wwidth,fieldinfomap.fieldsize[0]);
    glUniform1f(wheight,fieldinfomap.fieldsize[1]);
/*
 * ----------------------------------------------------------------------------
 *  Last minute setup for misc GL state reliant on user data
 * ----------------------------------------------------------------------------
 */
    glfwSetWindowSize(window,(int)fieldinfomap.fieldsize[0],
                             (int)fieldinfomap.fieldsize[1]);
    glViewport(0,0,fieldinfomap.fieldsize[0],fieldinfomap.fieldsize[1]);
/*
 * ----------------------------------------------------------------------------
 */
    while(!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(computeprogram);

        glDispatchCompute(fieldinfomap.fieldsize[0]/COMPUTE_LOCAL_FIELD_SIZE_X+1,
                          fieldinfomap.fieldsize[1]/COMPUTE_LOCAL_FIELD_SIZE_Y+1, 1);

        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT |
                        GL_SHADER_STORAGE_BARRIER_BIT);

        glUseProgram(shaderprogram);

        glUniform1f(stime,glfwGetTime());
        
        glBindVertexArray(canvas.vao);
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    free(fielddatamap.block_start);
    free(fieldinfomap.block_start);
    glfwTerminate();
    return 0;
}
