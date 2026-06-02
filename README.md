# context-switch-lab

A small C learning project for understanding RTOS-style context switching.

This project starts as a Linux userspace prototype. It uses cooperative tasks to demonstrate:

- task control blocks
- independent task stacks
- scheduler dispatch
- context save / restore
- how the idea maps to MCU RTOS concepts such as SysTick, PendSV, SP, PC, and saved registers

This is not a production RTOS. The goal is clarity for learning.

## Run Command
```
make docker-build
make docker-run
```

## Learning Stages
This project was built in small stages to make the RTOS context-switching concepts easier to understand.

### Stage 1: TCB and Task State
The first step was to model a task using a **Task Control Block (TCB)**.

Each task has:

- an ID
- a name
- a state
- an entry function

The task state is represented by:

```c
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_FINISHED
} TaskState;
```

This maps to a common RTOS idea: the scheduler does not manage raw functions directly. It manages task metadata stored in a TCB.

Key learning:
- A TCB is the scheduler's view of a task.
- Task state helps the scheduler decide whether a task can run.

### Stage 2: Cooperative Round-Robin Scheduler
The next step was to implement a simple cooperative scheduler.

The scheduler walks through the task table using a round-robin index:
```c
next = (next + 1) % count;
```

Only TASK_READY tasks are dispatched.

In the early version, each task manually yielded back to the scheduler after one step. This showed the idea of cooperative scheduling:

- The task decides when to give up control.
- The scheduler picks the next ready task.
- There is no timer interrupt or preemption yet.

Key learning:

- Round-robin scheduling is simple and predictable.
- Cooperative scheduling is easier to understand, but one task can block the system if it never yields.
- Preemptive RTOS scheduling usually depends on a hardware timer interrupt.

### Stage 3: Real Context Switching with ucontext
After the state-machine version worked, the project moved to ucontext.

Each TCB now stores:
```c
ucontext_t context;
unsigned char stack[STACK_SIZE];
```
The stack is assigned to the context:
```c
task->context.uc_stack.ss_sp = task->stack;
task->context.uc_stack.ss_size = sizeof(task->stack);
task->context.uc_link = &scheduler_context;
```
The scheduler switches to a task using:
```c
swapcontext(&scheduler_context, &task->context);
```
A task yields back using:
```c
swapcontext(&current_task->context, &scheduler_context);
```
Key learning:
- ucontext_t stores enough execution state to resume later.
- Conceptually, this includes PC, SP, registers, signal mask, stack information, and link context.
- Unlike the earlier state-machine version, the task resumes from the point after task_yield().
- Each task uses its own stack buffer.

### Stage 4: Context Trace and Per-Task Stack Visualization
To make the context switch visible, the project added trace output:
```text
[context] restore task=task_a stack_base=0x... stack_size=16384 state=RUNNING
[context] save    task=task_a stack_base=0x... stack_size=16384 state=READY
```
`[context] save` means that current task is about to yield back to the scheduler.
Conceptually, this saves the task's PC, SP, and registers into its context.

`[context] restore` means than the scheduler is about to resume a task.
Conceptually, this restores that task's PC, SP, and registers so execution continues from the previous yield point.

Key learning:

- Context switching can be understood as saving one execution context and restoring another.
- Each task has a separate stack region.
- In an RTOS, the saved context would usually live on the task stack and/or in the TCB.

### Stage 5: Idle / Low-Power Hook
The final stage added an idle hook:
```c
static void idle_hook(void)
{
    printf("[idle] all tasks are done; MCU could enter WFI/WFE here\n");
}
```
This models a common embedded systems pattern. When no task is runnable, an MCU-based RTOS may enter a low-power wait state.

On ARM Cortex-M, this is often done with:
- WFI — Wait For Interrupt
- WFE — Wait For Event

This demo only prints the idle transition. It does not execute MCU-specific instructions because it runs in userspace.

Key learning:
- Idle time is important in embedded systems.
- The idle task or idle hook is often where low-power policy begins.
- This userspace demo only prints the transition; it does not execute MCU-specific instructions.

### Stage 6: Event-Driven Scheduler
The event-driven version separates event type from task identity.

- `EventType` describes what happened.
- `target_task_id` decides which task handles it.
- `value` is a small event payload.

Example routing:

| Event | Target Task | Meaning |
| --- | --- | --- |
| `EVENT_TIMER` | `control_task` | periodic control tick |
| `EVENT_BUTTON` | `control_task` | GPIO button input |
| `EVENT_UART_RX` | `io_task` | received UART byte |
| `EVENT_SHUTDOWN` | both tasks | stop the demo task |

This models a common MCU pattern where drivers or ISRs post events into a queue, and tasks process events outside interrupt context.

### Stage 7: Critical Section

After introducing event delivery, the project added a simulated critical section.

In an MCU system, event delivery may be triggered by an ISR or driver callback, while the scheduler or tasks may also access shared task state. Shared fields such as pending events and task state should be protected from concurrent modification.

This demo uses:

```c
enter_critical();
...
exit_critical();
```
In this userspace demo, enter_critical() and exit_critical() only print trace messages. In real MCU firmware, this is where interrupt masking, scheduler locking, or another RTOS-provided protection mechanism could be used.

### Stage 8: Priority Scheduling

The final version adds priority-based scheduling.

Each task has a priority field in its TCB:

```c
int priority;
```
This project uses the rule:

```text
smaller priority value = higher priority
```
Events are delivered directly to the target task by `post_event()`

The scheduler then selects the highest-priority task that is both READY and has pending work:
```
TASK_READY && has_pending_event
```
This separates two ideas:

- `post_event()` performs event delivery.
- `scheduler_run()` performs scheduling policy.