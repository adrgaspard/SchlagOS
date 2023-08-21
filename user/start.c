#include "../shared/debug.h"
#include "start.h"
#include "syscalls.h"
#include "../shared/debug.h"
#include "test.h"

int les_tests_tkt(void *arg){
    (void)arg;
    auto_test();
    return 0;
}

void user_start(void)
{
    start(les_tests_tkt, 2048, 128, "test_user", NULL);
    while(1);
    return;
}

void console_putbytes(const char *str, int length)
{
    cons_write(str, length);
}