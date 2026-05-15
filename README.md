# context-switch-lab

A small C learning project for understanding RTOS-style context switching.

This project starts as a Linux userspace prototype. It uses cooperative tasks to demonstrate:

- task control blocks
- independent task stacks
- scheduler dispatch
- context save / restore
- how the idea maps to MCU RTOS concepts such as SysTick, PendSV, SP, PC, and saved registers

This is not a production RTOS. The goal is clarity for learning.