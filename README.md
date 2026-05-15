# context-switch-lab

A small C learning project for understanding RTOS-style context switching.

This project starts as a Linux userspace prototype. It uses cooperative tasks to demonstrate:

- task control blocks
- independent task stacks
- scheduler dispatch
- context save / restore
- how the idea maps to MCU RTOS concepts such as SysTick, PendSV, SP, PC, and saved registers

This is not a production RTOS. The goal is clarity for learning.

### Trace
`[context] save` means that current task is about to yield back to the scheduler.
Conceptually, this saves the task's PC, SP, and registers into its context.

`[context] restore` means than the scheduler is about to resume a task.
Conceptually, this resores that task's PC, SP, and registers so execution continues from the previous yield point.

the demo prints stack base and stack size to show that each task owns a separate stack.

### Idle / Low Power Hook

Many MCU RTOS ports provide an idle task or idle hook. When no application task is runnable, firmware can enter a low-power wait state.
On ARM Cortex-M, this is often where code may execute:

- `WFI` — Wait For Interrupt
- `WFE` — Wait For Event

This demo only prints the idle transition. It does not execute MCU-specific instructions because it runs in userspace.

### Run Command
```
make docker-build
make docker-run
```