#include "../shared/consts.h"

    .globl user_switch

user_switch:
mov $DATA_SEGMENT_USER_VALUE, %ax
mov %ax, %ds 
mov %ax, %es 
mov %ax, %fs 
mov %ax, %gs
iret