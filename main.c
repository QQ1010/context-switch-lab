#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ucontext.h>


#define TASK_COUNT 2
#define STACK_SIZE (16 * 1024)        // 16 KB = 16 * 1024

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_FINISHED
} TaskState;

typedef void (*TaskEntry)(void);

// Task Control Block
typedef struct TCB {
    int id;                             // task id
    const char *name;                   // name
    TaskState state;                    // state
    TaskEntry entry;                    // entry function
    ucontext_t context;                 // ucontext, include program counter, stack pointer, registers, signal mask, stack information, link context
    unsigned char stack[STACK_SIZE];    // per-task stack
} TCB;

// use ucontext_t scheduler and current task
static ucontext_t scheduler_context;
static TCB *current_task = NULL;

static const char *state_name(TaskState state) {
    switch(state) {
        case TASK_READY:
            return "READY";
        case TASK_RUNNING:
            return "RUNNING";
        case TASK_FINISHED:
            return "FINISHED";
        default:
            return "UNKNOWN";
    }
}


static void trace_context(const char *action, const TCB *task) {
    printf("[context] %-7s task=%s stack_base=%p stack_size=%zu state=%s\n",
        action,
        task->name,
        (void *) task->stack,
        sizeof(task->stack),
        state_name(task->state)
    );
}

static void print_task_table(const TCB tasks[], int count)
{
    printf("Task table:\n");
    printf("%-4s %-10s %-10s %-14s %-10s\n", "ID", "NAME", "STATE",  "STACK_BASE", "STACK_SIZE");

    for (int i = 0; i < count; i++) {
        printf("%-4d %-10s %-10s %-14p %-10zu\n",
                tasks[i].id,
                tasks[i].name,
                state_name(tasks[i].state),
                (void *)tasks[i].stack,
                sizeof(tasks[i].stack)
            );
    }
}


static void die(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

static void task_yield(void) {
    printf("[%s] yield: %s -> scheduler\n", current_task->name, current_task->name);
    
    if(current_task->state != TASK_FINISHED) {
        current_task->state = TASK_READY;
    }
    
    trace_context("save", current_task);

    // save current task context and resume scheduler context
    if(swapcontext(&current_task->context, &scheduler_context) == -1) {
        die("swapcontext");
    }
}

static void task_a(void) {
    
    for(int i = 1 ; i <= 3 ; i++) {
        printf("[task_a] step %d\n", i);
        task_yield();
    }
    
    printf("[task_a] finished\n");
    current_task->state = TASK_FINISHED;
    task_yield();
}

static void task_b(void) {
    for(int i = 1; i <= 3 ; i++) {
        printf("[task_b] step %d\n", i);
        task_yield();
    }
    
    printf("[task_b] finished\n");
    current_task->state = TASK_FINISHED;
    task_yield();
}

static void init_task(TCB *task) {
    if(getcontext(&task->context) == -1) {
        die("getcontext");
    }
    task->context.uc_stack.ss_sp = task->stack;         // start position
    task->context.uc_stack.ss_size = sizeof(task->stack);   // stack size
    task->context.uc_stack.ss_flags = 0;                 // 0: stack available
    task->context.uc_link = &scheduler_context;         // when the task function return, go back to scheduler_context
    makecontext(&task->context, task->entry, 0);   // first time used to start from task->entry function
}


static bool all_tasks_finished(const TCB tasks[], int count) {
    for(int i = 0 ; i < count; i++) {
        if(tasks[i].state != TASK_FINISHED) {
            return false;
        }
    }
    return true;
}

static bool has_ready_task(const TCB tasks[], int count) {
    for(int i = 0 ; i < count; i++) {
        if(tasks[i].state == TASK_READY) {
            return true;
        }
    }
    return false;
}

static void idle_hook(void) {
    printf("[idle] no runnable tasks; MCU cound enter WFI/WFE here\n");
}

static void scheduler_run(TCB tasks[], int count) {
    int next = 0;
    while(!all_tasks_finished(tasks, count)) {
        if (!has_ready_task(tasks, count)) {
            idle_hook();
            break;
        }

        TCB *task = &tasks[next];
        next = (next + 1) % count;
        
        if(task->state != TASK_READY) {
            continue;
        }

        current_task = task;
        task->state = TASK_RUNNING;

        printf("\n[scheduler] switch: scheduler -> %s\n", task->name);
        trace_context("restore", task);

        // save scheduler context and resume task context
        if(swapcontext(&scheduler_context, &task->context) == -1) {
            die("swapcontext");
        }
    }
    current_task = NULL;
    printf("\n[scheduler] all tasks finished\n");
    idle_hook();
}

int main(void)
{
    printf("context-switch-lab\n");
    TCB tasks[TASK_COUNT] = {
        {
            .id = 0,
            .name = "task_a",
            .state = TASK_READY,
            .entry = task_a,
        },
        {
            .id = 1,
            .name = "task_b",
            .state = TASK_READY,
            .entry = task_b,
        }
    };

    for(int i = 0 ; i < TASK_COUNT; i++) {
        init_task(&tasks[i]);
    }

    print_task_table(tasks, TASK_COUNT);
    scheduler_run(tasks, TASK_COUNT);
    print_task_table(tasks, TASK_COUNT);

    return 0;
}
