/* ==================== LAB3 MODIFIED ==================== */

#include "includes.h"

/* CONSTANTS */
#define TASK_STK_SIZE 512 // Size of each task's stacks (# of WORDs)
#define N_TASKS 3 // Number of tasks
#define SCENARIO 2

/* VARIABLES */
OS_STK TaskStartStk[TASK_STK_SIZE];
OS_STK TaskStk[N_TASKS][TASK_STK_SIZE]; // Tasks stacks

OS_EVENT *R1; // Pointer to mutex obj 
OS_EVENT *R2;
INT8U task_done;
INT8U task_total;

/* FUNCTION PROTOTYPES */
void TaskStart(void *data); // start up task
void Task1(void *pdata);
void Task2(void *pdata);
void Task3(void *pdata);
void CheckEscAndExit(void);
void RunTicks(INT16U ticks);
void LockResource(OS_EVENT *resource, INT8U resourceId);
void UnlockResource(OS_EVENT *resource, INT8U resourceId);

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
    INT16U prev;
    pdata = pdata;

    // Install uC/OS-II's clock tick ISR
    OS_ENTER_CRITICAL();
    PC_VectSet(0x08, OSTickISR);
    PC_SetTickRate(OS_TICKS_PER_SEC); // 把 time 頻率調成指定頻率
    OS_EXIT_CRITICAL();

    OSStatInit();

    // 創建 mutex lock
    R1 = OSMutexCreate(1, &err);
    R2 = OSMutexCreate(2, &err);
    task_done = 0;

    // Scenario 1
#if SCENARIO == 1
    task_total = 3;
    OSTaskCreate(Task1, (void *)0, &TaskStk[0][TASK_STK_SIZE - 1], 3);
    OSTaskCreate(Task2, (void *)0, &TaskStk[1][TASK_STK_SIZE - 1], 4);
    OSTaskCreate(Task3, (void *)0, &TaskStk[2][TASK_STK_SIZE - 1], 5);

    // Scenario 2
#elif SCENARIO == 2
    task_total = 2;
    OSTaskCreate(Task1, (void *)0, &TaskStk[0][TASK_STK_SIZE - 1], 3);
    OSTaskCreate(Task2, (void *)0, &TaskStk[1][TASK_STK_SIZE - 1], 4);

#else
#error "SCENARIO must be 1 or 2"
#endif

    // 把 tick 跟 log buffer 歸零
    OS_ENTER_CRITICAL();
    OSTimeSet(0);
    OSMsgHead = 0;
    OSMsgTail = 0;
    OS_EXIT_CRITICAL();
    
    // 釋放 CPU
    for(;;){
        CheckEscAndExit();
        if (task_done >= task_total) {
            // 把 TaskStart 自己被 tick 叫醒的那筆 Preempt(idle->TaskStart) 從 log 退回去，
            // 不然印出來的結尾會多一筆跟測試無關的雜訊
            OS_ENTER_CRITICAL();
            prev = (INT16U)((OSMsgHead + OS_SCHED_LOG_SIZE - 1) % OS_SCHED_LOG_SIZE);
            if ((OSMsgHead != OSMsgTail) &&
                (OSMsgRing[prev].event == 0) &&
                (OSMsgRing[prev].from == OS_LOWEST_PRIO) &&
                (OSMsgRing[prev].to == 0)) {
                OSMsgHead = prev;
            }
            OS_EXIT_CRITICAL();
            OS_DumpMsg();
            PC_DOSReturn();
        }
        OSTimeDly(OS_TICKS_PER_SEC);
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

/* LockResource 並記錄 lock 前後的 priority 變化 */
void LockResource(OS_EVENT *resource, INT8U resourceId){
#if OS_CRITICAL_METHOD == 3
    OS_CPU_SR  cpu_sr;
#endif
    INT8U err;
    INT8U oldPrio;
    INT8U newPrio;

    OS_ENTER_CRITICAL();
    oldPrio = OSTCBCur->OSTCBPrio;
    OS_EXIT_CRITICAL();

    OSMutexPend(resource, 0, &err);

    OS_ENTER_CRITICAL();
    newPrio = OSTCBCur->OSTCBPrio;
    OS_EXIT_CRITICAL();

    OS_AddMsg(4, resourceId, oldPrio, newPrio);
}

/* UnlockResource 並記錄 unlock 前後的 priority 變化 */
void UnlockResource(OS_EVENT *resource, INT8U resourceId){
#if OS_CRITICAL_METHOD == 3
    OS_CPU_SR  cpu_sr;
#endif
    INT8U oldPrio;
    INT8U newPrio;

    OS_ENTER_CRITICAL();
    oldPrio = OSTCBCur->OSTCBPrio;
    OS_EXIT_CRITICAL();

    OSMutexPost(resource);

    OS_ENTER_CRITICAL();
    newPrio = OSTCBCur->OSTCBPrio;
    OS_EXIT_CRITICAL();

    OS_AddMsg(5, resourceId, oldPrio, newPrio);
}

/* APPLICATION TASKS */
// Scenario 1
#if SCENARIO == 1
void Task1(void *pdata){
    INT16U toStart;

    pdata = pdata;
    toStart = 8;
    OSTimeDly(toStart);

    RunTicks(2);
    LockResource(R1, 1);
    RunTicks(2);

    LockResource(R2, 2);
    RunTicks(2);
    UnlockResource(R2, 2);
    UnlockResource(R1, 1);

    OS_ENTER_CRITICAL();
    task_done++;
    OS_EXIT_CRITICAL();

    OSTaskSuspend(OS_PRIO_SELF);
}

void Task2(void *pdata){
    INT16U toStart;

    pdata = pdata;
    toStart = 4;
    OSTimeDly(toStart);

    RunTicks(2);
    LockResource(R2, 2);
    RunTicks(4);
    UnlockResource(R2, 2);

    OS_ENTER_CRITICAL();
    task_done++;
    OS_EXIT_CRITICAL();

    OSTaskSuspend(OS_PRIO_SELF);
}

void Task3(void *pdata){
    INT16U toStart;

    pdata = pdata;
    toStart = 0;

    RunTicks(2);
    LockResource(R1, 1);
    RunTicks(7);
    UnlockResource(R1, 1);

    OS_ENTER_CRITICAL();
    task_done++;
    OS_EXIT_CRITICAL();

    OSTaskSuspend(OS_PRIO_SELF);
}

// Scenario 2
#elif SCENARIO == 2
void Task1(void *pdata){
    INT16U toStart;

    pdata = pdata;
    toStart = 5;
    OSTimeDly(toStart);

    RunTicks(2);
    LockResource(R2, 2);
    RunTicks(3);

    LockResource(R1, 1);
    RunTicks(3);
    UnlockResource(R1, 1);

    RunTicks(3);
    UnlockResource(R2, 2);

    OS_ENTER_CRITICAL();
    task_done++;
    OS_EXIT_CRITICAL();

    OSTaskSuspend(OS_PRIO_SELF);
}

void Task2(void *pdata){
    INT16U toStart;

    pdata = pdata;
    toStart = 0;

    RunTicks(2);
    LockResource(R1, 1);
    RunTicks(6);

    LockResource(R2, 2);
    RunTicks(2);
    UnlockResource(R2, 2);

    RunTicks(2);
    UnlockResource(R1, 1);

    OS_ENTER_CRITICAL();
    task_done++;
    OS_EXIT_CRITICAL();

    OSTaskSuspend(OS_PRIO_SELF);
}
#endif
