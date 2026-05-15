#include <stdio.h>

#define TASK_COUNT 2

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_FINISHED
} TaskState;

// Task Controll Block
typedef struct TCB {
    int id;
    const char *name;
    TaskState state;
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

static void printf_task_able(const TCB tasks[], int count) {
    printf("Task Table:\n");
    printf("%-4s %-10s %-10s\n", "ID", "NAME", "STATE");
    for(int i = 0 ; i < count; i ++) {
        printf("%-4d %-10s %-10s\n", tasks[i].id, tasks[i].name, state_name(tasks[i].state));
    }
}

int main(void)
{
    printf("context-switch-lab\n");
    TCB Tasks[TASK_COUNT] = {
        {0, "task_a", TASK_READY},
        {1, "task_b", TASK_READY},
    };
    printf_task_able(Tasks, TASK_COUNT);
    return 0;
}
