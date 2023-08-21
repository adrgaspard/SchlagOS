
#include "cpu.h"
#include "../shared/consts.h"
#include "debug.h"
#include "console.h"
#include "process.h"
#include "message_queue.h"
#include "timer.h"
#include <stdbool.h>
#include <stdint.h>

static bool check_arg(void *arg);

/*
    Cette fonction est appelée par le traitant de l'interruption 49 pour gérer les appels systèmes par l'utilisateur.
    Les arguments sont utilisés de façon variable en fonction de l'appel système réalisé, ce dernier est déterminé en fonction du code d'appel
*/
unsigned long syscall_handler(long call_code, void *arg1, void *arg2, void *arg3, void *arg4, void *arg5){
    outb(0x20, 0x20);

    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;

    switch (call_code)
    {
    case CODE_cons_write:
        if(!check_arg(arg1)){
            return -1;
        }
        cons_write((const char *) arg1, (long) arg2);
        return 0;
    case CODE_cons_echo:
        cons_echo((int) arg1);
        return 0;
    case CODE_start:
        if(!check_arg(arg1) || !check_arg(arg4)){
            return -1;
        }
        return start(arg1, (unsigned long) arg2, (int) arg3, arg4, arg5);
    case CODE_kill:
        return kill((int) arg1);
    case CODE_exit:
        exit((int) arg1);
        return 0;
    case CODE_chprio:
        return chprio((int) arg1, (int) arg2);
    case CODE_getpid:
        return getpid();
    case CODE_getprio:
        return getprio((int) arg1);
    case CODE_waitpid:
        if(!check_arg(arg2)){
            return -1;
        }
        return waitpid((int) arg1, arg2);
    case CODE_wait_clock:
        wait_clock((unsigned long) arg1);
        return 0;
    case CODE_current_clock:
        return current_clock();
    case CODE_clock_settings:
        if(!check_arg(arg1) || !check_arg(arg2)){
            return -1;
        }
        clock_settings(arg1, arg2);
        return 0;
    case CODE_pcount:
        if(!check_arg(arg2)){
            return -1;
        }
        return pcount((int) arg1, arg2);
    case CODE_pcreate:
        return pcreate((int) arg1);
    case CODE_pdelete:
        return pdelete((int) arg1);
    case CODE_preceive:
        if(!check_arg(arg2)){
            return -1;
        }
        return preceive((int) arg1, arg2);
    case CODE_preset:
        return preset((int) arg1);
    case CODE_psend:
        return psend((int) arg1, (int) arg2);
    
    case CODE_cons_read:
        if(!check_arg(arg1)){
            return -1;
        }
        return cons_read((char *) arg1, (unsigned long) arg2);
    default:
        printf("Unknown syscall code");
        assert(0); // On fait planter l'OS
        break;
    }

    return -1;
}

/*
  Cette fonction vérifie que les pointeurs donnés en argument sont bien dans la zone d'écriture de l'utilisateur, pour éviter certaines failles à base d'écriture dans des endroits sensibles
*/
static bool check_arg(void * arg){
    return arg == NULL || ((uint32_t) arg >=  0x1000000 && (uint32_t) arg <=  0x3000000);
}