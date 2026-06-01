#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ucontext.h>

#define TASK_COUNT 2
#define STACK_SIZE (16 * 1024)        // 16 KB = 16 * 1024
#define EVENT_QUEUE_SIZE 16

static bool in_critical = false;
static int system_tick = 0;

typedef enum {
    EVENT_TIMER,        // TIMER interrupt
    EVENT_UART_RX,      // UART interrupt
    EVENT_BUTTON,       // GPIO interrupt
    EVENT_SHUTDOWN      // shutdown task
} EventType;

typedef struct {
    int target_task_id;
    EventType type;
    int value;
} Event;

typedef struct {
    Event events[EVENT_QUEUE_SIZE];
    int head;
    int tail;
    int count;
} EventQueue;

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_FINISHED
} TaskState;

typedef void (*TaskEntry)(void);

// Task Control Block
typedef struct TCB {
    int id;                             // task id
    const char *name;                   // name
    TaskState state;                    // state
    int wake_tick;                      // system_tick >= wake_tick, wake up
    TaskEntry entry;                    // entry function
    ucontext_t context;                 // ucontext, include program counter, stack pointer, registers, signal mask, stack information, link context
    unsigned char stack[STACK_SIZE];    // per-task stack
} TCB;

// Global variables
// use ucontext_t scheduler and current task
static ucontext_t scheduler_context;
static TCB *current_task = NULL;
static EventQueue event_queue;
static Event current_event;
static bool has_current_event = false;

static const char *event_name(EventType type) {
    switch(type) {
        case EVENT_TIMER:
            return "TIMER";
        case EVENT_UART_RX:
            return "UART_RX";
        case EVENT_BUTTON:
            return "BUTTON";
        case EVENT_SHUTDOWN:
            return "SHUTDOWN";
        default:
            return "UNKNOWN";
    }
}

static const char *state_name(TaskState state) {
    switch(state) {
        case TASK_READY:
            return "READY";
        case TASK_RUNNING:
            return "RUNNING";
        case TASK_FINISHED:
            return "FINISHED";
        case TASK_BLOCKED:
            return "BLOCKED";
        default:
            return "UNKNOWN";
    }
}

static void scheduler_tick(TCB tasks[], int count) {
    system_tick++;

    printf("[tick] system_tick=%d\n", system_tick);

    for (int i = 0; i < count; i++) {
        if (tasks[i].state == TASK_BLOCKED &&
            system_tick >= tasks[i].wake_tick) {
            tasks[i].state = TASK_READY;
            printf("[tick] wake %s\n", tasks[i].name);
        }
    }
}

static void enter_critical(void) {
    if(in_critical) {
        printf("[critical] nested enter\n");
        return;
    }
    in_critical = true;
    printf("[critical] enter\n");
}

static void exit_critical(void) {
    if(!in_critical) {
        printf("[critical] exit without enter\n");
        return;
    }
    in_critical = false;
    printf("[critical] exit\n");
}

static bool event_push(EventQueue *queue, Event event) {
    
    enter_critical();
    
    if(queue->count == EVENT_QUEUE_SIZE) {
        return false;
    }

    queue->events[queue->tail] = event;
    queue->tail = (queue->tail + 1) % EVENT_QUEUE_SIZE;
    queue->count ++;

    printf("[event_queue] push %s -> target=%d value=%d\n",
           event_name(event.type),
           event.target_task_id,
           event.value);

    exit_critical();
    return true;
}

static bool event_pop(EventQueue *queue, Event *event) {
    
    enter_critical();
    
    if(queue->count == 0) {
        return false;
    }

    *event = queue->events[queue->head];
    queue->head = (queue->head+1) % EVENT_QUEUE_SIZE;
    queue->count --;

    printf("[event_queue] pop %s -> target=%d value=%d\n",
           event_name(event->type),
           event->target_task_id,
           event->value);

    exit_critical();
    return true;
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
    
    if(current_task->state == TASK_RUNNING) {
        current_task->state = TASK_READY;
    }
    
    trace_context("save", current_task);

    // save current task context and resume scheduler context
    if(swapcontext(&current_task->context, &scheduler_context) == -1) {
        die("swapcontext");
    }
}

static void task_delay(int ticks) {
    if(ticks <= 0) {
        return ;
    }
    current_task->wake_tick = system_tick + ticks;
    current_task->state = TASK_BLOCKED;

    printf("[%s] delay %d ticks: wake_tick=%d\n",
           current_task->name,
           ticks,
           current_task->wake_tick);

    task_yield();

}

static void control_task(void) {
    while(1) {
        if(has_current_event) {
            switch (current_event.type) {
                case EVENT_TIMER:
                    printf("[control_task] timer tick=%d\n", current_event.value);
                    task_delay(2);
                    break;
                case EVENT_BUTTON:
                    printf("[control_task] button id=%d\n", current_event.value);
                    break;
                case EVENT_SHUTDOWN:
                    printf("[control_task] shutdown\n");
                    current_task->state = TASK_FINISHED;
                    task_yield();
                    return;
                default:
                    printf("[control_task] ignored event=%s\n",
                        event_name(current_event.type));
                    break;
            }
        }
        task_yield();
    }
}
static void io_task(void) {
    while(1) {
        if(has_current_event) {
            switch (current_event.type) {
                case EVENT_UART_RX:
                    printf("[io_task] uart rx='%c' (%d)\n",
                        current_event.value,
                        current_event.value);
                    break;
                case EVENT_SHUTDOWN:
                    printf("[io_task] shutdown\n");
                    current_task->state = TASK_FINISHED;
                    task_yield();
                    return;
                default:
                    printf("[io_task] ignored event=%s\n",
                        event_name(current_event.type));
                    break;
            }
        }
        task_yield();
    }
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


static void idle_hook(void) {
    printf("[idle] no runnable tasks; MCU could enter WFI/WFE here\n");
}

static void scheduler_run(TCB tasks[], int count) {
    while(!all_tasks_finished(tasks, count)) {
        Event event;

        if(!event_pop(&event_queue, &event)) {
            idle_hook();
            break;
        }

        if(event.target_task_id < 0 || event.target_task_id >= count) {
            printf("[scheduler] drop invalid event target=%d\n", event.target_task_id);
            continue;
        }

        TCB *task = &tasks[event.target_task_id];

        if(task->state == TASK_FINISHED) {
            printf("[scheduler] drop event for finished task=%s\n", task->name);
            continue;
        }

        if (task->state == TASK_BLOCKED) {
            printf("[scheduler] target task blocked, defer event=%s for %s\n",
                event_name(event.type),
                task->name);
            event_push(&event_queue, event);
            scheduler_tick(tasks, count);
            continue;
        }

        current_event = event;
        has_current_event = true;

        current_task = task;
        task->state = TASK_RUNNING;

        printf("[scheduler] event %s -> %s\n", event_name(event.type), task->name);
        trace_context("restore", task);

        if(swapcontext(&scheduler_context, &task->context) == -1) {
            die("swapcontext");
        }
        has_current_event = false;
        scheduler_tick(tasks, count);
    }
    current_task = NULL;
    printf("\n[scheduler] event loop stopped\n");
}


int main(void)
{
    printf("context-switch-lab\n");
    TCB tasks[TASK_COUNT] = {
        {
            .id = 0,
            .name = "control_task",
            .state = TASK_READY,
            .entry = control_task,
        },
        {
            .id = 1,
            .name = "io_task",
            .state = TASK_READY,
            .entry = io_task,
        }
    };

    for(int i = 0 ; i < TASK_COUNT; i++) {
        init_task(&tasks[i]);
    }

    print_task_table(tasks, TASK_COUNT);

    event_push(&event_queue, (Event){0, EVENT_TIMER, 1});
    event_push(&event_queue, (Event){1, EVENT_UART_RX, 'A'});
    event_push(&event_queue, (Event){0, EVENT_BUTTON, 2});
    event_push(&event_queue, (Event){1, EVENT_UART_RX, 'B'});
    event_push(&event_queue, (Event){0, EVENT_SHUTDOWN, 0});
    event_push(&event_queue, (Event){1, EVENT_SHUTDOWN, 0});

    
    scheduler_run(tasks, TASK_COUNT); 
    print_task_table(tasks, TASK_COUNT);

    return 0;
}
