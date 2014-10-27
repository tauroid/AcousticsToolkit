#include <stdio.h>
#include <string.h>
#include <GL/gl.h>

#include "common.h"
#include "fim.h"

#define MAX_BLOCKS 32
#define RPTERRORLC(S) record(1,"Error - L%d, C%d: " S,linecount,columncount)

int current_point_source_loc = 0;
int current_point_source_freq = 0;
int current_point_source_phase = 0;

void recordToken(const int valid, const int n, GLenum datatype,
                 const char* const token, const void* const data) {
    if (datatype == GL_FLOAT) {
        if (valid) record(0,"> Token %d: %s -> %f at %p\n",n,token,*((GLfloat*)data),data);
        else record(1,"Error at token %d: \"%s\" <- float type expected\n",n,token);
    } else if (datatype == GL_INT) {
        if (valid) record(0,"> Token %d: %s -> %d at %p\n",n,token,*((GLint*)data),data);
        else record(1,"Error at token %d: \"%s\" <- integer type expected\n",n,token);
    } else if (datatype == GL_UNSIGNED_INT) {
        if (valid) record(0,"> Token %d: %s -> %u at %p\n",n,token,*((GLuint*)data),data);
        else record(1,"Error at token %d: \"%s\" <- unsigned integer type expected\n",n,token);
    }
}

int parseData(char* const cdata, const char* const delim,
              const int n, GLenum datatype, void* target)
{
    int tokenvalid;

    int i = 0;
    const char* token = strtok(cdata,delim);
    while(token != NULL && i<n) {
        if (datatype == GL_FLOAT) {
            tokenvalid = sscanf(token,"%f",(GLfloat*)target+i);
            recordToken(tokenvalid,i+1,datatype,token,(void*)((GLfloat*)target+i));
        } else if (datatype == GL_INT) {
            tokenvalid = sscanf(token,"%d",(GLint*)target+i);
            recordToken(tokenvalid,i+1,datatype,token,(void*)((GLint*)target+i));
        } else if (datatype == GL_UNSIGNED_INT) {
            tokenvalid = sscanf(token,"%u",(GLuint*)target+i);
            recordToken(tokenvalid,i+1,datatype,token,(void*)((GLuint*)target+i));
        } else return 0;
        token = strtok(NULL,delim);
        ++i;
    }

    return i;
}

int parsePointSource(char* const cdata,
                     const char* const delim,
                     const int index,
                     FieldInfoMap* const fim)
{
    GLfloat* const location = fim->ps_loc+NUM_DIMS*index;
    GLfloat* const frequency = fim->ps_freq+index;
    GLfloat* const phase = fim->ps_phase+index;

    const char* tokens[NUM_DIMS+2];
    int i = 0;

    tokens[i] = strtok(cdata,delim);
    while (tokens[i] != NULL && i<NUM_DIMS+2) {
        ++i;
        tokens[i] = strtok(NULL,delim);
    }

    const int numtokens = i;
    int tokenvalid;

    for (i=0; i<numtokens; ++i) {
        if (i < NUM_DIMS) {
            tokenvalid = sscanf(tokens[i],"%f",location+i);
            recordToken(tokenvalid,i+1,GL_FLOAT,tokens[i],(void*)(location+i));
        } else if (i == NUM_DIMS) {
            tokenvalid = sscanf(tokens[i],"%f",frequency);
            recordToken(tokenvalid,i+1,GL_FLOAT,tokens[i],(void*)(frequency));
        } else if (i == NUM_DIMS+1) {
            tokenvalid = sscanf(tokens[i],"%f",phase);
            recordToken(tokenvalid,i+1,GL_FLOAT,tokens[i],(void*)(phase));
        }
    }

    return numtokens;
}

int parseBlock(char* const block, FieldInfoMap* const fim, FieldDataMap* const fdm)
{
    const char* delim = " ,()\n";
    const char* blockname = strtok(block,delim);
    char* const cdata = (char* const)(blockname+strlen(blockname)+1);

    int i,j;
    int numtokens;

    record(0,"Block name: \"%s\"\n",blockname);

    if (strcmp("Material-C",blockname) == 0) {
        numtokens = parseData(cdata,delim,1,GL_FLOAT,(void*)(fim->mat_c));
    } else if (strcmp("PointSource-Number",blockname) == 0) {
        numtokens = parseData(cdata,delim,1,GL_INT,(void*)(fim->psn));
    } else if (strcmp("PointSource",blockname) == 0) {
        i = current_point_source_loc < current_point_source_freq ?
                (current_point_source_freq < current_point_source_phase ?
                     current_point_source_phase : current_point_source_freq) :
                (current_point_source_loc < current_point_source_phase ?
                     current_point_source_phase : current_point_source_loc);

        numtokens = parsePointSource(cdata,delim,i,fim);
        j = numtokens > 0 ? 1 : 0;

        current_point_source_loc = i+j;
        current_point_source_freq = i+j;
        current_point_source_phase = i+j;
    } else if (strcmp("PointSource-Location",blockname) == 0) {
        numtokens = parseData(cdata,delim,
                              NUM_DIMS*(MAX_POINT_SOURCE-current_point_source_loc),
                              GL_FLOAT,
                              (void*)(fim->ps_loc+NUM_DIMS*current_point_source_loc));

        current_point_source_loc += 1 + (numtokens - 1)/NUM_DIMS;
    } else if (strcmp("PointSource-Frequency",blockname) == 0) {
        numtokens = parseData(cdata,delim,MAX_POINT_SOURCE-current_point_source_freq,
                      GL_FLOAT,(void*)(fim->ps_freq+current_point_source_freq));

        current_point_source_freq += numtokens;
    } else if (strcmp("PointSource-Phase",blockname) == 0) {
        numtokens = parseData(cdata,delim,MAX_POINT_SOURCE-current_point_source_phase,
                      GL_FLOAT,(void*)(fim->ps_phase+current_point_source_phase));

        current_point_source_phase += numtokens;
    } else if (strcmp("Field-Offset",blockname) == 0) {
        numtokens = parseData(cdata,delim,NUM_DIMS,GL_FLOAT,(void*)(fim->fieldoffset));
    } else if (strcmp("Field-Dimensions",blockname) == 0) {
        numtokens = parseData(cdata,delim,NUM_DIMS,GL_FLOAT,(void*)(fim->fielddims));
    } else if (strcmp("Field-Size",blockname) == 0) {
        numtokens = parseData(cdata,delim,NUM_DIMS,
                              GL_UNSIGNED_INT,(void*)(fim->fieldsize));
    } else if (strcmp("Field-Max",blockname) == 0) {
        *(fdm->written) = 2;
        numtokens = parseData(cdata,delim,1,GL_FLOAT,(void*)(fdm->field_max));
    } else if (strcmp("Field-Min",blockname) == 0) {
        *(fdm->written) = 2;
        numtokens = parseData(cdata,delim,1,GL_FLOAT,(void*)(fdm->field_min));
    } else {
        record(1,"\"%s\" does not correspond to any available data field\n",
               blockname);
    }

    record(0,"Read %d tokens from block %s\n\n",numtokens,blockname);

    return numtokens;
}

int parseFieldInfo(char* const textbuffer, int bufsize,
                   FieldInfoMap* const fim, FieldDataMap* const fdm)
{
    record(0,"Scanning FI input buffer\n");
    char* block_start[MAX_BLOCKS];

    int i,j=0,open=0,linecount=0,columncount=0;
    for (i = 0; i<bufsize && j<MAX_BLOCKS; ++i) {
        if (open) {
            if (textbuffer[i] == ']') {
                textbuffer[i] = '\0';
                open=0;
                ++j;
            } else if (textbuffer[i] == '[') {
                RPTERRORLC("Can't open a block within a block.\n");
                return 1;
            }
        } else {
            if (textbuffer[i] == '[') {
                block_start[j] = textbuffer+i+1;
                open=1;
            } else if (textbuffer[i] == ']') {
                RPTERRORLC("No block to close\n");
                return 1;
            }
        }

        if (textbuffer[i] == '\n') {
            ++linecount;
            columncount=0;
        } else {
            ++columncount;
        }
    }

    record(0,"%d blocks found\n\n",j);

    const int numblocks = j;

    j = 0;
    const char* const delim = " ,()\n";
    for (i=0; i<numblocks; ++i) {
        parseBlock(block_start[i],fim,fdm);
    }

    return 0;
}
