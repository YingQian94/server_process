#ifndef _DATA_H
#define _DATA_H

#include "efun.h"
class Data
{
public:
    long fileLen;
    int k;
    char imagename[NAMELEN];
    char filename[NAMELEN];
    Data():fileLen(0),k(0){
        memset(imagename,0,NAMELEN);
        memset(filename,0,NAMELEN);
    }
};

#endif