#ifndef __PROCESS_INFO_H__
#define __PROCESS_INFO_H__

#include <stdint.h>
#include "consts.h"

typedef enum process_info_state_enum
{
    RUNNING = 1,
    WAITING = 2,
    BLOCKED_BY_MESSAGE = 4,
    BLOCKED_BY_IO = 8,
    BLOCKED_BY_CHILD = 16,
    SLEEPING = 32,
    ZOMBIE = 64
} process_info_state_enum;

typedef struct process_info_t
{
    int pid;
    int ppid;
    process_info_state_enum state;
    int priority;
    char name[PROCESS_NAME_MAX_LENGTH];
    uint64_t allocated_size;
} process_info_t;

typedef struct process_info_table_t
{
    process_info_t data[PROCESS_TABLE_MAX_SIZE];
    uint32_t size;
} process_info_table_t;

#endif
