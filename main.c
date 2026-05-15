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

struct TCB;
typedef void (*TaskEntry)(struct TCB *task);

// Task Controll Block
typedef struct TCB {
    int id;                             // task id
    const char *name;                   // name
    TaskState state;                    // state
    TaskEntry entry;                    // entry function
    int step;                           // software progress step
    unsigned char stack[STACK_SIZE];    // per-task stack
} TCB;

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

static void task_yield(TCB *task) {
    printf("[%s] yield\n", task->name);
    task->state = TASK_READY;
}

static void task_a(TCB *task) {
    task->step ++;
    printf("[task_a] step %d\n", task->step);
    
    if(task->step >= 3) {
        printf("[task_a] finished\n");
        task->state = TASK_FINISHED;
        return;
    }
    task_yield(task);
}

static void task_b(TCB *task) {
    task->step ++;
    printf("[task_b] step %d\n", task->step);
    
    if(task->step >= 3) {
        printf("[task_b] finished\n");
        task->state = TASK_FINISHED;
        return;
    }
    task_yield(task);
}

static void print_task_table(const TCB tasks[], int count)
{
    printf("Task table:\n");
    printf("%-4s %-10s %-10s %-6s %-14s %-10s\n", "ID", "NAME", "STATE", "STEP", "STACK_BASE", "STACK_SIZE");

    for (int i = 0; i < count; i++) {
        printf("%-4d %-10s %-10s %-6d %-14p %-10zu\n",
                tasks[i].id,
                tasks[i].name,
                state_name(tasks[i].state),
                tasks[i].step,
                (void *)tasks[i].stack,
                sizeof(tasks[i].stack)
            );
    }
}


static bool all_tasks_finished(const TCB tasks[], int count) {
    for(int i = 0 ; i < count; i++) {
        if(tasks[i].state != TASK_FINISHED) {
            return false;
        }
    }
    return true;
}

static void scheduler_run(TCB tasks[], int count) {
    int next = 0;
    while(!all_tasks_finished(tasks, count)) {
        TCB *task = &tasks[next];
        next = (next + 1) % count;
        
        if(task->state != TASK_READY) {
            continue;
        }

        printf("\n[sheduler] dispatch %s\n", task->name);
        task->state = TASK_RUNNING;
        task->entry(task);
    }
}

int main(void)
{
    printf("context-switch-lab\n");
    TCB Tasks[TASK_COUNT] = {
        {
            .id = 0,
            .name = "task_a",
            .state = TASK_READY,
            .entry = task_a,
            .step = 0,
        },
        {
            .id = 1,
            .name = "task_b",
            .state = TASK_READY,
            .entry = task_b,
            .step = 0,
        }
    };

    print_task_table(Tasks, TASK_COUNT);
    scheduler_run(Tasks, TASK_COUNT);
    printf("\n");
    print_task_table(Tasks, TASK_COUNT);

    return 0;
}
