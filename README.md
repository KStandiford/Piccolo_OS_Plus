# Piccolo OS Plus 
Piccolo OS Plus is an extension of [Piccolo OS v1](https://github.com/garyexplains/piccolo_os_v1),
a small multitasking OS for the Raspberry Pi Pico originally written by 
[Gary Sims](https://github.com/garyexplains) as a teaching tool to demonstrate 
the fundamentals of a co-operative multitasking OS and the Arm Cortex-M0+. (Piccolo OS v1 was later extended to 
allow pre-emptive scheduling as [Piccolo_OS_v1.1](https://github.com/garyexplains/piccolo_os_v1.1)).

Piccolo OS PLUS adds significant features to the previous versions v1 and v1.1 while remaining lean, efficient 
and small enough to easily learn to use. The added features are:
- Pre-emptive scheduling (also present in v1.1)
- Multi-core operation with the scheduler on both cores running any available task
- Dynamically allocated task structures and stacks (using `malloc()`)
- Task exit and deletion (using `free()`)
- Task blocking without execution (blocked tasks do NOT run)
- Task signalling with optional blocking and timeouts on sending or receiving
- `piccolo_sleep()` blocks rather than spinning until the timer expires
- An idle task which sleeps the processor to reduce power consumption
- Full integration with Pico SDK synchronization functionality like mutexes and semaphores. (Also present in v1.1)

While Piccolo OS Plus is still reasonably simple, the feature set expansion and complications of multi-core operation 
made it a bit too complex to be a simple teaching tool, so with Gary's permission and encouragement, it is presented here.

You can see the full documentation online [HERE](https://KStandiford.github.io/Piccolo_OS_Plus/). After building the project
you can also point your browser at `/docs/html/index.html` in the project directory. 

### One Caveat
The API for `picolo_sleep()` has been simplified so that it matches the API for `sleep_ms()` from the SDK.

## Basic Concepts
A _task_ (i.e. _user task_) is a function that is run by Piccolo OS in a round-robin fashion along with the other tasks. 
(For example, a function that flashes the onboard LED). Each task has its own stack, separate from all other tasks and the 
the kernal.

The _kernel_ (or _scheduler_) is `piccolo_start()` which is called by `main()` and never returns. In the multi-core environment,
the scheduler runs simultaneously on _both_ cores. Tasks are created during initialization (before `piccolo_start()` is called) or
may be created by another task during its execution. The scheduler (or schedulers) 
picks the next task that can be run (in a round-robin fashion), and transfers control to that task at the precise point 
where it left off execution during its last "turn" to run. A task may cooperatively relinquish control to the scheduler when 
it needs to pause or reaches a convenient point or the scheduler may preemptively "seize" control if the task has used its
allocated slice of computing time. At that point, the scheduler moves on to the next task available to run. The details of this 
process are described in the `Theory of Operation` and 
[Piccolo V1.1](https://github.com/garyexplains/piccolo_os_v1.1/blob/main/README.md) tabs of the documentation.

## Overview of Key New Features
### Pre-emptive Scheduling
Without pre-emptive scheduling (ie. Piccolo OS v1), a task would continue to execute until it voluntarily yielded the processor 
back to the scheduler. A compute intensive (or uncooperative) task could keep the processor too long and delay other tasks too much. 
(Indeed, the task could keep the processor forever, preventing any other task from running.) For pre-emptive scheduling, the 
kernal sets a timer 
when it starts a task, and stops the task when the timer expires if the task fails to yield first. This improves "fairness" for all tasks 
and improves the system response time overall.

### Multi-core Operation with an Idle Task
Running the kernal on both cores simply allows two tasks to execute simultaneously, with each processor choosing the next task 
as soon as its current task yields or is preempted. A task may be executed on either core. (But not both at once!) 

The idle task allows the processor to actually sleep if there are no tasks 
which are ready to run. This reduces power consumption.

### Dynamic Task Creation and Deletion 
Tasks can be created during initialization (`in main()`) or by another task at run time. There is no preset limit 
on the number of tasks (at least until there is no more free memory). A task which is no longer needed can execute a `return`,
at which point it will be removed from the kernal's list of tasks and the memory it was using will be returned to the 
system. (There is actually a built in garbage collector task which frees the memory.)

### Task Blocking Without Execution 
Tasks which are not ready to execute (waiting or 'blocking') are marked specially in the scheduler's list. The scheduler 
checks whether the conditions the task is waiting for have occurred (for example, a sleep time is over), and unblocks and executes 
the task if so. This is faster than just running the task so it can check for itself, and allows the scheduler to detect idle time.

### Signals
A signal is simply a notification to a task that some event of interest has occurred. (A signal contains no data, and occupies no
 memory.) Each task has its own input signal channel.
A signal can be sent to a task by another task, an interrupt service handler, or a callback routine. A task can just
check for a signal, or can block until one arrives. There are more details in the theory of operation section.

The Pico SDK also provides similar functionality with semaphores. However, the Piccolo integration with the SDK requires actually 
running the task to check the semaphore which substantially increases the overhead while waiting and prevents the 
cores from sleeping.

## Quick Start Guide
This is just a usage outline. Refer to the `Piccolo Plus APIs` tab for more details, and `piccolo_os_demo.c` for some examples.

### piccolo_init()
In `main()` you should initialize the SDK and the hardware as you like. In particular, here is the place to call `stdio_init_all()`. 
Any of the SDK functions can also be called from within a task. The exact order is not critical, but before creating any 
tasks or starting the kernal, you must call `piccolo_init()` in order to set up the Piccolo OS data structures.

### piccolo_create_task()
Call `piccolo_create_task()` to create your initial set of tasks. (There should be at least one!). Again, other calls to the SDK 
can be intermixed however you like, as long as the `piccolo_task_create()` calls come after `piccolo_init()` and before 
`piccolo_start()`.

### piccolo_start()
Call `piccolo_start()` last in `main()`. `piccolo_start()` completes the Piccolo_OS initialization, starts itself on core1 and 
begins scheduling and running tasks. It never returns. 

### Examples Within a Task
`piccolo_yield()` is called from inside a task to voluntarily yield the processor to the next task. 

`piccolo_sleep(<sleep time in ms>)` puts the task to sleep. The scheduler will awaken it when the time expires.


## Limitations
Many! Piccolo OS Plus is *NOT* intended to be a full scale RTOS. (After all, there is a perfectly good free one out there!)
There are no device drivers per say. There are some parts of the C libraries that are not thread safe. But most of the SDK
will work
and C++ will also run just fine. See more details in the `Theory of Operation` tab in the documentation.

## Build Instructions
Make sure you have the Pico C/C++ SDK installed and working on your machine. 
[Getting started with Raspberry Pi Pico](https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf)
is the best place to start.

You need to have PICO_SDK_PATH defined, e.g. `export PICO_SDK_PATH=/home/pi/pico/pico-sdk/`

Clone the code from the repository. Run `cmake` and `make` in the usual way for your development environment.

## Credits
Many thanks to [Gary Sims](https://github.com/garyexplains) for his original inspiration and ongoing support. Excerpts 
from his documentation are also used here.

Thanks also to [Bjorn](https://github.com/BjornTheProgrammer) for his interest and encouragement.

## Primary Copyrights
```
Copyright (C) 2022 Keith Standiford 
Copyright (C) 2021,2022 Gary Sims 
All rights reserved.
```


## License - 3-Clause BSD License
Copyright (C) 2022, Keith Standiford  
Copyright (C) 2021, 2022 Gary Sims  
All rights reserved.

SPDX short identifier: BSD-3-Clause

## Additional Copyrights
Some portions of code, intentionally or unintentionally, may or may not be attributed to the following people:  
Copyright (C) 2017 Scott Nelson: CMCM - https://github.com/scttnlsn/cmcm  
Copyright (C) 2015-2018 National Cheng Kung University, Taiwan: mini-arm-os - https://github.com/jserv/mini-arm-os  
Copyright (C) 2014-2017 Chris Stones: Shovel OS - https://github.com/chris-stones/ShovelOS  

# Release Notes

## Version 1.01
- Refactored the scheduler a bit to improve efficiency.
- Optimized the non-blocking `piccolo_get_signal()` function.
- The scheduler now will not sleep if any task is blocking to receive or send a signal. This improves response time at 
the expense of power consumption. This behavior can be disabled by setting PICCOLO_OS_NO_IDLE_FOR_SIGNALS to `false` in 
`piccolo_os.h` for lower power consumption if rapid response time is not critical.
- Cleaned up the timing reports from `piccolo_os_demo`.
- Numerous small documentation fixes.

## Version 1.02
- Fixed yet more typos in the documentation.
