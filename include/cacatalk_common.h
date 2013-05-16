/*
 * cacatalk_common.h
 *
 *  Created on: May 16, 2013
 *      Author: carlos
 */

#ifndef CACATALK_COMMON_H_
#define CACATALK_COMMON_H_

#include "caca.h"

#define BUFFER_SIZE 75
#define TEXT_ENTRIES 5

typedef struct textentry
{
    uint32_t buffer[BUFFER_SIZE + 1];
    unsigned int size, cursor, changed;
} textentry;


#endif /* CACATALK_COMMON_H_ */
