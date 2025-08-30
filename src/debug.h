#ifndef CLOX_DEBUG_H
#define CLOX_DEBUG_H

#include "chunk.h"

void dissasemble_chunk(chunk_t *chunk, const char *name);
int dissasemble_instruction(chunk_t *chunk, int offset);

#endif
