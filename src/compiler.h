#ifndef CLOX_COMPILER_H
#define CLOX_COMPILER_H

#include "chunk.h"
#include "common.h"

bool compile(const char *source, chunk_t *chunk);

#endif
