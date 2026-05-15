#include <stdio.h>

#define TASK_COUNT 2

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_FINISHED
} TaskState;

typedef void (*TaskEntry)(void);

// Task Controll Block
typedef struct TCB {
    int id;
    const char *name;
    TaskState state;
    TaskEntry entry;
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

static void task_a(void) {
    printf("[task a] running\n");
}

static void task_b(void) {
    printf("[task b] running\n");
}

static void printf_task_table(const TCB tasks[], int count) {
    printf("Task Table:\n");
    printf("%-4s %-10s %-10s\n", "ID", "NAME", "STATE");
    for(int i = 0 ; i < count; i ++) {
        printf("%-4d %-10s %-10s\n", tasks[i].id, tasks[i].name, state_name(tasks[i].state));
    }
}

static void sheduler_run(TCB tasks[], int count) {
    for(int i = 0 ; i < count; i++) {
        TCB *task = &tasks[i];
        if(task->state != TASK_READY) {
            continue;
        }
        printf("\n[sheduler] dispatch %s\n", task->name);
        task->state = TASK_RUNNING;
        task->entry();
        task->state = TASK_FINISHED;
        printf("[scheduler] %s finished\n", task->name);
    }
}

int main(void)
{
    printf("context-switch-lab\n");
    TCB Tasks[TASK_COUNT] = {
        {0, "task_a", TASK_READY, task_a},
        {1, "task_b", TASK_READY, task_b},
    };

    printf_task_table(Tasks, TASK_COUNT);
    sheduler_run(Tasks, TASK_COUNT);
    printf("\n");
    printf_task_table(Tasks, TASK_COUNT);

    return 0;
}
