/* ==================== LAB3 MODIFIED ==================== */

#include "includes.h"

/* CONSTANTS */
#define TASK_STK_SIZE 512 // Size of each task's stacks (# of WORDs)
#define N_TASKS 3 // Number of tasks

/* VARIABLES */
OS_STK TaskStartStk[TASK_STK_SIZE];
OS_STK TaskStk[N_TASKS][TASK_STK_SIZE]; // Tasks stacks

OS_EVENT *R1; // Pointer to mutex obj 
OS_EVENT *R2;
INT8U task_done;

/* FUNCTION PROTOTYPES */
void TaskStart(void *data); // start up task
void Task1(void *pdata);
void Task2(void *pdata);
// void Task3(void *pdata);
void CheckEscAndExit(void);
void RunTicks(INT16U ticks);

/* MAIN */
void  main (void){
    // Initialize uC/OS-II
    OSInit();
    // Save environment to return to DOS
    PC_DOSSaveReturn();
    // Install uC/OS-II's context switch vector 
    PC_VectSet(uCOS, OSCtxSw);
    // Create start up task
    OSTaskCreate(TaskStart, (void*) 0,
        (void*) &TaskStartStk[TASK_STK_SIZE-1],0
    );
    // Start multitasking
    OSStart(); 
}

/* START UP TASK */
void TaskStart(void *pdata){
#if OS_CRITICAL_METHOD == 3  // Allocate storage for CPU status register
    OS_CPU_SR  cpu_sr;
#endif     
    INT8U err;
    pdata = pdata;

    // Install uC/OS-II's clock tick ISR
    OS_ENTER_CRITICAL();
    PC_VectSet(0x08, OSTickISR);
    PC_SetTickRate(OS_TICKS_PER_SEC); // 把 time 頻率調成指定頻率
    OS_EXIT_CRITICAL();

    // 創建 mutex lock
    R1 = OSMutexCreate(1, &err);
    R2 = OSMutexCreate(2, &err);
    task_done = 0;

    // Create application tasks 
    OSTaskCreate(Task1, (void *)0, &TaskStk[0][TASK_STK_SIZE - 1], 3);
    OSTaskCreate(Task2, (void *)0, &TaskStk[1][TASK_STK_SIZE - 1], 4);
    // OSTaskCreate(Task3, (void *)0, &TaskStk[2][TASK_STK_SIZE - 1], 5);

    // 把 tick 歸零
    OS_ENTER_CRITICAL();
    // Old version: OS_TIME = 0;
    OSTimeSet(0);
    OS_EXIT_CRITICAL();
    
    // 釋放 CPU
    for(;;){
        CheckEscAndExit();
        if (task_done >= 2) {
            OS_DumpMsg();
            PC_DOSReturn();
        }
        OSTimeDly(100);
    }
}

void CheckEscAndExit(void){
    INT16S key;
    // 檢查有沒有按鍵
    if (PC_GetKey(&key) == TRUE) {
        // 檢查是不是 esc
        if (key == 0x1B) { // 0x1B: ASCII code of esc
            PC_DOSReturn();
        }
    }
}

/* 模擬 task execution time */
void RunTicks(INT16U ticks){
    OS_ENTER_CRITICAL();
    OSTCBCur->compTime = ticks;
    OS_EXIT_CRITICAL();

    while (OSTCBCur->compTime > 0) {
        // busy waiting
        CheckEscAndExit();
    }
}

/* APPLICATION TASKS */
void Task1(void *pdata){
    INT8U err;
    INT16U toStart;

    pdata = pdata;
    toStart = 5;
    OSTimeDly(toStart);

    RunTicks(2);
    OSMutexPend(R2, 0, &err);
    RunTicks(3);

    OSMutexPend(R1, 0, &err);
    RunTicks(3);
    OSMutexPost(R1);

    RunTicks(3);
    OSMutexPost(R2);

    OS_ENTER_CRITICAL();
    task_done++;
    OS_EXIT_CRITICAL();

    OSTaskSuspend(OS_PRIO_SELF);
}

void Task2(void *pdata){
    INT8U err;
    INT16U toStart;

    pdata = pdata;
    toStart = 0;

    RunTicks(2);
    OSMutexPend(R1, 0, &err);
    RunTicks(6);

    OSMutexPend(R2, 0, &err);
    RunTicks(2); 
    OSMutexPost(R2);

    RunTicks(2);
    OSMutexPost(R1);

    OS_ENTER_CRITICAL();
    task_done++;
    OS_EXIT_CRITICAL();

    OSTaskSuspend(OS_PRIO_SELF);
}

// void Task3(void *pdata){
//     pdata = pdata;
//     OSTaskSuspend(OS_PRIO_SELF);
// }
