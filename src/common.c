#include <stdarg.h>

unsigned int verbose = 0;

void setVerbose(unsigned int on) { verbose = on; }

void record(unsigned int level,const char* fmt,...) {
    va_list args;
    va_start(args,fmt);
    if(verbose || level > 0) vprintf(fmt,args);
    va_end(args);
}

