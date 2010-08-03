#ifndef TASK_H
#define TASK_H

#include "video/console.h"
#include "paging.h"
#include "descriptor_tables.h"
#include "scheduler.h"

#define KERNEL_STACK_SIZE 0x1000      // Use a 4 KB kernel stack

struct task
{
    bool              thread;         // Indicates whether its a thread or a task
    uint32_t          pid;            // Process ID
    uint32_t          esp;            // Stack pointer
    uint32_t          eip;            // Instruction pointer
    uint32_t          ss;             // stack segment
    page_directory_t* page_directory; // Page directory
    uint8_t           privilege;      // access privilege
    uint8_t*          heap_top;       // user heap top
    void*             kernel_stack;   // Kernel stack location
    uintptr_t         FPU_ptr;        // pointer to FPU data

    // Information needed by scheduler
    uint16_t  priority; // Indicates how often this task get the CPU
    blocker_t blocker;  // Object indicating reason and duration of blockade

    // task specific graphical output settings
    bool       ownConsole; // This task has an own console
    console_t* console;    // Console used by this task
    uint8_t    attrib;     // Color
} __attribute__((packed));

extern task_t* FPUTask; // fpu.c
extern task_t* shellTask;

extern bool task_switching;
extern task_t* currentTask;
extern console_t* currentConsole;

void tasking_install();

task_t*  create_task (page_directory_t* directory, void(*entry)(), uint8_t privilege); // Creates task using kernels console
task_t*  create_ctask(page_directory_t* directory, void(*entry)(), uint8_t privilege, const char* consoleName); // Creates task with own console
task_t*  create_thread (void(*entry)()); // Creates thread using currentTasks console
task_t*  create_cthread(void(*entry)(), const char* consoleName); // Creates a thread with own console
task_t*  create_vm86_task (void(*entry)());
task_t*  create_vm86_ctask(void(*entry)(), const char* consoleName);
void     switch_context();
uint32_t task_switch(uint32_t esp);
void     exit();

void waitForTask(task_t* blockingTask);

int32_t getpid();

void* task_grow_userheap(uint32_t increase);

void task_log(task_t* t);
void TSS_log(tss_entry_t* tss);

#endif
