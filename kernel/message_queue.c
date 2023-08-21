#include <stdint.h>
#include <stdbool.h>

#include "../shared/consts.h"
#include "../shared/debug.h"
#include "mem.h"
#include "message_queue.h"
#include "process.h"

#define INDEXES_LOCK_VALUE -1

#define MESSAGE_QUEUE_BUFFER_MAX_SIZE 536870911

typedef struct process_node_content_t
{
    int pid;
    int message;
    int *destination;
    int priority;
} process_node_content_t;
typedef struct process_node_t
{
    process_node_content_t content;
    struct process_node_t *previous;
    struct process_node_t *next;
} process_node_t;

typedef struct process_queue_t
{
    process_node_t *first;
    process_node_t *last;
    uint32_t size;
} process_queue_t;

typedef struct message_queue_t
{
    int *buffer;
    uint32_t nb_messages;
    uint32_t size;
    int message_consume_index;
    int message_produce_index;
    process_queue_t *locked_processes;
} message_queue_t;

typedef struct message_queue_table_t
{
    message_queue_t data[NBQUEUE];
    int fids[NBQUEUE];
    int fids_consume_index;
    int fids_release_index;
    uint32_t current_size;
    bool reset_releases[PROCESS_TABLE_MAX_SIZE];
} message_queue_table_t;

message_queue_table_t message_queue_table;
const message_queue_t empty_message_queue = { .buffer = NULL, .nb_messages = 0, .size = 0, .locked_processes = NULL };

process_queue_t process_queue_create()
{
    process_queue_t empty_process_queue = { .first = NULL, .last = NULL, .size = 0 };
    return empty_process_queue;
}

static void process_queue_add_on_priority(process_queue_t *queue, process_node_content_t content)
{
    process_node_t *new = mem_alloc(sizeof(process_node_t));
    assert(new != NULL);
    process_node_t *current_iterator = queue->last;
    process_node_t *previous_iterator = NULL;
    new->content = content;
    while (current_iterator != NULL)
    {
        if (current_iterator->content.priority > content.priority)
        {
            break;
        }
        previous_iterator = current_iterator;
        current_iterator = current_iterator->previous;
    }
    new->previous = current_iterator;
    new->next = previous_iterator;
    if (current_iterator == NULL)
    {
        queue->first = new;
    }
    else
    {
        current_iterator->next = new;
    }
    if (previous_iterator == NULL)
    {
        queue->last = new;
    }
    else
    {
        previous_iterator->previous = new;
    }
    queue->size++;
}

static bool process_queue_remove_first_with_pid(process_queue_t *queue, int pid, process_node_content_t *removed)
{
    process_node_t *current = queue->first;
    while (current != NULL)
    {
        if (current->content.pid == pid)
        {
            if (current->previous == NULL)
            {
                queue->first = current->next;
            }
            else
            {
                current->previous->next = current->next;
            }
            if (current->next == NULL)
            {
                queue->last = current->previous;
            }
            else
            {
                current->next->previous = current->previous;
            }
            if (removed != NULL)
            {
                *removed = current->content;
            }
            mem_free(current, sizeof(process_node_t));
            queue->size--;
            return true;
        }
        current = current->next;
    }
    return false;
}

static process_node_content_t process_queue_remove_first(process_queue_t *queue)
{
    process_node_content_t result = queue->first->content;
    process_node_t *old = queue->first;
    if (old->next != NULL)
    {
        old->next->previous = NULL;
    }
    else
    {
        queue->last = NULL;
    }
    queue->first = old->next;
    mem_free(old, sizeof(process_node_t));
    queue->size--;
    return result;
}

static void message_queue_register(message_queue_t *queue, int message)
{
    assert(queue->message_produce_index != INDEXES_LOCK_VALUE);
    queue->buffer[queue->message_produce_index] = message;
    if (queue->message_consume_index == INDEXES_LOCK_VALUE)
    {
        queue->message_consume_index = queue->message_produce_index;
    }
    queue->message_produce_index = (queue->message_produce_index + 1) % queue->size;
    if (queue->message_produce_index == queue->message_consume_index)
    {
        queue->message_produce_index = INDEXES_LOCK_VALUE;
    }
    queue->nb_messages++;
}

static int message_queue_unregister(message_queue_t *queue)
{
    assert(queue->message_consume_index != INDEXES_LOCK_VALUE);
    int message = queue->buffer[queue->message_consume_index];
    if (queue->message_produce_index == INDEXES_LOCK_VALUE)
    {
        queue->message_produce_index = queue->message_consume_index;
    }
    queue->message_consume_index = (queue->message_consume_index + 1) % queue->size;
    if (queue->message_consume_index == queue->message_produce_index)
    {
        queue->message_consume_index = INDEXES_LOCK_VALUE;
    }
    queue->nb_messages--;
    return message;
}

static int message_queue_table_register()
{
    assert(message_queue_table.fids_consume_index != INDEXES_LOCK_VALUE);
    int fid = message_queue_table.fids[message_queue_table.fids_consume_index];
    if (message_queue_table.fids_release_index == INDEXES_LOCK_VALUE)
    {
        message_queue_table.fids_release_index = message_queue_table.fids_consume_index;
    }
    message_queue_table.fids_consume_index = (message_queue_table.fids_consume_index + 1) % NBQUEUE;
    if (message_queue_table.fids_consume_index == message_queue_table.fids_release_index)
    {
        message_queue_table.fids_consume_index = INDEXES_LOCK_VALUE;
    }
    message_queue_table.current_size++;
    return fid;
}

static void message_queue_table_unregister(int fid)
{
    assert(message_queue_table.fids_release_index != INDEXES_LOCK_VALUE);
    message_queue_table.fids[message_queue_table.fids_release_index] = fid;
    if (message_queue_table.fids_consume_index == INDEXES_LOCK_VALUE)
    {
        message_queue_table.fids_consume_index = message_queue_table.fids_release_index;
    }
    message_queue_table.fids_release_index = (message_queue_table.fids_release_index + 1) % NBQUEUE;
    if (message_queue_table.fids_release_index == message_queue_table.fids_consume_index)
    {
        message_queue_table.fids_release_index = INDEXES_LOCK_VALUE;
    }
    message_queue_table.current_size--;
}

static int message_queue_reset(int fid, bool *schedule_needed)
{
    if (fid < 0 || fid >= NBQUEUE || message_queue_table.data[fid].buffer == NULL)
    {
        return -1;
    }
    bool need_schedule = false;
    message_queue_t *message_queue = &(message_queue_table.data[fid]);
    while (message_queue->locked_processes->size > 0)
    {
        process_node_content_t content = process_queue_remove_first(message_queue->locked_processes);
        if (content.destination != NULL)
        {
            *(content.destination) = -1;
        }
        need_schedule = awake(content.pid) || need_schedule;
        message_queue_table.reset_releases[content.pid - 1] = true;
    }
    while (message_queue_table.data[fid].nb_messages > 0)
    {
        message_queue_unregister(message_queue);
    }
    if (schedule_needed != NULL)
    {
        *schedule_needed = need_schedule;
    }
    return 0;
}

void message_queues_init(void)
{
    for (uint32_t i = 0; i < NBQUEUE; i++)
    {
        message_queue_t *message_queue = &(message_queue_table.data[i]);
        *message_queue = empty_message_queue;
        message_queue_table.fids[i] = i;
    }
    for (uint32_t j = 0; j < PROCESS_TABLE_MAX_SIZE; j++)
    {
        message_queue_table.reset_releases[j] = false;
    }
    message_queue_table.fids_consume_index = 0;
    message_queue_table.fids_release_index = INDEXES_LOCK_VALUE;
    message_queue_table.current_size = 0;
}

void message_queues_notify_priority_changed(int pid, int new_priority)
{
    for (uint32_t i = 0; i < NBQUEUE; i++)
    {
        message_queue_t *message_queue = &(message_queue_table.data[i]);
        if (message_queue->size > 0)
        {
            process_node_t *tmp = message_queue->locked_processes->first;
            while (tmp != NULL)
            {
                if (tmp->content.pid == pid)
                {
                    process_node_content_t content;
                    if (process_queue_remove_first_with_pid(message_queue->locked_processes, pid, &content))
                    {
                        content.priority = new_priority;
                        process_queue_add_on_priority(message_queue->locked_processes, content);
                    }
                }
                tmp = tmp->next;
            }
        }
    }
}

void message_queues_notify_processus_killed(int pid)
{
    for (uint32_t i = 0; i < NBQUEUE; i++)
    {
        message_queue_t *message_queue = &(message_queue_table.data[i]);
        if (message_queue->size > 0)
        {
            process_queue_remove_first_with_pid(message_queue->locked_processes, pid, NULL);
        }
    }
}

int pcreate(int count)
{
    if (count < 1 || message_queue_table.current_size >= NBQUEUE || count >MESSAGE_QUEUE_BUFFER_MAX_SIZE)
    {
        return -1;
    }
    int fid = message_queue_table_register();
    message_queue_t *message_queue = &(message_queue_table.data[fid]);
    message_queue->buffer = mem_alloc(count * sizeof(int));
    if (message_queue->buffer == NULL)
    {
        message_queue_table_unregister(fid);
        return -1;
    }
    message_queue->size = count;
    message_queue->locked_processes = mem_alloc(sizeof(process_queue_t));
    assert(message_queue->locked_processes != NULL);
    *(message_queue->locked_processes) = process_queue_create();
    return fid;
}

int pcount(int fid, int *count)
{
    if (fid < 0 || fid >= NBQUEUE || message_queue_table.data[fid].buffer == NULL)
    {
        return -1;
    }
    message_queue_t *message_queue = &(message_queue_table.data[fid]);
    if (count != NULL)
    {
        if (message_queue_table.data[fid].nb_messages == 0)
        {
            *count = -(message_queue->locked_processes->size);
        }
        else
        {
            *count = message_queue->nb_messages + message_queue->locked_processes->size;
        }
    }
    return 0;
}

int preset(int fid)
{
    bool need_schedule = false;
    int result = message_queue_reset(fid, &need_schedule);
    if (need_schedule)
    {
        schedule();
    }
    return result;
}

int pdelete(int fid)
{
    bool need_schedule = false;
    int result = message_queue_reset(fid, &need_schedule);
    if (result < 0)
    {
        return result;
    }
    message_queue_t *message_queue = &(message_queue_table.data[fid]);
    mem_free(message_queue->buffer, message_queue->size * sizeof(int));
    mem_free(message_queue->locked_processes, sizeof(process_queue_t));
    *message_queue = empty_message_queue;
    message_queue_table_unregister(fid);
    if (need_schedule)
    {
        schedule();
    }
    return result;
}

int psend(int fid, int message)
{
    if (fid < 0 || fid >= NBQUEUE || message_queue_table.data[fid].buffer == NULL)
    {
        return -1;
    }
    message_queue_t *message_queue = &(message_queue_table.data[fid]);
    if (message_queue->nb_messages == 0 && message_queue->locked_processes->size > 0)
    {
        process_node_content_t content = process_queue_remove_first(message_queue->locked_processes);
        if (content.destination != NULL)
        {
            *(content.destination) = message;
        }
        if (awake(content.pid))
        {
            schedule();
        }
    }
    else if (message_queue->nb_messages == message_queue->size)
    {
        process_node_content_t content;
        content.pid = getpid();
        content.message = message;
        content.destination = NULL;
        content.priority = getprio(content.pid);
        process_queue_add_on_priority(message_queue->locked_processes, content);
        block_by_message(content.pid);
        schedule();
        if (message_queue_table.reset_releases[content.pid - 1])
        {
            message_queue_table.reset_releases[content.pid - 1] = false;
            return -1;
        }
    }
    else
    {
        message_queue_register(message_queue, message);
    }
    return 0;
}

int preceive(int fid, int *message)
{
    if (fid < 0 || fid >= NBQUEUE || message_queue_table.data[fid].buffer == NULL)
    {
        return -1;
    }
    message_queue_t *message_queue = &(message_queue_table.data[fid]);
    if (message_queue->nb_messages == message_queue->size && message_queue->locked_processes->size > 0)
    {
        int received = message_queue_unregister(message_queue);
        process_node_content_t content = process_queue_remove_first(message_queue->locked_processes);
        message_queue_register(message_queue, content.message);
        if (message != NULL)
        {
            *message = received;
        }
        if (awake(content.pid))
        {
            schedule();
        }
    }
    else if (message_queue->nb_messages == 0)
    {
        process_node_content_t content;
        content.pid = getpid();
        content.destination = message;
        content.priority = getprio(content.pid);
        process_queue_add_on_priority(message_queue->locked_processes, content);
        block_by_message(content.pid);
        schedule();
        if (message_queue_table.reset_releases[content.pid - 1])
        {
            message_queue_table.reset_releases[content.pid - 1] = false;
            return -1;
        }
    }
    else
    {
        int received = message_queue_unregister(message_queue);
        if (message != NULL)
        {
            *message = received;
        }
    }
    return 0;
}
