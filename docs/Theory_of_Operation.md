# Theory of Operation# {#theory}
In this section we will explore some of the basic concepts and implementation decisions in Piccolo OS Plus. Since Piccolo
OS Plus is an extension of previous work, we will often only talk about the differences, so reviews of 
[Piccolo OS v1](https://github.com/garyexplains/piccolo_os_v1) and [Piccolo OS v1.1](https://github.com/garyexplains/piccolo_os_v1.1) may be helpful.

We will first discuss some of the implications of various feature choices, and then go on to some more detailed implementation explanations. At the end, we will discuss a couple of things NOT to do, to maybe save some grief.

# Implications of Select Features
Inclusion of some specific features raise questions and concerns which must be addressed. In this section, we will discuss some of these, and outline the solutions. The features of concern are:
 - Pre-emptive multitasking
 - Multi-core operation
 - Dynamic task creation and deletion
 - Signal channels and signaling

## Pre-emptive multitasking

### What is pre-emptive multitasking?
In a co-operative multitasking environment it is up to each task to yield control back to the kernel so that another task has the chance to run.
In a pre-emptive environment the kernel forces control away from a task and lets another task run. 

To do this, Piccolo OS uses the Systick exception. In essence, a Systick exception is raised by the hardware when the Systick counter reaches zero.
The Systick Control, Reload, and Status registers control how long it takes between exceptions, and if the exception is enabled or disabled. The Systick timer uses a 1μs clock.

### How do we avoid breaking the rest of the Pico SDK system?
Truth be told, we *cannot* do a perfect job. The Pico SDK documentation specifically states that 
much of the standard C library is not "core safe". (They mean you cannot call it from multiple
cores, but this would also apply to multiple tasks if preemption is allowed.) They do promise that 
`printf()` family and the `malloc(), calloc(), free()` family *are* core safe. They also advise users that
the SDK provides mutexes which a user could use to gate library function use. 

The Piccolo OS files 
`piccolo_os_lock_core.c` and `piccolo_os_lock_core.h` integrate Piccolo OS with the SDK internals
so that all of the SDK provided interlocking mechanisms such as mutexes and semaphores become *task safe* 
as well as core safe.

 - NOTE: While `malloc()` and `free()` *are* task safe and protected from preemption on both cores, they are protected by mutexes and mutexes are *not* interrupt safe. Since the kernal *is* an
interrupt handler, we cannot call `malloc()` or `free()` from within the kernal! 

### Preempting Interrupt Service Routines
We should worry about Systick preempting interrupt service handlers and timer routines. 
The Pico SDK configures all external interrupt priorities to level 2. (The ARM Cortex M0 has four levels,
with 0 being highest priority and 3 being lowest.) This means that the default Systick (and SVC) priorities
(set to 0) will be higher than any IRQ's used by the SDK or the user. Doing a pre-emptive context switch during an
Interrupt Service Routine would be a **disaster**! Since the user can also lower IRQ priorities further, we set the IRQ priorities for SVC and Systick to the lowest possible level.

### Do we create any race conditions with two exceptions doing the same thing?
The exception handlers for Systick and SVC both go to the kernal. Does this create a problem?
The short answer is yes! The timer can expire just before the SVC, or simultaneously. It can also expire
while the scheduler is running (after the SVC)! We did at least make sure that neither exception
will preempt the other when we set *both* their priorities to the lowest level.
- If the Systick timer expires before the SVC instruction executes, then the task misses the boat and will skip
a turn when it resumes and executes the SVC instruction. (So sad!)
- If both the SVC and Systick happen at the same time then SVC wins with a lower exception number and
the Systick interrupt will be pending. This is the same as Systick expiring while the scheduler is running 
because of an SVC, and will be discussed below. (Note that *at the same time* here means that Systick fires
during the execution of the SVC instruction.)
- The Systick timer can expire while the scheduler is running due to an SVC instruction. The scheduler is the
exception handler and will not be preempted by the Systick interrupt because they are at the same
priority level. But the Systick exception will be pending and will happen as soon as the scheduler switches to 
the next task! We can solve this by making sure that the scheduler specifically disables the Systick timer
and then clears any pending Systick exceptions. The timer can then be restarted when the next task is run.

## Multi-core operation
The primary concern with multi-core operation is to prevent both cores from trying to simultanously modify the same data.
While we have made this possible for tasks and the SDK in the implementation of preemption, we need to take additional steps
to protect the scheduler and and certain other task callable functions, since all can run on both cores.

### Protecting the scheduler
The scheduler's job is to cycle through all the tasks, looking for a task to run that is not blocked or running on the
other core. While doing this it will check each blocked task and unblock it if possible. This search *cannot* happen on both cores
simultaneously, because both of them might modify the blocking status of the same task, or even try to run the same one!

In addition, task creation or deletion must modify the list of tasks which the scheduler might currently be using! 

The solution is to use one of the spinlocks from the SDK. Spinlocks are interrupt and multicore safe, so we can insure that the critical sections of the scheduler cannot run at the same time. We use the same lock to prevent task creation and deletion while
the scheduler is running. Piccolo OS Plus uses one of the spinlocks already reserved by the SDK for operating system use. 

### Protecting the signal channels
The signal channel is only read by the task to which it is connected. By design, this is safe. But multiple senders are allowed
to send signals to the same receiving task, so the sending mechanism must be protected since two senders could be running
on seperate cores, or one could be preempted during sending. The solution is to again use a spinlock. (A spinlock is required, because we need interrupt protection since interrupt handlers are also allowed to send signals!) We currently use the same 
spinlock as the scheduler, though this is *not* required. 

### Getting the interface to the SDK right
A mentioned previously, we have integrated Piccolo OS Plus with the SDK synchronization mechanisms. We want to allow
a task that was waiting for a lock in the SDK to yield to other tasks. For this purpose, it would be enough to just make sure 
the scheduler was running on the core that called the SDK. (Remember you can call the SDK routines before `piccolo_start()` is called.) But sometimes the SDK actually cares *who* owns the lock! So we must return the correct task ID no matter which core
is running the task! The solution is to keep track of which task is running on each core, and lookup the task ID for that core when asked.

## Dynamic task creation and deletion
The addition of dynamic task creation and deletion forced one major design change from `v1` and `v1.1`, and adds a few additional 
concerns.

### A task must be a struct
In order to be dynamically added and deleted, tasks need to be represented as a `struct`, and the simplicity of an array of 
tasks had to be discarded. The scheduler uses a doubly linked (meaning forward and backward linked) list of tasks. This facilitates easy removal of a task from anywhere in the list, and easy addition of a new task at the end of the list.

### Protection of the structures
As mentioned before, in a multi-core and pre-emptive scheduling environment we must not allow task creation, deletion or 
scheduling to occur simultaneously, because these linked lists of structures would be corrupted. Therefore, all of these operations use the same spinlock as discussed above.

### Use of malloc() and free()
As discussed previously, `malloc()` and `free()` are task and core safe in Piccolo OS Plus, but *not* interrupt safe. The
scheduler *is* an interrupt handler, so though it is responsible for removing tasks which have ended, *it cannot call `free()` to return the memory for the task struct*. Our solution is to let the scheduler place the discarded task onto another list (the 
"zombie" list of dead tasks) and allow a special garbage collector task to return the memory using `free()`. Of course, the zombie list has to be protected using the same scheduler spin lock as before.

# Implementation Discussions
Here we talk about some of the implementation details in more depth. Topics include:
 - Non-obvious members of the task structure \ref piccolo_os_task_t and the internals structure \ref piccolo_os_internals_t.
 - The scheduler in a bit more detail
 - The garbage collector
 - The signal system design

 ## The task and internals structures
 The function of most of the members of the task structure piccolo_os_task_t and the internals structure
 piccolo_os_internals_t are reasonably obvious from the descriptions in the documentation, but a few deserve more information.

### The task data structure
 - `task_flags` is a logical 'or' of the bit masks from the enum \ref piccolo_task_flag_values. This is how tasks are marked as running or blocked. Note that a task is marked "blocked" and for what reason or reasons. For example, 
 a task waiting for a signal with timeout is blocked for *two* reasons! Resolving *either* will allow the task to run.
 - `task_sending_to` is the target task if the current task is blocked trying to send a signal.
The scheduler can then check the target task
to see if there is now room in its signal channel so the current task can be unbolcked.

### The internals data structure
 - `this_task[]` is how the Pico SDK interface routines find out which task is running on the current core. It is maintained
current by the scheduler on each core. The entries must be initialized to their core number (`%this_task[i]=i`) before
 - `piccoloc_start()` is called or core1 is started for any other reason. When `main()` is started, both entries are 0. This is OK because core1 is not running. Calling `piccolo_init()` sets `this_task[1]=1`, completing the initialization. 

## The scheduler in more detail
Here is where many of our concerns throughout are resolved. When `piccolo_start()` is called, it first does some final 
initialization only on core 0.
 - create the garbage collector task.
 - set the `current_task` (the last task run anywhere) to the last task in the list
 - start core1

Then enter handler mode and begin the main scheduler loop. The main loop is:
- Turn off Systick and clear any interrupt it may have pending.
- Lock the spin lock
- Starting with the task after the `current_task`, check all the tasks in turn
    - if the task is running, skip it
    - if the task is blocked, try to unblock it. (Has the timer expired, or data arrived, etc.)
    - if the task is not blocked,
        - we have found a task to run, mark it running
        - set `current_task` and `this_task[core number]` to this task
        - unlock spinlock (the other scheduler will now ignore this task)
        - go to run the task
    - Move to the next task, and keep searching. If we are out of tasks,
        - unlock the spinlock
        - if we can, run the idle task
        - return to the top of the main loop.
- To run the task, start the Systick timer for preemption
- Switch context to run the task
- Resume when the task yields or is preempted, check if has ended and is marked as a zombie
    - if it is a zombie
        - lock the spinlock
        - remove the task from the scheduler loop
        - if it was the `current task` set `current task` to the task preceeding this one. This makes sure the schedulers start looking in the right place!
        - add the dead task to the zombie list for the garbage collector
        - unlock the spinlock
        - send a signal to the garbage collector to wake him up because he has work to do!
    - otherwise, just mark the task as "not running"
- return to the top of the main loop

## The Garbage Collector
The garbage collector task is simple, but it is critical that it does not miss any signals, 
and should free space as soon as possible. It sits in the following loop:
- block waiting for all the signals available
- loop while the zombie list is not empty
    - lock the spinlock
    - remove the first dead task from the zombie list
    - unlock the spinlock
    - call `free()` to release the dead task space

## Signals
Signal channels are based on an old technique called circular buffering which required no synchronization 
primatives at all for a data producer and consumer to cooperate. In our case, we remove the data buffer and let the 
position of the old buffer pointers convey the information. We will describe how this works, and then discuss
how we have adapted it for multiple senders.
### How signals work
A signal channel contains two counters `IN` and `OUT`, and a `LIMIT` value. The sender only changes the `IN` value, and
the receiver only changes the `OUT` value. As long as there is only one sender and one receiver, this implies that
we are already task and core safe. Because the channel has a finite size, we also have a `LIMIT` value. `IN` and `OUT`
are always incremented modulo the `LIMIT`, so for the following discussion, please assume that
`IN+1` and `OUT+1` means `(IN+1)%%LIMIT` and `(OUT+1)%%LIMIT` respectively. So here is how to receive and send:
- To Receive a signal
    - if `IN == OUT`, return channel empty
    - else `IN+=1`, return signal received
- To Send a signal
    - if `OUT+1 == IN`, return channel full
    - else `OUT+=1`, return signal sent

### But we allow multiple senders!
Because we allow multiple senders, the signal sending routines use a spinlock to insure that the `OUT` pointer is
always updated correctly. Currently they use the same spinlock as the scheduler, but this is *not* really required because
the send routines and the scheduler do not actually modify any of the same variables.

# Pitfalls - Beware a few things
Just a couple of tips to maybe save some grief
## Don't do this in interrupt service routines or timer callbacks
Here are a few things you shouldn't do in interrupt handlers or timer callback routines. Some are instantly fatal, 
others may actually work *sometimes* but be horribly hard to find when they go wrong.
### Don't call piccolo_yield() 
Yes, this one may seem obviously nonsensical to many, but there is another important reason. 
Remember that `%piccolo_yield()` executes an `SVC` instruction. The ARM processors do what is called
"priority escalation" if the interrupt caused by the `SVC` instruction cannot be serviced immediately. 
Priority escalation generates a `hard fault` exception. Since
we have assigned the SVC interrupt the lowest priority, if an SVC is executed while handling another interrupt 
you *will* get a `hard fault`. This will crash the current core at a breakpoint in the SDK.

### Don't call any method that blocks
Again, many will already see this as a bad idea, since it stops interrupt processing until you are somehow unblocked. 
But remember that the routines that block in Piccolo OS or the SDK (due to Piccolo OS integration) **will** call 
`%piccolo_yield()`. That will be bad!

### Don't call piccolo_create_task()
This one may not be so obvious. Creating a task calls `malloc()` to get space for the task structure, and `malloc()` is
*not interrupt safe*! So this might work, even most of the time. But someday it will not. Create your task in
initialization or in another task and send it a signal or use an SDK method like semaphores instead. (But don't block!)

# About Core 1
You can configure Piccolo OS Plus to not use multi-core and then go ahead and use Core 1 as you please. All the SDK 
mechanisms and methods will be available. (Piccolo OS does not use the FIFOs, etc., and will NOT start core 1). All of the
SDK sychronization mechanisms will also work between the cores. You can even send signals from core 1 to tasks running 
under Piccolo OS on core 0. Just remember to call `%piccolo_init()` in `main()` **before** you start core 1! Otherwise the SDK integration may break.
