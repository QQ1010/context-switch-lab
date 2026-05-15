#include <stdio.h>
#include <stdbool.h>


#define TASK_COUNT 2

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_FINISHED
} TaskState;

struct TCB;
typedef void (*TaskEntry)(struct TCB *task);

// Task Controll Block
typedef struct TCB {
    int id;
    const char *name;
    TaskState state;
    TaskEntry entry;
    int step;
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
    printf("%-4s %-10s %-10s %-6s\n", "ID", "NAME", "STATE", "STEP");

    for (int i = 0; i < count; i++) {
        printf("%-4d %-10s %-10s %-6d\n",
               tasks[i].id,
               tasks[i].name,
               state_name(tasks[i].state),
               tasks[i].step);
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
        {0, "task_a", TASK_READY, task_a, 0},
        {1, "task_b", TASK_READY, task_b, 0},
    };

    print_task_table(Tasks, TASK_COUNT);
    scheduler_run(Tasks, TASK_COUNT);
    printf("\n");
    print_task_table(Tasks, TASK_COUNT);

    return 0;
}
