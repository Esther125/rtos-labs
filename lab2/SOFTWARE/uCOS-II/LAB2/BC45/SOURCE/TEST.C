/* ==================== LAB2 MODIFIED ==================== */

#include "includes.h"

/* CONSTANTS */
#define TASK_STK_SIZE 512 // Size of each task's stacks (# of WORDs)
#define N_TASKS 3 // Number of tasks

/* VARIABLES */
OS_STK TaskStartStk[TASK_STK_SIZE];
OS_STK TaskStk[N_TASKS][TASK_STK_SIZE]; // Tasks stacks

/* FUNCTION PROTOTYPES */
void TaskStart(void *data); // start up task
void BaseTask(INT8U taskId, INT16U compTime, INT16U period); 
void Task1(void *pdata);
void Task2(void *pdata);
void Task3(void *pdata);
void CheckEscAndExit(void);

/* MAIN */
void  main (void){
    // Initialize uC/OS-II
    OSInit();
    // Save environment to return to DOS
    PC_DOSSaveReturn();
    // Install uC/OS-II's context switch ISR into the interrupt vector table
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
    pdata = pdata;

    // Install uC/OS-II's clock tick ISR
    OS_ENTER_CRITICAL();
    PC_VectSet(0x08, OSTickISR);
    PC_SetTickRate(OS_TICKS_PER_SEC); // 把 time 頻率調成指定頻率
    OS_EXIT_CRITICAL();

    // Create application tasks 
    OSTaskCreate(Task1, (void *)0, &TaskStk[0][TASK_STK_SIZE - 1], 1);
    OSTaskCreate(Task2, (void *)0, &TaskStk[1][TASK_STK_SIZE - 1], 2);
    // Testset2
    // OSTaskCreate(Task3, (void *)0, &TaskStk[2][TASK_STK_SIZE - 1], 3);
    
    // 把 tick 歸零
    OS_ENTER_CRITICAL();
    // Old version: OS_TIME = 0;
    OSTimeSet(0);
    OS_EXIT_CRITICAL();
    
    // 釋放 CPU
    for(;;){
        CheckEscAndExit();
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

/* BASETASK */
void BaseTask(INT8U taskId, INT16U compTime, INT16U period){
#if OS_CRITICAL_METHOD == 3  // Allocate storage for CPU status register
    OS_CPU_SR  cpu_sr;
#endif 
    INT32U now;
    INT32U toDelay; // 等多久下一次 task 才會 release
    taskId = taskId;

    // 初始化自己的 TCB 欄位（假設所有 task 都是同時從 tick 0 開始釋放）
    OS_ENTER_CRITICAL();
    OSTCBCur -> period      = period;
    OSTCBCur -> compTime    = compTime;
    OSTCBCur -> releaseTick = 0;
    OSTCBCur -> deadline    = period;
    OS_EXIT_CRITICAL();

    while(1){
        CheckEscAndExit();
        // 有 deadline miss
        if (OSMissTaskPrio != 0) {
            OS_DumpMsg();
            printf("time:%lu task%lu exceed deadline\n",
            (unsigned long)OSMissDeadlineTick,
            (unsigned long)OSMissTaskPrio);
            // 開發板：OSTaskSuspend(OS_PRIO_SELF); // 把 task 停掉
            PC_DOSReturn();
        }

        // 沒有 deadline miss
        while(OSTCBCur -> compTime > 0){
            // busy waiting 等 compute time 消耗
        }
        OS_DumpMsg();

        // 準備下一個 job
        OS_ENTER_CRITICAL();
        OSTCBCur -> releaseTick += period;
        OSTCBCur -> deadline += period;
        OSTCBCur -> compTime = compTime; // compute time 重置
        now = OSTimeGet();
        if (now < OSTCBCur->releaseTick){ // 還沒到下一個 job release 的時間
            toDelay = OSTCBCur->releaseTick - now;
        } else {
            toDelay = 0;
        }
        OS_EXIT_CRITICAL();
        
        if (toDelay > 0){
            OSTimeDly(toDelay);
        }
    }
   
}

/* APPLICATION TASKS */
void Task1(void *pdata){
    pdata = pdata;
    // Testset1
    BaseTask(1, 1, 3);

    // Testset2
    // BaseTask(1, 1, 4);
}

void Task2(void *pdata){
    pdata = pdata;
    // Testset1
    BaseTask(2, 3, 5);

    // Testset2
    // BaseTask(2, 2, 5);
}

void Task3(void *pdata){
    pdata = pdata;
    // Testset2
    // BaseTask(3, 2, 10);
}
