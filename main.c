/*
 * context-switch-lab
 *
 * A small userspace C demo for learning RTOS-style scheduling.
 *
 * This version demonstrates:
 * - Task Control Blocks (TCB)
 * - per-task stacks
 * - ucontext-based context save/restore
 * - event posting to target tasks
 * - priority-based task selection
 * - an idle hook that maps to MCU low-power concepts
 *
 * This is not a production RTOS. It is a learning model.
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ucontext.h>

#define TASK_COUNT 2
#define STACK_SIZE (16 * 1024)        // 16 KB = 16 * 1024

static bool in_critical = false;

typedef enum {
    EVENT_TIMER,        // TIMER interrupt
    EVENT_UART_RX,      // UART interrupt
    EVENT_BUTTON,       // GPIO interrupt
    EVENT_SHUTDOWN      // shutdown task
} EventType;


/*
 * Event model.
 *
 * In a real MCU system, events may come from ISRs or drivers:
 * - timer interrupt
 * - UART RX interrupt
 * - GPIO/button interrupt
 *
 * target_task_id decides which task owns the event.
 * value is a small demo payload.
 */
typedef struct {
    int target_task_id;
    EventType type;
    int value;
} Event;

typedef enum {
    TASK_READY,             // task can be selected by the scheduler
    TASK_RUNNING,           // task is currently executing
    TASK_FINISHED           // task will no longer be scheduled
} TaskState;

typedef void (*TaskEntry)(void);

/*
 * Task Control Block.
 *
 * In an RTOS, the scheduler does not manage raw functions directly.
 * It manages TCBs. A TCB stores task metadata and the saved execution
 * context needed to resume a task later.
 *
 * ucontext_t is used here as a userspace stand-in for saved CPU context:
 * conceptually PC, SP, registers, signal mask, stack info, and link context.
 */
typedef struct TCB {
    int id;                             // task id
    const char *name;                   // name
    TaskState state;                    // state
    int priority;                       // smaller value has higher priority
    TaskEntry entry;                    // entry function
    ucontext_t context;                 // ucontext, include program counter, stack pointer, registers, signal mask, stack information, link context
    unsigned char stack[STACK_SIZE];    // per-task stack
    bool has_pending_event;             // true if this task has one event waiting
    Event pending_event;                // single pending event slot for this task
} TCB;

/*
 * Global scheduler state.
 *
 * scheduler_context stores where the scheduler should resume.
 * current_task points to the task currently running.
 * current_event is the event currently being handled by the running task.
 */
static ucontext_t scheduler_context;
static TCB *current_task = NULL;
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
        default:
            return "UNKNOWN";
    }
}


/*
* Simulated critical section.
*
* In real MCU firmware, this is where interrupt masking or a scheduler lock
* could protect shared state. Here it only prints trace messages and tracks
* whether we are inside a critical section.
*/
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

static bool post_event(TCB tasks[], int count, Event event) {
    /*
    * Post an event directly to its target task.
    *
    * This separates event delivery from scheduling policy:
    * - post_event() delivers work to a target task.
    * - scheduler_run() decides which READY task runs first.
    *
    * For clarity, each task only has one pending event slot. If another event is
    * posted before the previous one is handled, the new event is dropped.
    * A production RTOS would usually use a per-task queue or task notification.
    */
    enter_critical();

    if(event.target_task_id < 0 || event.target_task_id >= count) {
        printf("[scheduler] drop invalid event target=%d\n", event.target_task_id);
        exit_critical();
        return false;
    }

    TCB *target = &tasks[event.target_task_id];
    
    if(target->state == TASK_FINISHED) {
        printf("[scheduler] drop event for finished task=%s\n", target->name);
        exit_critical();
        return false;
    }
    

    if (target->has_pending_event) {
        printf("[scheduler] target already has pending event, defer event=%s for %s\n",
            event_name(event.type),
            target->name);
        exit_critical();
        return false;
    }

    target->pending_event = event;
    target->has_pending_event = true;
    target->state = TASK_READY;

    printf("[event] post %s -> %s priority=%d value=%d\n",
           event_name(event.type),
           target->name,
           target->priority,
           event.value);

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
    printf("%-4s %-10s %-10s  %-6s %-14s %-10s\n", "ID", "NAME", "STATE", "PRIO",  "STACK_BASE", "STACK_SIZE");

    for (int i = 0; i < count; i++) {
        printf("%-4d %-10s %-10s %-6d %-14p %-10zu\n",
                tasks[i].id,
                tasks[i].name,
                state_name(tasks[i].state),
                tasks[i].priority,
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
    /*
    * Cooperative yield.
    *
    * The running task saves its context and returns control to the scheduler.
    * If the task is still RUNNING, it becomes READY again.
    *
    * swapcontext() conceptually saves PC/SP/registers into current_task->context
    * and restores scheduler_context.
    */
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

static void control_task(void) {
    while(1) {
        if(has_current_event) {
            switch (current_event.type) {
                case EVENT_TIMER:
                    printf("[control_task] timer tick=%d\n", current_event.value);
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
    /*
    * Initialize a task context.
    *
    * Each task gets its own stack buffer. makecontext() sets the initial program
    * counter so the task starts from its entry function the first time it runs.
    */
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

static TCB *select_highest_priority_ready_task(TCB tasks[], int count) {
    /*
    * Priority scheduler policy.
    *
    * Select the READY task with pending work and the highest priority.
    * This demo uses a common convention: smaller priority number means higher
    * priority.
    */
    TCB *best = NULL;
    for(int i = 0 ; i < count; i++) {
        if(tasks[i].state != TASK_READY) {
            continue;
        }

        if (!tasks[i].has_pending_event) {
            continue;
        }

        if(best == NULL || tasks[i].priority < best->priority) {
            best = &tasks[i];
        }
    }
    return best;
}


static void idle_hook(void) {
    printf("[idle] no runnable tasks; MCU could enter WFI/WFE here\n");
}

static void scheduler_run(TCB tasks[], int count) {
    /*
    * Main scheduler loop.
    *
    * Events have already been delivered to task pending slots by post_event().
    * The scheduler only selects the highest-priority READY task with pending work,
    * restores its context, and waits for it to yield back.
    */
    while (!all_tasks_finished(tasks, count)) {
        TCB *selected = select_highest_priority_ready_task(tasks, count);

        if (selected == NULL) {
            idle_hook();
            break;
        }

        current_event = selected->pending_event;
        has_current_event = true;
        selected->has_pending_event = false;

        current_task = selected;
        selected->state = TASK_RUNNING;

        printf("[scheduler] selected %s priority=%d event=%s\n",
               selected->name,
               selected->priority,
               event_name(current_event.type));

        trace_context("restore", selected);

        if (swapcontext(&scheduler_context, &selected->context) == -1) {
            die("swapcontext");
        }

        has_current_event = false;
    }

    current_task = NULL;
    printf("\n[scheduler] event loop stopped\n");
}


int main(void)
{
    /*
    * Demo scenario.
    *
    * UART_RX is posted first to the lower-priority io_task.
    * BUTTON is posted second to the higher-priority control_task.
    *
    * Because scheduling is priority-based, control_task runs first even though
    * its event was posted later.
    */
    printf("context-switch-lab\n");
    TCB tasks[TASK_COUNT] = {
        {
            .id = 0,
            .name = "control_task",
            .state = TASK_READY,
            .entry = control_task,
            .priority = 1,
        },
        {
            .id = 1,
            .name = "io_task",
            .state = TASK_READY,
            .entry = io_task,
            .priority = 2,
        }
    };

    for(int i = 0 ; i < TASK_COUNT; i++) {
        init_task(&tasks[i]);
    }

    print_task_table(tasks, TASK_COUNT);

    post_event(tasks, TASK_COUNT, (Event){1, EVENT_UART_RX, 'A'});
    post_event(tasks, TASK_COUNT, (Event){0, EVENT_BUTTON, 1});

    scheduler_run(tasks, TASK_COUNT);
    post_event(tasks, TASK_COUNT, (Event){0, EVENT_SHUTDOWN, 0});
    post_event(tasks, TASK_COUNT, (Event){1, EVENT_SHUTDOWN, 0});

    scheduler_run(tasks, TASK_COUNT);
    print_task_table(tasks, TASK_COUNT);

    return 0;
}
