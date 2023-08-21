// CONSOLE

#define CONSOLE_BASE_ADDRESS 0xB8000

#define CONSOLE_WIDTH 80
#define CONSOLE_HEIGHT 25

#define CONSOLE_COMMAND_PORT 0x3D4
#define CONSOLE_DATA_PORT 0x3D5

#define CURSOR_LOW_COMMAND 0x0F
#define CURSOR_HIGH_COMMAND 0x0E

#define TABULATION_SIZE 8

// INTERRUPTIONS

#define HANDLER_MAP_ADDRESS 0x1000

#define IRQ_0_7_VECTOR_PORT 0x21
#define IRQ_8_15_VECTOR_PORT 0xA1

#define IT_VECTOR_P_VALID (1 << 15)
#define IT_VECTOR_DPL_USERMODE (3 << 13)
#define IT_VECTOR_TYPE_INTERRUPT (14 << 8)
#define IT_VECTOR_TYPE_TRAP (15 << 8)

// MESSAGE QUEUE

#define NBQUEUE 64

// PROCESS

#define PROCESS_TABLE_MAX_SIZE 512

#define PROCESS_MIN_PRIORITY 1
#define PROCESS_MAX_PRIORITY 256
#define PROCESS_NAME_MAX_LENGTH 64
#define PROCESS_REGISTERS_SAVE_LENGTH 6
#define PROCESS_MIN_STACK_SIZE 2048

#define PROCESS_NOT_FOUND_CODE -1

// SCHEDULER

#define SCHEDULER_FREQUENCY 50

// STACK

#define DATA_SEGMENT_USER_VALUE_CHAR "\x4b"
#define DATA_SEGMENT_USER_VALUE 0x4b
#define DATA_SEGMENT_KERNEL_VALUE 0x18
#define CODE_SEGMENT_USER_VALUE 0x43
#define CODE_SEGMENT_KERNEL_VALUE 0x10
#define EFLAGS_USER_VALUE 0x202

// SYSCALLS

#define CODE_SYSCALL 49

#define CODE_cons_write 0
#define CODE_cons_echo 1
#define CODE_cons_read 2

#define CODE_start 3
#define CODE_kill 4
#define CODE_exit 5
#define CODE_chprio 6
#define CODE_getpid 7
#define CODE_getprio 8
#define CODE_waitpid 9
#define CODE_wait_clock 10

#define CODE_current_clock 11
#define CODE_clock_settings 12

#define CODE_pcount 13
#define CODE_pcreate 14
#define CODE_pdelete 15
#define CODE_preceive 16
#define CODE_preset 17
#define CODE_psend 18

// TIMER

#define CLOCKFREQ 100
#define QUARTZ 0x1234DD

#define CLOCKPERIOD QUARTZ / CLOCKFREQ

#define CLOCK_CONTROL_PORT 0x43
#define CLOCK_DATA_PORT 0x40

#define CLOCK_INIT_COMMAND 0x34

// USER SWITCH

#define INSTRUCTION_SIZE 4

