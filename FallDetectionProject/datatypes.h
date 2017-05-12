#ifndef DATATYPES_H
#define DATATYPES_H

#include <inttypes.h>

typedef struct sDVSEventDepacked {
    int32_t ts;
    uint16_t x;
    uint16_t y;
    bool pol;
} sDVSEventDepacked;

#endif // DATATYPES_H
