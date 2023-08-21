#ifndef __MESSAGE_QUEUE_H__
#define __MESSAGE_QUEUE_H__

void message_queues_init(void);

void message_queues_notify_priority_changed(int pid, int new_priority);

void message_queues_notify_processus_killed(int pid);

// Primitive functions

int pcreate(int count);

int pcount(int fid, int *count);

int preset(int fid);

int pdelete(int fid);

int psend(int fid, int message);

int preceive(int fid,int *message);

#endif
