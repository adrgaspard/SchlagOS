#include <stdint.h>

#include "../shared/debug.h"
#include "../shared/string.h"
#include "../shared/consts.h"
#include "../shared/queue.h"
#include "../shared/stdio.h"
#include "mem.h"
#include "user_stack_mem.h"
#include "cpu.h"
#include "process.h"
#include "message_queue.h"
#include "../shared/timer.h"

#define INDEXES_LOCK_VALUE -1
#define STACK_PARAMETER_TYPE void *
#define STACK_PARAMETER_SIZE sizeof(STACK_PARAMETER_TYPE)
#define USER_ADDITIONNAL_STACK_SIZE 2 * STACK_PARAMETER_SIZE
#define KERNEL_STACK_SIZE 2048

typedef enum process_state_enum
{
    RUNNING = 1,
    WAITING = 2,
    BLOCKED_BY_MESSAGE = 4,
    BLOCKED_BY_IO = 8,
    BLOCKED_BY_CHILD = 16,
    BLOCKED = BLOCKED_BY_MESSAGE | BLOCKED_BY_IO | BLOCKED_BY_CHILD,
    SLEEPING = 32,
    ZOMBIE = 64
} process_state_enum;

typedef struct process_child_node_t
{
    int pid;
    struct process_child_node_t *next;
} process_child_node_t, *process_child_list_t;

typedef struct process_t
{
    int pid;
    int father_pid;
    process_child_list_t childs_pids;
    char name[PROCESS_NAME_MAX_LENGTH];
    process_state_enum state;
    int priority;
    void *user_stack;
    void *kernel_stack;
    uint64_t user_stack_size;
    uint32_t execution_context[PROCESS_REGISTERS_SAVE_LENGTH];
    link link_field;
    int return_code;
    int waiting_code;
    bool deallocated;
    unsigned long sleep_time;
    bool sealed;
} process_t;

typedef struct process_table_t 
{
    process_t data[PROCESS_TABLE_MAX_SIZE];
    int pids[PROCESS_TABLE_MAX_SIZE];
    int pids_to_zombify[PROCESS_TABLE_MAX_SIZE];
    int pids_to_destroy[PROCESS_TABLE_MAX_SIZE];
    int pids_waiting[PROCESS_TABLE_MAX_SIZE];
    int pids_consume_index;
    int pids_release_index;
    uint32_t current_size;
    uint32_t nb_to_zombify;
    uint32_t nb_to_destroy;
    uint32_t nb_waiting;
    process_t *running_process;
} process_table_t;

process_table_t process_table = {};
const process_t no_process = { .pid = PROCESS_NOT_FOUND_CODE, .name = "\0", .state = 0, .priority = 0, .user_stack = NULL, .kernel_stack = NULL, .user_stack_size = 0, .execution_context = {}, .link_field = {}, .deallocated = false, .return_code = -1, .sealed = false };
LIST_HEAD(process_queue);

extern void context_switch(void *old_context, void *new_context);

extern void exit_handler(void);

static process_child_list_t process_child_list_create()
{
    return NULL;
}

static void process_child_list_add_head(process_child_list_t *list, int pid)
{
    process_child_node_t *new = mem_alloc(sizeof(process_child_node_t));
    assert(new != NULL);
    new->next = *list;
    new->pid = pid;
    *list = new;
}

static bool process_child_list_remove(process_child_list_t *list, int pid)
{
    process_child_node_t *previous = NULL;
    process_child_node_t *current = *list;
    while (current != NULL)
    {
        if (current->pid == pid)
        {
            if (previous == NULL)
            {
                *list = current->next;
            }
            else
            {
                previous->next = current->next;
            }
            mem_free(current, sizeof(process_child_node_t));
            return true;
        }
        previous = current;
        current = current->next;
    }
    return false;
}

static bool process_child_list_is_empty(process_child_list_t list)
{
    return list == NULL;
}

static int process_child_list_remove_head(process_child_list_t *list)
{
    assert(*list != NULL);
    int result = (*list)->pid;
    process_child_node_t *next = (*list)->next;
    mem_free(*list, sizeof(process_child_node_t));
    *list = next;
    return result;
}

static int process_table_register()
{
    assert(process_table.pids_consume_index != INDEXES_LOCK_VALUE);
    int pid = process_table.pids[process_table.pids_consume_index];
    if (process_table.pids_release_index == INDEXES_LOCK_VALUE)
    {
        process_table.pids_release_index = process_table.pids_consume_index;
    }
    process_table.pids_consume_index = (process_table.pids_consume_index + 1) % PROCESS_TABLE_MAX_SIZE;
    if (process_table.pids_consume_index == process_table.pids_release_index)
    {
        process_table.pids_consume_index = INDEXES_LOCK_VALUE;
    }
    process_table.current_size++;
    return pid;
}

static void process_table_unregister(int pid)
{
    assert(process_table.pids_release_index != INDEXES_LOCK_VALUE);
    process_table.pids[process_table.pids_release_index] = pid;
    if (process_table.pids_consume_index == INDEXES_LOCK_VALUE)
    {
        process_table.pids_consume_index = process_table.pids_release_index;
    }
    process_table.pids_release_index = (process_table.pids_release_index + 1) % PROCESS_TABLE_MAX_SIZE;
    if (process_table.pids_release_index == process_table.pids_consume_index)
    {
        process_table.pids_release_index = INDEXES_LOCK_VALUE;
    }
    process_table.current_size--;
}

static process_t* get_process(int pid)
{
    assert(pid > 0 && pid <= PROCESS_TABLE_MAX_SIZE);
    return &(process_table.data[pid - 1]);
}

static void process_switch(process_t *old, process_t *new)
{
    new->state = RUNNING;
    process_table.running_process = new;
    if(old->state == RUNNING){
        old->state = WAITING;
        queue_add(old, &process_queue, process_t, link_field, priority);
    }
    context_switch((void *)(old->execution_context), (void *)(new->execution_context));
}

static void deallocate(process_t *process)
{
    assert(!(process->deallocated));
    if (process->pid != 1)
    {
        user_stack_free(process->user_stack, process->user_stack_size);
        process->user_stack = NULL;
        process->user_stack_size = 0;
    }
    mem_free(process->kernel_stack, KERNEL_STACK_SIZE);
    process->kernel_stack = NULL;
    process->deallocated = true;
}

static void zombify(int pid)
{
    assert(pid != 1);
    process_t *process = get_process(pid);
    assert(process != NULL);
    assert(process->state == ZOMBIE && process_table.running_process != process);
    process_t *init = get_process(1);
    process_t *father = get_process(process->father_pid);
    assert(father != NULL);
    deallocate(process);
    process_child_node_t *current = process->childs_pids;
    process_child_node_t *next = NULL;
    while (current != NULL)
    {
        next = current->next;
        process_t *child = get_process(current->pid);
        if (child->deallocated)
        {
            request_destroy(child->pid);
        }
        else
        {
            process_child_list_remove(&(process->childs_pids), current->pid);
            child->father_pid = 1;
            process_child_list_add_head(&(init->childs_pids), current->pid);
        }
        current = next;
    }

    if(father->state == BLOCKED_BY_CHILD && !(process->sealed) && (father->waiting_code < 0 || father->waiting_code == process->pid)){
        // Dans ce cas là on doit réveiller le père car il attend ce fils
        father->waiting_code = process->pid; // Juste au cas où le père attendait un fils quelconque, on lui indique quel fils consulter à son réveil
        father->state = WAITING;
        queue_add(father, &process_queue, process_t, link_field, priority);
    }

    if (!(process->sealed) && (father->state == ZOMBIE || father->pid == 1)) // 1 récupère les orphelins, il ne fait pas attention à ses enfants
    {
        request_destroy(pid);
    }
    schedule(); // On ne fait le schedule que à la fin car on veut que le dernier if soit forcément passé, sinon on risque de request_destroy un processus déjà détruit
}

static void destroy(int pid)
{
    assert(pid != 1);
    process_t *process = get_process(pid);
    assert(process != NULL);
    assert(process->state != RUNNING && process_table.running_process != process);
    process_child_list_remove(&(get_process(process->father_pid)->childs_pids), pid);
    bool has_no_child = process_child_list_is_empty(process->childs_pids);
    while (!has_no_child)
    {
        int child_pid = process_child_list_remove_head(&(process->childs_pids));
        process_t *child = get_process(child_pid);
        if (child->deallocated)
        {
            request_destroy(child->pid);
        }
        else
        {
            child->father_pid = 1;
            process_child_list_add_head(&(get_process(1)->childs_pids), pid);            
        }
        has_no_child = process_child_list_is_empty(process->childs_pids);
    }
    process_table_unregister(pid);
    if (!process->deallocated)
    {
        deallocate(process);
    }
    else
    {
        assert(process->user_stack == NULL && process->user_stack_size == 0 && process->kernel_stack == NULL);
    }
    *process = no_process;
}

static void clean_processes()
{
    while (process_table.nb_to_zombify > 0)
    {
        int pid_to_zombify = process_table.pids_to_zombify[process_table.nb_to_zombify - 1];
        if (process_table.running_process != NULL && process_table.running_process->pid == pid_to_zombify)
        {
            if (process_table.nb_to_zombify == 1)
            {
                break;
            }
            process_table.pids_to_zombify[process_table.nb_to_zombify - 1] = process_table.pids_to_zombify[0];
            process_table.pids_to_zombify[0] = pid_to_zombify;
            pid_to_zombify = process_table.pids_to_zombify[process_table.nb_to_zombify - 1];
        }
        process_table.nb_to_zombify--;
        process_table.pids_to_zombify[process_table.nb_to_zombify] = PROCESS_NOT_FOUND_CODE;
        zombify(pid_to_zombify);
    }
    while (process_table.nb_to_destroy > 0)
    {
        int pid_to_destroy = process_table.pids_to_destroy[process_table.nb_to_destroy - 1];
        if (process_table.running_process != NULL && process_table.running_process->pid == pid_to_destroy)
        {
            if (process_table.nb_to_destroy == 1)
            {
                break;
            }
            process_table.pids_to_destroy[process_table.nb_to_destroy - 1] = process_table.pids_to_destroy[0];
            process_table.pids_to_destroy[0] = pid_to_destroy;
            pid_to_destroy = process_table.pids_to_destroy[process_table.nb_to_destroy - 1];
        }
        process_table.nb_to_destroy--;
        process_table.pids_to_destroy[process_table.nb_to_destroy] = PROCESS_NOT_FOUND_CODE;
        destroy(pid_to_destroy);
    }
}

void process_table_init(void)
{
    for (uint32_t i = 0; i < PROCESS_TABLE_MAX_SIZE; i++)
    {
        process_table.data[i] = no_process;
        process_table.pids[i] = i + 1;
        process_table.pids_to_zombify[i] = PROCESS_NOT_FOUND_CODE;
        process_table.pids_to_destroy[i] = PROCESS_NOT_FOUND_CODE;
        process_table.pids_waiting[i] = PROCESS_NOT_FOUND_CODE;
    }
    process_table.pids_consume_index = 0;
    process_table.pids_release_index = INDEXES_LOCK_VALUE;
    process_table.current_size = 0;
    process_table.nb_to_zombify = 0;
    process_table.nb_to_destroy = 0;
    process_table.nb_waiting = 0;
    process_table.running_process = NULL;
}

void schedule(void)
{
    clean_processes();
    process_t *old = process_table.running_process;
    process_t *candidate = queue_top(&process_queue, process_t, link_field); // On fait un top: On ne fait que regarder la valeur sans la retirer de la file
    if (old != NULL && old->pid != 1 && process_table.current_size >= PROCESS_TABLE_MAX_SIZE)
    {
        candidate = get_process(1);
    }
    if(old == NULL)
    {
        // On est dans le cas où on lance le premier processus
        candidate = queue_out(&process_queue, process_t, link_field);
        candidate->state = RUNNING;
        process_table.running_process = candidate;
        context_switch(candidate->execution_context, candidate->execution_context);
    }
    else if(candidate == NULL || old->pid == candidate->pid || (old->state == RUNNING && old->priority > candidate->priority)) // Ce cas est trivial: Le processus courant n'a pas à être remplacé
    {
        return;
    }
    else
    {
        candidate = queue_out(&process_queue, process_t, link_field); // Là on retire effectivement le candidat de la file
        process_switch(old, candidate);
    }
}

void request_zombify(int pid)
{
    process_t *process = get_process(pid);
    assert(process->state != ZOMBIE);
    process_table.pids_to_zombify[process_table.nb_to_zombify] = pid;
    process_table.nb_to_zombify++;
    if(pid != process_table.running_process->pid && (process->state & (WAITING | RUNNING)) == process->state)
    {
        queue_del(process, link_field);
    }
    message_queues_notify_processus_killed(pid);
    process->state = ZOMBIE;
}

void request_destroy(int pid)
{
    process_t *process = get_process(pid);
    assert(!(process->sealed));
    process->sealed = true;
    process_table.pids_to_destroy[process_table.nb_to_destroy] = pid;
    process_table.nb_to_destroy++;
    if (process->state != ZOMBIE)
    {
        if(pid != process_table.running_process->pid)
        {
            queue_del(process, link_field);
        }
        message_queues_notify_processus_killed(pid);
    }
}

extern void user_switch(void);
extern void user_exit_handler(void);

int start(int (*program)(void *), unsigned long stack_size, int priority, const char *name, void *argument)
{
    if (priority < PROCESS_MIN_PRIORITY || priority > PROCESS_MAX_PRIORITY || process_table.current_size >= PROCESS_TABLE_MAX_SIZE || stack_size + USER_ADDITIONNAL_STACK_SIZE <= stack_size)
    {
        return -1;
    }
    if (stack_size < PROCESS_MIN_STACK_SIZE)
    {
        stack_size = PROCESS_MIN_STACK_SIZE;
    }
    int pid = process_table_register();
    process_t *process = get_process(pid);
    process->pid = pid;
    process->father_pid = getpid();
    process->childs_pids = process_child_list_create();
    process->priority = priority;
    process->return_code = 0;
    process->deallocated = false;
    process->link_field.prev = NULL;
    process->link_field.next = NULL;
    for (uint32_t i = 0; i < PROCESS_REGISTERS_SAVE_LENGTH; i++)
    {
        process->execution_context[i] = 0;
    }
    for (uint32_t i = 0; i < PROCESS_NAME_MAX_LENGTH; i++)
    {
        process->name[i] = name[i];
        if (name[i] == '\0')
        {
            break;
        }
    }
    process->name[PROCESS_NAME_MAX_LENGTH - 1] = '\0';
    process->kernel_stack = mem_alloc(KERNEL_STACK_SIZE);
    if (process->kernel_stack == NULL)
    {
        process_table_unregister(pid);
        return -1;
    }
    if (process->father_pid != PROCESS_NOT_FOUND_CODE)
    {
        assert(process->pid != 1);
        process->user_stack_size = stack_size + USER_ADDITIONNAL_STACK_SIZE;
        process->user_stack = user_stack_alloc(process->user_stack_size);
        if (process->user_stack == NULL)
        {
            process_table_unregister(pid);
            mem_free(process->kernel_stack, KERNEL_STACK_SIZE);
            return -1;
        }
        process_t *father = get_process(process->father_pid);
        process_child_list_add_head(&(father->childs_pids), process->pid);
        void** user_stack_arg = (STACK_PARAMETER_TYPE *)(process->user_stack + process->user_stack_size - 1 * STACK_PARAMETER_SIZE);
        void** user_stack_exit = (STACK_PARAMETER_TYPE *)(process->user_stack + process->user_stack_size - 2 * STACK_PARAMETER_SIZE);
        *user_stack_arg = argument;
        *user_stack_exit = user_exit_handler;
        void** kernel_stack_data_segment = (STACK_PARAMETER_TYPE *)(process->kernel_stack + KERNEL_STACK_SIZE - 1 * STACK_PARAMETER_SIZE);
        void** kernel_stack_user_stack_top = (STACK_PARAMETER_TYPE *)(process->kernel_stack + KERNEL_STACK_SIZE - 2 * STACK_PARAMETER_SIZE);
        void** kernel_stack_eflags = (STACK_PARAMETER_TYPE *)(process->kernel_stack + KERNEL_STACK_SIZE - 3 * STACK_PARAMETER_SIZE);
        void** kernel_stack_code_segment = (STACK_PARAMETER_TYPE *)(process->kernel_stack + KERNEL_STACK_SIZE - 4 * STACK_PARAMETER_SIZE);
        void** stack_function_ptr = (STACK_PARAMETER_TYPE *)(process->kernel_stack + KERNEL_STACK_SIZE - 5 * STACK_PARAMETER_SIZE);
        void** iret_function_ptr = (STACK_PARAMETER_TYPE *)(process->kernel_stack + KERNEL_STACK_SIZE - 6 * STACK_PARAMETER_SIZE);
        *kernel_stack_data_segment = (STACK_PARAMETER_TYPE) DATA_SEGMENT_USER_VALUE;    
        *kernel_stack_user_stack_top = (STACK_PARAMETER_TYPE) user_stack_exit;
        *kernel_stack_eflags = (STACK_PARAMETER_TYPE) EFLAGS_USER_VALUE;
        *kernel_stack_code_segment = (STACK_PARAMETER_TYPE) CODE_SEGMENT_USER_VALUE;
        *stack_function_ptr = (STACK_PARAMETER_TYPE) program;
        *iret_function_ptr = (STACK_PARAMETER_TYPE) user_switch;
        process->execution_context[0] = 0;
        process->execution_context[1] = (uint32_t)(STACK_PARAMETER_TYPE) iret_function_ptr;
        process->execution_context[2] = 0;
        process->execution_context[3] = 0;
        process->execution_context[4] = 0;
        process->execution_context[5] = (uint32_t) stack_function_ptr;
    }
    else
    {
        assert(process->pid == 1);
        assert(process->kernel_stack != NULL);
        process->user_stack_size = 0;
        process->user_stack = NULL;
        void** stack_arg = (void**)(process->kernel_stack + KERNEL_STACK_SIZE - 1 * STACK_PARAMETER_SIZE);
        void** stack_exit = (void**)(process->kernel_stack + KERNEL_STACK_SIZE - 2 * STACK_PARAMETER_SIZE);
        void** stack_ptr_function = (void**)(process->kernel_stack + KERNEL_STACK_SIZE - 3 * STACK_PARAMETER_SIZE);
        *stack_arg = argument;
        *stack_exit = exit_handler;
        *stack_ptr_function = program;
        process->execution_context[0] = 0;
        process->execution_context[1] = (uint32_t)(STACK_PARAMETER_TYPE) stack_ptr_function;
        process->execution_context[2] = 0;
        process->execution_context[3] = 0;
        process->execution_context[4] = 0;
        process->execution_context[5] = (size_t) process->kernel_stack + KERNEL_STACK_SIZE;
    }
    process->state = WAITING;
    queue_add(process, &process_queue, process_t, link_field, priority);
    if(process_table.running_process == NULL || priority > process_table.running_process->priority)
    {
        // On appelle à la main schedule car, running_process a normalement la plus haute priorité (sinon il ne serait pas running), donc il faut refaire un ordonnancement pour lancer le fils qui est prioritaire
        schedule();
    }
    return process->pid;
}

/**
 * @brief Cette fonction termine le processus courant
 * 
 * @param retval Code de retour du processus
 */
void exit(int retval)
{

    process_t *this = process_table.running_process;
    this->return_code = retval;
    request_zombify(this->pid);
    schedule(); 
    while(1); // Cette fonction doit être "noreturn", donc un while 1 est nécessaire (selon les spécifications)
}

/**
 * @brief Cette fonction termine un processus donné 
 * 
 * @param pid Identifiant du processus à terminer
 * @return int 0 si tout s'est bien passé, -1 sinon
 */
int kill(int pid){
    if(pid < 1 || pid > PROCESS_TABLE_MAX_SIZE) return -1;

    process_t *p = get_process(pid);
    
    if(p->pid == PROCESS_NOT_FOUND_CODE || p->state == ZOMBIE){
        return -1;
    }

    //If process is in a waiting queue (waitpid, wait i/o, wait clock,...), remove it !
    if(p->state == SLEEPING)
    {
        process_table.nb_waiting--;
        process_table.pids_waiting[process_table.nb_waiting] = PROCESS_NOT_FOUND_CODE;
    }

    p->return_code = 0;
    request_zombify(pid);
    schedule(); 
    return 0;
}

/**
 * @brief Cette fonction endort le processus jusqu'à ce que le fils spécifié (ou un de ses fils) ait été terminé et ait donné sa valeur de retour
 * 
 * @param pid Identifiant du fils à attendre, si c'est négatif on récupère le code de retour de n'importe quel fils
 * @param retvalp Pointeur sur lequel on écrira la valeur de retour du processus
 * @return int 0 si tout s'est bien passé, -1 sinon
 */
int waitpid(int pid, int *retvalp)
{
    if(pid < 0)
    {
        process_t *this = process_table.running_process;
        process_child_node_t *childs = this->childs_pids;

        int nb_childs = 0;
        while(childs != NULL){
            process_t *p = get_process(childs->pid);
            if (!(p->sealed))
            {
                if(p->state == ZOMBIE)
                {
                    if(retvalp != NULL)
                    {
                        *retvalp = p->return_code;
                    }
                    request_destroy(p->pid);
                    return p->pid;
                }
                nb_childs++;
            }
            childs = childs->next;
        }

        if(nb_childs > 0){
            // Aucun fils n'était zombie, mais il existe des fils, il suffit alors d'attendre que l'un d'eux devienne zombie
            this->state = BLOCKED_BY_CHILD;
            this->waiting_code = -1;
            schedule();
            // Normalement on reprend à cet endroit du code au réveil
            process_t *p = get_process(this->waiting_code);
            assert(p->pid != PROCESS_NOT_FOUND_CODE);
            if(retvalp != NULL){
                *retvalp = p->return_code;
            }
            request_destroy(p->pid);
            return p->pid;
        }
        else
        {
            return -1;
        }
    }
    else
    {
        if(pid < 1 || pid > PROCESS_TABLE_MAX_SIZE) return -1;
        process_t *p = get_process(pid);
        if(p->pid == PROCESS_NOT_FOUND_CODE || p->sealed || p->father_pid != getpid())
        {
            return -1;
        }
        if(p->state == ZOMBIE)
        {
            if(retvalp != NULL){
                    *retvalp = p->return_code;
            }
            request_destroy(pid);
            return pid;
        }
        else
        {
            // On endort le père tant que le fils n'a pas répondu
            process_t *this = process_table.running_process;
            this->state = BLOCKED_BY_CHILD;
            this->waiting_code = pid; // On précise bien que le père attend un fils en particulier
            schedule();
            // Normalement, quand le processus sera réveillé il reprendra à cet endroit là du code
            if(retvalp != NULL){
                    *retvalp = p->return_code;
            }
            request_destroy(pid);
            return pid;
        }
    }
}

/**
 * @brief Donne le pid du processus courant
 * 
 * @return int Le pid
 */
int getpid(void)
{
    if (process_table.running_process != NULL)
    {
        return process_table.running_process->pid;
    }
    return PROCESS_NOT_FOUND_CODE;
}

void wait_clock(unsigned long clock){
    if(clock > 0){ // Au cas où un petit malin tenterait un wait_clock(0)
        process_t *this = process_table.running_process;
        this->state = SLEEPING;
        this->sleep_time = clock;
        process_table.pids_waiting[process_table.nb_waiting] = this->pid;
        process_table.nb_waiting++;
        schedule();
    }
}

void clock_tick(){
    bool awaken = false;
    for (uint32_t i = 0; i < process_table.nb_waiting; i++)
    {
        process_t *process = get_process(process_table.pids_waiting[i]);
        if (process->sleep_time <= current_clock())
        {
            awaken = true;
            process->state = WAITING;
            process_table.nb_waiting--;
            process_table.pids_waiting[i] = process_table.pids_waiting[process_table.nb_waiting];
            process_table.pids_waiting[process_table.nb_waiting] = PROCESS_NOT_FOUND_CODE;
            i--;
            queue_add(process, &process_queue, process_t, link_field, priority);
        }
    }
    if (awaken)
    {
        schedule();
    }
}

int getprio(int pid)
{
    return get_process(pid)->priority;
}

void* getcontext(int pid)
{
    return get_process(pid)->execution_context;
}

int chprio(int pid, int newprio){
    if(pid < 1 || pid > PROCESS_TABLE_MAX_SIZE) return -1;
    if(newprio < PROCESS_MIN_PRIORITY || newprio > PROCESS_MAX_PRIORITY) return -1;

    process_t *p = get_process(pid);
    
    if(p->pid == PROCESS_NOT_FOUND_CODE || p->state == ZOMBIE){
        return -1;
    }

    int oldprio = p->priority;
    p->priority = newprio;
    if (p->state == BLOCKED_BY_MESSAGE)
    {
        message_queues_notify_priority_changed(pid, newprio);
    }

    // On retrie le process dans la file avec sa nouvelle priorité (si ce n'est pas le proc courant)
    if(process_table.running_process->pid != pid && (p->state & BLOCKED) == 0){
        queue_del(p, link_field);
        queue_add(p, &process_queue, process_t, link_field, priority);
    }

    process_t *next_proc = queue_top(&process_queue, process_t, link_field);

    // Si le nouveau processus est plus prioritaire que l'actuel processus, on fait un appel du scheduler 
    if(newprio > process_table.running_process->priority || (process_table.running_process->pid == pid && next_proc->priority > newprio)){
        schedule();
    }

    return oldprio;
}

bool awake(int pid)
{
    process_t *process = get_process(pid);
    process->state = WAITING;
    queue_add(process, &process_queue, process_t, link_field, priority);
    return process->priority > process_table.running_process->priority;
}

void block_by_message(int pid)
{
    process_t *process = get_process(pid);
    process->state = BLOCKED_BY_MESSAGE;
    if(pid != process_table.running_process->pid)
    {
        queue_del(process, link_field);
    } 
}
