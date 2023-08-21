#include <stdint.h>

#include "debugger.h"
#include "cpu.h"
#include "../shared/stdio.h"
#include "handlers_functions.h"
#include "timer.h"
#include "process.h"
#include "test.h"
#include "message_queue.h"
#include "start.h"
#include "stdin.h"

int init(void *arg)
{
	(void)arg;
	//start(test_proc, 4000, 128, "tests", NULL);
	// for (int i = 128; i < 256; i++)
	// {
	// 	if (i % 10 == 1) printf("\n");
	// 	printf("%x %c - ", i, (unsigned char)i);
	// }
	// printf("\n\033F0");
	// printf("\033BF\xc9\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xbb\n");
	// printf("\033BF\xba      \033B6  \033BF    \xba\n");
	// printf("\033BF\xba      \033B6 \033BF     \xba\n");
	// printf("\033BF\xba    \033B7  \033B6 \033B8 \033BF    \xba\n");
	// printf("\033BF\xba  \033B7  \033BC \033B6  \033B4 \033B8  \033BF  \xba\n");
	// printf("\033BF\xba \033B7 \033BC    \033B4 \033BC \033B4  \033B8 \033BF \xba\n");
	// printf("\033BF\xba \033B7 \033BC \033BF  \033BC    \033B4 \033B8 \033BF \xba\n");
	// printf("\033BF\xba\033B7 \033BC \033BF  \033BC     \033B4  \033B8 \033BF\xba\n");
	// printf("\033BF\xba\033B7 \033BC \033BF  \033BC     \033B4  \033B8 \033BF\xba\n");
	// printf("\033BF\xba\033B7 \033BC        \033B4  \033B8 \033BF\xba\n");
	// printf("\033BF\xba\033B7 \033BC       \033B4   \033B8 \033BF\xba\n");
	// printf("\033BF\xba \033B8 \033B4 \033BC    \033B4   \033B8 \033BF \xba\n");
	// printf("\033BF\xba \033B8 \033B4        \033B8 \033BF \xba\n");
	// printf("\033BF\xba  \033B8 \033B4  \033B8  \033B4  \033B8 \033BF  \xba\n");
	// printf("\033BF\xba   \033B8  \033BF  \033B8  \033BF   \xba\n");
	// printf("\033BF\xc8\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xbc\n");
	// printf("\033D");
	// printf("\033B0m\033B8m\033B7h\033BF \033BBp\033B3o\033B9u\033B1l\033B2e\033BAr\033BE!\033B6!\033BC!\033B4!\033B5!\033BD!\033D\n");
	// for (int j = 0; j < 0; j++)
	// {
	// 	printf("\033B0 \033B8 \033B7 \033BF \033BB \033B3 \033B9 \033B1 \033B2 \033BA \033BE \033B6 \033BC \033B4 \033B5 \033BD ");
	// }	
	sti();
	start(user_start, 4000, 2, "user", NULL);
	while(1)
	{
		hlt();
	}
}

int test_proc(void *arg){
	(void)arg;
	run_phase3_tests();
	run_phase4_tests();
	return 0;
}

void kernel_start(void)
{
	process_table_init();
	message_queues_init();
	stdin_init();
	clock_init();
	handlers_init();
	unmask_IRQ_channel(0);
	unmask_IRQ_channel(1);
	printf("\f   _____      __    __            ____  _____\n  / ___/_____/ /_  / /___ _____ _/ __ \\/ ___/\n  \\__ \\/ ___/ __ \\/ / __ `/ __ `/ / / /\\__ \\ \n ___/ / /__/ / / / / /_/ / /_/ / /_/ /___/ / \n/____/\\___/_/ /_/_/\\__,_/\\__, /\\____//____/  \n                        /____/\n");
	start(init, 8192, 1, "init", NULL);
	init(NULL);
	return;
}
