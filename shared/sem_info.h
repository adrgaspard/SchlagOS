#ifndef __SEM_INFO_H__
#define __SEM_INFO_H__

#include <stdint.h>
#include "consts.h"

typedef struct sem_info_t
{
    int sid;
    int count;
} sem_info_t;

typedef struct sem_info_table_t
{
    int lock_table[PROCESS_TABLE_MAX_SIZE];
    sem_info_t data[NBQUEUE];
    uint32_t size;
} sem_info_table_t;

#endif
